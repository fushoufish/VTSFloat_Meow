#include <windows.h>
#include <tlhelp32.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <avrt.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <winhttp.h>
#include <shlwapi.h>
#include <iphlpapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "SpoutDX.h"
#include "resource.h"

using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;

namespace {

constexpr wchar_t kWindowClass[] = L"LilyVtsLayeredOverlay";
constexpr wchar_t kToolbarClass[] = L"LilyVtsLayeredOverlayToolbar";
constexpr wchar_t kStatusClass[] = L"LilyVtsVtsStatus";
constexpr wchar_t kWindowTitle[] = L"VTSFloat_Meow";
constexpr wchar_t kToolbarTitle[] = L"VTSFloat_Meow Controls";
constexpr wchar_t kInstanceName[] = L"Local\\LilyVtsLayeredOverlayInstance";
constexpr wchar_t kVtsExecutableName[] = L"VTube Studio.exe";
constexpr wchar_t kVtsBatchName[] = L"start_without_steam.bat";
constexpr int kHotkeyId = 1;
constexpr UINT kSetLockedMessage = WM_APP + 1;
constexpr UINT kInteractiveRenderMessage = WM_APP + 2;
constexpr UINT kTrayCallbackMessage = WM_APP + 3;
constexpr UINT kTrayIconId = 1;
constexpr UINT kTrayResetCommand = 4101;
constexpr UINT kTrayToggleVisibilityCommand = 4102;
constexpr UINT kTrayExitCommand = 4103;
constexpr UINT kStatusLaunchExeCommand = 5101;
constexpr UINT kStatusLaunchBatchCommand = 5102;
// 1280x773 VTube Studio output scaled to the user's established 640px width.
// 387 is the correctly rounded height; 386 caused the old startup stretch.
constexpr int kDefaultWidth = 640;
constexpr int kDefaultHeight = 387;
constexpr int kResetWidth = 1280;
constexpr int kResetHeight = 720;
constexpr int kResizeGrip = 10;
constexpr int kCornerLength = 18;
constexpr int kToolbarHeight = 42;
constexpr int kToolbarDebugHeight = 68;
constexpr int kToolbarDebugModeHeight = 86;
constexpr int kToolbarCompactHeight = 122;
constexpr int kToolbarLockedHeight = 32;
constexpr int kLockedDebugLabelWidth = 180;
constexpr int kToolbarGap = 5;
constexpr int kMinimumWidth = 640;
constexpr int kToolbarWideMinimumWidth = 1000;
constexpr int kMinimumFps = 1;
constexpr int kMaximumFps = 240;
constexpr int kScalingPerformance = 0;
constexpr int kScalingBalanced = 1;
constexpr int kScalingQuality = 2;
constexpr int kDefaultHoverOpacityPercent = 45;
constexpr int kMinimumHoverOpacityPercent = 0;
constexpr int kApiNotificationHeight = 28;
constexpr int kBgNotificationHeight = 28;
constexpr int kHoverFadeDurationMs = 350;
constexpr int kBorderModeCustom = 0;
constexpr int kBorderModeChase = 1;
constexpr int kBorderModeNormal = 2;
constexpr int kDefaultBorderThickness = 6;
constexpr int kMaximumBorderThickness = 50;
constexpr COLORREF kDefaultCustomBorderColor = RGB(24, 132, 255);
constexpr wchar_t kGithubUrl[] = L"https://github.com/fushoufish/VTSFloat_Meow";

enum class ToolbarButton {
    None,
    Gpu,
    Quality,
    FrameRate,
    GraphicsSettings,
    Aspect,
    Border,
    Reset,
    Monitor,
    Hotkey,
    Debug,
    Lock,
    Github,
    Hide,
    Close,
    Unlock,
};

enum class VtsStatusMode {
    Hidden,
    WaitingForVts,
    WaitingForSpout,
    LaunchChoices,
};

enum class FpsMode {
    Fixed,
    FollowVts,
    FollowMonitor,
    FollowVtsApi,
};

struct GpuAdapterInfo {
    UINT index = 0;
    std::wstring name;
    LUID luid{};
};

// A precomputed source-coordinate pair used by the CPU bilinear scaler.
// Fractions are fixed-point 0..256 so per-frame scaling avoids floats.
struct BilinearSample {
    UINT first = 0;
    UINT second = 0;
    unsigned fraction = 0;
};

std::filesystem::path LogPath() {
    wchar_t localAppData[MAX_PATH]{};
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, ARRAYSIZE(localAppData))) {
        return L"vts_overlay_layered.log";
    }
    return std::filesystem::path(localAppData) / L"vts_overlay_layered.log";
}

std::filesystem::path DesktopLogPath() {
    wchar_t path[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, path) == S_OK) {
        // Keep one predictable diagnostic file.  A timestamped file was
        // created by every exception callback, which produced several copies
        // of the same full history whenever shutdown cascaded through more
        // than one handler.
        return std::filesystem::path(path) / L"VTSFloat_Meow_crash.log";
    }
    return L"VTSFloat_Meow_crash.log";
}

std::atomic_flag gCrashDumpWritten = ATOMIC_FLAG_INIT;

void CopyLogToDesktop(const std::string& reason) {
    if (gCrashDumpWritten.test_and_set(std::memory_order_acq_rel)) {
        return;
    }
    try {
        const auto src = LogPath();
        const auto dst = DesktopLogPath();
        std::ofstream out(dst, std::ios::trunc);
        out << "===== CRASH / EXIT DUMP =====\n"
            << "Reason: " << reason << "\n"
            << "===== LOG TAIL (last 200 lines) =====\n";
        std::ifstream in(src, std::ios::binary);
        if (in) {
            std::deque<std::string> tail;
            std::string line;
            while (std::getline(in, line)) {
                if (tail.size() >= 200) {
                    tail.pop_front();
                }
                tail.push_back(std::move(line));
            }
            for (const auto& item : tail) {
                out << item << '\n';
            }
        }
    } catch (...) {}
}

LONG WINAPI VectoredExceptionHandler(EXCEPTION_POINTERS* info) {
    // 0xE06D7363 is the normal MSVC C++ exception dispatch code.  It can be
    // caught by the owning thread (including the VTS API worker) and must not
    // be treated as an application crash by the early vectored callback.
    if (info && info->ExceptionRecord &&
        info->ExceptionRecord->ExceptionCode == 0xE06D7363) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    std::ostringstream reason;
    reason << "Vectored exception 0x" << std::hex << std::uppercase
           << (info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0)
           << " at 0x"
           << (info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr);
    CopyLogToDesktop(reason.str());
    return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* info) {
    std::ostringstream reason;
    reason << "SEH exception 0x" << std::hex << std::uppercase
           << (info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0)
           << " at 0x"
           << (info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr);
    CopyLogToDesktop(reason.str());
    return EXCEPTION_EXECUTE_HANDLER;
}

void PurecallHandler() {
    CopyLogToDesktop("pure virtual call");
    std::abort();
}

void InvalidParamHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t) {
    CopyLogToDesktop("CRT invalid parameter");
    std::abort();
}

std::filesystem::path ConfigPath() {
    wchar_t localAppData[MAX_PATH]{};
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, ARRAYSIZE(localAppData))) {
        return L"vts_overlay_layered.ini";
    }
    return std::filesystem::path(localAppData) / L"vts_overlay_layered.ini";
}

int ReadConfigInt(const wchar_t* section, const wchar_t* key, int fallback) {
    wchar_t value[64]{};
    const std::wstring path = ConfigPath().wstring();
    GetPrivateProfileStringW(section, key, L"", value, ARRAYSIZE(value), path.c_str());
    if (!value[0]) {
        return fallback;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

void WriteConfigInt(const wchar_t* section, const wchar_t* key, int value) {
    const std::wstring text = std::to_wstring(value);
    const std::wstring path = ConfigPath().wstring();
    WritePrivateProfileStringW(section, key, text.c_str(), path.c_str());
}

std::wstring ReadConfigString(
    const wchar_t* section, const wchar_t* key, const wchar_t* fallback = L"") {
    wchar_t value[2048]{};
    const std::wstring path = ConfigPath().wstring();
    GetPrivateProfileStringW(
        section, key, fallback, value, ARRAYSIZE(value), path.c_str());
    return value;
}

void WriteConfigString(
    const wchar_t* section, const wchar_t* key, const std::wstring& value) {
    const std::wstring path = ConfigPath().wstring();
    WritePrivateProfileStringW(section, key, value.c_str(), path.c_str());
}

bool IsHighFrequencyPerfLog(const std::string& line) {
    // Performance snapshots are rendered in the in-app debug panel.  They
    // are intentionally not persisted: writing a multi-line sample every
    // couple of seconds makes the long-lived log grow rapidly and provides
    // little diagnostic value compared with errors and state transitions.
    return line.rfind("[layered perf]", 0) == 0 ||
        line.rfind("[bridge perf]", 0) == 0 ||
        line.rfind("[dx11 perf]", 0) == 0 ||
        line.rfind("[native perf]", 0) == 0 ||
        line.rfind("[perf]", 0) == 0;
}

void Log(const std::string& line) {
    if (IsHighFrequencyPerfLog(line)) {
        return;
    }
    std::ofstream output(LogPath(), std::ios::app);
    SYSTEMTIME time{};
    GetLocalTime(&time);
    output << '['
           << std::setfill('0') << std::setw(4) << time.wYear << '-'
           << std::setw(2) << time.wMonth << '-'
           << std::setw(2) << time.wDay << ' '
           << std::setw(2) << time.wHour << ':'
           << std::setw(2) << time.wMinute << ':'
           << std::setw(2) << time.wSecond << '.'
           << std::setw(3) << time.wMilliseconds << "] "
           << line << std::endl;
}

std::string HresultText(HRESULT result) {
    std::ostringstream stream;
    stream << "HRESULT 0x" << std::hex << std::uppercase
           << static_cast<unsigned long>(result);
    return stream.str();
}

void Check(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        throw std::runtime_error(std::string(operation) + " failed: " + HresultText(result));
    }
}

std::string WideToUtf8(const wchar_t* text) {
    const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) {
        return {};
    }
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), length, nullptr, nullptr);
    result.resize(static_cast<size_t>(length - 1));
    return result;
}

std::wstring ReadCpuModelName() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            0, KEY_READ, &key) != ERROR_SUCCESS) {
        return L"CPU";
    }
    wchar_t value[256]{};
    DWORD type = 0;
    DWORD bytes = sizeof(value);
    const LONG result = RegQueryValueExW(
        key, L"ProcessorNameString", nullptr, &type,
        reinterpret_cast<LPBYTE>(value), &bytes);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS || type != REG_SZ || value[0] == L'\0') {
        return L"CPU";
    }
    std::wstring name(value);
    while (!name.empty() && iswspace(name.back())) {
        name.pop_back();
    }
    return name.empty() ? L"CPU" : name;
}

struct VtsFpsConfig {
    int fps = 0;
    std::wstring mode;
};

std::optional<VtsFpsConfig> ReadVtsConfiguredFps(int monitorRefreshFps) {
    wchar_t localAppData[MAX_PATH]{};
    if (!GetEnvironmentVariableW(
            L"LOCALAPPDATA", localAppData, ARRAYSIZE(localAppData))) {
        return std::nullopt;
    }
    const std::filesystem::path logPath =
        std::filesystem::path(localAppData).parent_path() /
        L"LocalLow\\Denchi\\VTube Studio\\Player.log";

    // Use Win32 API to open with sharing (VTS may hold the file open)
    HANDLE hFile = CreateFileW(
        logPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }
    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart <= 0) {
        CloseHandle(hFile);
        return std::nullopt;
    }
    constexpr LONGLONG kTailBytes = 64 * 1024;
    const LONGLONG readOffset =
        fileSize.QuadPart > kTailBytes ? fileSize.QuadPart - kTailBytes : 0;
    const DWORD readSize = static_cast<DWORD>(
        (std::min)(fileSize.QuadPart, kTailBytes));
    LARGE_INTEGER seekPos{};
    seekPos.QuadPart = readOffset;
    SetFilePointerEx(hFile, seekPos, nullptr, FILE_BEGIN);
    std::string tail(readSize, '\0');
    DWORD bytesRead = 0;
    ReadFile(hFile, tail.data(), readSize, &bytesRead, nullptr);
    CloseHandle(hFile);
    if (bytesRead == 0) {
        return std::nullopt;
    }
    tail.resize(bytesRead);

    const size_t fixedPosition = tail.rfind("FPS limited to ");
    const size_t vsyncHalfPosition = tail.rfind("FPS limit set to VSYNC_HALF");
    const size_t vsyncPosition = tail.rfind("FPS limit set to VSYNC");

    // Find the last (most recent) matching line in the log.
    // Use a helper that treats npos as "not found" (less than any real position).
    size_t position = std::string::npos;
    enum class Mode { Fixed, VsyncHalf, Vsync } mode = Mode::Fixed;

    if (fixedPosition != std::string::npos) {
        position = fixedPosition;
        mode = Mode::Fixed;
    }
    if (vsyncHalfPosition != std::string::npos &&
        (position == std::string::npos || vsyncHalfPosition > position)) {
        position = vsyncHalfPosition;
        mode = Mode::VsyncHalf;
    }
    if (vsyncPosition != std::string::npos &&
        (position == std::string::npos || vsyncPosition > position) &&
        vsyncPosition != vsyncHalfPosition) {
        position = vsyncPosition;
        mode = Mode::Vsync;
    }
    if (position == std::string::npos) {
        return std::nullopt;
    }
    if (mode == Mode::VsyncHalf) {
        return VtsFpsConfig{ (std::max)(1, monitorRefreshFps / 2), L"VSYNC_HALF" };
    }
    if (mode == Mode::Vsync) {
        return VtsFpsConfig{ monitorRefreshFps, L"VSYNC" };
    }
    const size_t numberStart = position + std::string("FPS limited to ").size();
    size_t numberEnd = numberStart;
    while (numberEnd < tail.size() &&
           tail[numberEnd] >= '0' && tail[numberEnd] <= '9') {
        ++numberEnd;
    }
    if (numberEnd == numberStart) {
        return std::nullopt;
    }
    try {
        const int fps = (std::clamp)(
            std::stoi(tail.substr(numberStart, numberEnd - numberStart)),
            kMinimumFps, kMaximumFps);
        return VtsFpsConfig{ fps, L"FPS_" + std::to_wstring(fps) };
    } catch (...) {
        return std::nullopt;
    }
}

constexpr wchar_t kVtsApiPluginName[] = L"VTSFloat_Meow";
constexpr wchar_t kVtsApiPluginDeveloper[] = L"Lily";
constexpr int kVtsApiPort = 8001;
constexpr int kVtsApiPollIntervalMs = 2000;
// Keep API recovery responsive after the initial probe. The silent retry path
// uses this cadence for the INI-saved endpoint only.
constexpr int kVtsApiReconnectDelayMs = 600;
constexpr int kVtsApiModelReadyDelayMs = 2500;

std::string Base64Encode(const std::vector<BYTE>& data) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        const uint32_t b0 = data[i];
        const uint32_t b1 = i + 1 < data.size() ? data[i + 1] : 0;
        const uint32_t b2 = i + 2 < data.size() ? data[i + 2] : 0;
        const uint32_t v = (b0 << 16) | (b1 << 8) | b2;
        result += alphabet[(v >> 18) & 0x3F];
        result += alphabet[(v >> 12) & 0x3F];
        result += i + 1 < data.size() ? alphabet[(v >> 6) & 0x3F] : '=';
        result += i + 2 < data.size() ? alphabet[v & 0x3F] : '=';
    }
    return result;
}

std::string LoadIconAsBase64Png(HINSTANCE instance) {
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(1302 /*IDR_PLUGIN_LOGO*/), RT_RCDATA);
    if (!resource) return {};
    HGLOBAL loaded = LoadResource(instance, resource);
    if (!loaded) return {};
    const void* data = LockResource(loaded);
    const DWORD size = SizeofResource(instance, resource);
    if (!data || size == 0) return {};

    Gdiplus::GdiplusStartupInput input{};
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) return {};

    IStream* srcStream = SHCreateMemStream(
        static_cast<const BYTE*>(data), size);
    if (!srcStream) { Gdiplus::GdiplusShutdown(token); return {}; }
    Gdiplus::Bitmap* srcBitmap = Gdiplus::Bitmap::FromStream(srcStream);
    srcStream->Release();
    if (!srcBitmap || srcBitmap->GetLastStatus() != Gdiplus::Ok) {
        delete srcBitmap;
        Gdiplus::GdiplusShutdown(token);
        return {};
    }

    Gdiplus::Bitmap resized(128, 128, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics graphics(&resized);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.DrawImage(srcBitmap, 0, 0, 128, 128);
    }
    delete srcBitmap;

    IStream* outStream = nullptr;
    if (CreateStreamOnHGlobal(nullptr, TRUE, &outStream) != S_OK) {
        Gdiplus::GdiplusShutdown(token);
        return {};
    }

    CLSID pngClsid{};
    UINT numEncoders = 0, sizeEncoders = 0;
    Gdiplus::GetImageEncodersSize(&numEncoders, &sizeEncoders);
    std::vector<BYTE> encoderBuf(sizeEncoders);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(encoderBuf.data());
    Gdiplus::GetImageEncoders(numEncoders, sizeEncoders, encoders);
    for (UINT i = 0; i < numEncoders; ++i) {
        if (wcscmp(encoders[i].MimeType, L"image/png") == 0) {
            pngClsid = encoders[i].Clsid;
            break;
        }
    }
    resized.Save(outStream, &pngClsid, nullptr);

    HGLOBAL hGlobal = nullptr;
    GetHGlobalFromStream(outStream, &hGlobal);
    const SIZE_T pngSize = GlobalSize(hGlobal);
    std::vector<BYTE> pngBytes(pngSize);
    void* p = GlobalLock(hGlobal);
    if (p) {
        memcpy(pngBytes.data(), p, pngSize);
        GlobalUnlock(hGlobal);
    }
    outStream->Release();
    Gdiplus::GdiplusShutdown(token);
    return Base64Encode(pngBytes);
}

std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int len = MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring result(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
        result.data(), len);
    return result;
}

std::string BuildJsonRequest(const std::string& type, const std::string& dataFields = "") {
    std::string json = R"({"apiName":"VTubeStudioPublicAPI","apiVersion":"1.0","requestID":"vtsfloat",)";
    json += R"("messageType":")" + type + R"(",)";
    json += R"("data":{)" + dataFields + R"(}})";
    return json;
}

std::string EscapeJsonString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char c : value) {
        switch (c) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped += c; break;
        }
    }
    return escaped;
}

std::string DecodeJsonString(const std::string& raw) {
    std::string result;
    result.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (c == '\\' && i + 1 < raw.size()) {
            char next = raw[i + 1];
            if (next == 'u' && i + 5 < raw.size()) {
                uint32_t cp = 0;
                bool ok = true;
                for (int k = 0; k < 4; ++k) {
                    char h = raw[i + 2 + k];
                    cp <<= 4;
                    if (h >= '0' && h <= '9') cp |= h - '0';
                    else if (h >= 'a' && h <= 'f') cp |= 10 + h - 'a';
                    else if (h >= 'A' && h <= 'F') cp |= 10 + h - 'A';
                    else { ok = false; break; }
                }
                if (ok) {
                    // Handle surrogate pair
                    if (cp >= 0xD800 && cp <= 0xDBFF && i + 11 < raw.size() &&
                        raw[i + 6] == '\\' && raw[i + 7] == 'u') {
                        uint32_t low = 0;
                        bool ok2 = true;
                        for (int k = 0; k < 4; ++k) {
                            char h = raw[i + 8 + k];
                            low <<= 4;
                            if (h >= '0' && h <= '9') low |= h - '0';
                            else if (h >= 'a' && h <= 'f') low |= 10 + h - 'a';
                            else if (h >= 'A' && h <= 'F') low |= 10 + h - 'A';
                            else { ok2 = false; break; }
                        }
                        if (ok2 && low >= 0xDC00 && low <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                            i += 10;
                        } else {
                            i += 4;
                        }
                    } else {
                        i += 4;
                    }
                    // Encode cp as UTF-8
                    if (cp < 0x80) {
                        result += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        result += static_cast<char>(0xC0 | (cp >> 6));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else if (cp < 0x10000) {
                        result += static_cast<char>(0xE0 | (cp >> 12));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        result += static_cast<char>(0xF0 | (cp >> 18));
                        result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    i += 1;
                    continue;
                }
            }
            switch (next) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                default: result += next; break;
            }
            ++i;
        } else {
            result += c;
        }
    }
    return result;
}

std::string ExtractJsonString(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return {};
    ++pos;
    // Scan for closing " while respecting escape sequences
    size_t end = pos;
    while (end < json.size() && json[end] != '"') {
        if (json[end] == '\\' && end + 1 < json.size()) end += 2;
        else ++end;
    }
    if (end >= json.size()) return {};
    return DecodeJsonString(json.substr(pos, end - pos));
}

int ExtractJsonInt(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return -1;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return -1;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    if (pos >= json.size()) return -1;
    bool negative = false;
    if (json[pos] == '-') { negative = true; ++pos; }
    int value = 0;
    bool found = false;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        value = value * 10 + (json[pos] - '0');
        found = true;
        ++pos;
    }
    if (!found) return -1;
    return negative ? -value : value;
}

long long ExtractJsonLong(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return -1;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return -1;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    if (pos >= json.size()) return -1;
    bool negative = false;
    if (json[pos] == '-') { negative = true; ++pos; }
    long long value = 0;
    bool found = false;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        value = value * 10 + (json[pos] - '0');
        found = true;
        ++pos;
    }
    if (!found) return -1;
    return negative ? -value : value;
}

bool ExtractJsonBool(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return false;
    return json.find("true", pos) == pos + 1 ||
           json.find("true", pos) < pos + 5;
}

struct VtsExpressionCommand {
    std::string file;
    bool active = false;
    double fadeTime = 0.3;
};

// VTS API WebSocket client - runs in a background thread.
class VtsApiClient {
public:
    struct ExpressionInfo {
        std::string file;
        std::string name;
        bool active = false;
    };

    void Start(HINSTANCE instance) {
        if (running_.exchange(true)) return;
        instance_ = instance;
        const int savedPort = ReadConfigInt(L"VtsApi", L"Port", 0);
        portConfigured_ = savedPort > 0 && savedPort <= 65535;
        port_ = portConfigured_ ? savedPort : kVtsApiPort;
        initialDiscoveryStarted_.store(false, std::memory_order_relaxed);
        initialDiscoveryDone_.store(false, std::memory_order_relaxed);
        authPending_.store(false, std::memory_order_relaxed);
        thread_ = std::thread([this]() { ThreadMain(); });
    }

    void Stop() {
        running_.store(false);
        if (thread_.joinable()) thread_.join();
    }

    int GetRealtimeFps() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return realtimeFps_;
    }

    struct ExtraStats {
        long long uptimeMs = 0;
        std::string modelId;
        int artmeshCount = 0;
        int itemCount = 0;
        int vtsWindowWidth = 0;
        int vtsWindowHeight = 0;
        double apiLatencyMs = 0.0;
    };

    ExtraStats GetExtraStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return extraStats_;
    }

    std::vector<ExpressionInfo> GetExpressions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return expressions_;
    }

    bool GetExpressionActive(const std::string& file, bool& active) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& expression : expressions_) {
            if (expression.file == file) {
                active = expression.active;
                return true;
            }
        }
        return false;
    }

    void RequestExpressionState() {
        expressionStateRequested_.store(true, std::memory_order_relaxed);
    }

    void SetExpressionActive(const std::string& file, bool active, double fadeTime = 0.3) {
        if (file.empty()) return;
        std::lock_guard<std::mutex> lock(expressionCommandMutex_);
        // Keep the queue bounded and collapse repeated updates for the same
        // expression. Hover transitions can otherwise enqueue stale toggles
        // while VTS is reconnecting.
        for (auto it = expressionCommands_.begin(); it != expressionCommands_.end();) {
            if (it->file == file) it = expressionCommands_.erase(it);
            else ++it;
        }
        expressionCommands_.push_back(VtsExpressionCommand{ file, active, fadeTime });
    }

    void ResetCache() {
        Stop();
        cachedToken_.clear();
        activePort_ = 0;
        connected_.store(false);
        everAuthenticated_.store(false);
        scanning_.store(false);
        scanRequested_.store(false);
        scanResult_.store(0);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            realtimeFps_ = 0;
            extraStats_ = ExtraStats{};
            expressions_.clear();
            lastExpressionModelId_.clear();
        }
        {
            std::lock_guard<std::mutex> lock(expressionCommandMutex_);
            expressionCommands_.clear();
        }
        expressionStateRequested_.store(false, std::memory_order_relaxed);
        initialDiscoveryStarted_.store(false, std::memory_order_relaxed);
        initialDiscoveryDone_.store(false, std::memory_order_relaxed);
        authPending_.store(false, std::memory_order_relaxed);
    }

    bool IsConnected() const {
        return connected_.load();
    }

    bool IsInitialDiscoveryDone() const {
        return initialDiscoveryDone_.load(std::memory_order_relaxed);
    }

    bool IsAwaitingAuthorization() const {
        return authPending_.load(std::memory_order_relaxed);
    }

    bool HasConfiguredPort() const {
        return portConfigured_;
    }

    bool IsScanning() const {
        return scanning_.load();
    }

    int GetScanResult() const {
        return scanResult_.load();
    }

    void ClearScanResult() {
        scanResult_.store(0);
    }

    void RequestScan() {
        scanResult_.store(0);
        scanRequested_.store(true);
    }

    bool WasEverAuthenticated() const {
        return everAuthenticated_.load();
    }

private:
    void ThreadMain() {
        try {
            ThreadMainImpl();
        } catch (const std::exception& e) {
            Log(std::string("[vts-api] thread exception: ") + e.what());
        } catch (...) {
            Log("[vts-api] thread unknown exception");
        }
    }

    void ThreadMainImpl() {
        LoadCachedToken();
        Log("[vts-api] thread started");
        while (running_.load()) {
            bool connectedToApi = false;
            if (!initialDiscoveryStarted_.exchange(true, std::memory_order_relaxed)) {
                Log("[vts-api] initial discovery started");
                connectedToApi = ConnectInitial();
                initialDiscoveryDone_.store(true, std::memory_order_relaxed);
            } else {
                connectedToApi = ConnectSilent();
            }
            if (!connectedToApi) {
                const int step = 100;
                for (int elapsed = 0; elapsed < kVtsApiReconnectDelayMs && running_.load(); elapsed += step) {
                    if (scanRequested_.load()) break;
                    Sleep(step);
                }
                continue;
            }
            if (!Authenticate()) {
                Disconnect();
                SleepMs(kVtsApiReconnectDelayMs);
                continue;
            }
            connected_.store(true);
            everAuthenticated_.store(true);
            RequestExpressionState();
            if (activePort_ > 0) {
                if (activePort_ != port_) {
                    port_ = activePort_;
                }
                portConfigured_ = true;
                WriteConfigInt(L"VtsApi", L"Port", activePort_);
                Log("[vts-api] saved port " + std::to_string(activePort_) + " to config");
            }
            while (running_.load()) {
                if (!PollStatistics()) {
                    break;
                }
                SleepMs(kVtsApiPollIntervalMs);
            }
            connected_.store(false);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                realtimeFps_ = 0;
                extraStats_ = ExtraStats{};
            }
            Disconnect();
            if (running_.load()) SleepMs(kVtsApiReconnectDelayMs);
        }
    }

    bool ScanRequestedPorts() {
        if (scanRequested_.exchange(false)) {
            scanning_.store(true);
            std::vector<int> ports = FindVtsListeningPorts();
            for (int port : ports) {
                if (!running_.load()) { scanning_.store(false); return false; }
                if (TryConnectPort(port) && ProbeIsVtsApi()) {
                    scanning_.store(false);
                    scanResult_.store(1);
                    return true;
                }
                Disconnect();
            }
            scanning_.store(false);
            scanResult_.store(2);
            return false;
        }
        return false;
    }

    bool ConnectInitial() {
        if (!running_.load()) return false;
        if (portConfigured_) {
            for (int attempt = 0; attempt < 2 && running_.load(); ++attempt) {
                if (TryConnectPort(port_, 150, 150) && ProbeIsVtsApi()) {
                    return true;
                }
                Disconnect();
            }
        }

        constexpr struct Candidate {
            int port;
            DWORD timeoutMs;
        } candidates[] = { { 8001, 100 }, { 8002, 50 }, { 8000, 50 } };
        for (const Candidate& candidate : candidates) {
            if (!running_.load()) return false;
            if (portConfigured_ && candidate.port == port_) continue;
            if (TryConnectPort(candidate.port, candidate.timeoutMs, candidate.timeoutMs) &&
                ProbeIsVtsApi()) {
                return true;
            }
            Disconnect();
        }
        return false;
    }

    bool ConnectSilent() {
        if (scanRequested_.load()) {
            return ScanRequestedPorts();
        }
        if (!portConfigured_ || !running_.load()) return false;
        if (TryConnectPort(port_, 100, 100) && ProbeIsVtsApi()) {
            return true;
        }
        Disconnect();
        return false;
    }

    bool ProbeIsVtsApi() {
        // Send an APIStateRequest (no authentication needed).
        // Verify the response is from VTS API.
        if (!SendJson(BuildJsonRequest("APIStateRequest"))) {
            Log("[vts-api] probe send failed");
            return false;
        }
        std::string response = ReceiveJson();
        if (response.empty()) {
            Log("[vts-api] probe recv empty");
            return false;
        }
        const std::string apiName = ExtractJsonString(response, "apiName");
        if (apiName != "VTubeStudioPublicAPI") {
            Log("[vts-api] probe: not VTS API on port " + std::to_string(activePort_) +
                " (got apiName='" + apiName.substr(0, 50) + "')");
            return false;
        }
        Log("[vts-api] probe OK on port " + std::to_string(activePort_));
        return true;
    }

    std::vector<int> FindVtsListeningPorts() {
        std::vector<int> result;
        DWORD vtsPid = 0;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            if (Process32FirstW(snapshot, &entry)) {
                do {
                    if (_wcsicmp(entry.szExeFile, L"VTube Studio.exe") == 0) {
                        vtsPid = entry.th32ProcessID;
                        break;
                    }
                } while (Process32NextW(snapshot, &entry));
            }
            CloseHandle(snapshot);
        }
        if (vtsPid == 0) return result;

        DWORD size = 0;
        GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0);
        if (size == 0) return result;
        std::vector<BYTE> buffer(size);
        if (GetExtendedTcpTable(buffer.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0) != NO_ERROR) {
            return result;
        }
        auto* table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buffer.data());
        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            if (table->table[i].dwOwningPid == vtsPid) {
                const DWORD raw = table->table[i].dwLocalPort;
                // dwLocalPort stores network byte order in the low 16 bits
                const int port = static_cast<int>(
                    ((raw & 0xFF) << 8) | ((raw >> 8) & 0xFF));
                Log("[vts-api] found VTS listening port " + std::to_string(port));
                if (port > 0 && port <= 65535) {
                    result.push_back(port);
                }
            }
        }
        return result;
    }

    bool TryConnectPort(int port, DWORD connectTimeoutMs = 200, DWORD ioTimeoutMs = 500) {
        activePort_ = 0;
        hSession_ = WinHttpOpen(
            L"VTSFloat_Meow/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession_) return false;

        DWORD connectTimeout = connectTimeoutMs;
        WinHttpSetOption(hSession_, WINHTTP_OPTION_CONNECT_TIMEOUT, &connectTimeout, sizeof(connectTimeout));
        DWORD ioTimeout = ioTimeoutMs;
        WinHttpSetOption(hSession_, WINHTTP_OPTION_SEND_TIMEOUT, &ioTimeout, sizeof(ioTimeout));
        WinHttpSetOption(hSession_, WINHTTP_OPTION_RECEIVE_TIMEOUT, &ioTimeout, sizeof(ioTimeout));

        hConnect_ = WinHttpConnect(
            hSession_, L"localhost", static_cast<INTERNET_PORT>(port), 0);
        if (!hConnect_) { CleanupHandles(); return false; }

        hRequest_ = WinHttpOpenRequest(
            hConnect_, L"GET", L"/", nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hRequest_) { CleanupHandles(); return false; }

        if (!WinHttpSetOption(hRequest_, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) {
            CleanupHandles(); return false;
        }
        if (!WinHttpSendRequest(hRequest_, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0)) {
            CleanupHandles(); return false;
        }
        if (!WinHttpReceiveResponse(hRequest_, nullptr)) {
            CleanupHandles(); return false;
        }
        hWebSocket_ = WinHttpWebSocketCompleteUpgrade(hRequest_, 0);
        if (!hWebSocket_) { CleanupHandles(); return false; }

        WinHttpCloseHandle(hRequest_);
        hRequest_ = nullptr;
        activePort_ = port;
        Log("[vts-api] WebSocket connected on port " + std::to_string(port));
        return true;
    }

    void Disconnect() {
        if (hWebSocket_) {
            WinHttpWebSocketClose(hWebSocket_, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
            WinHttpCloseHandle(hWebSocket_);
            hWebSocket_ = nullptr;
        }
        CleanupHandles();
        Log("[vts-api] disconnected");
    }

    void CleanupHandles() {
        if (hRequest_) { WinHttpCloseHandle(hRequest_); hRequest_ = nullptr; }
        if (hConnect_) { WinHttpCloseHandle(hConnect_); hConnect_ = nullptr; }
        if (hSession_) { WinHttpCloseHandle(hSession_); hSession_ = nullptr; }
    }

    bool SendJson(const std::string& json) {
        if (!hWebSocket_) return false;
        DWORD err = WinHttpWebSocketSend(
            hWebSocket_, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
            const_cast<char*>(json.data()), static_cast<DWORD>(json.size()));
        return err == ERROR_SUCCESS;
    }

    std::string ReceiveJson() {
        if (!hWebSocket_) return {};
        std::string buffer(8192, '\0');
        DWORD bytesRead = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType{};
        DWORD err = WinHttpWebSocketReceive(
            hWebSocket_, buffer.data(), static_cast<DWORD>(buffer.size()),
            &bytesRead, &bufferType);
        if (err != ERROR_SUCCESS || bytesRead == 0) return {};
        buffer.resize(bytesRead);
        return buffer;
    }

    // VTS_API_CLASS_PLACEHOLDER_2

    bool Authenticate() {
        if (cachedToken_.empty()) {
            return RequestNewToken();
        }
        std::string data = R"("pluginName":"VTSFloat_Meow","pluginDeveloper":"Lily","authenticationToken":")" + cachedToken_ + R"(")";
        std::string request = BuildJsonRequest("AuthenticationRequest", data);
        if (!SendJson(request)) return false;
        std::string response = ReceiveJson();
        if (response.empty()) return false;

        if (ExtractJsonBool(response, "authenticated")) {
            Log("[vts-api] authenticated with cached token");
            return true;
        }
        Log("[vts-api] cached token rejected, requesting new one");
        cachedToken_.clear();
        everAuthenticated_.store(false);
        return RequestNewToken();
    }

    bool RequestNewToken() {
        Log("[vts-api] requesting new token");
        authPending_.store(true, std::memory_order_relaxed);
        const std::string iconB64;
        std::string data = R"("pluginName":"VTSFloat_Meow","pluginDeveloper":"Lily","pluginIcon":")" + iconB64 + R"(")";
        std::string request = BuildJsonRequest("AuthenticationTokenRequest", data);
        if (!SendJson(request)) {
            authPending_.store(false, std::memory_order_relaxed);
            return false;
        }
        std::string response = ReceiveJson();
        if (response.empty()) {
            // VTS keeps the authorization dialog open while waiting for the
            // user. Leave this state visible to the toolbar; the API thread
            // will retry after the short reconnect interval if the request
            // timed out.
            return false;
        }

        std::string token = ExtractJsonString(response, "authenticationToken");
        if (token.empty()) {
            authPending_.store(false, std::memory_order_relaxed);
            Log("[vts-api] token request failed: " + ExtractJsonString(response, "message"));
            return false;
        }
        cachedToken_ = token;
        SaveToken();
        Log("[vts-api] received new token");

        std::string authData = R"("pluginName":"VTSFloat_Meow","pluginDeveloper":"Lily","authenticationToken":")" + cachedToken_ + R"(")";
        std::string authReq = BuildJsonRequest("AuthenticationRequest", authData);
        if (!SendJson(authReq)) {
            authPending_.store(false, std::memory_order_relaxed);
            return false;
        }
        response = ReceiveJson();
        if (response.empty()) return false;

        if (ExtractJsonBool(response, "authenticated")) {
            authPending_.store(false, std::memory_order_relaxed);
            Log("[vts-api] authenticated with new token");
            return true;
        }
        authPending_.store(false, std::memory_order_relaxed);
        Log("[vts-api] authentication failed after token grant");
        return false;
    }

    bool PollStatistics() {
        const auto pollStart = Clock::now();
        // Statistics request (fps + uptime)
        if (!SendJson(BuildJsonRequest("StatisticsRequest"))) return false;
        std::string response = ReceiveJson();
        if (response.empty()) return false;
        if (ExtractJsonString(response, "messageType") == "APIError") {
            Log("[vts-api] statistics error: " + ExtractJsonString(response, "message"));
            return false;
        }
        int fps = ExtractJsonInt(response, "framerate");
        long long uptime = ExtractJsonLong(response, "uptime");
        int winW = ExtractJsonInt(response, "windowWidth");
        int winH = ExtractJsonInt(response, "windowHeight");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (fps > 0) realtimeFps_ = fps;
            if (uptime >= 0) extraStats_.uptimeMs = uptime;
            if (winW > 0) extraStats_.vtsWindowWidth = winW;
            if (winH > 0) extraStats_.vtsWindowHeight = winH;
        }

        // CurrentModelRequest
        if (!SendJson(BuildJsonRequest("CurrentModelRequest"))) return false;
        response = ReceiveJson();
        if (!response.empty() && ExtractJsonString(response, "messageType") != "APIError") {
            std::string modelId = ExtractJsonString(response, "modelName");
            int artmesh = ExtractJsonInt(response, "numberOfLive2DArtmeshes");
            bool modelChanged = false;
            bool expressionNeedsRefresh = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                modelChanged = modelId != extraStats_.modelId;
                extraStats_.modelId = modelId;
                if (artmesh >= 0) extraStats_.artmeshCount = artmesh;
                if (modelChanged) {
                    expressions_.clear();
                    lastExpressionModelId_.clear();
                }
                expressionNeedsRefresh = !modelId.empty() &&
                    modelId != lastExpressionModelId_;
            }
            if (modelChanged || expressionNeedsRefresh) RequestExpressionState();
        }

        // ItemListRequest
        std::string itemReqData =
            R"("includeAvailableSpots":false,"includeItemInstancesInScene":true,"includeAvailableItemFiles":false)";
        if (!SendJson(BuildJsonRequest("ItemListRequest", itemReqData))) return false;
        response = ReceiveJson();
        if (!response.empty() && ExtractJsonString(response, "messageType") != "APIError") {
            int count = 0;
            size_t pos = 0;
            const std::string needle = "\"instanceID\"";
            while ((pos = response.find(needle, pos)) != std::string::npos) {
                ++count;
                pos += needle.size();
            }
            std::lock_guard<std::mutex> lock(mutex_);
            extraStats_.itemCount = count;
        }
        {
            const double ms = std::chrono::duration<double, std::milli>(
                Clock::now() - pollStart).count();
            std::lock_guard<std::mutex> lock(mutex_);
            extraStats_.apiLatencyMs = ms;
        }
        ProcessExpressionRequests();
        return true;
    }

    void ProcessExpressionRequests() {
        if (!hWebSocket_) return;
        if (expressionStateRequested_.exchange(false, std::memory_order_relaxed)) {
            if (SendJson(BuildJsonRequest("ExpressionStateRequest", R"("details":true)"))) {
                const std::string response = ReceiveJson();
                if (!response.empty() &&
                    ExtractJsonString(response, "messageType") != "APIError") {
                    std::vector<ExpressionInfo> parsed;
                    size_t search = 0;
                    while (true) {
                        const size_t fileKey = response.find(R"("file")", search);
                        if (fileKey == std::string::npos) break;
                        const size_t objectEnd = response.find('}', fileKey);
                        const size_t end = objectEnd == std::string::npos
                            ? response.size() : objectEnd;
                        const std::string object = response.substr(fileKey, end - fileKey);
                        const std::string file = ExtractJsonString(object, "file");
                        if (!file.empty()) {
                            ExpressionInfo info;
                            info.file = file;
                            info.name = ExtractJsonString(object, "name");
                            info.active = ExtractJsonBool(object, "active");
                            bool duplicate = false;
                            for (const auto& existing : parsed) {
                                if (existing.file == info.file) {
                                    duplicate = true;
                                    break;
                                }
                            }
                            if (!duplicate) parsed.push_back(std::move(info));
                        }
                        search = fileKey + 6;
                    }
                    std::lock_guard<std::mutex> lock(mutex_);
                    expressions_ = std::move(parsed);
                    lastExpressionModelId_ = ExtractJsonString(response, "modelName");
                }
            }
        }

        std::deque<VtsExpressionCommand> commands;
        {
            std::lock_guard<std::mutex> lock(expressionCommandMutex_);
            commands.swap(expressionCommands_);
        }
        for (const auto& command : commands) {
            const std::string data =
                R"("expressionFile":")" + EscapeJsonString(command.file) +
                R"(","active":)" + (command.active ? "true" : "false") +
                R"(,"fadeTime":)" + std::to_string((std::clamp)(command.fadeTime, 0.0, 2.0));
            if (!SendJson(BuildJsonRequest("ExpressionActivationRequest", data))) {
                continue;
            }
            const std::string response = ReceiveJson();
            if (response.empty() || ExtractJsonString(response, "messageType") == "APIError") {
                continue;
            }
            // Keep the cached state coherent for quick restoration without
            // waiting for the next full ExpressionStateRequest.
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& expression : expressions_) {
                if (expression.file == command.file) {
                    expression.active = command.active;
                    break;
                }
            }
        }
    }

    void LoadCachedToken() {
        std::wstring tokenW = ReadConfigString(L"VtsApi", L"Token");
        cachedToken_ = WideToUtf8(tokenW.c_str());
        if (!cachedToken_.empty()) {
            everAuthenticated_.store(true);
        }
    }

    void SaveToken() {
        std::wstring tokenW = Utf8ToWide(cachedToken_);
        WriteConfigString(L"VtsApi", L"Token", tokenW);
    }

    void SleepMs(int ms) {
        const int step = 100;
        for (int elapsed = 0; elapsed < ms && running_.load(); elapsed += step) {
            Sleep(step);
        }
    }

    mutable std::mutex mutex_;
    std::atomic<bool> running_{ false };
    std::atomic<bool> connected_{ false };
    std::atomic<bool> scanning_{ false };
    std::atomic<bool> scanRequested_{ false };
    std::atomic<int> scanResult_{ 0 };  // 0=none, 1=success, 2=fail
    std::atomic<bool> everAuthenticated_{ false };
    std::atomic<bool> initialDiscoveryStarted_{ false };
    std::atomic<bool> initialDiscoveryDone_{ false };
    std::atomic<bool> authPending_{ false };
    std::thread thread_;
    int realtimeFps_ = 0;
    int port_ = kVtsApiPort;
    bool portConfigured_ = false;
    int activePort_ = 0;
    std::string cachedToken_;
    ExtraStats extraStats_;
    std::vector<ExpressionInfo> expressions_;
    std::string lastExpressionModelId_;
    std::mutex expressionCommandMutex_;
    std::deque<VtsExpressionCommand> expressionCommands_;
    std::atomic<bool> expressionStateRequested_{ false };

    HINTERNET hSession_ = nullptr;
    HINTERNET hConnect_ = nullptr;
    HINTERNET hRequest_ = nullptr;
    HINTERNET hWebSocket_ = nullptr;
    HINSTANCE instance_ = nullptr;
};

struct VtsProcessInfo {
    DWORD processId = 0;
    std::filesystem::path executable;
};

std::filesystem::path CurrentExecutablePath() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return length ? std::filesystem::path(std::wstring(buffer.data(), length))
                  : std::filesystem::path{};
}

std::optional<VtsProcessInfo> FindRunningVts() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    std::optional<VtsProcessInfo> result;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, kVtsExecutableName) != 0) {
                continue;
            }
            HANDLE process = OpenProcess(
                PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (!process) {
                continue;
            }
            std::vector<wchar_t> path(32768);
            DWORD length = static_cast<DWORD>(path.size());
            if (QueryFullProcessImageNameW(process, 0, path.data(), &length)) {
                result = VtsProcessInfo{
                    entry.th32ProcessID,
                    std::filesystem::path(std::wstring(path.data(), length)),
                };
            }
            CloseHandle(process);
            if (result) {
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

HWND FindMainWindowForProcess(DWORD processId) {
    struct Search {
        DWORD processId;
        HWND window;
    } search{ processId, nullptr };
    EnumWindows(
        [](HWND window, LPARAM value) -> BOOL {
            auto* search = reinterpret_cast<Search*>(value);
            DWORD owner = 0;
            GetWindowThreadProcessId(window, &owner);
            if (owner == search->processId && IsWindowVisible(window) &&
                GetWindow(window, GW_OWNER) == nullptr) {
                search->window = window;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    return search.window;
}

bool IsVtsDirectory(const std::filesystem::path& directory) {
    std::error_code error;
    return std::filesystem::is_regular_file(directory / kVtsExecutableName, error) &&
           std::filesystem::is_regular_file(directory / kVtsBatchName, error);
}

std::optional<std::filesystem::path> ReadRegistryString(
    HKEY root, const wchar_t* subkey, const wchar_t* valueName) {
    wchar_t value[32768]{};
    DWORD bytes = sizeof(value);
    if (RegGetValueW(
            root, subkey, valueName, RRF_RT_REG_SZ,
            nullptr, value, &bytes) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return std::filesystem::path(value);
}

std::vector<std::filesystem::path> SteamLibraryRoots(
    const std::filesystem::path& steamRoot) {
    std::vector<std::filesystem::path> roots{ steamRoot };
    std::ifstream input(steamRoot / L"steamapps" / L"libraryfolders.vdf");
    std::string line;
    while (std::getline(input, line)) {
        std::vector<std::string> fields;
        size_t cursor = 0;
        while (true) {
            const size_t begin = line.find('"', cursor);
            if (begin == std::string::npos) break;
            const size_t end = line.find('"', begin + 1);
            if (end == std::string::npos) break;
            fields.push_back(line.substr(begin + 1, end - begin - 1));
            cursor = end + 1;
        }
        if (fields.size() < 2 || fields[0] != "path") {
            continue;
        }
        std::string decoded;
        decoded.reserve(fields[1].size());
        for (size_t index = 0; index < fields[1].size(); ++index) {
            if (fields[1][index] == '\\' && index + 1 < fields[1].size() &&
                fields[1][index + 1] == '\\') {
                decoded.push_back('\\');
                ++index;
            } else {
                decoded.push_back(fields[1][index]);
            }
        }
        const int wideLength = MultiByteToWideChar(
            CP_UTF8, 0, decoded.c_str(), static_cast<int>(decoded.size()),
            nullptr, 0);
        if (wideLength > 0) {
            std::wstring wide(static_cast<size_t>(wideLength), L'\0');
            MultiByteToWideChar(
                CP_UTF8, 0, decoded.c_str(), static_cast<int>(decoded.size()),
                wide.data(), wideLength);
            roots.emplace_back(wide);
        }
    }
    return roots;
}

std::optional<std::filesystem::path> FindVtsDirectory() {
    std::vector<std::filesystem::path> candidates;
    if (const auto running = FindRunningVts()) {
        candidates.push_back(running->executable.parent_path());
    }
    const std::wstring cached = ReadConfigString(L"vts", L"install_dir");
    if (!cached.empty()) {
        candidates.emplace_back(cached);
    }

    std::optional<std::filesystem::path> steamRoot = ReadRegistryString(
        HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath");
    if (!steamRoot) {
        wchar_t programFiles[MAX_PATH]{};
        if (GetEnvironmentVariableW(
                L"ProgramFiles(x86)", programFiles, ARRAYSIZE(programFiles))) {
            steamRoot = std::filesystem::path(programFiles) / L"Steam";
        }
    }
    if (steamRoot) {
        for (const auto& library : SteamLibraryRoots(*steamRoot)) {
            candidates.push_back(
                library / L"steamapps" / L"common" / L"VTube Studio");
        }
    }

    for (const auto& candidate : candidates) {
        if (!IsVtsDirectory(candidate)) {
            continue;
        }
        WriteConfigString(L"vts", L"install_dir", candidate.wstring());
        return candidate;
    }
    return std::nullopt;
}

struct RegistryValueBackup {
    bool existed = false;
    std::wstring value;
};

bool SetVtsGpuPreference(
    const std::filesystem::path& executable, int preference,
    RegistryValueBackup& backup) {
    constexpr wchar_t subkey[] = L"Software\\Microsoft\\DirectX\\UserGpuPreferences";
    HKEY key = nullptr;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER, subkey, 0, nullptr, 0,
            KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    const std::wstring name = executable.wstring();
    wchar_t existing[4096]{};
    DWORD type = 0;
    DWORD bytes = sizeof(existing);
    const LSTATUS query = RegQueryValueExW(
        key, name.c_str(), nullptr, &type,
        reinterpret_cast<BYTE*>(existing), &bytes);
    backup.existed = query == ERROR_SUCCESS && type == REG_SZ;
    backup.value = backup.existed ? existing : L"";

    std::wstring updated = backup.value;
    const std::wstring setting = L"GpuPreference=";
    size_t position = updated.find(setting);
    if (position != std::wstring::npos) {
        size_t end = updated.find(L';', position);
        end = end == std::wstring::npos ? updated.size() : end + 1;
        updated.erase(position, end - position);
    }
    if (!updated.empty() && updated.back() != L';') {
        updated += L';';
    }
    updated += setting + std::to_wstring(preference) + L';';
    const LSTATUS written = RegSetValueExW(
        key, name.c_str(), 0, REG_SZ,
        reinterpret_cast<const BYTE*>(updated.c_str()),
        static_cast<DWORD>((updated.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return written == ERROR_SUCCESS;
}

void RestoreVtsGpuPreference(
    const std::filesystem::path& executable,
    const RegistryValueBackup& backup) {
    constexpr wchar_t subkey[] = L"Software\\Microsoft\\DirectX\\UserGpuPreferences";
    HKEY key = nullptr;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER, subkey, 0, nullptr, 0,
            KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return;
    }
    const std::wstring name = executable.wstring();
    if (backup.existed) {
        RegSetValueExW(
            key, name.c_str(), 0, REG_SZ,
            reinterpret_cast<const BYTE*>(backup.value.c_str()),
            static_cast<DWORD>((backup.value.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, name.c_str());
    }
    RegCloseKey(key);
}

int RunDelayedRestart(DWORD parentProcessId) {
    if (HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parentProcessId)) {
        WaitForSingleObject(parent, 20000);
        CloseHandle(parent);
    }

    const auto deadline = Clock::now() + std::chrono::seconds(30);
    while (Clock::now() < deadline && !FindRunningVts()) {
        Sleep(250);
    }
    // Give Unity and the Spout sender time to finish initialization.
    Sleep(7000);

    const std::filesystem::path executable = CurrentExecutablePath();
    std::wstring command = L"\"" + executable.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(
        executable.c_str(), command.data(), nullptr, nullptr, FALSE,
        DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW,
        nullptr, executable.parent_path().c_str(), &startup, &process);
    if (created) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        Log("[gpu switch] overlay restarted");
        return 0;
    }
    Log("[gpu switch] failed to restart overlay error=" +
        std::to_string(GetLastError()));
    return 1;
}

int StopRunningOverlay() {
    HWND window = FindWindowW(kWindowClass, nullptr);
    if (!window) {
        return 0;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    HANDLE process = processId
        ? OpenProcess(SYNCHRONIZE, FALSE, processId)
        : nullptr;

    if (!PostMessageW(window, WM_CLOSE, 0, 0)) {
        if (process) {
            CloseHandle(process);
        }
        return 1;
    }

    if (process) {
        WaitForSingleObject(process, 5000);
        CloseHandle(process);
    }
    return 0;
}

class LayeredOverlay {
public:
    int Run(HINSTANCE instance) {
        instance_ = instance;
        uiThreadId_ = GetCurrentThreadId();
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        INITCOMMONCONTROLSEX commonControls{};
        commonControls.dwSize = sizeof(commonControls);
        commonControls.dwICC = ICC_BAR_CLASSES;
        InitCommonControlsEx(&commonControls);

        instanceMutex_ = CreateMutexW(nullptr, TRUE, kInstanceName);
        if (!instanceMutex_ || GetLastError() == ERROR_ALREADY_EXISTS) {
            return 0;
        }

        appIconLarge_ = static_cast<HICON>(LoadImageW(
            instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
            32, 32, LR_DEFAULTCOLOR));
        appIconSmall_ = static_cast<HICON>(LoadImageW(
            instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
            16, 16, LR_DEFAULTCOLOR));
        RegisterWindowClass();
        LoadUiSettings();
        cpuModel_ = ReadCpuModelName();
        CreateOverlayWindow();
        UpdateMonitorRefreshRate(true);
        CreateToolbarWindow();
        taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");
        CreateGraphics();
        RecreateDib(kDefaultWidth, kDefaultHeight);

        LoadHotkeySettings();
        RegisterConfiguredHotkey(true);
        ApplyClickThrough();
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        SetWindowPos(
            hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        AddTrayIcon();

        DWORD taskIndex = 0;
        HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Games", &taskIndex);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        const MMRESULT timerResolution = timeBeginPeriod(1);

        if (fpsMode_ == FpsMode::FollowVtsApi) {
            fpsMode_ = FpsMode::FollowVts;
        }

        statsStarted_ = Clock::now();
        auto nextFrame = statsStarted_;
        bool running = true;
        while (running) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    running = false;
                    break;
                }
                if (borderPanelHwnd_ && IsWindow(borderPanelHwnd_) &&
                    IsDialogMessageW(borderPanelHwnd_, &message)) {
                    continue;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (!running) {
                break;
            }
            if (resetFrameSchedule_) {
                nextFrame = Clock::now();
                resetFrameSchedule_ = false;
            }

            const auto now = Clock::now();
            if (now >= nextFrame) {
                const double wakeLateMs = std::chrono::duration<double, std::milli>(
                    now - nextFrame).count();
                ++requested_;
                wakeLateMs_ += wakeLateMs;
                maxWakeLateMs_ = (std::max)(maxWakeLateMs_, wakeLateMs);
                RenderFrame();
                PrintStats();
                nextFrame += std::chrono::duration_cast<Clock::duration>(
                    std::chrono::duration<double>(1.0 / EffectiveRequestFps()));
                if (now - nextFrame > std::chrono::milliseconds(100)) {
                    nextFrame = now;
                }
            } else {
                const auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(nextFrame - now);
                MsgWaitForMultipleObjectsEx(
                    0,
                    nullptr,
                    static_cast<DWORD>((std::max)(1LL, wait.count())),
                    QS_ALLINPUT,
                    MWMO_INPUTAVAILABLE);
            }
        }

        if (mmcss) {
            AvRevertMmThreadCharacteristics(mmcss);
        }
        if (timerResolution == TIMERR_NOERROR) {
            timeEndPeriod(1);
        }
        return 0;
    }

    ~LayeredOverlay() {
        if (borderPanelHwnd_ && IsWindow(borderPanelHwnd_)) {
            DestroyWindow(borderPanelHwnd_);
        }
        vtsApi_.Stop();
        StopInteractiveRendering();
        RemoveTrayIcon();
        receiver_.ReleaseReceiver();
        if (gpuUsageQuery_) {
            PdhCloseQuery(gpuUsageQuery_);
            gpuUsageQuery_ = nullptr;
            gpuUsageCounter_ = nullptr;
        }
        if (hwnd_) {
            UnregisterHotKey(hwnd_, kHotkeyId);
        }
        if (toolbarHwnd_ && IsWindow(toolbarHwnd_)) {
            DestroyWindow(toolbarHwnd_);
        }
        if (statusHwnd_ && IsWindow(statusHwnd_)) {
            DestroyWindow(statusHwnd_);
            statusHwnd_ = nullptr;
        }
        if (githubIcon_) {
            DestroyIcon(githubIcon_);
            githubIcon_ = nullptr;
        }
        if (appIconLarge_) {
            DestroyIcon(appIconLarge_);
            appIconLarge_ = nullptr;
        }
        if (appIconSmall_) {
            DestroyIcon(appIconSmall_);
            appIconSmall_ = nullptr;
        }
        DestroyDib();
        if (instanceMutex_) {
            ReleaseMutex(instanceMutex_);
            CloseHandle(instanceMutex_);
        }
    }

private:
    void RegisterWindowClass() {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.hInstance = instance_;
        windowClass.lpfnWndProc = &LayeredOverlay::WindowProc;
        windowClass.lpszClassName = kWindowClass;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hIcon = appIconLarge_;
        windowClass.hIconSm = appIconSmall_;
        Check(RegisterClassExW(&windowClass) ? S_OK : HRESULT_FROM_WIN32(GetLastError()),
              "RegisterClassExW");

        WNDCLASSEXW toolbarClass{};
        toolbarClass.cbSize = sizeof(toolbarClass);
        toolbarClass.hInstance = instance_;
        toolbarClass.lpfnWndProc = &LayeredOverlay::ToolbarWindowProc;
        toolbarClass.lpszClassName = kToolbarClass;
        toolbarClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        toolbarClass.hbrBackground = nullptr;
        toolbarClass.hIcon = appIconLarge_;
        toolbarClass.hIconSm = appIconSmall_;
        Check(RegisterClassExW(&toolbarClass) ? S_OK : HRESULT_FROM_WIN32(GetLastError()),
              "RegisterClassExW(toolbar)");

        WNDCLASSEXW statusClass{};
        statusClass.cbSize = sizeof(statusClass);
        statusClass.hInstance = instance_;
        statusClass.lpfnWndProc = &LayeredOverlay::StatusWindowProc;
        statusClass.lpszClassName = kStatusClass;
        statusClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        statusClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        statusClass.hIcon = appIconLarge_;
        statusClass.hIconSm = appIconSmall_;
        Check(RegisterClassExW(&statusClass) ? S_OK : HRESULT_FROM_WIN32(GetLastError()),
              "RegisterClassExW(status)");
    }

    void CreateOverlayWindow() {
        const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int width = ReadConfigInt(L"window", L"width", kDefaultWidth);
        int height = ReadConfigInt(L"window", L"height", kDefaultHeight);
        int x = ReadConfigInt(
            L"window", L"x", (std::max)(20, (screenWidth - width) / 2));
        int y = ReadConfigInt(L"window", L"y", 100);
        width = (std::max)(kMinimumWidth, width);
        height = (std::max)(96, height);
        RECT saved{ x, y, x + width, y + height };
        if (!MonitorFromRect(&saved, MONITOR_DEFAULTTONULL)) {
            width = kDefaultWidth;
            height = kDefaultHeight;
            x = (std::max)(20, (screenWidth - width) / 2);
            y = 100;
        }
        hwnd_ = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                WS_EX_TRANSPARENT,
            kWindowClass,
            kWindowTitle,
            WS_POPUP,
            x,
            y,
            width,
            height,
            nullptr,
            nullptr,
            instance_,
            this);
        if (!hwnd_) {
            throw std::runtime_error("CreateWindowExW failed");
        }
    }

    void CreateToolbarWindow() {
        toolbarHwnd_ = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            kToolbarClass,
            kToolbarTitle,
            WS_POPUP,
            0,
            0,
            locked_ ? 92 : kMinimumWidth,
            ToolbarCurrentHeight(),
            hwnd_,
            nullptr,
            instance_,
            this);
        if (!toolbarHwnd_) {
            throw std::runtime_error("CreateWindowExW(toolbar) failed");
        }
        githubIcon_ = static_cast<HICON>(LoadImageW(
            instance_, MAKEINTRESOURCEW(IDI_GITHUB), IMAGE_ICON,
            32, 32, LR_DEFAULTCOLOR));
        PositionToolbar();
        ShowWindow(
            toolbarHwnd_, (!locked_ || debugMode_) ? SW_SHOWNOACTIVATE : SW_HIDE);
    }

    void CreateVtsStatusWindow() {
        statusHwnd_ = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            kStatusClass,
            L"VTSFloat_Meow - VTube Studio",
            WS_POPUP | WS_CAPTION | WS_SYSMENU,
            0, 0, 620, 270,
            nullptr, nullptr, instance_, this);
        if (!statusHwnd_) {
            Log("[vts status] failed to create window");
            return;
        }
        statusHeadline_ = CreateWindowExW(
            0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
            24, 20, 570, 28, statusHwnd_, nullptr, instance_, nullptr);
        statusInstructions_ = CreateWindowExW(
            0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
            24, 56, 570, 62, statusHwnd_, nullptr, instance_, nullptr);
        statusPath_ = CreateWindowExW(
            0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
            24, 178, 570, 34, statusHwnd_, nullptr, instance_, nullptr);
        statusLaunchExe_ = CreateWindowExW(
            0, L"BUTTON", L"从 Steam 启动 VTube Studio",
            WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
            24, 130, 270, 32, statusHwnd_,
            reinterpret_cast<HMENU>(kStatusLaunchExeCommand), instance_, nullptr);
        statusLaunchBatch_ = CreateWindowExW(
            0, L"BUTTON", L"从外部启动 VTS",
            WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
            314, 130, 280, 32, statusHwnd_,
            reinterpret_cast<HMENU>(kStatusLaunchBatchCommand), instance_, nullptr);
        ShowWindow(statusHwnd_, SW_HIDE);
    }

    void PositionVtsStatusWindow() {
        if (!statusHwnd_ || !IsWindow(statusHwnd_)) return;
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        const HMONITOR monitor = MonitorFromWindow(
            hwnd_, MONITOR_DEFAULTTONEAREST);
        if (!monitor || !GetMonitorInfoW(monitor, &info)) return;
        RECT current{};
        GetWindowRect(statusHwnd_, &current);
        const int width = current.right - current.left;
        const int height = current.bottom - current.top;
        const int x = info.rcWork.left +
            ((info.rcWork.right - info.rcWork.left) - width) / 2;
        const int y = info.rcWork.top +
            ((info.rcWork.bottom - info.rcWork.top) - height) / 2;
        SetWindowPos(statusHwnd_, HWND_TOPMOST, x, y, width, height,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    void SetVtsStatusMode(
        VtsStatusMode mode, const std::filesystem::path& directory = {}) {
        const bool changed = mode != statusMode_ || directory != statusDirectory_;
        if (changed) {
            statusDismissed_ = false;
            statusMode_ = mode;
            statusDirectory_ = directory;
            statusFrameDirty_ = true;
        }
        if (mode == VtsStatusMode::Hidden || statusDismissed_) {
            statusFrameDirty_ = true;
            ApplyClickThrough();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        // The status page is rendered inside the layered overlay itself.  It
        // is temporarily made interactive so the two launch buttons can be
        // clicked without opening a separate dialog window.
        ApplyClickThrough();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void PollVtsStatus(bool force = false) {
        if (GetCurrentThreadId() != uiThreadId_) return;
        // Keep the first-run view clean until a real model frame has arrived.
        // Status recovery is only useful after a stream was seen once.
        if (!hasReceivedModel_) {
            if (statusMode_ != VtsStatusMode::Hidden) {
                HideVtsStatus();
            }
            return;
        }
        const auto now = Clock::now();
        if (!force && now - lastVtsStatusCheck_ < std::chrono::milliseconds(750)) {
            return;
        }
        lastVtsStatusCheck_ = now;
        const auto directory = FindVtsDirectory();
        const auto running = FindRunningVts();
        if (!directory) {
            SetVtsStatusMode(VtsStatusMode::WaitingForVts);
        } else if (!running) {
            SetVtsStatusMode(VtsStatusMode::LaunchChoices, *directory);
        } else {
            SetVtsStatusMode(VtsStatusMode::WaitingForSpout, *directory);
        }
    }

    void HideVtsStatus() {
        statusMode_ = VtsStatusMode::Hidden;
        statusDismissed_ = false;
        statusFrameDirty_ = true;
        ApplyClickThrough();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void LaunchVtsFromStatus(bool external) {
        const auto directory = FindVtsDirectory();
        if (!directory) {
            PollVtsStatus(true);
            return;
        }
        const auto target = *directory / (external ? kVtsBatchName : kVtsExecutableName);
        const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
            hwnd_, L"open", target.c_str(), nullptr,
            directory->c_str(), SW_SHOWNORMAL));
        if (result <= 32) {
            Log("[vts status] failed to launch selected VTS entry");
        }
        statusDismissed_ = false;
        PollVtsStatus(true);
    }

    void SaveWindowPlacement() const {
        if (!hwnd_ || !IsWindow(hwnd_)) {
            return;
        }
        RECT rect{};
        if (!GetWindowRect(hwnd_, &rect)) {
            return;
        }
        WriteConfigInt(L"window", L"x", rect.left);
        WriteConfigInt(L"window", L"y", rect.top);
        WriteConfigInt(L"window", L"width", rect.right - rect.left);
        WriteConfigInt(L"window", L"height", rect.bottom - rect.top);
    }

    void LoadHotkeySettings() {
        hotkeyModifiers_ = static_cast<UINT>(ReadConfigInt(
            L"hotkey", L"modifiers", MOD_CONTROL | MOD_SHIFT));
        hotkeyModifiers_ &= MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN;
        hotkeyVk_ = static_cast<UINT>(ReadConfigInt(L"hotkey", L"vk", 'L'));
        // Migrate the old built-in Ctrl+Alt+L default, while leaving any
        // other user-selected combination untouched.
        if (hotkeyModifiers_ == (MOD_CONTROL | MOD_ALT) && hotkeyVk_ == 'L') {
            hotkeyModifiers_ = MOD_CONTROL | MOD_SHIFT;
        }
        if (hotkeyVk_ < 1 || hotkeyVk_ > 0xFE || hotkeyModifiers_ == 0) {
            hotkeyModifiers_ = MOD_CONTROL | MOD_SHIFT;
            hotkeyVk_ = 'L';
        }
    }

    void LoadUiSettings() {
        // Every normal launch restores the saved bounds in edit mode. Locking
        // is a runtime action only and never hides the GUI on the next launch.
        locked_ = false;
        debugMode_ = ReadConfigInt(L"debug", L"enabled", 0) != 0;
        bgWarningPermanentlyDismissed_ = ReadConfigInt(L"warnings", L"suppress_bg_opaque", 0) != 0;
        gpuWarningPermanentlyDismissed_ = ReadConfigInt(L"warnings", L"suppress_gpu_high_perf", 0) != 0;
        selectedGpuIndex_ = ReadConfigInt(L"gpu", L"adapter_index", -1);
        targetFps_ = (std::clamp)(
            ReadConfigInt(L"render", L"fps", 60), kMinimumFps, kMaximumFps);
        const int savedFpsMode = ReadConfigInt(L"render", L"fps_mode", -1);
        if (savedFpsMode >= static_cast<int>(FpsMode::Fixed) &&
            savedFpsMode <= static_cast<int>(FpsMode::FollowVtsApi)) {
            fpsMode_ = static_cast<FpsMode>(savedFpsMode);
        } else {
            // Migrate the old "follow monitor" setting to VTS-follow mode,
            // which is safer for Spout because it matches the sender target.
            fpsMode_ = ReadConfigInt(L"render", L"follow_monitor", 1) != 0
                ? FpsMode::FollowVts
                : FpsMode::Fixed;
        }
        scalingQuality_ = (std::clamp)(
            ReadConfigInt(L"render", L"scaling_quality", kScalingBalanced),
            kScalingPerformance, kScalingQuality);
        hoverFadeEnabled_ = ReadConfigInt(L"opacity", L"locked_hover_fade", 1) != 0;
        hoverOpacityPercent_ = (std::clamp)(
            ReadConfigInt(
                L"opacity", L"locked_hover_percent", kDefaultHoverOpacityPercent),
            kMinimumHoverOpacityPercent,
            100);
        modelOpacityPercent_ = (std::clamp)(
            ReadConfigInt(L"opacity", L"model_percent", 100), 10, 100);
        hoverExpandPx_ = (std::clamp)(
            ReadConfigInt(L"opacity", L"hover_expand_px", 0), -500, 500);
        hoverExpressionEnabled_ = ReadConfigInt(
            L"expression", L"hover_enabled", 0) != 0;
        hoverExpressionFile_ = WideToUtf8(ReadConfigString(
            L"expression", L"hover_file").c_str());
        aspectLocked_ = ReadConfigInt(L"window", L"lock_aspect", 1) != 0;
        const int borderSchema = ReadConfigInt(L"border", L"schema", 1);
        const int savedBorderMode = ReadConfigInt(
            L"border", L"mode", borderSchema < 2 ? 0 : kBorderModeNormal);
        borderMode_ = borderSchema < 2 && savedBorderMode == 0
            ? kBorderModeNormal
            : (std::clamp)(savedBorderMode, kBorderModeCustom, kBorderModeNormal);
        customBorderColor_ = static_cast<COLORREF>(ReadConfigInt(
            L"border", L"custom_color",
            ReadConfigInt(
                L"border", L"start_color", kDefaultCustomBorderColor)));
        borderThickness_ = (std::clamp)(
            ReadConfigInt(
                L"border", L"thickness", kDefaultBorderThickness),
            1,
            kMaximumBorderThickness);
        if (ReadConfigInt(L"ui", L"start_unlocked_once", 0) != 0) {
            locked_ = false;
            WriteConfigInt(L"ui", L"start_unlocked_once", 0);
        }
    }

    void SaveUiSettings() const {
        WriteConfigInt(L"debug", L"enabled", debugMode_ ? 1 : 0);
        WriteConfigInt(L"gpu", L"adapter_index", selectedGpuIndex_);
        WriteConfigInt(L"render", L"fps", targetFps_);
        WriteConfigInt(L"render", L"fps_mode", static_cast<int>(fpsMode_));
        WriteConfigInt(
            L"render", L"follow_monitor",
            fpsMode_ == FpsMode::FollowMonitor ? 1 : 0);
        WriteConfigInt(L"render", L"scaling_quality", scalingQuality_);
        WriteConfigInt(L"opacity", L"locked_hover_fade", hoverFadeEnabled_ ? 1 : 0);
        WriteConfigInt(L"opacity", L"locked_hover_percent", hoverOpacityPercent_);
        WriteConfigInt(L"opacity", L"model_percent", modelOpacityPercent_);
        WriteConfigInt(L"opacity", L"hover_expand_px", hoverExpandPx_);
        WriteConfigInt(L"expression", L"hover_enabled", hoverExpressionEnabled_ ? 1 : 0);
        WriteConfigString(L"expression", L"hover_file", Utf8ToWide(hoverExpressionFile_));
        WriteConfigInt(L"window", L"lock_aspect", aspectLocked_ ? 1 : 0);
        WriteConfigInt(L"border", L"schema", 2);
        WriteConfigInt(L"border", L"mode", borderMode_);
        WriteConfigInt(
            L"border", L"custom_color", static_cast<int>(customBorderColor_));
        WriteConfigInt(L"border", L"thickness", borderThickness_);
    }

    void ResetSettingsToDefaults() {
        // This is deliberately scoped to the application's own INI file. It
        // does not touch the VTS installation, Spout, logs, or any user files.
        CancelFpsCapture();
        CancelHotkeyCapture();
        if (toolbarHwnd_) {
            KillTimer(toolbarHwnd_, 48);
        }
        if (borderPanelHwnd_ && IsWindow(borderPanelHwnd_)) {
            SendMessageW(borderPanelHwnd_, WM_COMMAND, IDCANCEL, 0);
        }

        const std::filesystem::path path = ConfigPath();
        std::error_code error;
        std::filesystem::remove(path, error);

        if (hotkeyRegistered_) {
            UnregisterHotKey(hwnd_, kHotkeyId);
            hotkeyRegistered_ = false;
        }
        hotkeyModifiers_ = MOD_CONTROL | MOD_SHIFT;
        hotkeyVk_ = 'L';
        debugMode_ = false;
        selectedGpuIndex_ = -1;
        gpuSelectionFallback_ = false;
        targetFps_ = 60;
        fpsMode_ = FpsMode::FollowVts;
        scalingQuality_ = kScalingBalanced;
        hoverFadeEnabled_ = true;
        hoverOpacityPercent_ = kDefaultHoverOpacityPercent;
        modelOpacityPercent_ = 100;
        hoverExpandPx_ = 0;
        CancelHoverExpression();
        hoverExpressionEnabled_ = false;
        hoverExpressionFile_.clear();
        aspectLocked_ = true;
        borderMode_ = kBorderModeNormal;
        customBorderColor_ = kDefaultCustomBorderColor;
        borderThickness_ = kDefaultBorderThickness;
        bgWarningPermanentlyDismissed_ = false;
        gpuWarningPermanentlyDismissed_ = false;
        gpuWarningShown_ = false;
        statusDismissed_ = false;
        hasReceivedModel_ = false;
        apiStartScheduled_ = false;
        apiStartedAfterModel_ = false;
        lastApiNotificationVisible_ = false;
        vtsConfiguredFps_ = 0;
        vtsConfiguredMode_.clear();
        vtsApiFps_ = 0;
        apiNotificationDismissed_ = false;
        apiWasConnected_ = false;
        opaqueBackgroundDetected_ = false;
        opaqueFrameCount_ = 0;
        holdNotificationForScanResult_ = false;
        scanStartedObserved_ = false;
        awaitingUserApproval_ = false;
        showingApiSuccess_ = false;
        toolbarHovered_ = ToolbarButton::None;
        toolbarPressed_ = ToolbarButton::None;
        capturingFps_ = false;
        fpsInput_.clear();
        capturingHotkey_ = false;
        hoverOpacityPreviewActive_ = false;
        hoverExpandEditing_ = false;
        hoverExpandPreviewAlpha_ = 0.0;
        currentOverlayAlpha_ = 255;
        hoverFadeStartAlpha_ = 255;
        hoverTargetAlpha_ = 255;
        hoverFadeStarted_ = Clock::now();
        debugFpsHistory_.clear();
        debugFrameMsHistory_.clear();
        debugCacheDirty_ = true;

        RegisterConfiguredHotkey(true);
        SetOverlayVisible(true);
        SetLocked(false);

        const HMONITOR monitor = MonitorFromPoint(
            POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (monitor && GetMonitorInfoW(monitor, &info)) {
            const int x = info.rcWork.left +
                (info.rcWork.right - info.rcWork.left - kDefaultWidth) / 2;
            const int y = info.rcWork.top +
                (info.rcWork.bottom - info.rcWork.top - kDefaultHeight) / 2;
            SetWindowPos(hwnd_, HWND_TOPMOST, x, y,
                         kDefaultWidth, kDefaultHeight,
                         SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
        PositionToolbar();
        ++requested_;
        resetFrameSchedule_ = true;
        RenderFrame();

        // The VTS API token and port are in the same INI. Restarting this
        // client drops the in-memory token as well as the on-disk cache.
        vtsApi_.ResetCache();
        Log(std::string("[settings] reset_to_defaults") +
            (error ? " remove_error=" + std::to_string(error.value()) : ""));
    }

    void SaveHotkeySettings() const {
        WriteConfigInt(L"hotkey", L"modifiers", static_cast<int>(hotkeyModifiers_));
        WriteConfigInt(L"hotkey", L"vk", static_cast<int>(hotkeyVk_));
    }

    bool RegisterConfiguredHotkey(bool allowFallback) {
        const auto attempt = [this](UINT modifiers, UINT key) {
            return RegisterHotKey(hwnd_, kHotkeyId, modifiers | MOD_NOREPEAT, key) != FALSE;
        };
        if (attempt(hotkeyModifiers_, hotkeyVk_)) {
            hotkeyRegistered_ = true;
            return true;
        }
        const DWORD firstError = GetLastError();
        hotkeyRegistered_ = false;
        if (allowFallback) {
            struct Candidate { UINT modifiers; UINT key; };
            constexpr Candidate candidates[] = {
                { MOD_CONTROL | MOD_SHIFT, 'L' },
                { MOD_CONTROL | MOD_ALT, 'L' },
                { MOD_CONTROL | MOD_ALT | MOD_SHIFT, 'L' },
                { MOD_CONTROL | MOD_ALT, VK_F10 },
            };
            for (const Candidate& candidate : candidates) {
                if (candidate.modifiers == hotkeyModifiers_ && candidate.key == hotkeyVk_) {
                    continue;
                }
                if (!attempt(candidate.modifiers, candidate.key)) {
                    continue;
                }
                hotkeyModifiers_ = candidate.modifiers;
                hotkeyVk_ = candidate.key;
                hotkeyRegistered_ = true;
                SaveHotkeySettings();
                Log("[layered] configured hotkey unavailable; fallback=" +
                    WideToUtf8(HotkeyText().c_str()));
                return true;
            }
        }
        Log("[layered] RegisterHotKey failed error=" + std::to_string(firstError) +
            "; toolbar can still lock/unlock");
        return false;
    }

    std::wstring HotkeyText() const {
        std::wstring text;
        if (hotkeyModifiers_ & MOD_CONTROL) text += L"Ctrl+";
        if (hotkeyModifiers_ & MOD_ALT) text += L"Alt+";
        if (hotkeyModifiers_ & MOD_SHIFT) text += L"Shift+";
        if (hotkeyModifiers_ & MOD_WIN) text += L"Win+";

        if ((hotkeyVk_ >= 'A' && hotkeyVk_ <= 'Z') ||
            (hotkeyVk_ >= '0' && hotkeyVk_ <= '9')) {
            text.push_back(static_cast<wchar_t>(hotkeyVk_));
            return text;
        }
        if (hotkeyVk_ >= VK_F1 && hotkeyVk_ <= VK_F24) {
            text += L"F" + std::to_wstring(hotkeyVk_ - VK_F1 + 1);
            return text;
        }
        wchar_t name[64]{};
        const UINT scanCode = MapVirtualKeyW(hotkeyVk_, MAPVK_VK_TO_VSC);
        if (GetKeyNameTextW(static_cast<LONG>(scanCode << 16), name, ARRAYSIZE(name))) {
            text += name;
        } else {
            text += L"VK" + std::to_wstring(hotkeyVk_);
        }
        return text;
    }

    void PositionToolbar() {
        if (!toolbarHwnd_ || !hwnd_ || !IsWindow(hwnd_)) {
            return;
        }
        RECT overlay{};
        GetWindowRect(hwnd_, &overlay);
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitorInfo);
        const int width = locked_
            ? 92 + (debugMode_ ? kLockedDebugLabelWidth : 0)
            : (std::max)(kMinimumWidth, static_cast<int>(overlay.right - overlay.left));
        const int height = ToolbarCurrentHeight();
        const int desiredY = overlay.top - height - kToolbarGap;
        const int y = (std::max)(static_cast<int>(monitorInfo.rcWork.top), desiredY);
        int x = overlay.left;
        const int workWidth = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
        if (width <= workWidth) {
            x = (std::max)(static_cast<int>(monitorInfo.rcWork.left),
                (std::min)(x, static_cast<int>(monitorInfo.rcWork.right) - width));
        }
        SetWindowPos(
            toolbarHwnd_, HWND_TOPMOST, x, y, width, height,
            SWP_NOACTIVATE);
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }

    void CenterOnMonitor(HMONITOR monitor) {
        if (!monitor) {
            return;
        }
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(monitor, &info)) {
            return;
        }
        RECT rect{};
        GetWindowRect(hwnd_, &rect);
        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        const int x = info.rcWork.left + (info.rcWork.right - info.rcWork.left - width) / 2;
        const int y = info.rcWork.top + (info.rcWork.bottom - info.rcWork.top - height) / 2;
        SetWindowPos(
            hwnd_, HWND_TOPMOST, x, y, width, height,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
        PositionToolbar();
        SaveWindowPlacement();
    }

    void ResetToPrimaryMonitor() {
        const POINT origin{ 0, 0 };
        const HMONITOR monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (!monitor || !GetMonitorInfoW(monitor, &info)) {
            return;
        }

        SetOverlayVisible(true);
        SetLocked(false);
        const int x = info.rcWork.left +
            (info.rcWork.right - info.rcWork.left - kResetWidth) / 2;
        const int y = info.rcWork.top +
            (info.rcWork.bottom - info.rcWork.top - kResetHeight) / 2;
        SetWindowPos(
            hwnd_, HWND_TOPMOST, x, y, kResetWidth, kResetHeight,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
        SaveUiSettings();
        initialAspectApplied_ = true;
        PositionToolbar();
        SaveWindowPlacement();
        resetFrameSchedule_ = true;
    }

    void AddTrayIcon() {
        if (trayIconAdded_ || !hwnd_ || !IsWindow(hwnd_)) {
            return;
        }
        trayIcon_ = {};
        trayIcon_.cbSize = sizeof(trayIcon_);
        trayIcon_.hWnd = hwnd_;
        trayIcon_.uID = kTrayIconId;
        trayIcon_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        trayIcon_.uCallbackMessage = kTrayCallbackMessage;
        trayIcon_.hIcon = appIconSmall_
            ? appIconSmall_
            : LoadIconW(nullptr, IDI_APPLICATION);
        wcscpy_s(trayIcon_.szTip, L"VTSFloat_Meow");
        if (!Shell_NotifyIconW(NIM_ADD, &trayIcon_)) {
            Log("[tray] failed to add icon error=" + std::to_string(GetLastError()));
            return;
        }
        trayIconAdded_ = true;
        trayIcon_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &trayIcon_);
        Log("[tray] icon added");
    }

    void RemoveTrayIcon() {
        if (!trayIconAdded_) {
            return;
        }
        Shell_NotifyIconW(NIM_DELETE, &trayIcon_);
        trayIconAdded_ = false;
    }

    void SetOverlayVisible(bool visible) {
        overlayVisible_ = visible;
        if (!visible) {
            if (toolbarHwnd_ && IsWindow(toolbarHwnd_)) {
                ShowWindow(toolbarHwnd_, SW_HIDE);
            }
            if (hwnd_ && IsWindow(hwnd_)) {
                ShowWindow(hwnd_, SW_HIDE);
            }
            Log("[tray] overlay hidden");
            return;
        }

        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        SetWindowPos(
            hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        PositionToolbar();
        if (toolbarHwnd_ && IsWindow(toolbarHwnd_)) {
            if (!locked_ || debugMode_) {
                ShowWindow(toolbarHwnd_, SW_SHOWNOACTIVATE);
                SetWindowPos(
                    toolbarHwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                RedrawWindow(
                    toolbarHwnd_, nullptr, nullptr,
                    RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
            } else {
                ShowWindow(toolbarHwnd_, SW_HIDE);
            }
        }
        ++requested_;
        RenderFrame();
        resetFrameSchedule_ = true;
        Log("[tray] overlay shown");
    }

    void ToggleOverlayVisibility() {
        SetOverlayVisible(!overlayVisible_);
    }

    void ActivateTrayCommand(UINT command) {
        switch (command) {
        case kTrayResetCommand:
            ResetToPrimaryMonitor();
            Log("[tray] action=center_primary");
            break;
        case kTrayToggleVisibilityCommand:
            ToggleOverlayVisibility();
            break;
        case kTrayExitCommand:
            Log("[tray] action=exit");
            PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            break;
        default:
            break;
        }
    }

    void ShowTrayMenu() {
        HMENU menu = CreatePopupMenu();
        if (!menu) {
            return;
        }
        AppendMenuW(menu, MF_STRING, kTrayResetCommand,
                    L"\u91cd\u7f6e\u5230\u4e3b\u5c4f\u5e55\u4e2d\u95f4");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(
            menu, MF_STRING, kTrayToggleVisibilityCommand,
            overlayVisible_ ? L"\u6682\u65f6\u9690\u85cf" : L"\u663e\u793a\u7a97\u53e3");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kTrayExitCommand, L"\u9000\u51fa\u7a0b\u5e8f");

        POINT point{};
        GetCursorPos(&point);
        SetForegroundWindow(hwnd_);
        const UINT command = TrackPopupMenu(
            menu,
            TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
            point.x,
            point.y,
            0,
            hwnd_,
            nullptr);
        PostMessageW(hwnd_, WM_NULL, 0, 0);
        DestroyMenu(menu);
        ActivateTrayCommand(command);
    }

    void MoveToNextMonitor() {
        std::vector<HMONITOR> monitors;
        EnumDisplayMonitors(
            nullptr,
            nullptr,
            [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
                reinterpret_cast<std::vector<HMONITOR>*>(data)->push_back(monitor);
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&monitors));
        if (monitors.empty()) {
            return;
        }
        const HMONITOR current = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        auto iterator = std::find(monitors.begin(), monitors.end(), current);
        size_t index = iterator == monitors.end()
            ? 0
            : (static_cast<size_t>(iterator - monitors.begin()) + 1) % monitors.size();
        CenterOnMonitor(monitors[index]);
    }

    std::wstring GpuName(int adapterIndex) const {
        const auto iterator = std::find_if(
            gpuAdapters_.begin(), gpuAdapters_.end(),
            [adapterIndex](const GpuAdapterInfo& info) {
                return static_cast<int>(info.index) == adapterIndex;
            });
        return iterator == gpuAdapters_.end() ? L"未知 GPU" : iterator->name;
    }

    std::wstring GpuButtonText() const {
        if (selectedGpuIndex_ < 0) {
            if (minimumPowerGpuIndex_ >= 0) {
                return L"GPU 推荐：" + GpuName(minimumPowerGpuIndex_);
            }
            return L"GPU 推荐（未检测到核显）";
        }
        if (gpuSelectionFallback_) {
            return L"GPU 已选：" + GpuName(selectedGpuIndex_) + L"（当前回退）";
        }
        return L"GPU：" + GpuName(activeGpuIndex_);
    }

    std::wstring DebugStatusText(bool compact = false) const {
        std::wostringstream text;
        text << std::fixed << std::setprecision(1)
             << L"FPS " << lastUpdateFps_;
        if (vtsApiFps_ > 0) {
            text << L"  VTS " << vtsApiFps_;
        }
        if (!compact) {
            text << L"  " << GpuTelemetryText();
        }
        text << std::setprecision(2)
             << L"  接收 " << lastReceiveMs_ << L"ms"
             << L"  缩放 " << lastMapScaleMs_ << L"ms"
             << L"  上屏 " << lastUpdateMs_ << L"ms";
        if (compact) {
            text << L"\n" << GpuTelemetryText();
        }
        if (debugMode_) {
            text << L"\n";
            if (sourceWidth_ > 0 && dibWidth_ > 0) {
                const double srcPx = static_cast<double>(sourceWidth_) * sourceHeight_;
                const double dstPx = static_cast<double>(dibWidth_) * dibHeight_;
                const double ratio = (std::min)(srcPx, dstPx) / (std::max)(srcPx, dstPx) * 100.0;
                text << L"渲染 " << dibWidth_ << L"x" << dibHeight_
                     << L"  VTS " << sourceWidth_ << L"x" << sourceHeight_
                     << std::setprecision(1) << L"  对齐 " << ratio << L"%";
            }
            const auto extra = vtsApi_.GetExtraStats();
            if (vtsApi_.IsConnected() && extra.uptimeMs > 0) {
                const long long s = extra.uptimeMs / 1000;
                text << L"  API " << std::fixed << std::setprecision(1)
                     << extra.apiLatencyMs << L"ms"
                     << L"  面数 " << extra.artmeshCount
                     << L"  道具 " << extra.itemCount;
            }
        }
        return text.str();
    }

    std::wstring GpuTelemetryText() const {
        std::wostringstream text;
        text << std::fixed << std::setprecision(1)
             << L"CPU ";
        if (cpuUsagePercent_) {
            text << *cpuUsagePercent_ << L"%";
        } else {
            text << L"--";
        }
        text << L"  GPU ";
        if (gpuUsagePercent_) {
            text << *gpuUsagePercent_ << L"%";
        } else {
            text << L"--";
        }
        if (gpuMemoryCurrentBytes_ > 0 || gpuMemoryBudgetBytes_ > 0) {
            constexpr double kMegabyte = 1024.0 * 1024.0;
            text << L"  显存 "
                 << std::setprecision(0)
                 << static_cast<double>(gpuMemoryCurrentBytes_) / kMegabyte
                 << L"/"
                 << static_cast<double>(gpuMemoryBudgetBytes_) / kMegabyte
                 << L" MB";
        }
        if (processMemoryBytes_ > 0) {
            constexpr double kMegabyte = 1024.0 * 1024.0;
            text << L"  内存 "
                 << std::setprecision(0)
                 << static_cast<double>(processMemoryBytes_) / kMegabyte
                 << L" MB";
        }
        return text.str();
    }

    std::wstring FpsButtonText() const {
        if (capturingFps_) {
            return fpsInput_.empty()
                ? L"输入 FPS：_"
                : L"输入 FPS：" + fpsInput_ + L"_";
        }
        if (fpsMode_ == FpsMode::FollowVtsApi) {
            return L"对齐 VTS 渲染";
        }
        if (fpsMode_ == FpsMode::FollowVts) {
            return L"跟随 VTS 配置";
        }
        if (fpsMode_ == FpsMode::FollowMonitor) {
            return L"跟随屏幕 " + std::to_wstring(monitorRefreshFps_) + L" FPS";
        }
        return L"FPS: " + std::to_wstring(targetFps_);
    }

    std::wstring ScalingQualityText() const {
        switch (scalingQuality_) {
        case kScalingPerformance: return L"画质 性能";
        case kScalingQuality: return L"画质 质量";
        default: return L"画质 平衡";
        }
    }

    void SetScalingQuality(int quality) {
        const int selected = (std::clamp)(
            quality, kScalingPerformance, kScalingQuality);
        if (scalingQuality_ == selected) {
            return;
        }
        scalingQuality_ = selected;
        SaveUiSettings();
        if (toolbarHwnd_ && IsWindow(toolbarHwnd_)) {
            InvalidateRect(toolbarHwnd_, nullptr, FALSE);
        }
        Log("[scaler] quality=" + std::to_string(scalingQuality_));
    }

    int EffectiveRequestFps() const {
        if (fpsMode_ == FpsMode::FollowVtsApi) {
            if (vtsApiFps_ > 0) return vtsApiFps_;
            return vtsConfiguredFps_ > 0
                ? vtsConfiguredFps_
                : monitorRefreshFps_;
        }
        if (fpsMode_ == FpsMode::FollowVts) {
            return vtsConfiguredFps_ > 0
                ? vtsConfiguredFps_
                : monitorRefreshFps_;
        }
        return fpsMode_ == FpsMode::FollowMonitor
            ? monitorRefreshFps_
            : targetFps_;
    }

    int QueryCurrentMonitorRefreshRate() const {
        if (!hwnd_) {
            return 60;
        }
        const HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        if (!monitor || !GetMonitorInfoW(
                monitor, reinterpret_cast<MONITORINFO*>(&info))) {
            return 60;
        }
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        if (!EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode) ||
            mode.dmDisplayFrequency <= 1) {
            return 60;
        }
        return (std::clamp)(
            static_cast<int>(mode.dmDisplayFrequency),
            kMinimumFps,
            kMaximumFps);
    }

    void UpdateMonitorRefreshRate(bool force = false) {
        const int refreshRate = QueryCurrentMonitorRefreshRate();
        if (!force && refreshRate == monitorRefreshFps_) {
            return;
        }
        monitorRefreshFps_ = refreshRate;
        resetFrameSchedule_ = true;
        if (toolbarHwnd_ && !locked_) {
            InvalidateRect(toolbarHwnd_, nullptr, FALSE);
        }
        Log("[display] fixed_refresh=" + std::to_string(monitorRefreshFps_) + " Hz");
    }

    void SetFollowVtsFps() {
        fpsMode_ = FpsMode::FollowVts;
        UpdateMonitorRefreshRate(true);
        capturingFps_ = false;
        fpsReplaceOnNextDigit_ = false;
        fpsInput_.clear();
        SaveUiSettings();
        resetFrameSchedule_ = true;
        PositionToolbar();
        RedrawWindow(toolbarHwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        Log("[toolbar] fps_mode=follow_vts");
    }

    void SetFollowVtsApiFps() {
        fpsMode_ = FpsMode::FollowVtsApi;
        UpdateMonitorRefreshRate(true);
        capturingFps_ = false;
        fpsReplaceOnNextDigit_ = false;
        fpsInput_.clear();
        SaveUiSettings();
        resetFrameSchedule_ = true;
        PositionToolbar();
        RedrawWindow(toolbarHwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        Log("[toolbar] fps_mode=follow_vts_api");
    }

    void SetTargetFps(int fps) {
        fpsMode_ = FpsMode::Fixed;
        targetFps_ = (std::clamp)(fps, kMinimumFps, kMaximumFps);
        capturingFps_ = false;
        fpsReplaceOnNextDigit_ = false;
        fpsInput_.clear();
        SaveUiSettings();
        resetFrameSchedule_ = true;
        PositionToolbar();
        RedrawWindow(toolbarHwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        Log("[toolbar] target_fps=" + std::to_string(targetFps_));
    }

    void BeginFpsCapture() {
        if (capturingHotkey_) {
            CancelHotkeyCapture();
        }
        capturingFps_ = true;
        fpsInput_ = std::to_wstring(targetFps_);
        fpsReplaceOnNextDigit_ = true;
        PositionToolbar();
        ShowWindow(toolbarHwnd_, SW_SHOWNOACTIVATE);
        SetActiveWindow(toolbarHwnd_);
        SetForegroundWindow(toolbarHwnd_);
        SetFocus(toolbarHwnd_);
        RedrawWindow(toolbarHwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        Log("[toolbar] fps_input_begin current=" + std::to_string(targetFps_));
    }

    void CancelFpsCapture() {
        if (!capturingFps_) {
            return;
        }
        capturingFps_ = false;
        fpsReplaceOnNextDigit_ = false;
        fpsInput_.clear();
        PositionToolbar();
        RedrawWindow(toolbarHwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        Log("[toolbar] fps_input_cancel");
    }

    void CaptureFpsKey(UINT virtualKey) {
        if (virtualKey == VK_ESCAPE) {
            CancelFpsCapture();
            return;
        }
        if (virtualKey == VK_BACK) {
            if (fpsReplaceOnNextDigit_) {
                fpsInput_.clear();
                fpsReplaceOnNextDigit_ = false;
            } else if (!fpsInput_.empty()) {
                fpsInput_.pop_back();
            }
            RedrawWindow(toolbarHwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            return;
        }
        if (virtualKey == VK_RETURN) {
            if (!fpsInput_.empty()) {
                const int fps = std::stoi(fpsInput_);
                if (fps >= kMinimumFps && fps <= kMaximumFps) {
                    SetTargetFps(fps);
                } else {
                    MessageBeep(MB_ICONWARNING);
                    fpsReplaceOnNextDigit_ = true;
                    RedrawWindow(
                        toolbarHwnd_, nullptr, nullptr,
                        RDW_INVALIDATE | RDW_UPDATENOW);
                }
            } else {
                MessageBeep(MB_ICONWARNING);
            }
            return;
        }
        wchar_t digit = 0;
        if (virtualKey >= '0' && virtualKey <= '9') {
            digit = static_cast<wchar_t>(virtualKey);
        } else if (virtualKey >= VK_NUMPAD0 && virtualKey <= VK_NUMPAD9) {
            digit = static_cast<wchar_t>(L'0' + virtualKey - VK_NUMPAD0);
        }
        if (digit) {
            if (fpsReplaceOnNextDigit_) {
                fpsInput_.clear();
                fpsReplaceOnNextDigit_ = false;
            }
            if (fpsInput_.size() < 3) {
                fpsInput_.push_back(digit);
                RedrawWindow(
                    toolbarHwnd_, nullptr, nullptr,
                    RDW_INVALIDATE | RDW_UPDATENOW);
            }
        }
    }

    void ShowDebugMenu() {
        HMENU menu = CreatePopupMenu();
        if (!menu) return;

        constexpr UINT kDebugToggleCommand = 3401;
        constexpr UINT kResetSettingsCommand = 3402;
        AppendMenuW(
            menu,
            MF_STRING | (debugMode_ ? MF_CHECKED : 0),
            kDebugToggleCommand,
            L"调试");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(
            menu,
            MF_STRING,
            kResetSettingsCommand,
            L"清除缓存并重置脚本");

        const RECT debugRect = ToolbarButtonRect(ToolbarButton::Debug);
        POINT popup{ debugRect.left, debugRect.bottom };
        ClientToScreen(toolbarHwnd_, &popup);
        const UINT command = RunModalWhileRendering([this, menu, popup]() {
            return TrackPopupMenu(
                menu,
                TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,
                popup.x, popup.y, 0, toolbarHwnd_, nullptr);
        });
        DestroyMenu(menu);
        if (!command) return;

        if (command == kDebugToggleCommand) {
            debugMode_ = !debugMode_;
            SaveUiSettings();
            PositionToolbar();
            InvalidateRect(toolbarHwnd_, nullptr, FALSE);
            Log(std::string("[toolbar] debug=") + (debugMode_ ? "1" : "0"));
        } else if (command == kResetSettingsCommand) {
            Log("[toolbar] action=reset_settings");
            ResetSettingsToDefaults();
        }
    }

    void ShowGraphicsSettingsMenu() {
        HMENU menu = CreatePopupMenu();
        if (!menu) return;

        // --- GPU submenu ---
        HMENU gpuSub = CreatePopupMenu();
        constexpr UINT kRecommendedGpuCmd = 1900;
        constexpr UINT kAdapterBase = 2000;
        const bool hasRec = minimumPowerGpuIndex_ >= 0;
        std::wstring recLabel = L"推荐（核显/节能 GPU）";
        if (hasRec) recLabel += L"：" + GpuName(minimumPowerGpuIndex_);
        AppendMenuW(gpuSub,
            MF_STRING | (selectedGpuIndex_ == minimumPowerGpuIndex_ && hasRec ? MF_CHECKED : 0)
                | (hasRec ? 0 : MF_GRAYED),
            kRecommendedGpuCmd, recLabel.c_str());
        AppendMenuW(gpuSub, MF_SEPARATOR, 0, nullptr);
        for (size_t i = 0; i < gpuAdapters_.size(); ++i) {
            const auto& info = gpuAdapters_[i];
            UINT flags = MF_STRING;
            if (selectedGpuIndex_ == static_cast<int>(info.index)) flags |= MF_CHECKED;
            std::wstring label = info.name;
            if (static_cast<int>(info.index) == senderGpuIndex_) label += L"  [Spout]";
            AppendMenuW(gpuSub, flags, kAdapterBase + static_cast<UINT>(i), label.c_str());
        }
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(gpuSub), L"GPU 选择");

        // --- Quality submenu ---
        HMENU qualSub = CreatePopupMenu();
        constexpr UINT kPerfCmd = 3290;
        constexpr UINT kBalCmd = 3291;
        constexpr UINT kQualCmd = 3292;
        AppendMenuW(qualSub, MF_STRING | (scalingQuality_ == kScalingPerformance ? MF_CHECKED : 0),
            kPerfCmd, L"性能（最近邻）");
        AppendMenuW(qualSub, MF_STRING | (scalingQuality_ == kScalingBalanced ? MF_CHECKED : 0),
            kBalCmd, L"平衡（双线性，推荐）");
        AppendMenuW(qualSub, MF_STRING | (scalingQuality_ == kScalingQuality ? MF_CHECKED : 0),
            kQualCmd, L"质量（双三次）");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(qualSub), L"画质");

        // --- FPS submenu ---
        HMENU fpsSub = CreatePopupMenu();
        constexpr UINT kFollowVtsApiCmd = 3097;
        constexpr UINT kFollowVtsCmd = 3098;
        constexpr UINT kFpsBase = 3100;
        constexpr UINT kCustomFpsCmd = 3199;
        constexpr int presets[] = { 30, 45, 60, 90, 120, 144, 165, 240 };
        const bool apiOk = vtsApi_.IsConnected();
        AppendMenuW(fpsSub,
            MF_STRING | (fpsMode_ == FpsMode::FollowVtsApi ? MF_CHECKED : 0) | (apiOk ? 0 : MF_GRAYED),
            kFollowVtsApiCmd,
            apiOk ? L"对齐 VTS 实际渲染帧数（推荐）" : L"对齐 VTS 实际渲染帧数（未启用 API）");
        AppendMenuW(fpsSub,
            MF_STRING | (fpsMode_ == FpsMode::FollowVts ? MF_CHECKED : 0),
            kFollowVtsCmd, L"跟随 VTS 配置");
        AppendMenuW(fpsSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(fpsSub, MF_STRING, kCustomFpsCmd, L"自定义…（1-240）");
        AppendMenuW(fpsSub, MF_SEPARATOR, 0, nullptr);
        for (size_t i = 0; i < ARRAYSIZE(presets); ++i) {
            UINT flags = MF_STRING | (fpsMode_ == FpsMode::Fixed && targetFps_ == presets[i] ? MF_CHECKED : 0);
            AppendMenuW(fpsSub, flags, kFpsBase + static_cast<UINT>(i),
                (std::to_wstring(presets[i]) + L" FPS").c_str());
        }
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fpsSub), L"帧率");


        // Show menu
        RECT btnRect = ToolbarButtonRect(ToolbarButton::GraphicsSettings);
        POINT popup{ btnRect.left, btnRect.bottom };
        ClientToScreen(toolbarHwnd_, &popup);
        const UINT command = RunModalWhileRendering([this, menu, popup]() {
            return TrackPopupMenu(menu,
                TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,
                popup.x, popup.y, 0, toolbarHwnd_, nullptr);
        });
        DestroyMenu(menu);
        if (!command) return;
        Log("[toolbar] graphics_menu_command=" + std::to_string(command));

        // Handle GPU
        if (command == kRecommendedGpuCmd) {
            int requested = minimumPowerGpuIndex_;
            if (requested >= 0 && requested != senderGpuIndex_) {
                RestartVtsOnGpu(requested);
            } else {
                selectedGpuIndex_ = requested;
                SaveUiSettings();
            }
        } else if (command >= kAdapterBase && command < kAdapterBase + gpuAdapters_.size()) {
            int requested = static_cast<int>(gpuAdapters_[command - kAdapterBase].index);
            if (requested >= 0 && requested != senderGpuIndex_) {
                RestartVtsOnGpu(requested);
            } else {
                selectedGpuIndex_ = requested;
                SaveUiSettings();
            }
        }
        // Handle Quality
        else if (command == kPerfCmd) { SetScalingQuality(kScalingPerformance); }
        else if (command == kBalCmd) { SetScalingQuality(kScalingBalanced); }
        else if (command == kQualCmd) { SetScalingQuality(kScalingQuality); }
        // Handle FPS
        else if (command == kFollowVtsApiCmd) { SetFollowVtsApiFps(); }
        else if (command == kFollowVtsCmd) { SetFollowVtsFps(); }
        else if (command >= kFpsBase && command < kFpsBase + ARRAYSIZE(presets)) {
            SetTargetFps(presets[command - kFpsBase]);
        }
        else if (command == kCustomFpsCmd) { ShowCustomFpsDialog(); }
    }

    void ShowFpsMenu() {
        HMENU menu = CreatePopupMenu();
        if (!menu) return;
        constexpr UINT kFollowVtsApiCommand = 3097;
        constexpr UINT kFollowVtsCommand = 3098;
        constexpr UINT kPresetBase = 3100;
        constexpr UINT kCustomCommand = 3199;
        constexpr int presets[] = { 30, 45, 60, 90, 120, 144, 165, 240 };
        const bool apiAvailable = vtsApi_.IsConnected();
        AppendMenuW(
            menu,
            MF_STRING | (fpsMode_ == FpsMode::FollowVtsApi ? MF_CHECKED : 0)
                | (apiAvailable ? 0 : MF_GRAYED),
            kFollowVtsApiCommand,
            apiAvailable
                ? L"对齐 VTS 实际渲染帧数（推荐）"
                : L"对齐 VTS 实际渲染帧数（未启用 API）");
        AppendMenuW(
            menu,
            MF_STRING | (fpsMode_ == FpsMode::FollowVts ? MF_CHECKED : 0),
            kFollowVtsCommand,
            L"跟随 VTS 配置");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kCustomCommand, L"自定义…（1-240）");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        for (size_t index = 0; index < ARRAYSIZE(presets); ++index) {
            const int fps = presets[index];
            const UINT flags = MF_STRING |
                (fpsMode_ == FpsMode::Fixed && targetFps_ == fps ? MF_CHECKED : 0);
            const std::wstring label = std::to_wstring(fps) + L" FPS";
            AppendMenuW(menu, flags, kPresetBase + static_cast<UINT>(index), label.c_str());
        }
        RECT fpsRect = ToolbarButtonRect(ToolbarButton::FrameRate);
        POINT popup{ fpsRect.left, fpsRect.bottom };
        ClientToScreen(toolbarHwnd_, &popup);
        const UINT command = RunModalWhileRendering([this, menu, popup]() {
            return TrackPopupMenu(
                menu,
                TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,
                popup.x, popup.y, 0, toolbarHwnd_, nullptr);
        });
        DestroyMenu(menu);
        Log("[toolbar] fps_menu_command=" + std::to_string(command));
        if (command == kFollowVtsApiCommand) {
            SetFollowVtsApiFps();
        } else if (command == kFollowVtsCommand) {
            SetFollowVtsFps();
        } else if (command >= kPresetBase &&
            command < kPresetBase + ARRAYSIZE(presets)) {
            SetTargetFps(presets[command - kPresetBase]);
        } else if (command == kCustomCommand) {
            ShowCustomFpsDialog();
        }
    }

    void ShowScalingQualityMenu() {
        HMENU menu = CreatePopupMenu();
        if (!menu) return;
        constexpr UINT kPerformanceCommand = 3290;
        constexpr UINT kBalancedCommand = 3291;
        constexpr UINT kQualityCommand = 3292;
        AppendMenuW(
            menu,
            MF_STRING | (scalingQuality_ == kScalingPerformance ? MF_CHECKED : 0),
            kPerformanceCommand,
            L"性能（最近邻，最低开销）");
        AppendMenuW(
            menu,
            MF_STRING | (scalingQuality_ == kScalingBalanced ? MF_CHECKED : 0),
            kBalancedCommand,
            L"平衡（GPU 双线性，推荐）");
        AppendMenuW(
            menu,
            MF_STRING | (scalingQuality_ == kScalingQuality ? MF_CHECKED : 0),
            kQualityCommand,
            L"质量（GPU 双三次，更细腻）");
        RECT qualityRect = ToolbarButtonRect(ToolbarButton::Quality);
        POINT popup{ qualityRect.left, qualityRect.bottom };
        ClientToScreen(toolbarHwnd_, &popup);
        const UINT command = RunModalWhileRendering([this, menu, popup]() {
            return TrackPopupMenu(
                menu,
                TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,
                popup.x, popup.y, 0, toolbarHwnd_, nullptr);
        });
        DestroyMenu(menu);
        if (command == kPerformanceCommand) {
            SetScalingQuality(kScalingPerformance);
        } else if (command == kBalancedCommand) {
            SetScalingQuality(kScalingBalanced);
        } else if (command == kQualityCommand) {
            SetScalingQuality(kScalingQuality);
        }
    }

    void ShowVtsApiGuide() {
        HRSRC resource = FindResourceW(instance_, MAKEINTRESOURCEW(IDR_VTS_API_GUIDE), RT_RCDATA);
        if (!resource) return;
        HGLOBAL loaded = LoadResource(instance_, resource);
        if (!loaded) return;
        const void* data = LockResource(loaded);
        const DWORD size = SizeofResource(instance_, resource);
        if (!data || size == 0) return;

        Gdiplus::GdiplusStartupInput gdipInput{};
        ULONG_PTR token = 0;
        if (Gdiplus::GdiplusStartup(&token, &gdipInput, nullptr) != Gdiplus::Ok) return;

        IStream* stream = SHCreateMemStream(
            static_cast<const BYTE*>(data), size);
        if (!stream) { Gdiplus::GdiplusShutdown(token); return; }

        Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromStream(stream);
        stream->Release();
        if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok) {
            delete bitmap;
            Gdiplus::GdiplusShutdown(token);
            return;
        }

        const int imgW = static_cast<int>(bitmap->GetWidth());
        const int imgH = static_cast<int>(bitmap->GetHeight());

        const int maxW = 900;
        const int maxH = 700;
        int dispW = imgW;
        int dispH = imgH;
        if (dispW > maxW) { dispH = dispH * maxW / dispW; dispW = maxW; }
        if (dispH > maxH) { dispW = dispW * maxH / dispH; dispH = maxH; }

        const int winW = dispW + 16;
        const int winH = dispH + 39;

        WNDCLASSW wc{};
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = instance_;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = L"VTSFloatApiGuide";
        RegisterClassW(&wc);

        HWND guideHwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
            L"VTSFloatApiGuide",
            L"如何开启 VTubeStudio Plugins API",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
            toolbarHwnd_, nullptr, instance_, nullptr);
        if (!guideHwnd) {
            delete bitmap;
            Gdiplus::GdiplusShutdown(token);
            return;
        }

        if (appIconSmall_) SendMessageW(guideHwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(appIconSmall_));
        if (appIconLarge_) SendMessageW(guideHwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(appIconLarge_));

        MONITORINFO mi{}; mi.cbSize = sizeof(mi);
        GetMonitorInfoW(MonitorFromWindow(toolbarHwnd_, MONITOR_DEFAULTTONEAREST), &mi);
        const int cx = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - winW) / 2;
        const int cy = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - winH) / 2;
        SetWindowPos(guideHwnd, HWND_TOPMOST, cx, cy, winW, winH, SWP_NOACTIVATE);
        ShowWindow(guideHwnd, SW_SHOW);
        UpdateWindow(guideHwnd);

        HDC dc = GetDC(guideHwnd);
        Gdiplus::Graphics graphics(dc);
        RECT cr{}; GetClientRect(guideHwnd, &cr);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.DrawImage(bitmap, 0, 0, cr.right, cr.bottom);
        ReleaseDC(guideHwnd, dc);

        MSG msg{};
        while (IsWindow(guideHwnd) && GetMessageW(&msg, nullptr, 0, 0)) {
            if (msg.hwnd == guideHwnd && msg.message == WM_SYSCOMMAND &&
                (msg.wParam & 0xFFF0) == SC_CLOSE) {
                DestroyWindow(guideHwnd);
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (IsWindow(guideHwnd)) DestroyWindow(guideHwnd);
        UnregisterClassW(L"VTSFloatApiGuide", instance_);
        delete bitmap;
        Gdiplus::GdiplusShutdown(token);
    }

    void ShowCustomFpsDialog() {
        Log("[toolbar] fps_dialog_begin current=" + std::to_string(targetFps_));
        const INT_PTR result = RunModalWhileRendering([this]() {
            return DialogBoxParamW(
                instance_,
                MAKEINTRESOURCEW(IDD_CUSTOM_FPS),
                toolbarHwnd_,
                &LayeredOverlay::FpsDialogProc,
                reinterpret_cast<LPARAM>(this));
        });
        if (result >= kMinimumFps && result <= kMaximumFps) {
            SetTargetFps(static_cast<int>(result));
        } else if (result == -1) {
            Log("[toolbar] fps_dialog_failed error=" +
                std::to_string(GetLastError()));
            MessageBoxW(
                toolbarHwnd_,
                L"无法打开自定义帧率输入框。",
                L"VTSFloat_Meow",
                MB_OK | MB_ICONERROR);
        } else {
            Log("[toolbar] fps_dialog_cancel");
        }
    }

    static INT_PTR CALLBACK FpsDialogProc(
        HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
        LayeredOverlay* overlay = reinterpret_cast<LayeredOverlay*>(
            GetWindowLongPtrW(dialog, DWLP_USER));
        if (message == WM_INITDIALOG) {
            overlay = reinterpret_cast<LayeredOverlay*>(lParam);
            SetWindowLongPtrW(
                dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(overlay));
            SetDlgItemInt(dialog, IDC_FPS_EDIT, overlay->targetFps_, FALSE);
            SendDlgItemMessageW(dialog, IDC_FPS_EDIT, EM_SETSEL, 0, -1);

            RECT owner{};
            RECT window{};
            GetWindowRect(overlay->toolbarHwnd_, &owner);
            GetWindowRect(dialog, &window);
            const int width = window.right - window.left;
            const int height = window.bottom - window.top;
            int x = owner.left + (owner.right - owner.left - width) / 2;
            int y = owner.bottom + 12;
            MONITORINFO info{};
            info.cbSize = sizeof(info);
            if (GetMonitorInfoW(
                    MonitorFromWindow(dialog, MONITOR_DEFAULTTONEAREST), &info)) {
                x = (std::clamp)(
                    x,
                    static_cast<int>(info.rcWork.left),
                    static_cast<int>(info.rcWork.right) - width);
                y = (std::clamp)(
                    y,
                    static_cast<int>(info.rcWork.top),
                    static_cast<int>(info.rcWork.bottom) - height);
            }
            SetWindowPos(
                dialog, HWND_TOPMOST, x, y, width, height,
                SWP_SHOWWINDOW);
            SetForegroundWindow(dialog);
            SetFocus(GetDlgItem(dialog, IDC_FPS_EDIT));
            return FALSE;
        }
        if (message == WM_COMMAND) {
            switch (LOWORD(wParam)) {
            case IDOK: {
                wchar_t input[16]{};
                GetDlgItemTextW(dialog, IDC_FPS_EDIT, input, ARRAYSIZE(input));
                wchar_t* end = nullptr;
                const long fps = wcstol(input, &end, 10);
                if (!input[0] || !end || *end != L'\0' ||
                    fps < kMinimumFps || fps > kMaximumFps) {
                    MessageBoxW(
                        dialog,
                        L"请输入 1 到 240 之间的整数。",
                        L"帧率无效",
                        MB_OK | MB_ICONWARNING);
                    SendDlgItemMessageW(dialog, IDC_FPS_EDIT, EM_SETSEL, 0, -1);
                    SetFocus(GetDlgItem(dialog, IDC_FPS_EDIT));
                    return TRUE;
                }
                EndDialog(dialog, static_cast<INT_PTR>(fps));
                return TRUE;
            }
            case IDCANCEL:
                EndDialog(dialog, 0);
                return TRUE;
            default:
                break;
            }
        }
        if (message == WM_CLOSE) {
            EndDialog(dialog, 0);
            return TRUE;
        }
        return FALSE;
    }

    struct RainbowColor {
        std::uint8_t red;
        std::uint8_t green;
        std::uint8_t blue;
    };

    #if 0
    struct BorderDialogState {
        LayeredOverlay* overlay = nullptr;
        int originalMode = kBorderModeNormal;
        COLORREF originalStart = kDefaultBorderStart;
        COLORREF originalEnd = kDefaultBorderEnd;
        int originalThickness = kDefaultBorderThickness;
        int editingColor = 0;
    };

    void ShowBorderSettingsDialog() {
        BorderDialogState state{};
        state.overlay = this;
        state.originalMode = borderMode_;
        state.originalStart = borderStartColor_;
        state.originalEnd = borderEndColor_;
        state.originalThickness = borderThickness_;
        Log("[toolbar] border_dialog_begin");
        const INT_PTR result = DialogBoxParamW(
            instance_,
            MAKEINTRESOURCEW(IDD_BORDER_SETTINGS),
            toolbarHwnd_,
            &LayeredOverlay::BorderDialogProc,
            reinterpret_cast<LPARAM>(&state));
        if (result == -1) {
            Log("[toolbar] border_dialog_failed error=" +
                std::to_string(GetLastError()));
        }
    }

    static COLORREF BorderDialogEditedColor(const BorderDialogState* state) {
        return state->editingColor == 0
            ? state->overlay->borderStartColor_
            : state->overlay->borderEndColor_;
    }

    static void LoadBorderColorTracks(HWND dialog, BorderDialogState* state) {
        const COLORREF color = BorderDialogEditedColor(state);
        SendDlgItemMessageW(dialog, IDC_BORDER_RED, TBM_SETPOS, TRUE, GetRValue(color));
        SendDlgItemMessageW(dialog, IDC_BORDER_GREEN, TBM_SETPOS, TRUE, GetGValue(color));
        SendDlgItemMessageW(dialog, IDC_BORDER_BLUE, TBM_SETPOS, TRUE, GetBValue(color));
        SetDlgItemInt(dialog, IDC_BORDER_RED_VALUE, GetRValue(color), FALSE);
        SetDlgItemInt(dialog, IDC_BORDER_GREEN_VALUE, GetGValue(color), FALSE);
        SetDlgItemInt(dialog, IDC_BORDER_BLUE_VALUE, GetBValue(color), FALSE);
    }

    static void ApplyBorderColorTracks(HWND dialog, BorderDialogState* state) {
        const int red = static_cast<int>(
            SendDlgItemMessageW(dialog, IDC_BORDER_RED, TBM_GETPOS, 0, 0));
        const int green = static_cast<int>(
            SendDlgItemMessageW(dialog, IDC_BORDER_GREEN, TBM_GETPOS, 0, 0));
        const int blue = static_cast<int>(
            SendDlgItemMessageW(dialog, IDC_BORDER_BLUE, TBM_GETPOS, 0, 0));
        const COLORREF color = RGB(red, green, blue);
        if (state->editingColor == 0) {
            state->overlay->borderStartColor_ = color;
        } else {
            state->overlay->borderEndColor_ = color;
        }
        SetDlgItemInt(dialog, IDC_BORDER_RED_VALUE, red, FALSE);
        SetDlgItemInt(dialog, IDC_BORDER_GREEN_VALUE, green, FALSE);
        SetDlgItemInt(dialog, IDC_BORDER_BLUE_VALUE, blue, FALSE);
    }

    static void RenderBorderDialogChange(HWND dialog, BorderDialogState* state) {
        InvalidateRect(GetDlgItem(dialog, IDC_BORDER_PREVIEW), nullptr, TRUE);
        ++state->overlay->requested_;
        state->overlay->RenderFrame();
    }

    static void RestoreBorderDialogState(HWND dialog, BorderDialogState* state) {
        state->overlay->borderMode_ = state->originalMode;
        state->overlay->borderStartColor_ = state->originalStart;
        state->overlay->borderEndColor_ = state->originalEnd;
        state->overlay->borderThickness_ = state->originalThickness;
        KillTimer(dialog, 1);
        ++state->overlay->requested_;
        state->overlay->RenderFrame();
    }

    static INT_PTR CALLBACK BorderDialogProc(
        HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* state = reinterpret_cast<BorderDialogState*>(
            GetWindowLongPtrW(dialog, DWLP_USER));
        if (message == WM_INITDIALOG) {
            state = reinterpret_cast<BorderDialogState*>(lParam);
            SetWindowLongPtrW(
                dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(state));

            SendDlgItemMessageW(
                dialog, IDC_BORDER_MODE, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(L"普通渐变（整圈同步变色）"));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_MODE, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(L"彩虹跑马灯"));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_MODE, CB_SETCURSEL,
                state->overlay->borderMode_, 0);
            SendDlgItemMessageW(
                dialog, IDC_BORDER_COLOR_TARGET, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(L"起始颜色"));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_COLOR_TARGET, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(L"结束颜色"));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_COLOR_TARGET, CB_SETCURSEL, 0, 0);

            constexpr int colorTracks[] = {
                IDC_BORDER_RED, IDC_BORDER_GREEN, IDC_BORDER_BLUE
            };
            for (int control : colorTracks) {
                SendDlgItemMessageW(
                    dialog, control, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
                SendDlgItemMessageW(dialog, control, TBM_SETPAGESIZE, 0, 16);
                SendDlgItemMessageW(dialog, control, TBM_SETTICFREQ, 32, 0);
            }
            SendDlgItemMessageW(
                dialog, IDC_BORDER_THICKNESS,
                TBM_SETRANGE, TRUE, MAKELPARAM(1, 10));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_THICKNESS,
                TBM_SETPOS, TRUE, state->overlay->borderThickness_);
            SetDlgItemInt(
                dialog, IDC_BORDER_THICKNESS_VALUE,
                state->overlay->borderThickness_, FALSE);
            LoadBorderColorTracks(dialog, state);

            RECT owner{};
            RECT window{};
            GetWindowRect(state->overlay->toolbarHwnd_, &owner);
            GetWindowRect(dialog, &window);
            const int width = window.right - window.left;
            const int height = window.bottom - window.top;
            int x = owner.left + (owner.right - owner.left - width) / 2;
            int y = owner.bottom + 12;
            MONITORINFO info{};
            info.cbSize = sizeof(info);
            if (GetMonitorInfoW(
                    MonitorFromWindow(dialog, MONITOR_DEFAULTTONEAREST), &info)) {
                x = (std::clamp)(
                    x, static_cast<int>(info.rcWork.left),
                    static_cast<int>(info.rcWork.right) - width);
                y = (std::clamp)(
                    y, static_cast<int>(info.rcWork.top),
                    static_cast<int>(info.rcWork.bottom) - height);
            }
            SetWindowPos(
                dialog, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW);
            SetTimer(dialog, 1, 16, nullptr);
            return TRUE;
        }

        if (!state) {
            return FALSE;
        }

        switch (message) {
        case WM_TIMER:
            if (wParam == 1) {
                ++state->overlay->requested_;
                state->overlay->RenderFrame();
                return TRUE;
            }
            break;
        case WM_HSCROLL: {
            const HWND control = reinterpret_cast<HWND>(lParam);
            if (control == GetDlgItem(dialog, IDC_BORDER_THICKNESS)) {
                state->overlay->borderThickness_ = static_cast<int>(
                    SendDlgItemMessageW(
                        dialog, IDC_BORDER_THICKNESS, TBM_GETPOS, 0, 0));
                SetDlgItemInt(
                    dialog, IDC_BORDER_THICKNESS_VALUE,
                    state->overlay->borderThickness_, FALSE);
            } else {
                ApplyBorderColorTracks(dialog, state);
            }
            RenderBorderDialogChange(dialog, state);
            return TRUE;
        }
        case WM_DRAWITEM: {
            const auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (item && item->CtlID == IDC_BORDER_PREVIEW) {
                const int width = item->rcItem.right - item->rcItem.left;
                const int height = item->rcItem.bottom - item->rcItem.top;
                const auto elapsedMs =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        Clock::now().time_since_epoch()).count();
                const int phase = static_cast<int>((elapsedMs % 5000) * 1536 / 5000);
                for (int x = 0; x < width; ++x) {
                    RainbowColor color{};
                    if (state->overlay->borderMode_ == kBorderModeChase) {
                        color = state->overlay->RainbowAt(x, (std::max)(1, width), phase);
                    } else {
                        const int denominator = (std::max)(1, width - 1);
                        const COLORREF start = state->overlay->borderStartColor_;
                        const COLORREF end = state->overlay->borderEndColor_;
                        color.red = static_cast<std::uint8_t>(
                            (GetRValue(start) * (denominator - x) +
                             GetRValue(end) * x) / denominator);
                        color.green = static_cast<std::uint8_t>(
                            (GetGValue(start) * (denominator - x) +
                             GetGValue(end) * x) / denominator);
                        color.blue = static_cast<std::uint8_t>(
                            (GetBValue(start) * (denominator - x) +
                             GetBValue(end) * x) / denominator);
                    }
                    for (int y = 0; y < height; ++y) {
                        SetPixelV(
                            item->hDC, x, y,
                            RGB(color.red, color.green, color.blue));
                    }
                }
                FrameRect(
                    item->hDC, &item->rcItem,
                    static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
                return TRUE;
            }
            break;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
            case IDC_BORDER_MODE:
                if (HIWORD(wParam) == CBN_SELCHANGE) {
                    state->overlay->borderMode_ = static_cast<int>(
                        SendDlgItemMessageW(
                            dialog, IDC_BORDER_MODE, CB_GETCURSEL, 0, 0));
                    RenderBorderDialogChange(dialog, state);
                }
                return TRUE;
            case IDC_BORDER_COLOR_TARGET:
                if (HIWORD(wParam) == CBN_SELCHANGE) {
                    state->editingColor = static_cast<int>(
                        SendDlgItemMessageW(
                            dialog, IDC_BORDER_COLOR_TARGET,
                            CB_GETCURSEL, 0, 0));
                    LoadBorderColorTracks(dialog, state);
                    InvalidateRect(
                        GetDlgItem(dialog, IDC_BORDER_PREVIEW), nullptr, TRUE);
                }
                return TRUE;
            case IDC_BORDER_RESET:
                state->overlay->borderMode_ = kBorderModeNormal;
                state->overlay->borderStartColor_ = kDefaultBorderStart;
                state->overlay->borderEndColor_ = kDefaultBorderEnd;
                state->overlay->borderThickness_ = kDefaultBorderThickness;
                state->editingColor = 0;
                SendDlgItemMessageW(
                    dialog, IDC_BORDER_MODE, CB_SETCURSEL,
                    kBorderModeNormal, 0);
                SendDlgItemMessageW(
                    dialog, IDC_BORDER_COLOR_TARGET, CB_SETCURSEL, 0, 0);
                SendDlgItemMessageW(
                    dialog, IDC_BORDER_THICKNESS,
                    TBM_SETPOS, TRUE, kDefaultBorderThickness);
                SetDlgItemInt(
                    dialog, IDC_BORDER_THICKNESS_VALUE,
                    kDefaultBorderThickness, FALSE);
                LoadBorderColorTracks(dialog, state);
                RenderBorderDialogChange(dialog, state);
                return TRUE;
            case IDOK:
                KillTimer(dialog, 1);
                state->overlay->SaveUiSettings();
                Log("[toolbar] border_settings_saved mode=" +
                    std::to_string(state->overlay->borderMode_) +
                    " thickness=" +
                    std::to_string(state->overlay->borderThickness_));
                EndDialog(dialog, IDOK);
                return TRUE;
            case IDCANCEL:
                RestoreBorderDialogState(dialog, state);
                Log("[toolbar] border_settings_cancel");
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            default:
                break;
            }
            break;
        case WM_CLOSE:
            RestoreBorderDialogState(dialog, state);
            Log("[toolbar] border_settings_cancel");
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        default:
            break;
        }
        return FALSE;
    }

    #endif

    struct BorderDialogState {
        LayeredOverlay* overlay = nullptr;
        int originalMode = kBorderModeNormal;
        COLORREF originalColor = kDefaultCustomBorderColor;
        int originalThickness = kDefaultBorderThickness;
        bool originalHoverFadeEnabled = true;
        int originalHoverOpacityPercent = kDefaultHoverOpacityPercent;
        int originalModelOpacityPercent = 100;
        int originalHoverExpandPx = 0;
        bool originalHoverExpressionEnabled = false;
        std::string originalHoverExpressionFile;
        std::vector<std::uint32_t> wheelPixels;
        int wheelBitmapWidth = 0;
        int wheelBitmapHeight = 0;
        int wheelBitmapValuePercent = -1;
        double colorHue = 0.0;
        double colorSaturation = 0.0;
        int colorValuePercent = 100;
        int activeHit = 0;
    };

    static RainbowColor HsvWheelColor(
        double hue, double saturation, double value = 1.0) {
        hue -= std::floor(hue);
        saturation = (std::clamp)(saturation, 0.0, 1.0);
        value = (std::clamp)(value, 0.0, 1.0);
        const double scaled = hue * 6.0;
        const int sector = static_cast<int>(std::floor(scaled)) % 6;
        const double fraction = scaled - std::floor(scaled);
        const double low = 1.0 - saturation;
        const double falling = 1.0 - fraction * saturation;
        const double rising = 1.0 - (1.0 - fraction) * saturation;
        double red = 0.0;
        double green = 0.0;
        double blue = 0.0;
        switch (sector) {
        case 0: red = 1.0; green = rising; blue = low; break;
        case 1: red = falling; green = 1.0; blue = low; break;
        case 2: red = low; green = 1.0; blue = rising; break;
        case 3: red = low; green = falling; blue = 1.0; break;
        case 4: red = rising; green = low; blue = 1.0; break;
        default: red = 1.0; green = low; blue = falling; break;
        }
        return {
            static_cast<std::uint8_t>(red * value * 255.0 + 0.5),
            static_cast<std::uint8_t>(green * value * 255.0 + 0.5),
            static_cast<std::uint8_t>(blue * value * 255.0 + 0.5)
        };
    }

    static void CustomColorHueSaturation(
        COLORREF color, double& hue, double& saturation) {
        const double red = GetRValue(color) / 255.0;
        const double green = GetGValue(color) / 255.0;
        const double blue = GetBValue(color) / 255.0;
        const double maximum = (std::max)({ red, green, blue });
        const double minimum = (std::min)({ red, green, blue });
        const double delta = maximum - minimum;
        saturation = maximum <= 0.0 ? 0.0 : delta / maximum;
        if (delta <= 0.00001) {
            hue = 0.0;
        } else if (maximum == red) {
            hue = (green - blue) / delta / 6.0;
        } else if (maximum == green) {
            hue = (2.0 + (blue - red) / delta) / 6.0;
        } else {
            hue = (4.0 + (red - green) / delta) / 6.0;
        }
        if (hue < 0.0) hue += 1.0;
    }

    static int CustomColorValuePercent(COLORREF color) {
        const int maximum = (std::max)({
            static_cast<int>(GetRValue(color)),
            static_cast<int>(GetGValue(color)),
            static_cast<int>(GetBValue(color)) });
        return (std::clamp)(
            static_cast<int>(std::lround(maximum * 100.0 / 255.0)), 0, 100);
    }

    static void SetBorderControlDlu(
        HWND dialog, int controlId, int x, int y, int width, int height) {
        RECT rect{ x, y, x + width, y + height };
        MapDialogRect(dialog, &rect);
        SetWindowPos(
            GetDlgItem(dialog, controlId), nullptr,
            rect.left, rect.top,
            rect.right - rect.left, rect.bottom - rect.top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }

    static void UpdateCustomColorText(HWND dialog, BorderDialogState* state) {
        const COLORREF color = state->overlay->customBorderColor_;
        wchar_t text[96]{};
        swprintf_s(
            text,
            L"#%02X%02X%02X  ·  RGB(%u, %u, %u)",
            GetRValue(color), GetGValue(color), GetBValue(color),
            GetRValue(color), GetGValue(color), GetBValue(color));
        SetDlgItemTextW(dialog, IDC_BORDER_COLOR_TEXT, text);
    }

    static void UpdateHoverOpacityText(HWND dialog, BorderDialogState* state) {
        const std::wstring text =
            std::to_wstring(state->overlay->hoverOpacityPercent_) + L"%";
        SetDlgItemTextW(dialog, IDC_HOVER_OPACITY_VALUE, text.c_str());
    }

    static BOOL CALLBACK DarkThemeEnumProc(HWND child, LPARAM) {
        wchar_t className[64]{};
        GetClassNameW(child, className, ARRAYSIZE(className));
        if (_wcsicmp(className, L"BUTTON") == 0 ||
            _wcsicmp(className, L"COMBOBOX") == 0 ||
            _wcsicmp(className, L"msctls_trackbar32") == 0) {
            SetWindowTheme(child, L"DarkMode_Explorer", nullptr);
        }
        return TRUE;
    }

    static void ApplyDarkThemeToDialog(HWND dialog) {
        BOOL dark = TRUE;
        DwmSetWindowAttribute(dialog, 20, &dark, sizeof(dark));
        DwmSetWindowAttribute(dialog, 19, &dark, sizeof(dark));
        EnumChildWindows(dialog, &DarkThemeEnumProc, 0);
    }

    static void UpdateHoverOpacityControls(HWND dialog, BorderDialogState* state) {
        const bool enabled = state->overlay->hoverFadeEnabled_;
        CheckDlgButton(
            dialog, IDC_HOVER_FADE_ENABLED,
            enabled ? BST_CHECKED : BST_UNCHECKED);
        const int show = enabled ? SW_SHOW : SW_HIDE;
        ShowWindow(GetDlgItem(dialog, IDC_HOVER_OPACITY), show);
        ShowWindow(GetDlgItem(dialog, IDC_HOVER_OPACITY_LABEL), show);
        ShowWindow(GetDlgItem(dialog, IDC_HOVER_OPACITY_VALUE), show);
        ShowWindow(GetDlgItem(dialog, IDC_HOVER_EXPAND), show);
        ShowWindow(GetDlgItem(dialog, IDC_HOVER_EXPAND_LABEL), show);
        ShowWindow(GetDlgItem(dialog, IDC_HOVER_EXPAND_VALUE), show);
        ShowWindow(GetDlgItem(dialog, IDC_HOVER_EXPAND_RESET), show);
        UpdateHoverOpacityText(dialog, state);
    }

    static void UpdateBorderDialogLayout(HWND dialog, BorderDialogState* state) {
        const bool custom = state->overlay->borderMode_ == kBorderModeCustom;
        ShowWindow(GetDlgItem(dialog, IDC_BORDER_COLOR_WHEEL), custom ? SW_SHOW : SW_HIDE);
        ShowWindow(
            GetDlgItem(dialog, IDC_BORDER_COLOR_BRIGHTNESS),
            custom ? SW_SHOW : SW_HIDE);
        ShowWindow(
            GetDlgItem(dialog, IDC_BORDER_COLOR_BRIGHTNESS_LABEL),
            custom ? SW_SHOW : SW_HIDE);
        ShowWindow(GetDlgItem(dialog, IDC_BORDER_COLOR_TEXT), custom ? SW_SHOW : SW_HIDE);

        if (custom) {
            SetBorderControlDlu(dialog, IDC_BORDER_COLOR_BRIGHTNESS_LABEL, 10, 248, 40, 12);
            SetBorderControlDlu(dialog, IDC_BORDER_COLOR_BRIGHTNESS, 52, 244, 238, 20);
            SetBorderControlDlu(dialog, IDC_BORDER_THICKNESS_LABEL, 10, 270, 40, 12);
            SetBorderControlDlu(dialog, IDC_BORDER_THICKNESS, 52, 266, 200, 20);
            SetBorderControlDlu(dialog, IDC_BORDER_THICKNESS_VALUE, 258, 270, 32, 12);
            SetBorderControlDlu(dialog, IDC_MODEL_OPACITY_LABEL, 10, 295, 78, 12);
            SetBorderControlDlu(dialog, IDC_MODEL_OPACITY, 88, 290, 160, 20);
            SetBorderControlDlu(dialog, IDC_MODEL_OPACITY_VALUE, 252, 295, 36, 12);
            SetBorderControlDlu(dialog, IDC_HOVER_FADE_ENABLED, 10, 317, 280, 14);
            SetBorderControlDlu(dialog, IDC_HOVER_HINT, 26, 331, 264, 10);
            SetBorderControlDlu(dialog, IDC_HOVER_OPACITY_LABEL, 10, 350, 78, 12);
            SetBorderControlDlu(dialog, IDC_HOVER_OPACITY, 88, 345, 160, 20);
            SetBorderControlDlu(dialog, IDC_HOVER_OPACITY_VALUE, 252, 350, 36, 12);
            SetBorderControlDlu(dialog, IDC_HOVER_EXPAND_LABEL, 10, 372, 78, 12);
            SetBorderControlDlu(dialog, IDC_HOVER_EXPAND, 88, 367, 130, 20);
            SetBorderControlDlu(dialog, IDC_HOVER_EXPAND_VALUE, 222, 372, 30, 12);
            SetBorderControlDlu(dialog, IDC_HOVER_EXPAND_RESET, 258, 369, 32, 14);
            SetBorderControlDlu(dialog, IDC_BORDER_RESET, 10, 410, 68, 15);
            SetBorderControlDlu(dialog, IDOK, 166, 410, 58, 15);
            SetBorderControlDlu(dialog, IDCANCEL, 232, 410, 58, 15);
        } else {
            SetBorderControlDlu(dialog, IDC_BORDER_THICKNESS_LABEL, 10, 47, 40, 12);
            SetBorderControlDlu(dialog, IDC_BORDER_THICKNESS, 52, 43, 200, 20);
            SetBorderControlDlu(dialog, IDC_BORDER_THICKNESS_VALUE, 258, 47, 32, 12);
            SetBorderControlDlu(dialog, IDC_MODEL_OPACITY_LABEL, 10, 72, 78, 12);
            SetBorderControlDlu(dialog, IDC_MODEL_OPACITY, 88, 67, 160, 20);
            SetBorderControlDlu(dialog, IDC_MODEL_OPACITY_VALUE, 252, 72, 36, 12);
            SetBorderControlDlu(dialog, IDC_HOVER_FADE_ENABLED, 10, 94, 280, 14);
            SetBorderControlDlu(dialog, IDC_HOVER_HINT, 26, 108, 264, 10);
            SetBorderControlDlu(dialog, IDC_HOVER_OPACITY_LABEL, 10, 127, 78, 12);
            SetBorderControlDlu(dialog, IDC_HOVER_OPACITY, 88, 122, 160, 20);
            SetBorderControlDlu(dialog, IDC_HOVER_OPACITY_VALUE, 252, 127, 36, 12);
            SetBorderControlDlu(dialog, IDC_HOVER_EXPAND_LABEL, 10, 149, 78, 12);
            SetBorderControlDlu(dialog, IDC_HOVER_EXPAND, 88, 144, 130, 20);
            SetBorderControlDlu(dialog, IDC_HOVER_EXPAND_VALUE, 222, 149, 30, 12);
            SetBorderControlDlu(dialog, IDC_HOVER_EXPAND_RESET, 258, 146, 32, 14);
            SetBorderControlDlu(dialog, IDC_BORDER_RESET, 10, 187, 68, 15);
            SetBorderControlDlu(dialog, IDOK, 166, 187, 58, 15);
            SetBorderControlDlu(dialog, IDCANCEL, 232, 187, 58, 15);
        }

        RECT client{};
        RECT window{};
        GetClientRect(dialog, &client);
        GetWindowRect(dialog, &window);
        RECT desired{ 0, 0, 300, custom ? 430 : 215 };
        MapDialogRect(dialog, &desired);
        const int nonClientHeight =
            (window.bottom - window.top) - (client.bottom - client.top);
        const int desiredHeight = desired.bottom + nonClientHeight;
        int y = window.top;
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(
                MonitorFromWindow(dialog, MONITOR_DEFAULTTONEAREST), &info)) {
            y = (std::min)(
                y, static_cast<int>(info.rcWork.bottom) - desiredHeight);
            y = (std::max)(y, static_cast<int>(info.rcWork.top));
        }
        SetWindowPos(
            dialog, HWND_TOPMOST, window.left, y,
            window.right - window.left, desiredHeight,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    static void RenderBorderDialogChange(HWND dialog, BorderDialogState* state) {
        InvalidateRect(GetDlgItem(dialog, IDC_BORDER_COLOR_WHEEL), nullptr, FALSE);
    }

    static void EnsureColorWheelBitmap(
        BorderDialogState* state, int width, int height) {
        if (state->wheelBitmapWidth == width &&
            state->wheelBitmapHeight == height &&
            state->wheelBitmapValuePercent == state->colorValuePercent &&
            !state->wheelPixels.empty()) {
            return;
        }
        state->wheelBitmapWidth = width;
        state->wheelBitmapHeight = height;
        state->wheelBitmapValuePercent = state->colorValuePercent;
        // Keep the square around the circular wheel consistent with the
        // panel instead of using the system's white 3-D face color.
        constexpr std::uint32_t kPanelBackgroundBgra =
            39u | (24u << 8) | (14u << 16); // RGB(14, 24, 39)
        state->wheelPixels.assign(
            static_cast<size_t>(width) * height,
            kPanelBackgroundBgra);
        const double centerX = width / 2.0;
        const double centerY = height / 2.0;
        const double radius = (std::max)(
            1.0, (std::min)(centerX, centerY) - 5.0);
        constexpr double kPi = 3.14159265358979323846;
        for (int y = 0; y < height; ++y) {
            auto* row = state->wheelPixels.data() + static_cast<size_t>(y) * width;
            for (int x = 0; x < width; ++x) {
                const double dx = x + 0.5 - centerX;
                const double dy = y + 0.5 - centerY;
                const double distance = std::sqrt(dx * dx + dy * dy);
                if (distance > radius) {
                    continue;
                }
                double hue = std::atan2(dy, dx) / (2.0 * kPi);
                if (hue < 0.0) hue += 1.0;
                const RainbowColor color = HsvWheelColor(
                    hue, distance / radius, state->colorValuePercent / 100.0);
                row[x] = static_cast<std::uint32_t>(color.blue) |
                    (static_cast<std::uint32_t>(color.green) << 8) |
                    (static_cast<std::uint32_t>(color.red) << 16);
            }
        }
    }

    static void UpdateColorFromWheel(
        HWND wheel, BorderDialogState* state, int mouseX, int mouseY) {
        RECT client{};
        GetClientRect(wheel, &client);
        const double centerX = (client.right - client.left) / 2.0;
        const double centerY = (client.bottom - client.top) / 2.0;
        const double radius = (std::max)(
            1.0, (std::min)(centerX, centerY) - 5.0);
        double dx = mouseX - centerX;
        double dy = mouseY - centerY;
        double distance = std::sqrt(dx * dx + dy * dy);
        if (distance > radius) {
            dx *= radius / distance;
            dy *= radius / distance;
            distance = radius;
        }
        constexpr double kPi = 3.14159265358979323846;
        double hue = std::atan2(dy, dx) / (2.0 * kPi);
        if (hue < 0.0) hue += 1.0;
        state->colorHue = hue;
        state->colorSaturation = distance / radius;
        const RainbowColor color = HsvWheelColor(
            state->colorHue,
            state->colorSaturation,
            state->colorValuePercent / 100.0);
        state->overlay->customBorderColor_ = RGB(color.red, color.green, color.blue);
        HWND dialog = GetParent(wheel);
        UpdateCustomColorText(dialog, state);
        RenderBorderDialogChange(dialog, state);
    }

    static LRESULT CALLBACK ColorWheelSubclassProc(
        HWND wheel, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR, DWORD_PTR reference) {
        auto* state = reinterpret_cast<BorderDialogState*>(reference);
        switch (message) {
        case WM_LBUTTONDOWN:
            SetCapture(wheel);
            UpdateColorFromWheel(
                wheel, state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MOUSEMOVE:
            if (wParam & MK_LBUTTON) {
                UpdateColorFromWheel(
                    wheel, state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            UpdateColorFromWheel(
                wheel, state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (GetCapture() == wheel) ReleaseCapture();
            return 0;
        default:
            break;
        }
        return DefSubclassProc(wheel, message, wParam, lParam);
    }

    static void UpdateColorFromBrightness(
        HWND dialog, BorderDialogState* state, int valuePercent) {
        state->colorValuePercent = (std::clamp)(valuePercent, 0, 100);
        const RainbowColor color = HsvWheelColor(
            state->colorHue,
            state->colorSaturation,
            state->colorValuePercent / 100.0);
        state->overlay->customBorderColor_ = RGB(color.red, color.green, color.blue);
        UpdateCustomColorText(dialog, state);
        RenderBorderDialogChange(dialog, state);
    }

    static int PersonalPanelHeight(const LayeredOverlay* overlay) {
        return overlay && overlay->borderMode_ == kBorderModeCustom ? 700 : 520;
    }

    static void PanelText(
        HDC dc, const std::wstring& text, RECT rect, COLORREF color,
        int pixelHeight = 15, bool bold = false, UINT flags = DT_LEFT | DT_VCENTER) {
        HFONT font = CreateFontW(
            -pixelHeight, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE,
            FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HGDIOBJ old = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        DrawTextW(dc, text.c_str(), -1, &rect, flags);
        SelectObject(dc, old);
        DeleteObject(font);
    }

    static void PanelFill(HDC dc, RECT rect, COLORREF color) {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
    }

    static void RefreshPersonalPanel(HWND panel) {
        if (!panel || !IsWindow(panel)) return;
        // InvalidateRect alone may defer WM_PAINT until the drag message
        // finishes. Force the tiny self-drawn panel to repaint now so the
        // thumb visibly follows the cursor on every mouse-move message.
        RedrawWindow(panel, nullptr, nullptr,
            RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
    }

    static void PanelSlider(
        HDC dc, RECT rect, int value, int minimum, int maximum, bool active) {
        const int width = rect.right - rect.left;
        const int clamped = (std::clamp)(value, minimum, maximum);
        const int knobX = rect.left + (maximum > minimum
            ? (clamped - minimum) * (width - 12) / (maximum - minimum)
            : 0) + 6;
        PanelFill(dc, rect, RGB(31, 48, 71));
        RECT filled = rect;
        filled.right = knobX;
        PanelFill(dc, filled, active ? RGB(60, 145, 232) : RGB(43, 105, 169));
        HBRUSH brush = CreateSolidBrush(active ? RGB(238, 247, 255) : RGB(191, 218, 246));
        RECT knob{ knobX - 6, rect.top - 4, knobX + 7, rect.bottom + 4 };
        FillRect(dc, &knob, brush);
        DeleteObject(brush);
    }

    static RECT PersonalRect(int left, int top, int right, int bottom) {
        return RECT{ left, top, right, bottom };
    }

    static RECT PersonalWheelRect(const LayeredOverlay* overlay) {
        return overlay && overlay->borderMode_ == kBorderModeCustom
            ? PersonalRect(20, 102, 220, 302)
            : PersonalRect(0, 0, 0, 0);
    }

    static void DrawPersonalPanel(HWND panel, BorderDialogState* state) {
        if (!state || !state->overlay) return;
        LayeredOverlay* overlay = state->overlay;
        PAINTSTRUCT paint{};
        HDC target = BeginPaint(panel, &paint);
        RECT client{};
        GetClientRect(panel, &client);
        HDC dc = CreateCompatibleDC(target);
        HBITMAP backBuffer = CreateCompatibleBitmap(
            target, (std::max)(1L, client.right), (std::max)(1L, client.bottom));
        HGDIOBJ oldBitmap = SelectObject(dc, backBuffer);
        PanelFill(dc, client, RGB(14, 24, 39));

        RECT header{ 0, 0, client.right, 44 };
        PanelFill(dc, header, RGB(28, 48, 75));
        PanelText(dc, L"个性化", RECT{ 16, 0, 180, 44 }, RGB(240, 246, 255), 18, true,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        PanelText(dc, L"×", RECT{ client.right - 44, 0, client.right - 8, 44 },
            RGB(240, 246, 255), 25, false, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        PanelText(dc, L"边框模式", RECT{ 18, 53, 100, 83 }, RGB(166, 198, 232), 14, true);
        const int modeLeft = 105;
        const int modeWidth = (client.right - modeLeft - 16) / 3;
        const wchar_t* modeLabels[] = { L"自定义", L"跑马灯渐变", L"普通渐变" };
        for (int i = 0; i < 3; ++i) {
            RECT button{ modeLeft + i * modeWidth, 52,
                modeLeft + (i + 1) * modeWidth - 4, 84 };
            PanelFill(dc, button, overlay->borderMode_ == i
                ? RGB(41, 105, 170) : RGB(24, 40, 62));
            PanelText(dc, modeLabels[i], button, RGB(240, 246, 255), 13, true,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        int y = 0;
        if (overlay->borderMode_ == kBorderModeCustom) {
            const RECT wheel = PersonalWheelRect(overlay);
            EnsureColorWheelBitmap(state, wheel.right - wheel.left, wheel.bottom - wheel.top);
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = wheel.right - wheel.left;
            info.bmiHeader.biHeight = -(wheel.bottom - wheel.top);
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            StretchDIBits(dc, wheel.left, wheel.top, wheel.right - wheel.left,
                wheel.bottom - wheel.top, 0, 0, wheel.right - wheel.left,
                wheel.bottom - wheel.top, state->wheelPixels.data(), &info,
                DIB_RGB_COLORS, SRCCOPY);
            double hue = 0.0;
            double saturation = 0.0;
            CustomColorHueSaturation(overlay->customBorderColor_, hue, saturation);
            constexpr double pi = 3.14159265358979323846;
            const double angle = hue * 2.0 * pi;
            const double radius = 94.0;
            const int markerX = static_cast<int>(wheel.left + 100 + std::cos(angle) * saturation * radius);
            const int markerY = static_cast<int>(wheel.top + 100 + std::sin(angle) * saturation * radius);
            HPEN black = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
            HPEN white = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
            HGDIOBJ oldPen = SelectObject(dc, black);
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Ellipse(dc, markerX - 7, markerY - 7, markerX + 8, markerY + 8);
            SelectObject(dc, white);
            Ellipse(dc, markerX - 5, markerY - 5, markerX + 6, markerY + 6);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(black);
            DeleteObject(white);

            const COLORREF color = overlay->customBorderColor_;
            PanelText(dc, L"自定义颜色", RECT{ 245, 105, 405, 135 },
                RGB(166, 198, 232), 14, true);
            RECT swatch{ 245, 145, 405, 178 };
            PanelFill(dc, swatch, color);
            wchar_t colorText[96]{};
            swprintf_s(colorText, L"#%02X%02X%02X  RGB(%u,%u,%u)",
                GetRValue(color), GetGValue(color), GetBValue(color),
                GetRValue(color), GetGValue(color), GetBValue(color));
            PanelText(dc, colorText, RECT{ 245, 182, 410, 205 },
                RGB(220, 232, 248), 12, false);
            PanelText(dc, L"亮度", RECT{ 245, 220, 300, 243 }, RGB(166, 198, 232), 13, true);
            PanelSlider(dc, RECT{ 245, 250, 405, 260 }, state->colorValuePercent, 0, 100,
                state->activeHit == 4);
            y = 330;
        } else {
            PanelText(dc, L"当前模式不需要色盘", RECT{ 22, 112, 390, 144 },
                RGB(126, 157, 193), 14, false);
            y = 145;
        }

        PanelText(dc, L"边框粗细", RECT{ 20, y, 125, y + 28 }, RGB(166, 198, 232), 13, true);
        PanelSlider(dc, RECT{ 110, y + 10, 355, y + 20 }, overlay->borderThickness_, 1,
            kMaximumBorderThickness,
            state->activeHit == 5);
        PanelText(dc, std::to_wstring(overlay->borderThickness_), RECT{ 365, y, 410, y + 28 },
            RGB(240, 246, 255), 13, true, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        y += 45;
        PanelText(dc, L"模型不透明度", RECT{ 20, y, 145, y + 28 }, RGB(166, 198, 232), 13, true);
        PanelSlider(dc, RECT{ 130, y + 10, 355, y + 20 }, overlay->modelOpacityPercent_, 10, 100,
            state->activeHit == 6);
        PanelText(dc, std::to_wstring(overlay->modelOpacityPercent_) + L"%", RECT{ 365, y, 410, y + 28 },
            RGB(240, 246, 255), 13, true, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        y += 42;
        RECT check{ 20, y + 3, 38, y + 21 };
        PanelFill(dc, check, overlay->hoverFadeEnabled_ ? RGB(55, 139, 221) : RGB(41, 58, 80));
        if (overlay->hoverFadeEnabled_) {
            PanelText(dc, L"✓", check, RGB(255, 255, 255), 14, true, DT_CENTER | DT_VCENTER);
        }
        PanelText(dc, L"鼠标经过模型时将模型透明化", RECT{ 48, y, 350, y + 28 },
            RGB(220, 232, 248), 13, false);

        y += 38;
        PanelText(dc, L"悬停不透明度", RECT{ 20, y, 145, y + 28 }, RGB(166, 198, 232), 13, true);
        PanelSlider(dc, RECT{ 130, y + 10, 355, y + 20 }, overlay->hoverOpacityPercent_, 0, 100,
            state->activeHit == 7);
        PanelText(dc, std::to_wstring(overlay->hoverOpacityPercent_) + L"%", RECT{ 365, y, 410, y + 28 },
            RGB(240, 246, 255), 13, true, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        y += 38;
        PanelText(dc, L"悬停扩展", RECT{ 20, y, 125, y + 28 }, RGB(166, 198, 232), 13, true);
        PanelSlider(dc, RECT{ 130, y + 10, 330, y + 20 }, overlay->hoverExpandPx_, -500, 500,
            state->activeHit == 8);
        PanelText(dc, std::to_wstring(overlay->hoverExpandPx_) + L"px", RECT{ 340, y, 410, y + 28 },
            RGB(240, 246, 255), 13, true, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        y += 42;
        RECT expressionCheck{ 20, y + 3, 38, y + 21 };
        PanelFill(dc, expressionCheck,
            overlay->hoverExpressionEnabled_ ? RGB(55, 139, 221) : RGB(41, 58, 80));
        if (overlay->hoverExpressionEnabled_) {
            PanelText(dc, L"✓", expressionCheck, RGB(255, 255, 255), 14, true,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        PanelText(dc, L"鼠标悬停时触发表情（4秒后恢复）",
            RECT{ 48, y, 390, y + 28 }, RGB(220, 232, 248), 13, false,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        y += 34;
        RECT expressionButton{ 20, y, 258, y + 32 };
        PanelFill(dc, expressionButton, RGB(26, 46, 71));
        std::wstring expressionLabel = L"选择表情";
        if (!overlay->hoverExpressionFile_.empty()) {
            expressionLabel = L"表情：" + Utf8ToWide(overlay->hoverExpressionFile_);
        }
        PanelText(dc, expressionLabel, expressionButton, RGB(240, 246, 255), 12, true,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT previewButton{ 266, y, 405, y + 32 };
        PanelFill(dc, previewButton, RGB(32, 91, 151));
        PanelText(dc, L"预览 4 秒", previewButton, RGB(240, 246, 255), 12, true,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        const int footerY = client.bottom - 48;
        PanelFill(dc, RECT{ 16, footerY - 8, client.right - 16, footerY - 7 }, RGB(38, 58, 82));
        const int buttonW = (client.right - 48) / 3;
        const wchar_t* footer[] = { L"恢复默认", L"取消", L"完成" };
        for (int i = 0; i < 3; ++i) {
            RECT button{ 16 + i * (buttonW + 8), footerY,
                16 + i * (buttonW + 8) + buttonW, footerY + 32 };
            PanelFill(dc, button, i == 2 ? RGB(32, 91, 151) : RGB(26, 46, 71));
            PanelText(dc, footer[i], button, RGB(240, 246, 255), 13, true,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(backBuffer);
        DeleteDC(dc);
        EndPaint(panel, &paint);
    }

    static int PersonalPanelHitTest(const LayeredOverlay* overlay, int x, int y) {
        if (!overlay) return 0;
        if (x >= 370 && x < 420 && y < 44) return 1; // close
        const int modeLeft = 105;
        const int modeWidth = (420 - modeLeft - 16) / 3;
        if (y >= 52 && y < 84 && x >= modeLeft && x < 420 - 16) {
            return 10 + (x - modeLeft) / modeWidth;
        }
        int base = overlay->borderMode_ == kBorderModeCustom ? 330 : 145;
        if (y >= base && y < base + 30) return 5;
        if (y >= base + 45 && y < base + 75) return 6;
        if (y >= base + 87 && y < base + 120 && x < 350) return 9;
        if (y >= base + 125 && y < base + 160) return 7;
        if (y >= base + 163 && y < base + 200) return 8;
        if (y >= base + 200 && y < base + 235 && x < 395) return 13;
        if (y >= base + 235 && y < base + 280) {
            return x < 262 ? 14 : 15;
        }
        if (overlay->borderMode_ == kBorderModeCustom) {
            const RECT wheel = PersonalWheelRect(overlay);
            if (x >= wheel.left && x < wheel.right && y >= wheel.top && y < wheel.bottom) return 3;
            if (x >= 235 && x < 415 && y >= 210 && y < 275) return 4;
        }
        const int footerY = PersonalPanelHeight(overlay) - 48;
        if (y >= footerY && y < footerY + 40) {
            const int buttonW = (420 - 48) / 3;
            if (x >= 16 && x < 16 + buttonW) return 20;
            if (x >= 24 + buttonW && x < 24 + 2 * buttonW) return 21;
            if (x >= 32 + 2 * buttonW && x < 32 + 3 * buttonW) return 22;
        }
        return 0;
    }

    static void UpdatePersonalSlider(
        HWND panel, BorderDialogState* state, int hit, int x) {
        if (!state || !state->overlay) return;
        LayeredOverlay* overlay = state->overlay;
        const int left = hit == 4 ? 245 : (hit == 5 ? 110 : 130);
        const int right = hit == 4 ? 405 : (hit == 8 ? 330 : 355);
        const int clampedX = (std::clamp)(x, left, right);
        const double ratio = (clampedX - left) / static_cast<double>((std::max)(1, right - left));
        if (hit == 4) {
            state->colorValuePercent = static_cast<int>(std::lround(ratio * 100.0));
            const RainbowColor color = HsvWheelColor(state->colorHue, state->colorSaturation,
                state->colorValuePercent / 100.0);
            overlay->customBorderColor_ = RGB(color.red, color.green, color.blue);
        } else if (hit == 5) {
            overlay->borderThickness_ = (std::clamp)(
                1 + static_cast<int>(std::lround(
                    ratio * (kMaximumBorderThickness - 1))),
                1, kMaximumBorderThickness);
        } else if (hit == 6) {
            overlay->modelOpacityPercent_ = (std::clamp)(10 + static_cast<int>(std::lround(ratio * 90.0)), 10, 100);
            // Model opacity is the normal opacity target. Clear a pending
            // hover-preview timer so this slider updates the model itself.
            overlay->hoverOpacityPreviewActive_ = false;
            KillTimer(panel, 91);
        } else if (hit == 7) {
            overlay->hoverOpacityPercent_ = (std::clamp)(static_cast<int>(std::lround(ratio * 100.0)), 0, 100);
            overlay->hoverOpacityPreviewActive_ = true;
            SetTimer(panel, 91, 1000, nullptr);
        } else if (hit == 8) {
            overlay->hoverExpandPx_ = (std::clamp)(-500 + static_cast<int>(std::lround(ratio * 1000.0)), -500, 500);
            // Keep the band visible while the slider is being dragged so the
            // configured hit range can be inspected and updated in real time.
            overlay->showHoverExpandPreview_ = true;
            overlay->hoverExpandPreviewUntil_ = Clock::time_point::max();
        }
        RefreshPersonalPanel(panel);
        ++overlay->requested_;
        overlay->RenderFrame();
        RefreshPersonalPanel(panel);
    }

    static void UpdatePersonalWheel(HWND panel, BorderDialogState* state, int x, int y) {
        if (!state || !state->overlay) return;
        const RECT wheel = PersonalWheelRect(state->overlay);
        const double centerX = (wheel.left + wheel.right) / 2.0;
        const double centerY = (wheel.top + wheel.bottom) / 2.0;
        const double radius = (std::min)(wheel.right - wheel.left, wheel.bottom - wheel.top) / 2.0 - 6.0;
        double dx = x - centerX;
        double dy = y - centerY;
        double distance = std::sqrt(dx * dx + dy * dy);
        if (distance > radius) {
            dx *= radius / distance;
            dy *= radius / distance;
            distance = radius;
        }
        constexpr double pi = 3.14159265358979323846;
        double hue = std::atan2(dy, dx) / (2.0 * pi);
        if (hue < 0.0) hue += 1.0;
        state->colorHue = hue;
        state->colorSaturation = distance / radius;
        const RainbowColor color = HsvWheelColor(hue, state->colorSaturation,
            state->colorValuePercent / 100.0);
        state->overlay->customBorderColor_ = RGB(color.red, color.green, color.blue);
        RefreshPersonalPanel(panel);
        ++state->overlay->requested_;
        state->overlay->RenderFrame();
        RefreshPersonalPanel(panel);
    }

    static void ResetPersonalPanel(HWND panel, BorderDialogState* state) {
        if (!state || !state->overlay) return;
        LayeredOverlay* overlay = state->overlay;
        overlay->borderMode_ = kBorderModeNormal;
        overlay->customBorderColor_ = kDefaultCustomBorderColor;
        CustomColorHueSaturation(overlay->customBorderColor_, state->colorHue, state->colorSaturation);
        state->colorValuePercent = CustomColorValuePercent(overlay->customBorderColor_);
        overlay->borderThickness_ = kDefaultBorderThickness;
        overlay->hoverFadeEnabled_ = true;
        overlay->hoverOpacityPercent_ = kDefaultHoverOpacityPercent;
        overlay->modelOpacityPercent_ = 100;
        overlay->hoverExpandPx_ = 0;
        overlay->CancelHoverExpression();
        overlay->hoverExpressionEnabled_ = false;
        overlay->hoverExpressionFile_.clear();
        overlay->RestorePanelExpressionPreview();
        overlay->hoverOpacityPreviewActive_ = true;
        SetTimer(panel, 91, 1000, nullptr);
        state->activeHit = 0;
        overlay->PositionBorderPanel();
        ++overlay->requested_;
        overlay->RenderFrame();
        RefreshPersonalPanel(panel);
    }

    static void RestorePersonalPanel(HWND panel, BorderDialogState* state) {
        if (!state || !state->overlay) return;
        LayeredOverlay* overlay = state->overlay;
        overlay->borderMode_ = state->originalMode;
        overlay->customBorderColor_ = state->originalColor;
        overlay->borderThickness_ = state->originalThickness;
        overlay->hoverFadeEnabled_ = state->originalHoverFadeEnabled;
        overlay->hoverOpacityPercent_ = state->originalHoverOpacityPercent;
        overlay->modelOpacityPercent_ = state->originalModelOpacityPercent;
        overlay->hoverExpandPx_ = state->originalHoverExpandPx;
        overlay->hoverExpressionEnabled_ = state->originalHoverExpressionEnabled;
        overlay->hoverExpressionFile_ = state->originalHoverExpressionFile;
        overlay->RestorePanelExpressionPreview();
        overlay->hoverOpacityPreviewActive_ = false;
        ++overlay->requested_;
        overlay->RenderFrame();
        Log("[toolbar] border_settings_cancel");
        DestroyWindow(panel);
    }

    static void ShowExpressionMenu(HWND panel, BorderDialogState* state) {
        if (!state || !state->overlay) return;
        LayeredOverlay* overlay = state->overlay;
        const auto expressions = overlay->vtsApi_.GetExpressions();
        HMENU menu = CreatePopupMenu();
        if (!menu) return;
        constexpr UINT kExpressionBase = 6200;
        if (expressions.empty()) {
            AppendMenuW(menu, MF_STRING | MF_GRAYED, kExpressionBase,
                L"暂无可用表情（请确认 VTS API 已连接）");
            overlay->vtsApi_.RequestExpressionState();
        } else {
            for (size_t i = 0; i < expressions.size(); ++i) {
                const auto& expression = expressions[i];
                std::wstring label = Utf8ToWide(
                    expression.name.empty() ? expression.file : expression.name);
                if (label.empty()) label = L"未命名表情";
                if (expression.file == overlay->hoverExpressionFile_) {
                    label += L"  ✓";
                }
                AppendMenuW(menu,
                    MF_STRING | (expression.file == overlay->hoverExpressionFile_
                        ? MF_CHECKED : 0),
                    kExpressionBase + static_cast<UINT>(i), label.c_str());
            }
        }
        RECT button{ 20, 0, 258, 0 };
        const int base = overlay->borderMode_ == kBorderModeCustom ? 330 : 145;
        button.top = base + 239;
        button.bottom = button.top + 32;
        POINT popup{ button.left, button.bottom };
        ClientToScreen(panel, &popup);
        const UINT command = overlay->RunModalWhileRendering([&]() {
            return TrackPopupMenu(menu,
                TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,
                popup.x, popup.y, 0, panel, nullptr);
        });
        DestroyMenu(menu);
        if (command < kExpressionBase ||
            command >= kExpressionBase + expressions.size()) {
            return;
        }
        overlay->hoverExpressionFile_ = expressions[command - kExpressionBase].file;
        if (overlay->PreviewExpressionFromPanel()) {
            SetTimer(panel, 94, 4000, nullptr);
        }
        RefreshPersonalPanel(panel);
    }

    static LRESULT CALLBACK PersonalPanelProc(
        HWND panel, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* state = reinterpret_cast<BorderDialogState*>(GetWindowLongPtrW(panel, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            state = reinterpret_cast<BorderDialogState*>(create->lpCreateParams);
            SetWindowLongPtrW(panel, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        }
        if (!state || !state->overlay) return DefWindowProcW(panel, message, wParam, lParam);
        LayeredOverlay* overlay = state->overlay;
        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            DrawPersonalPanel(panel, state);
            return 0;
        case WM_TIMER:
            if (wParam == 91) {
                KillTimer(panel, 91);
                overlay->hoverOpacityPreviewActive_ = false;
                ++overlay->requested_;
                overlay->RenderFrame();
                return 0;
            }
            if (wParam == 92) {
                KillTimer(panel, 92);
                if (!overlay->hoverExpandEditing_ && !overlay->IsCursorOverModel()) {
                    overlay->showHoverExpandPreview_ = false;
                    overlay->hoverExpandPreviewUntil_ = Clock::time_point{};
                }
                return 0;
            }
            if (wParam == 93) {
                RefreshPersonalPanel(panel);
                return 0;
            }
            if (wParam == 94) {
                KillTimer(panel, 94);
                overlay->RestorePanelExpressionPreview();
                return 0;
            }
            break;
        case WM_LBUTTONDOWN: {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            const int hit = PersonalPanelHitTest(overlay, x, y);
            if (hit == 1) {
                RestorePersonalPanel(panel, state);
                return 0;
            }
            if (hit >= 10 && hit <= 12) {
                overlay->borderMode_ = hit - 10;
                state->activeHit = 0;
                overlay->PositionBorderPanel();
                ++overlay->requested_;
                overlay->RenderFrame();
                RefreshPersonalPanel(panel);
                return 0;
            }
            if (hit == 3) {
                state->activeHit = hit;
                SetCapture(panel);
                UpdatePersonalWheel(panel, state, x, y);
                return 0;
            }
            if (hit >= 4 && hit <= 8) {
                state->activeHit = hit;
                SetCapture(panel);
                if (hit == 8) {
                    overlay->hoverExpandEditing_ = true;
                    overlay->showHoverExpandPreview_ = true;
                    overlay->hoverExpandPreviewUntil_ = Clock::time_point::max();
                }
                UpdatePersonalSlider(panel, state, hit, x);
                return 0;
            }
            if (hit == 9) {
                overlay->hoverFadeEnabled_ = !overlay->hoverFadeEnabled_;
                // Toggling this option must clear any previous hover target.
                // When disabled, restore the configured model opacity
                // immediately instead of leaving the old faded alpha active.
                overlay->hoverOpacityPreviewActive_ = false;
                KillTimer(panel, 91);
                if (!overlay->hoverFadeEnabled_) {
                    overlay->hoverExpandEditing_ = false;
                    overlay->showHoverExpandPreview_ = false;
                    overlay->hoverExpandPreviewUntil_ = Clock::time_point{};
                    overlay->hoverExpandPreviewAlpha_ = 0.0;
                }
                const int baseAlpha = overlay->modelOpacityPercent_ * 255 / 100;
                overlay->currentOverlayAlpha_ = baseAlpha;
                overlay->hoverFadeStartAlpha_ = baseAlpha;
                overlay->hoverTargetAlpha_ = baseAlpha;
                overlay->hoverFadeStarted_ = Clock::now();
                ++overlay->requested_;
                overlay->RenderFrame();
                RefreshPersonalPanel(panel);
                return 0;
            }
            if (hit == 13) {
                overlay->hoverExpressionEnabled_ = !overlay->hoverExpressionEnabled_;
                if (!overlay->hoverExpressionEnabled_) {
                    overlay->CancelHoverExpression();
                } else {
                    overlay->vtsApi_.RequestExpressionState();
                }
                ++overlay->requested_;
                overlay->RenderFrame();
                RefreshPersonalPanel(panel);
                return 0;
            }
            if (hit == 14) {
                ShowExpressionMenu(panel, state);
                return 0;
            }
            if (hit == 15) {
                if (overlay->PreviewExpressionFromPanel()) {
                    SetTimer(panel, 94, 4000, nullptr);
                }
                return 0;
            }
            if (hit == 20) {
                ResetPersonalPanel(panel, state);
                return 0;
            }
            if (hit == 21) {
                RestorePersonalPanel(panel, state);
                return 0;
            }
            if (hit == 22) {
                overlay->EndHoverOpacityPreview();
                KillTimer(panel, 94);
                overlay->RestorePanelExpressionPreview();
                overlay->SaveUiSettings();
                Log("[toolbar] border_settings_saved mode=" + std::to_string(overlay->borderMode_) +
                    " thickness=" + std::to_string(overlay->borderThickness_));
                DestroyWindow(panel);
                return 0;
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            if (state->activeHit == 3 && GetCapture() == panel) {
                POINT cursor{};
                GetCursorPos(&cursor);
                ScreenToClient(panel, &cursor);
                UpdatePersonalWheel(panel, state, cursor.x, cursor.y);
                return 0;
            }
            if (state->activeHit >= 4 && state->activeHit <= 8 && GetCapture() == panel) {
                POINT cursor{};
                GetCursorPos(&cursor);
                ScreenToClient(panel, &cursor);
                UpdatePersonalSlider(panel, state, state->activeHit, cursor.x);
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (state->activeHit != 0) {
                POINT cursor{};
                GetCursorPos(&cursor);
                ScreenToClient(panel, &cursor);
                const int x = cursor.x;
                const int y = cursor.y;
                if (state->activeHit == 3) {
                    UpdatePersonalWheel(panel, state, x, y);
                } else if (state->activeHit >= 4 && state->activeHit <= 8) {
                    UpdatePersonalSlider(panel, state, state->activeHit, x);
                }
                if (state->activeHit == 8) {
                    overlay->hoverExpandEditing_ = false;
                    overlay->showHoverExpandPreview_ = false;
                    overlay->hoverExpandPreviewUntil_ = Clock::time_point{};
                }
                state->activeHit = 0;
                if (GetCapture() == panel) ReleaseCapture();
                if (overlay->borderDialogOpen_) {
                    ++overlay->requested_;
                    overlay->RenderFrame();
                }
                RefreshPersonalPanel(panel);
                return 0;
            }
            break;
        case WM_CAPTURECHANGED:
            if (state->activeHit == 8) {
                overlay->hoverExpandEditing_ = false;
                overlay->showHoverExpandPreview_ = false;
                overlay->hoverExpandPreviewUntil_ = Clock::time_point{};
            }
            state->activeHit = 0;
            RefreshPersonalPanel(panel);
            break;
        case WM_COMMAND:
            // SetLocked() sends IDOK before click-through is enabled so the
            // in-app panel commits and closes without reopening a modal dialog.
            if (LOWORD(wParam) == IDOK) {
                overlay->EndHoverOpacityPreview();
                overlay->SaveUiSettings();
                Log("[toolbar] border_settings_saved mode=" + std::to_string(overlay->borderMode_) +
                    " thickness=" + std::to_string(overlay->borderThickness_));
                DestroyWindow(panel);
                return 0;
            }
            break;
        case WM_CLOSE:
            RestorePersonalPanel(panel, state);
            return 0;
        case WM_DESTROY:
            KillTimer(panel, 1);
            KillTimer(panel, 91);
            KillTimer(panel, 92);
            KillTimer(panel, 93);
            KillTimer(panel, 94);
            if (GetCapture() == panel) ReleaseCapture();
            overlay->RestorePanelExpressionPreview();
            overlay->borderDialogOpen_ = false;
            overlay->hoverOpacityPreviewActive_ = false;
            overlay->showHoverExpandPreview_ = false;
            overlay->hoverExpandEditing_ = false;
            overlay->hoverExpandPreviewAlpha_ = 0.0;
            overlay->borderPanelHwnd_ = nullptr;
            delete state;
            return 0;
        default:
            break;
        }
        return DefWindowProcW(panel, message, wParam, lParam);
    }

    void ShowBorderSettingsDialog() {
        if (borderPanelHwnd_ && IsWindow(borderPanelHwnd_)) {
            DestroyWindow(borderPanelHwnd_);
            borderPanelHwnd_ = nullptr;
            return;
        }
        auto* state = new BorderDialogState{};
        state->overlay = this;
        state->originalMode = borderMode_;
        state->originalColor = customBorderColor_;
        state->originalThickness = borderThickness_;
        state->originalHoverFadeEnabled = hoverFadeEnabled_;
        state->originalHoverOpacityPercent = hoverOpacityPercent_;
        state->originalModelOpacityPercent = modelOpacityPercent_;
        state->originalHoverExpandPx = hoverExpandPx_;
        state->originalHoverExpressionEnabled = hoverExpressionEnabled_;
        state->originalHoverExpressionFile = hoverExpressionFile_;
        CustomColorHueSaturation(customBorderColor_, state->colorHue, state->colorSaturation);
        state->colorValuePercent = CustomColorValuePercent(customBorderColor_);
        borderDialogOpen_ = true;
        Log("[toolbar] border_panel_open");

        static const wchar_t kPersonalPanelClass[] = L"LilyVtsPersonalizationPanel";
        static bool registered = false;
        if (!registered) {
            WNDCLASSW wc{};
            wc.lpfnWndProc = &LayeredOverlay::PersonalPanelProc;
            wc.hInstance = instance_;
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            wc.lpszClassName = kPersonalPanelClass;
            registered = RegisterClassW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        }
        borderPanelHwnd_ = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            kPersonalPanelClass, L"个性化", WS_POPUP,
            0, 0, 420, PersonalPanelHeight(this), toolbarHwnd_, nullptr, instance_, state);
        if (!borderPanelHwnd_) {
            Log("[toolbar] border_panel_failed error=" + std::to_string(GetLastError()));
            borderDialogOpen_ = false;
            delete state;
            return;
        }
        SetWindowPos(borderPanelHwnd_, HWND_TOPMOST, 0, 0, 420, PersonalPanelHeight(this),
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
        PositionBorderPanel();
        ShowWindow(borderPanelHwnd_, SW_SHOWNOACTIVATE);
        vtsApi_.RequestExpressionState();
        SetTimer(borderPanelHwnd_, 93, 500, nullptr);
        InvalidateRect(borderPanelHwnd_, nullptr, FALSE);
    }

    void PositionBorderPanel() {
        if (!borderPanelHwnd_ || !IsWindow(borderPanelHwnd_) || !hwnd_) return;
        RECT model{};
        GetWindowRect(hwnd_, &model);
        const int panelW = 420;
        const int panelH = PersonalPanelHeight(this);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
        const int screenMid = (mi.rcWork.left + mi.rcWork.right) / 2;
        const int modelMid = (model.left + model.right) / 2;
        int x = modelMid < screenMid ? model.right + 8 : model.left - panelW - 8;
        if (x + panelW > mi.rcWork.right) x = model.left - panelW - 8;
        if (x < mi.rcWork.left) x = model.right + 8;
        int y = model.bottom - panelH;
        y = (std::clamp)(y, static_cast<int>(mi.rcWork.top), static_cast<int>(mi.rcWork.bottom) - panelH);
        SetWindowPos(borderPanelHwnd_, HWND_TOPMOST, x, y, panelW, panelH,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    static void RestoreBorderDialogState(HWND dialog, BorderDialogState* state) {
        state->overlay->borderMode_ = state->originalMode;
        state->overlay->customBorderColor_ = state->originalColor;
        state->overlay->borderThickness_ = state->originalThickness;
        state->overlay->hoverFadeEnabled_ = state->originalHoverFadeEnabled;
        state->overlay->hoverOpacityPercent_ = state->originalHoverOpacityPercent;
        state->overlay->modelOpacityPercent_ = state->originalModelOpacityPercent;
        state->overlay->hoverExpandPx_ = state->originalHoverExpandPx;
        state->overlay->hoverOpacityPreviewActive_ = false;
        KillTimer(dialog, 1);
        ++state->overlay->requested_;
        state->overlay->RenderFrame();
    }

    static INT_PTR CALLBACK BorderDialogProc(
        HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* state = reinterpret_cast<BorderDialogState*>(
            GetWindowLongPtrW(dialog, DWLP_USER));
        if (message == WM_INITDIALOG) {
            state = reinterpret_cast<BorderDialogState*>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_MODE, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(L"自定义"));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_MODE, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(L"跑马灯渐变"));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_MODE, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(L"普通渐变"));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_MODE, CB_SETCURSEL,
                state->overlay->borderMode_, 0);
            SendDlgItemMessageW(
                dialog, IDC_BORDER_THICKNESS,
                TBM_SETRANGE, TRUE, MAKELPARAM(1, 10));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_THICKNESS,
                TBM_SETPOS, TRUE, state->overlay->borderThickness_);
            SetDlgItemInt(
                dialog, IDC_BORDER_THICKNESS_VALUE,
                state->overlay->borderThickness_, FALSE);
            CustomColorHueSaturation(
                state->overlay->customBorderColor_,
                state->colorHue, state->colorSaturation);
            state->colorValuePercent = CustomColorValuePercent(
                state->overlay->customBorderColor_);
            SendDlgItemMessageW(
                dialog, IDC_BORDER_COLOR_BRIGHTNESS,
                TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_COLOR_BRIGHTNESS,
                TBM_SETTICFREQ, 10, 0);
            SendDlgItemMessageW(
                dialog, IDC_BORDER_COLOR_BRIGHTNESS,
                TBM_SETPOS, TRUE, state->colorValuePercent);
            SendDlgItemMessageW(
                dialog, IDC_HOVER_OPACITY,
                TBM_SETRANGE, TRUE,
                MAKELPARAM(kMinimumHoverOpacityPercent, 100));
            SendDlgItemMessageW(
                dialog, IDC_HOVER_OPACITY,
                TBM_SETTICFREQ, 10, 0);
            SendDlgItemMessageW(
                dialog, IDC_HOVER_OPACITY,
                TBM_SETPOS, TRUE,
                state->overlay->hoverOpacityPercent_);
            // Model opacity: 10-100
            SendDlgItemMessageW(dialog, IDC_MODEL_OPACITY,
                TBM_SETRANGE, TRUE, MAKELPARAM(10, 100));
            SendDlgItemMessageW(dialog, IDC_MODEL_OPACITY,
                TBM_SETTICFREQ, 10, 0);
            SendDlgItemMessageW(dialog, IDC_MODEL_OPACITY,
                TBM_SETPOS, TRUE, state->overlay->modelOpacityPercent_);
            // Hover expand: -500 to 500
            SendDlgItemMessageW(dialog, IDC_HOVER_EXPAND,
                TBM_SETRANGE, TRUE, MAKELPARAM(-500, 500));
            SendDlgItemMessageW(dialog, IDC_HOVER_EXPAND,
                TBM_SETTICFREQ, 100, 0);
            SendDlgItemMessageW(dialog, IDC_HOVER_EXPAND,
                TBM_SETPOS, TRUE, state->overlay->hoverExpandPx_);
            SetDlgItemTextW(dialog, IDC_MODEL_OPACITY_VALUE,
                (std::to_wstring(state->overlay->modelOpacityPercent_) + L"%").c_str());
            SetDlgItemTextW(dialog, IDC_HOVER_EXPAND_VALUE,
                (std::to_wstring(state->overlay->hoverExpandPx_) + L"px").c_str());
            UpdateHoverOpacityControls(dialog, state);
            UpdateCustomColorText(dialog, state);
            SetWindowSubclass(
                GetDlgItem(dialog, IDC_BORDER_COLOR_WHEEL),
                &LayeredOverlay::ColorWheelSubclassProc,
                1, reinterpret_cast<DWORD_PTR>(state));

            // Apply dark theme (title bar via DWM, controls via WM_CTLCOLOR)
            ApplyDarkThemeToDialog(dialog);

            // Positioning handled by ShowBorderSettingsDialog / PositionBorderPanel.
            UpdateBorderDialogLayout(dialog, state);
            SetTimer(dialog, 1, 16, nullptr);
            return TRUE;
        }
        if (!state) return FALSE;

        // Dark theme colors (match main toolbar)
        static HBRUSH s_darkBrush = nullptr;
        if (!s_darkBrush) s_darkBrush = CreateSolidBrush(RGB(19, 29, 45));

        switch (message) {
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, RGB(230, 239, 252));
            SetBkColor(dc, RGB(19, 29, 45));
            SetBkMode(dc, OPAQUE);
            return reinterpret_cast<INT_PTR>(s_darkBrush);
        }
        case WM_ERASEBKGND: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            RECT client{};
            GetClientRect(dialog, &client);
            FillRect(dc, &client, s_darkBrush);
            return TRUE;
        }
        case WM_TIMER:
            if (wParam == 1) {
                ++state->overlay->requested_;
                state->overlay->RenderFrame();
                return TRUE;
            }
            if (wParam == 91) {
                // Restore full opacity 1s after last adjustment
                KillTimer(dialog, 91);
                state->overlay->hoverOpacityPreviewActive_ = false;
                return TRUE;
            }
            if (wParam == 92) {
                KillTimer(dialog, 92);
                state->overlay->showHoverExpandPreview_ = false;
                return TRUE;
            }
            break;
        case WM_HSCROLL:
            if (reinterpret_cast<HWND>(lParam) ==
                GetDlgItem(dialog, IDC_BORDER_THICKNESS)) {
                state->overlay->borderThickness_ = static_cast<int>(
                    SendDlgItemMessageW(
                        dialog, IDC_BORDER_THICKNESS, TBM_GETPOS, 0, 0));
                SetDlgItemInt(
                    dialog, IDC_BORDER_THICKNESS_VALUE,
                    state->overlay->borderThickness_, FALSE);
                RenderBorderDialogChange(dialog, state);
                return TRUE;
            }
            if (reinterpret_cast<HWND>(lParam) ==
                GetDlgItem(dialog, IDC_HOVER_OPACITY)) {
                state->overlay->hoverOpacityPercent_ = static_cast<int>(
                    SendDlgItemMessageW(
                        dialog, IDC_HOVER_OPACITY, TBM_GETPOS, 0, 0));
                UpdateHoverOpacityText(dialog, state);
                state->overlay->hoverOpacityPreviewActive_ = true;
                SetTimer(dialog, 91, 1000, nullptr);
                return TRUE;
            }
            if (reinterpret_cast<HWND>(lParam) ==
                GetDlgItem(dialog, IDC_MODEL_OPACITY)) {
                int val = static_cast<int>(SendDlgItemMessageW(
                    dialog, IDC_MODEL_OPACITY, TBM_GETPOS, 0, 0));
                state->overlay->modelOpacityPercent_ = val;
                SetDlgItemTextW(dialog, IDC_MODEL_OPACITY_VALUE,
                    (std::to_wstring(val) + L"%").c_str());
                SetTimer(dialog, 91, 1000, nullptr);
                return TRUE;
            }
            if (reinterpret_cast<HWND>(lParam) ==
                GetDlgItem(dialog, IDC_HOVER_EXPAND)) {
                int val = static_cast<int>(SendDlgItemMessageW(
                    dialog, IDC_HOVER_EXPAND, TBM_GETPOS, 0, 0));
                state->overlay->hoverExpandPx_ = val;
                SetDlgItemTextW(dialog, IDC_HOVER_EXPAND_VALUE,
                    (std::to_wstring(val) + L"px").c_str());
                state->overlay->showHoverExpandPreview_ = true;
                state->overlay->hoverExpandPreviewUntil_ =
                    Clock::now() + std::chrono::milliseconds(2400);
                SetTimer(dialog, 92, 2400, nullptr);
                return TRUE;
            }
            if (reinterpret_cast<HWND>(lParam) ==
                GetDlgItem(dialog, IDC_BORDER_COLOR_BRIGHTNESS)) {
                const int valuePercent = static_cast<int>(SendDlgItemMessageW(
                    dialog, IDC_BORDER_COLOR_BRIGHTNESS, TBM_GETPOS, 0, 0));
                UpdateColorFromBrightness(dialog, state, valuePercent);
                return TRUE;
            }
            break;
        case WM_DRAWITEM: {
            const auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (item && item->CtlID == IDC_BORDER_COLOR_WHEEL) {
                const int width = item->rcItem.right - item->rcItem.left;
                const int height = item->rcItem.bottom - item->rcItem.top;
                const double centerX = width / 2.0;
                const double centerY = height / 2.0;
                const double radius = (std::max)(
                    1.0, (std::min)(centerX, centerY) - 5.0);
                EnsureColorWheelBitmap(state, width, height);
                BITMAPINFO bitmapInfo{};
                bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bitmapInfo.bmiHeader.biWidth = width;
                bitmapInfo.bmiHeader.biHeight = -height;
                bitmapInfo.bmiHeader.biPlanes = 1;
                bitmapInfo.bmiHeader.biBitCount = 32;
                bitmapInfo.bmiHeader.biCompression = BI_RGB;
                StretchDIBits(
                    item->hDC,
                    item->rcItem.left, item->rcItem.top, width, height,
                    0, 0, width, height,
                    state->wheelPixels.data(), &bitmapInfo,
                    DIB_RGB_COLORS, SRCCOPY);
                constexpr double kPi = 3.14159265358979323846;
                double hue = 0.0;
                double saturation = 0.0;
                CustomColorHueSaturation(
                    state->overlay->customBorderColor_, hue, saturation);
                const double angle = hue * 2.0 * kPi;
                const int markerX = static_cast<int>(
                    centerX + std::cos(angle) * saturation * radius);
                const int markerY = static_cast<int>(
                    centerY + std::sin(angle) * saturation * radius);
                HPEN blackPen = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
                HPEN whitePen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
                HGDIOBJ oldPen = SelectObject(item->hDC, blackPen);
                HGDIOBJ oldBrush = SelectObject(
                    item->hDC, GetStockObject(HOLLOW_BRUSH));
                Ellipse(
                    item->hDC,
                    markerX - 7, markerY - 7,
                    markerX + 8, markerY + 8);
                SelectObject(item->hDC, whitePen);
                Ellipse(
                    item->hDC,
                    markerX - 5, markerY - 5,
                    markerX + 6, markerY + 6);
                SelectObject(item->hDC, oldBrush);
                SelectObject(item->hDC, oldPen);
                DeleteObject(blackPen);
                DeleteObject(whitePen);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
            case IDC_BORDER_MODE:
                if (HIWORD(wParam) == CBN_DROPDOWN) {
                    // Pause the render timer while the dropdown is open;
                    // RenderFrame invalidates the toolbar which closes the popup.
                    KillTimer(dialog, 1);
                    return TRUE;
                }
                if (HIWORD(wParam) == CBN_CLOSEUP) {
                    SetTimer(dialog, 1, 16, nullptr);
                    return TRUE;
                }
                if (HIWORD(wParam) == CBN_SELCHANGE) {
                    state->overlay->borderMode_ = static_cast<int>(
                        SendDlgItemMessageW(
                            dialog, IDC_BORDER_MODE, CB_GETCURSEL, 0, 0));
                    UpdateBorderDialogLayout(dialog, state);
                    RenderBorderDialogChange(dialog, state);
                }
                return TRUE;
            case IDC_HOVER_FADE_ENABLED:
                if (HIWORD(wParam) == BN_CLICKED) {
                    state->overlay->hoverFadeEnabled_ =
                        IsDlgButtonChecked(dialog, IDC_HOVER_FADE_ENABLED) == BST_CHECKED;
                    state->overlay->hoverOpacityPreviewActive_ = true;
                    SetTimer(dialog, 91, 1000, nullptr);
                    UpdateHoverOpacityControls(dialog, state);
                }
                return TRUE;
            case IDC_HOVER_EXPAND_RESET:
                if (HIWORD(wParam) == BN_CLICKED) {
                    state->overlay->hoverExpandPx_ = 0;
                    SendDlgItemMessageW(dialog, IDC_HOVER_EXPAND,
                        TBM_SETPOS, TRUE, 0);
                    SetDlgItemTextW(dialog, IDC_HOVER_EXPAND_VALUE, L"0px");
                    state->overlay->showHoverExpandPreview_ = false;
                    KillTimer(dialog, 92);
                }
                return TRUE;
            case IDC_BORDER_RESET:
                state->overlay->borderMode_ = kBorderModeNormal;
                state->overlay->customBorderColor_ = kDefaultCustomBorderColor;
                CustomColorHueSaturation(
                    state->overlay->customBorderColor_,
                    state->colorHue, state->colorSaturation);
                state->colorValuePercent = CustomColorValuePercent(
                    state->overlay->customBorderColor_);
                state->overlay->borderThickness_ = kDefaultBorderThickness;
                state->overlay->hoverFadeEnabled_ = true;
                state->overlay->hoverOpacityPercent_ = kDefaultHoverOpacityPercent;
                SendDlgItemMessageW(
                    dialog, IDC_BORDER_MODE, CB_SETCURSEL,
                    kBorderModeNormal, 0);
                SendDlgItemMessageW(
                    dialog, IDC_BORDER_COLOR_BRIGHTNESS,
                    TBM_SETPOS, TRUE, state->colorValuePercent);
                SendDlgItemMessageW(
                    dialog, IDC_BORDER_THICKNESS,
                    TBM_SETPOS, TRUE, kDefaultBorderThickness);
                SetDlgItemInt(
                    dialog, IDC_BORDER_THICKNESS_VALUE,
                    kDefaultBorderThickness, FALSE);
                SendDlgItemMessageW(
                    dialog, IDC_HOVER_OPACITY,
                    TBM_SETPOS, TRUE, kDefaultHoverOpacityPercent);
                UpdateHoverOpacityControls(dialog, state);
                UpdateCustomColorText(dialog, state);
                UpdateBorderDialogLayout(dialog, state);
                RenderBorderDialogChange(dialog, state);
                return TRUE;
            case IDOK:
                KillTimer(dialog, 1);
                state->overlay->EndHoverOpacityPreview();
                state->overlay->SaveUiSettings();
                Log("[toolbar] border_settings_saved mode=" +
                    std::to_string(state->overlay->borderMode_) +
                    " thickness=" +
                    std::to_string(state->overlay->borderThickness_));
                DestroyWindow(dialog);
                return TRUE;
            case IDCANCEL:
                RestoreBorderDialogState(dialog, state);
                Log("[toolbar] border_settings_cancel");
                DestroyWindow(dialog);
                return TRUE;
            default:
                break;
            }
            break;
        case WM_CLOSE:
            RestoreBorderDialogState(dialog, state);
            Log("[toolbar] border_settings_cancel");
            DestroyWindow(dialog);
            return TRUE;
        case WM_DESTROY:
            KillTimer(dialog, 1);
            KillTimer(dialog, 91);
            KillTimer(dialog, 92);
            state->overlay->borderDialogOpen_ = false;
            state->overlay->hoverOpacityPreviewActive_ = false;
            state->overlay->showHoverExpandPreview_ = false;
            state->overlay->borderPanelHwnd_ = nullptr;
            delete state;
            return TRUE;
        default:
            break;
        }
        return FALSE;
    }

    void ToggleAspectLock() {
        aspectLocked_ = !aspectLocked_;
        if (aspectLocked_ && sourceWidth_ && sourceHeight_) {
            RECT rect{};
            GetWindowRect(hwnd_, &rect);
            const int width = rect.right - rect.left;
            const int height = (std::max)(1, static_cast<int>(
                (static_cast<std::uint64_t>(width) * sourceHeight_ + sourceWidth_ / 2) /
                sourceWidth_));
            SetWindowPos(
                hwnd_, HWND_TOPMOST, rect.left, rect.top, width, height,
                SWP_NOACTIVATE | SWP_SHOWWINDOW);
            SaveWindowPlacement();
        }
        SaveUiSettings();
        PositionToolbar();
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
        Log(std::string("[toolbar] aspect_lock=") + (aspectLocked_ ? "1" : "0"));
    }

    template <typename Callback>
    auto RunModalWhileRendering(Callback callback) -> decltype(callback()) {
        // Native popup menus and message boxes own a modal loop that throttles
        // ordinary timers. Keep the model alive on a temporary 60 Hz worker.
        std::atomic<bool> modalOpen{ true };
        std::thread modalRenderer([this, &modalOpen]() {
            DWORD taskIndex = 0;
            HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Games", &taskIndex);
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
            const auto frameDuration = std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(1.0 / EffectiveRequestFps()));
            auto nextFrame = Clock::now();
            try {
                while (modalOpen.load(std::memory_order_relaxed)) {
                    const auto now = Clock::now();
                    if (now >= nextFrame) {
                        ++requested_;
                        RenderFrame();
                        nextFrame += frameDuration;
                        if (now - nextFrame > std::chrono::milliseconds(100)) {
                            nextFrame = now;
                        }
                    } else {
                        Sleep(1);
                    }
                }
            } catch (const std::exception& error) {
                Log(std::string("[modal] renderer failed: ") + error.what());
            }
            if (mmcss) {
                AvRevertMmThreadCharacteristics(mmcss);
            }
        });
        auto result = callback();
        modalOpen.store(false, std::memory_order_relaxed);
        modalRenderer.join();
        PrintStats();
        resetFrameSchedule_ = true;
        return result;
    }

    void StartInteractiveRendering() {
        bool expected = false;
        if (!interactiveRendering_.compare_exchange_strong(expected, true)) {
            return;
        }
        interactiveRenderer_ = std::thread([this]() {
            const auto frameDuration = std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(1.0 / 30.0));
            auto nextFrame = Clock::now();
            while (interactiveRendering_.load(std::memory_order_relaxed)) {
                const auto now = Clock::now();
                if (now >= nextFrame) {
                    if (!interactiveFramePending_.exchange(true)) {
                        PostMessageW(hwnd_, kInteractiveRenderMessage, 0, 0);
                    }
                    nextFrame += frameDuration;
                    if (now - nextFrame > std::chrono::milliseconds(100)) {
                        nextFrame = now;
                    }
                } else {
                    Sleep(1);
                }
            }
        });
        Log("[drag] live rendering started");
    }

    void StopInteractiveRendering() {
        if (!interactiveRendering_.exchange(false)) {
            return;
        }
        if (interactiveRenderer_.joinable()) {
            interactiveRenderer_.join();
        }
        interactiveFramePending_.store(false);
        ++requested_;
        RenderFrame();
        PrintStats();
        resetFrameSchedule_ = true;
        PositionToolbar();
        RedrawWindow(
            toolbarHwnd_, nullptr, nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        Log("[drag] live rendering stopped");
    }

    int WindowsGpuPreference(int adapterIndex) const {
        if (adapterIndex == minimumPowerGpuIndex_) {
            return 1;
        }
        if (adapterIndex == highPerformanceGpuIndex_) {
            return 2;
        }
        return 0;
    }

    std::wstring GpuPreferenceLabel(int adapterIndex) const {
        if (adapterIndex == minimumPowerGpuIndex_ &&
            adapterIndex != highPerformanceGpuIndex_) {
            return L"核显/节能";
        }
        if (adapterIndex == highPerformanceGpuIndex_) {
            return L"高性能";
        }
        return L"Windows 自动";
    }

    bool StartOverlayRestartHelper() {
        const std::filesystem::path executable = CurrentExecutablePath();
        std::wstring command = L"\"" + executable.wstring() +
            L"\" --restart-after " + std::to_wstring(GetCurrentProcessId());
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        const BOOL created = CreateProcessW(
            executable.c_str(), command.data(), nullptr, nullptr, FALSE,
            DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW,
            nullptr, executable.parent_path().c_str(), &startup, &process);
        if (!created) {
            Log("[gpu switch] failed to launch restart helper error=" +
                std::to_string(GetLastError()));
            return false;
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return true;
    }

    int ChooseVtsLaunchMode(
        const std::filesystem::path& executable,
        const std::filesystem::path& batch) {
        if (!std::filesystem::is_regular_file(executable) ||
            !std::filesystem::is_regular_file(batch)) {
            return 0;
        }
        TASKDIALOG_BUTTON buttons[] = {
            { 1, L"从 Steam 启动 VTube Studio" },
            { 2, L"从外部启动 start_without_steam.bat" },
        };
        TASKDIALOGCONFIG config{};
        config.cbSize = sizeof(config);
        config.hwndParent = toolbarHwnd_;
        config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
        config.pszWindowTitle = L"VTSFloat_Meow - 选择启动方式";
        config.pszMainInstruction = L"VTube Studio 已关闭，请选择重启方式";
        config.pszContent =
            L"Steam 选项会直接打开 VTube Studio.exe；外部选项会运行安装目录中的 start_without_steam.bat。";
        config.pButtons = buttons;
        config.cButtons = ARRAYSIZE(buttons);
        int selected = 0;
        const HRESULT result = TaskDialogIndirect(
            &config, &selected, nullptr, nullptr);
        return SUCCEEDED(result) ? selected : 0;
    }

    bool RestartVtsOnGpu(int adapterIndex) {
        const int preference = WindowsGpuPreference(adapterIndex);
        if (preference == 0) {
            RunModalWhileRendering([this]() {
                return MessageBoxW(
                    toolbarHwnd_,
                    L"Windows 只能一键指定“节能”或“高性能”GPU，无法可靠指定这张额外显卡。\n\n"
                    L"本次没有修改设置。",
                    L"VTSFloat_Meow - GPU 切换",
                    MB_OK | MB_ICONINFORMATION);
            });
            return false;
        }

        const auto directory = FindVtsDirectory();
        if (!directory) {
            RunModalWhileRendering([this]() {
                return MessageBoxW(
                    toolbarHwnd_,
                    L"没有找到 VTube Studio 安装目录。\n\n"
                    L"程序已经检查正在运行的 VTS、Steam 库和默认安装目录。"
                    L"请先启动一次 VTube Studio 后再尝试。",
                    L"VTSFloat_Meow - 找不到 VTube Studio",
                    MB_OK | MB_ICONWARNING);
            });
            return false;
        }

        const std::filesystem::path vtsExecutable = *directory / kVtsExecutableName;
        const std::filesystem::path startBatch = *directory / kVtsBatchName;
        std::wstring confirmation =
            L"将把 VTube Studio 切换到：\n\n" + GpuName(adapterIndex) +
            L"（" + GpuPreferenceLabel(adapterIndex) + L"）\n\n"
            L"这会暂时中断面捕，并依次执行：\n"
            L"1. 正常关闭 VTube Studio\n"
            L"2. 保存 Windows 显卡偏好\n"
            L"3. 运行 VTube Studio\n"
            L"4. 自动重启覆盖层\n\n"
            L"是否继续？";
        const int answer = RunModalWhileRendering([this, &confirmation]() {
            return MessageBoxW(
                toolbarHwnd_, confirmation.c_str(),
                L"VTSFloat_Meow - 确认切换 GPU",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        });
        if (answer != IDYES) {
            Log("[gpu switch] cancelled by user");
            return false;
        }

        RegistryValueBackup preferenceBackup;
        if (!SetVtsGpuPreference(vtsExecutable, preference, preferenceBackup)) {
            MessageBoxW(
                toolbarHwnd_,
                L"写入 Windows 显卡偏好失败，本次没有关闭 VTube Studio。",
                L"VTSFloat_Meow - GPU 切换失败",
                MB_OK | MB_ICONERROR);
            return false;
        }

        if (const auto running = FindRunningVts()) {
            HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, running->processId);
            const HWND window = FindMainWindowForProcess(running->processId);
            if (!window || !PostMessageW(window, WM_CLOSE, 0, 0)) {
                if (process) CloseHandle(process);
                RestoreVtsGpuPreference(vtsExecutable, preferenceBackup);
                MessageBoxW(
                    toolbarHwnd_,
                    L"无法向 VTube Studio 发送正常退出命令。为了保护当前状态，"
                    L"程序不会强制结束它。",
                    L"VTSFloat_Meow - GPU 切换取消",
                    MB_OK | MB_ICONWARNING);
                return false;
            }
            const DWORD wait = process
                ? WaitForSingleObject(process, 15000)
                : WAIT_FAILED;
            if (process) CloseHandle(process);
            if (wait != WAIT_OBJECT_0) {
                RestoreVtsGpuPreference(vtsExecutable, preferenceBackup);
                MessageBoxW(
                    toolbarHwnd_,
                    L"VTube Studio 在 15 秒内没有退出。为了避免丢失状态，"
                    L"程序没有强制关闭它，GPU 设置也已还原。",
                    L"VTSFloat_Meow - GPU 切换取消",
                    MB_OK | MB_ICONWARNING);
                return false;
            }
        }

        const int launchMode = RunModalWhileRendering([this, &vtsExecutable, &startBatch]() {
            return ChooseVtsLaunchMode(vtsExecutable, startBatch);
        });
        if (launchMode == 0) {
            RestoreVtsGpuPreference(vtsExecutable, preferenceBackup);
            Log("[gpu switch] launch mode cancelled by user");
            return false;
        }
        const std::filesystem::path launchTarget = launchMode == 1
            ? vtsExecutable
            : startBatch;
        const auto launchResult = reinterpret_cast<INT_PTR>(ShellExecuteW(
            nullptr, L"open", launchTarget.c_str(), nullptr,
            directory->c_str(), launchMode == 1 ? SW_SHOWNORMAL : SW_HIDE));
        if (launchResult <= 32) {
            RestoreVtsGpuPreference(vtsExecutable, preferenceBackup);
            MessageBoxW(
                toolbarHwnd_,
                L"未能运行 start_without_steam.bat。显卡偏好已还原，"
                L"请手动启动 VTube Studio。",
                L"VTSFloat_Meow - 启动失败",
                MB_OK | MB_ICONERROR);
            return false;
        }

        selectedGpuIndex_ = adapterIndex;
        gpuSelectionFallback_ = true;
        SaveUiSettings();
        WriteConfigInt(L"ui", L"start_unlocked_once", 1);
        SaveWindowPlacement();
        Log("[gpu switch] target=" + WideToUtf8(GpuName(adapterIndex).c_str()) +
            " preference=" + std::to_string(preference) +
            " vts_dir=" + WideToUtf8(directory->c_str()));

        if (!StartOverlayRestartHelper()) {
            MessageBoxW(
                toolbarHwnd_,
                L"VTube Studio 已开始重启，但覆盖层自动重启助手启动失败。\n\n"
                L"请稍后手动运行 start_overlay.cmd。",
                L"VTSFloat_Meow - 请手动重启覆盖层",
                MB_OK | MB_ICONWARNING);
            return false;
        }
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
        return true;
    }

    void ShowGpuMenu() {
        HMENU menu = CreatePopupMenu();
        if (!menu) {
            return;
        }
        constexpr UINT kRecommendedCommand = 1900;
        constexpr UINT kAdapterCommandBase = 2000;
        const bool hasRecommendedGpu = minimumPowerGpuIndex_ >= 0;
        std::wstring recommendedLabel = L"推荐（使用核显/节能 GPU）";
        if (hasRecommendedGpu) {
            recommendedLabel += L"：" + GpuName(minimumPowerGpuIndex_);
        } else {
            recommendedLabel += L"（未检测到）";
        }
        AppendMenuW(
            menu,
            MF_STRING |
                (selectedGpuIndex_ == minimumPowerGpuIndex_ && hasRecommendedGpu
                     ? MF_CHECKED
                     : 0) |
                (hasRecommendedGpu ? 0 : MF_GRAYED),
            kRecommendedCommand,
            recommendedLabel.c_str());
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        // Keep the high-performance adapter selectable, but make the
        // scheduling trade-off visible right where the user chooses a GPU.
        // This is a recommendation, not a hard restriction: on systems
        // without an iGPU, or when the game is not GPU-bound, the discrete
        // adapter can still be the right choice.
        AppendMenuW(
            menu,
            MF_STRING | MF_GRAYED,
            0,
            L"建议：优先使用核显/节能 GPU 驱动覆盖层（若可用）");
        AppendMenuW(
            menu,
            MF_STRING | MF_GRAYED,
            0,
            L"高性能 GPU 可能与游戏争用调度资源，造成卡顿");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        for (size_t position = 0; position < gpuAdapters_.size(); ++position) {
            const GpuAdapterInfo& info = gpuAdapters_[position];
            UINT flags = MF_STRING;
            if (selectedGpuIndex_ == static_cast<int>(info.index)) {
                flags |= MF_CHECKED;
            }
            std::wstring label = std::to_wstring(position + 1) + L". " + info.name;
            label += L"  [" + GpuPreferenceLabel(static_cast<int>(info.index)) + L"]";
            if (static_cast<int>(info.index) == senderGpuIndex_) {
                label += L"  [Spout 当前]";
            }
            AppendMenuW(
                menu,
                flags,
                kAdapterCommandBase + static_cast<UINT>(position),
                label.c_str());
        }

        POINT cursor{};
        GetCursorPos(&cursor);
        const UINT command = RunModalWhileRendering([this, menu, cursor]() {
            return TrackPopupMenu(
                menu,
                TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,
                cursor.x,
                cursor.y,
                0,
                toolbarHwnd_,
                nullptr);
        });
        DestroyMenu(menu);
        if (!command) {
            return;
        }

        int requested = selectedGpuIndex_;
        if (command == kRecommendedCommand) {
            requested = minimumPowerGpuIndex_;
        } else if (command >= kAdapterCommandBase &&
                   command < kAdapterCommandBase + gpuAdapters_.size()) {
            requested = static_cast<int>(
                gpuAdapters_[command - kAdapterCommandBase].index);
        }
        if (requested >= 0 && requested != senderGpuIndex_) {
            RestartVtsOnGpu(requested);
            return;
        }
        selectedGpuIndex_ = requested;
        gpuSelectionFallback_ = false;
        SaveUiSettings();
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }

    void CreateGraphics() {
        receiver_.SetReceiverName("VTubeStudioSpout");

        ComPtr<IDXGIFactory6> factory;
        Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");

        gpuAdapters_.clear();
        for (UINT index = 0;; ++index) {
            ComPtr<IDXGIAdapter1> enumerated;
            if (factory->EnumAdapters1(index, &enumerated) == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            DXGI_ADAPTER_DESC1 description{};
            enumerated->GetDesc1(&description);
            // Some drivers publish several DXGI logical nodes for one physical
            // board (the current NVIDIA driver exposes three identical RTX
            // entries). Present one useful choice per physical model instead
            // of confusing the user with duplicate menu items.
            const bool duplicate = std::any_of(
                gpuAdapters_.begin(), gpuAdapters_.end(),
                [&description](const GpuAdapterInfo& info) {
                    return _wcsicmp(info.name.c_str(), description.Description) == 0;
                });
            if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 && !duplicate) {
                gpuAdapters_.push_back(GpuAdapterInfo{
                    index,
                    description.Description,
                    description.AdapterLuid,
                });
            }
        }
        for (const GpuAdapterInfo& info : gpuAdapters_) {
            Log("[gpu] index=" + std::to_string(info.index) +
                " name=" + WideToUtf8(info.name.c_str()));
        }

        const auto preferredAdapterIndex =
            [this, &factory](DXGI_GPU_PREFERENCE preference) {
                ComPtr<IDXGIAdapter1> preferred;
                if (FAILED(factory->EnumAdapterByGpuPreference(
                        0, preference, IID_PPV_ARGS(&preferred)))) {
                    return -1;
                }
                DXGI_ADAPTER_DESC1 description{};
                preferred->GetDesc1(&description);
                const auto exact = std::find_if(
                    gpuAdapters_.begin(), gpuAdapters_.end(),
                    [&description](const GpuAdapterInfo& info) {
                        return info.luid.HighPart == description.AdapterLuid.HighPart &&
                               info.luid.LowPart == description.AdapterLuid.LowPart;
                    });
                if (exact != gpuAdapters_.end()) {
                    return static_cast<int>(exact->index);
                }
                const auto sameModel = std::find_if(
                    gpuAdapters_.begin(), gpuAdapters_.end(),
                    [&description](const GpuAdapterInfo& info) {
                        return _wcsicmp(
                            info.name.c_str(), description.Description) == 0;
                    });
                return sameModel == gpuAdapters_.end()
                    ? -1
                    : static_cast<int>(sameModel->index);
            };
        minimumPowerGpuIndex_ = preferredAdapterIndex(
            DXGI_GPU_PREFERENCE_MINIMUM_POWER);
        highPerformanceGpuIndex_ = preferredAdapterIndex(
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE);
        Log("[gpu] minimum_power_index=" +
            std::to_string(minimumPowerGpuIndex_) +
            " high_performance_index=" +
            std::to_string(highPerformanceGpuIndex_));

        int senderAdapter = -1;
        char senderName[256]{};
        if (receiver_.GetActiveSender(senderName)) {
            senderAdapter = receiver_.GetSenderAdapter(senderName);
        }
        senderGpuIndex_ = senderAdapter;
        if (senderAdapter >= 0) {
            ComPtr<IDXGIAdapter1> rawSender;
            if (SUCCEEDED(factory->EnumAdapters1(
                    static_cast<UINT>(senderAdapter), &rawSender))) {
                DXGI_ADAPTER_DESC1 senderDescription{};
                rawSender->GetDesc1(&senderDescription);
                const auto normalized = std::find_if(
                    gpuAdapters_.begin(), gpuAdapters_.end(),
                    [&senderDescription](const GpuAdapterInfo& info) {
                        return _wcsicmp(
                            info.name.c_str(), senderDescription.Description) == 0;
                    });
                if (normalized != gpuAdapters_.end()) {
                    senderGpuIndex_ = static_cast<int>(normalized->index);
                }
            }
        }

        const auto hasAdapter = [this](int index) {
            return std::any_of(
                gpuAdapters_.begin(), gpuAdapters_.end(),
                [index](const GpuAdapterInfo& info) {
                    return static_cast<int>(info.index) == index;
                });
        };
        if (selectedGpuIndex_ >= 0 && !hasAdapter(selectedGpuIndex_)) {
            selectedGpuIndex_ = -1;
            SaveUiSettings();
        }

        int requestedAdapter = selectedGpuIndex_;
        int chosenAdapter = requestedAdapter;
        gpuSelectionFallback_ = false;
        if (senderAdapter >= 0) {
            if (requestedAdapter >= 0 && requestedAdapter != senderGpuIndex_) {
                gpuSelectionFallback_ = true;
            }
            // A Spout shared texture is local to the sender's adapter. Always
            // prefer a working image over a user-selected black surface.
            chosenAdapter = senderAdapter;
        }

        ComPtr<IDXGIAdapter1> adapter;
        if (chosenAdapter >= 0) {
            factory->EnumAdapters1(static_cast<UINT>(chosenAdapter), &adapter);
        }
        if (!adapter) {
            for (UINT index = 0;
                 factory->EnumAdapterByGpuPreference(
                     index,
                     DXGI_GPU_PREFERENCE_MINIMUM_POWER,
                     IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
                 ++index) {
                DXGI_ADAPTER_DESC1 description{};
                adapter->GetDesc1(&description);
                if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
                    break;
                }
                adapter.Reset();
            }
        }
        if (!adapter) {
            Check(factory->EnumAdapters1(0, &adapter), "EnumAdapters1");
        }

        DXGI_ADAPTER_DESC1 adapterDescription{};
        adapter->GetDesc1(&adapterDescription);
        activeGpuIndex_ = -1;
        for (const GpuAdapterInfo& info : gpuAdapters_) {
            if (info.luid.HighPart == adapterDescription.AdapterLuid.HighPart &&
                info.luid.LowPart == adapterDescription.AdapterLuid.LowPart) {
                activeGpuIndex_ = static_cast<int>(info.index);
                break;
            }
        }
        if (activeGpuIndex_ < 0) {
            const auto sameModel = std::find_if(
                gpuAdapters_.begin(), gpuAdapters_.end(),
                [&adapterDescription](const GpuAdapterInfo& info) {
                    return _wcsicmp(
                        info.name.c_str(), adapterDescription.Description) == 0;
                });
            if (sameModel != gpuAdapters_.end()) {
                activeGpuIndex_ = static_cast<int>(sameModel->index);
            }
        }

        constexpr D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };
        D3D_FEATURE_LEVEL selected{};
        Check(
            D3D11CreateDevice(
                adapter.Get(),
                D3D_DRIVER_TYPE_UNKNOWN,
                nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                levels,
                ARRAYSIZE(levels),
                D3D11_SDK_VERSION,
                &device_,
                &selected,
                &context_),
            "D3D11CreateDevice");

        ComPtr<IDXGIDevice> dxgiDevice;
        Check(device_.As(&dxgiDevice), "Query IDXGIDevice");
        dxgiDevice->SetGPUThreadPriority(7);
        if (SUCCEEDED(adapter.As(&activeAdapter3_))) {
            InitializeGpuTelemetry();
        } else {
            Log("[gpu telemetry] IDXGIAdapter3 unavailable; memory telemetry disabled");
        }

        if (!receiver_.OpenDirectX11(device_.Get())) {
            throw std::runtime_error("SpoutDX could not use the D3D11 device");
        }

        std::ostringstream gpuLog;
        gpuLog << "[layered] adapter=" << WideToUtf8(adapterDescription.Description)
               << " active_index=" << activeGpuIndex_
               << " sender_index=" << senderGpuIndex_
               << " sender_dxgi_index=" << senderAdapter
               << " selected_index=" << selectedGpuIndex_
               << " fallback=" << (gpuSelectionFallback_ ? 1 : 0)
               << " mode=UpdateLayeredWindow no_dwm_flush";
        Log(gpuLog.str());
    }

    void RecreateStaging(ID3D11Texture2D* source) {
        D3D11_TEXTURE2D_DESC description{};
        source->GetDesc(&description);
        const UINT previousSourceWidth = sourceWidth_;
        const UINT previousSourceHeight = sourceHeight_;
        const bool sourceAspectChanged =
            previousSourceWidth == 0 || previousSourceHeight == 0 ||
            static_cast<std::uint64_t>(previousSourceWidth) * description.Height !=
                static_cast<std::uint64_t>(description.Width) * previousSourceHeight;
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.SampleDesc.Count = 1;
        description.SampleDesc.Quality = 0;
        description.Usage = D3D11_USAGE_STAGING;
        description.BindFlags = 0;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        description.MiscFlags = 0;

        staging_.Reset();
        Check(device_->CreateTexture2D(&description, nullptr, &staging_),
              "CreateTexture2D(staging)");
        sourceWidth_ = description.Width;
        sourceHeight_ = description.Height;
        sourceFormat_ = description.Format;

        // Keep a locked overlay in sync not only at startup, but also when
        // VTube Studio changes its Spout canvas aspect while being resized.
        if (aspectLocked_ && sourceWidth_ && sourceHeight_ &&
            (!initialAspectApplied_ || sourceAspectChanged)) {
            RECT window{};
            GetWindowRect(hwnd_, &window);
            const int width = (std::max)(1L, window.right - window.left);
            const int currentHeight = (std::max)(1L, window.bottom - window.top);
            const int height = (std::max)(1, static_cast<int>(
                (static_cast<std::uint64_t>(width) * sourceHeight_ + sourceWidth_ / 2) /
                sourceWidth_));
            if (currentHeight != height) {
                SetWindowPos(
                    hwnd_, HWND_TOPMOST, 0, 0, width, height,
                    SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            }
            initialAspectApplied_ = true;
            if (sourceAspectChanged && previousSourceWidth != 0) {
                Log("[layered] sender_aspect_sync=" +
                    std::to_string(sourceWidth_) + "x" +
                    std::to_string(sourceHeight_));
            }
        }
        if (!initialAspectApplied_ && !aspectLocked_) {
            // A freely stretched size is intentional and must survive restart.
            initialAspectApplied_ = true;
        }

        std::ostringstream log;
        log << "[layered] sender=" << sourceWidth_ << "x" << sourceHeight_
            << " format=" << static_cast<unsigned>(sourceFormat_);
        Log(log.str());
    }

    void RecreateDib(int width, int height) {
        width = (std::max)(1, width);
        height = (std::max)(1, height);
        if (width == dibWidth_ && height == dibHeight_ && dibBits_) {
            return;
        }

        DestroyDib();
        memoryDc_ = CreateCompatibleDC(nullptr);
        if (!memoryDc_) {
            throw std::runtime_error("CreateCompatibleDC failed");
        }

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        dib_ = CreateDIBSection(
            memoryDc_, &info, DIB_RGB_COLORS, &dibBits_, nullptr, 0);
        if (!dib_ || !dibBits_) {
            throw std::runtime_error("CreateDIBSection failed");
        }
        previousBitmap_ = SelectObject(memoryDc_, dib_);
        dibWidth_ = width;
        dibHeight_ = height;
        // CreateDIBSection does not guarantee initialized pixels. Clear a
        // newly sized surface before the first Spout frame arrives; otherwise
        // a startup/resize gap can present white garbage as a frozen frame.
        std::memset(dibBits_, 0, static_cast<size_t>(width) * height * 4);
    }

    void DestroyDib() {
        if (memoryDc_ && previousBitmap_) {
            SelectObject(memoryDc_, previousBitmap_);
            previousBitmap_ = nullptr;
        }
        if (dib_) {
            DeleteObject(dib_);
            dib_ = nullptr;
            dibBits_ = nullptr;
        }
        if (memoryDc_) {
            DeleteDC(memoryDc_);
            memoryDc_ = nullptr;
        }
        dibWidth_ = dibHeight_ = 0;
    }

    bool InitializeGpuScaler() {
        if (gpuScalerUnavailable_) {
            return false;
        }
        if (scaleVertexShader_ && scalePixelShader_ && scaleSampler_ &&
            scalePointSampler_ && scaleSettingsBuffer_ && scaleRasterizer_) {
            return true;
        }

        static constexpr char kVertexShader[] = R"(
struct VertexOut {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};
VertexOut main(uint vertexId : SV_VertexID) {
    const float2 positions[3] = {
        float2(-1.0, -1.0), float2(-1.0, 3.0), float2(3.0, -1.0)
    };
    VertexOut output;
    output.position = float4(positions[vertexId], 0.0, 1.0);
    output.uv = float2((positions[vertexId].x + 1.0) * 0.5,
                       (1.0 - positions[vertexId].y) * 0.5);
    return output;
}
 )";
        static constexpr char kPixelShader[] = R"(
Texture2D sourceTexture : register(t0);
SamplerState linearSampler : register(s0);
SamplerState pointSampler : register(s1);
cbuffer ScaleSettings : register(b0) {
    uint2 sourceSize;
    uint qualityMode;
    float padding;
};
float4 Premultiply(float4 color) {
    return float4(color.rgb * color.a, color.a);
}
float CubicWeight(float value) {
    value = abs(value);
    if (value <= 1.0) return 1.5 * value * value * value - 2.5 * value * value + 1.0;
    if (value < 2.0) return -0.5 * value * value * value + 2.5 * value * value - 4.0 * value + 2.0;
    return 0.0;
}
float4 BicubicSample(float2 uv) {
    float2 position = uv * float2(sourceSize) - 0.5;
    int2 base = int2(floor(position));
    float2 fraction = frac(position);
    float4 result = 0.0;
    [unroll]
    for (int y = -1; y <= 2; ++y) {
        const float wy = CubicWeight(float(y) - fraction.y);
        [unroll]
        for (int x = -1; x <= 2; ++x) {
            const float wx = CubicWeight(float(x) - fraction.x);
            const int2 location = clamp(
                base + int2(x, y), int2(0, 0), int2(sourceSize) - int2(1, 1));
            result += Premultiply(sourceTexture.Load(int3(location, 0))) * (wx * wy);
        }
    }
    return saturate(result);
}
float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    if (qualityMode == 0) return Premultiply(sourceTexture.Sample(pointSampler, uv));
    if (qualityMode == 2) return BicubicSample(uv);
    return Premultiply(sourceTexture.Sample(linearSampler, uv));
}
 )";

        const auto fail = [this](const char* stage, HRESULT result,
                                 ID3DBlob* errors = nullptr) {
            std::string message = std::string("[scaler] ") + stage +
                " failed hr=" + std::to_string(static_cast<long>(result));
            if (errors && errors->GetBufferPointer()) {
                message += " ";
                message.append(
                    static_cast<const char*>(errors->GetBufferPointer()),
                    errors->GetBufferSize());
            }
            Log(message);
            gpuScalerUnavailable_ = true;
            return false;
        };

        ComPtr<ID3DBlob> vertexCode;
        ComPtr<ID3DBlob> pixelCode;
        ComPtr<ID3DBlob> errors;
        HRESULT result = D3DCompile(
            kVertexShader, sizeof(kVertexShader) - 1, nullptr, nullptr, nullptr,
            "main", "vs_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
            &vertexCode, &errors);
        if (FAILED(result)) return fail("vertex_compile", result, errors.Get());
        result = D3DCompile(
            kPixelShader, sizeof(kPixelShader) - 1, nullptr, nullptr, nullptr,
            "main", "ps_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
            &pixelCode, &errors);
        if (FAILED(result)) return fail("pixel_compile", result, errors.Get());
        result = device_->CreateVertexShader(
            vertexCode->GetBufferPointer(), vertexCode->GetBufferSize(), nullptr,
            &scaleVertexShader_);
        if (FAILED(result)) return fail("vertex_create", result);
        result = device_->CreatePixelShader(
            pixelCode->GetBufferPointer(), pixelCode->GetBufferSize(), nullptr,
            &scalePixelShader_);
        if (FAILED(result)) return fail("pixel_create", result);

        D3D11_SAMPLER_DESC sampler{};
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.MaxLOD = D3D11_FLOAT32_MAX;
        result = device_->CreateSamplerState(&sampler, &scaleSampler_);
        if (FAILED(result)) return fail("sampler_create", result);
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        result = device_->CreateSamplerState(&sampler, &scalePointSampler_);
        if (FAILED(result)) return fail("point_sampler_create", result);

        D3D11_BUFFER_DESC settings{};
        settings.ByteWidth = 16;
        settings.Usage = D3D11_USAGE_DEFAULT;
        settings.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        result = device_->CreateBuffer(&settings, nullptr, &scaleSettingsBuffer_);
        if (FAILED(result)) return fail("settings_buffer_create", result);

        D3D11_RASTERIZER_DESC rasterizer{};
        rasterizer.FillMode = D3D11_FILL_SOLID;
        rasterizer.CullMode = D3D11_CULL_NONE;
        rasterizer.DepthClipEnable = TRUE;
        result = device_->CreateRasterizerState(&rasterizer, &scaleRasterizer_);
        if (FAILED(result)) return fail("rasterizer_create", result);
        Log("[scaler] gpu_bilinear initialized");
        return true;
    }

    bool EnsureGpuScalerTextures(ID3D11Texture2D* source, int width, int height) {
        if (!InitializeGpuScaler()) {
            return false;
        }
        D3D11_TEXTURE2D_DESC sourceDescription{};
        source->GetDesc(&sourceDescription);
        if (scaleInput_ && scaleSourceWidth_ == sourceDescription.Width &&
            scaleSourceHeight_ == sourceDescription.Height &&
            scaleSourceFormat_ == sourceDescription.Format &&
            scaleOutputWidth_ == width && scaleOutputHeight_ == height) {
            return true;
        }

        scaleInput_.Reset();
        scaleInputView_.Reset();
        scaleOutput_.Reset();
        scaleOutputView_.Reset();
        scaleOutputReadback_.Reset();

        D3D11_TEXTURE2D_DESC input{};
        input.Width = sourceDescription.Width;
        input.Height = sourceDescription.Height;
        input.MipLevels = 1;
        input.ArraySize = 1;
        input.Format = sourceDescription.Format;
        input.SampleDesc.Count = 1;
        input.Usage = D3D11_USAGE_DEFAULT;
        input.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        HRESULT result = device_->CreateTexture2D(&input, nullptr, &scaleInput_);
        if (FAILED(result)) {
            Log("[scaler] input_texture failed hr=" +
                std::to_string(static_cast<long>(result)));
            gpuScalerUnavailable_ = true;
            return false;
        }
        result = device_->CreateShaderResourceView(
            scaleInput_.Get(), nullptr, &scaleInputView_);
        if (FAILED(result)) {
            Log("[scaler] input_view failed hr=" +
                std::to_string(static_cast<long>(result)));
            gpuScalerUnavailable_ = true;
            return false;
        }

        D3D11_TEXTURE2D_DESC output{};
        output.Width = static_cast<UINT>(width);
        output.Height = static_cast<UINT>(height);
        output.MipLevels = 1;
        output.ArraySize = 1;
        output.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        output.SampleDesc.Count = 1;
        output.Usage = D3D11_USAGE_DEFAULT;
        output.BindFlags = D3D11_BIND_RENDER_TARGET;
        result = device_->CreateTexture2D(&output, nullptr, &scaleOutput_);
        if (FAILED(result)) {
            Log("[scaler] output_texture failed hr=" +
                std::to_string(static_cast<long>(result)));
            gpuScalerUnavailable_ = true;
            return false;
        }
        result = device_->CreateRenderTargetView(
            scaleOutput_.Get(), nullptr, &scaleOutputView_);
        if (FAILED(result)) {
            Log("[scaler] output_view failed hr=" +
                std::to_string(static_cast<long>(result)));
            gpuScalerUnavailable_ = true;
            return false;
        }
        output.Usage = D3D11_USAGE_STAGING;
        output.BindFlags = 0;
        output.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        result = device_->CreateTexture2D(&output, nullptr, &scaleOutputReadback_);
        if (FAILED(result)) {
            Log("[scaler] readback_texture failed hr=" +
                std::to_string(static_cast<long>(result)));
            gpuScalerUnavailable_ = true;
            return false;
        }
        scaleSourceWidth_ = sourceDescription.Width;
        scaleSourceHeight_ = sourceDescription.Height;
        scaleSourceFormat_ = sourceDescription.Format;
        scaleOutputWidth_ = width;
        scaleOutputHeight_ = height;
        Log("[scaler] gpu_bilinear resources=" + std::to_string(width) + "x" +
            std::to_string(height));
        return true;
    }

    bool GpuScaleIntoDib(ID3D11Texture2D* source, int width, int height) {
        if (!EnsureGpuScalerTextures(source, width, height)) {
            return false;
        }
        context_->CopyResource(scaleInput_.Get(), source);
        ID3D11RenderTargetView* renderTarget = scaleOutputView_.Get();
        context_->OMSetRenderTargets(1, &renderTarget, nullptr);
        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        context_->RSSetViewports(1, &viewport);
        context_->RSSetState(scaleRasterizer_.Get());
        context_->IASetInputLayout(nullptr);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(scaleVertexShader_.Get(), nullptr, 0);
        context_->PSSetShader(scalePixelShader_.Get(), nullptr, 0);
        ID3D11ShaderResourceView* sourceView = scaleInputView_.Get();
        context_->PSSetShaderResources(0, 1, &sourceView);
        ID3D11SamplerState* sampler = scaleSampler_.Get();
        ID3D11SamplerState* samplers[] = { sampler, scalePointSampler_.Get() };
        context_->PSSetSamplers(0, ARRAYSIZE(samplers), samplers);
        struct ScaleSettings {
            UINT sourceWidth;
            UINT sourceHeight;
            UINT qualityMode;
            float padding;
        } settings{
            scaleSourceWidth_,
            scaleSourceHeight_,
            static_cast<UINT>(scalingQuality_),
            0.0f,
        };
        context_->UpdateSubresource(scaleSettingsBuffer_.Get(), 0, nullptr, &settings, 0, 0);
        ID3D11Buffer* settingsBuffer = scaleSettingsBuffer_.Get();
        context_->PSSetConstantBuffers(0, 1, &settingsBuffer);
        context_->Draw(3, 0);
        ID3D11ShaderResourceView* nullView = nullptr;
        context_->PSSetShaderResources(0, 1, &nullView);
        context_->CopyResource(scaleOutputReadback_.Get(), scaleOutput_.Get());

        D3D11_MAPPED_SUBRESOURCE mapped{};
        const HRESULT mapResult = context_->Map(
            scaleOutputReadback_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(mapResult)) {
            Log("[scaler] readback_map failed hr=" +
                std::to_string(static_cast<long>(mapResult)));
            return false;
        }
        auto* destination = static_cast<std::uint8_t*>(dibBits_);
        const auto* sourceBytes = static_cast<const std::uint8_t*>(mapped.pData);
        for (int y = 0; y < height; ++y) {
            std::memcpy(
                destination + static_cast<size_t>(y) * width * 4,
                sourceBytes + static_cast<size_t>(y) * mapped.RowPitch,
                static_cast<size_t>(width) * 4);
        }
        context_->Unmap(scaleOutputReadback_.Get(), 0);
        return true;
    }

    bool IsCursorOverModel() const {
        if (!dibBits_ || dibWidth_ <= 0 || dibHeight_ <= 0 || !hwnd_) {
            return false;
        }
        POINT cursor{};
        RECT window{};
        if (!GetCursorPos(&cursor) || !GetWindowRect(hwnd_, &window)) {
            return false;
        }
        const int x = cursor.x - window.left;
        const int y = cursor.y - window.top;
        const int expand = hoverExpandPx_;
        const int w = dibWidth_;
        const int h = dibHeight_;
        const bool haveRawModel = !hoverPreviewBasePixels_.empty() &&
            hoverPreviewBaseWidth_ == w && hoverPreviewBaseHeight_ == h;
        const auto* pixels = haveRawModel
            ? hoverPreviewBasePixels_.data()
            : static_cast<const std::uint8_t*>(dibBits_);
        auto alphaAt = [&](int xx, int yy) -> int {
            if (xx < 0 || yy < 0 || xx >= w || yy >= h) return 0;
            // Border decoration pixels do not count as model.
            if (PreviewBorderPixel(xx, yy)) return 0;
            // Performance/debug pixels do not count as model either.
            if (DebugOverlayPixel(xx, yy)) return 0;
            return pixels[(static_cast<size_t>(yy) * w + xx) * 4 + 3];
        };
        if (expand == 0) {
            return alphaAt(x, y) > 8;
        }
        if (expand > 0) {
            // Any opaque pixel within `expand` radius (Chebyshev distance) triggers hover.
            const int step = (std::max)(1, expand / 20);
            for (int dy = -expand; dy <= expand; dy += step) {
                for (int dx = -expand; dx <= expand; dx += step) {
                    if (alphaAt(x + dx, y + dy) > 8) return true;
                }
            }
            return false;
        }
        // expand < 0: require cursor be `-expand` inside model boundary.
        const int inset = -expand;
        // Cursor must be on a model pixel AND surrounded by model pixels within inset radius.
        if (alphaAt(x, y) <= 8) return false;
        const int step = (std::max)(1, inset / 20);
        for (int dy = -inset; dy <= inset; dy += step) {
            for (int dx = -inset; dx <= inset; dx += step) {
                if (alphaAt(x + dx, y + dy) <= 8) return false;
            }
        }
        return true;
    }

    bool PreviewBorderPixel(int x, int y) const {
        const int preserved = (std::min)(
            (std::max)(2, borderThickness_ + 2),
            (std::min)(dibWidth_, dibHeight_) / 2);
        return x < preserved || y < preserved ||
            x >= dibWidth_ - preserved || y >= dibHeight_ - preserved;
    }

    // The debug performance readout is an overlay, not model content. Keep a
    // conservative hit-test rectangle around it so opacity/hover expansion
    // cannot turn its text or chart into a model pixel.
    bool DebugOverlayPixel(int x, int y) const {
        if (!debugMode_ || dibWidth_ < 180 || dibHeight_ < 90) {
            return false;
        }
        constexpr int kOverlayWidth = 520;
        constexpr int kOverlayHeight = 300;
        return x >= 0 && y >= 0 &&
            x < (std::min)(dibWidth_, kOverlayWidth) &&
            y < (std::min)(dibHeight_, kOverlayHeight);
    }

    void RestoreHoverPreviewBase() {
        if (!dibBits_ || hoverPreviewBasePixels_.empty() ||
            hoverPreviewBaseWidth_ != dibWidth_ ||
            hoverPreviewBaseHeight_ != dibHeight_) {
            return;
        }
        std::memcpy(
            dibBits_, hoverPreviewBasePixels_.data(),
            hoverPreviewBasePixels_.size());
    }

    void ApplyHoverPreviewOpacity() {
        const bool debugOpacity = debugMode_ && currentOverlayAlpha_ < 255;
        const bool panelOpacity = borderDialogOpen_ && currentOverlayAlpha_ < 255;
        if ((!hoverOpacityPreviewActive_ && !debugOpacity && !panelOpacity) || !dibBits_ ||
            hoverPreviewBasePixels_.empty() ||
            hoverPreviewBaseWidth_ != dibWidth_ ||
            hoverPreviewBaseHeight_ != dibHeight_) {
            return;
        }
        RestoreHoverPreviewBase();
        const double factor = (std::clamp)(
            currentOverlayAlpha_ / 255.0, 0.0, 1.0);
        if (factor >= 0.999) {
            return;
        }
        auto* pixels = static_cast<std::uint8_t*>(dibBits_);
        for (int y = 0; y < dibHeight_; ++y) {
            for (int x = 0; x < dibWidth_; ++x) {
                if (PreviewBorderPixel(x, y) || DebugOverlayPixel(x, y)) {
                    continue;
                }
                auto* pixel = pixels +
                    (static_cast<size_t>(y) * dibWidth_ + x) * 4;
                pixel[0] = static_cast<std::uint8_t>(
                    std::lround(pixel[0] * factor));
                pixel[1] = static_cast<std::uint8_t>(
                    std::lround(pixel[1] * factor));
                pixel[2] = static_cast<std::uint8_t>(
                    std::lround(pixel[2] * factor));
                pixel[3] = static_cast<std::uint8_t>(
                    std::lround(pixel[3] * factor));
            }
        }
    }

    void CaptureHoverPreviewBase() {
        const bool debugOpacity = debugMode_ && currentOverlayAlpha_ < 255;
        const bool panelOpacity = borderDialogOpen_ && currentOverlayAlpha_ < 255;
        if ((!hoverOpacityPreviewActive_ && !debugOpacity && !panelOpacity) || !dibBits_ ||
            dibWidth_ <= 0 || dibHeight_ <= 0) {
            return;
        }
        const size_t bytes = static_cast<size_t>(dibWidth_) * dibHeight_ * 4;
        hoverPreviewBasePixels_.resize(bytes);
        std::memcpy(hoverPreviewBasePixels_.data(), dibBits_, bytes);
        hoverPreviewBaseWidth_ = dibWidth_;
        hoverPreviewBaseHeight_ = dibHeight_;
        ApplyHoverPreviewOpacity();
    }

    void EndHoverOpacityPreview() {
        RestoreHoverPreviewBase();
        hoverPreviewBasePixels_.clear();
        hoverPreviewBaseWidth_ = 0;
        hoverPreviewBaseHeight_ = 0;
        hoverOpacityPreviewActive_ = false;
        currentOverlayAlpha_ = 255;
        hoverFadeStartAlpha_ = 255;
        hoverTargetAlpha_ = 255;
        hoverFadeStarted_ = Clock::now();
    }

    bool UpdateHoverOpacity() {
        // The VTS status page is an application UI, not model content. Never
        // apply the model hover-fade alpha to it.
        if (statusMode_ != VtsStatusMode::Hidden && !statusDismissed_) {
            const bool changed = currentOverlayAlpha_ != 255 ||
                hoverTargetAlpha_ != 255;
            currentOverlayAlpha_ = 255;
            hoverFadeStartAlpha_ = 255;
            hoverTargetAlpha_ = 255;
            hoverFadeStarted_ = Clock::now();
            return changed;
        }
        // Model opacity is a normal target even while the personalization
        // panel is open. The render path below keeps the panel itself opaque.
        int baseAlpha = modelOpacityPercent_ * 255 / 100;
        int target = baseAlpha;
        if (hoverOpacityPreviewActive_) {
            target = hoverFadeEnabled_
                ? hoverOpacityPercent_ * 255 / 100
                : modelOpacityPercent_ * 255 / 100;
        } else if ((locked_ || borderDialogOpen_) && hoverFadeEnabled_ && IsCursorOverModel()) {
            target = hoverOpacityPercent_ * 255 / 100;
        }

        const auto now = Clock::now();
        hoverTargetAlpha_ = target;
        // Exponential smoothing: alpha approaches target with time constant
        // proportional to kHoverFadeDurationMs. Frame-rate independent.
        const double dtMs = std::chrono::duration<double, std::milli>(
            now - hoverFadeStarted_).count();
        hoverFadeStarted_ = now;
        // t = 1 - exp(-dt / tau), tau ≈ kHoverFadeDurationMs / 3
        const double tau = kHoverFadeDurationMs / 3.0;
        const double t = (std::clamp)(1.0 - std::exp(-dtMs / tau), 0.0, 1.0);
        const int alpha = static_cast<int>(std::lround(
            currentOverlayAlpha_ + (hoverTargetAlpha_ - currentOverlayAlpha_) * t));
        if (alpha == currentOverlayAlpha_) {
            return false;
        }
        currentOverlayAlpha_ = alpha;
        return true;
    }

    void RestoreHoverExpression() {
        if (!hoverExpressionHovered_ || hoverExpressionRestored_ ||
            hoverExpressionFile_.empty()) {
            return;
        }
        vtsApi_.SetExpressionActive(
            hoverExpressionFile_, hoverExpressionOriginalActive_, 0.35);
        hoverExpressionRestored_ = true;
    }

    void CancelHoverExpression() {
        RestoreHoverExpression();
        hoverExpressionHovered_ = false;
        hoverExpressionRestored_ = false;
        hoverExpressionOriginalActive_ = false;
        hoverExpressionRestoreAt_ = Clock::time_point{};
    }

    void UpdateHoverExpression() {
        const bool shouldHover = locked_ && !borderDialogOpen_ &&
            hoverExpressionEnabled_ && !hoverExpressionFile_.empty() &&
            hasReceivedModel_ && vtsApi_.IsConnected() && IsCursorOverModel();
        if (shouldHover && !hoverExpressionHovered_) {
            bool originalActive = false;
            if (!vtsApi_.GetExpressionActive(hoverExpressionFile_, originalActive)) {
                // The API worker refreshes the expression list asynchronously.
                // Do not activate until the pre-hover state is known, otherwise
                // leaving the model could accidentally disable a user expression.
                vtsApi_.RequestExpressionState();
                return;
            }
            hoverExpressionOriginalActive_ = originalActive;
            hoverExpressionHovered_ = true;
            hoverExpressionRestored_ = false;
            hoverExpressionRestoreAt_ = Clock::now() + std::chrono::seconds(4);
            vtsApi_.SetExpressionActive(hoverExpressionFile_, true, 0.3);
        } else if (!shouldHover && hoverExpressionHovered_) {
            CancelHoverExpression();
        } else if (hoverExpressionHovered_ && !hoverExpressionRestored_ &&
            Clock::now() >= hoverExpressionRestoreAt_) {
            // Keep the hover edge latched until the pointer leaves, so a
            // stationary pointer does not retrigger the expression every 4s.
            RestoreHoverExpression();
        }
    }

    void RestorePanelExpressionPreview() {
        if (!expressionPanelPreviewActive_ || expressionPanelPreviewFile_.empty()) {
            return;
        }
        vtsApi_.SetExpressionActive(
            expressionPanelPreviewFile_, expressionPanelPreviewOriginalActive_, 0.35);
        expressionPanelPreviewActive_ = false;
        expressionPanelPreviewOriginalActive_ = false;
        expressionPanelPreviewFile_.clear();
    }

    bool PreviewExpressionFromPanel() {
        if (hoverExpressionFile_.empty() || !vtsApi_.IsConnected()) {
            vtsApi_.RequestExpressionState();
            return false;
        }
        RestorePanelExpressionPreview();
        bool originalActive = false;
        if (!vtsApi_.GetExpressionActive(hoverExpressionFile_, originalActive)) {
            vtsApi_.RequestExpressionState();
            return false;
        }
        expressionPanelPreviewFile_ = hoverExpressionFile_;
        expressionPanelPreviewOriginalActive_ = originalActive;
        expressionPanelPreviewActive_ = true;
        vtsApi_.SetExpressionActive(expressionPanelPreviewFile_, true, 0.3);
        return true;
    }

    void DrawBgWarningOverlay(int width, int height) {
        if (locked_ || !opaqueBackgroundDetected_ || bgWarningPermanentlyDismissed_) {
            bgWarningCloseRect_ = RECT{};
            bgWarningDontShowRect_ = RECT{};
            return;
        }
        if (!memoryDc_ || !dibBits_ || width < 320 || height < 90) return;

        const int panelW = 470;
        const int panelH = 62;
        const int panelX = width - panelW - 12;
        const int panelY = 12;

        HDC dc = memoryDc_;
        RECT panel{ panelX, panelY, panelX + panelW, panelY + panelH };
        HBRUSH bg = CreateSolidBrush(RGB(40, 12, 28));
        FillRect(dc, &panel, bg);
        DeleteObject(bg);
        HBRUSH border = CreateSolidBrush(RGB(255, 120, 180));
        FrameRect(dc, &panel, border);
        DeleteObject(border);

        HFONT font = CreateFontW(
            -14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        HGDIOBJ oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);

        SetTextColor(dc, RGB(255, 220, 235));
        RECT line1{ panelX + 8, panelY + 4, panelX + panelW - 8, panelY + 22 };
        DrawTextW(dc, L"检测到当前 Spout 非透明推流：",
            -1, &line1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT line2{ panelX + 8, panelY + 20, panelX + panelW - 8, panelY + 38 };
        DrawTextW(dc, L"请在 VTS 主界面更改背景为 \"ColorPicker\" 并启用 \"透明推流\"",
            -1, &line2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Buttons at bottom-right
        const int btnW = 90;
        const int btnH = 18;
        const int btnY = panelY + panelH - btnH - 4;
        const int gap = 4;
        bgWarningCloseRect_ = RECT{
            panelX + panelW - 8 - btnW, btnY,
            panelX + panelW - 8, btnY + btnH };
        bgWarningDontShowRect_ = RECT{
            bgWarningCloseRect_.left - gap - btnW, btnY,
            bgWarningCloseRect_.left - gap, btnY + btnH };
        SetTextColor(dc, RGB(160, 200, 255));
        DrawTextW(dc, L"[不再提示]", -1, &bgWarningDontShowRect_,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(dc, RGB(200, 200, 200));
        DrawTextW(dc, L"[关闭提示]", -1, &bgWarningCloseRect_,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, oldFont);
        DeleteObject(font);

        auto* pixels = static_cast<std::uint8_t*>(dibBits_);
        const int stride = width * 4;
        for (int y = panelY; y < panelY + panelH && y < height; ++y) {
            for (int x = panelX; x < panelX + panelW && x < width; ++x) {
                pixels[y * stride + x * 4 + 3] = 255;
            }
        }
    }

    void DrawHoverExpandPreview(int width, int height) {
        // The expansion band is a hover affordance: show it only while the
        // cursor is over the model or its configured expansion hit area. Once
        // shown, keep it for 2.4 seconds after the cursor leaves.
        const bool hoverPreview = borderDialogOpen_ && hoverFadeEnabled_ && IsCursorOverModel();
        const bool editingPreview = borderDialogOpen_ && hoverFadeEnabled_ && hoverExpandEditing_;
        if (!hoverFadeEnabled_) {
            showHoverExpandPreview_ = false;
            hoverExpandPreviewUntil_ = Clock::time_point{};
            hoverExpandPreviewAlpha_ = 0.0;
            return;
        }
        const auto now = Clock::now();
        if (hoverPreview || editingPreview) {
            showHoverExpandPreview_ = true;
            if (!editingPreview) {
                hoverExpandPreviewUntil_ = now + std::chrono::milliseconds(2400);
            }
        } else if (showHoverExpandPreview_ &&
                   now >= hoverExpandPreviewUntil_) {
            showHoverExpandPreview_ = false;
        }
        const double targetAlpha = showHoverExpandPreview_ ? 1.0 : 0.0;
        const double dtMs = std::clamp(
            std::chrono::duration<double, std::milli>(
                now - hoverExpandFadeStarted_).count(),
            0.0, 100.0);
        hoverExpandFadeStarted_ = now;
        const double fadeStep = 1.0 - std::exp(-dtMs / 70.0);
        hoverExpandPreviewAlpha_ +=
            (targetAlpha - hoverExpandPreviewAlpha_) * fadeStep;
        if (hoverExpandPreviewAlpha_ < 0.01 && !showHoverExpandPreview_) {
            hoverExpandPreviewAlpha_ = 0.0;
            return;
        }
        if (!dibBits_ || width <= 0 || height <= 0) return;
        const int r = hoverExpandPx_;
        if (r == 0) return;
        const int absR = std::abs(r);
        auto* pixels = static_cast<std::uint8_t*>(dibBits_);
        const bool haveRawModel = !hoverPreviewBasePixels_.empty() &&
            hoverPreviewBaseWidth_ == width && hoverPreviewBaseHeight_ == height;
        const auto* modelPixels = haveRawModel
            ? hoverPreviewBasePixels_.data() : pixels;
        // Ignore the decorative border ring by treating it as background.
        auto isBorderRegion = [&](int x, int y) {
            return PreviewBorderPixel(x, y);
        };
        auto alphaAt = [&](int x, int y) -> int {
            if (x < 0 || y < 0 || x >= width || y >= height) return 0;
            if (isBorderRegion(x, y)) return 0;
            if (DebugOverlayPixel(x, y)) return 0;
            return modelPixels[(static_cast<size_t>(y) * width + x) * 4 + 3];
        };
        auto isModel = [&](int x, int y) { return alphaAt(x, y) > 128; };
        const bool outside = r > 0;
        auto setHighlight = [&](int x, int y) {
            if (x < 0 || y < 0 || x >= width || y >= height) return;
            if (isBorderRegion(x, y)) return;
            if (DebugOverlayPixel(x, y)) return;
            auto* p = pixels + (static_cast<size_t>(y) * width + x) * 4;
            if (outside) {
                p[0] = static_cast<std::uint8_t>(std::lround(40 * hoverExpandPreviewAlpha_));
                p[1] = static_cast<std::uint8_t>(std::lround(40 * hoverExpandPreviewAlpha_));
                p[2] = static_cast<std::uint8_t>(std::lround(255 * hoverExpandPreviewAlpha_));
            } else {
                p[0] = static_cast<std::uint8_t>(std::lround(255 * hoverExpandPreviewAlpha_));
                p[1] = static_cast<std::uint8_t>(std::lround(60 * hoverExpandPreviewAlpha_));
                p[2] = static_cast<std::uint8_t>(std::lround(40 * hoverExpandPreviewAlpha_));
            }
            p[3] = static_cast<std::uint8_t>(
                std::lround(255 * hoverExpandPreviewAlpha_));
        };
        // Compute chebyshev distance transform.
        std::vector<int> dist(static_cast<size_t>(width) * height, 0);
        const int inf = width + height;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const bool isSource = outside ? isModel(x, y) : !isModel(x, y);
                dist[static_cast<size_t>(y) * width + x] = isSource ? 0 : inf;
            }
        }
        // Forward pass
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int& d = dist[static_cast<size_t>(y) * width + x];
                if (d == 0) continue;
                if (x > 0) d = (std::min)(d, dist[static_cast<size_t>(y) * width + x - 1] + 1);
                if (y > 0) d = (std::min)(d, dist[static_cast<size_t>(y - 1) * width + x] + 1);
                if (x > 0 && y > 0) d = (std::min)(d,
                    dist[static_cast<size_t>(y - 1) * width + x - 1] + 1);
                if (x < width - 1 && y > 0) d = (std::min)(d,
                    dist[static_cast<size_t>(y - 1) * width + x + 1] + 1);
                if (d > absR + 2) d = absR + 2;
            }
        }
        // Backward pass
        for (int y = height - 1; y >= 0; --y) {
            for (int x = width - 1; x >= 0; --x) {
                int& d = dist[static_cast<size_t>(y) * width + x];
                if (d == 0) continue;
                if (x < width - 1) d = (std::min)(d,
                    dist[static_cast<size_t>(y) * width + x + 1] + 1);
                if (y < height - 1) d = (std::min)(d,
                    dist[static_cast<size_t>(y + 1) * width + x] + 1);
                if (x < width - 1 && y < height - 1) d = (std::min)(d,
                    dist[static_cast<size_t>(y + 1) * width + x + 1] + 1);
                if (x > 0 && y < height - 1) d = (std::min)(d,
                    dist[static_cast<size_t>(y + 1) * width + x - 1] + 1);
            }
        }
        // For negative expand: first dim all model pixels to 40% alpha so
        // the inner blue band is visible without dimming the highlight itself.
        if (!outside) {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    if (isBorderRegion(x, y) || DebugOverlayPixel(x, y)) continue;
                    auto* p = pixels + (static_cast<size_t>(y) * width + x) * 4;
                    if (p[3] > 8) {
                        // Reduce alpha to 40% but keep premultiplied colors proportional
                        const int newA = static_cast<int>(p[3]) * 40 / 100;
                        if (p[3] > 0) {
                            p[0] = static_cast<std::uint8_t>(p[0] * newA / p[3]);
                            p[1] = static_cast<std::uint8_t>(p[1] * newA / p[3]);
                            p[2] = static_cast<std::uint8_t>(p[2] * newA / p[3]);
                            p[3] = static_cast<std::uint8_t>(newA);
                        }
                    }
                }
            }
        }
        // Fill pixels where dist <= absR (band from edge outward/inward)
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int d = dist[static_cast<size_t>(y) * width + x];
                if (d > 0 && d <= absR) {
                    setHighlight(x, y);
                }
            }
        }
    }

    void DrawGpuWarningOverlay(int width, int height) {
        if (locked_ || !gpuWarningShown_ || gpuWarningPermanentlyDismissed_) {
            gpuWarningDontShowRect_ = RECT{};
            return;
        }
        if (!memoryDc_ || !dibBits_ || width < 320 || height < 60) return;

        const int panelW = (std::min)(620, width - 24);
        const int panelH = 34;
        const int panelX = width - panelW - 12;
        int panelY = 12;
        if (opaqueBackgroundDetected_ && !bgWarningPermanentlyDismissed_) {
            panelY = 12 + 62 + 8;  // below BG warning
        }
        HDC dc = memoryDc_;
        RECT panel{ panelX, panelY, panelX + panelW, panelY + panelH };
        HBRUSH bg = CreateSolidBrush(RGB(50, 40, 15));
        FillRect(dc, &panel, bg);
        DeleteObject(bg);
        HBRUSH border = CreateSolidBrush(RGB(255, 200, 100));
        FrameRect(dc, &panel, border);
        DeleteObject(border);

        HFONT font = CreateFontW(
            -14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        HGDIOBJ oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);

        gpuWarningDontShowRect_ = RECT{
            panelX + 6, panelY + 6, panelX + 6 + 90, panelY + panelH - 6 };
        SetTextColor(dc, RGB(160, 200, 255));
        DrawTextW(dc, L"[不再提示]", -1, &gpuWarningDontShowRect_,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SetTextColor(dc, RGB(255, 230, 170));
        RECT textRect{ panelX + 6 + 90 + 6, panelY,
            panelX + panelW - 6, panelY + panelH };
        DrawTextW(dc, L"当前运行在高性能显卡中，高负载场景性能将会受限",
            -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(dc, oldFont);
        DeleteObject(font);

        // Force alpha=255 for panel pixels
        auto* pixels = static_cast<std::uint8_t*>(dibBits_);
        const int stride = width * 4;
        for (int y = panelY; y < panelY + panelH && y < height; ++y) {
            for (int x = panelX; x < panelX + panelW && x < width; ++x) {
                pixels[y * stride + x * 4 + 3] = 255;
            }
        }
    }

    void PresentLayeredFrame(const RECT& rect, int width, int height) {
        if (!memoryDc_ || !dibBits_ || width != dibWidth_ || height != dibHeight_) {
            return;
        }
        DrawBgWarningOverlay(width, height);
        DrawGpuWarningOverlay(width, height);
        DrawHoverExpandPreview(width, height);
        const auto updateStarted = Clock::now();
        POINT destination{ rect.left, rect.top };
        SIZE size{ width, height };
        POINT sourcePoint{ 0, 0 };
        BLENDFUNCTION blend{
            AC_SRC_OVER, 0,
            static_cast<BYTE>(
                (statusMode_ != VtsStatusMode::Hidden && !statusDismissed_)
                    ? 255
                    // Debug text is drawn after the model-only opacity pass;
                    // never multiply it by the window-wide alpha.
                    : (debugMode_ || borderDialogOpen_ ? 255
                                  : (hoverOpacityPreviewActive_ ? 255 : currentOverlayAlpha_))),
            AC_SRC_ALPHA };
        HDC screen = GetDC(nullptr);
        const BOOL updated = UpdateLayeredWindow(
            hwnd_, screen, &destination, &size, memoryDc_, &sourcePoint,
            0, &blend, ULW_ALPHA);
        ReleaseDC(nullptr, screen);

        const double updateMs = std::chrono::duration<double, std::milli>(
            Clock::now() - updateStarted).count();
        updateMs_ += updateMs;
        maxUpdateMs_ = (std::max)(maxUpdateMs_, updateMs);
        if (updated) {
            ++updated_;
        } else {
            ++errors_;
        }
    }

    void RenderFrame() {
        if (!overlayVisible_) {
            return;
        }
        UpdateHoverOpacity();
        UpdateHoverExpression();
        const auto presentVtsStatus = [this]() {
            RECT current{};
            GetWindowRect(hwnd_, &current);
            const int width = (std::max)(1L, current.right - current.left);
            const int height = (std::max)(1L, current.bottom - current.top);
            RecreateDib(width, height);
            if (statusFrameDirty_ || width != statusDrawWidth_ || height != statusDrawHeight_) {
                DrawVtsStatusPanel(width, height);
                statusDrawWidth_ = width;
                statusDrawHeight_ = height;
            }
            PresentLayeredFrame(current, width, height);
        };
        const auto receiveStarted = Clock::now();
        if (!receiver_.ReceiveTexture()) {
            PollVtsStatus();
            presentVtsStatus();
            return;
        }
        // Spout's optional frame-count mutex can block for 67 ms when another
        // receiver or a busy game holds it. The overlay already has its own
        // cadence and does not need Spout's semaphore; release that optional
        // lock after the first successful receive so texture acquisition is
        // non-blocking. We still copy the latest shared texture each tick.
        if (!spoutFrameSyncDisabled_) {
            receiver_.DisableFrameCount();
            spoutFrameSyncDisabled_ = true;
        }
        receiveMs_ += std::chrono::duration<double, std::milli>(
            Clock::now() - receiveStarted).count();
        ++received_;

        ID3D11Texture2D* source = receiver_.GetSenderTexture();
        if (!source || (!spoutFrameSyncDisabled_ && !receiver_.IsFrameNew())) {
            PollVtsStatus();
            presentVtsStatus();
            return;
        }
        // Modal menus render on a worker thread.  Do not change window styles
        // from that thread; ApplyClickThrough must stay on the UI thread or
        // TrackPopupMenu can deadlock and make the process appear hung.
        if (GetCurrentThreadId() == uiThreadId_) {
            HideVtsStatus();
        }
        ++newFrames_;
        const bool firstModelFrame = !hasReceivedModel_;
        hasReceivedModel_ = true;
        if (firstModelFrame && GetCurrentThreadId() == uiThreadId_) {
            if (!apiStartedAfterModel_ && toolbarHwnd_) {
                apiStartScheduled_ = true;
                SetTimer(toolbarHwnd_, 48, kVtsApiModelReadyDelayMs, nullptr);
            }
            // The API banner is intentionally hidden before a real Spout
            // model frame arrives. Reflow and repaint immediately on that
            // first frame so an unavailable API is not hidden until a later
            // timer tick or toolbar interaction.
            PositionToolbar();
            if (toolbarHwnd_) {
                InvalidateRect(toolbarHwnd_, nullptr, FALSE);
            }
        }

        D3D11_TEXTURE2D_DESC description{};
        source->GetDesc(&description);
        if (!staging_ || description.Width != sourceWidth_ ||
            description.Height != sourceHeight_ || description.Format != sourceFormat_) {
            RecreateStaging(source);
        }

        RECT rect{};
        GetWindowRect(hwnd_, &rect);
        const int width = (std::max)(1L, rect.right - rect.left);
        const int height = (std::max)(1L, rect.bottom - rect.top);
        RecreateDib(width, height);

        const auto mapStarted = Clock::now();
        const bool needsScaling =
            static_cast<UINT>(width) != sourceWidth_ ||
            static_cast<UINT>(height) != sourceHeight_;
        bool scaledOnGpu = needsScaling && GpuScaleIntoDib(source, width, height);
        if (!scaledOnGpu) {
            context_->CopyResource(staging_.Get(), source);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            const HRESULT mapResult = context_->Map(
                staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
            if (FAILED(mapResult)) {
                ++errors_;
                return;
            }
            ConvertAndScale(mapped, width, height);
            context_->Unmap(staging_.Get(), 0);
        }
        mapMs_ += std::chrono::duration<double, std::milli>(Clock::now() - mapStarted).count();

        // Capture the raw model before controls/debug text are painted. This
        // keeps opacity previews and hover fades restricted to model pixels.
        CaptureHoverPreviewBase();

        if (!locked_ || debugMode_) {
            DrawControls(width, height);
        }
        if (debugMode_) {
            DrawDebugOverlay(width, height);
        }

        PresentLayeredFrame(rect, width, height);
    }

    void EnsureBilinearScaleMaps(int width, int height) {
        if (scaleMapWidth_ == width && scaleMapHeight_ == height &&
            scaleMapSourceWidth_ == sourceWidth_ &&
            scaleMapSourceHeight_ == sourceHeight_) {
            return;
        }

        const auto makeSample = [](int destinationPosition, int destinationExtent,
                                   UINT sourceExtent) {
            BilinearSample sample{};
            if (sourceExtent <= 1 || destinationExtent <= 0) {
                return sample;
            }
            const std::int64_t coordinate =
                ((static_cast<std::int64_t>(destinationPosition) * 2 + 1) *
                 sourceExtent * 256) /
                    (static_cast<std::int64_t>(destinationExtent) * 2) -
                128;
            const std::int64_t maximum =
                static_cast<std::int64_t>(sourceExtent - 1) * 256;
            if (coordinate <= 0) {
                return sample;
            }
            if (coordinate >= maximum) {
                sample.first = sample.second = sourceExtent - 1;
                return sample;
            }
            sample.first = static_cast<UINT>(coordinate / 256);
            sample.second = sample.first + 1;
            sample.fraction = static_cast<unsigned>(coordinate % 256);
            return sample;
        };

        scaleMapX_.resize(width);
        scaleMapY_.resize(height);
        for (int x = 0; x < width; ++x) {
            scaleMapX_[x] = makeSample(x, width, sourceWidth_);
        }
        for (int y = 0; y < height; ++y) {
            scaleMapY_[y] = makeSample(y, height, sourceHeight_);
        }
        scaleMapWidth_ = width;
        scaleMapHeight_ = height;
        scaleMapSourceWidth_ = sourceWidth_;
        scaleMapSourceHeight_ = sourceHeight_;
    }

    void ConvertAndScale(const D3D11_MAPPED_SUBRESOURCE& mapped, int width, int height) {
        auto* destination = static_cast<std::uint8_t*>(dibBits_);
        const auto* sourceBase = static_cast<const std::uint8_t*>(mapped.pData);
        const bool sourceIsBgra =
            sourceFormat_ == DXGI_FORMAT_B8G8R8A8_UNORM ||
            sourceFormat_ == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

        const auto writePremultiplied = [sourceIsBgra](
            const std::uint8_t* pixel, std::uint8_t* output) {
            const std::uint8_t alpha = pixel[3];
            const std::uint8_t red = sourceIsBgra ? pixel[2] : pixel[0];
            const std::uint8_t green = pixel[1];
            const std::uint8_t blue = sourceIsBgra ? pixel[0] : pixel[2];
            output[0] = static_cast<std::uint8_t>(
                (static_cast<unsigned>(blue) * alpha + 127) / 255);
            output[1] = static_cast<std::uint8_t>(
                (static_cast<unsigned>(green) * alpha + 127) / 255);
            output[2] = static_cast<std::uint8_t>(
                (static_cast<unsigned>(red) * alpha + 127) / 255);
            output[3] = alpha;
        };

        // No resampling at 1:1: retain the original fast copy path.
        if (static_cast<UINT>(width) == sourceWidth_ &&
            static_cast<UINT>(height) == sourceHeight_) {
            for (int y = 0; y < height; ++y) {
                const auto* sourceRow = sourceBase +
                    static_cast<size_t>(y) * mapped.RowPitch;
                auto* destinationRow = destination +
                    static_cast<size_t>(y) * width * 4;
                for (int x = 0; x < width; ++x) {
                    writePremultiplied(sourceRow + static_cast<size_t>(x) * 4,
                                       destinationRow + static_cast<size_t>(x) * 4);
                }
            }
            return;
        }

        EnsureBilinearScaleMaps(width, height);
        const auto sourceChannel = [sourceIsBgra](
            const std::uint8_t* pixel, int outputChannel) -> unsigned {
            if (outputChannel == 1) return pixel[1];
            if (outputChannel == 0) return sourceIsBgra ? pixel[0] : pixel[2];
            return sourceIsBgra ? pixel[2] : pixel[0];
        };

        for (int y = 0; y < height; ++y) {
            const BilinearSample& vertical = scaleMapY_[y];
            const auto* topRow = sourceBase +
                static_cast<size_t>(vertical.first) * mapped.RowPitch;
            const auto* bottomRow = sourceBase +
                static_cast<size_t>(vertical.second) * mapped.RowPitch;
            auto* destinationRow = destination + static_cast<size_t>(y) * width * 4;

            for (int x = 0; x < width; ++x) {
                const BilinearSample& horizontal = scaleMapX_[x];
                const auto* topLeft = topRow + static_cast<size_t>(horizontal.first) * 4;
                const auto* topRight = topRow + static_cast<size_t>(horizontal.second) * 4;
                const auto* bottomLeft = bottomRow + static_cast<size_t>(horizontal.first) * 4;
                const auto* bottomRight = bottomRow + static_cast<size_t>(horizontal.second) * 4;
                const unsigned x0 = 256 - horizontal.fraction;
                const unsigned y0 = 256 - vertical.fraction;
                const unsigned weights[4] = {
                    x0 * y0,
                    horizontal.fraction * y0,
                    x0 * vertical.fraction,
                    horizontal.fraction * vertical.fraction,
                };
                const std::uint8_t* pixels[4] = {
                    topLeft, topRight, bottomLeft, bottomRight };
                auto* output = destinationRow + static_cast<size_t>(x) * 4;
                for (int channel = 0; channel < 3; ++channel) {
                    // Each channel is at most 255 * (weights summing to 65536),
                    // so 32-bit arithmetic is sufficient and much faster here.
                    unsigned value = 0;
                    for (int sample = 0; sample < 4; ++sample) {
                        const unsigned alpha = pixels[sample][3];
                        const unsigned premultiplied =
                            (sourceChannel(pixels[sample], channel) * alpha + 127) / 255;
                        value += premultiplied * weights[sample];
                    }
                    output[channel] = static_cast<std::uint8_t>((value + 32768) >> 16);
                }
                unsigned alpha = 0;
                for (int sample = 0; sample < 4; ++sample) {
                    alpha += static_cast<unsigned>(pixels[sample][3]) * weights[sample];
                }
                output[3] = static_cast<std::uint8_t>((alpha + 32768) >> 16);
            }
        }
    }

    void PutPixel(int x, int y, std::uint8_t blue, std::uint8_t green,
                  std::uint8_t red, std::uint8_t alpha = 255) {
        if (x < 0 || y < 0 || x >= dibWidth_ || y >= dibHeight_) {
            return;
        }
        auto* pixel = static_cast<std::uint8_t*>(dibBits_) +
            (static_cast<size_t>(y) * dibWidth_ + x) * 4;
        pixel[0] = blue;
        pixel[1] = green;
        pixel[2] = red;
        pixel[3] = alpha;
    }

    void BlendPixel(int x, int y, std::uint8_t blue, std::uint8_t green,
                    std::uint8_t red, std::uint8_t alpha) {
        if (x < 0 || y < 0 || x >= dibWidth_ || y >= dibHeight_) {
            return;
        }
        auto* pixel = static_cast<std::uint8_t*>(dibBits_) +
            (static_cast<size_t>(y) * dibWidth_ + x) * 4;
        const unsigned inverse = 255u - alpha;
        pixel[0] = static_cast<std::uint8_t>(
            (static_cast<unsigned>(blue) * alpha + pixel[0] * inverse + 127) / 255);
        pixel[1] = static_cast<std::uint8_t>(
            (static_cast<unsigned>(green) * alpha + pixel[1] * inverse + 127) / 255);
        pixel[2] = static_cast<std::uint8_t>(
            (static_cast<unsigned>(red) * alpha + pixel[2] * inverse + 127) / 255);
        pixel[3] = static_cast<std::uint8_t>(
            alpha + (static_cast<unsigned>(pixel[3]) * inverse + 127) / 255);
    }

    RainbowColor RainbowAt(int perimeterPosition, int perimeter, int phase) const {
        if (perimeter <= 0) {
            return { 255, 255, 255 };
        }
        // Two complete gradients around the window, with one animated color
        // revolution every five seconds. Integer HSV keeps this inexpensive.
        int hue = static_cast<int>(
            (static_cast<std::int64_t>(perimeterPosition) * 1536 * 2) /
            perimeter);
        hue = (hue + phase) % 1536;
        if (hue < 0) hue += 1536;
        const int sector = hue / 256;
        const std::uint8_t value = static_cast<std::uint8_t>(hue % 256);
        const std::uint8_t inverse = static_cast<std::uint8_t>(255 - value);
        switch (sector) {
        case 0: return { 255, value, 0 };
        case 1: return { inverse, 255, 0 };
        case 2: return { 0, 255, value };
        case 3: return { 0, inverse, 255 };
        case 4: return { value, 0, 255 };
        default: return { 255, 0, inverse };
        }
    }

    void PutRainbowPixel(
        int x, int y, int perimeterPosition, int perimeter, int phase) {
        const RainbowColor color = RainbowAt(perimeterPosition, perimeter, phase);
        PutPixel(x, y, color.blue, color.green, color.red);
    }

    void DrawControls(int width, int height) {
        // Normal mode crossfades the entire frame in sync. Chase mode keeps
        // the spatial rainbow moving around the perimeter.
        const int horizontal = (std::max)(0, width - 1);
        const int vertical = (std::max)(0, height - 1);
        const int perimeter = (std::max)(1, 2 * horizontal + 2 * vertical);
        constexpr std::int64_t kAnimationPeriodMs = 5000;
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now().time_since_epoch()).count();
        const std::int64_t cycleMs = elapsedMs % kAnimationPeriodMs;
        const int phase = static_cast<int>(
            cycleMs * 1536 / kAnimationPeriodMs);
        const RainbowColor wholeFrameColor = borderMode_ == kBorderModeCustom
            ? RainbowColor{
                GetRValue(customBorderColor_),
                GetGValue(customBorderColor_),
                GetBValue(customBorderColor_)
            }
            : HsvWheelColor(static_cast<double>(phase) / 1536.0, 1.0);
        const auto put = [this, perimeter, phase, wholeFrameColor](
            int x, int y, int perimeterPosition) {
            if (borderMode_ == kBorderModeChase) {
                PutRainbowPixel(x, y, perimeterPosition, perimeter, phase);
            } else {
                PutPixel(
                    x, y,
                    wholeFrameColor.blue,
                    wholeFrameColor.green,
                    wholeFrameColor.red);
            }
        };

        for (int n = 0; n < borderThickness_; ++n) {
            for (int x = n; x < width - n; ++x) {
                put(x, n, x);
                put(
                    x, height - 1 - n,
                    horizontal + vertical + (horizontal - x));
            }
            for (int y = n; y < height - n; ++y) {
                put(n, y, perimeter - y);
                put(width - 1 - n, y, horizontal + y);
            }
        }
        for (int n = borderThickness_; n < borderThickness_ + 2; ++n) {
            for (int i = n; i < (std::min)(kCornerLength, width - n); ++i) {
                put(i, n, i);
                put(width - 1 - i, n, horizontal - i);
                put(
                    i, height - 1 - n,
                    horizontal + vertical + (horizontal - i));
                put(
                    width - 1 - i, height - 1 - n,
                    horizontal + vertical + i);
            }
            for (int i = n; i < (std::min)(kCornerLength, height - n); ++i) {
                put(n, i, perimeter - i);
                put(width - 1 - n, i, horizontal + i);
                put(
                    n, height - 1 - i,
                    perimeter - (vertical - i));
                put(
                    width - 1 - n, height - 1 - i,
                    horizontal + vertical - i);
            }
        }
    }

    void DrawDebugOverlay(int width, int height) {
        if (!debugMode_ || !dibBits_ || !memoryDc_ || width < 180 || height < 90) {
            return;
        }
        DrawDebugOverlayImpl(width, height);
    }

    void DrawDebugOverlayImpl(int width, int height) {

        const int left = 12;
        const int top = 10;
        const int lineHeight = 17;
        const int chartTop = top + lineHeight * 9 + 5;
        const int chartWidth = (std::min)(340, width - left * 2);
        const int chartHeight = (std::min)(86, height - chartTop - 12);

        std::wostringstream first;
        first << std::fixed << std::setprecision(1)
              << L"FPS " << lastUpdateFps_
              << L"  目标 " << EffectiveRequestFps();
        std::wostringstream firstB;
        firstB << L"VTS配置 ";
        if (vtsConfiguredFps_ > 0) {
            if (!vtsConfiguredMode_.empty() &&
                vtsConfiguredMode_ != L"FPS_" + std::to_wstring(vtsConfiguredFps_)) {
                firstB << vtsConfiguredMode_ << L" ";
            }
            firstB << vtsConfiguredFps_;
        } else {
            firstB << L"--";
        }
        firstB << L"  VTS实时 ";
        if (vtsApiFps_ > 0) {
            firstB << vtsApiFps_;
        } else if (apiWasConnected_ && !vtsApi_.IsConnected()) {
            firstB << L"Failed";
        } else {
            firstB << L"未启用";
        }
        std::wostringstream second;
        second << std::fixed << std::setprecision(2)
               << L"接收 " << lastReceiveMs_ << L"ms"
               << L"  缩放 " << lastMapScaleMs_ << L"ms"
               << L"  上屏 " << lastUpdateMs_ << L"ms";
        std::wostringstream third;
        third << std::fixed << std::setprecision(1)
              << L"CPU "
              << (cpuUsagePercent_ ? *cpuUsagePercent_ : 0.0) << L"%/"
              << systemCpuPercent_ << L"%";
        std::wostringstream fourth;
        fourth << std::fixed << std::setprecision(1)
               << L"GPU "
               << (gpuUsagePercent_ ? *gpuUsagePercent_ : 0.0) << L"%/"
               << systemGpuPercent_ << L"%";
        std::wostringstream fifth;
        fifth << std::fixed << std::setprecision(0)
              << L"显存 "
              << static_cast<double>(gpuMemoryCurrentBytes_) / (1024.0 * 1024.0)
              << L" MB  内存 "
              << static_cast<double>(processMemoryBytes_) / (1024.0 * 1024.0)
              << L"/" << systemMemoryMb_ << L" MB";

        std::wostringstream sixth;
        const auto extra = vtsApi_.GetExtraStats();
        if (vtsApi_.IsConnected() && extra.uptimeMs > 0) {
            const long long totalSec = extra.uptimeMs / 1000;
            const long long h = totalSec / 3600;
            const long long m = (totalSec % 3600) / 60;
            const long long s = totalSec % 60;
            sixth << L"VTS运行 " << h << L":"
                  << std::setw(2) << std::setfill(L'0') << m << L":"
                  << std::setw(2) << std::setfill(L'0') << s
                  << std::setfill(L' ')
                  << L"  模型 "
                  << (extra.modelId.empty() ? L"--" : Utf8ToWide(extra.modelId))
                  << L"  面数 " << extra.artmeshCount
                  << L"  道具 " << extra.itemCount
                  << L"  API " << std::fixed << std::setprecision(1) << extra.apiLatencyMs << L"ms";
        } else {
            sixth << L"VTS API 未连接";
        }

        std::wostringstream seventh;
        std::wostringstream eighth;
        if (sourceWidth_ > 0 && sourceHeight_ > 0 && dibWidth_ > 0 && dibHeight_ > 0) {
            const double srcPixels = static_cast<double>(sourceWidth_) * sourceHeight_;
            const double dstPixels = static_cast<double>(dibWidth_) * dibHeight_;
            const double ratio = (std::min)(srcPixels, dstPixels) / (std::max)(srcPixels, dstPixels);
            const double pct = ratio * 100.0;
            seventh << L"渲染 " << dibWidth_ << L"x" << dibHeight_
                    << L"  VTS " << sourceWidth_ << L"x" << sourceHeight_
                    << L"  对齐率 " << std::fixed << std::setprecision(1)
                    << pct << L"%";

            const wchar_t* rating;
            if (pct >= 90.0) {
                rating = L"完美（推荐对齐率区间）";
            } else if (dstPixels < srcPixels) {
                // Render < VTS: downscaling, quality direction
                if (pct < 10.0) rating = L"绝顶质量";
                else if (pct < 30.0) rating = L"极高质量";
                else if (pct < 50.0) rating = L"超高质量";
                else if (pct < 70.0) rating = L"较高质量";
                else rating = L"质量";
            } else {
                // Render > VTS: upscaling, performance direction
                if (pct < 10.0) rating = L"究极性能";
                else if (pct < 30.0) rating = L"极高性能";
                else if (pct < 50.0) rating = L"超高性能";
                else if (pct < 70.0) rating = L"较高性能";
                else rating = L"性能";
            }
            eighth << L"当前性能评级：" << rating;
        } else {
            seventh << L"渲染 --  VTS --  对齐率 --";
            eighth << L"当前性能评级：--";
        }

        HFONT font = CreateFontW(
            -14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HGDIOBJ oldFont = SelectObject(memoryDc_, font);
        SetBkMode(memoryDc_, TRANSPARENT);

        const auto drawText = [this, width](
            const std::wstring& value, int x, int y, COLORREF color) {
            RECT rect{ x, y, width - 8, y + 16 };
            if (rect.right <= rect.left || rect.bottom <= rect.top) return;
            const int rectWidth = rect.right - rect.left;
            const int rectHeight = rect.bottom - rect.top;
            auto* pixels = static_cast<std::uint8_t*>(dibBits_);
            // Save original pixels
            std::vector<std::uint8_t> before(
                static_cast<size_t>(rectWidth) * rectHeight * 4);
            for (int row = 0; row < rectHeight; ++row) {
                std::memcpy(
                    before.data() + static_cast<size_t>(row) * rectWidth * 4,
                    pixels + (static_cast<size_t>(rect.top + row) * width + rect.left) * 4,
                    static_cast<size_t>(rectWidth) * 4);
            }
            SetTextColor(memoryDc_, color);
            DrawTextW(memoryDc_, value.c_str(), -1, &rect,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            // Restore non-text pixels, set alpha=255 only where text was drawn
            for (int row = 0; row < rectHeight; ++row) {
                for (int col = 0; col < rectWidth; ++col) {
                    auto* pixel = pixels +
                        (static_cast<size_t>(rect.top + row) * width + rect.left + col) * 4;
                    const auto* orig = before.data() +
                        (static_cast<size_t>(row) * rectWidth + col) * 4;
                    if (pixel[0] != orig[0] || pixel[1] != orig[1] || pixel[2] != orig[2]) {
                        pixel[3] = 255;
                    } else {
                        pixel[0] = orig[0];
                        pixel[1] = orig[1];
                        pixel[2] = orig[2];
                        pixel[3] = orig[3];
                    }
                }
            }
        };

        drawText(first.str(), left, top, RGB(255, 255, 255));
        drawText(firstB.str(), left, top + lineHeight, RGB(255, 255, 255));
        drawText(second.str(), left, top + lineHeight * 2, RGB(183, 229, 255));
        drawText(third.str(), left, top + lineHeight * 3, RGB(255, 225, 155));
        drawText(fourth.str(), left, top + lineHeight * 4, RGB(183, 255, 198));
        drawText(fifth.str(), left, top + lineHeight * 5, RGB(183, 255, 198));
        drawText(sixth.str(), left, top + lineHeight * 6, RGB(200, 220, 255));
        drawText(seventh.str(), left, top + lineHeight * 7, RGB(255, 200, 220));
        drawText(eighth.str(), left, top + lineHeight * 8, RGB(255, 235, 180));

        if (chartWidth > 40 && chartHeight > 20 && debugFpsHistory_.size() > 1) {
            drawText(L"FPS / 帧时", left, chartTop - 2, RGB(226, 239, 255));
            const int graphTop = chartTop + 14;
            const int graphHeight = chartHeight - 14;
            const double fpsScale = (std::max)(60.0, static_cast<double>(EffectiveRequestFps()));
            const double frameScale = 33.3;
            const auto drawSeries = [this, left, chartWidth, graphTop, graphHeight](
                const std::vector<double>& values, double scale,
                std::uint8_t blue, std::uint8_t green, std::uint8_t red) {
                if (values.size() < 2) {
                    return;
                }
                for (size_t index = 1; index < values.size(); ++index) {
                    const auto point = [left, chartWidth, graphTop, graphHeight,
                                       &values, scale](size_t position) {
                        const double normalized = (std::clamp)(
                            values[position] / scale, 0.0, 1.0);
                        const int x = left + static_cast<int>(
                            (position * (chartWidth - 1)) /
                            (values.size() - 1));
                        const int y = graphTop + graphHeight - 1 - static_cast<int>(
                            normalized * (graphHeight - 1));
                        return POINT{ x, y };
                    };
                    const POINT from = point(index - 1);
                    const POINT to = point(index);
                    const int steps = (std::max)(
                        std::abs(to.x - from.x), std::abs(to.y - from.y));
                    for (int step = 0; step <= steps; ++step) {
                        const double ratio = steps == 0
                            ? 0.0
                            : static_cast<double>(step) / steps;
                        const int x = static_cast<int>(std::lround(
                            from.x + (to.x - from.x) * ratio));
                        const int y = static_cast<int>(std::lround(
                            from.y + (to.y - from.y) * ratio));
                        BlendPixel(x, y, blue, green, red, 220);
                    }
                }
            };
            drawSeries(debugFpsHistory_, fpsScale, 40, 220, 255);
            drawSeries(debugFrameMsHistory_, frameScale, 80, 170, 255);
        }
        SelectObject(memoryDc_, oldFont);
        DeleteObject(font);
    }

    void DrawVtsStatusPanel(int width, int height) {
        if (statusMode_ == VtsStatusMode::Hidden || statusDismissed_ || !dibBits_) {
            return;
        }
        std::memset(dibBits_, 0, static_cast<size_t>(width) * height * 4);
        // The status page belongs to the overlay, so dock it directly below
        // the toolbar and use exactly the same width as the model window.
        const int panelWidth = width;
        const int panelHeight = (std::min)(height, 280);
        const int left = 0;
        const int top = 0;
        const RECT panel{ left, top, left + panelWidth, top + panelHeight };
        statusExeRect_ = RECT{};
        statusBatchRect_ = RECT{};
        const auto fill = [this](const RECT& rect, BYTE r, BYTE g, BYTE b, BYTE a) {
            const BYTE blue = static_cast<BYTE>((b * a + 127) / 255);
            const BYTE green = static_cast<BYTE>((g * a + 127) / 255);
            const BYTE red = static_cast<BYTE>((r * a + 127) / 255);
            for (int y = rect.top; y < rect.bottom; ++y) {
                for (int x = rect.left; x < rect.right; ++x) {
                    PutPixel(x, y, blue, green, red, a);
                }
            }
        };
        fill(panel, 17, 27, 43, 255);
        for (int x = panel.left; x < panel.right; ++x) {
            PutPixel(x, panel.top, 24, 132, 255);
            PutPixel(x, panel.bottom - 1, 24, 132, 255);
        }
        for (int y = panel.top; y < panel.bottom; ++y) {
            PutPixel(panel.left, y, 24, 132, 255);
            PutPixel(panel.right - 1, y, 24, 132, 255);
        }

        const bool canLaunch = statusMode_ == VtsStatusMode::LaunchChoices;
        if (canLaunch) {
            const int gap = 14;
            const int buttonTop = panel.bottom - 94;
            const int buttonHeight = 42;
            const int buttonWidth = (panelWidth - 48 - gap) / 2;
            statusExeRect_ = RECT{
                panel.left + 24, buttonTop,
                panel.left + 24 + buttonWidth, buttonTop + buttonHeight };
            statusBatchRect_ = RECT{
                statusExeRect_.right + gap, buttonTop,
                statusExeRect_.right + gap + buttonWidth, buttonTop + buttonHeight };
            fill(statusExeRect_, 33, 74, 118, 255);
            fill(statusBatchRect_, 33, 74, 118, 255);
        }

        HFONT titleFont = CreateFontW(
            -22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT bodyFont = CreateFontW(
            -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HGDIOBJ oldFont = SelectObject(memoryDc_, titleFont);
        SetBkMode(memoryDc_, TRANSPARENT);
        SetTextColor(memoryDc_, RGB(240, 246, 255));
        RECT titleRect{ panel.left + 24, panel.top + 22,
                        panel.right - 24, panel.top + 54 };
        const std::wstring title = statusMode_ == VtsStatusMode::LaunchChoices
            ? L"已找到 VTube Studio，但当前尚未运行"
            : L"等待 VTube Studio 启动中";
        DrawTextW(memoryDc_, title.c_str(), -1, &titleRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        SelectObject(memoryDc_, bodyFont);
        RECT instructionRect{ panel.left + 24, panel.top + 64,
                              panel.right - 24, panel.top + 124 };
        const std::wstring instruction =
            L"如已启动，请在 设置 - 相机 中打开“激活 Spout2”开关，\r\n"
            L"将背景调整成“ColorPicker”，然后启动透明推流。";
        DrawTextW(memoryDc_, instruction.c_str(), -1, &instructionRect,
                  DT_LEFT | DT_TOP | DT_WORDBREAK);

        RECT pathRect{ panel.left + 24, panel.bottom - 42,
                       panel.right - 24, panel.bottom - 14 };
        const std::wstring pathText = statusDirectory_.empty()
            ? L"尚未找到 VTube Studio 安装目录，请先安装或启动一次 VTube Studio。"
            : L"VTS 安装路径：" + statusDirectory_.wstring();
        SetTextColor(memoryDc_, RGB(164, 204, 240));
        DrawTextW(memoryDc_, pathText.c_str(), -1, &pathRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        if (canLaunch) {
            SetTextColor(memoryDc_, RGB(240, 246, 255));
            const std::wstring exeLabel = L"从 Steam 启动 VTube Studio";
            const std::wstring batchLabel = L"从外部启动 VTS";
            DrawTextW(memoryDc_, exeLabel.c_str(), -1, &statusExeRect_,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            DrawTextW(memoryDc_, batchLabel.c_str(), -1, &statusBatchRect_,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        SelectObject(memoryDc_, oldFont);
        DeleteObject(titleFont);
        DeleteObject(bodyFont);

        // GDI text on a 32-bit DIB may leave alpha at zero. Make glyph pixels
        // opaque after drawing so they survive UpdateLayeredWindow blending.
        for (int y = panel.top; y < panel.bottom; ++y) {
            for (int x = panel.left; x < panel.right; ++x) {
                auto* pixel = static_cast<BYTE*>(dibBits_) +
                    (static_cast<size_t>(y) * width + x) * 4;
                if (pixel[3] == 0 && (pixel[0] || pixel[1] || pixel[2])) {
                    pixel[3] = 255;
                }
            }
        }
        statusFrameDirty_ = false;
    }

    void InitializeGpuTelemetry() {
        // GPU Engine is the same per-process counter Task Manager uses.  It is
        // sampled only with our existing two-second perf update, never per frame.
        const PDH_STATUS queryStatus = PdhOpenQueryW(nullptr, 0, &gpuUsageQuery_);
        if (queryStatus != ERROR_SUCCESS) {
            gpuUsageQuery_ = nullptr;
            Log("[gpu telemetry] PdhOpenQuery failed=" + std::to_string(queryStatus));
            return;
        }
        const PDH_STATUS counterStatus = PdhAddEnglishCounterW(
            gpuUsageQuery_,
            L"\\GPU Engine(*)\\Utilization Percentage",
            0,
            &gpuUsageCounter_);
        if (counterStatus != ERROR_SUCCESS) {
            PdhCloseQuery(gpuUsageQuery_);
            gpuUsageQuery_ = nullptr;
            gpuUsageCounter_ = nullptr;
            Log("[gpu telemetry] GPU Engine counter unavailable=" +
                std::to_string(counterStatus));
            return;
        }
        PdhCollectQueryData(gpuUsageQuery_); // Establish the first rate sample.
        Log("[gpu telemetry] enabled");
    }

    void UpdateGpuTelemetry() {
        FILETIME creationTime{};
        FILETIME exitTime{};
        FILETIME kernelTime{};
        FILETIME userTime{};
        if (GetProcessTimes(
                GetCurrentProcess(), &creationTime, &exitTime,
                &kernelTime, &userTime)) {
            const auto toTicks = [](const FILETIME& value) {
                ULARGE_INTEGER ticks{};
                ticks.LowPart = value.dwLowDateTime;
                ticks.HighPart = value.dwHighDateTime;
                return ticks.QuadPart;
            };
            const ULONGLONG kernelTicks = toTicks(kernelTime);
            const ULONGLONG userTicks = toTicks(userTime);
            const auto now = Clock::now();
            if (cpuSampleInitialized_) {
                const double elapsedSeconds =
                    std::chrono::duration<double>(now - cpuSampleTime_).count();
                const ULONGLONG processTicks =
                    (kernelTicks - previousKernelTicks_) +
                    (userTicks - previousUserTicks_);
                const DWORD processorCount =
                    GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
                const DWORD normalizedProcessorCount =
                    processorCount == 0 ? 1 : processorCount;
                if (elapsedSeconds > 0.0) {
                    const double processSeconds =
                        static_cast<double>(processTicks) / 10000000.0;
                    cpuUsagePercent_ = (std::clamp)(
                        processSeconds / elapsedSeconds /
                            static_cast<double>(normalizedProcessorCount) *
                                100.0,
                        0.0, 100.0);
                }
            }
            previousKernelTicks_ = kernelTicks;
            previousUserTicks_ = userTicks;
            cpuSampleTime_ = now;
            cpuSampleInitialized_ = true;
        }

        PROCESS_MEMORY_COUNTERS_EX processMemory{};
        if (GetProcessMemoryInfo(
                GetCurrentProcess(),
                reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&processMemory),
                sizeof(processMemory))) {
            processMemoryBytes_ = processMemory.WorkingSetSize;
        }

        // System-wide CPU
        FILETIME sysIdle{}, sysKernel{}, sysUser{};
        if (GetSystemTimes(&sysIdle, &sysKernel, &sysUser)) {
            auto ft2u = [](const FILETIME& ft) -> ULONGLONG {
                return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
            };
            ULONGLONG idle = ft2u(sysIdle), kernel = ft2u(sysKernel), user = ft2u(sysUser);
            if (prevSysKernel_ != 0 || prevSysUser_ != 0) {
                ULONGLONG sysTotal = (kernel - prevSysKernel_) + (user - prevSysUser_);
                ULONGLONG sysIdleDelta = idle - prevSysIdle_;
                if (sysTotal > 0) {
                    systemCpuPercent_ = (std::clamp)(
                        static_cast<double>(sysTotal - sysIdleDelta) / sysTotal * 100.0, 0.0, 100.0);
                }
            }
            prevSysIdle_ = idle;
            prevSysKernel_ = kernel;
            prevSysUser_ = user;
        }

        // System-wide memory
        MEMORYSTATUSEX memStatus{};
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus)) {
            systemMemoryMb_ = static_cast<double>(
                memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024.0 * 1024.0);
        }

        if (activeAdapter3_) {
            DXGI_QUERY_VIDEO_MEMORY_INFO memory{};
            HRESULT memoryResult = activeAdapter3_->QueryVideoMemoryInfo(
                0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memory);
            if (FAILED(memoryResult) || memory.Budget == 0) {
                memory = {};
                memoryResult = activeAdapter3_->QueryVideoMemoryInfo(
                    0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &memory);
            }
            if (SUCCEEDED(memoryResult)) {
                gpuMemoryCurrentBytes_ = memory.CurrentUsage;
                gpuMemoryBudgetBytes_ = memory.Budget;
            }
        }

        if (!gpuUsageQuery_ || !gpuUsageCounter_) {
            return;
        }
        if (PdhCollectQueryData(gpuUsageQuery_) != ERROR_SUCCESS) {
            return;
        }

        DWORD bytes = 0;
        DWORD items = 0;
        PDH_STATUS status = PdhGetFormattedCounterArrayW(
            gpuUsageCounter_, PDH_FMT_DOUBLE, &bytes, &items, nullptr);
        if (status != PDH_MORE_DATA || bytes == 0) {
            return;
        }
        std::vector<BYTE> storage(bytes);
        auto* values = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(storage.data());
        status = PdhGetFormattedCounterArrayW(
            gpuUsageCounter_, PDH_FMT_DOUBLE, &bytes, &items, values);
        if (status != ERROR_SUCCESS) {
            return;
        }

        const DWORD processId = GetCurrentProcessId();
        const LUID expectedLuid = ActiveGpuLuid();
        double utilization = 0.0;
        double systemGpuTotal = 0.0;
        bool found = false;
        for (DWORD index = 0; index < items; ++index) {
            const auto& value = values[index];
            if (value.FmtValue.CStatus != ERROR_SUCCESS || !value.szName) {
                continue;
            }
            unsigned long pid = 0;
            unsigned long high = 0;
            unsigned long low = 0;
            if (swscanf_s(
                    value.szName,
                    L"pid_%lu_luid_0x%lx_0x%lx",
                    &pid,
                    &high,
                    &low) != 3) {
                continue;
            }
            if (static_cast<DWORD>(high) == static_cast<DWORD>(expectedLuid.HighPart) &&
                static_cast<DWORD>(low) == expectedLuid.LowPart) {
                systemGpuTotal += value.FmtValue.doubleValue;
                if (pid == processId) {
                    utilization += value.FmtValue.doubleValue;
                    found = true;
                }
            }
        }
        gpuUsagePercent_ = found
            ? std::optional<double>((std::clamp)(utilization, 0.0, 100.0))
            : std::nullopt;
        systemGpuPercent_ = (std::clamp)(systemGpuTotal, 0.0, 100.0);
    }

    LUID ActiveGpuLuid() const {
        const auto adapter = std::find_if(
            gpuAdapters_.begin(), gpuAdapters_.end(),
            [this](const GpuAdapterInfo& info) {
                return static_cast<int>(info.index) == activeGpuIndex_;
            });
        return adapter == gpuAdapters_.end() ? LUID{} : adapter->luid;
    }

    void PrintStats() {
        const auto now = Clock::now();
        const double seconds = std::chrono::duration<double>(now - statsStarted_).count();
        if (seconds < 2.0) {
            return;
        }

        UpdateGpuTelemetry();
        if (const auto configuredFps = ReadVtsConfiguredFps(monitorRefreshFps_)) {
            if (vtsConfiguredFps_ != configuredFps->fps ||
                vtsConfiguredMode_ != configuredFps->mode) {
                vtsConfiguredFps_ = configuredFps->fps;
                vtsConfiguredMode_ = configuredFps->mode;
                if (fpsMode_ == FpsMode::FollowVts || fpsMode_ == FpsMode::FollowVtsApi) {
                    resetFrameSchedule_ = true;
                }
                if (toolbarHwnd_ && !locked_) {
                    InvalidateRect(toolbarHwnd_, nullptr, FALSE);
                }
            }
        }
        const int prevApiFps = vtsApiFps_;
        vtsApiFps_ = vtsApi_.GetRealtimeFps();
        const bool apiNotificationVisible = ShowApiNotification();
        if (apiNotificationVisible != lastApiNotificationVisible_) {
            lastApiNotificationVisible_ = apiNotificationVisible;
            PositionToolbar();
            if (toolbarHwnd_) {
                InvalidateRect(toolbarHwnd_, nullptr, FALSE);
            }
        }
        DetectOpaqueBackground();
        if (vtsApiFps_ > 0 && prevApiFps == 0) {
            apiWasConnected_ = true;
            if (awaitingUserApproval_) {
                awaitingUserApproval_ = false;
                showingApiSuccess_ = true;
                SetTimer(toolbarHwnd_, 47, 4000, nullptr);
                PositionToolbar();
            }
            if (fpsMode_ == FpsMode::FollowVts) {
                fpsMode_ = FpsMode::FollowVtsApi;
                SaveUiSettings();
                resetFrameSchedule_ = true;
            }
        }
        if (vtsApiFps_ == 0 && prevApiFps > 0) {
            if (fpsMode_ == FpsMode::FollowVtsApi) {
                fpsMode_ = FpsMode::FollowVts;
                SaveUiSettings();
                resetFrameSchedule_ = true;
            }
            awaitingUserApproval_ = false;
            apiNotificationDismissed_ = false;
            PositionToolbar();
        }
        // Spout returns the monitor refresh rate as a fallback when the
        // sender has not enabled frame counting. Never present that fallback
        // as the VTS sender FPS.
        if (receiver_.IsFrameCountEnabled() && receiver_.GetSenderFrame() > 0) {
            const double senderFps = receiver_.GetSenderFps();
            senderFps_ = senderFps > 0.0 ? senderFps : 0.0;
        } else {
            senderFps_ = 0.0;
        }

        const double requestFps = requested_ / seconds;
        const double receiveFps = received_ / seconds;
        const double newFps = newFrames_ / seconds;
        const double updateFps = updated_ / seconds;

        // Perf line no longer written to disk — data still surfaces in the
        // in-app debug panel via the last* fields below.

        lastRequestFps_ = requestFps;
        lastReceiveFps_ = receiveFps;
        lastNewFps_ = newFps;
        lastUpdateFps_ = updateFps;
        lastReceiveMs_ = received_ ? receiveMs_ / received_ : 0.0;
        lastMapScaleMs_ = newFrames_ ? mapMs_ / newFrames_ : 0.0;
        lastUpdateMs_ = updated_ ? updateMs_ / updated_ : 0.0;
        debugFpsHistory_.push_back(lastUpdateFps_);
        debugFrameMsHistory_.push_back(
            lastReceiveMs_ + lastMapScaleMs_ + lastUpdateMs_);
        constexpr size_t kDebugHistoryLength = 120;
        if (debugFpsHistory_.size() > kDebugHistoryLength) {
            debugFpsHistory_.erase(debugFpsHistory_.begin());
        }
        if (debugFrameMsHistory_.size() > kDebugHistoryLength) {
            debugFrameMsHistory_.erase(debugFrameMsHistory_.begin());
        }
        debugCacheDirty_ = true;
        if (toolbarHwnd_ && !locked_) {
            InvalidateRect(toolbarHwnd_, nullptr, FALSE);
        }

        statsStarted_ = now;
        requested_ = received_ = newFrames_ = updated_ = errors_ = 0;
        receiveMs_ = mapMs_ = updateMs_ = maxUpdateMs_ = 0.0;
        wakeLateMs_ = maxWakeLateMs_ = 0.0;
    }

    bool ToolbarUsesCompactLayout() const {
        if (locked_ || !hwnd_ || !IsWindow(hwnd_)) {
            return false;
        }
        RECT overlay{};
        return GetWindowRect(hwnd_, &overlay)
            && overlay.right - overlay.left < kToolbarWideMinimumWidth;
    }

    int ToolbarControlsHeight() const {
        int h = ToolbarUsesCompactLayout() ? kToolbarHeight * 2 : kToolbarHeight;
        h += TotalNotificationOffset();
        return h;
    }

    bool ShowApiNotification() const {
        if (locked_) return false;
        if (!hasReceivedModel_) return false;
        if (!apiStartedAfterModel_) return false;
        if (holdNotificationForScanResult_) return true;
        if (awaitingUserApproval_ || vtsApi_.IsAwaitingAuthorization()) return true;
        if (showingApiSuccess_) return true;
        if (!vtsApi_.IsInitialDiscoveryDone()) return false;
        // Only show the API hint after a real model frame has been received.
        // On a new computer, or after VTS/API disconnects, this is the
        // reliable way to tell the user that VTS API access needs attention.
        return !vtsApi_.IsConnected();
    }

    int TotalNotificationOffset() const {
        return ShowApiNotification() ? kApiNotificationHeight : 0;
    }

    void CheckGpuWarning() {
        if (gpuWarningPermanentlyDismissed_ || gpuWarningShown_) return;
        if (!hasReceivedModel_) return;
        if (activeGpuIndex_ < 0) return;
        const bool onlyOneGpu = gpuAdapters_.size() <= 1;
        const bool runningOnHighPerf = minimumPowerGpuIndex_ >= 0 &&
                                       activeGpuIndex_ != minimumPowerGpuIndex_;
        if (onlyOneGpu || runningOnHighPerf) {
            gpuWarningShown_ = true;
            gpuWarningStart_ = Clock::now();
            if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }

    void DetectOpaqueBackground() {
        CheckGpuWarning();
        if (bgWarningPermanentlyDismissed_) {
            opaqueBackgroundDetected_ = false;
            return;
        }
        if (!dibBits_ || dibWidth_ < 80 || dibHeight_ < 80 || sourceWidth_ == 0) {
            opaqueFrameCount_ = 0;
            opaqueBackgroundDetected_ = false;
            return;
        }
        auto* pixels = static_cast<const std::uint8_t*>(dibBits_);
        const int w = dibWidth_;
        const int h = dibHeight_;
        const int stride = w * 4;
        auto alphaAt = [&](int x, int y) -> uint8_t {
            return pixels[y * stride + x * 4 + 3];
        };
        // For each of the 4 corners, scan the 50x50 pixel region.
        // If ANY pixel there is transparent (alpha < 250), that corner is
        // considered transparent. If ALL 4 corners have their regions fully
        // opaque, we consider the background non-transparent.
        const int region = (std::min)(50, (std::min)(w, h) / 4);
        const int corners[4][2] = {
            {0, 0}, {w - region, 0},
            {0, h - region}, {w - region, h - region}
        };
        int opaqueCorners = 0;
        for (int c = 0; c < 4; ++c) {
            const int x0 = corners[c][0];
            const int y0 = corners[c][1];
            bool cornerFullyOpaque = true;
            for (int y = 0; y < region && cornerFullyOpaque; y += 4) {
                for (int x = 0; x < region; x += 4) {
                    if (alphaAt(x0 + x, y0 + y) < 250) {
                        cornerFullyOpaque = false;
                        break;
                    }
                }
            }
            if (cornerFullyOpaque) ++opaqueCorners;
        }
        // Only trigger when all 4 corner regions are fully opaque
        if (opaqueCorners == 4) {
            ++opaqueFrameCount_;
        } else {
            opaqueFrameCount_ = 0;
        }
        const bool wasDetected = opaqueBackgroundDetected_;
        opaqueBackgroundDetected_ = opaqueFrameCount_ >= 5;
        if (wasDetected != opaqueBackgroundDetected_ && hwnd_ && !locked_) {
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }

    int ToolbarCurrentHeight() const {
        if (locked_) return kToolbarLockedHeight;
        int height;
        if (ToolbarUsesCompactLayout()) {
            height = kToolbarCompactHeight;
        } else if (debugMode_) {
            height = kToolbarDebugModeHeight;
        } else {
            height = kToolbarDebugHeight;
        }
        height += TotalNotificationOffset();
        return height;
    }

    RECT ToolbarButtonRect(ToolbarButton button) const {
        RECT client{};
        GetClientRect(toolbarHwnd_, &client);
        if (locked_) {
            return button == ToolbarButton::Unlock
                ? RECT{
                    1, 1,
                    client.right - 1 -
                        (debugMode_ ? kLockedDebugLabelWidth : 0),
                    client.bottom - 1 }
                : RECT{};
        }
        const int notifOffset = TotalNotificationOffset();
        const auto take = [notifOffset](int& right, int width, int rowTop) {
            RECT rect{
                right - width,
                notifOffset + rowTop + 5,
                right,
                notifOffset + rowTop + kToolbarHeight - 5 };
            right = rect.left - 4;
            return rect;
        };
        RECT close{};
        RECT lock{};
        RECT github{};
        RECT debug{};
        RECT border{};
        RECT aspect{};
        RECT frameRate{};
        RECT quality{};
        RECT hotkey{};
        RECT monitor{};
        RECT reset{};
        RECT hide{};
        RECT gpu{};
        RECT graphicsSettings{};
        if (ToolbarUsesCompactLayout()) {
            constexpr int gap = 4;
            // Row 1: [graphics] [hotkey] on left, [close][hide][github][lock] on right
            int row1Right = client.right - 5;
            close = take(row1Right, 40, 0);
            hide = take(row1Right, 40, 0);
            github = take(row1Right, 40, 0);
            lock = take(row1Right, 60, 0);
            graphicsSettings = RECT{
                5, notifOffset + 5, 5 + 100, notifOffset + kToolbarHeight - 5 };
            {
                int hkLeft = 5 + 100 + gap;
                int hkRight = lock.left - gap;
                if (hkRight - hkLeft > 60) {
                    hotkey = RECT{ hkLeft, notifOffset + 5, hkRight,
                        notifOffset + kToolbarHeight - 5 };
                }
            }
            // Row 2: [reset] [monitor] [aspect] [border] [debug] centered
            constexpr int row2Width = 86 + 86 + 88 + 88 + 64 + gap * 4;
            const int clientWidth = static_cast<int>(client.right);
            int cursor = (std::max)(5, (clientWidth - row2Width) / 2);
            const auto place = [&cursor, gap, notifOffset](int width) {
                RECT rect{
                    cursor,
                    notifOffset + kToolbarHeight + 5,
                    cursor + width,
                    notifOffset + kToolbarHeight * 2 - 5 };
                cursor = rect.right + gap;
                return rect;
            };
            reset = place(86);
            monitor = place(86);
            aspect = place(88);
            border = place(88);
            debug = place(64);
        } else {
            // Right side (fixed order): close, hide, github, lock, debug
            int right = client.right - 5;
            close = take(right, 46, 0);
            hide = take(right, 46, 0);
            github = take(right, 46, 0);
            lock = take(right, 60, 0);
            debug = take(right, 64, 0);
            // Left side (left-to-right): graphics, reset, monitor, hotkey, aspect, border
            constexpr int gap = 4;
            const auto placeLeft = [notifOffset, gap](int& cursor, int width) {
                RECT r{ cursor, notifOffset + 5,
                        cursor + width, notifOffset + kToolbarHeight - 5 };
                cursor += width + gap;
                return r;
            };
            int cursor = 5;
            graphicsSettings = placeLeft(cursor, 100);
            reset = placeLeft(cursor, 86);
            monitor = placeLeft(cursor, 86);
            hotkey = placeLeft(cursor, 220);
            aspect = placeLeft(cursor, 88);
            border = placeLeft(cursor, 88);
        }
        switch (button) {
        case ToolbarButton::GraphicsSettings: return graphicsSettings;
        case ToolbarButton::Gpu: return gpu;
        case ToolbarButton::Quality: return quality;
        case ToolbarButton::FrameRate: return frameRate;
        case ToolbarButton::Aspect: return aspect;
        case ToolbarButton::Border: return border;
        case ToolbarButton::Reset: return reset;
        case ToolbarButton::Monitor: return monitor;
        case ToolbarButton::Hotkey: return hotkey;
        case ToolbarButton::Debug: return debug;
        case ToolbarButton::Lock: return lock;
        case ToolbarButton::Github: return github;
        case ToolbarButton::Hide: return hide;
        case ToolbarButton::Close: return close;
        case ToolbarButton::Unlock:
            return locked_ ? RECT{ 1, 1, client.right - 1, client.bottom - 1 } : RECT{};
        default: return RECT{};
        }
    }

    ToolbarButton HitTestToolbar(POINT point) const {
        if (locked_) {
            RECT unlock = ToolbarButtonRect(ToolbarButton::Unlock);
            return PtInRect(&unlock, point) ? ToolbarButton::Unlock : ToolbarButton::None;
        }
        constexpr ToolbarButton buttons[] = {
            ToolbarButton::GraphicsSettings,
            ToolbarButton::Aspect,
            ToolbarButton::Border,
            ToolbarButton::Reset,
            ToolbarButton::Monitor,
            ToolbarButton::Hotkey,
            ToolbarButton::Debug,
            ToolbarButton::Lock,
            ToolbarButton::Github,
            ToolbarButton::Hide,
            ToolbarButton::Close,
        };
        for (ToolbarButton button : buttons) {
            RECT rect = ToolbarButtonRect(button);
            if (PtInRect(&rect, point)) {
                return button;
            }
        }
        return ToolbarButton::None;
    }

    void PaintToolbar() {
        PAINTSTRUCT paint{};
        HDC target = BeginPaint(toolbarHwnd_, &paint);
        RECT client{};
        GetClientRect(toolbarHwnd_, &client);
        HDC buffer = CreateCompatibleDC(target);
        HBITMAP bitmap = CreateCompatibleBitmap(
            target, client.right - client.left, client.bottom - client.top);
        HGDIOBJ oldBitmap = SelectObject(buffer, bitmap);

        HBRUSH background = CreateSolidBrush(RGB(19, 29, 45));
        FillRect(buffer, &client, background);
        DeleteObject(background);
        HBRUSH border = CreateSolidBrush(RGB(24, 132, 255));
        FrameRect(buffer, &client, border);
        DeleteObject(border);

        int notifY = 1;
        if (ShowApiNotification()) {
            const bool awaitingAuthorization =
                awaitingUserApproval_ || vtsApi_.IsAwaitingAuthorization();
            RECT notifRect{ 1, notifY, client.right - 1, notifY + kApiNotificationHeight };
            HBRUSH notifBg = CreateSolidBrush(
                (awaitingAuthorization || showingApiSuccess_) ? RGB(15, 50, 22) : RGB(50, 30, 10));
            FillRect(buffer, &notifRect, notifBg);
            DeleteObject(notifBg);
            HFONT notifFont = CreateFontW(
                -15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HGDIOBJ oldNotifFont = SelectObject(buffer, notifFont);
            SetBkMode(buffer, TRANSPARENT);
            SetTextColor(buffer,
                (awaitingAuthorization || showingApiSuccess_) ? RGB(130, 255, 150) : RGB(255, 200, 100));
            const bool disconnected = apiWasConnected_ && !vtsApi_.IsConnected();
            std::wstring notifBuf;
            const wchar_t* notifText;
            if (showingApiSuccess_) {
                notifText = L"添加成功！";
            } else if (awaitingAuthorization) {
                const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                    Clock::now().time_since_epoch()).count();
                const int dotCount = static_cast<int>(seconds % 4);
                notifBuf = L"等待 VTube Studio 授权插件连接中，请打开 VTS 插件进行授权";
                for (int i = 0; i < dotCount; ++i) notifBuf += L".";
                notifText = notifBuf.c_str();
            } else if (disconnected) {
                notifText = L"VTubeStudio API 连接已断开，无法同步实时渲染帧数，请点击右侧“如何开启”";
            } else {
                notifText = L"尚未获取 VTubeStudio API 授权，请点击右侧“如何开启”";
            }
            if (awaitingAuthorization || showingApiSuccess_) {
                RECT textRect{ 0, notifY, client.right, notifY + kApiNotificationHeight };
                DrawTextW(buffer, notifText, -1, &textRect,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } else {
                const int textRight = vtsApi_.IsScanning() ? client.right - 280 : client.right - 190;
                RECT textRect{ 8, notifY, textRight, notifY + kApiNotificationHeight };
                DrawTextW(buffer, notifText, -1, &textRect,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                const int scanResult = vtsApi_.GetScanResult();
                if (vtsApi_.IsScanning()) {
                    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                        Clock::now().time_since_epoch()).count();
                    const int dotCount = static_cast<int>(seconds % 4);
                    std::wstring scanText = L"扫描中";
                    for (int i = 0; i < dotCount; ++i) scanText += L".";
                    SetTextColor(buffer, RGB(255, 220, 130));
                    RECT scanRect{ client.right - 275, notifY, client.right - 190, notifY + kApiNotificationHeight };
                    DrawTextW(buffer, scanText.c_str(), -1, &scanRect,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                } else if (scanResult == 1) {
                    SetTextColor(buffer, RGB(130, 255, 150));
                    RECT scanRect{ client.right - 275, notifY, client.right - 190, notifY + kApiNotificationHeight };
                    DrawTextW(buffer, L"扫描成功", -1, &scanRect,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                } else if (scanResult == 2) {
                    SetTextColor(buffer, RGB(255, 130, 130));
                    RECT scanRect{ client.right - 275, notifY, client.right - 190, notifY + kApiNotificationHeight };
                    DrawTextW(buffer, L"获取失败", -1, &scanRect,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
                SetTextColor(buffer, RGB(100, 180, 255));
                RECT scanBtnRect{ client.right - 185, notifY, client.right - 95, notifY + kApiNotificationHeight };
                DrawTextW(buffer, L"[扫描端口]", -1, &scanBtnRect,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                RECT howRect{ client.right - 90, notifY, client.right - 5, notifY + kApiNotificationHeight };
                DrawTextW(buffer, L"[如何开启]", -1, &howRect,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            SelectObject(buffer, oldNotifFont);
            DeleteObject(notifFont);
        }

        HFONT font = CreateFontW(
            -15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT closeFont = CreateFontW(
            -25, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
        HGDIOBJ oldFont = SelectObject(buffer, font);
        SetBkMode(buffer, TRANSPARENT);
        SetTextColor(buffer, RGB(230, 239, 252));

        const auto drawButton = [&](ToolbarButton button, const std::wstring& label) {
            RECT rect = ToolbarButtonRect(button);
            COLORREF color = button == ToolbarButton::Lock
                ? RGB(33, 49, 72)
                : RGB(20, 36, 58);
            if (button == toolbarHovered_) {
                if (button == ToolbarButton::Close) {
                    color = RGB(205, 55, 62);
                } else if (button == ToolbarButton::Lock) {
                    color = RGB(41, 91, 151);
                } else {
                    color = RGB(26, 70, 116);
                }
            }
            if (button == toolbarPressed_) {
                if (button == ToolbarButton::Close) {
                    color = RGB(170, 35, 42);
                } else if (button == ToolbarButton::Lock) {
                    color = RGB(29, 72, 123);
                } else {
                    color = RGB(18, 50, 88);
                }
            }
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(buffer, &rect, brush);
            DeleteObject(brush);
            SetTextColor(buffer, RGB(240, 246, 255));
            HGDIOBJ previousFont = nullptr;
            if (button == ToolbarButton::Close) {
                previousFont = SelectObject(buffer, closeFont);
            }
            DrawTextW(buffer, label.c_str(), -1, &rect,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (previousFont) {
                SelectObject(buffer, previousFont);
            }
        };

        const auto drawGithubButton = [&]() {
            drawButton(ToolbarButton::Github, L"");
            if (!githubIcon_) {
                return;
            }
            const RECT rect = ToolbarButtonRect(ToolbarButton::Github);
            // The GitHub button is 32 logical pixels high.  Rendering the
            // 32px resource at its native size avoids an extra HICON
            // downsample (which made the white circle and cat look jagged).
            const int iconWidth = 32;
            const int iconHeight = 32;
            DrawIconEx(
                buffer,
                rect.left + (rect.right - rect.left - iconWidth) / 2,
                rect.top + (rect.bottom - rect.top - iconHeight) / 2,
                githubIcon_, iconWidth, iconHeight, 0, nullptr, DI_NORMAL);
        };

        if (locked_) {
            drawButton(ToolbarButton::Unlock, L"解锁");
            if (debugMode_) {
                RECT debugLabel{
                    client.right - kLockedDebugLabelWidth,
                    1,
                    client.right - 1,
                    client.bottom - 1 };
                SetTextColor(buffer, RGB(157, 210, 255));
                DrawTextW(
                    buffer, L"调试模式已打开", -1, &debugLabel,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        } else {
            drawButton(ToolbarButton::GraphicsSettings, L"图形设置");
            drawButton(ToolbarButton::Reset, L"主屏居中");
            drawButton(ToolbarButton::Monitor, L"切换屏幕");
            drawButton(
                ToolbarButton::Hotkey,
                capturingHotkey_
                    ? L"请按组合键（Esc取消）"
                    : L"锁定/解锁 " + HotkeyText());
            drawButton(
                ToolbarButton::Aspect,
                aspectLocked_ ? L"比例 锁定" : L"比例 自由");
            drawButton(ToolbarButton::Border, L"个性化");
            drawButton(
                ToolbarButton::Debug,
                L"调试");
            drawButton(ToolbarButton::Lock, L"完成");
            drawGithubButton();
            drawButton(ToolbarButton::Hide, L"—");
            drawButton(ToolbarButton::Close, L"×");
        }

        if (!locked_) {
            const bool compact = ToolbarUsesCompactLayout();
            HFONT debugFont = CreateFontW(
                -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            SelectObject(buffer, debugFont);
            SetTextColor(
                buffer,
                capturingFps_ ? RGB(255, 214, 102) : RGB(157, 210, 255));
            RECT debugRect{
                8, ToolbarControlsHeight(), client.right - 8, client.bottom - 2 };
            const std::wstring status = capturingFps_
                ? L"直接输入 1–240；Enter 确认，Esc 取消（确认前不会修改当前帧率）"
                : DebugStatusText(compact);
            DrawTextW(buffer, status.c_str(), -1, &debugRect,
                      DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
            SelectObject(buffer, font);
            DeleteObject(debugFont);
        }

        BitBlt(
            target, 0, 0, client.right - client.left, client.bottom - client.top,
            buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, oldFont);
        SelectObject(buffer, oldBitmap);
        DeleteObject(closeFont);
        DeleteObject(font);
        DeleteObject(bitmap);
        DeleteDC(buffer);
        EndPaint(toolbarHwnd_, &paint);
    }

    void BeginHotkeyCapture() {
        if (capturingHotkey_) {
            return;
        }
        if (hotkeyRegistered_) {
            UnregisterHotKey(hwnd_, kHotkeyId);
            hotkeyRegistered_ = false;
        }
        capturingHotkey_ = true;
        SetForegroundWindow(toolbarHwnd_);
        SetFocus(toolbarHwnd_);
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }

    void CancelHotkeyCapture() {
        if (!capturingHotkey_) {
            return;
        }
        capturingHotkey_ = false;
        RegisterConfiguredHotkey(false);
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }

    void CaptureHotkey(UINT virtualKey) {
        if (virtualKey == VK_ESCAPE) {
            CancelHotkeyCapture();
            return;
        }
        if (virtualKey == VK_CONTROL || virtualKey == VK_LCONTROL || virtualKey == VK_RCONTROL ||
            virtualKey == VK_MENU || virtualKey == VK_LMENU || virtualKey == VK_RMENU ||
            virtualKey == VK_SHIFT || virtualKey == VK_LSHIFT || virtualKey == VK_RSHIFT ||
            virtualKey == VK_LWIN || virtualKey == VK_RWIN) {
            return;
        }

        UINT modifiers = 0;
        if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
        if (GetKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;
        if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
        if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) modifiers |= MOD_WIN;
        if (!modifiers) {
            MessageBeep(MB_ICONWARNING);
            return;
        }

        const UINT previousModifiers = hotkeyModifiers_;
        const UINT previousKey = hotkeyVk_;
        hotkeyModifiers_ = modifiers;
        hotkeyVk_ = virtualKey;
        if (!RegisterConfiguredHotkey(false)) {
            hotkeyModifiers_ = previousModifiers;
            hotkeyVk_ = previousKey;
            RegisterConfiguredHotkey(false);
            MessageBoxW(
                toolbarHwnd_,
                L"这个快捷键已被其他程序占用，请换一个组合。",
                L"VTSFloat_Meow",
                MB_OK | MB_ICONWARNING);
        } else {
            SaveHotkeySettings();
            Log("[layered] hotkey=" + WideToUtf8(HotkeyText().c_str()));
        }
        capturingHotkey_ = false;
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }

    void ActivateToolbarButton(ToolbarButton button) {
        switch (button) {
        case ToolbarButton::GraphicsSettings:
            CancelFpsCapture();
            Log("[toolbar] action=graphics_settings");
            ShowGraphicsSettingsMenu();
            break;
        case ToolbarButton::Gpu:
            CancelFpsCapture();
            Log("[toolbar] action=gpu_menu");
            ShowGpuMenu();
            break;
        case ToolbarButton::Quality:
            CancelFpsCapture();
            Log("[toolbar] action=scaling_quality_menu");
            ShowScalingQualityMenu();
            break;
        case ToolbarButton::FrameRate:
            Log("[toolbar] action=fps_menu");
            CancelFpsCapture();
            ShowFpsMenu();
            break;
        case ToolbarButton::Aspect:
            CancelFpsCapture();
            ToggleAspectLock();
            break;
        case ToolbarButton::Border:
            CancelFpsCapture();
            CancelHotkeyCapture();
            Log("[toolbar] action=border_settings");
            ShowBorderSettingsDialog();
            break;
        case ToolbarButton::Reset:
            CancelFpsCapture();
            Log("[toolbar] action=center_primary");
            ResetToPrimaryMonitor();
            break;
        case ToolbarButton::Monitor:
            CancelFpsCapture();
            Log("[toolbar] action=next_monitor");
            MoveToNextMonitor();
            break;
        case ToolbarButton::Hotkey:
            CancelFpsCapture();
            Log("[toolbar] action=capture_hotkey");
            if (capturingHotkey_) {
                CancelHotkeyCapture();
            } else {
                BeginHotkeyCapture();
            }
            break;
        case ToolbarButton::Debug:
            CancelFpsCapture();
            Log("[toolbar] action=debug_menu");
            ShowDebugMenu();
            break;
        case ToolbarButton::Lock:
            Log("[toolbar] action=lock");
            CancelFpsCapture();
            CancelHotkeyCapture();
            SetLocked(true);
            break;
        case ToolbarButton::Github:
            Log("[toolbar] action=github");
            if (kGithubUrl[0]) {
                ShellExecuteW(nullptr, L"open", kGithubUrl, nullptr, nullptr, SW_SHOWNORMAL);
            }
            break;
        case ToolbarButton::Hide:
            Log("[toolbar] action=hide_to_tray");
            CancelFpsCapture();
            CancelHotkeyCapture();
            SetOverlayVisible(false);
            break;
        case ToolbarButton::Close:
            Log("[toolbar] action=close");
            CancelFpsCapture();
            CancelHotkeyCapture();
            SendMessageW(hwnd_, WM_CLOSE, 0, 0);
            break;
        case ToolbarButton::Unlock:
            Log("[toolbar] action=unlock");
            CancelFpsCapture();
            SetLocked(false);
            break;
        default:
            break;
        }
    }

    LRESULT OnToolbarMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_PAINT:
            PaintToolbar();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE: {
            POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const ToolbarButton hovered = HitTestToolbar(point);
            if (hovered != toolbarHovered_) {
                toolbarHovered_ = hovered;
                InvalidateRect(toolbarHwnd_, nullptr, FALSE);
            }
            TRACKMOUSEEVENT tracking{};
            tracking.cbSize = sizeof(tracking);
            tracking.dwFlags = TME_LEAVE;
            tracking.hwndTrack = toolbarHwnd_;
            TrackMouseEvent(&tracking);
            return 0;
        }
        case WM_MOUSELEAVE: {
            toolbarHovered_ = ToolbarButton::None;
            InvalidateRect(toolbarHwnd_, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            SetForegroundWindow(toolbarHwnd_);
            SetFocus(toolbarHwnd_);
            POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (ShowApiNotification() && point.y < kApiNotificationHeight) {
                if (awaitingUserApproval_ || vtsApi_.IsAwaitingAuthorization()) {
                    return 0;
                }
                RECT clientR{};
                GetClientRect(toolbarHwnd_, &clientR);
                if (point.x >= clientR.right - 90) {
                    ShowVtsApiGuide();
                } else if (point.x >= clientR.right - 185 && point.x < clientR.right - 95) {
                    vtsApi_.RequestScan();
                    holdNotificationForScanResult_ = true;
                    scanStartedObserved_ = false;
                    SetTimer(toolbarHwnd_, 42, 300, nullptr);
                    InvalidateRect(toolbarHwnd_, nullptr, FALSE);
                }
                return 0;
            }
            toolbarPressed_ = HitTestToolbar(point);
            if (toolbarPressed_ != ToolbarButton::None) {
                SetCapture(toolbarHwnd_);
                InvalidateRect(toolbarHwnd_, nullptr, FALSE);
                return 0;
            }
            POINT screen{};
            GetCursorPos(&screen);
            ReleaseCapture();
            SendMessageW(
                hwnd_, WM_NCLBUTTONDOWN, HTCAPTION,
                MAKELPARAM(screen.x, screen.y));
            return 0;
        }
        case WM_LBUTTONUP: {
            POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const ToolbarButton released = HitTestToolbar(point);
            const ToolbarButton pressed = toolbarPressed_;
            toolbarPressed_ = ToolbarButton::None;
            if (GetCapture() == toolbarHwnd_) {
                ReleaseCapture();
            }
            InvalidateRect(toolbarHwnd_, nullptr, FALSE);
            if (pressed != ToolbarButton::None && pressed == released) {
                ActivateToolbarButton(pressed);
            }
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (capturingFps_) {
                CaptureFpsKey(static_cast<UINT>(wParam));
                return 0;
            }
            if (capturingHotkey_) {
                CaptureHotkey(static_cast<UINT>(wParam));
                return 0;
            }
            break;
        case WM_GETDLGCODE:
            if (capturingFps_ || capturingHotkey_) {
                return DLGC_WANTALLKEYS;
            }
            break;
        case WM_TIMER:
            if (wParam == 48) {
                KillTimer(toolbarHwnd_, 48);
                apiStartScheduled_ = false;
                if (hasReceivedModel_ && !apiStartedAfterModel_) {
                    apiStartedAfterModel_ = true;
                    vtsApi_.Start(instance_);
                    InvalidateRect(toolbarHwnd_, nullptr, FALSE);
                }
                return 0;
            }
            if (wParam == 42) {
                InvalidateRect(toolbarHwnd_, nullptr, FALSE);
                if (vtsApi_.IsScanning()) {
                    scanStartedObserved_ = true;
                }
                if (scanStartedObserved_ && !vtsApi_.IsScanning()) {
                    KillTimer(toolbarHwnd_, 42);
                    scanStartedObserved_ = false;
                    Log("[vts-api] timer42 done result=" + std::to_string(vtsApi_.GetScanResult()));
                    if (vtsApi_.GetScanResult() == 1) {
                        SetTimer(toolbarHwnd_, 43, 1000, nullptr);
                    } else if (vtsApi_.GetScanResult() == 2) {
                        SetTimer(toolbarHwnd_, 44, 3000, nullptr);
                    } else {
                        holdNotificationForScanResult_ = false;
                        PositionToolbar();
                    }
                }
                return 0;
            }
            if (wParam == 43) {
                KillTimer(toolbarHwnd_, 43);
                vtsApi_.ClearScanResult();
                holdNotificationForScanResult_ = false;
                if (vtsApiFps_ == 0) {
                    awaitingUserApproval_ = true;
                    SetTimer(toolbarHwnd_, 45, 500, nullptr);  // dot animation
                    SetTimer(toolbarHwnd_, 46, 60000, nullptr);  // 60s timeout
                }
                PositionToolbar();
                InvalidateRect(toolbarHwnd_, nullptr, FALSE);
                return 0;
            }
            if (wParam == 45) {
                if (awaitingUserApproval_) {
                    if (vtsApiFps_ > 0) {
                        KillTimer(toolbarHwnd_, 45);
                        KillTimer(toolbarHwnd_, 46);
                        awaitingUserApproval_ = false;
                        PositionToolbar();
                    } else {
                        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
                    }
                } else {
                    KillTimer(toolbarHwnd_, 45);
                }
                return 0;
            }
            if (wParam == 46) {
                KillTimer(toolbarHwnd_, 45);
                KillTimer(toolbarHwnd_, 46);
                awaitingUserApproval_ = false;
                PositionToolbar();
                InvalidateRect(toolbarHwnd_, nullptr, FALSE);
                return 0;
            }
            if (wParam == 47) {
                KillTimer(toolbarHwnd_, 47);
                showingApiSuccess_ = false;
                PositionToolbar();
                InvalidateRect(toolbarHwnd_, nullptr, FALSE);
                return 0;
            }
            if (wParam == 44) {
                KillTimer(toolbarHwnd_, 44);
                vtsApi_.ClearScanResult();
                holdNotificationForScanResult_ = false;
                PositionToolbar();
                InvalidateRect(toolbarHwnd_, nullptr, FALSE);
                return 0;
            }
            break;
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                POINT point{};
                GetCursorPos(&point);
                ScreenToClient(toolbarHwnd_, &point);
                LPCWSTR cursor = IDC_SIZEALL;
                if (point.y < TotalNotificationOffset()) {
                    RECT clientR{};
                    GetClientRect(toolbarHwnd_, &clientR);
                    if (ShowApiNotification() && !awaitingUserApproval_ &&
                        !vtsApi_.IsAwaitingAuthorization() &&
                        point.x >= clientR.right - 185) {
                        cursor = IDC_HAND;
                    } else {
                        cursor = IDC_ARROW;
                    }
                } else if (HitTestToolbar(point) != ToolbarButton::None) {
                    cursor = IDC_HAND;
                }
                SetCursor(LoadCursorW(nullptr, cursor));
                return TRUE;
            }
            break;
        case WM_CLOSE:
            SetLocked(true);
            return 0;
        }
        return DefWindowProcW(toolbarHwnd_, message, wParam, lParam);
    }

    void SetLocked(bool locked) {
        if (locked && capturingHotkey_) {
            CancelHotkeyCapture();
        }
        if (locked && capturingFps_) {
            CancelFpsCapture();
        }
        if (locked && borderPanelHwnd_ && IsWindow(borderPanelHwnd_)) {
            // Save and close the panel when the model is locked.
            SendMessageW(borderPanelHwnd_, WM_COMMAND, IDOK, 0);
        }
        locked_ = locked;
        if (!locked_) {
            CancelHoverExpression();
        }
        if (!locked_ && !hoverOpacityPreviewActive_) {
            currentOverlayAlpha_ = 255;
            hoverFadeStartAlpha_ = 255;
            hoverTargetAlpha_ = 255;
            hoverFadeStarted_ = Clock::now();
        }
        ApplyClickThrough();
        Log(std::string("[layered] clickthrough=") + (locked_ ? "1" : "0"));
    }

    void ApplyClickThrough() {
        LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
        style |= WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
        const bool statusInteractive =
            statusMode_ != VtsStatusMode::Hidden && !statusDismissed_;
        if (locked_ && !statusInteractive) {
            style |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
        } else {
            style &= ~(static_cast<LONG_PTR>(WS_EX_TRANSPARENT) | WS_EX_NOACTIVATE);
        }
        SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, style);
        UINT overlayFlags =
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED;
        if (overlayVisible_) {
            overlayFlags |= SWP_SHOWWINDOW;
        }
        SetWindowPos(
            hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
            overlayFlags);
        if (toolbarHwnd_) {
            PositionToolbar();
            if (overlayVisible_ && (!locked_ || debugMode_)) {
                ShowWindow(toolbarHwnd_, SW_SHOWNOACTIVATE);
                SetWindowPos(
                    toolbarHwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                InvalidateRect(toolbarHwnd_, nullptr, FALSE);
            } else {
                ShowWindow(toolbarHwnd_, SW_HIDE);
                if (!overlayVisible_) {
                    ShowWindow(hwnd_, SW_HIDE);
                }
            }
        }
    }

    LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        if (taskbarCreatedMessage_ && message == taskbarCreatedMessage_) {
            trayIconAdded_ = false;
            AddTrayIcon();
            return 0;
        }
        switch (message) {
        case kSetLockedMessage:
            SetLocked(wParam != 0);
            return 0;
        case kInteractiveRenderMessage:
            interactiveFramePending_.store(false);
            if (interactiveRendering_.load(std::memory_order_relaxed)) {
                ++requested_;
                RenderFrame();
                PrintStats();
            }
            return 0;
        case kTrayCallbackMessage: {
            const UINT event = LOWORD(lParam);
            if (event == WM_CONTEXTMENU || event == WM_RBUTTONUP) {
                ShowTrayMenu();
            } else if (event == WM_LBUTTONDBLCLK) {
                ToggleOverlayVisibility();
            }
            return 0;
        }
        case WM_COMMAND: {
            const UINT command = LOWORD(wParam);
            if (command == kTrayResetCommand ||
                command == kTrayToggleVisibilityCommand ||
                command == kTrayExitCommand) {
                ActivateTrayCommand(command);
                return 0;
            }
            break;
        }
        case WM_HOTKEY:
            if (wParam == kHotkeyId) {
                SetLocked(!locked_);
                return 0;
            }
            break;
        case WM_SETCURSOR:
            {
                // Layered windows may report HTCAPTION/HTTRANSPARENT while
                // the pointer is over the rendered warning. Use the actual
                // screen position instead of requiring HTCLIENT so the two
                // warning actions always get a hand cursor.
                POINT p{};
                GetCursorPos(&p);
                ScreenToClient(hwnd_, &p);
                if ((opaqueBackgroundDetected_ && !bgWarningPermanentlyDismissed_ &&
                    (PtInRect(&bgWarningCloseRect_, p) ||
                     PtInRect(&bgWarningDontShowRect_, p))) ||
                    (gpuWarningShown_ && !gpuWarningPermanentlyDismissed_ &&
                     PtInRect(&gpuWarningDontShowRect_, p))) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }
            }
            break;
        case WM_LBUTTONDOWN:
            if (statusMode_ != VtsStatusMode::Hidden && !statusDismissed_) {
                POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                statusPressedButton_ = PtInRect(&statusExeRect_, point) ? 1
                    : (PtInRect(&statusBatchRect_, point) ? 2 : 0);
            if (statusPressedButton_ != 0) {
                SetCapture(hwnd_);
                statusFrameDirty_ = true;
                InvalidateRect(hwnd_, nullptr, FALSE);
                    return 0;
                }
            }
            if (opaqueBackgroundDetected_ && !bgWarningPermanentlyDismissed_) {
                POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                if (PtInRect(&bgWarningDontShowRect_, point)) {
                    bgWarningPermanentlyDismissed_ = true;
                    opaqueBackgroundDetected_ = false;
                    WriteConfigInt(L"warnings", L"suppress_bg_opaque", 1);
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    return 0;
                }
                if (PtInRect(&bgWarningCloseRect_, point)) {
                    opaqueBackgroundDetected_ = false;
                    opaqueFrameCount_ = 0;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    return 0;
                }
            }
            if (gpuWarningShown_ && !gpuWarningPermanentlyDismissed_) {
                POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                if (PtInRect(&gpuWarningDontShowRect_, point)) {
                    gpuWarningPermanentlyDismissed_ = true;
                    gpuWarningShown_ = false;
                    WriteConfigInt(L"warnings", L"suppress_gpu_high_perf", 1);
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    return 0;
                }
            }
            break;
        case WM_LBUTTONUP:
            if (statusPressedButton_ != 0) {
                POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                const int pressed = statusPressedButton_;
                statusPressedButton_ = 0;
                if (GetCapture() == hwnd_) ReleaseCapture();
                statusFrameDirty_ = true;
                InvalidateRect(hwnd_, nullptr, FALSE);
                if ((pressed == 1 && PtInRect(&statusExeRect_, point)) ||
                    (pressed == 2 && PtInRect(&statusBatchRect_, point))) {
                    LaunchVtsFromStatus(pressed == 2);
                }
                return 0;
            }
            break;
        case WM_NCHITTEST: {
            const bool statusInteractive =
                statusMode_ != VtsStatusMode::Hidden && !statusDismissed_;
            if (locked_ && !statusInteractive) {
                return HTTRANSPARENT;
            }
            POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd_, &point);
            if (statusInteractive) {
                if (PtInRect(&statusExeRect_, point) ||
                    PtInRect(&statusBatchRect_, point)) {
                    return HTCLIENT;
                }
                return HTTRANSPARENT;
            }
            if (!locked_ &&
                ((opaqueBackgroundDetected_ && !bgWarningPermanentlyDismissed_ &&
                  (PtInRect(&bgWarningCloseRect_, point) ||
                   PtInRect(&bgWarningDontShowRect_, point))) ||
                 (gpuWarningShown_ && !gpuWarningPermanentlyDismissed_ &&
                  PtInRect(&gpuWarningDontShowRect_, point)))) {
                return HTCLIENT;
            }
            RECT client{};
            GetClientRect(hwnd_, &client);
            const bool left = point.x < kResizeGrip;
            const bool right = point.x >= client.right - kResizeGrip;
            const bool top = point.y < kResizeGrip;
            const bool bottom = point.y >= client.bottom - kResizeGrip;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
            return HTCAPTION;
        }
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = kMinimumWidth;
            info->ptMinTrackSize.y = 96;
            return 0;
        }
        case WM_SIZING: {
            if (!aspectLocked_ || !sourceWidth_ || !sourceHeight_) {
                break;
            }
            auto* rect = reinterpret_cast<RECT*>(lParam);
            const double aspect = static_cast<double>(sourceWidth_) / sourceHeight_;
            int width = (std::max)(
                kMinimumWidth, static_cast<int>(rect->right - rect->left));
            int height = rect->bottom - rect->top;
            if (wParam == WMSZ_TOP || wParam == WMSZ_BOTTOM) {
                width = (std::max)(
                    kMinimumWidth, static_cast<int>(height * aspect + 0.5));
                height = static_cast<int>(width / aspect + 0.5);
            } else {
                height = static_cast<int>(width / aspect + 0.5);
            }
            if (wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT) {
                rect->left = rect->right - width;
            } else {
                rect->right = rect->left + width;
            }
            if (wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT) {
                rect->top = rect->bottom - height;
            } else {
                rect->bottom = rect->top + height;
            }
            return TRUE;
        }
        case WM_ENTERSIZEMOVE:
            StartInteractiveRendering();
            return 0;
        case WM_WINDOWPOSCHANGED:
            UpdateMonitorRefreshRate();
            PositionToolbar();
            PositionBorderPanel();
            break;
        case WM_DISPLAYCHANGE:
            UpdateMonitorRefreshRate(true);
            return 0;
        case WM_DPICHANGED:
            if (const auto* suggested = reinterpret_cast<RECT*>(lParam)) {
                SetWindowPos(
                    hwnd_, nullptr, suggested->left, suggested->top,
                    suggested->right - suggested->left,
                    suggested->bottom - suggested->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
        case WM_EXITSIZEMOVE:
            StopInteractiveRendering();
            SaveWindowPlacement();
            return 0;
        case WM_CLOSE:
            SaveWindowPlacement();
            DestroyWindow(hwnd_);
            return 0;
        case WM_DESTROY:
            RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        LayeredOverlay* overlay = reinterpret_cast<LayeredOverlay*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            overlay = static_cast<LayeredOverlay*>(create->lpCreateParams);
            overlay->hwnd_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
        }
        return overlay ? overlay->OnMessage(message, wParam, lParam)
                       : DefWindowProcW(hwnd, message, wParam, lParam);
    }

    static LRESULT CALLBACK ToolbarWindowProc(
        HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        LayeredOverlay* overlay = reinterpret_cast<LayeredOverlay*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            overlay = static_cast<LayeredOverlay*>(create->lpCreateParams);
            overlay->toolbarHwnd_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
        }
        return overlay ? overlay->OnToolbarMessage(message, wParam, lParam)
                       : DefWindowProcW(hwnd, message, wParam, lParam);
    }

    static LRESULT CALLBACK StatusWindowProc(
        HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        LayeredOverlay* overlay = reinterpret_cast<LayeredOverlay*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            overlay = static_cast<LayeredOverlay*>(create->lpCreateParams);
            overlay->statusHwnd_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
        }
        if (!overlay) return DefWindowProcW(hwnd, message, wParam, lParam);
        switch (message) {
        case WM_COMMAND:
            if (LOWORD(wParam) == kStatusLaunchExeCommand) {
                overlay->LaunchVtsFromStatus(false);
                return 0;
            }
            if (LOWORD(wParam) == kStatusLaunchBatchCommand) {
                overlay->LaunchVtsFromStatus(true);
                return 0;
            }
            break;
        case WM_CLOSE:
            overlay->statusDismissed_ = true;
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        }
        default:
            break;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND toolbarHwnd_ = nullptr;
    HWND statusHwnd_ = nullptr;
    HWND statusHeadline_ = nullptr;
    HWND statusInstructions_ = nullptr;
    HWND statusPath_ = nullptr;
    HWND statusLaunchExe_ = nullptr;
    HWND statusLaunchBatch_ = nullptr;
    RECT statusExeRect_{};
    RECT statusBatchRect_{};
    int statusPressedButton_ = 0;
    bool statusFrameDirty_ = true;
    int statusDrawWidth_ = 0;
    int statusDrawHeight_ = 0;
    HICON appIconLarge_ = nullptr;
    HICON appIconSmall_ = nullptr;
    HICON githubIcon_ = nullptr;
    HANDLE instanceMutex_ = nullptr;
    DWORD uiThreadId_ = 0;
    Clock::time_point lastVtsStatusCheck_{};
    VtsStatusMode statusMode_ = VtsStatusMode::Hidden;
    std::filesystem::path statusDirectory_;
    bool statusDismissed_ = false;
    bool hasReceivedModel_ = false;
    bool apiStartScheduled_ = false;
    bool apiStartedAfterModel_ = false;
    bool lastApiNotificationVisible_ = false;
    bool locked_ = true;
    bool initialAspectApplied_ = false;
    bool hotkeyRegistered_ = false;
    bool capturingHotkey_ = false;
    bool capturingFps_ = false;
    bool fpsReplaceOnNextDigit_ = false;
    bool debugMode_ = false;
    bool aspectLocked_ = true;
    bool hoverFadeEnabled_ = true;
    int hoverOpacityPercent_ = kDefaultHoverOpacityPercent;
    int modelOpacityPercent_ = 100;
    int hoverExpandPx_ = 0;
    bool hoverExpressionEnabled_ = false;
    std::string hoverExpressionFile_;
    bool hoverExpressionHovered_ = false;
    bool hoverExpressionRestored_ = false;
    bool hoverExpressionOriginalActive_ = false;
    Clock::time_point hoverExpressionRestoreAt_{};
    bool expressionPanelPreviewActive_ = false;
    bool expressionPanelPreviewOriginalActive_ = false;
    std::string expressionPanelPreviewFile_;
    bool borderDialogOpen_ = false;
    HWND borderPanelHwnd_ = nullptr;
    bool showHoverExpandPreview_ = false;
    bool hoverExpandEditing_ = false;
    Clock::time_point hoverExpandPreviewUntil_{};
    Clock::time_point hoverExpandFadeStarted_ = Clock::now();
    double hoverExpandPreviewAlpha_ = 0.0;
    bool hoverOpacityPreviewActive_ = false;
    int currentOverlayAlpha_ = 255;
    int hoverFadeStartAlpha_ = 255;
    int hoverTargetAlpha_ = 255;
    Clock::time_point hoverFadeStarted_ = Clock::now();
    std::vector<std::uint8_t> hoverPreviewBasePixels_;
    int hoverPreviewBaseWidth_ = 0;
    int hoverPreviewBaseHeight_ = 0;
    int borderMode_ = kBorderModeNormal;
    COLORREF customBorderColor_ = kDefaultCustomBorderColor;
    int borderThickness_ = kDefaultBorderThickness;
    bool gpuSelectionFallback_ = false;
    bool resetFrameSchedule_ = false;
    bool overlayVisible_ = true;
    bool trayIconAdded_ = false;
    std::atomic<bool> interactiveRendering_{ false };
    std::atomic<bool> interactiveFramePending_{ false };
    std::thread interactiveRenderer_;
    UINT hotkeyModifiers_ = MOD_CONTROL | MOD_SHIFT;
    UINT hotkeyVk_ = 'L';
    UINT taskbarCreatedMessage_ = 0;
    NOTIFYICONDATAW trayIcon_{};
    int targetFps_ = 60;
    FpsMode fpsMode_ = FpsMode::FollowVts;
    int scalingQuality_ = kScalingBalanced;
    int monitorRefreshFps_ = 60;
    int selectedGpuIndex_ = -1;
    int activeGpuIndex_ = -1;
    int senderGpuIndex_ = -1;
    int minimumPowerGpuIndex_ = -1;
    int highPerformanceGpuIndex_ = -1;
    std::wstring cpuModel_ = L"CPU";
    int vtsConfiguredFps_ = 0;
    std::wstring vtsConfiguredMode_;
    VtsApiClient vtsApi_;
    int vtsApiFps_ = 0;
    bool apiNotificationDismissed_ = false;
    bool apiWasConnected_ = false;
    bool opaqueBackgroundDetected_ = false;
    int opaqueFrameCount_ = 0;
    bool bgWarningPermanentlyDismissed_ = false;
    RECT bgWarningCloseRect_{};
    RECT bgWarningDontShowRect_{};
    bool gpuWarningPermanentlyDismissed_ = false;
    bool gpuWarningShown_ = false;
    Clock::time_point gpuWarningStart_{};
    RECT gpuWarningDontShowRect_{};
    bool holdNotificationForScanResult_ = false;
    bool scanStartedObserved_ = false;
    bool awaitingUserApproval_ = false;
    bool showingApiSuccess_ = false;
    ULONG_PTR gdiplusToken_ = 0;
    std::vector<GpuAdapterInfo> gpuAdapters_;
    std::wstring fpsInput_;
    ToolbarButton toolbarHovered_ = ToolbarButton::None;
    ToolbarButton toolbarPressed_ = ToolbarButton::None;

    spoutDX receiver_;
    bool spoutFrameSyncDisabled_ = false;
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGIAdapter3> activeAdapter3_;
    PDH_HQUERY gpuUsageQuery_ = nullptr;
    PDH_HCOUNTER gpuUsageCounter_ = nullptr;
    std::optional<double> gpuUsagePercent_;
    std::optional<double> cpuUsagePercent_;
    UINT64 gpuMemoryCurrentBytes_ = 0;
    UINT64 gpuMemoryBudgetBytes_ = 0;
    SIZE_T processMemoryBytes_ = 0;
    ULONGLONG previousKernelTicks_ = 0;
    ULONGLONG previousUserTicks_ = 0;
    Clock::time_point cpuSampleTime_{};
    bool cpuSampleInitialized_ = false;
    ULONGLONG prevSysIdle_ = 0;
    ULONGLONG prevSysKernel_ = 0;
    ULONGLONG prevSysUser_ = 0;
    double systemCpuPercent_ = 0.0;
    double systemMemoryMb_ = 0.0;
    double systemGpuPercent_ = 0.0;
    ComPtr<ID3D11Texture2D> staging_;
    ComPtr<ID3D11Texture2D> scaleInput_;
    ComPtr<ID3D11ShaderResourceView> scaleInputView_;
    ComPtr<ID3D11Texture2D> scaleOutput_;
    ComPtr<ID3D11RenderTargetView> scaleOutputView_;
    ComPtr<ID3D11Texture2D> scaleOutputReadback_;
    ComPtr<ID3D11VertexShader> scaleVertexShader_;
    ComPtr<ID3D11PixelShader> scalePixelShader_;
    ComPtr<ID3D11SamplerState> scaleSampler_;
    ComPtr<ID3D11SamplerState> scalePointSampler_;
    ComPtr<ID3D11Buffer> scaleSettingsBuffer_;
    ComPtr<ID3D11RasterizerState> scaleRasterizer_;
    bool gpuScalerUnavailable_ = false;
    UINT scaleSourceWidth_ = 0;
    UINT scaleSourceHeight_ = 0;
    DXGI_FORMAT scaleSourceFormat_ = DXGI_FORMAT_UNKNOWN;
    int scaleOutputWidth_ = 0;
    int scaleOutputHeight_ = 0;
    UINT sourceWidth_ = 0;
    UINT sourceHeight_ = 0;
    DXGI_FORMAT sourceFormat_ = DXGI_FORMAT_UNKNOWN;
    std::vector<BilinearSample> scaleMapX_;
    std::vector<BilinearSample> scaleMapY_;
    int scaleMapWidth_ = 0;
    int scaleMapHeight_ = 0;
    UINT scaleMapSourceWidth_ = 0;
    UINT scaleMapSourceHeight_ = 0;

    HDC memoryDc_ = nullptr;
    HBITMAP dib_ = nullptr;
    HGDIOBJ previousBitmap_ = nullptr;
    void* dibBits_ = nullptr;
    int dibWidth_ = 0;
    int dibHeight_ = 0;

    Clock::time_point statsStarted_{};
    std::uint64_t requested_ = 0;
    std::uint64_t received_ = 0;
    std::uint64_t newFrames_ = 0;
    std::uint64_t updated_ = 0;
    std::uint64_t errors_ = 0;
    double receiveMs_ = 0.0;
    double mapMs_ = 0.0;
    double updateMs_ = 0.0;
    double maxUpdateMs_ = 0.0;
    double wakeLateMs_ = 0.0;
    double maxWakeLateMs_ = 0.0;
    double lastRequestFps_ = 0.0;
    double lastReceiveFps_ = 0.0;
    double lastNewFps_ = 0.0;
    double lastUpdateFps_ = 0.0;
    double lastReceiveMs_ = 0.0;
    double lastMapScaleMs_ = 0.0;
    double lastUpdateMs_ = 0.0;
    double senderFps_ = 0.0;
    std::vector<double> debugFpsHistory_;
    std::vector<double> debugFrameMsHistory_;
    HDC debugCacheDc_ = nullptr;
    HBITMAP debugCacheBitmap_ = nullptr;
    int debugCacheWidth_ = 0;
    int debugCacheHeight_ = 0;
    bool debugCacheDirty_ = true;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int) {
    SetUnhandledExceptionFilter(&UnhandledExceptionHandler);
    AddVectoredExceptionHandler(0, &VectoredExceptionHandler);
    _set_purecall_handler(&PurecallHandler);
    _set_invalid_parameter_handler(&InvalidParamHandler);
    try {
        if (commandLine) {
            std::wistringstream arguments(commandLine);
            std::wstring mode;
            DWORD parentProcessId = 0;
            arguments >> mode >> parentProcessId;
            if (mode == L"--stop") {
                return StopRunningOverlay();
            }
            if (mode == L"--restart-after" && parentProcessId != 0) {
                return RunDelayedRestart(parentProcessId);
            }
        }
        LayeredOverlay overlay;
        return overlay.Run(instance);
    } catch (const std::exception& error) {
        Log(std::string("[layered fatal] ") + error.what());
        CopyLogToDesktop(std::string("std::exception: ") + error.what());
        MessageBoxA(nullptr, error.what(), "VTSFloat_Meow renderer", MB_ICONERROR | MB_OK);
        return 1;
    } catch (...) {
        Log("[layered fatal] unknown exception");
        CopyLogToDesktop("unknown C++ exception");
        return 1;
    }
}
