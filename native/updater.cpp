#include "updater.h"
#include "resource.h"

#include <bcrypt.h>
#include <roapi.h>
#include <winhttp.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace vtsfloat::update {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
using winrt::Windows::Data::Json::JsonObject;
using winrt::Windows::Data::Json::JsonValue;

constexpr wchar_t kLatestRelease[] =
    L"https://api.github.com/repos/fushoufish/VTSFloat_Meow/releases/latest";
constexpr wchar_t kAssetPrefix[] =
    L"https://github.com/fushoufish/VTSFloat_Meow/releases/download/";
constexpr wchar_t kAssetName[] = L"VTSFloat_Meow.exe";
constexpr wchar_t kCacheName[] = L"VTSFloat_Meow-updates";
constexpr std::uint64_t kMaxAssetSize = 128 * 1024 * 1024;
constexpr size_t kMaxJsonSize = 1024 * 1024;
constexpr size_t kMaxTaskSize = 64 * 1024;

class JsonApartment {
public:
    JsonApartment() : result_(RoInitialize(RO_INIT_MULTITHREADED)) {
        if (FAILED(result_) && result_ != RPC_E_CHANGED_MODE) {
            throw std::runtime_error("Initialize Windows JSON");
        }
    }
    ~JsonApartment() {
        if (SUCCEEDED(result_)) {
            // Windows may unload WinRT activation factories at apartment
            // shutdown. Drop the C++/WinRT cache before short-lived workers exit.
            winrt::clear_factory_cache();
            RoUninitialize();
        }
    }
private:
    HRESULT result_;
};

using Handle = std::unique_ptr<void, decltype(&CloseHandle)>;
using Internet = std::unique_ptr<void, decltype(&WinHttpCloseHandle)>;

[[noreturn]] void Win32Error(const char* operation) {
    const DWORD error = GetLastError();
    throw std::runtime_error(std::string(operation) + " (" + std::to_string(error) + ")");
}

std::string Hex(const unsigned char* bytes, size_t count) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(count * 2);
    for (size_t i = 0; i < count; ++i) {
        result.push_back(digits[bytes[i] >> 4]);
        result.push_back(digits[bytes[i] & 15]);
    }
    return result;
}

bool IsHex(std::wstring_view value, size_t length) {
    return value.size() == length &&
        std::all_of(value.begin(), value.end(), [](wchar_t ch) {
            return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f');
        });
}

std::wstring RandomToken() {
    std::array<unsigned char, 32> bytes{};
    if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
        throw std::runtime_error("Create update token");
    }
    const auto hex = Hex(bytes.data(), bytes.size());
    return {hex.begin(), hex.end()};
}

std::wstring ProcessCreated(HANDLE process) {
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(process, &created, &exited, &kernel, &user)) {
        Win32Error("Read process creation time");
    }
    ULARGE_INTEGER value{};
    value.LowPart = created.dwLowDateTime;
    value.HighPart = created.dwHighDateTime;
    return std::to_wstring(value.QuadPart);
}

bool IsLiveProcess(DWORD pid, const std::wstring& created) {
    if (!pid || created.empty()) return false;
    Handle process(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                               FALSE, pid), CloseHandle);
    return process && WaitForSingleObject(process.get(), 0) == WAIT_TIMEOUT &&
        ProcessCreated(process.get()) == created;
}

std::string ReadSmallFile(const fs::path& path, size_t limit = kMaxTaskSize) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Read update file");
    std::string data(limit + 1, '\0');
    file.read(data.data(), static_cast<std::streamsize>(data.size()));
    const auto size = static_cast<size_t>(file.gcount());
    if (file.bad() || size > limit) throw std::runtime_error("Invalid update file size");
    data.resize(size);
    return data;
}

void WriteAtomic(const fs::path& path, std::string_view data) {
    const fs::path temporary = path.wstring() + L"." +
        std::to_wstring(GetCurrentProcessId()) + L".tmp";
    HANDLE raw = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (raw == INVALID_HANDLE_VALUE) Win32Error("Create update file");
    Handle file(raw, CloseHandle);
    DWORD written = 0;
    const DWORD length = static_cast<DWORD>(data.size());
    const bool saved = WriteFile(file.get(), data.data(), length, &written, nullptr) &&
        written == length && FlushFileBuffers(file.get());
    file.reset();
    if (!saved || !MoveFileExW(temporary.c_str(), path.c_str(),
                               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        DeleteFileW(temporary.c_str());
        SetLastError(error);
        Win32Error("Save update file");
    }
}

std::string FileSha256(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Open file for SHA-256");
    struct HashHandles {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        ~HashHandles() {
            if (hash) BCryptDestroyHash(hash);
            if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        }
    } handles;
    if (BCryptOpenAlgorithmProvider(&handles.algorithm, BCRYPT_SHA256_ALGORITHM,
                                    nullptr, 0) < 0 ||
        BCryptCreateHash(handles.algorithm, &handles.hash, nullptr, 0, nullptr, 0, 0) < 0) {
        throw std::runtime_error("Initialize SHA-256");
    }
    std::array<unsigned char, 64 * 1024> buffer{};
    while (file) {
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        const auto read = static_cast<ULONG>(file.gcount());
        if (read && BCryptHashData(handles.hash, buffer.data(), read, 0) < 0) {
            throw std::runtime_error("Hash update file");
        }
    }
    std::array<unsigned char, 32> digest{};
    if (!file.eof() || BCryptFinishHash(handles.hash, digest.data(),
                                       static_cast<ULONG>(digest.size()), 0) < 0) {
        throw std::runtime_error("Finish SHA-256");
    }
    return Hex(digest.data(), digest.size());
}

bool MatchesHash(const fs::path& file, const std::string& sha256) {
    try {
        return !sha256.empty() && FileSha256(file) == sha256;
    } catch (...) {
        return false;
    }
}

void VerifyExecutable(const fs::path& path, const Release& release) {
    if (fs::file_size(path) != release.size || !MatchesHash(path, release.sha256)) {
        throw std::runtime_error("EXE size or SHA-256 mismatch");
    }
    std::ifstream input(path, std::ios::binary);
    IMAGE_DOS_HEADER dos{};
    input.read(reinterpret_cast<char*>(&dos), sizeof(dos));
    if (!input || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew < static_cast<LONG>(sizeof(dos)) ||
        static_cast<std::uint64_t>(dos.e_lfanew) + sizeof(DWORD) +
            sizeof(IMAGE_FILE_HEADER) > release.size) {
        throw std::runtime_error("Invalid EXE header");
    }
    input.seekg(dos.e_lfanew);
    DWORD signature = 0;
    IMAGE_FILE_HEADER header{};
    input.read(reinterpret_cast<char*>(&signature), sizeof(signature));
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
#if defined(_M_ARM64)
    constexpr WORD machine = IMAGE_FILE_MACHINE_ARM64;
#elif defined(_M_IX86)
    constexpr WORD machine = IMAGE_FILE_MACHINE_I386;
#else
    constexpr WORD machine = IMAGE_FILE_MACHINE_AMD64;
#endif
    if (!input || signature != IMAGE_NT_SIGNATURE || header.Machine != machine ||
        !(header.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) ||
        (header.Characteristics & IMAGE_FILE_DLL)) {
        throw std::runtime_error("EXE architecture mismatch");
    }
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (!size || size > kMaxTaskSize) throw std::runtime_error("Missing EXE version");
    std::vector<unsigned char> info(size);
    void* value = nullptr;
    UINT length = 0;
    if (!GetFileVersionInfoW(path.c_str(), 0, size, info.data()) ||
        !VerQueryValueW(info.data(), L"\\", &value, &length) ||
        length < sizeof(VS_FIXEDFILEINFO)) {
        throw std::runtime_error("Read EXE version");
    }
    const auto& version = *static_cast<const VS_FIXEDFILEINFO*>(value);
    const Version actual{HIWORD(version.dwFileVersionMS),
                         LOWORD(version.dwFileVersionMS),
                         HIWORD(version.dwFileVersionLS)};
    if (version.dwSignature != 0xFEEF04BD || actual != release.version) {
        throw std::runtime_error("EXE version does not match Release tag");
    }
}

using Chunk = std::function<void(const char*, DWORD)>;

void Fetch(std::wstring url, bool metadata, std::uint64_t limit,
           const std::atomic<bool>& cancelled, const Chunk& consume,
           const std::function<void(const std::string&)>& log, bool proxy) {
    const std::string route = proxy ? "gh-proxy.com" : "GitHub";
    const std::string prefix = std::string("network ") + (metadata ? "check" : "download") +
        " route=" + route;
    const auto started = Clock::now();
    DWORD status = 0;
    std::uint64_t total = 0;
    const auto finish = [&](const char* result, const std::string& detail = {}) {
        if (!log) return;
        log(prefix + " result=" + result + " http=" + (status ? std::to_string(status) : "-") +
            " bytes=" + std::to_string(total) + " elapsed_ms=" +
            std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count()) +
            (detail.empty() ? "" : " detail=" + detail));
    };
    try {
        if (cancelled.load()) throw std::runtime_error("Update cancelled");
        const auto deadline = Clock::now() + (metadata ? 15s : 120s);
        URL_COMPONENTS parts{};
        parts.dwStructSize = sizeof(parts);
        parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength =
            static_cast<DWORD>(-1);
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) ||
            parts.nScheme != INTERNET_SCHEME_HTTPS) {
            throw std::runtime_error("Invalid HTTPS update URL");
        }
        const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
        std::wstring resource(parts.lpszUrlPath, parts.dwUrlPathLength);
        if (log) log(prefix + " GET " + winrt::to_string(winrt::hstring(host)) +
            (metadata ? winrt::to_string(winrt::hstring(resource)) : ""));
        if (parts.dwExtraInfoLength) {
            resource.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        }
        Internet session(WinHttpOpen(L"VTSFloat_Meow/Updater",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0), WinHttpCloseHandle);
        if (!session || !WinHttpSetTimeouts(session.get(), 3000, 4000, 4000, 5000)) {
            Win32Error("Open update connection");
        }
        Internet connection(WinHttpConnect(session.get(), host.c_str(), parts.nPort, 0),
                            WinHttpCloseHandle);
        if (!connection) Win32Error("Connect to update host");
        Internet request(WinHttpOpenRequest(connection.get(), L"GET", resource.c_str(),
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE),
            WinHttpCloseHandle);
        if (!request) Win32Error("Create update request");
        DWORD redirects = 5;
        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
        DWORD disabled = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
        if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS,
                              &redirects, sizeof(redirects)) ||
            !WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy)) ||
            !WinHttpSetOption(request.get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled))) {
            Win32Error("Configure update request");
        }
        const wchar_t* headers = metadata ? L"Accept: application/json\r\n" :
                                           L"Accept: application/octet-stream\r\n";
        if (!WinHttpSendRequest(request.get(), headers, static_cast<DWORD>(-1),
                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            Win32Error("Send update request");
        }
        if (!WinHttpReceiveResponse(request.get(), nullptr)) Win32Error("Receive update headers");
        DWORD statusSize = sizeof(status);
        if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE |
                                WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                                &status, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
            Win32Error("Read update HTTP status");
        }
        if (status != 200) throw std::runtime_error("HTTP " + std::to_string(status));
        std::array<char, 64 * 1024> buffer{};
        for (;;) {
            if (cancelled.load()) throw std::runtime_error("Update cancelled");
            if (Clock::now() >= deadline) throw std::runtime_error("Update request timed out");
            DWORD read = 0;
            if (!WinHttpReadData(request.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &read)) {
                Win32Error("Read update response");
            }
            if (!read) break;
            total += read;
            if (total > limit) throw std::runtime_error("Update response exceeds size limit");
            consume(buffer.data(), read);
        }
        finish("ok");
    } catch (const std::exception& error) {
        finish(cancelled.load() ? "cancelled" : "failed", error.what());
        throw;
    }
}

} // namespace

std::optional<Version> ParseVersion(std::wstring_view text) {
    if (!text.empty() && (text.front() == L'v' || text.front() == L'V')) {
        text.remove_prefix(1);
    }
    Version version{};
    size_t parts = 0;
    while (!text.empty() && parts < version.size()) {
        const auto dot = text.find(L'.');
        const auto digits = text.substr(0, dot);
        if (digits.empty() || digits.size() > 5) return std::nullopt;
        unsigned value = 0;
        for (const auto digit : digits) {
            if (digit < L'0' || digit > L'9') return std::nullopt;
            value = value * 10 + static_cast<unsigned>(digit - L'0');
        }
        if (value > 65535) return std::nullopt;
        version[parts++] = value;
        if (dot == std::wstring_view::npos) {
            return parts >= 2 ? std::optional<Version>(version) : std::nullopt;
        }
        text.remove_prefix(dot + 1);
    }
    return std::nullopt;
}

std::optional<Release> ParseRelease(std::string_view json) {
    if (json.empty() || json.size() > kMaxJsonSize) return std::nullopt;
    try {
        JsonApartment apartment;
        const auto root = JsonObject::Parse(winrt::to_hstring(json));
        if (root.GetNamedBoolean(L"draft", true) ||
            root.GetNamedBoolean(L"prerelease", true)) return std::nullopt;
        const std::wstring tag(root.GetNamedString(L"tag_name"));
        const auto version = ParseVersion(tag);
        if (!version) return std::nullopt;
        const auto assets = root.GetNamedArray(L"assets");
        for (std::uint32_t i = 0; i < assets.Size(); ++i) {
            const auto asset = assets.GetAt(i).GetObject();
            if (asset.GetNamedString(L"name", L"") != kAssetName) continue;
            if (asset.GetNamedString(L"state", L"") != L"uploaded") return std::nullopt;
            const std::wstring url(asset.GetNamedString(L"browser_download_url"));
            if (url != std::wstring(kAssetPrefix) + tag + L"/" + kAssetName) {
                return std::nullopt;
            }
            const double size = asset.GetNamedNumber(L"size");
            if (!std::isfinite(size) || size < 1024 || size > kMaxAssetSize ||
                std::floor(size) != size) return std::nullopt;
            std::wstring digest(asset.GetNamedString(L"digest"));
            if (digest.compare(0, 7, L"sha256:") != 0) return std::nullopt;
            digest.erase(0, 7);
            if (!IsHex(digest, 64)) return std::nullopt;
            return Release{*version, tag, url, winrt::to_string(winrt::hstring(digest)),
                           static_cast<std::uint64_t>(size)};
        }
    } catch (const winrt::hresult_error&) {
        return std::nullopt;
    }
    return std::nullopt;
}

std::wstring RouteUrl(std::wstring_view githubUrl, bool proxy) {
    return (proxy ? std::wstring(L"https://gh-proxy.com/") : std::wstring()) +
        std::wstring(githubUrl);
}

ReleaseLookup LookupLatest(const std::function<std::string(bool)>& request,
                           const std::atomic<bool>& cancelled) {
    ReleaseLookup result;
    for (const bool proxy : {false, true}) {
        if (cancelled) break;
        try {
            result.release = ParseRelease(request(proxy));
            if (!result.release) throw std::runtime_error("Invalid GitHub Release response");
            result.proxy = proxy;
            return result;
        } catch (const std::exception& error) {
            if (!result.diagnostic.empty()) result.diagnostic += " | ";
            result.diagnostic += proxy ? "gh-proxy.com: " : "GitHub: ";
            result.diagnostic += error.what();
        }
    }
    return result;
}

namespace {

struct Task {
    fs::path directory;
    fs::path target;
    std::wstring token;
    std::wstring phase;
    Release release;
    std::string originalHash;
    DWORD parentPid = 0;
    std::wstring parentCreated;
    DWORD updaterPid = 0;
    std::wstring updaterCreated;

    fs::path File(const wchar_t* name) const { return directory / name; }
    fs::path SideFile(const wchar_t* suffix) const {
        return target.wstring() + L".update-" + token.substr(0, 32) + suffix;
    }
};

bool SamePath(const fs::path& a, const fs::path& b) {
    return _wcsicmp(a.lexically_normal().c_str(), b.lexically_normal().c_str()) == 0;
}

void Put(JsonObject& object, const wchar_t* name, std::wstring_view value) {
    object.SetNamedValue(name, JsonValue::CreateStringValue(winrt::hstring(value)));
}

void Put(JsonObject& object, const wchar_t* name, double value) {
    object.SetNamedValue(name, JsonValue::CreateNumberValue(value));
}

void WriteTask(const Task& task) {
    JsonApartment apartment;
    JsonObject object;
    Put(object, L"schema", 1.0);
    Put(object, L"target", task.target.wstring());
    Put(object, L"token", task.token);
    Put(object, L"phase", task.phase);
    Put(object, L"tag", task.release.tag);
    Put(object, L"sha256", std::wstring(task.release.sha256.begin(), task.release.sha256.end()));
    Put(object, L"size", static_cast<double>(task.release.size));
    Put(object, L"original_sha256", std::wstring(task.originalHash.begin(), task.originalHash.end()));
    Put(object, L"parent_pid", task.parentPid);
    Put(object, L"parent_created", task.parentCreated);
    Put(object, L"updater_pid", task.updaterPid);
    Put(object, L"updater_created", task.updaterCreated);
    WriteAtomic(task.File(L"task.json"), winrt::to_string(object.Stringify()));
}

DWORD ReadPid(const JsonObject& object, const wchar_t* name) {
    const double value = object.GetNamedNumber(name, 0);
    if (!std::isfinite(value) || value < 0 || value > MAXDWORD ||
        value != std::floor(value)) {
        throw std::runtime_error("Invalid update process ID");
    }
    return static_cast<DWORD>(value);
}

Task ReadTask(const fs::path& file, const fs::path& cache, const fs::path& target) {
    JsonApartment apartment;
    if (!file.is_absolute() || file.filename() != L"task.json" ||
        !SamePath(file.parent_path().parent_path(), cache)) {
        throw std::runtime_error("Unexpected update task path");
    }
    const auto object = JsonObject::Parse(winrt::to_hstring(ReadSmallFile(file)));
    Task task;
    task.directory = file.parent_path();
    task.target = std::wstring(object.GetNamedString(L"target"));
    task.token = std::wstring(object.GetNamedString(L"token"));
    task.phase = std::wstring(object.GetNamedString(L"phase"));
    if (object.GetNamedNumber(L"schema") != 1 ||
        !IsHex(task.token, 64) || task.directory.filename() != task.token.substr(0, 32) ||
        !task.target.is_absolute() || !SamePath(task.target, target)) {
        throw std::runtime_error("Invalid update task identity");
    }
    task.release.tag = std::wstring(object.GetNamedString(L"tag", L""));
    if (!task.release.tag.empty()) {
        const auto version = ParseVersion(task.release.tag);
        const std::wstring hash(object.GetNamedString(L"sha256"));
        const double size = object.GetNamedNumber(L"size");
        if (!version || !IsHex(hash, 64) || !std::isfinite(size) ||
            size < 1024 || size > kMaxAssetSize || std::floor(size) != size) {
            throw std::runtime_error("Invalid cached Release information");
        }
        task.release.version = *version;
        task.release.sha256 = winrt::to_string(winrt::hstring(hash));
        task.release.size = static_cast<std::uint64_t>(size);
    }
    task.originalHash = winrt::to_string(object.GetNamedString(L"original_sha256", L""));
    task.parentPid = ReadPid(object, L"parent_pid");
    task.parentCreated = std::wstring(object.GetNamedString(L"parent_created"));
    task.updaterPid = ReadPid(object, L"updater_pid");
    task.updaterCreated = std::wstring(object.GetNamedString(L"updater_created", L""));
    return task;
}

void SendSignal(const Task& task, const wchar_t* name) {
    JsonApartment apartment;
    JsonObject message;
    Put(message, L"token", task.token);
    Put(message, L"pid", GetCurrentProcessId());
    Put(message, L"created", ProcessCreated(GetCurrentProcess()));
    Put(message, L"tag", task.release.tag);
    WriteAtomic(task.File(name), winrt::to_string(message.Stringify()));
}

bool ReceiveSignal(const Task& task, const wchar_t* name,
                   DWORD pid, const std::wstring& created, bool forCurrentProcess) {
    try {
        JsonApartment apartment;
        const auto message = JsonObject::Parse(winrt::to_hstring(ReadSmallFile(task.File(name))));
        if (message.GetNamedString(L"token") != task.token ||
            message.GetNamedString(L"tag") != task.release.tag ||
            ReadPid(message, L"pid") != pid ||
            message.GetNamedString(L"created") != created) return false;
        return !forCurrentProcess ||
            (ReadPid(message, L"app_pid") == GetCurrentProcessId() &&
             message.GetNamedString(L"app_created") == ProcessCreated(GetCurrentProcess()));
    } catch (...) {
        return false;
    }
}

void RemoveFile(const fs::path& path) {
    std::error_code error;
    fs::remove(path, error);
}

void RemoveSignals(const Task& task) {
    for (const auto* name : {L"ready.json", L"commit.json", L"abort.json",
                            L"app-ready.json", L"show-success.json", L"shown.json"}) {
        RemoveFile(task.File(name));
    }
}

void CleanupTask(const Task& task, bool removeBackup) {
    const auto backup = task.SideFile(L".bak");
    if (removeBackup) RemoveFile(backup);
    // Keep the journal if cleanup must be retried or the backup is still needed.
    if (fs::exists(backup)) return;
    RemoveFile(task.SideFile(L".new"));
    RemoveFile(task.SideFile(L".failed"));
    if (fs::exists(task.SideFile(L".new")) || fs::exists(task.SideFile(L".failed"))) return;
    RemoveSignals(task);
    for (const auto* name : {L"payload.part", L"updater.ps1", L"failure.txt", L"task.json"}) {
        RemoveFile(task.File(name));
    }
    RemoveFile(task.directory);
}

std::wstring Quote(std::wstring_view value) {
    // Windows paths cannot contain quotation marks. The quoted arguments below
    // are absolute file paths or fixed-format tokens, never shell code.
    return L"\"" + std::wstring(value) + L"\"";
}

std::string Diagnostic(const char* route, const std::exception& error) {
    return std::string(route) + ": " + error.what();
}

} // namespace

struct Updater::Impl {
    HINSTANCE instance;
    fs::path cache;
    fs::path executable;
    Version version;
    Task task;
    Status status;
    std::mutex mutex;
    std::thread worker;
    std::atomic<bool> cancelled{false};
    Handle process{nullptr, CloseHandle};
    DWORD scriptPid = 0;
    std::wstring scriptCreated;
    Clock::time_point deadline{};
    bool handedOff = false;
    bool resuming = false;

    std::function<void(const std::string&)> log;

    Impl(HINSTANCE instance, fs::path config, fs::path executable, std::wstring versionText,
         std::function<void(const std::string&)> logger)
        : instance(instance),
          cache(fs::absolute(config).parent_path() / kCacheName),
          executable(std::move(executable)), version(ParseVersion(versionText).value()), log(std::move(logger)) {}

    ~Impl() {
        cancelled = true;
        if (worker.joinable()) worker.join();
        AbortPreparation();
        if (!task.directory.empty() && !ScriptRunning() && !handedOff) {
            try { CleanupTask(task, false); } catch (...) {}
        }
    }

    Status GetStatus() {
        std::lock_guard<std::mutex> lock(mutex);
        return status;
    }

    void SetStatus(Phase phase, bool proxy = false, int percent = 0,
                   std::string diagnostic = {}) {
        std::lock_guard<std::mutex> lock(mutex);
        status.phase = phase;
        status.proxy = proxy;
        status.percent = percent;
        status.tag = task.release.tag;
        status.diagnostic = std::move(diagnostic);
        ++status.revision;
    }

    void JoinFinishedWorker() {
        if (worker.joinable()) worker.join();
    }

    bool ScriptRunning() const {
        return process && WaitForSingleObject(process.get(), 0) == WAIT_TIMEOUT;
    }

    void NewTask() {
        if (executable.empty() || !executable.is_absolute()) {
            throw std::runtime_error("Cannot determine the running EXE path");
        }
        const auto token = RandomToken();
        const auto directory = cache / token.substr(0, 32);
        fs::create_directories(cache);
        if (!fs::create_directory(directory)) throw std::runtime_error("Create update task directory");
        task = {};
        task.directory = directory;
        task.target = executable;
        task.token = token;
        task.phase = L"checking";
        task.parentPid = GetCurrentProcessId();
        task.parentCreated = ProcessCreated(GetCurrentProcess());
        WriteTask(task);
    }

    void CheckNetwork() {
        std::string diagnostic;
        try {
            NewTask();
            const auto result = LookupLatest([&](bool proxy) {
                SetStatus(Phase::Checking, proxy);
                std::string json;
                Fetch(RouteUrl(kLatestRelease, proxy), true, kMaxJsonSize, cancelled,
                      [&](const char* data, DWORD bytes) { json.append(data, bytes); }, log, proxy);
                return json;
            }, cancelled);
            diagnostic = result.diagnostic;
            if (result.release && !cancelled) {
                task.release = *result.release;
                task.phase = L"available";
                WriteTask(task);
                SetStatus(task.release.version > version ? Phase::Available : Phase::Current,
                          result.proxy, 0, diagnostic);
                return;
            }
        } catch (const std::exception& error) {
            diagnostic = error.what();
        } catch (...) {
            diagnostic = "Unable to save update check state";
        }
        SetStatus(cancelled ? Phase::Idle : Phase::CheckFailed, false, 0, diagnostic);
    }

    void StartScript(bool resume) {
        const HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(IDR_UPDATER_SCRIPT), RT_RCDATA);
        if (!resource) Win32Error("Locate embedded updater");
        const HGLOBAL loaded = LoadResource(instance, resource);
        const auto* data = loaded ? static_cast<const char*>(LockResource(loaded)) : nullptr;
        const DWORD size = SizeofResource(instance, resource);
        if (!data || !size) throw std::runtime_error("Load embedded updater");
        RemoveSignals(task);
        WriteAtomic(task.File(L"updater.ps1"), {data, size});
        wchar_t system[MAX_PATH]{};
        const UINT length = GetSystemDirectoryW(system, ARRAYSIZE(system));
        if (!length || length >= ARRAYSIZE(system)) Win32Error("Find Windows PowerShell");
        const auto powershell = fs::path(system) / L"WindowsPowerShell/v1.0/powershell.exe";
        std::wstring command = Quote(powershell.wstring()) +
            L" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File " +
            Quote(task.File(L"updater.ps1").wstring()) +
            L" -TaskPath " + Quote(task.File(L"task.json").wstring()) +
            L" -Token " + Quote(task.token);
        if (resume) {
            command += L" -ResumePid " + std::to_wstring(GetCurrentProcessId()) +
                L" -ResumeCreated " + ProcessCreated(GetCurrentProcess());
        }
        STARTUPINFOW startup{sizeof(STARTUPINFOW)};
        PROCESS_INFORMATION info{};
        if (!CreateProcessW(powershell.c_str(), command.data(), nullptr, nullptr, FALSE,
                            CREATE_NO_WINDOW, nullptr, cache.c_str(), &startup, &info)) {
            Win32Error("Start Windows PowerShell updater");
        }
        process.reset(info.hProcess);
        CloseHandle(info.hThread);
        scriptPid = info.dwProcessId;
        scriptCreated = ProcessCreated(process.get());
        deadline = Clock::now() + 20s;
        resuming = resume;
        SetStatus(Phase::Preparing);
    }

    void DownloadNetwork() {
        const auto download = task.File(L"payload.part");
        const auto staged = task.SideFile(L".new");
        std::string diagnostic;
        bool verified = false;
        for (const bool proxy : {false, true}) {
            if (cancelled) break;
            SetStatus(Phase::Downloading, proxy, 0, diagnostic);
            try {
                std::ofstream output(download, std::ios::binary | std::ios::trunc);
                if (!output) throw std::runtime_error("Open download cache");
                std::uint64_t total = 0;
                int progress = -1;
                Fetch(RouteUrl(task.release.url, proxy), false, task.release.size, cancelled,
                      [&](const char* data, DWORD bytes) {
                          output.write(data, bytes);
                          if (!output) throw std::runtime_error("Write download cache");
                          total += bytes;
                          const int percent = static_cast<int>(total * 100 / task.release.size);
                          if (percent != progress) {
                              progress = percent;
                              SetStatus(Phase::Downloading, proxy, percent, diagnostic);
                          }
                      }, log, proxy);
                output.close();
                if (!output) throw std::runtime_error("Close download cache");
                VerifyExecutable(download, task.release);
                verified = true;
                break;
            } catch (const std::exception& error) {
                RemoveFile(download);
                if (!diagnostic.empty()) diagnostic += " | ";
                diagnostic += Diagnostic(proxy ? "gh-proxy.com" : "GitHub", error);
            }
        }
        if (!verified || cancelled) {
            RemoveFile(download);
            SetStatus(cancelled ? Phase::Idle : Phase::DownloadFailed, false, 0, diagnostic);
            return;
        }
        try {
            task.originalHash = FileSha256(executable);
            if (!CopyFileW(download.c_str(), staged.c_str(), TRUE)) Win32Error("Stage update beside EXE");
            VerifyExecutable(staged, task.release);
            RemoveFile(download);
            task.phase = L"prepared";
            WriteTask(task);
            if (cancelled) {
                CleanupTask(task, false);
                SetStatus(Phase::Idle);
                return;
            }
            StartScript(false);
        } catch (const std::exception& error) {
            AbortPreparation();
            SetStatus(Phase::ApplyFailed, false, 0, error.what());
        } catch (...) {
            AbortPreparation();
            SetStatus(Phase::ApplyFailed, false, 0, "Prepare update transaction");
        }
    }

    void AbortPreparation() noexcept {
        if (process && !handedOff && !resuming) {
            try { SendSignal(task, L"abort.json"); } catch (...) {}
        }
    }

    void Attach(const fs::path& file, std::wstring_view token) {
        task = ReadTask(file, cache, executable);
        if (task.token != token || task.release.version != version ||
            !IsLiveProcess(task.updaterPid, task.updaterCreated)) {
            throw std::runtime_error("Unrecognized updater handoff");
        }
        VerifyExecutable(executable, task.release);
        handedOff = true;
        scriptPid = task.updaterPid;
        scriptCreated = task.updaterCreated;
        SendSignal(task, L"app-ready.json");
        deadline = Clock::now() + 60s;
        SetStatus(Phase::WaitingForSuccess);
    }

    void Recover() {
        const auto result = cache / L"last-result.json";
        if (fs::exists(result)) {
            try {
                JsonApartment apartment;
                const auto object = JsonObject::Parse(winrt::to_hstring(ReadSmallFile(result)));
                if (SamePath(fs::path(std::wstring(object.GetNamedString(L"target"))), executable)) {
                    const bool restored = object.GetNamedBoolean(L"restored", false);
                    const std::string detail = winrt::to_string(object.GetNamedString(L"detail", L""));
                    RemoveFile(result);
                    SetStatus(restored ? Phase::RolledBack : Phase::RecoveryNeeded, false, 0, detail);
                }
            } catch (...) {}
        }
        if (!fs::exists(cache)) return;
        for (const auto& entry : fs::directory_iterator(cache)) {
            if (!entry.is_directory() || !IsHex(entry.path().filename().wstring(), 32)) continue;
            try {
                auto previous = ReadTask(entry.path() / L"task.json", cache, executable);
                if (IsLiveProcess(previous.parentPid, previous.parentCreated) ||
                    IsLiveProcess(previous.updaterPid, previous.updaterCreated)) continue;
                if (previous.phase == L"committed") {
                    CleanupTask(previous, true);
                    continue;
                }
                const auto backup = previous.SideFile(L".bak");
                if (!fs::exists(backup)) {
                    CleanupTask(previous, false);
                    continue;
                }
                if (!MatchesHash(backup, previous.originalHash)) {
                    SetStatus(Phase::RecoveryNeeded, false, 0, "Update backup failed verification");
                    continue;
                }
                if (MatchesHash(executable, previous.originalHash)) {
                    CleanupTask(previous, true);
                    continue;
                }
                if (previous.release.version == version &&
                    MatchesHash(executable, previous.release.sha256)) {
                    task = std::move(previous);
                    StartScript(true);
                    return;
                }
                SetStatus(Phase::RecoveryNeeded, false, 0, "Interrupted update backup was retained");
            } catch (...) {
                // Ignore incomplete journals and tasks for other portable copies.
            }
        }
    }

    void PollHandoff(Phase phase) {
        if (phase == Phase::Preparing) {
            if (cancelled && !resuming) {
                AbortPreparation();
                SetStatus(Phase::Idle);
                return;
            }
            if (ScriptRunning() &&
                ReceiveSignal(task, L"ready.json", scriptPid, scriptCreated, false)) {
                if (resuming) {
                    SendSignal(task, L"app-ready.json");
                    handedOff = true;
                    deadline = Clock::now() + 60s;
                    SetStatus(Phase::WaitingForSuccess);
                } else {
                    SetStatus(Phase::ReadyToExit);
                }
            } else if (Clock::now() >= deadline || !ScriptRunning()) {
                AbortPreparation();
                std::string detail = "PowerShell updater did not acknowledge preparation";
                try { detail = ReadSmallFile(task.File(L"failure.txt")); } catch (...) {}
                SetStatus(Phase::ApplyFailed, false, 0, detail);
            }
        } else if (phase == Phase::WaitingForSuccess) {
            if (IsLiveProcess(scriptPid, scriptCreated) &&
                ReceiveSignal(task, L"show-success.json", scriptPid, scriptCreated, true)) {
                SetStatus(Phase::ShowSuccess);
            } else if (Clock::now() >= deadline || !IsLiveProcess(scriptPid, scriptCreated)) {
                SetStatus(Phase::RecoveryNeeded, false, 0, "Updater startup confirmation was interrupted");
            }
        }
    }
};

Updater::Updater(HINSTANCE instance, fs::path config, fs::path executable, std::wstring version,
                 std::function<void(const std::string&)> log)
    : impl_(std::make_unique<Impl>(instance, std::move(config),
                                  std::move(executable), std::move(version), std::move(log))) {}

Updater::~Updater() = default;

bool Updater::Check(std::filesystem::path executable) {
    auto& state = *impl_;
    switch (state.GetStatus().phase) {
    case Phase::Checking:
    case Phase::Downloading:
    case Phase::Preparing:
    case Phase::ReadyToExit:
    case Phase::WaitingForSuccess:
    case Phase::ShowSuccess:
        return false;
    default:
        break;
    }
    state.JoinFinishedWorker();
    if (!state.task.directory.empty() && !state.handedOff && !state.ScriptRunning()) {
        try { CleanupTask(state.task, false); } catch (...) {}
    }
    state.task = {};
    state.executable = std::move(executable);
    state.process.reset();
    state.handedOff = false;
    state.resuming = false;
    state.cancelled = false;
    state.SetStatus(Phase::Checking);
    try {
        state.worker = std::thread([&state] { state.CheckNetwork(); });
    } catch (const std::exception& error) {
        state.SetStatus(Phase::CheckFailed, false, 0, error.what());
    }
    return true;
}

bool Updater::Download() {
    auto& state = *impl_;
    if (state.GetStatus().phase != Phase::Available) return false;
    state.JoinFinishedWorker();
    state.cancelled = false;
    state.SetStatus(Phase::Downloading);
    try {
        state.worker = std::thread([&state] { state.DownloadNetwork(); });
    } catch (const std::exception& error) {
        state.SetStatus(Phase::DownloadFailed, false, 0, error.what());
    }
    return true;
}

void Updater::Dismiss() {
    auto& state = *impl_;
    switch (state.GetStatus().phase) {
    case Phase::Checking:
    case Phase::Downloading:
    case Phase::Preparing:
        Cancel();
        return;
    case Phase::ReadyToExit:
    case Phase::WaitingForSuccess:
    case Phase::ShowSuccess:
        return;
    default:
        state.SetStatus(Phase::Idle);
        break;
    }
}

void Updater::Cancel() {
    auto& state = *impl_;
    state.cancelled = true;
    const auto phase = state.GetStatus().phase;
    if (phase == Phase::Preparing || phase == Phase::ReadyToExit) {
        state.AbortPreparation();
        state.SetStatus(Phase::Idle);
    }
}

Status Updater::Poll() {
    auto& state = *impl_;
    try {
        state.PollHandoff(state.GetStatus().phase);
    } catch (...) {
        state.SetStatus(Phase::ApplyFailed, false, 0, "Read updater handoff");
    }
    return state.GetStatus();
}

void Updater::OnStartup(const fs::path& taskFile, std::wstring_view token) {
    auto& state = *impl_;
    try {
        if (taskFile.empty()) state.Recover();
        else state.Attach(taskFile, token);
    } catch (...) {
        // The helper rolls back if a launched instance cannot acknowledge readiness.
        if (taskFile.empty()) state.SetStatus(Phase::RecoveryNeeded);
        else state.task = {};
    }
}

bool Updater::Commit() {
    auto& state = *impl_;
    if (state.GetStatus().phase != Phase::ReadyToExit) return false;
    try {
        if (!state.process || WaitForSingleObject(state.process.get(), 0) != WAIT_TIMEOUT) {
            throw std::runtime_error("PowerShell updater exited before commit");
        }
        SendSignal(state.task, L"commit.json");
        state.handedOff = true;
        return true;
    } catch (...) {
        state.AbortPreparation();
        state.SetStatus(Phase::ApplyFailed, false, 0, "Commit update transaction");
        return false;
    }
}

void Updater::AcknowledgeSuccess() {
    auto& state = *impl_;
    if (state.GetStatus().phase != Phase::ShowSuccess) return;
    try {
        SendSignal(state.task, L"shown.json");
        state.SetStatus(Phase::Success);
    } catch (...) {
        state.SetStatus(Phase::RecoveryNeeded, false, 0, "Acknowledge update success");
    }
}

} // namespace vtsfloat::update
