#include "anr_package.h"
#include "cache_directory.h"
#include "updater.h"

#include <windows.h>
#include <objbase.h>

#include <array>
#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace vtsfloat {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
constexpr wchar_t kAssetUrl[] = L"https://github.com/apex-compute/ANR/releases/download/FG-V.B15/anr-fg-V.B15-addon.zip";
constexpr char kAssetSha256[] = "717e93b3e63cb08dbaf6240cd585bae94d498c2749d8603d800d17ad97ef9e41";

bool IsCacheDirectory(const fs::path& directory) {
    const DWORD attributes = GetFileAttributesW(directory.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) &&
        !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
}

void EnsureCacheDirectory(const fs::path& directory) {
    fs::create_directory(directory);
    if (!IsCacheDirectory(directory)) throw std::runtime_error("Invalid ANR cache directory");
}

struct InstallWorkspace {
    fs::path path;

    explicit InstallWorkspace(const fs::path& root) {
        GUID id{};
        wchar_t name[40]{};
        if (FAILED(CoCreateGuid(&id)) || !StringFromGUID2(id, name, ARRAYSIZE(name))) {
            throw std::runtime_error("Create ANR staging directory name");
        }
        path = root / (std::wstring(L".anr-fg-") + name);
        if (!fs::create_directory(path)) throw std::runtime_error("Create ANR staging directory");
        std::error_code error;
        const auto resolved = fs::canonical(path, error);
        if (error || resolved.parent_path() != root || !IsCacheDirectory(resolved)) {
            throw std::runtime_error("Invalid ANR staging directory");
        }
        path = resolved;
    }

    ~InstallWorkspace() {
        if (!IsCacheDirectory(path)) return;
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

void ExtractProvider(const fs::path& systemDirectory, const fs::path& archive,
                     const fs::path& destination, const AnrDownloadProgress& progress) {
    const auto tar = systemDirectory / L"tar.exe";
    std::wstring command = L"\"" + tar.wstring() + L"\" -xf \"" + archive.wstring() +
        L"\" -C \"" + destination.wstring() + L"\" --strip-components 2 \"Apex Neural Rendering FG/dlssg\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(tar.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                       nullptr, nullptr, &startup, &process)) {
        throw std::runtime_error("Start Windows tar extractor");
    }
    CloseHandle(process.hThread);
    const auto deadline = Clock::now() + std::chrono::seconds(20);
    DWORD result = WAIT_TIMEOUT;
    while (!progress.cancelled && Clock::now() < deadline && result == WAIT_TIMEOUT) {
        result = WaitForSingleObject(process.hProcess, 100);
    }
    DWORD exitCode = 1;
    if (result == WAIT_OBJECT_0) {
        GetExitCodeProcess(process.hProcess, &exitCode);
    } else {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, 5000);
    }
    CloseHandle(process.hProcess);
    if (exitCode != 0) throw std::runtime_error("Extract ANR package");
}

} // namespace

fs::path CachedAnrProvider() {
    const auto& root = CacheDirectory();
    const auto folder = root / L"anr-fg";
    if (!IsCacheDirectory(root) || !IsCacheDirectory(folder)) return {};
    const auto provider = folder / L"nvngx_dlssg.dll";
    const auto marker = folder / L"package.sha256";
    std::error_code error;
    if (fs::file_size(marker, error) != sizeof(kAssetSha256) - 1) return {};
    std::ifstream stream(marker, std::ios::binary);
    std::string hash(std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{});
    if (hash == kAssetSha256 && fs::is_regular_file(provider, error) &&
        fs::is_regular_file(folder / L"path_table.json", error) &&
        fs::is_regular_file(folder / L"anr-vkloader.dll", error)) {
        return provider;
    }
    return {};
}

fs::path InstallAnrPackage(AnrDownloadProgress& progress) {
    const auto& cache = CacheDirectory();
    if (!IsCacheDirectory(cache)) throw std::runtime_error("Invalid VTSFloat_Meow cache directory");
    const auto root = fs::canonical(cache);
    InstallWorkspace workspace(root);
    const auto archive = workspace.path / L"anr-fg.zip";
    for (const bool proxy : {false, true}) {
        try {
            progress.downloaded = 0;
            update::DownloadVerifiedAsset(kAssetUrl, archive, kAnrPackageSize, kAssetSha256,
                progress.cancelled, proxy, [&](std::uint64_t bytes) { progress.downloaded = bytes; });
            break;
        } catch (...) {
            if (proxy || progress.cancelled) throw;
        }
    }
    if (progress.cancelled) throw std::runtime_error("ANR download cancelled");
    progress.installing = true;
    std::array<wchar_t, MAX_PATH> systemBuffer{};
    const UINT length = GetSystemDirectoryW(systemBuffer.data(), static_cast<UINT>(systemBuffer.size()));
    if (!length || length >= systemBuffer.size()) throw std::runtime_error("Find Windows system directory");
    const fs::path systemDirectory(systemBuffer.data());
    const auto payload = workspace.path / L"payload";
    fs::create_directory(payload);
    ExtractProvider(systemDirectory, archive, payload, progress);
    if (!fs::is_regular_file(payload / L"nvngx_dlssg.dll") || !fs::is_regular_file(payload / L"path_table.json")) {
        throw std::runtime_error("ANR archive does not contain the expected provider");
    }
    fs::copy_file(systemDirectory / L"vulkan-1.dll", payload / L"anr-vkloader.dll");
    if (progress.cancelled) throw std::runtime_error("ANR download cancelled");

    const auto folder = root / L"anr-fg";
    EnsureCacheDirectory(folder);
    const auto marker = folder / L"package.sha256";
    fs::remove(marker);
    // A previous reset may have left ANR logs here. Publish the verified
    // component files separately and write the completion marker last.
    std::vector<fs::directory_entry> entries;
    for (const auto& entry : fs::recursive_directory_iterator(payload)) entries.push_back(entry);
    for (const auto& entry : entries) {
        const auto target = folder / entry.path().lexically_relative(payload);
        if (entry.is_directory()) {
            EnsureCacheDirectory(target);
        } else if (!MoveFileExW(entry.path().c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            throw std::runtime_error("Install ANR component file");
        }
    }
    const auto ready = workspace.path / L"package.sha256";
    std::ofstream stream(ready, std::ios::binary);
    stream << kAssetSha256;
    stream.close();
    if (!stream || !MoveFileExW(ready.c_str(), marker.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw std::runtime_error("Finish ANR cache installation");
    }
    return folder / L"nvngx_dlssg.dll";
}

} // namespace vtsfloat
