#include <windows.h>
#include <tlhelp32.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <avrt.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <dbghelp.h>
#include <winhttp.h>
#include <shlwapi.h>
#include <iphlpapi.h>
#include <objidl.h>
#include <roapi.h>
#include <gdiplus.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cctype>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "SpoutDX.h"
#include "cache_directory.h"
#include "localization.h"
#include "desktop_mode_policy.h"
#include "resource.h"
#include "update_version.h"
#include "updater.h"
#include "toolbar_text.h"
#include "image_output.h"
#include "image_backdrop.h"
#include "image_shadow.h"
#include "github_mark.h"

using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;
using vtsfloat::i18n::Tr;
using vtsfloat::i18n::UiFontFace;
using vtsfloat::i18n::UiLanguage;

namespace {

#ifdef VTSFLOAT_LANGUAGE_PREVIEW
constexpr wchar_t kWindowClass[] = L"LilyVtsLayeredOverlayLanguagePreview";
constexpr wchar_t kToolbarClass[] = L"LilyVtsLayeredOverlayToolbarLanguagePreview";
constexpr wchar_t kStatusClass[] = L"LilyVtsVtsStatusLanguagePreview";
constexpr wchar_t kSubjectSelectionClass[] = L"LilyVtsSubjectSelectionLanguagePreview";
constexpr wchar_t kSubjectSelectionToolbarClass[] = L"LilyVtsSubjectSelectionToolbarLanguagePreview";
constexpr wchar_t kWindowTitle[] = L"VTSFloat_Meow Language Preview";
constexpr wchar_t kToolbarTitle[] = L"VTSFloat_Meow Language Preview Controls";
constexpr wchar_t kInstanceName[] = L"Local\\LilyVtsLayeredOverlayLanguagePreviewInstance";
constexpr wchar_t kConfigFileName[] = L"vts_overlay_language_preview.ini";
#else
constexpr wchar_t kWindowClass[] = L"LilyVtsLayeredOverlay";
constexpr wchar_t kToolbarClass[] = L"LilyVtsLayeredOverlayToolbar";
constexpr wchar_t kStatusClass[] = L"LilyVtsVtsStatus";
constexpr wchar_t kSubjectSelectionClass[] = L"LilyVtsSubjectSelection";
constexpr wchar_t kSubjectSelectionToolbarClass[] = L"LilyVtsSubjectSelectionToolbar";
constexpr wchar_t kWindowTitle[] = L"VTSFloat_Meow";
constexpr wchar_t kToolbarTitle[] = L"VTSFloat_Meow Controls";
constexpr wchar_t kInstanceName[] = L"Local\\LilyVtsLayeredOverlayInstance";
constexpr wchar_t kConfigFileName[] = L"vts_overlay_layered.ini";
#endif
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
constexpr UINT kCheckUpdateCommand = 4104;
constexpr UINT kUpdateLaterCommand = 4105;
constexpr UINT kUpdateInstallCommand = 4106;
constexpr UINT kUpdateDismissCommand = 4107;
constexpr UINT kGithubDebugCommand = 4108;
constexpr UINT kUpdateStartupMessage = WM_APP + 7;
constexpr UINT kActivateExistingMessage = WM_APP + 8;
constexpr UINT_PTR kUpdateTimer = 68;
constexpr UINT_PTR kUpdateNoticeDismissTimer = 71;
constexpr UINT kUpdateNoticeTimeoutMs = 5000;
constexpr wchar_t kUpdateBannerClass[] = L"VTSFloatMeowUpdateBanner";
constexpr UINT_PTR kGithubDropdownTimer = 69;
constexpr wchar_t kGithubDropdownClass[] = L"VTSFloatMeowGithubDropdown";
constexpr wchar_t kToolbarHintClass[] = L"VTSFloatMeowToolbarHint";
constexpr COLORREF kToolbarPopoverBackground = RGB(19, 29, 45);
constexpr COLORREF kToolbarPopoverText = RGB(166, 198, 232);
constexpr wchar_t kDebugReadoutClass[] = L"VTSFloatMeowDebugReadout";
constexpr wchar_t kToolbarGuideClass[] = L"VTSFloatMeowToolbarGuide";
constexpr UINT_PTR kToolbarGuideTimer = 70;
constexpr UINT kGuideContinueCommand = 4110;
constexpr UINT kGuideCloseCommand = 4111;
constexpr wchar_t kCropReadoutClass[] = L"VTSFloatMeowCropReadout";
constexpr int kCropReadoutGap = 4;
constexpr int kCropReadoutRowHeight = 26;
constexpr int kCropReadoutFontHeight = 11;
constexpr UINT kCropWidthEdit = 4112;
constexpr UINT kCropHeightEdit = 4113;
constexpr UINT kFirstFeatherEdit = 4114;
constexpr wchar_t kFeatherPanelClass[] = L"LilyVtsFeatherPanel";
constexpr wchar_t kFeatherScrubClass[] = L"LilyVtsFeatherScrub";
constexpr int kFeatherScrubStep = 10;
constexpr int kFeatherScrubDragStep = 5;
constexpr int kFeatherScrubWidth = 226;
constexpr int kFeatherScrubHeight = 72;
constexpr int kDefaultWidth = 1800;
constexpr int kDefaultHeight = 860;
constexpr int kResizeGrip = 10;
constexpr LONG kCropExtent = 1000000;
constexpr RECT kFullCrop{0, 0, kCropExtent, kCropExtent};
constexpr int kMaximumFeatherPixels = 5000;
constexpr int kFeatherPanelWidth = 440;
constexpr int kFeatherPanelHeight = 463;
constexpr UINT kCropAngleEdit = kFirstFeatherEdit + 4;
constexpr int kCornerLength = 18;
constexpr int kToolbarHeight = 42;
constexpr int kToolbarDebugHeight = 68;
constexpr int kToolbarDebugModeHeight = 86;
constexpr int kToolbarLockedHeight = 32;
constexpr int kLockedDebugLabelWidth = 180;
constexpr int kToolbarGap = 5;
constexpr int kMinimumWidth = 640;
constexpr int kToolbarWideMinimumWidth = 990;
constexpr int kPersonalBorderModeLeft = 118;
constexpr int kToolbarAppTitleWidth = 128;
constexpr int kToolbarAppTitleFontHeight = 13;
constexpr wchar_t kToolbarAppName[] = L"VTSFloat_Meow";
constexpr int kToolbarCompactWidthMaxScale = 140;
constexpr int kMinimumFps = 1;
constexpr int kMaximumFps = 240;
constexpr int kScalingPerformance = 0;
constexpr int kScalingBalanced = 1;
constexpr int kScalingQuality = 2;
constexpr int kDefaultHoverOpacityPercent = 45;
constexpr int kMinimumHoverOpacityPercent = 0;
constexpr double kDefaultHoverOpacityRestoreDelaySeconds = 0.0;
constexpr int kSubjectMaskCellPx = 4;
constexpr int kSubjectMaskIniChunkChars = 1800;
// A 4K overlay produces 72 chunks at the current 4 px mask grid. The old
// limit of 64 rejected that valid mask on the next launch. 512 also leaves
// enough headroom for 8K while still rejecting a corrupt INI value.
constexpr int kMaxSubjectMaskIniChunks = 512;
constexpr double kDefaultHoverExpressionDurationSeconds = 1.0;
constexpr UINT kExpressionSelectionPreviewWaitMs = 2500;
constexpr double kHoverExpressionFadeInSeconds = 0.3;
constexpr double kHoverExpressionFadeOutSeconds = 1.0;
constexpr UINT kExpressionStatePollIntervalMs = 600;
constexpr UINT kHoverExpressionStatePollIntervalMs = 250;
constexpr UINT kHoverExpressionRetriggerCooldownMs = 2000;
constexpr int kToolbarNotificationHeight = 28;
constexpr int kBgNotificationHeight = 28;
constexpr int kHoverFadeDurationMs = 350;
constexpr int kBorderModeCustom = 0;
constexpr int kBorderModeChase = 1;
constexpr int kBorderModeNormal = 2;
constexpr int kDefaultBorderThickness = 6;
constexpr int kMaximumBorderThickness = 50;
constexpr int kDefaultUiScalePercent = 100;
constexpr int kMinimumUiScalePercent = 70;
constexpr int kMaximumUiScalePercent = 200;
constexpr int kUiScaleStepPercent = 10;
constexpr COLORREF kDefaultCustomBorderColor = RGB(24, 132, 255);
constexpr wchar_t kGithubUrl[] = L"https://github.com/fushoufish/VTSFloat_Meow";
constexpr wchar_t kDocsUrl[] = L"https://fushoufish.github.io/VTSFloat_Meow/";

enum class ToolbarButton {
    None,
    Gpu,
    Quality,
    FrameRate,
    GraphicsSettings,
    Aspect,
    Crop,
    Border,
    WindowMode,
    ViewSettings,
    Hotkey,
    Language,
    Lock,
    Github,
    Hide,
    Close,
    CropAspect,
    ViewCrop,
    ViewImage,
    ViewBlend,
    ViewShadow,
    ConnectApi,
    ViewDone,
    CropUiScale,
    CropRestore,
    CropCancel,
    CropSave,
    Unlock,
    AppTitle,
};

struct ToolbarLayout {
    std::array<RECT, static_cast<size_t>(ToolbarButton::AppTitle) + 1> buttons{};
    int rows = 1;
};

struct CropToolbarLayout {
    RECT appTitle{};
    RECT title{};
    RECT aspect{};
    RECT crop{};
    RECT image{};
    RECT blend{};
    RECT shadow{};
    RECT done{};
    RECT settings{};
    RECT restore{};
    RECT cancel{};
    RECT save{};
    int rows = 1;
};

struct CropReadoutLayout {
    RECT source{};
    RECT label{};
    RECT widthEdit{};
    RECT separator{};
    RECT heightEdit{};
    SIZE size{};
};

enum FeatherSide : size_t { FeatherLeft, FeatherTop, FeatherRight, FeatherBottom, FeatherSideCount };

struct FeatherEdge {
    int pixels = 0;
    bool enabled = false;

    bool operator==(const FeatherEdge& other) const {
        return pixels == other.pixels && enabled == other.enabled;
    }
};

using FeatherEdges = std::array<FeatherEdge, FeatherSideCount>;
using FeatherRadii = std::array<int, FeatherSideCount>;

enum FeatherPanelAction : int {
    FeatherNoAction = -1,
    FeatherRotateLeft = FeatherSideCount,
    FeatherRotateRight,
    FeatherRotateHalf,
    FeatherFlipX,
    FeatherFlipY,
    FeatherResetRotation,
    FeatherActionCount,
    ViewClose = FeatherActionCount,
};

struct FeatherPanelLayout {
    RECT header{};
    RECT close{};
    RECT hint{};
    RECT canvas{};
    RECT guide{};
    RECT angle{};
    RECT angleLabel{};
    RECT angleUnit{};
    RECT rotationTitle{};
    std::array<RECT, FeatherActionCount - FeatherRotateLeft> buttons{};
    std::array<std::array<POINT, 4>, FeatherSideCount> edges{};
    std::array<RECT, FeatherSideCount> edits{};
    std::array<RECT, FeatherSideCount> units{};
};

struct FeatherRamp {
    int outputExtent = 0;
    int sourceExtent = 0;
    int radius = 0;
    std::vector<std::uint16_t> opacity;
};

struct SubjectOutlinePixel {
    int x;
    int y;
    std::uint8_t alpha;
};

enum class VtsStatusMode {
    Hidden,
    WaitingForVts,
    WaitingForSpout,
    LaunchChoices,
};

enum class UpdateNoticeStyle {
    Standard,
    Offer,
    Checking,
    Positive,
};

enum class UpdateNoticeDismissal {
    Manual,
    Timed,
    None,
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
    UINT64 dedicatedVideoMemory = 0;
};

// A precomputed source-coordinate pair used by the CPU bilinear scaler.
// Fractions are fixed-point 0..256 so per-frame scaling avoids floats.
struct BilinearSample {
    UINT first = 0;
    UINT second = 0;
    unsigned fraction = 0;
};

std::filesystem::path LogPath() {
    return vtsfloat::CacheDirectory() / L"vts_overlay_layered.log";
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

std::filesystem::path DesktopDumpPath() {
    wchar_t path[MAX_PATH]{};
    if (SHGetFolderPathW(
            nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, path) == S_OK) {
        return std::filesystem::path(path) / L"VTSFloat_Meow_crash.dmp";
    }
    return L"VTSFloat_Meow_crash.dmp";
}

std::string CrashWideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int length = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), length, nullptr, nullptr);
    return result;
}

std::wstring CrashModulePath(DWORD64 address) {
    if (address == 0) return {};
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(
            reinterpret_cast<const void*>(address), &memory,
            sizeof(memory)) == 0 || !memory.AllocationBase) {
        return {};
    }
    std::vector<wchar_t> path(32768);
    const DWORD length = GetModuleFileNameW(
        static_cast<HMODULE>(memory.AllocationBase), path.data(),
        static_cast<DWORD>(path.size()));
    return length > 0 ? std::wstring(path.data(), length) : std::wstring{};
}

std::string CrashAccessDescription(EXCEPTION_POINTERS* info) {
    if (!info || !info->ExceptionRecord) return {};
    const EXCEPTION_RECORD* record = info->ExceptionRecord;
    if ((record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
         record->ExceptionCode != EXCEPTION_IN_PAGE_ERROR) ||
        record->NumberParameters < 2) {
        return {};
    }
    const char* operation = record->ExceptionInformation[0] == 0
        ? "read" : record->ExceptionInformation[0] == 1
            ? "write" : record->ExceptionInformation[0] == 8
                ? "execute" : "unknown";
    std::ostringstream text;
    text << "Operation: " << operation << "\n"
         << "Fault address: 0x" << std::hex << std::uppercase
         << static_cast<DWORD64>(record->ExceptionInformation[1]) << "\n";
    if (record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR &&
        record->NumberParameters >= 3) {
        text << "Underlying status: 0x"
             << static_cast<DWORD64>(record->ExceptionInformation[2]) << "\n";
    }
    return text.str();
}

std::string CrashStackTrace(EXCEPTION_POINTERS* info) {
    CONTEXT context{};
    if (info && info->ContextRecord) {
        context = *info->ContextRecord;
    } else {
        RtlCaptureContext(&context);
    }

    STACKFRAME64 frame{};
    DWORD machine = 0;
#if defined(_M_X64)
    machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context.Rip;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrStack.Offset = context.Rsp;
#elif defined(_M_IX86)
    machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context.Eip;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrStack.Offset = context.Esp;
#else
    return "Call stack unavailable for this architecture.\n";
#endif
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    if (!SymInitialize(process, nullptr, TRUE)) {
        std::ostringstream failure;
        failure << "Symbol engine initialization failed: "
                << GetLastError() << "\n";
        return failure.str();
    }

    std::ostringstream trace;
    DWORD64 previousAddress = 0;
    for (unsigned index = 0; index < 64; ++index) {
        const DWORD64 address = frame.AddrPC.Offset;
        if (address == 0 || address == previousAddress) break;
        previousAddress = address;

        trace << '#' << std::dec << index << " 0x"
              << std::hex << std::uppercase << address;

        IMAGEHLP_MODULE64 module{};
        module.SizeOfStruct = sizeof(module);
        const bool hasModule = SymGetModuleInfo64(
            process, address, &module) != FALSE;
        if (hasModule && module.ModuleName[0]) {
            trace << ' ' << module.ModuleName;
            if (module.BaseOfImage != 0 && address >= module.BaseOfImage) {
                trace << "+0x" << (address - module.BaseOfImage);
            }
        } else {
            const std::wstring path = CrashModulePath(address);
            if (!path.empty()) {
                trace << ' ' << CrashWideToUtf8(
                    std::filesystem::path(path).filename().wstring());
            }
        }

        alignas(SYMBOL_INFO) unsigned char symbolStorage[
            sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolStorage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        if (SymFromAddr(process, address, &displacement, symbol)) {
            trace << " ! " << symbol->Name;
            if (displacement != 0) trace << "+0x" << displacement;
        }

        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(
                process, address, &lineDisplacement, &line) &&
            line.FileName) {
            trace << " (" << line.FileName << ':' << std::dec
                  << line.LineNumber << ')';
        }
        trace << '\n';

        if (!StackWalk64(
                machine, process, thread, &frame, &context, nullptr,
                SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
            break;
        }
    }
    SymCleanup(process);
    return trace.str();
}

bool WriteMiniDump(
    const std::filesystem::path& path, EXCEPTION_POINTERS* info,
    DWORD& error) {
    error = ERROR_SUCCESS;
    HANDLE file = CreateFileW(
        path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        return false;
    }

    MINIDUMP_EXCEPTION_INFORMATION exception{};
    exception.ThreadId = GetCurrentThreadId();
    exception.ExceptionPointers = info;
    exception.ClientPointers = FALSE;
    const MINIDUMP_TYPE type = static_cast<MINIDUMP_TYPE>(
        MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
    const BOOL written = MiniDumpWriteDump(
        GetCurrentProcess(), GetCurrentProcessId(), file, type,
        info ? &exception : nullptr, nullptr, nullptr);
    if (!written) error = GetLastError();
    CloseHandle(file);
    if (!written) DeleteFileW(path.c_str());
    return written != FALSE;
}

std::atomic_flag gCrashDumpWritten = ATOMIC_FLAG_INIT;

void CopyLogToDesktop(
    const std::string& reason, EXCEPTION_POINTERS* info = nullptr) {
    if (gCrashDumpWritten.test_and_set(std::memory_order_acq_rel)) {
        return;
    }
    try {
        const auto src = LogPath();
        const auto dst = DesktopLogPath();
        const auto dump = DesktopDumpPath();
        DWORD dumpError = ERROR_SUCCESS;
        const bool dumpWritten = WriteMiniDump(dump, info, dumpError);
        std::ofstream out(dst, std::ios::trunc);
        out << "===== CRASH / EXIT DUMP =====\n"
            << "Reason: " << reason << "\n"
            << "Process ID: " << GetCurrentProcessId() << "\n"
            << "Thread ID: " << GetCurrentThreadId() << "\n";
        if (info && info->ExceptionRecord) {
            const DWORD64 exceptionAddress = reinterpret_cast<DWORD64>(
                info->ExceptionRecord->ExceptionAddress);
            const std::wstring modulePath = CrashModulePath(exceptionAddress);
            out << CrashAccessDescription(info);
            if (!modulePath.empty()) {
                out << "Exception module: "
                    << CrashWideToUtf8(modulePath) << "\n";
                MEMORY_BASIC_INFORMATION memory{};
                if (VirtualQuery(
                        info->ExceptionRecord->ExceptionAddress, &memory,
                        sizeof(memory)) != 0 && memory.AllocationBase) {
                    out << "Module offset: 0x" << std::hex << std::uppercase
                        << exceptionAddress - reinterpret_cast<DWORD64>(
                            memory.AllocationBase) << std::dec << "\n";
                }
            }
        }
        out << "Mini dump: " << CrashWideToUtf8(dump.wstring()) << "\n"
            << "Mini dump written: " << (dumpWritten ? "yes" : "no")
            << " (error " << dumpError << ")\n"
            << "===== CALL STACK =====\n"
            << CrashStackTrace(info)
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
    // caught by the owning thread (including the VTube Studio API worker) and must not
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
    CopyLogToDesktop(reason.str(), info);
    return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* info) {
    std::ostringstream reason;
    reason << "SEH exception 0x" << std::hex << std::uppercase
           << (info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0)
           << " at 0x"
           << (info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr);
    CopyLogToDesktop(reason.str(), info);
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
    return vtsfloat::CacheDirectory() / kConfigFileName;
}

bool MigrateLegacyConfig() {
    const auto destination = ConfigPath();
    const auto legacy = destination.parent_path().parent_path() / kConfigFileName;
    // Do not overwrite current settings. Moving the old file also prevents
    // a cache reset from importing the same settings again.
    return MoveFileExW(legacy.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
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

// Normal settings writes retain their existing behavior. The update transaction
// checks all writes made on the UI thread before it lets the app exit.
thread_local bool* checkedConfigWrites = nullptr;

bool WriteConfigInt(const wchar_t* section, const wchar_t* key, int value) {
    const std::wstring text = std::to_wstring(value);
    const std::wstring path = ConfigPath().wstring();
    const bool saved = WritePrivateProfileStringW(section, key, text.c_str(), path.c_str()) != FALSE;
    if (checkedConfigWrites && !saved) *checkedConfigWrites = false;
    return saved;
}

std::wstring ReadConfigString(
    const wchar_t* section, const wchar_t* key, const wchar_t* fallback = L"") {
    wchar_t value[2048]{};
    const std::wstring path = ConfigPath().wstring();
    GetPrivateProfileStringW(
        section, key, fallback, value, ARRAYSIZE(value), path.c_str());
    return value;
}

bool WriteConfigString(
    const wchar_t* section, const wchar_t* key, const std::wstring& value) {
    const std::wstring path = ConfigPath().wstring();
    const bool saved = WritePrivateProfileStringW(section, key, value.c_str(), path.c_str()) != FALSE;
    if (checkedConfigWrites && !saved) *checkedConfigWrites = false;
    return saved;
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

struct LogSession {
    std::mutex mutex;
    std::optional<std::streamoff> start;
};

LogSession& CurrentLogSession() {
    static LogSession session;
    return session;
}

void Log(const std::string& line) {
    if (IsHighFrequencyPerfLog(line)) {
        return;
    }
    auto& session = CurrentLogSession();
    std::lock_guard<std::mutex> lock(session.mutex);
    const auto path = LogPath();
    if (!session.start) {
        std::error_code error;
        constexpr std::uintmax_t kMaximumPersistentLogBytes = 4u * 1024u * 1024u;
        const auto size = std::filesystem::file_size(path, error);
        if (!error && size > kMaximumPersistentLogBytes) {
            // Clear oversized history before appending this session.
            std::ofstream(path, std::ios::trunc).close();
        }
    }
    std::ofstream output(path, std::ios::binary | std::ios::app | std::ios::ate);
    if (!output) return;
    if (!session.start) {
        const auto start = output.tellp();
        if (start < 0) return;
        session.start = static_cast<std::streamoff>(start);
    }
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
           << line << "\r\n";
}

bool CopyCurrentLogSession(const std::filesystem::path& destination, const std::string& header) {
    auto& session = CurrentLogSession();
    std::lock_guard<std::mutex> lock(session.mutex);
    if (!session.start) return false;

    std::ifstream source(LogPath(), std::ios::binary);
    if (!source.seekg(*session.start)) return false;
    if (source.peek() == std::char_traits<char>::eof()) return false;

    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output << header << "\r\n\r\n" << source.rdbuf();
    output.close();
    return output.good();
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
constexpr wchar_t kVtsApiPluginDeveloper[] = L"fushoufish——咕噜灵波";
constexpr int kVtsApiPort = 8001;
constexpr int kVtsApiPollIntervalMs = 2000;
// Keep API recovery responsive after the initial probe. The silent retry path
// uses this cadence for the INI-saved endpoint only.
constexpr int kVtsApiReconnectDelayMs = 600;
constexpr int kVtsApiAuthorizationWaitMs = 60000;
constexpr int kVtsApiModelReadyDelayMs = 2500;
// Port discovery deliberately uses short timeouts, but reusing those values
// for the authenticated WebSocket makes a busy game look like an API drop.
constexpr DWORD kVtsApiSessionIoTimeoutMs = 1000;

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

std::wstring HexEncode(const std::vector<std::uint8_t>& data) {
    static constexpr wchar_t digits[] = L"0123456789ABCDEF";
    std::wstring encoded;
    encoded.resize(data.size() * 2);
    for (size_t i = 0; i < data.size(); ++i) {
        encoded[i * 2] = digits[(data[i] >> 4) & 0x0F];
        encoded[i * 2 + 1] = digits[data[i] & 0x0F];
    }
    return encoded;
}

bool HexDecode(const std::wstring& encoded, std::vector<std::uint8_t>& data) {
    if (encoded.size() % 2 != 0) return false;
    auto value = [](wchar_t character) -> int {
        if (character >= L'0' && character <= L'9') return character - L'0';
        if (character >= L'A' && character <= L'F') return character - L'A' + 10;
        if (character >= L'a' && character <= L'f') return character - L'a' + 10;
        return -1;
    };
    data.resize(encoded.size() / 2);
    for (size_t i = 0; i < data.size(); ++i) {
        const int high = value(encoded[i * 2]);
        const int low = value(encoded[i * 2 + 1]);
        if (high < 0 || low < 0) {
            data.clear();
            return false;
        }
        data[i] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return true;
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

    std::vector<BYTE> pngBytes;
    const bool converted = [&]() {
        // Every GDI+ object intentionally lives inside this scope.  The old
        // code called GdiplusShutdown while the stack-allocated `resized`
        // Bitmap was still alive; its destructor then entered an already shut
        // down GDI+ runtime and could raise 0xC0000005 exactly when an expired
        // token forced VTS through the new-authorization/icon path.
        ComPtr<IStream> srcStream;
        srcStream.Attach(SHCreateMemStream(
            static_cast<const BYTE*>(data), size));
        if (!srcStream) return false;

        Gdiplus::Bitmap srcBitmap(srcStream.Get());
        if (srcBitmap.GetLastStatus() != Gdiplus::Ok) return false;

        Gdiplus::Bitmap resized(128, 128, PixelFormat32bppARGB);
        if (resized.GetLastStatus() != Gdiplus::Ok) return false;
        {
            Gdiplus::Graphics graphics(&resized);
            if (graphics.GetLastStatus() != Gdiplus::Ok) return false;
            graphics.SetInterpolationMode(
                Gdiplus::InterpolationModeHighQualityBicubic);
            if (graphics.DrawImage(&srcBitmap, 0, 0, 128, 128) !=
                Gdiplus::Ok) {
                return false;
            }
        }

        ComPtr<IStream> outStream;
        IStream* rawOutStream = nullptr;
        if (FAILED(CreateStreamOnHGlobal(
                nullptr, TRUE, &rawOutStream)) || !rawOutStream) {
            return false;
        }
        outStream.Attach(rawOutStream);

        UINT numEncoders = 0;
        UINT sizeEncoders = 0;
        if (Gdiplus::GetImageEncodersSize(
                &numEncoders, &sizeEncoders) != Gdiplus::Ok ||
            numEncoders == 0 || sizeEncoders == 0) {
            return false;
        }
        std::vector<BYTE> encoderBuf(sizeEncoders);
        auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(
            encoderBuf.data());
        if (Gdiplus::GetImageEncoders(
                numEncoders, sizeEncoders, encoders) != Gdiplus::Ok) {
            return false;
        }
        const CLSID* pngClsid = nullptr;
        for (UINT i = 0; i < numEncoders; ++i) {
            if (encoders[i].MimeType &&
                wcscmp(encoders[i].MimeType, L"image/png") == 0) {
                pngClsid = &encoders[i].Clsid;
                break;
            }
        }
        if (!pngClsid ||
            resized.Save(outStream.Get(), pngClsid, nullptr) != Gdiplus::Ok) {
            return false;
        }

        STATSTG statistics{};
        if (FAILED(outStream->Stat(&statistics, STATFLAG_NONAME)) ||
            statistics.cbSize.QuadPart == 0 ||
            statistics.cbSize.QuadPart >
                static_cast<ULONGLONG>((std::numeric_limits<SIZE_T>::max)())) {
            return false;
        }
        HGLOBAL hGlobal = nullptr;
        if (FAILED(GetHGlobalFromStream(outStream.Get(), &hGlobal)) ||
            !hGlobal) {
            return false;
        }
        const SIZE_T pngSize = static_cast<SIZE_T>(
            statistics.cbSize.QuadPart);
        const void* bytes = GlobalLock(hGlobal);
        if (!bytes) return false;
        pngBytes.assign(
            static_cast<const BYTE*>(bytes),
            static_cast<const BYTE*>(bytes) + pngSize);
        GlobalUnlock(hGlobal);
        return !pngBytes.empty();
    }();

    // All Bitmap/Graphics/stream wrappers above have been destroyed before
    // the matching shutdown call.
    Gdiplus::GdiplusShutdown(token);
    return converted ? Base64Encode(pngBytes) : std::string{};
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
    ++pos;
    while (pos < json.size() &&
           (json[pos] == ' ' || json[pos] == '\t' ||
            json[pos] == '\r' || json[pos] == '\n')) {
        ++pos;
    }
    return json.compare(pos, 4, "true") == 0;
}

// VTS returns expression names without the file suffix in most responses,
// but older models/API versions may return the filename instead. Keep the
// actual filename for API requests and only normalize the user-facing label.
std::string StripExpressionSuffix(std::string value) {
    auto endsWithInsensitive = [](const std::string& text, const char* suffix) {
        const size_t suffixLength = std::strlen(suffix);
        if (text.size() < suffixLength) return false;
        const size_t start = text.size() - suffixLength;
        for (size_t i = 0; i < suffixLength; ++i) {
            if (std::tolower(static_cast<unsigned char>(text[start + i])) !=
                std::tolower(static_cast<unsigned char>(suffix[i]))) {
                return false;
            }
        }
        return true;
    };

    // Be tolerant of both the normal .exp3.json filename and models that
    // expose one suffix at a time.
    while (endsWithInsensitive(value, ".exp3.json")) {
        value.resize(value.size() - std::strlen(".exp3.json"));
    }
    while (endsWithInsensitive(value, ".exp3") ||
           endsWithInsensitive(value, ".json")) {
        value.resize(value.size() - 5);
    }
    return value;
}

struct VtsExpressionCommand {
    std::string file;
    bool active = false;
    double fadeTime = 0.3;
};

struct VtsExpressionState {
    std::string file;
    bool active = false;
};

// VTube Studio API WebSocket client - runs in a background thread.
class VtsApiClient {
public:
    struct ExpressionInfo {
        std::string file;
        std::string name;
        bool active = false;
    };

    void Start(HINSTANCE instance) {
        if (running_.load()) return;
        if (thread_.joinable()) thread_.join();
        running_.store(true);
        workerDone_.store(false);
        instance_ = instance;
        const int savedPort = ReadConfigInt(L"VtsApi", L"Port", 0);
        portConfigured_ = savedPort > 0 && savedPort <= 65535;
        port_ = portConfigured_ ? savedPort : kVtsApiPort;
        initialDiscoveryStarted_.store(false, std::memory_order_relaxed);
        authPending_.store(false, std::memory_order_relaxed);
        authDenied_.store(false, std::memory_order_relaxed);
        authorizationRetryRequested_.store(false, std::memory_order_relaxed);
        scanRequested_.store(false, std::memory_order_relaxed);
        scanning_.store(false, std::memory_order_relaxed);
        ClearScanResult();
        thread_ = std::thread([this]() { ThreadMain(); workerDone_.store(true); });
    }

    void RequestStop() { running_.store(false); }
    bool IsStopped() const { return workerDone_.load(); }

    void Stop() {
        RequestStop();
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

    std::uint64_t GetExpressionStateRevision() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return expressionStateRevision_;
    }

    bool GetExpressionStateAfter(
        std::uint64_t previousRevision,
        std::vector<ExpressionInfo>& expressions,
        std::uint64_t& revision) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (expressionStateRevision_ <= previousRevision || expressions_.empty()) {
            return false;
        }
        expressions = expressions_;
        revision = expressionStateRevision_;
        return true;
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
        {
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
        // The queue represents the newest intended state. Update the local
        // cache immediately instead of waiting for the WebSocket round trip;
        // otherwise a leave/restore followed by a quick re-entry can capture
        // the stale pre-restore value and leave the expression permanently on.
        std::lock_guard<std::mutex> stateLock(mutex_);
        for (auto& expression : expressions_) {
            if (expression.file == file) {
                expression.active = active;
                break;
            }
        }
    }

    bool WaitForExpressionCommands(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(expressionCommandMutex_);
        return expressionCommandCondition_.wait_for(lock, timeout, [this]() {
            return expressionCommands_.empty() && expressionCommandsInFlight_ == 0;
        });
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
            expressionStateRevision_ = 0;
        }
        {
            std::lock_guard<std::mutex> lock(expressionCommandMutex_);
            expressionCommands_.clear();
            expressionCommandsInFlight_ = 0;
        }
        expressionCommandCondition_.notify_all();
        expressionStateRequested_.store(false, std::memory_order_relaxed);
        initialDiscoveryStarted_.store(false, std::memory_order_relaxed);
        authPending_.store(false, std::memory_order_relaxed);
        authDenied_.store(false, std::memory_order_relaxed);
        authorizationRetryRequested_.store(false, std::memory_order_relaxed);
    }

    bool IsConnected() const {
        return connected_.load();
    }

    bool IsAwaitingAuthorization() const {
        return authPending_.load(std::memory_order_relaxed);
    }

    bool WasAuthorizationDenied() const {
        return authDenied_.load(std::memory_order_relaxed);
    }

    void RequestAuthorizationRetry() {
        // A denial is sticky by design. Only this explicit user action clears
        // it and wakes the API thread for one new authorization flow.
        authorizationRetryRequested_.store(true, std::memory_order_relaxed);
        authDenied_.store(false, std::memory_order_relaxed);
        authPending_.store(false, std::memory_order_relaxed);
        scanResult_.store(0, std::memory_order_relaxed);
        Log("[vts-api] authorization retry requested by user");
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

    std::atomic<bool> workerDone_{true};

    void ThreadMainImpl() {
        LoadCachedToken();
        Log("[vts-api] thread started");
        while (running_.load()) {
            if (authDenied_.load(std::memory_order_relaxed)) {
                // Do not keep reopening VTS's authorization prompt after the
                // user explicitly denied error 50. A manual port scan remains
                // available, but it must not imply authorization consent.
                if (scanRequested_.load(std::memory_order_relaxed)) {
                    ScanRequestedPorts();
                    Disconnect();
                }
                constexpr int kDeniedWaitStepMs = 100;
                for (int elapsed = 0;
                     elapsed < kVtsApiReconnectDelayMs && running_.load() &&
                     authDenied_.load(std::memory_order_relaxed) &&
                     !authorizationRetryRequested_.load(std::memory_order_relaxed) &&
                     !scanRequested_.load(std::memory_order_relaxed);
                     elapsed += kDeniedWaitStepMs) {
                    Sleep(kDeniedWaitStepMs);
                }
                continue;
            }
            if (authorizationRetryRequested_.exchange(
                    false, std::memory_order_relaxed)) {
                Log("[vts-api] starting user-requested authorization retry");
            }
            bool connectedToApi = false;
            if (!initialDiscoveryStarted_.exchange(true, std::memory_order_relaxed)) {
                Log("[vts-api] initial discovery started");
                connectedToApi = ConnectInitial();
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
            // Remember a verified VTS endpoint before authorization. This is
            // required so the retry button can reconnect after a first-run
            // denial even when no API port existed in the INI beforehand.
            if (!running_.load()) { Disconnect(); break; }
            if (activePort_ > 0) {
                const bool portChanged =
                    !portConfigured_ || activePort_ != port_;
                if (activePort_ != port_) {
                    port_ = activePort_;
                }
                portConfigured_ = true;
                if (portChanged) {
                    WriteConfigInt(L"VtsApi", L"Port", activePort_);
                    Log("[vts-api] saved verified port " +
                        std::to_string(activePort_) + " to config");
                }
            }
            if (!Authenticate()) {
                Disconnect();
                SleepMs(kVtsApiReconnectDelayMs);
                continue;
            }
            connected_.store(true);
            everAuthenticated_.store(true);
            RequestExpressionState();
            int consecutivePollFailures = 0;
            while (running_.load()) {
                if (!PollStatistics()) {
                    // A busy VTS/game can occasionally delay one WebSocket
                    // response. Keep the authenticated session alive across
                    // a few transient read timeouts; dropping it immediately
                    // makes the UI show a false API-disconnected prompt.
                    if (++consecutivePollFailures < 3) {
                        SleepMs(250);
                        continue;
                    }
                    break;
                }
                consecutivePollFailures = 0;
                // Keep expression commands responsive without increasing the
                // statistics request rate. Hover transitions are generated by
                // the render thread and should not wait for the next 2-second
                // statistics poll.
                constexpr int kExpressionPumpIntervalMs = 50;
                for (int elapsed = 0;
                     elapsed < kVtsApiPollIntervalMs && running_.load();
                     elapsed += kExpressionPumpIntervalMs) {
                    ProcessExpressionRequests();
                    Sleep(kExpressionPumpIntervalMs);
                }
            }
            connected_.store(false);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                realtimeFps_ = 0;
                extraStats_ = ExtraStats{};
            }
            Disconnect(true);
            if (running_.load()) SleepMs(kVtsApiReconnectDelayMs);
        }
    }

    bool ScanRequestedPorts() {
        if (scanRequested_.exchange(false)) {
            scanning_.store(true);
            std::vector<int> ports = FindVtsListeningPorts();
            for (int port : ports) {
                if (!running_.load()) { scanning_.store(false); return false; }
                const DWORD timeout = port == kVtsApiPort ? 3000 : 1000;
                if (TryConnectPort(port, timeout, timeout) && ProbeIsVtsApi()) {
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
            const int attempts = port_ == kVtsApiPort ? 3 : 1;
            const DWORD timeout = port_ == kVtsApiPort ? 3000 : 1000;
            for (int attempt = 0; attempt < attempts && running_.load(); ++attempt) {
                if (TryConnectPort(port_, timeout, timeout) && ProbeIsVtsApi()) {
                    return true;
                }
                Disconnect();
            }
        }

        constexpr struct Candidate {
            int port;
            int attempts;
            DWORD timeoutMs;
        } candidates[] = { { 8001, 3, 3000 }, { 8002, 1, 1000 }, { 8000, 1, 1000 } };
        for (const Candidate& candidate : candidates) {
            if (!running_.load()) return false;
            if (portConfigured_ && candidate.port == port_) continue;
            for (int attempt = 0; attempt < candidate.attempts && running_.load(); ++attempt) {
                if (TryConnectPort(candidate.port, candidate.timeoutMs, candidate.timeoutMs) &&
                    ProbeIsVtsApi()) {
                    return true;
                }
                Disconnect();
            }
        }
        return false;
    }

    bool ConnectSilent() {
        if (scanRequested_.load()) {
            return ScanRequestedPorts();
        }
        if (!portConfigured_ || !running_.load()) return false;
        if (TryConnectPort(port_, 1000, 1000) && ProbeIsVtsApi()) {
            return true;
        }
        Disconnect();
        return false;
    }

    bool ProbeIsVtsApi() {
        // Send an APIStateRequest (no authentication needed).
        // Verify the response is from VTube Studio API.
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
            Log("[vts-api] probe: not VTube Studio API on port " + std::to_string(activePort_) +
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

    bool TryConnectPort(int port, DWORD connectTimeoutMs = 1000, DWORD ioTimeoutMs = 1000) {
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

        // Keep the probe timeout consistent with the selected endpoint:
        // 8001 gets the longer first-discovery window, while all other
        // candidate ports use the one-second scan budget.
        DWORD sessionIoTimeout = port == kVtsApiPort
            ? 3000 : kVtsApiSessionIoTimeoutMs;
        WinHttpSetOption(
            hSession_, WINHTTP_OPTION_RECEIVE_TIMEOUT,
            &sessionIoTimeout, sizeof(sessionIoTimeout));
        WinHttpSetOption(
            hSession_, WINHTTP_OPTION_SEND_TIMEOUT,
            &sessionIoTimeout, sizeof(sessionIoTimeout));

        WinHttpCloseHandle(hRequest_);
        hRequest_ = nullptr;
        activePort_ = port;
        Log("[vts-api] WebSocket connected on port " + std::to_string(port));
        return true;
    }

    void Disconnect(bool logAuthenticatedTransition = false) {
        if (hWebSocket_) {
            WinHttpWebSocketClose(hWebSocket_, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
            WinHttpCloseHandle(hWebSocket_);
            hWebSocket_ = nullptr;
        }
        CleanupHandles();
        // Failed background probes are expected while VTS is closed. Logging
        // every silent reconnect attempt made long-running logs grow forever;
        // only record the loss of a session that had reached the authenticated
        // polling loop.
        if (logAuthenticatedTransition) {
            Log("[vts-api] disconnected");
        }
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

    std::string ReceiveJson(DWORD* receiveError = nullptr) {
        if (receiveError) *receiveError = ERROR_SUCCESS;
        if (!hWebSocket_) return {};

        // WinHTTP may split a large UTF-8 message into multiple fragments.
        // ExpressionStateResponse contains hotkey/parameter details and can
        // easily exceed 8 KiB. Reading only the first fragment truncates the
        // JSON, leaves the remaining fragments queued, and desynchronizes all
        // following request/response pairs.
        std::string message;
        message.reserve(32768);
        std::vector<char> chunk(16384);
        for (;;) {
            DWORD bytesRead = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType{};
            const DWORD err = WinHttpWebSocketReceive(
                hWebSocket_, chunk.data(), static_cast<DWORD>(chunk.size()),
                &bytesRead, &bufferType);
            if (err != ERROR_SUCCESS) {
                if (receiveError) *receiveError = err;
                return {};
            }
            if (bufferType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
                if (receiveError) *receiveError = ERROR_CONNECTION_ABORTED;
                return {};
            }
            if (bytesRead > 0) {
                message.append(chunk.data(), bytesRead);
            }
            if (bufferType != WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE &&
                bufferType != WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE) {
                break;
            }
        }
        return message;
    }

    // VTS_API_CLASS_PLACEHOLDER_2

    static const std::string& PluginIdentityJson() {
        static const std::string identity =
            R"("pluginName":")" + EscapeJsonString(WideToUtf8(kVtsApiPluginName)) +
            R"(","pluginDeveloper":")" + EscapeJsonString(WideToUtf8(kVtsApiPluginDeveloper)) + '"';
        return identity;
    }

    bool Authenticate() {
        if (cachedToken_.empty()) {
            return RequestNewToken();
        }
        std::string data = PluginIdentityJson() + R"(,"authenticationToken":")" + EscapeJsonString(cachedToken_) + R"(")";
        std::string request = BuildJsonRequest("AuthenticationRequest", data);
        if (!SendJson(request)) return false;
        std::string response = ReceiveJson();
        if (response.empty()) return false;

        if (ExtractJsonBool(response, "authenticated")) {
            authDenied_.store(false, std::memory_order_relaxed);
            Log("[vts-api] authenticated with cached token");
            return true;
        }
        Log("[vts-api] cached token rejected, requesting new one");
        cachedToken_.clear();
        // A token belongs to the VTS installation that granted it.  When an
        // INI is copied to another computer, do not leave the rejected token
        // on disk and retry it on every application restart.
        SaveToken();
        everAuthenticated_.store(false);
        return RequestNewToken();
    }

    bool RequestNewToken() {
        Log("[vts-api] requesting new token");
        authPending_.store(true, std::memory_order_relaxed);
        if (!pluginIconLoaded_) {
            pluginIconBase64_ = LoadIconAsBase64Png(instance_);
            pluginIconLoaded_ = true;
            Log("[vts-api] plugin icon prepared bytes=" +
                std::to_string(pluginIconBase64_.size()));
        }
        std::string data = PluginIdentityJson() + R"(,"pluginIcon":")" + pluginIconBase64_ + R"(")";
        std::string request = BuildJsonRequest("AuthenticationTokenRequest", data);
        if (!SendJson(request)) {
            authPending_.store(false, std::memory_order_relaxed);
            return false;
        }
        std::string response;
        const ULONGLONG authorizationDeadline =
            GetTickCount64() + kVtsApiAuthorizationWaitMs;
        do {
            DWORD receiveError = ERROR_SUCCESS;
            response = ReceiveJson(&receiveError);
            if (!response.empty() || !running_.load() ||
                receiveError != ERROR_WINHTTP_TIMEOUT) {
                break;
            }
            // Keep receiving the original request instead of sending another
            // token request every time the short socket timeout expires.
        } while (GetTickCount64() < authorizationDeadline);
        if (!running_.load() || response.empty()) {
            authPending_.store(false, std::memory_order_relaxed);
            Log("[vts-api] authorization request timed out");
            return false;
        }

        std::string token = ExtractJsonString(response, "authenticationToken");
        if (token.empty()) {
            authPending_.store(false, std::memory_order_relaxed);
            const int errorId = ExtractJsonInt(response, "errorID");
            const std::string message = ExtractJsonString(response, "message");
            if (errorId == 50) {
                authDenied_.store(true, std::memory_order_relaxed);
                authorizationRetryRequested_.store(false, std::memory_order_relaxed);
                Log("[vts-api] authorization denied by user errorID=50");
            } else {
                Log("[vts-api] token request failed errorID=" +
                    std::to_string(errorId) + " message=" + message);
            }
            return false;
        }
        cachedToken_ = token;
        SaveToken();
        Log("[vts-api] received new token");

        std::string authData = PluginIdentityJson() + R"(,"authenticationToken":")" + EscapeJsonString(cachedToken_) + R"(")";
        std::string authReq = BuildJsonRequest("AuthenticationRequest", authData);
        if (!SendJson(authReq)) {
            authPending_.store(false, std::memory_order_relaxed);
            return false;
        }
        response = ReceiveJson();
        if (response.empty()) return false;

        if (ExtractJsonBool(response, "authenticated")) {
            authPending_.store(false, std::memory_order_relaxed);
            authDenied_.store(false, std::memory_order_relaxed);
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
            const bool modelLoaded = ExtractJsonBool(response, "modelLoaded");
            std::string modelId = ExtractJsonString(response, "modelName");
            int artmesh = ExtractJsonInt(response, "numberOfLive2DArtmeshes");
            bool modelChanged = false;
            bool expressionNeedsRefresh = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                // During model switching VTS can briefly answer with
                // modelLoaded=false and an empty modelName. Do not replace
                // the last valid nickname with an empty string, otherwise
                // the debug/API line visibly flickers until the next poll.
                if (modelLoaded && !modelId.empty()) {
                    modelChanged = modelId != extraStats_.modelId;
                    extraStats_.modelId = modelId;
                } else {
                    modelId = extraStats_.modelId;
                }
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
            // We only need file/name/active. Requesting parameter and hotkey
            // details can make a model with many expressions return a very
            // large payload, delaying the menu and increasing timeout risk.
            if (SendJson(BuildJsonRequest("ExpressionStateRequest", R"("details":false)"))) {
                const std::string response = ReceiveJson();
                if (!response.empty() &&
                    ExtractJsonString(response, "messageType") != "APIError") {
                    std::vector<ExpressionInfo> parsed;
                    // Parse only objects in data.expressions. The previous
                    // string search started at each "file" key and stopped
                    // at the first closing brace, which lost the preceding
                    // "name" field and was fragile for nested details.
                    size_t expressionsKey = response.find(R"("expressions")");
                    size_t arrayStart = expressionsKey == std::string::npos
                        ? std::string::npos : response.find('[', expressionsKey);
                    if (arrayStart != std::string::npos) {
                        size_t cursor = arrayStart + 1;
                        while (cursor < response.size()) {
                            while (cursor < response.size() &&
                                   (response[cursor] == ' ' || response[cursor] == '\t' ||
                                    response[cursor] == '\r' || response[cursor] == '\n' ||
                                    response[cursor] == ',')) {
                                ++cursor;
                            }
                            if (cursor >= response.size() || response[cursor] == ']') break;
                            if (response[cursor] != '{') {
                                ++cursor;
                                continue;
                            }

                            const size_t objectStart = cursor++;
                            int depth = 1;
                            bool inString = false;
                            bool escaped = false;
                            while (cursor < response.size() && depth > 0) {
                                const char c = response[cursor++];
                                if (inString) {
                                    if (escaped) escaped = false;
                                    else if (c == '\\') escaped = true;
                                    else if (c == '"') inString = false;
                                    continue;
                                }
                                if (c == '"') inString = true;
                                else if (c == '{') ++depth;
                                else if (c == '}') --depth;
                            }
                            if (depth != 0) break;
                            const std::string object = response.substr(
                                objectStart, cursor - objectStart);
                            const std::string file = ExtractJsonString(object, "file");
                            if (file.empty()) continue;

                            ExpressionInfo info;
                            info.file = file;
                            info.name = StripExpressionSuffix(
                                ExtractJsonString(object, "name"));
                            if (info.name.empty()) {
                                info.name = StripExpressionSuffix(file);
                            }
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
                    }
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        expressions_ = std::move(parsed);
                        lastExpressionModelId_ = ExtractJsonString(response, "modelName");
                        ++expressionStateRevision_;
                    }
                }
            }
        }

        std::deque<VtsExpressionCommand> commands;
        {
            std::lock_guard<std::mutex> lock(expressionCommandMutex_);
            commands.swap(expressionCommands_);
            expressionCommandsInFlight_ += commands.size();
        }
        for (const auto& command : commands) {
            const std::string data =
                R"("expressionFile":")" + EscapeJsonString(command.file) +
                R"(","active":)" + (command.active ? "true" : "false") +
                R"(,"fadeTime":)" + std::to_string((std::clamp)(command.fadeTime, 0.0, 2.0));
            bool retry = false;
            if (!SendJson(BuildJsonRequest("ExpressionActivationRequest", data))) {
                // Preserve a valid hover transition if the socket was busy or
                // briefly unavailable. The next connection/pump will retry it.
                retry = true;
            } else {
                const std::string response = ReceiveJson();
                retry = response.empty();
            }
            if (retry) {
                SetExpressionActive(command.file, command.active, command.fadeTime);
            }
            // SetExpressionActive updates the cache optimistically when the
            // command is queued. Do not overwrite it here: a newer opposite
            // command may already be waiting while this response arrives.
            {
                std::lock_guard<std::mutex> lock(expressionCommandMutex_);
                if (expressionCommandsInFlight_ > 0) {
                    --expressionCommandsInFlight_;
                }
            }
            expressionCommandCondition_.notify_all();
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
    std::atomic<bool> authPending_{ false };
    std::atomic<bool> authDenied_{ false };
    std::atomic<bool> authorizationRetryRequested_{ false };
    std::thread thread_;
    int realtimeFps_ = 0;
    int port_ = kVtsApiPort;
    bool portConfigured_ = false;
    int activePort_ = 0;
    std::string cachedToken_;
    std::string pluginIconBase64_;
    bool pluginIconLoaded_ = false;
    ExtraStats extraStats_;
    std::vector<ExpressionInfo> expressions_;
    std::string lastExpressionModelId_;
    std::uint64_t expressionStateRevision_ = 0;
    std::mutex expressionCommandMutex_;
    std::condition_variable expressionCommandCondition_;
    std::deque<VtsExpressionCommand> expressionCommands_;
    size_t expressionCommandsInFlight_ = 0;
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
    return length && length < buffer.size()
        ? std::filesystem::path(std::wstring(buffer.data(), length))
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
    // start_without_steam.bat is optional in some VTube Studio installs.
    // The executable alone is enough to identify a usable installation.
    return std::filesystem::is_regular_file(directory / kVtsExecutableName, error);
}

bool TryParseNonNegativeDecimal(const std::wstring& text, double& value) {
    if (text.empty()) {
        return false;
    }
    bool hasDigit = false;
    bool hasDot = false;
    for (const wchar_t ch : text) {
        if (ch >= L'0' && ch <= L'9') {
            hasDigit = true;
        } else if (ch == L'.' && !hasDot) {
            hasDot = true;
        } else {
            return false;
        }
    }
    if (!hasDigit) {
        return false;
    }
    wchar_t* end = nullptr;
    const double parsed = std::wcstod(text.c_str(), &end);
    if (end == text.c_str() || *end != L'\0' ||
        !std::isfinite(parsed) || parsed < 0.0) {
        return false;
    }
    value = parsed;
    return true;
}

double ReadConfigNonNegativeDecimal(
    const wchar_t* section, const wchar_t* key, double fallback) {
    double value = fallback;
    const std::wstring text = ReadConfigString(section, key);
    return TryParseNonNegativeDecimal(text, value) ? value : fallback;
}

std::wstring FormatNonNegativeDecimal(double value) {
    if (!std::isfinite(value) || value < 0.0) {
        value = kDefaultHoverExpressionDurationSeconds;
    }
    std::wostringstream output;
    output << std::fixed << std::setprecision(3) << value;
    std::wstring text = output.str();
    while (text.size() > 1 && text.back() == L'0') text.pop_back();
    if (!text.empty() && text.back() == L'.') text.pop_back();
    return text.empty() ? L"0" : text;
}

Clock::time_point DeadlineAfterSeconds(
    Clock::time_point now, double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0) return now;
    const double maximumSeconds = std::chrono::duration<double>(
        Clock::time_point::max() - now).count();
    if (seconds >= maximumSeconds) return Clock::time_point::max();
    return now + std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(seconds));
}

bool HasVtsExternalLauncher(const std::filesystem::path& directory) {
    std::error_code error;
    return std::filesystem::is_regular_file(directory / kVtsBatchName, error);
}

bool IsVtsExecutable(const std::filesystem::path& path) {
    return _wcsicmp(path.filename().c_str(), kVtsExecutableName) == 0;
}

std::optional<std::filesystem::path> FindVtsDirectoryInSelection(
    const std::filesystem::path& selected) {
    std::error_code error;
    if (selected.empty()) {
        return std::nullopt;
    }

    std::filesystem::path root = selected;
    if (std::filesystem::is_regular_file(root, error)) {
        if (IsVtsExecutable(root) && IsVtsDirectory(root.parent_path())) {
            return root.parent_path();
        }
        root = root.parent_path();
    }
    if (!std::filesystem::is_directory(root, error)) {
        return std::nullopt;
    }

    // Cover the common cases without a recursive scan first: selecting the
    // install directory itself, or selecting its parent directory.
    for (const auto& candidate : {root, root / L"VTube Studio"}) {
        if (IsVtsDirectory(candidate)) {
            return candidate;
        }
    }

    // A manually selected Steam or games directory can contain VTube Studio
    // further down. Limit traversal depth so selecting a whole drive never
    // turns this into an expensive scan.
    std::filesystem::recursive_directory_iterator iterator(
        root, std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;
    while (!error && iterator != end) {
        const auto entry = *iterator;
        if (entry.is_directory(error) && iterator.depth() >= 5) {
            iterator.disable_recursion_pending();
        }
        if (!error && entry.is_regular_file(error) && IsVtsExecutable(entry.path())) {
            const auto directory = entry.path().parent_path();
            if (IsVtsDirectory(directory)) {
                return directory;
            }
        }
        iterator.increment(error);
    }
    return std::nullopt;
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

void ActivateRunningOverlay() {
    // The mutex can already exist while the first launch is creating its window.
    for (int attempt = 0; attempt < 50; ++attempt) {
        if (const HWND window = FindWindowW(kWindowClass, nullptr)) {
            DWORD processId = 0;
            GetWindowThreadProcessId(window, &processId);
            if (processId) AllowSetForegroundWindow(processId);
            // Keep the launching process alive while the existing UI uses its foreground grant.
            if (!SendMessageTimeoutW(window, kActivateExistingMessage, 0, 0,
                    SMTO_ABORTIFHUNG | SMTO_BLOCK, 1000, nullptr)) {
                PostMessageW(window, kActivateExistingMessage, 0, 0);
            }
            return;
        }
        Sleep(20);
    }
}

class LayeredOverlay {
public:
    int Run(HINSTANCE instance, std::filesystem::path updateTask = {},
            std::wstring updateToken = {}) {
        instance_ = instance;
        uiThreadId_ = GetCurrentThreadId();
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        INITCOMMONCONTROLSEX commonControls{};
        commonControls.dwSize = sizeof(commonControls);
        commonControls.dwICC = ICC_BAR_CLASSES | ICC_WIN95_CLASSES;
        InitCommonControlsEx(&commonControls);

        instanceMutex_ = CreateMutexW(nullptr, TRUE, kInstanceName);
        if (!instanceMutex_) return 0;
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            CloseHandle(std::exchange(instanceMutex_, nullptr));
            if (updateTask.empty()) ActivateRunningOverlay();
            return 0;
        }

        appIconLarge_ = static_cast<HICON>(LoadImageW(
            instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
            32, 32, LR_DEFAULTCOLOR));
        appIconSmall_ = static_cast<HICON>(LoadImageW(
            instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
            16, 16, LR_DEFAULTCOLOR));
        RegisterWindowClass();
        const bool migrated = MigrateLegacyConfig();
        LoadUiSettings();
        if (migrated) {
            SaveUiSettings();
            SaveImageTone();
            SaveFeatherEdges();
            SaveShadowSettings();
            Log("[settings] migrated legacy LocalAppData configuration");
        }
        // Generate the editable template even when using a built-in language.
        vtsfloat::i18n::CustomLanguageFilePath();
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
            hwnd_, WindowZOrder(), 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        AddTrayIcon();
        SetTimer(hwnd_, 67, 250, nullptr);
        UpdateDesktopPause();
        updateStartupTask_ = std::move(updateTask);
        updateStartupToken_ = std::move(updateToken);
        try {
            updater_ = std::make_unique<vtsfloat::update::Updater>(
                instance_, ConfigPath(), CurrentExecutablePath(), VTSFLOAT_VERSION,
                [](const std::string& message) { Log("[update] " + message); });
            SetTimer(hwnd_, kUpdateTimer, 100, nullptr);
            PostMessageW(hwnd_, kUpdateStartupMessage, 0, 0);
        } catch (...) {
            Log("[update] initialization failed");
        }

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
                if (updateBannerHwnd_ && IsWindowVisible(updateBannerHwnd_) &&
                    IsDialogMessageW(updateBannerHwnd_, &message)) {
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

            if (desktopOutputPaused_.load(std::memory_order_relaxed)) {
                MsgWaitForMultipleObjectsEx(0, nullptr, 250, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                nextFrame = Clock::now();
                continue;
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
        CloseShadowPanel(false);
        CloseImagePanel(false);
        CloseUiScalePicker(uiScalePickerHwnd_);
        cropEditing_ = false;
        CloseFeatherPanel();
        DestroyCropEdits();
        updater_.reset();
        if (updateBannerHwnd_ && IsWindow(updateBannerHwnd_)) {
            DestroyWindow(updateBannerHwnd_);
        }
        if (updateBannerFont_) DeleteObject(updateBannerFont_);
        // WM_CLOSE normally performs this first. Keep a destructor fallback so
        // every orderly message-loop exit still restores temporary VTS state
        // before the API worker is stopped.
        RestoreExpressionsBeforeShutdown();
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
            gpuProcessMemoryCounter_ = nullptr;
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
        if (githubGraphicsToken_) Gdiplus::GdiplusShutdown(githubGraphicsToken_);
        for (const HICON icon : toolbarToggleIcons_) {
            if (icon) DestroyIcon(icon);
        }
        if (appIconLarge_) {
            DestroyIcon(appIconLarge_);
            appIconLarge_ = nullptr;
        }
        if (appIconSmall_) {
            DestroyIcon(appIconSmall_);
            appIconSmall_ = nullptr;
        }
        if (exportDebugLogOnExit_) ExportDebugLogOnExit();
        DestroyDib();
        if (instanceMutex_) {
            ReleaseMutex(instanceMutex_);
            CloseHandle(instanceMutex_);
        }
    }

private:
    void ExportDebugLogOnExit() const {
        wchar_t desktop[MAX_PATH]{};
        const HRESULT result = SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, SHGFP_TYPE_CURRENT, desktop);
        if (FAILED(result)) {
            Log("[debug] desktop log export failed: cannot resolve Desktop (" + std::to_string(result) + ")");
            return;
        }
        const auto destination = std::filesystem::path(desktop) / L"VTSFloat_Meow_debug.log";
        const double seconds = std::chrono::duration<double>(lastModelPresentation_ - firstModelPresentation_).count();
        std::ostringstream header;
        header.imbue(std::locale::classic());
        header << "VTSFloat_Meow exited in debug mode. This log was exported to your desktop. ";
        if (modelPresentations_ > 1 && seconds > 0) {
            header << "Average frame rate: " << std::fixed << std::setprecision(1)
                   << (modelPresentations_ - 1) / seconds << " FPS.";
        } else {
            header << "Average frame rate: unavailable (not enough model frames).";
        }
        if (!CopyCurrentLogSession(destination, header.str())) {
            Log("[debug] desktop log export failed: could not copy the current session");
        }
    }

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

        WNDCLASSEXW updateClass{};
        updateClass.cbSize = sizeof(updateClass);
        updateClass.hInstance = instance_;
        updateClass.lpfnWndProc = &LayeredOverlay::UpdateBannerWindowProc;
        updateClass.lpszClassName = kUpdateBannerClass;
        updateClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        Check(RegisterClassExW(&updateClass) ? S_OK : HRESULT_FROM_WIN32(GetLastError()),
              "RegisterClassExW(update banner)");

        WNDCLASSEXW debugClass = updateClass;
        debugClass.lpfnWndProc = DefWindowProcW;
        debugClass.lpszClassName = kDebugReadoutClass;
        Check(RegisterClassExW(&debugClass) ? S_OK : HRESULT_FROM_WIN32(GetLastError()),
              "RegisterClassExW(debug readout)");

        WNDCLASSEXW guideClass = updateClass;
        guideClass.lpfnWndProc = &LayeredOverlay::ToolbarGuideProc;
        guideClass.lpszClassName = kToolbarGuideClass;
        Check(RegisterClassExW(&guideClass) ? S_OK : HRESULT_FROM_WIN32(GetLastError()),
              "RegisterClassExW(toolbar guide)");

        WNDCLASSEXW githubClass = updateClass;
        githubClass.lpfnWndProc = &LayeredOverlay::GithubDropdownProc;
        githubClass.lpszClassName = kGithubDropdownClass;
        Check(RegisterClassExW(&githubClass) ? S_OK : HRESULT_FROM_WIN32(GetLastError()),
              "RegisterClassExW(GitHub dropdown)");

        WNDCLASSEXW hintClass = githubClass;
        hintClass.lpfnWndProc = &LayeredOverlay::ToolbarHintProc;
        hintClass.lpszClassName = kToolbarHintClass;
        Check(RegisterClassExW(&hintClass) ? S_OK : HRESULT_FROM_WIN32(GetLastError()),
              "RegisterClassExW(toolbar hint)");

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

        WNDCLASSEXW subjectSelectionClass{};
        subjectSelectionClass.cbSize = sizeof(subjectSelectionClass);
        subjectSelectionClass.style = CS_DBLCLKS;
        subjectSelectionClass.hInstance = instance_;
        subjectSelectionClass.lpfnWndProc = &LayeredOverlay::SubjectSelectionWindowProc;
        subjectSelectionClass.lpszClassName = kSubjectSelectionClass;
        subjectSelectionClass.hCursor = LoadCursorW(nullptr, IDC_CROSS);
        subjectSelectionClass.hbrBackground = nullptr;
        Check(RegisterClassExW(&subjectSelectionClass)
                ? S_OK : HRESULT_FROM_WIN32(GetLastError()),
              "RegisterClassExW(subject selection)");

        WNDCLASSEXW subjectToolbarClass{};
        subjectToolbarClass.cbSize = sizeof(subjectToolbarClass);
        subjectToolbarClass.hInstance = instance_;
        subjectToolbarClass.lpfnWndProc =
            &LayeredOverlay::SubjectSelectionToolbarWindowProc;
        subjectToolbarClass.lpszClassName = kSubjectSelectionToolbarClass;
        subjectToolbarClass.hCursor = LoadCursorW(nullptr, IDC_HAND);
        subjectToolbarClass.hbrBackground = nullptr;
        Check(RegisterClassExW(&subjectToolbarClass)
                ? S_OK : HRESULT_FROM_WIN32(GetLastError()),
              "RegisterClassExW(subject selection toolbar)");
    }

    void CreateOverlayWindow() {
        const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int width = ReadConfigInt(L"window", L"width", kDefaultWidth);
        int height = ReadConfigInt(L"window", L"height", kDefaultHeight);
        int x = ReadConfigInt(
            L"window", L"x", (std::max)(20, (screenWidth - width) / 2));
        int y = ReadConfigInt(L"window", L"y", 100);
        const bool editedBounds = CropActive() || imageTransform_.active();
        width = (std::max)(editedBounds ? 1 : kMinimumWidth, width);
        height = (std::max)(editedBounds ? 1 : 96, height);
        RECT saved{ x, y, x + width, y + height };
        if (!MonitorFromRect(&saved, MONITOR_DEFAULTTONULL)) {
            width = kDefaultWidth;
            height = kDefaultHeight;
            x = (std::max)(20, (screenWidth - width) / 2);
            y = 100;
        }
        hwnd_ = CreateWindowExW(
            WS_EX_LAYERED | (desktopMode_ ? WS_EX_APPWINDOW :
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT),
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
            desktopMode_ ? 0 : WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            kToolbarClass,
            kToolbarTitle,
            WS_POPUP | WS_CLIPCHILDREN,
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
        Gdiplus::GdiplusStartupInput graphicsStartup{};
        if (Gdiplus::GdiplusStartup(&githubGraphicsToken_, &graphicsStartup, nullptr) != Gdiplus::Ok) {
            githubGraphicsToken_ = 0;
        }
        PositionToolbar();
    }

    void CreateVtsStatusWindow() {
        statusHwnd_ = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            kStatusClass, kWindowTitle, WS_POPUP,
            0, 0, 1, 1, hwnd_, nullptr, instance_, this);
        if (!statusHwnd_) Log("[vts status] failed to create window");
    }

    void SetVtsStatusMode(
        VtsStatusMode mode, const std::filesystem::path& directory = {}) {
        if (mode != VtsStatusMode::Hidden) {
            EndViewEdit();
            StopImageBackdrop();
        }
        const bool changed = mode != statusMode_ || directory != statusDirectory_;
        if (changed) {
            statusDismissed_ = false;
            statusMode_ = mode;
            statusDirectory_ = directory;
            statusFrameDirty_ = true;
        }
        if (mode != VtsStatusMode::Hidden && !statusDismissed_) {
            // The VTS status page replaces the model surface. Runtime-only
            // notices (API, opaque-background and GPU warnings) refer to a
            // live model frame and become misleading once VTS/Spout is gone.
            opaqueFrameCount_ = 0;
            opaqueBackgroundDetected_ = false;
            bgWarningCloseRect_ = RECT{};
            bgWarningDontShowRect_ = RECT{};
            gpuWarningShown_ = false;
            gpuWarningDontShowRect_ = RECT{};
            showingApiSuccess_ = false;
        }
        if (mode == VtsStatusMode::Hidden || statusDismissed_) {
            statusFrameDirty_ = true;
            ApplyClickThrough();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        ApplyClickThrough();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void PollVtsStatus(bool force = false) {
        if (GetCurrentThreadId() != uiThreadId_) return;
        // The VTS/Spout waiting page is useful before the first model frame:
        // it tells the user why the overlay is empty and offers the launch
        // buttons. API authorization notices remain separately gated by
        // hasReceivedModel_ in ShowApiNotification().
        const auto now = Clock::now();
        if (!force && now - lastVtsStatusCheck_ < std::chrono::milliseconds(750)) {
            return;
        }
        lastVtsStatusCheck_ = now;
        const auto directory = FindVtsDirectory();
        const auto running = FindRunningVts();
        if (!directory) {
            if (hasReceivedModel_ && locked_) {
                // The model had been visible and VTS is now gone. Return to
                // edit mode so the toolbar is immediately available instead
                // of leaving a click-through locked overlay on the desktop.
                SetLocked(false);
                Log("[vts status] VTS missing; unlocked editor");
            }
            // A previous model frame must not keep API/renderer warnings
            // alive while the status page is explaining that VTS is closed.
            hasReceivedModel_ = false;
            SetVtsStatusMode(VtsStatusMode::WaitingForVts);
        } else if (!running) {
            if (hasReceivedModel_ && locked_) {
                SetLocked(false);
                Log("[vts status] VTS closed; unlocked editor");
            }
            hasReceivedModel_ = false;
            SetVtsStatusMode(VtsStatusMode::LaunchChoices, *directory);
        } else {
            SetVtsStatusMode(VtsStatusMode::WaitingForSpout, *directory);
        }
    }

    void HideVtsStatus() {
        if (statusMode_ == VtsStatusMode::Hidden && !statusDismissed_) return;
        statusMode_ = VtsStatusMode::Hidden;
        statusDismissed_ = false;
        statusFrameDirty_ = true;
        statusPressedButton_ = 0;
        if (statusHwnd_) ShowWindow(statusHwnd_, SW_HIDE);
        ApplyClickThrough();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void LaunchVtsFromStatus(bool external) {
        const auto directory = FindVtsDirectory();
        if (!directory) {
            PollVtsStatus(true);
            return;
        }
        if (external && !HasVtsExternalLauncher(*directory)) {
            // The status page does not expose this action when the optional
            // batch file is absent, but keep this guard for all call paths.
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

    std::optional<std::filesystem::path> PickVtsFolder() const {
        HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool mustUninitialize = SUCCEEDED(initialized);
        ComPtr<IFileOpenDialog> dialog;
        const HRESULT created = CoCreateInstance(
            CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dialog));
        if (FAILED(created)) {
            if (mustUninitialize) CoUninitialize();
            return std::nullopt;
        }
        FILEOPENDIALOGOPTIONS options{};
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM |
                           FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
        dialog->SetTitle(Tr(L"选择 VTube Studio 安装文件夹"));
        const HRESULT shown = dialog->Show(hwnd_);
        if (FAILED(shown)) {
            if (mustUninitialize) CoUninitialize();
            return std::nullopt;
        }
        ComPtr<IShellItem> result;
        std::optional<std::filesystem::path> path;
        if (SUCCEEDED(dialog->GetResult(&result))) {
            PWSTR rawPath = nullptr;
            if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &rawPath)) && rawPath) {
                path = std::filesystem::path(rawPath);
                CoTaskMemFree(rawPath);
            }
        }
        if (mustUninitialize) CoUninitialize();
        return path;
    }

    std::optional<std::filesystem::path> PickVtsExecutable(bool shortcut = false) const {
        HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool mustUninitialize = SUCCEEDED(initialized);
        ComPtr<IFileOpenDialog> dialog;
        const HRESULT created = CoCreateInstance(
            CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dialog));
        if (FAILED(created)) {
            if (mustUninitialize) CoUninitialize();
            return std::nullopt;
        }
        const COMDLG_FILTERSPEC filters[] = {
            { L"VTube Studio.exe", L"VTube Studio.exe" },
            { Tr(L"可执行文件"), L"*.exe" },
        };
        FILEOPENDIALOGOPTIONS options{};
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST |
                           FOS_FILEMUSTEXIST | FOS_NOCHANGEDIR);
        if (!shortcut) {
            dialog->SetFileTypes(ARRAYSIZE(filters), filters);
            dialog->SetFileTypeIndex(1);
        }
        dialog->SetTitle(shortcut ? Tr(L"选择快捷应用") : Tr(L"选择 VTube Studio.exe"));
        const HRESULT shown = dialog->Show(hwnd_);
        if (FAILED(shown)) {
            dialog.Reset();
            if (mustUninitialize) CoUninitialize();
            return std::nullopt;
        }
        ComPtr<IShellItem> result;
        std::optional<std::filesystem::path> path;
        if (SUCCEEDED(dialog->GetResult(&result))) {
            PWSTR rawPath = nullptr;
            if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &rawPath)) && rawPath) {
                path = std::filesystem::path(rawPath);
                CoTaskMemFree(rawPath);
            }
        }
        result.Reset();
        dialog.Reset();
        if (mustUninitialize) CoUninitialize();
        return path;
    }

    void SaveShortcutApp(const std::filesystem::path& path) {
        if (!WriteConfigString(L"launcher", L"path", path.wstring())) {
            MessageBoxW(hwnd_, Tr(L"无法保存快捷应用设置"), kWindowTitle, MB_OK | MB_ICONWARNING);
            return;
        }
        shortcutApp_ = path;
        shortcutHoverAction_ = 0;
        statusFrameDirty_ = true;
    }

    void ActivateShortcutApp() {
        if (shortcutApp_.empty()) {
            const auto path = PickVtsExecutable(true);
            if (path) SaveShortcutApp(*path);
            return;
        }
        const auto folder = shortcutApp_.parent_path();
        if (reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd_, L"open", shortcutApp_.c_str(), nullptr,
                folder.c_str(), SW_SHOWNORMAL)) <= 32) {
            MessageBoxW(hwnd_, Tr(L"无法启动快捷应用，请检查文件路径"), kWindowTitle, MB_OK | MB_ICONWARNING);
        }
    }

    void ChooseVtsLaunchPath() {
        constexpr int kChooseFolderButton = 101;
        constexpr int kChooseExecutableButton = 102;
        const TASKDIALOG_BUTTON choices[] = {
            { kChooseFolderButton, Tr(L"选择安装文件夹") },
            { kChooseExecutableButton, Tr(L"选择 VTube Studio.exe") },
        };
        TASKDIALOGCONFIG dialog{};
        dialog.cbSize = sizeof(dialog);
        dialog.hwndParent = hwnd_;
        dialog.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
        dialog.pszWindowTitle = L"VTSFloat_Meow";
        dialog.pszMainInstruction = Tr(L"添加 VTube Studio 启动路径");
        dialog.pszContent = Tr(L"可以选择 VTube Studio 的安装文件夹，或直接选择 VTube Studio.exe。\n程序会自动查找并保存正确的安装目录。");
        dialog.cButtons = ARRAYSIZE(choices);
        dialog.pButtons = choices;
        dialog.nDefaultButton = kChooseFolderButton;
        int selected = 0;
        if (FAILED(TaskDialogIndirect(&dialog, &selected, nullptr, nullptr)) ||
            (selected != kChooseFolderButton &&
             selected != kChooseExecutableButton)) {
            return;
        }
        const auto selectedPath = selected == kChooseFolderButton
            ? PickVtsFolder() : PickVtsExecutable();
        if (!selectedPath) {
            return;
        }
        const auto directory = FindVtsDirectoryInSelection(*selectedPath);
        if (!directory) {
            statusPathLookupFailed_ = true;
            statusFrameDirty_ = true;
            InvalidateRect(hwnd_, nullptr, FALSE);
            Log("[vts status] no VTube Studio executable in selected path");
            return;
        }
        WriteConfigString(L"vts", L"install_dir", directory->wstring());
        statusPathLookupFailed_ = false;
        statusDismissed_ = false;
        lastVtsStatusCheck_ = Clock::time_point{};
        Log("[vts status] install_dir set manually");
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
        if (cropEditing_) rect = cropBeforeEditBounds_;
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

    void LoadSubjectHoverMask() {
        subjectHoverMaskGridWidth_ = ReadConfigInt(
            L"opacity", L"subject_hover_mask_width", 0);
        subjectHoverMaskGridHeight_ = ReadConfigInt(
            L"opacity", L"subject_hover_mask_height", 0);
        const int chunks = ReadConfigInt(
            L"opacity", L"subject_hover_mask_chunks", 0);
        if (subjectHoverMaskGridWidth_ <= 0 || subjectHoverMaskGridHeight_ <= 0 ||
            chunks <= 0 || chunks > kMaxSubjectMaskIniChunks) {
            subjectHoverMaskBits_.clear();
            subjectHoverRegionConfigured_ = false;
            subjectHoverTrackingReferenceValid_ = false;
            subjectHoverMaskDirty_ = false;
            return;
        }
        std::wstring encoded;
        encoded.reserve(static_cast<size_t>(chunks) * kSubjectMaskIniChunkChars);
        for (int i = 0; i < chunks; ++i) {
            const std::wstring key = L"subject_hover_mask_" + std::to_wstring(i);
            encoded += ReadConfigString(L"opacity", key.c_str());
        }
        const size_t requiredBytes =
            (static_cast<size_t>(subjectHoverMaskGridWidth_) *
                subjectHoverMaskGridHeight_ + 7) / 8;
        if (!HexDecode(encoded, subjectHoverMaskBits_) ||
            subjectHoverMaskBits_.size() != requiredBytes) {
            subjectHoverMaskBits_.clear();
            subjectHoverMaskGridWidth_ = 0;
            subjectHoverMaskGridHeight_ = 0;
            subjectHoverRegionConfigured_ = false;
            subjectHoverTrackingReferenceValid_ = false;
            subjectHoverMaskDirty_ = false;
            return;
        }
        subjectHoverTrackingReferenceCenterX_ =
            ReadConfigInt(L"opacity", L"subject_hover_track_center_x_milli", 0) / 1000.0;
        subjectHoverTrackingReferenceCenterY_ =
            ReadConfigInt(L"opacity", L"subject_hover_track_center_y_milli", 0) / 1000.0;
        subjectHoverTrackingReferenceWidth_ =
            ReadConfigInt(L"opacity", L"subject_hover_track_width_milli", 0) / 1000.0;
        subjectHoverTrackingReferenceHeight_ =
            ReadConfigInt(L"opacity", L"subject_hover_track_height_milli", 0) / 1000.0;
        subjectHoverTrackingReferenceValid_ =
            subjectHoverTrackingReferenceWidth_ > 0.0 &&
            subjectHoverTrackingReferenceHeight_ > 0.0;
        subjectHoverTrackingCurrentCenterX_ = subjectHoverTrackingReferenceCenterX_;
        subjectHoverTrackingCurrentCenterY_ = subjectHoverTrackingReferenceCenterY_;
        subjectHoverTrackingScale_ = 1.0;
        subjectHoverMaskDirty_ = false;
    }

    void SaveSubjectHoverMask() const {
        // At high resolutions this data can be hundreds of kilobytes. Most
        // settings changes do not alter it, so do not rewrite it with every
        // generic SaveUiSettings call.
        if (!subjectHoverMaskDirty_) return;
        const int previousChunks = ReadConfigInt(
            L"opacity", L"subject_hover_mask_chunks", 0);
        const bool valid = subjectHoverRegionConfigured_ &&
            subjectHoverMaskGridWidth_ > 0 && subjectHoverMaskGridHeight_ > 0 &&
            !subjectHoverMaskBits_.empty();
        const std::wstring encoded = valid
            ? HexEncode(subjectHoverMaskBits_) : std::wstring{};
        const int chunks = encoded.empty() ? 0 : static_cast<int>(
            (encoded.size() + kSubjectMaskIniChunkChars - 1) /
            kSubjectMaskIniChunkChars);
        WriteConfigInt(L"opacity", L"subject_hover_mask_width",
            valid ? subjectHoverMaskGridWidth_ : 0);
        WriteConfigInt(L"opacity", L"subject_hover_mask_height",
            valid ? subjectHoverMaskGridHeight_ : 0);
        WriteConfigInt(L"opacity", L"subject_hover_track_center_x_milli",
            valid && subjectHoverTrackingReferenceValid_
                ? static_cast<int>(std::lround(subjectHoverTrackingReferenceCenterX_ * 1000.0))
                : 0);
        WriteConfigInt(L"opacity", L"subject_hover_track_center_y_milli",
            valid && subjectHoverTrackingReferenceValid_
                ? static_cast<int>(std::lround(subjectHoverTrackingReferenceCenterY_ * 1000.0))
                : 0);
        WriteConfigInt(L"opacity", L"subject_hover_track_width_milli",
            valid && subjectHoverTrackingReferenceValid_
                ? static_cast<int>(std::lround(subjectHoverTrackingReferenceWidth_ * 1000.0))
                : 0);
        WriteConfigInt(L"opacity", L"subject_hover_track_height_milli",
            valid && subjectHoverTrackingReferenceValid_
                ? static_cast<int>(std::lround(subjectHoverTrackingReferenceHeight_ * 1000.0))
                : 0);
        WriteConfigInt(L"opacity", L"subject_hover_mask_chunks", chunks);
        for (int i = 0; i < chunks; ++i) {
            const size_t offset = static_cast<size_t>(i) * kSubjectMaskIniChunkChars;
            const std::wstring key = L"subject_hover_mask_" + std::to_wstring(i);
            WriteConfigString(L"opacity", key.c_str(),
                encoded.substr(offset, kSubjectMaskIniChunkChars));
        }
        for (int i = chunks; i < previousChunks; ++i) {
            const std::wstring key = L"subject_hover_mask_" + std::to_wstring(i);
            WriteConfigString(L"opacity", key.c_str(), L"");
        }
        subjectHoverMaskDirty_ = false;
    }

    static int DefaultUiScalePercent(HMONITOR monitor) {
        MONITORINFO info{sizeof(info)};
        if (GetMonitorInfoW(monitor, &info) && info.rcMonitor.right - info.rcMonitor.left == 3840 &&
            info.rcMonitor.bottom - info.rcMonitor.top == 2160) return 140;
        return kDefaultUiScalePercent;
    }

    void LoadUiSettings() {
        shortcutApp_ = ReadConfigString(L"launcher", L"path");
        LoadImageSettings();
        LoadShadowSettings();
        cropAspectLocked_ = ReadConfigInt(L"crop", L"lock_aspect", 0) != 0;
        LoadFeatherEdges();
        RECT crop{};
        const std::wstring savedCrop = ReadConfigString(L"crop", L"rect");
        if (swscanf_s(savedCrop.c_str(), L"%ld,%ld,%ld,%ld",
                &crop.left, &crop.top, &crop.right, &crop.bottom) == 4 &&
            crop.left >= 0 && crop.top >= 0 && crop.right <= kCropExtent &&
            crop.bottom <= kCropExtent && !IsRectEmpty(&crop)) {
            cropSelection_ = crop;
        }
        const UiLanguage detectedLanguage = vtsfloat::i18n::DetectSystemLanguage();
        const std::wstring savedLanguage = ReadConfigString(L"ui", L"language");
        vtsfloat::i18n::SetLanguage(vtsfloat::i18n::LanguageFromCode(
            savedLanguage, detectedLanguage));
        const int savedUiScalePercent = (std::clamp)(
            ReadConfigInt(L"ui", L"scale_percent", DefaultUiScalePercent(
                MonitorFromPoint(POINT{ReadConfigInt(L"window", L"x", 0), ReadConfigInt(L"window", L"y", 0)},
                    MONITOR_DEFAULTTOPRIMARY))),
            kMinimumUiScalePercent, kMaximumUiScalePercent);
        uiScalePercent_ = (std::clamp)(
            static_cast<int>(std::lround(
                savedUiScalePercent / static_cast<double>(kUiScaleStepPercent))) *
                kUiScaleStepPercent,
            kMinimumUiScalePercent, kMaximumUiScalePercent);
        // Every normal launch restores the saved bounds in edit mode. Locking
        // is a runtime action only and never hides the GUI on the next launch.
        locked_ = false;
        apiOptedOut_ = ReadConfigInt(L"VtsApi", L"disabled", 0) != 0;
        toolbarGuideSeen_ = ReadConfigInt(L"ui", L"toolbar_guide_seen", 0) != 0;
        desktopMode_ = ReadConfigInt(L"window", L"desktop_mode", 0) != 0;
        pauseOnFullscreen_ = ReadConfigInt(L"render", L"pause_on_fullscreen", 1) != 0;
        debugMode_ = ReadConfigInt(L"debug", L"enabled", 0) != 0;
        bgWarningPermanentlyDismissed_ = ReadConfigInt(L"warnings", L"suppress_bg_opaque", 0) != 0;
        viewSettingsWarningDismissed_ = ReadConfigInt(L"warnings", L"suppress_view_settings", 0) != 0;
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
        fadeShadowOnHover_ = ReadConfigInt(L"opacity", L"fade_shadow_on_hover", 0) != 0;
        hoverFadeEnabled_ = ReadConfigInt(L"opacity", L"locked_hover_fade", 1) != 0;
        hoverOpacityPercent_ = (std::clamp)(
            ReadConfigInt(
                L"opacity", L"locked_hover_percent", kDefaultHoverOpacityPercent),
            kMinimumHoverOpacityPercent,
            100);
        hoverOpacityRestoreDelaySeconds_ = ReadConfigNonNegativeDecimal(
            L"opacity", L"hover_restore_delay_seconds",
            kDefaultHoverOpacityRestoreDelaySeconds);
        modelOpacityPercent_ = (std::clamp)(
            ReadConfigInt(L"opacity", L"model_percent", 100), 10, 100);
        hoverExpandPx_ = (std::clamp)(
            ReadConfigInt(L"opacity", L"hover_expand_px", 0), -500, 500);
        excludeEffectsFromHover_ = ReadConfigInt(
            L"opacity", L"exclude_effects_from_hover", 0) != 0;
        subjectHoverRegionConfigured_ = ReadConfigInt(
            L"opacity", L"subject_hover_region_configured", 0) != 0;
        subjectHoverRegionLeft_ = (std::clamp)(
            ReadConfigInt(L"opacity", L"subject_hover_region_left", 0), 0, 10000);
        subjectHoverRegionTop_ = (std::clamp)(
            ReadConfigInt(L"opacity", L"subject_hover_region_top", 0), 0, 10000);
        subjectHoverRegionRight_ = (std::clamp)(
            ReadConfigInt(L"opacity", L"subject_hover_region_right", 10000), 0, 10000);
        subjectHoverRegionBottom_ = (std::clamp)(
            ReadConfigInt(L"opacity", L"subject_hover_region_bottom", 10000), 0, 10000);
        if (subjectHoverRegionLeft_ >= subjectHoverRegionRight_ ||
            subjectHoverRegionTop_ >= subjectHoverRegionBottom_) {
            subjectHoverRegionConfigured_ = false;
        }
        LoadSubjectHoverMask();
        hoverExpressionEnabled_ = ReadConfigInt(
            L"expression", L"hover_enabled", 0) != 0;
        hoverExpressionFiles_.clear();
        const int expressionFileCount = ReadConfigInt(
            L"expression", L"hover_file_count", -1);
        if (expressionFileCount >= 0) {
            for (int i = 0; i < (std::min)(expressionFileCount, 128); ++i) {
                const std::wstring key = L"hover_file_" + std::to_wstring(i);
                const std::string file = WideToUtf8(
                    ReadConfigString(L"expression", key.c_str()).c_str());
                if (!file.empty() && std::find(
                        hoverExpressionFiles_.begin(), hoverExpressionFiles_.end(),
                        file) == hoverExpressionFiles_.end()) {
                    hoverExpressionFiles_.push_back(file);
                }
            }
        } else {
            // Migrate the Beta 1.0.3 single-expression setting.
            const std::string legacyFile = WideToUtf8(ReadConfigString(
                L"expression", L"hover_file").c_str());
            if (!legacyFile.empty()) hoverExpressionFiles_.push_back(legacyFile);
        }
        hoverExpressionDurationSeconds_ = ReadConfigNonNegativeDecimal(
            L"expression", L"hover_duration_seconds",
            kDefaultHoverExpressionDurationSeconds);
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

    bool SaveBorderMode() const {
        return WriteConfigInt(L"border", L"schema", 2) &&
            WriteConfigInt(L"border", L"mode", borderMode_);
    }

    void SaveUiSettings() const {
        WriteConfigInt(L"window", L"desktop_mode", desktopMode_ ? 1 : 0);
        WriteConfigInt(L"render", L"pause_on_fullscreen", pauseOnFullscreen_ ? 1 : 0);
        WriteConfigString(
            L"ui", L"language",
            vtsfloat::i18n::LanguageCode(vtsfloat::i18n::GetLanguage()));
        WriteConfigInt(L"ui", L"scale_percent", uiScalePercent_);
        WriteConfigInt(L"debug", L"enabled", debugMode_ ? 1 : 0);
        WriteConfigInt(L"VtsApi", L"disabled", apiOptedOut_);
        WriteConfigInt(L"gpu", L"adapter_index", selectedGpuIndex_);
        WriteConfigInt(L"render", L"fps", targetFps_);
        WriteConfigInt(L"render", L"fps_mode", static_cast<int>(fpsMode_));
        WriteConfigInt(
            L"render", L"follow_monitor",
            fpsMode_ == FpsMode::FollowMonitor ? 1 : 0);
        WriteConfigInt(L"render", L"scaling_quality", scalingQuality_);
        WriteConfigInt(L"opacity", L"fade_shadow_on_hover", fadeShadowOnHover_);
        WriteConfigInt(L"opacity", L"locked_hover_fade", hoverFadeEnabled_ ? 1 : 0);
        WriteConfigInt(L"opacity", L"locked_hover_percent", hoverOpacityPercent_);
        WriteConfigString(
            L"opacity", L"hover_restore_delay_seconds",
            FormatNonNegativeDecimal(hoverOpacityRestoreDelaySeconds_));
        WriteConfigInt(L"opacity", L"model_percent", modelOpacityPercent_);
        WriteConfigInt(L"opacity", L"hover_expand_px", hoverExpandPx_);
        WriteConfigInt(
            L"opacity", L"exclude_effects_from_hover",
            excludeEffectsFromHover_ ? 1 : 0);
        WriteConfigInt(
            L"opacity", L"subject_hover_region_configured",
            subjectHoverRegionConfigured_ ? 1 : 0);
        WriteConfigInt(L"opacity", L"subject_hover_region_left", subjectHoverRegionLeft_);
        WriteConfigInt(L"opacity", L"subject_hover_region_top", subjectHoverRegionTop_);
        WriteConfigInt(L"opacity", L"subject_hover_region_right", subjectHoverRegionRight_);
        WriteConfigInt(L"opacity", L"subject_hover_region_bottom", subjectHoverRegionBottom_);
        WriteConfigInt(L"expression", L"hover_enabled", hoverExpressionEnabled_ ? 1 : 0);
        WriteConfigInt(L"expression", L"hover_file_count",
            static_cast<int>(hoverExpressionFiles_.size()));
        for (size_t i = 0; i < hoverExpressionFiles_.size(); ++i) {
            const std::wstring key = L"hover_file_" + std::to_wstring(i);
            WriteConfigString(L"expression", key.c_str(),
                Utf8ToWide(hoverExpressionFiles_[i]));
        }
        // Keep the old key readable by earlier builds without making it the
        // source of truth for this multi-select format.
        WriteConfigString(L"expression", L"hover_file",
            hoverExpressionFiles_.empty() ? L"" : Utf8ToWide(hoverExpressionFiles_.front()));
        WriteConfigString(
            L"expression", L"hover_duration_seconds",
            FormatNonNegativeDecimal(hoverExpressionDurationSeconds_));
        WriteConfigInt(L"window", L"lock_aspect", aspectLocked_ ? 1 : 0);
        SaveBorderMode();
        WriteConfigInt(
            L"border", L"custom_color", static_cast<int>(customBorderColor_));
        WriteConfigInt(L"border", L"thickness", borderThickness_);
        SaveSubjectHoverMask();
    }

    void ResetSettingsToDefaults() {
        EndViewEdit();
        CloseFeatherPanel();
        imageTransform_ = {};
        imageTone_ = {};
        imageEffects_ = {};
        shadow_ = {};
        RefreshImageTone();
        cropSelection_ = kFullCrop;
        cropAspectLocked_ = false;
        cropFeather_ = {};
        // This is deliberately scoped to the application's own INI file. It
        // does not touch the VTS installation, Spout, logs, or any user files.
        CancelFpsCapture();
        CancelHotkeyCapture();
        if (toolbarGuideHwnd_) DestroyWindow(toolbarGuideHwnd_);
        if (toolbarHwnd_) {
            KillTimer(toolbarHwnd_, 48);
        }
        if (borderPanelHwnd_ && IsWindow(borderPanelHwnd_)) {
            SendMessageW(borderPanelHwnd_, WM_COMMAND, IDCANCEL, 0);
        }

        const std::filesystem::path path = ConfigPath();
        std::error_code error;
        std::filesystem::remove(path, error);

        vtsfloat::i18n::SetLanguage(vtsfloat::i18n::DetectSystemLanguage());

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
        fadeShadowOnHover_ = false;
        hoverFadeEnabled_ = true;
        hoverOpacityPercent_ = kDefaultHoverOpacityPercent;
        hoverOpacityRestoreDelaySeconds_ =
            kDefaultHoverOpacityRestoreDelaySeconds;
        modelOpacityPercent_ = 100;
        uiScalePercent_ = DefaultUiScalePercent(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTOPRIMARY));
        hoverExpandPx_ = 0;
        excludeEffectsFromHover_ = false;
        subjectHoverRegionConfigured_ = false;
        subjectHoverRegionLeft_ = 0;
        subjectHoverRegionTop_ = 0;
        subjectHoverRegionRight_ = 10000;
        subjectHoverRegionBottom_ = 10000;
        subjectHoverMaskGridWidth_ = 0;
        subjectHoverMaskGridHeight_ = 0;
        subjectHoverMaskBits_.clear();
        subjectHoverMaskDirty_ = true;
        subjectHoverTrackingReferenceValid_ = false;
        subjectHoverTrackingScale_ = 1.0;
        CancelHoverExpression();
        hoverExpressionEnabled_ = false;
        hoverExpressionFiles_.clear();
        hoverExpressionDurationSeconds_ = kDefaultHoverExpressionDurationSeconds;
        aspectLocked_ = true;
        borderMode_ = kBorderModeNormal;
        customBorderColor_ = kDefaultCustomBorderColor;
        borderThickness_ = kDefaultBorderThickness;
        bgWarningPermanentlyDismissed_ = false;
        viewSettingsWarningDismissed_ = false;
        gpuWarningPermanentlyDismissed_ = false;
        gpuWarningShown_ = false;
        statusDismissed_ = false;
        hasReceivedModel_ = false;
        apiStartScheduled_ = false;
        apiStartedAfterModel_ = false;
        apiOptedOut_ = false;
        vtsApi_.RequestStop();
        toolbarGuideSeen_ = false;
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
        lastAuthorizationDenied_ = false;
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
        hoverOpacityCursorWasOverModel_ = false;
        hoverOpacityRestorePending_ = false;
        hoverOpacityRestoreAt_ = Clock::time_point{};
        debugFpsHistory_.clear();
        debugFrameMsHistory_.clear();

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
            SetWindowPos(hwnd_, WindowZOrder(), x, y,
                         kDefaultWidth, kDefaultHeight,
                         SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
        PositionToolbar();
        ++requested_;
        resetFrameSchedule_ = true;
        RenderFrame();

        // The VTube Studio API token and port are in the same INI. Restarting this
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

    HWND WindowZOrder() const { return desktopMode_ ? HWND_NOTOPMOST : HWND_TOPMOST; }

    bool ForegroundIsFullscreen() const {
        HWND foreground = GetForegroundWindow();
        if (!foreground || !IsWindowVisible(foreground) || IsIconic(foreground)) return false;
        DWORD processId = 0;
        GetWindowThreadProcessId(foreground, &processId);
        if (!processId || processId == GetCurrentProcessId()) return false;
        wchar_t className[128]{};
        GetClassNameW(foreground, className, ARRAYSIZE(className));
        if (wcscmp(className, L"Progman") == 0 || wcscmp(className, L"WorkerW") == 0 ||
            wcscmp(className, L"Shell_TrayWnd") == 0 ||
            wcscmp(className, L"Shell_SecondaryTrayWnd") == 0) return false;
        DWORD cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(foreground, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) return false;
        const LONG_PTR style = GetWindowLongPtrW(foreground, GWL_STYLE);
        if (IsZoomed(foreground) && (style & WS_CAPTION) == WS_CAPTION) return false;
        const HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        if (MonitorFromWindow(foreground, MONITOR_DEFAULTTONULL) != monitor) return false;
        MONITORINFO info{ sizeof(MONITORINFO) };
        RECT bounds{};
        if (!GetMonitorInfoW(monitor, &info)) return false;
        if (FAILED(DwmGetWindowAttribute(foreground, DWMWA_EXTENDED_FRAME_BOUNDS,
                &bounds, sizeof(bounds))) && !GetWindowRect(foreground, &bounds)) return false;
        return vtsfloat::CoversMonitor(bounds, info.rcMonitor);
    }

    void UpdateDesktopPause() {
        // Only the UI thread performs detection. Modal rendering reads the atomic flag.
        const bool paused = desktopMode_ && (IsIconic(hwnd_) ||
            (pauseOnFullscreen_ && ForegroundIsFullscreen()));
        if (desktopOutputPaused_.exchange(paused) != paused) {
            resetFrameSchedule_ = true;
            RefreshDebugReadout(false);
            Log(paused ? "[desktop] output paused: fullscreen or minimized" : "[desktop] output resumed");
        }
    }

    void ToggleDesktopMode() {
        CancelHotkeyCapture();
        desktopMode_ = !desktopMode_;
        ApplyClickThrough();
        UpdateDesktopPause();
        SaveUiSettings();
        resetFrameSchedule_ = true;
        Log(desktopMode_ ? "[window] desktop mode" : "[window] floating mode");
    }

    int PreferredWideToolbarWidth(int modelWidth) const {
        const int singleRowWidth = UiScaled(kToolbarWideMinimumWidth);
        return uiScalePercent_ <= kToolbarCompactWidthMaxScale
            ? singleRowWidth : (std::max)(singleRowWidth, modelWidth);
    }

    void PositionToolbar() {
        if (!toolbarHwnd_ || !hwnd_ || !IsWindow(hwnd_)) {
            return;
        }
        RECT overlay{};
        if (!GetWindowRect(hwnd_, &overlay)) return;
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (!GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitorInfo)) {
            return;
        }
        const RECT work = monitorInfo.rcWork;
        const bool narrowLocked = locked_ && updateBannerText_.empty();
        const int modelWidth = overlay.right - overlay.left;
        const bool modelVisible = hasReceivedModel_ && statusMode_ == VtsStatusMode::Hidden;
        const int minimumToolbarWidth = viewEditing_ ? UiScaled(680)
            : (!locked_ && (CropActive() || imageTransform_.active()) ? UiScaled(560) : 0);
        const int preferredWidth = narrowLocked
            ? UiScaled(92 + (debugMode_ ? kLockedDebugLabelWidth : 0))
            : (modelVisible ? (std::max)(modelWidth, minimumToolbarWidth)
                : PreferredWideToolbarWidth(modelWidth));
        const int width = (std::min)(preferredWidth, static_cast<int>(work.right - work.left));
        if (width <= 0) return;
        if (viewEditing_) {
            cropToolbarLayout_ = BuildCropToolbarLayout(width);
        } else if (!locked_) {
            toolbarLayout_ = BuildToolbarLayout(width * 100 / uiScalePercent_);
        }
        const int height = ToolbarCurrentHeight();
        const int desiredY = overlay.top - height - UiScaled(kToolbarGap);
        const int y = (std::max)(static_cast<int>(work.top), desiredY);
        const int x = (std::clamp)(static_cast<int>(overlay.left),
            static_cast<int>(work.left), static_cast<int>(work.right) - width);
        const bool show = overlayVisible_ && (!locked_ || debugMode_ || UpdateNotificationHeight() > 0);
        const bool visible = (GetWindowLongPtrW(toolbarHwnd_, GWL_STYLE) & WS_VISIBLE) != 0;
        const UINT visibility = show == visible ? 0 : (show ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
        const UINT toolbarFlags = SWP_NOACTIVATE | visibility;
        RECT statusBounds{};
        RECT currentStatus{};
        if (overlayVisible_ && statusMode_ != VtsStatusMode::Hidden && !statusDismissed_ &&
            statusHwnd_ && IsWindowVisible(statusHwnd_) && GetWindowRect(statusHwnd_, &currentStatus)) {
            const bool docked = !locked_ && show;
            const RECT anchor = docked ? RECT{x, y, x + width, y + height} : overlay;
            statusBounds = VtsStatusBounds(overlay, anchor, work, docked);
        }
        const bool moveStatus = !IsRectEmpty(&statusBounds) &&
            (statusBounds.left != currentStatus.left || statusBounds.top != currentStatus.top);
        const RECT toolbarBounds{x, y, x + width, y + height};
        const UINT moveFlags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;
        struct AttachedWindow { HWND hwnd; std::optional<POINT> position; };
        const AttachedWindow attached[]{
            {statusHwnd_, moveStatus ? std::optional<POINT>{{statusBounds.left, statusBounds.top}} : std::nullopt},
            {borderPanelHwnd_, DockedPersonalPanelPosition(overlay, work, show ? &toolbarBounds : nullptr)},
            {featherPanelHwnd_, show ? DockedPanelPosition(featherPanelHwnd_, featherPanelDetached_, toolbarBounds, work) : std::nullopt},
            {imagePanelHwnd_, show ? DockedPanelPosition(imagePanelHwnd_, imagePanelDetached_, toolbarBounds, work) : std::nullopt},
            {shadowPanelHwnd_, show ? DockedPanelPosition(shadowPanelHwnd_, shadowPanelDetached_, toolbarBounds, work) : std::nullopt},
        };
        int moves = 0;
        for (const auto& window : attached) if (window.position) ++moves;
        bool positionedTogether = false;
        if (moves) {
            HDWP batch = BeginDeferWindowPos(1 + moves);
            if (batch) batch = DeferWindowPos(batch, toolbarHwnd_, WindowZOrder(),
                x, y, width, height, toolbarFlags);
            for (const auto& window : attached) {
                if (batch && window.position) batch = DeferWindowPos(batch, window.hwnd, nullptr,
                    window.position->x, window.position->y, 0, 0, moveFlags);
            }
            positionedTogether = batch && EndDeferWindowPos(batch);
        }
        if (!positionedTogether) {
            SetWindowPos(toolbarHwnd_, WindowZOrder(), x, y, width, height, toolbarFlags);
            for (const auto& window : attached) {
                if (window.position) SetWindowPos(window.hwnd, nullptr,
                    window.position->x, window.position->y, 0, 0, moveFlags);
            }
        }
        if (!IsRectEmpty(&statusBounds) &&
            (statusBounds.right - statusBounds.left != currentStatus.right - currentStatus.left ||
             statusBounds.bottom - statusBounds.top != currentStatus.bottom - currentStatus.top)) {
            PresentVtsStatus();
        }
        if (cropEditing_) PositionCropReadout(overlay, work);
        PositionToolbarHint();
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
        PositionUpdateBanner();
        RefreshDebugReadout(false);
        PositionToolbarGuide();
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
        SetWindowPos(hwnd_, WindowZOrder(), x, y, width, height,
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

        EndViewEdit();
        SetOverlayVisible(true);
        SetLocked(false);
        const int width = kDefaultWidth;
        // Keep the sender's aspect when recentering a live, aspect-locked model.
        const RECT source = DisplayImageBounds();
        const LONG sourceWidth = source.right - source.left;
        const LONG sourceHeight = source.bottom - source.top;
        const int height = aspectLocked_ && sourceWidth && sourceHeight
            ? (std::max)(1, static_cast<int>(
                (static_cast<std::uint64_t>(width) * sourceHeight +
                    sourceWidth / 2) / sourceWidth))
            : kDefaultHeight;
        const int x = info.rcWork.left +
            (info.rcWork.right - info.rcWork.left - width) / 2;
        const int y = info.rcWork.top +
            (info.rcWork.bottom - info.rcWork.top - height) / 2;
        if ((CropActive() || imageTransform_.active()) && aspectLocked_ && !IsRectEmpty(&source)) {
            FitCropViewport(RECT{x, y, x + width, y + height}, source, monitor);
        } else {
            SetWindowPos(hwnd_, WindowZOrder(), x, y, width, height,
                SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
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
        if (!visible) {
            HideToolbarHint();
            CloseImagePanel(false);
            StopImageBackdrop();
            CloseFeatherPanel();
        }
        if (!visible) EndViewEdit();
        overlayVisible_ = visible;
        if (!visible) {
            if (toolbarGuideHwnd_) FinishToolbarGuide(false);
            if (debugReadoutHwnd_) ShowWindow(debugReadoutHwnd_, SW_HIDE);
            if (statusHwnd_) ShowWindow(statusHwnd_, SW_HIDE);
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
            hwnd_, WindowZOrder(), 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        PositionToolbar();
        if (toolbarHwnd_ && IsWindowVisible(toolbarHwnd_)) {
            RedrawWindow(toolbarHwnd_, nullptr, nullptr,
                RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        }
        ++requested_;
        RenderFrame();
        resetFrameSchedule_ = true;
        Log("[tray] overlay shown");
    }

    void ToggleOverlayVisibility() {
        SetOverlayVisible(!overlayVisible_);
    }

    enum class ToolbarGuideStep { Done, Shortcut, Scale };

    ToolbarButton ToolbarGuideTarget() const {
        switch (toolbarGuideStep_) {
        case ToolbarGuideStep::Shortcut: return ToolbarButton::Hotkey;
        case ToolbarGuideStep::Scale: return ToolbarButton::CropUiScale;
        case ToolbarGuideStep::Done: return ToolbarButton::Lock;
        }
        return ToolbarButton::Lock;
    }

    const wchar_t* ToolbarGuideNextLabel() const {
        return toolbarGuideStep_ == ToolbarGuideStep::Scale ? Tr(L"知道了") : Tr(L"下一步");
    }

    bool BeginToolbarGuide() {
        toolbarGuideHwnd_ = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kToolbarGuideClass, kWindowTitle,
            WS_POPUP | WS_CLIPCHILDREN, 0, 0, 1, 1, toolbarHwnd_,
            nullptr, instance_, this);
        if (!toolbarGuideHwnd_) return false;
        const auto control = [this](const wchar_t* type, DWORD style, UINT command = 0) {
            return CreateWindowExW(0, type, L"", WS_CHILD | WS_VISIBLE | style,
                0, 0, 0, 0, toolbarGuideHwnd_,
                reinterpret_cast<HMENU>(static_cast<UINT_PTR>(command)), instance_, nullptr);
        };
        toolbarGuideTitle_ = control(L"STATIC", SS_CENTERIMAGE | SS_NOPREFIX);
        toolbarGuideText_ = control(L"STATIC", SS_NOPREFIX | SS_EDITCONTROL);
        toolbarGuideNext_ = control(L"BUTTON", BS_OWNERDRAW, kGuideContinueCommand);
        toolbarGuideClose_ = control(L"BUTTON", BS_OWNERDRAW, kGuideCloseCommand);
        if (!toolbarGuideTitle_ || !toolbarGuideText_ || !toolbarGuideNext_ || !toolbarGuideClose_) {
            DestroyWindow(toolbarGuideHwnd_);
            return false;
        }
        toolbarGuideStep_ = ToolbarGuideStep::Done;
        toolbarGuideAnimationStarted_ = {};
        LayoutToolbarGuide();
        return true;
    }

    void LayoutToolbarGuide() {
        if (!toolbarGuideHwnd_) return;
        const int padding = UiScaled(14);
        const int header = UiScaled(35);
        const int footer = UiScaled(34);
        const int closeSize = UiScaled(26);
        MONITORINFO monitor{sizeof(MONITORINFO)};
        GetMonitorInfoW(MonitorFromWindow(toolbarHwnd_, MONITOR_DEFAULTTONEAREST), &monitor);
        toolbarGuideWidth_ = (std::min)(UiScaled(376),
            static_cast<int>(monitor.rcWork.right - monitor.rcWork.left));
        if (toolbarGuideWidth_ <= padding * 2) toolbarGuideWidth_ = UiScaled(376);
        const auto font = [this](int size, int weight) {
            return CreateFontW(-UiScaled(size), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        };
        HFONT titleFont = font(16, FW_SEMIBOLD);
        HFONT bodyFont = font(14, FW_NORMAL);
        SendMessageW(toolbarGuideTitle_, WM_SETFONT, reinterpret_cast<WPARAM>(titleFont), FALSE);
        SendMessageW(toolbarGuideText_, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont), FALSE);
        if (toolbarGuideTitleFont_) DeleteObject(toolbarGuideTitleFont_);
        if (toolbarGuideBodyFont_) DeleteObject(toolbarGuideBodyFont_);
        toolbarGuideTitleFont_ = titleFont;
        toolbarGuideBodyFont_ = bodyFont;

        const wchar_t* title = Tr(L"完成");
        std::wstring body;
        switch (toolbarGuideStep_) {
        case ToolbarGuideStep::Done:
            body = std::wstring(Tr(L"点击「完成」会锁定悬浮窗口并隐藏工具栏。")) + L"\r\n" +
                Tr(L"如果担心找不到界面，你可以在系统托盘中进行重置恢复。");
            break;
        case ToolbarGuideStep::Shortcut:
            title = Tr(L"快捷键");
            body = std::wstring(Tr(L"重新打开工具栏的快捷键：")) + HotkeyText() + L"\r\n" +
                Tr(L"点击「快捷键」可以自定义组合键。");
            break;
        case ToolbarGuideStep::Scale:
            title = Tr(L"界面缩放");
            body = Tr(L"如果觉得界面太大或者太小，可以点击此处自行设置缩放。");
            break;
        }
        const wchar_t* next = ToolbarGuideNextLabel();
        SetWindowTextW(toolbarGuideTitle_, title);
        SetWindowTextW(toolbarGuideText_, body.c_str());
        SetWindowTextW(toolbarGuideNext_, next);
        SetWindowTextW(toolbarGuideClose_, L"×");
        HDC dc = GetDC(toolbarGuideHwnd_);
        HGDIOBJ previous = bodyFont ? SelectObject(dc, bodyFont) : nullptr;
        RECT measured{0, 0, toolbarGuideWidth_ - 2 * padding, 0};
        DrawTextW(dc, body.c_str(), -1, &measured, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        SIZE nextSize{};
        GetTextExtentPoint32W(dc, next, static_cast<int>(wcslen(next)), &nextSize);
        if (previous) SelectObject(dc, previous);
        ReleaseDC(toolbarGuideHwnd_, dc);
        const int textHeight = (std::max)(UiScaled(20), static_cast<int>(measured.bottom));
        const int footerTop = header + textHeight + UiScaled(12);
        toolbarGuideHeight_ = footerTop + footer + UiScaled(10);
        const int buttonWidth = (std::min)(toolbarGuideWidth_ - 2 * padding,
            (std::max)(UiScaled(90), static_cast<int>(nextSize.cx) + 2 * padding));
        MoveWindow(toolbarGuideTitle_, padding, 0,
            toolbarGuideWidth_ - padding * 2 - closeSize, header, TRUE);
        MoveWindow(toolbarGuideClose_, toolbarGuideWidth_ - closeSize - UiScaled(5),
            UiScaled(4), closeSize, closeSize, TRUE);
        MoveWindow(toolbarGuideText_, padding, header,
            toolbarGuideWidth_ - padding * 2, textHeight, TRUE);
        MoveWindow(toolbarGuideNext_, toolbarGuideWidth_ - padding - buttonWidth, footerTop,
            buttonWidth, footer, TRUE);
        PositionToolbarGuide();
        InvalidateRect(toolbarGuideHwnd_, nullptr, FALSE);
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }

    void PositionToolbarGuide() {
        if (!toolbarGuideHwnd_ || !overlayVisible_ || locked_ || !IsWindowVisible(toolbarHwnd_)) return;
        RECT anchor = ToolbarButtonRect(ToolbarGuideTarget());
        MapWindowPoints(toolbarHwnd_, nullptr, reinterpret_cast<POINT*>(&anchor), 2);
        RECT toolbar{};
        GetWindowRect(toolbarHwnd_, &toolbar);
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (!GetMonitorInfoW(MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST), &monitor)) return;
        const RECT work = monitor.rcWork;
        const int maxX = (std::max)(work.left, work.right - toolbarGuideWidth_);
        const int gap = UiScaled(8);
        int x = (std::clamp)(static_cast<int>((anchor.left + anchor.right - toolbarGuideWidth_) / 2),
            static_cast<int>(work.left), maxX);
        int y = toolbar.bottom + gap + toolbarGuideHeight_ <= work.bottom
            ? toolbar.bottom + gap
            : (std::max)(work.top, toolbar.top - gap - toolbarGuideHeight_);
        if (toolbarGuideAnimationStarted_ != Clock::time_point{}) {
            const double progress = (std::min)(1.0,
                std::chrono::duration<double, std::milli>(
                    Clock::now() - toolbarGuideAnimationStarted_).count() / 220.0);
            const double remaining = 1.0 - progress;
            x += static_cast<int>(std::lround((toolbarGuideFrom_.x - x) * remaining * remaining));
            y += static_cast<int>(std::lround((toolbarGuideFrom_.y - y) * remaining * remaining));
            if (progress >= 1.0) {
                toolbarGuideAnimationStarted_ = {};
                KillTimer(toolbarHwnd_, kToolbarGuideTimer);
            }
        }
        RECT current{};
        GetWindowRect(toolbarGuideHwnd_, &current);
        if (!IsWindowVisible(toolbarGuideHwnd_) || current.left != x || current.top != y ||
            current.right - current.left != toolbarGuideWidth_ ||
            current.bottom - current.top != toolbarGuideHeight_) {
            SetWindowPos(toolbarGuideHwnd_, WindowZOrder(), x, y,
                toolbarGuideWidth_, toolbarGuideHeight_, SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        }
    }

    void ContinueToolbarGuide() {
        if (toolbarGuideStep_ == ToolbarGuideStep::Scale) {
            FinishToolbarGuide(true);
            return;
        }
        RECT current{};
        GetWindowRect(toolbarGuideHwnd_, &current);
        toolbarGuideFrom_ = POINT{current.left, current.top};
        toolbarGuideStep_ = toolbarGuideStep_ == ToolbarGuideStep::Done
            ? ToolbarGuideStep::Shortcut : ToolbarGuideStep::Scale;
        toolbarGuideAnimationStarted_ = Clock::now();
        SetTimer(toolbarHwnd_, kToolbarGuideTimer, 25, nullptr);
        LayoutToolbarGuide();
    }

    void FinishToolbarGuide(bool lockOverlay) {
        toolbarGuideSeen_ = true;
        WriteConfigInt(L"ui", L"toolbar_guide_seen", 1);
        DestroyWindow(toolbarGuideHwnd_);
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
        if (lockOverlay) ActivateToolbarButton(ToolbarButton::Lock);
    }

    static LRESULT CALLBACK ToolbarGuideProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* overlay = reinterpret_cast<LayeredOverlay*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            overlay = static_cast<LayeredOverlay*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
        }
        if (!overlay) return DefWindowProcW(window, message, wParam, lParam);
        constexpr COLORREF background = RGB(19, 29, 45);
        switch (message) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_COMMAND:
            if (LOWORD(wParam) == kGuideContinueCommand) overlay->ContinueToolbarGuide();
            else if (LOWORD(wParam) == kGuideCloseCommand) overlay->FinishToolbarGuide(false);
            return 0;
        case WM_CLOSE:
            overlay->FinishToolbarGuide(false);
            return 0;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, RGB(230, 239, 252));
            SetBkColor(dc, background);
            SetDCBrushColor(dc, background);
            return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
        }
        case WM_DRAWITEM: {
            const auto& item = *reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            const bool close = item.CtlID == kGuideCloseCommand;
            PanelFill(item.hDC, item.rcItem, close ? background :
                ((item.itemState & ODS_SELECTED) ? RGB(29, 72, 123) : RGB(43, 105, 169)));
            const wchar_t* label = close ? L"×" :
                overlay->ToolbarGuideNextLabel();
            PanelText(item.hDC, label, item.rcItem, RGB(240, 246, 255),
                overlay->UiScaled(14), false, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            return TRUE;
        }
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            RECT rect{};
            GetClientRect(window, &rect);
            PanelFill(dc, rect, background);
            HBRUSH border = CreateSolidBrush(RGB(71, 153, 231));
            FrameRect(dc, &rect, border);
            DeleteObject(border);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_NCDESTROY:
            if (overlay->toolbarGuideTitleFont_) DeleteObject(overlay->toolbarGuideTitleFont_);
            if (overlay->toolbarGuideBodyFont_) DeleteObject(overlay->toolbarGuideBodyFont_);
            overlay->toolbarGuideTitleFont_ = overlay->toolbarGuideBodyFont_ = nullptr;
            overlay->toolbarGuideHwnd_ = nullptr;
            KillTimer(overlay->toolbarHwnd_, kToolbarGuideTimer);
            break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    static void PaintToolbarPopoverBackground(HDC dc, const RECT& bounds) {
        PanelFill(dc, bounds, kToolbarPopoverBackground);
        const HBRUSH border = CreateSolidBrush(RGB(53, 81, 117));
        FrameRect(dc, &bounds, border);
        DeleteObject(border);
    }

    std::optional<RECT> ToolbarPopoverBounds(const RECT& anchor, int preferredWidth, int height) const {
        if (IsRectEmpty(&anchor)) return std::nullopt;
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (!GetMonitorInfoW(MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST), &monitor)) return std::nullopt;
        const RECT work = monitor.rcWork;
        const int width = (std::min)(preferredWidth, static_cast<int>(work.right - work.left));
        const int gap = UiScaled(4);
        const LONG centeredX = anchor.left + (anchor.right - anchor.left - width) / 2;
        const LONG x = (std::clamp)(centeredX, work.left, work.right - width);
        const LONG above = anchor.top - gap - height;
        const LONG desiredY = above >= work.top ? above : anchor.bottom + gap;
        const LONG y = (std::clamp)(desiredY, work.top, (std::max)(work.top, work.bottom - height));
        return RECT{x, y, x + width, y + height};
    }

    void HideGithubDropdown() {
        KillTimer(toolbarHwnd_, kGithubDropdownTimer);
        if (githubDropdownHwnd_) ShowWindow(githubDropdownHwnd_, SW_HIDE);
    }

    void ShowGithubDropdown() {
        if (locked_ || !overlayVisible_ || updateClosing_ ||
            (githubDropdownHwnd_ && IsWindowVisible(githubDropdownHwnd_))) return;
        if (!githubDropdownHwnd_) {
            githubDropdownHwnd_ = CreateWindowExW(
                WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, kGithubDropdownClass, L"GitHub",
                WS_POPUP | WS_CLIPCHILDREN, 0, 0, 1, 1, toolbarHwnd_,
                nullptr, instance_, this);
            if (!githubDropdownHwnd_) return;
            githubVersionLabel_ = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_ENDELLIPSIS | SS_NOPREFIX,
                0, 0, 0, 0, githubDropdownHwnd_, nullptr, instance_, nullptr);
            const auto createButton = [this](UINT command) {
                return CreateWindowExW(0, L"BUTTON", L"",
                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, githubDropdownHwnd_,
                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(command)), instance_, nullptr);
            };
            githubUpdateButton_ = createButton(kCheckUpdateCommand);
            githubDebugButton_ = createButton(kGithubDebugCommand);
            if (!githubVersionLabel_ || !githubUpdateButton_ || !githubDebugButton_) {
                DestroyWindow(githubDropdownHwnd_);
                return;
            }
        }

        const std::wstring version = std::wstring(Tr(L"当前版本：")) + L"v" + VTSFLOAT_VERSION;
        const wchar_t* update = Tr(L"检查更新");
        const wchar_t* debug = Tr(L"调试");
        SetWindowTextW(githubVersionLabel_, version.c_str());
        SetWindowTextW(githubUpdateButton_, update);
        SetWindowTextW(githubDebugButton_, debug);
        HFONT font = CreateFontW(-UiScaled(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        if (font) {
            for (HWND control : {githubVersionLabel_, githubUpdateButton_, githubDebugButton_}) {
                SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
            }
            if (githubDropdownFont_) DeleteObject(githubDropdownFont_);
            githubDropdownFont_ = font;
        }
        HDC dc = GetDC(githubDropdownHwnd_);
        HGDIOBJ previous = githubDropdownFont_ ? SelectObject(dc, githubDropdownFont_) : nullptr;
        const auto measure = [dc](std::wstring_view label) {
            SIZE size{};
            GetTextExtentPoint32W(dc, label.data(), static_cast<int>(label.size()), &size);
            return static_cast<int>(size.cx);
        };
        const int labelWidth = (std::max)({measure(version), measure(update), measure(debug)});
        if (previous) SelectObject(dc, previous);
        ReleaseDC(githubDropdownHwnd_, dc);
        githubDropdownWidth_ = (std::max)(UiScaled(176), labelWidth + UiScaled(24));
        githubDropdownLastHover_ = Clock::now();
        githubHoveredButton_ = nullptr;
        SetTimer(toolbarHwnd_, kGithubDropdownTimer, 25, nullptr);
        UpdateGithubDropdown();
    }

    void UpdateGithubDropdown() {
        if (!githubDropdownHwnd_ || locked_ || !overlayVisible_ ||
            !IsWindowVisible(toolbarHwnd_)) {
            HideGithubDropdown();
            return;
        }
        RECT anchor = ToolbarButtonRect(ToolbarButton::Github);
        MapWindowPoints(toolbarHwnd_, nullptr, reinterpret_cast<POINT*>(&anchor), 2);
        const int rowHeight = UiScaled(32);
        const int height = rowHeight * 3;
        const auto popup = ToolbarPopoverBounds(anchor, githubDropdownWidth_, height);
        if (!popup) {
            HideGithubDropdown();
            return;
        }
        const RECT fullMenu = *popup;
        const int x = fullMenu.left, y = fullMenu.top, width = fullMenu.right - fullMenu.left;
        const auto now = Clock::now();
        POINT cursor{};
        GetCursorPos(&cursor);
        const HWND capture = GetCapture();
        if (PtInRect(&anchor, cursor) || PtInRect(&fullMenu, cursor) ||
            capture == githubUpdateButton_ || capture == githubDebugButton_) {
            githubDropdownLastHover_ = now;
        } else if (now - githubDropdownLastHover_ > std::chrono::milliseconds(180)) {
            HideGithubDropdown();
            return;
        }
        RECT current{};
        GetWindowRect(githubDropdownHwnd_, &current);
        if (!IsWindowVisible(githubDropdownHwnd_) || current.left != x || current.top != y ||
            current.right - current.left != width || current.bottom - current.top != height) {
            const int padding = UiScaled(4);
            MoveWindow(githubVersionLabel_, UiScaled(12), 1, width - UiScaled(24), rowHeight - 1, TRUE);
            MoveWindow(githubUpdateButton_, padding, rowHeight,
                width - 2 * padding, rowHeight, TRUE);
            MoveWindow(githubDebugButton_, padding, rowHeight * 2,
                width - 2 * padding, rowHeight - padding, TRUE);
            SetWindowPos(githubDropdownHwnd_, WindowZOrder(), x, y, width, height,
                SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        }
        HWND hovered = nullptr;
        if (PtInRect(&fullMenu, cursor)) {
            for (HWND button : {githubUpdateButton_, githubDebugButton_}) {
                RECT action{};
                if (GetWindowRect(button, &action) && PtInRect(&action, cursor)) hovered = button;
            }
        }
        if (hovered != githubHoveredButton_) {
            if (githubHoveredButton_) InvalidateRect(githubHoveredButton_, nullptr, FALSE);
            githubHoveredButton_ = hovered;
            if (hovered) InvalidateRect(hovered, nullptr, FALSE);
        }
    }

    static LRESULT CALLBACK GithubDropdownProc(HWND window, UINT message,
                                               WPARAM wParam, LPARAM lParam) {
        auto* overlay = reinterpret_cast<LayeredOverlay*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            overlay = static_cast<LayeredOverlay*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
        }
        if (!overlay) return DefWindowProcW(window, message, wParam, lParam);
        switch (message) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_COMMAND:
            if (HIWORD(wParam) != BN_CLICKED) break;
            if (LOWORD(wParam) == kCheckUpdateCommand) {
                overlay->HideGithubDropdown();
                overlay->BeginUpdateCheck();
                return 0;
            }
            if (LOWORD(wParam) == kGithubDebugCommand) {
                RECT item{};
                if (GetWindowRect(overlay->githubDebugButton_, &item)) {
                    overlay->HideGithubDropdown();
                    overlay->CancelFpsCapture();
                    Log("[toolbar] action=debug_menu");
                    overlay->ShowDebugMenu(POINT{item.left, item.bottom});
                }
                return 0;
            }
            break;
        case WM_SETCURSOR:
            if (reinterpret_cast<HWND>(wParam) == overlay->githubUpdateButton_ ||
                reinterpret_cast<HWND>(wParam) == overlay->githubDebugButton_) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
            break;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, kToolbarPopoverText);
            SetBkColor(dc, kToolbarPopoverBackground);
            SetDCBrushColor(dc, kToolbarPopoverBackground);
            return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
        }
        case WM_DRAWITEM: {
            const auto& item = *reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            const COLORREF color = (item.itemState & ODS_SELECTED) ? RGB(18, 50, 88) :
                (item.hwndItem == overlay->githubHoveredButton_ ? RGB(26, 70, 116) : RGB(24, 42, 65));
            PanelFill(item.hDC, item.rcItem, color);
            RECT text = item.rcItem;
            text.left += overlay->UiScaled(8);
            wchar_t label[256]{};
            GetWindowTextW(item.hwndItem, label, ARRAYSIZE(label));
            PanelText(item.hDC, label, text, RGB(240, 246, 255),
                overlay->UiScaled(14), false, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            return TRUE;
        }
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            RECT bounds{};
            GetClientRect(window, &bounds);
            PaintToolbarPopoverBackground(dc, bounds);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_NCDESTROY:
            if (overlay->githubDropdownFont_) DeleteObject(overlay->githubDropdownFont_);
            overlay->githubDropdownFont_ = nullptr;
            overlay->githubDropdownHwnd_ = nullptr;
            break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    int UpdateNotificationHeight() const {
        return viewEditing_ || updateBannerText_.empty() ? 0 : UiScaled(kToolbarNotificationHeight);
    }

    bool CreateUpdateBanner() {
        if (updateBannerHwnd_) return true;
        updateBannerHwnd_ = CreateWindowExW(
            WS_EX_CONTROLPARENT, kUpdateBannerClass, L"",
            WS_CHILD | WS_CLIPCHILDREN, 0, 0, 1, 1,
            toolbarHwnd_, nullptr, instance_, this);
        if (!updateBannerHwnd_) return false;
        updateBannerLabel_ = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_ENDELLIPSIS | SS_NOPREFIX,
            0, 0, 0, 0, updateBannerHwnd_, nullptr, instance_, nullptr);
        const auto button = [this](UINT command) {
            return CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                0, 0, 0, 0, updateBannerHwnd_,
                reinterpret_cast<HMENU>(static_cast<UINT_PTR>(command)), instance_, nullptr);
        };
        updateLaterButton_ = button(kUpdateLaterCommand);
        updateInstallButton_ = button(kUpdateInstallCommand);
        updateDismissButton_ = button(kUpdateDismissCommand);
        if (updateBannerLabel_ && updateLaterButton_ && updateInstallButton_ &&
            updateDismissButton_) return true;
        DestroyWindow(updateBannerHwnd_);
        updateBannerHwnd_ = nullptr;
        return false;
    }

    void PositionUpdateBanner() {
        if (!updateBannerHwnd_ || updateBannerText_.empty()) return;
        if (viewEditing_) {
            ShowWindow(updateBannerHwnd_, SW_HIDE);
            return;
        }
        RECT toolbar{};
        GetClientRect(toolbarHwnd_, &toolbar);
        const int inset = UiScaled(1);
        const int width = toolbar.right - 2 * inset;
        const int height = UpdateNotificationHeight();
        if (width <= 0) return;

        const int fontHeight = UiScaled(15);
        const std::wstring fontFace(UiFontFace());
        if (!updateBannerFont_ || fontHeight != updateBannerFontHeight_ ||
            fontFace != updateBannerFontFace_) {
            HFONT font = CreateFontW(-fontHeight, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, fontFace.c_str());
            if (font) {
                SendMessageW(updateBannerLabel_, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
                if (updateBannerFont_) DeleteObject(updateBannerFont_);
                updateBannerFont_ = font;
                updateBannerFontHeight_ = fontHeight;
                updateBannerFontFace_ = fontFace;
            }
        }

        const int padding = UiScaled(7);
        const int closeWidth = updateNoticeDismissal_ == UpdateNoticeDismissal::Manual ? UiScaled(25) : 0;
        int laterWidth = 0;
        int installWidth = 0;
        if (updateNoticeStyle_ == UpdateNoticeStyle::Offer) {
            HDC dc = GetDC(updateBannerHwnd_);
            HGDIOBJ oldFont = updateBannerFont_ ? SelectObject(dc, updateBannerFont_) : nullptr;
            const auto measure = [dc, padding](HWND control) {
                wchar_t label[256]{};
                const int length = GetWindowTextW(control, label, ARRAYSIZE(label));
                SIZE size{};
                GetTextExtentPoint32W(dc, label, length, &size);
                return static_cast<int>(size.cx) + 2 * padding;
            };
            laterWidth = measure(updateLaterButton_);
            installWidth = measure(updateInstallButton_);
            if (oldFont) SelectObject(dc, oldFont);
            ReleaseDC(updateBannerHwnd_, dc);
            const int budget = (std::max)(0, width - closeWidth - UiScaled(120));
            if (laterWidth + installWidth > budget) {
                laterWidth = laterWidth * budget / (laterWidth + installWidth);
                installWidth = budget - laterWidth;
            }
        }

        SetWindowPos(updateBannerHwnd_, nullptr, inset, inset, width, height,
            SWP_NOACTIVATE | SWP_NOZORDER);
        int right = width - inset;
        const auto place = [&right, height](HWND control, int controlWidth) {
            right -= controlWidth;
            MoveWindow(control, right, 0, controlWidth, height, TRUE);
        };
        if (closeWidth > 0) place(updateDismissButton_, closeWidth);
        if (updateNoticeStyle_ == UpdateNoticeStyle::Offer) {
            place(updateInstallButton_, installWidth);
            place(updateLaterButton_, laterWidth);
        }
        MoveWindow(updateBannerLabel_, padding, 0,
            (std::max)(0, right - 2 * padding), height, TRUE);
        if (!(GetWindowLongPtrW(updateBannerHwnd_, GWL_STYLE) & WS_VISIBLE)) {
            ShowWindow(updateBannerHwnd_, SW_SHOWNA);
        }
    }

    bool ShowUpdateBanner(
        std::wstring text, UpdateNoticeStyle style = UpdateNoticeStyle::Standard,
        UpdateNoticeDismissal dismissal = UpdateNoticeDismissal::Manual) {
        if (updateNoticeDismissAt_ != Clock::time_point{}) {
            KillTimer(hwnd_, kUpdateNoticeDismissTimer);
            updateNoticeDismissAt_ = {};
        }
        if (text.empty()) {
            updateBannerText_.clear();
            updateNoticeStyle_ = UpdateNoticeStyle::Standard;
            updateNoticeDismissal_ = UpdateNoticeDismissal::None;
            if (updateBannerHwnd_) ShowWindow(updateBannerHwnd_, SW_HIDE);
            PositionToolbar();
            return false;
        }
        if (!CreateUpdateBanner()) return false;
        updateBannerText_ = std::move(text);
        updateNoticeStyle_ = style;
        updateNoticeDismissal_ = dismissal;
        const bool offer = style == UpdateNoticeStyle::Offer;
        SetWindowTextW(updateBannerLabel_, updateBannerText_.c_str());
        if (offer) {
            const auto later = L"[" + std::wstring(Tr(L"暂不更新")) + L"]";
            const auto install = L"[" + std::wstring(Tr(L"更新并重启")) + L"]";
            SetWindowTextW(updateLaterButton_, later.c_str());
            SetWindowTextW(updateInstallButton_, install.c_str());
        }
        SetWindowTextW(updateDismissButton_, L"×");
        ShowWindow(updateLaterButton_, offer ? SW_SHOWNA : SW_HIDE);
        ShowWindow(updateInstallButton_, offer ? SW_SHOWNA : SW_HIDE);
        ShowWindow(updateDismissButton_, dismissal == UpdateNoticeDismissal::Manual ? SW_SHOWNA : SW_HIDE);
        PositionToolbar();
        RedrawWindow(toolbarHwnd_, nullptr, nullptr,
            RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        if (dismissal == UpdateNoticeDismissal::Timed) {
            updateNoticeDismissAt_ = Clock::now() + std::chrono::milliseconds(kUpdateNoticeTimeoutMs);
            SetTimer(hwnd_, kUpdateNoticeDismissTimer, kUpdateNoticeTimeoutMs, nullptr);
        }
        // The notice stays queued if the user hid the overlay during startup.
        return true;
    }

    void BeginUpdateCheck() {
        if (updateClosing_) return;
        if (!updater_) {
            ShowUpdateBanner(Tr(L"检查失败"), UpdateNoticeStyle::Standard, UpdateNoticeDismissal::Timed);
            return;
        }
        if (!updater_->Check(CurrentExecutablePath())) return;
        updateDismissed_ = false;
        SetTimer(hwnd_, kUpdateTimer, 100, nullptr);
        PollUpdate();
    }

    bool SaveSettingsForUpdate() {
        bool saved = true;
        struct RestoreWriteTracking {
            bool* previous;
            ~RestoreWriteTracking() { checkedConfigWrites = previous; }
        } restore{std::exchange(checkedConfigWrites, &saved)};
        if (borderPanelHwnd_ && IsWindow(borderPanelHwnd_)) {
            auto* state = reinterpret_cast<BorderDialogState*>(
                GetWindowLongPtrW(borderPanelHwnd_, GWLP_USERDATA));
            if (state && state->editingHoverOpacityRestoreDelay) {
                ApplyHoverOpacityRestoreDelayInput(borderPanelHwnd_, state);
            }
            if (state && state->editingHoverExpressionDuration) {
                ApplyHoverExpressionDurationInput(borderPanelHwnd_, state);
            }
            EndHoverOpacityPreview();
        }
        SaveUiSettings();
        SaveHotkeySettings();
        SaveWindowPlacement();
        if (!saved) return false;
        const auto config = ConfigPath();
        WritePrivateProfileStringW(nullptr, nullptr, nullptr, config.c_str());
        HANDLE file = CreateFileW(config.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        saved = FlushFileBuffers(file) != FALSE;
        CloseHandle(file);
        return saved;
    }

    void PollUpdate() {
        using vtsfloat::update::Phase;
        if (!updater_ || updateClosing_) return;
        const auto status = updater_->Poll();
        if (status.revision == updateRevision_) return;
        updateRevision_ = status.revision;
        switch (status.phase) {
        case Phase::Checking:
        case Phase::Downloading:
        case Phase::Preparing:
        case Phase::ReadyToExit:
        case Phase::WaitingForSuccess:
        case Phase::ShowSuccess:
            break;
        default:
            KillTimer(hwnd_, kUpdateTimer);
            break;
        }
        if (!status.diagnostic.empty() && status.diagnostic != updateLastDiagnostic_) {
            Log("[update] " + status.diagnostic);
        }
        updateLastDiagnostic_ = status.diagnostic;
        if (updateDismissed_ && status.phase != Phase::ReadyToExit &&
            status.phase != Phase::ShowSuccess) return;
        switch (status.phase) {
        case Phase::Idle:
            ShowUpdateBanner({});
            break;
        case Phase::Checking:
            ShowUpdateBanner(status.proxy ? Tr(L"尝试其他线路") : Tr(L"正在检查更新…"),
                UpdateNoticeStyle::Checking, UpdateNoticeDismissal::None);
            break;
        case Phase::Available:
            ShowUpdateBanner(std::wstring(Tr(L"存在新版本：")) + status.tag + Tr(L"，是否更新？"),
                UpdateNoticeStyle::Offer);
            break;
        case Phase::Current:
            ShowUpdateBanner(Tr(L"当前已是最新版本"),
                UpdateNoticeStyle::Positive, UpdateNoticeDismissal::Timed);
            break;
        case Phase::CheckFailed:
            ShowUpdateBanner(Tr(L"检查失败"), UpdateNoticeStyle::Standard, UpdateNoticeDismissal::Timed);
            break;
        case Phase::Downloading:
            ShowUpdateBanner(std::wstring(status.proxy ? Tr(L"尝试其他线路") :
                Tr(L"正在下载更新…")) + L" " + std::to_wstring(status.percent) + L"%",
                UpdateNoticeStyle::Positive);
            break;
        case Phase::DownloadFailed:
            ShowUpdateBanner(Tr(L"下载更新失败"));
            break;
        case Phase::Preparing:
            ShowUpdateBanner(Tr(L"正在准备更新…"), UpdateNoticeStyle::Positive);
            break;
        case Phase::ReadyToExit: {
            bool saved = false;
            try { saved = SaveSettingsForUpdate(); } catch (...) {}
            if (!saved) {
                updater_->Cancel();
                updateRevision_ = updater_->Poll().revision;
                ShowUpdateBanner(Tr(L"保存设置失败，更新已取消"));
            } else if (updater_->Commit()) {
                updateClosing_ = true;
                EnableWindow(hwnd_, FALSE);
                if (toolbarHwnd_) EnableWindow(toolbarHwnd_, FALSE);
                if (borderPanelHwnd_) EnableWindow(borderPanelHwnd_, FALSE);
                if (updateBannerHwnd_) EnableWindow(updateBannerHwnd_, FALSE);
                PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            }
            break;
        }
        case Phase::WaitingForSuccess:
            break;
        case Phase::ShowSuccess:
            if (ShowUpdateBanner(std::wstring(Tr(L"更新成功，当前版本号为 ")) + status.tag,
                    UpdateNoticeStyle::Positive)) {
                updater_->AcknowledgeSuccess();
            }
            break;
        case Phase::Success:
            ShowUpdateBanner(std::wstring(Tr(L"更新成功，当前版本号为 ")) + status.tag,
                UpdateNoticeStyle::Positive);
            break;
        case Phase::ApplyFailed:
            ShowUpdateBanner(Tr(L"更新失败"));
            break;
        case Phase::RolledBack:
            ShowUpdateBanner(Tr(L"更新失败，已恢复旧版本"));
            break;
        case Phase::RecoveryNeeded:
            ShowUpdateBanner(Tr(L"更新未完成，已保留旧版本备份"));
            break;
        }
    }

    void ActivateUpdateCommand(UINT command) {
        if (updateClosing_) return;
        if (command == kUpdateDismissCommand) {
            if (updateNoticeDismissal_ != UpdateNoticeDismissal::Manual) return;
            updateDismissed_ = true;
            if (updater_) updater_->Dismiss();
            ShowUpdateBanner({});
            return;
        }
        if (!updater_ || updateNoticeStyle_ != UpdateNoticeStyle::Offer) return;
        switch (command) {
        case kUpdateLaterCommand:
            ShowUpdateBanner({});
            updateDismissed_ = true;
            updater_->Dismiss();
            break;
        case kUpdateInstallCommand:
            if (updater_->Download()) {
                SetTimer(hwnd_, kUpdateTimer, 100, nullptr);
                PollUpdate();
            }
            break;
        default:
            break;
        }
    }

    static LRESULT CALLBACK UpdateBannerWindowProc(HWND window, UINT message,
                                                    WPARAM wParam, LPARAM lParam) {
        auto* overlay = reinterpret_cast<LayeredOverlay*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            overlay = static_cast<LayeredOverlay*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
        }
        if (!overlay) return DefWindowProcW(window, message, wParam, lParam);
        COLORREF background = RGB(50, 30, 10);
        COLORREF text = RGB(255, 200, 100);
        if (overlay->updateNoticeStyle_ == UpdateNoticeStyle::Checking) {
            background = RGB(14, 37, 63);
            text = RGB(157, 210, 255);
        } else if (overlay->updateNoticeStyle_ == UpdateNoticeStyle::Positive) {
            background = RGB(15, 50, 22);
            text = RGB(130, 255, 150);
        }
        switch (message) {
        case WM_COMMAND:
            overlay->ActivateUpdateCommand(LOWORD(wParam));
            return 0;
        case WM_CLOSE:
            overlay->ActivateUpdateCommand(kUpdateDismissCommand);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT && GetDlgCtrlID(reinterpret_cast<HWND>(wParam)) > 0) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
            break;
        case DM_GETDEFID:
            if (overlay->updateNoticeDismissal_ != UpdateNoticeDismissal::Manual) return 0;
            return MAKELRESULT(overlay->updateNoticeStyle_ == UpdateNoticeStyle::Offer ? kUpdateLaterCommand :
                kUpdateDismissCommand, DC_HASDEFID);
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, text);
            SetBkColor(dc, background);
            SetDCBrushColor(dc, background);
            return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
        }
        case WM_DRAWITEM: {
            const auto& item = *reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            const bool pressed = (item.itemState & ODS_SELECTED) != 0;
            PanelFill(item.hDC, item.rcItem, background);
            wchar_t label[256]{};
            GetWindowTextW(item.hwndItem, label, ARRAYSIZE(label));
            RECT bounds = item.rcItem;
            HGDIOBJ previous = overlay->updateBannerFont_
                ? SelectObject(item.hDC, overlay->updateBannerFont_) : nullptr;
            SetTextColor(item.hDC, pressed ? RGB(170, 217, 255) : RGB(100, 180, 255));
            SetBkMode(item.hDC, TRANSPARENT);
            DrawTextW(item.hDC, label, -1, &bounds,
                DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
            if (item.itemState & ODS_FOCUS) {
                InflateRect(&bounds, -2, -2);
                DrawFocusRect(item.hDC, &bounds);
            }
            if (previous) SelectObject(item.hDC, previous);
            return TRUE;
        }
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            RECT bounds{};
            GetClientRect(window, &bounds);
            PanelFill(dc, bounds, background);
            EndPaint(window, &paint);
            return 0;
        }
        default:
            break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
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
                    Tr(L"重置到主屏幕中间"));
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(
            menu, MF_STRING, kTrayToggleVisibilityCommand,
            overlayVisible_ ? Tr(L"暂时隐藏") : Tr(L"显示窗口"));
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kTrayExitCommand, Tr(L"退出程序"));

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

    std::wstring GpuName(int adapterIndex) const {
        const auto iterator = std::find_if(
            gpuAdapters_.begin(), gpuAdapters_.end(),
            [adapterIndex](const GpuAdapterInfo& info) {
                return static_cast<int>(info.index) == adapterIndex;
            });
        return iterator == gpuAdapters_.end() ? Tr(L"未知 GPU") : iterator->name;
    }

    std::wstring GpuButtonText() const {
        if (selectedGpuIndex_ < 0) {
            if (minimumPowerGpuIndex_ >= 0) {
                return std::wstring(Tr(L"GPU 推荐：")) + GpuName(minimumPowerGpuIndex_);
            }
            return Tr(L"GPU 推荐（未检测到核显）");
        }
        if (gpuSelectionFallback_) {
            return std::wstring(Tr(L"GPU 已选：")) + GpuName(selectedGpuIndex_) + Tr(L"（当前回退）");
        }
        return std::wstring(L"GPU") + Tr(L"：") + GpuName(activeGpuIndex_);
    }

    std::wstring DebugStatusText(bool wrapped) const {
        std::wostringstream text;
        text << std::fixed << std::setprecision(1)
             << L"FPS " << lastUpdateFps_;
        if (vtsApiFps_ > 0) {
            text << L"  VTS " << vtsApiFps_;
        }
        if (!wrapped) text << L"  " << GpuTelemetryText();
        text << std::setprecision(2)
             << Tr(L"  接收 ") << lastReceiveMs_ << L"ms"
             << Tr(L"  缩放 ") << lastMapScaleMs_ << L"ms"
             << Tr(L"  上屏 ") << lastUpdateMs_ << L"ms";
        if (wrapped) text << L"\n" << GpuTelemetryText();
        if (debugMode_) {
            text << L"\n";
            if (sourceWidth_ > 0 && dibWidth_ > 0) {
                const double srcPx = static_cast<double>(sourceWidth_) * sourceHeight_;
                const double dstPx = static_cast<double>(dibWidth_) * dibHeight_;
                const double ratio = (std::min)(srcPx, dstPx) / (std::max)(srcPx, dstPx) * 100.0;
                text << Tr(L"渲染 ") << dibWidth_ << L"x" << dibHeight_
                     << L"  VTS " << sourceWidth_ << L"x" << sourceHeight_
                     << std::setprecision(1) << Tr(L"  对齐 ") << ratio << L"%";
            }
            const auto extra = vtsApi_.GetExtraStats();
            if (vtsApi_.IsConnected() && extra.uptimeMs > 0) {
                const long long s = extra.uptimeMs / 1000;
                text << L"  API " << std::fixed << std::setprecision(1)
                     << extra.apiLatencyMs << L"ms"
                     << Tr(L"  面数 ") << extra.artmeshCount
                     << Tr(L"  道具 ") << extra.itemCount;
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
            text << Tr(L"  显存 ")
                 << std::setprecision(0)
                 << static_cast<double>(gpuMemoryCurrentBytes_) / kMegabyte
                 << L"/"
                 << static_cast<double>(gpuMemoryBudgetBytes_) / kMegabyte
                 << L" MB";
        }
        if (processMemoryBytes_ > 0) {
            constexpr double kMegabyte = 1024.0 * 1024.0;
            text << Tr(L"  内存 ")
                 << std::setprecision(0)
                 << static_cast<double>(processMemoryBytes_) / kMegabyte
                 << L" MB";
        }
        return text.str();
    }

    std::wstring FpsButtonText() const {
        if (capturingFps_) {
            return fpsInput_.empty()
                ? Tr(L"输入 FPS：_")
                : std::wstring(Tr(L"输入 FPS：")) + fpsInput_ + L"_";
        }
        if (fpsMode_ == FpsMode::FollowVtsApi) {
            return Tr(L"对齐 VTS 渲染");
        }
        if (fpsMode_ == FpsMode::FollowVts) {
            return Tr(L"跟随 VTS 配置");
        }
        if (fpsMode_ == FpsMode::FollowMonitor) {
            return std::wstring(Tr(L"跟随屏幕 ")) + std::to_wstring(monitorRefreshFps_) + L" FPS";
        }
        return L"FPS: " + std::to_wstring(targetFps_);
    }

    std::wstring ScalingQualityText() const {
        switch (scalingQuality_) {
        case kScalingPerformance: return Tr(L"画质 性能");
        case kScalingQuality: return Tr(L"画质 质量");
        default: return Tr(L"画质 平衡");
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

    void ShowDebugMenu(POINT popup) {
        HMENU menu = CreatePopupMenu();
        if (!menu) return;

        constexpr UINT kDebugToggleCommand = 3401;
        constexpr UINT kResetSettingsCommand = 3402;
        constexpr UINT kOpenCacheCommand = 3403;
        AppendMenuW(
            menu,
            MF_STRING | (debugMode_ ? MF_CHECKED : 0),
            kDebugToggleCommand,
            Tr(L"调试"));
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kOpenCacheCommand, Tr(L"打开缓存文件夹"));
        AppendMenuW(
            menu,
            MF_STRING,
            kResetSettingsCommand,
            Tr(L"清除缓存并重置脚本"));

        SetForegroundWindow(toolbarHwnd_);
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
        } else if (command == kOpenCacheCommand) {
            const auto& folder = vtsfloat::CacheDirectory();
            std::error_code error;
            std::filesystem::create_directories(folder, error);
            ShellExecuteW(toolbarHwnd_, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        } else if (command == kResetSettingsCommand) {
            Log("[toolbar] action=reset_settings");
            ResetSettingsToDefaults();
        }
    }

    void ShowLanguageMenu() {
        HMENU menu = CreatePopupMenu();
        if (!menu) return;

        constexpr UINT kLanguageCommandBase = 3450;
        constexpr UINT kEditCustomLanguageCommand = 3460;
        constexpr UiLanguage languages[] = {
            UiLanguage::SimplifiedChinese,
            UiLanguage::TraditionalChinese,
            UiLanguage::English,
            UiLanguage::Japanese,
            UiLanguage::Korean,
            UiLanguage::Russian,
            UiLanguage::Custom,
        };
        const UiLanguage current = vtsfloat::i18n::GetLanguage();
        for (size_t i = 0; i < ARRAYSIZE(languages); ++i) {
            AppendMenuW(
                menu,
                MF_STRING | (languages[i] == current ? MF_CHECKED : 0),
                kLanguageCommandBase + static_cast<UINT>(i),
                vtsfloat::i18n::LanguageDisplayName(languages[i]));
        }
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(
            menu, MF_STRING, kEditCustomLanguageCommand,
            Tr(L"编辑自定义语言配置…"));

        const RECT languageRect = ToolbarButtonRect(ToolbarButton::Language);
        POINT popup{ languageRect.right, languageRect.bottom };
        ClientToScreen(toolbarHwnd_, &popup);
        const UINT command = RunModalWhileRendering([this, menu, popup]() {
            return TrackPopupMenu(
                menu,
                TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTALIGN | TPM_TOPALIGN,
                popup.x, popup.y, 0, toolbarHwnd_, nullptr);
        });
        DestroyMenu(menu);
        if (command == kEditCustomLanguageCommand) {
            const std::wstring path =
                vtsfloat::i18n::CustomLanguageFilePath();
            const auto result = path.empty()
                ? static_cast<INT_PTR>(SE_ERR_FNF)
                : reinterpret_cast<INT_PTR>(ShellExecuteW(
                    toolbarHwnd_, L"open", path.c_str(), nullptr, nullptr,
                    SW_SHOWNORMAL));
            if (path.empty() || result <= 32) {
                const std::wstring message =
                    std::wstring(Tr(L"无法打开自定义语言配置文件：\n")) + path;
                MessageBoxW(
                    toolbarHwnd_, message.c_str(), Tr(L"自定义语言"),
                    MB_OK | MB_ICONERROR);
            }
            return;
        }
        if (command < kLanguageCommandBase ||
            command >= kLanguageCommandBase + ARRAYSIZE(languages)) {
            return;
        }

        vtsfloat::i18n::SetLanguage(languages[command - kLanguageCommandBase]);
        SaveUiSettings();
        PositionToolbar();
        LayoutToolbarGuide();
        RefreshDebugReadout(true);
        if (!updateBannerText_.empty() && !updateDismissed_) {
            updateRevision_ = (std::numeric_limits<std::uint64_t>::max)();
            PollUpdate();
        }
        // The VTS waiting/launch page is drawn into the layered-window DIB and
        // cached between frames. Invalidating the HWND alone only re-presents
        // those old pixels, so a language switch used to leave this page in
        // Chinese until its status changed. Rebuild the cached page now.
        statusFrameDirty_ = true;
        if (toolbarHwnd_) RedrawWindow(toolbarHwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        if (borderPanelHwnd_) InvalidateRect(borderPanelHwnd_, nullptr, FALSE);
        if (expressionPickerHwnd_) InvalidateRect(expressionPickerHwnd_, nullptr, FALSE);
        if (subjectSelectionToolbarHwnd_) InvalidateRect(subjectSelectionToolbarHwnd_, nullptr, FALSE);
        if (hwnd_) {
            RenderFrame();
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        Log("[ui] language=" + WideToUtf8(
            vtsfloat::i18n::LanguageCode(vtsfloat::i18n::GetLanguage())));
    }

    void ShowGraphicsSettingsMenu() {
        const auto senders = receiver_.GetSenderList();
        HMENU menu = CreatePopupMenu();
        if (!menu) return;

        // --- GPU submenu ---
        HMENU gpuSub = CreatePopupMenu();
        constexpr UINT kRecommendedGpuCmd = 1900;
        constexpr UINT kAdapterBase = 2000;
        const bool hasRec = minimumPowerGpuIndex_ >= 0;
        std::wstring recLabel = Tr(L"推荐（核显/节能 GPU）");
        if (hasRec) recLabel += std::wstring(Tr(L"：")) + GpuName(minimumPowerGpuIndex_);
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
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(gpuSub), Tr(L"GPU 选择"));

        // --- Quality submenu ---
        HMENU qualSub = CreatePopupMenu();
        constexpr UINT kPerfCmd = 3290;
        constexpr UINT kBalCmd = 3291;
        constexpr UINT kQualCmd = 3292;
        AppendMenuW(qualSub, MF_STRING | (scalingQuality_ == kScalingPerformance ? MF_CHECKED : 0),
            kPerfCmd, Tr(L"性能（最近邻）"));
        AppendMenuW(qualSub, MF_STRING | (scalingQuality_ == kScalingBalanced ? MF_CHECKED : 0),
            kBalCmd, Tr(L"平衡（双线性，推荐）"));
        AppendMenuW(qualSub, MF_STRING | (scalingQuality_ == kScalingQuality ? MF_CHECKED : 0),
            kQualCmd, Tr(L"质量（双三次）"));
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(qualSub), Tr(L"画质"));

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
            apiOk ? Tr(L"对齐 VTS 实际渲染帧数（推荐）") : Tr(L"对齐 VTS 实际渲染帧数（未启用 API）"));
        AppendMenuW(fpsSub,
            MF_STRING | (fpsMode_ == FpsMode::FollowVts ? MF_CHECKED : 0),
            kFollowVtsCmd, Tr(L"跟随 VTS 配置"));
        AppendMenuW(fpsSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(fpsSub, MF_STRING, kCustomFpsCmd, Tr(L"自定义…（1-240）"));
        AppendMenuW(fpsSub, MF_SEPARATOR, 0, nullptr);
        for (size_t i = 0; i < ARRAYSIZE(presets); ++i) {
            UINT flags = MF_STRING | (fpsMode_ == FpsMode::Fixed && targetFps_ == presets[i] ? MF_CHECKED : 0);
            AppendMenuW(fpsSub, flags, kFpsBase + static_cast<UINT>(i),
                (std::to_wstring(presets[i]) + L" FPS").c_str());
        }
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fpsSub), Tr(L"帧率"));


        constexpr UINT kPauseFullscreenCmd = 3300;
        constexpr UINT kSpoutAutomaticCmd = 20000;
        if (const HMENU sources = CreatePopupMenu()) {
            AppendMenuW(sources, MF_STRING | (manualSpoutSender_.empty() ? MF_CHECKED : 0),
                kSpoutAutomaticCmd, Tr(L"自动选择"));
            for (size_t i = 0; i < senders.size(); ++i) {
                auto label = Utf8ToWide(senders[i]);
                for (size_t at = 0; (at = label.find(L'&', at)) != std::wstring::npos; at += 2) label.insert(at, 1, L'&');
                AppendMenuW(sources, MF_STRING | (senders[i] == spoutSender_ ? MF_CHECKED : 0),
                    kSpoutAutomaticCmd + 1 + static_cast<UINT>(i), label.c_str());
            }
            AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sources), Tr(L"Spout2 发送源"));
        }
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (pauseOnFullscreen_ ? MF_CHECKED : 0),
            kPauseFullscreenCmd, Tr(L"桌面模式：其他程序全屏时暂停输出"));

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
        if (command >= kSpoutAutomaticCmd && command <= kSpoutAutomaticCmd + senders.size()) {
            manualSpoutSender_ = command == kSpoutAutomaticCmd ? std::string{} : senders[command - kSpoutAutomaticCmd - 1];
            nextSpoutScan_ = {};
            resetFrameSchedule_ = true;
            return;
        }
        Log("[toolbar] graphics_menu_command=" + std::to_string(command));

        if (command == kPauseFullscreenCmd) {
            pauseOnFullscreen_ = !pauseOnFullscreen_;
            SaveUiSettings();
            UpdateDesktopPause();
            return;
        }
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
                ? Tr(L"对齐 VTS 实际渲染帧数（推荐）")
                : Tr(L"对齐 VTS 实际渲染帧数（未启用 API）"));
        AppendMenuW(
            menu,
            MF_STRING | (fpsMode_ == FpsMode::FollowVts ? MF_CHECKED : 0),
            kFollowVtsCommand,
            Tr(L"跟随 VTS 配置"));
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kCustomCommand, Tr(L"自定义…（1-240）"));
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
            Tr(L"性能（最近邻，最低开销）"));
        AppendMenuW(
            menu,
            MF_STRING | (scalingQuality_ == kScalingBalanced ? MF_CHECKED : 0),
            kBalancedCommand,
            Tr(L"平衡（GPU 双线性，推荐）"));
        AppendMenuW(
            menu,
            MF_STRING | (scalingQuality_ == kScalingQuality ? MF_CHECKED : 0),
            kQualityCommand,
            Tr(L"质量（GPU 双三次，更细腻）"));
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
        // The bundled screenshot is Chinese. Showing it under another selected
        // language would reintroduce an untranslated prompt, so use a localized
        // text guide for every non-Chinese UI. Chinese keeps the illustrated
        // guide that existing users are familiar with.
        if (vtsfloat::i18n::GetLanguage() != UiLanguage::SimplifiedChinese) {
            RunModalWhileRendering([this]() {
                return MessageBoxW(
                    toolbarHwnd_,
                    Tr(L"1. 打开 VTube Studio 设置。\n2. 在“常规设置”中启用“允许插件 API 访问”。\n3. 返回 VTSFloat_Meow，点击“扫描端口”。\n4. VTube Studio 弹出授权请求时，选择允许。"),
                    Tr(L"如何开启 VTube Studio Plugins API"),
                    MB_OK | MB_ICONINFORMATION);
            });
            return;
        }
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

        // GDI+ keeps using the source stream for the entire lifetime of an
        // image created by FromStream. Releasing it here happened to work on
        // some machines, but could dereference freed memory when the guide
        // window was closed (0xC0000005). Keep it alive until after Bitmap.
        Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromStream(stream);
        if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok) {
            delete bitmap;
            stream->Release();
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
            Tr(L"如何开启 VTube Studio Plugins API"),
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
            toolbarHwnd_, nullptr, instance_, nullptr);
        if (!guideHwnd) {
            delete bitmap;
            stream->Release();
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
        BOOL messageResult = TRUE;
        while (IsWindow(guideHwnd) &&
               (messageResult = GetMessageW(&msg, nullptr, 0, 0)) > 0) {
            if (msg.hwnd == guideHwnd && msg.message == WM_SYSCOMMAND &&
                (msg.wParam & 0xFFF0) == SC_CLOSE) {
                DestroyWindow(guideHwnd);
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        // Preserve an application quit request consumed by this nested loop.
        if (messageResult == 0) PostQuitMessage(static_cast<int>(msg.wParam));
        if (IsWindow(guideHwnd)) DestroyWindow(guideHwnd);
        UnregisterClassW(L"VTSFloatApiGuide", instance_);
        delete bitmap;
        stream->Release();
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
                Tr(L"无法打开自定义帧率输入框。"),
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
            SetWindowTextW(dialog, Tr(L"自定义帧率"));
            SetDlgItemTextW(dialog, IDC_FPS_PROMPT, Tr(L"请输入 1–240 FPS："));
            SetDlgItemTextW(dialog, IDC_FPS_HINT, Tr(L"输入内容会在这里直接显示，确定后才会应用。"));
            SetDlgItemTextW(dialog, IDOK, Tr(L"确定"));
            SetDlgItemTextW(dialog, IDCANCEL, Tr(L"取消"));
            SetDlgItemInt(dialog, IDC_FPS_EDIT, overlay->targetFps_, FALSE);
            const HWND edit = GetDlgItem(dialog, IDC_FPS_EDIT);
            SetWindowSubclass(edit, &LayeredOverlay::ImageFieldProc, 1, reinterpret_cast<DWORD_PTR>(overlay));
            overlay->RegisterNumericHint(edit);
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
                        Tr(L"请输入 1 到 240 之间的整数。"),
                        Tr(L"帧率无效"),
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
                reinterpret_cast<LPARAM>(Tr(L"普通渐变（整圈同步变色）")));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_MODE, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(Tr(L"彩虹跑马灯")));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_MODE, CB_SETCURSEL,
                state->overlay->borderMode_, 0);
            SendDlgItemMessageW(
                dialog, IDC_BORDER_COLOR_TARGET, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(Tr(L"起始颜色")));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_COLOR_TARGET, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(Tr(L"结束颜色")));
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
        double originalHoverOpacityRestoreDelaySeconds =
            kDefaultHoverOpacityRestoreDelaySeconds;
        int originalModelOpacityPercent = 100;
        int originalHoverExpandPx = 0;
        bool originalExcludeEffectsFromHover = false;
        bool originalSubjectHoverRegionConfigured = false;
        int originalSubjectHoverRegionLeft = 0;
        int originalSubjectHoverRegionTop = 0;
        int originalSubjectHoverRegionRight = 10000;
        int originalSubjectHoverRegionBottom = 10000;
        int originalSubjectHoverMaskGridWidth = 0;
        int originalSubjectHoverMaskGridHeight = 0;
        std::vector<std::uint8_t> originalSubjectHoverMaskBits;
        bool originalSubjectHoverTrackingReferenceValid = false;
        double originalSubjectHoverTrackingReferenceCenterX = 0.0;
        double originalSubjectHoverTrackingReferenceCenterY = 0.0;
        double originalSubjectHoverTrackingReferenceWidth = 0.0;
        double originalSubjectHoverTrackingReferenceHeight = 0.0;
        bool originalHoverExpressionEnabled = false;
        std::vector<std::string> originalHoverExpressionFiles;
        double originalHoverExpressionDurationSeconds =
            kDefaultHoverExpressionDurationSeconds;
        bool editingHoverOpacityRestoreDelay = false;
        bool replaceHoverOpacityRestoreDelayOnNextInput = false;
        HWND numericHint = nullptr;
        int numericHintHit = 0;
        POINT numericPress{};
        double numericBase = 0;
        bool numericDragged = false;
        std::wstring hoverOpacityRestoreDelayInput;
        bool editingHoverExpressionDuration = false;
        bool replaceHoverExpressionDurationOnNextInput = false;
        std::wstring hoverExpressionDurationInput;
        std::vector<std::uint32_t> wheelPixels;
        int wheelBitmapWidth = 0;
        int wheelBitmapHeight = 0;
        int wheelBitmapValuePercent = -1;
        double colorHue = 0.0;
        double colorSaturation = 0.0;
        int colorValuePercent = 100;
        int activeHit = 0;
        bool trackingMouseLeave = false;
        bool draggingPanel = false;
        POINT dragStartCursor{};
        POINT dragStartWindow{};
    };

    struct ExpressionPickerState {
        LayeredOverlay* overlay = nullptr;
        HWND ownerPanel = nullptr;
        std::vector<VtsApiClient::ExpressionInfo> expressions;
        int scrollOffset = 0;
        int hoveredRow = -1;
        bool hoveredFinish = false;
        bool draggingScrollThumb = false;
        int scrollDragStartY = 0;
        int scrollDragStartOffset = 0;
        bool scheduleRestoreOnClose = true;
    };

    struct UiScalePickerState {
        LayeredOverlay* overlay = nullptr;
        int hoveredIndex = -1;
        HHOOK mouseHook = nullptr;
    };
    inline static thread_local HWND uiScaleClickWatcher_ = nullptr;

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
        BuildColorWheelPixels(state->wheelPixels, width, height, state->colorValuePercent / 100.0,
            39u | (24u << 8) | (14u << 16));
    }

    static void BuildColorWheelPixels(std::vector<std::uint32_t>& pixels, int width, int height,
                                     double value, std::uint32_t background) {
        pixels.assign(static_cast<size_t>(width) * height, background);
        const double centerX = width / 2.0;
        const double centerY = height / 2.0;
        const double radius = (std::max)(
            1.0, (std::min)(centerX, centerY) - 5.0);
        constexpr double kPi = 3.14159265358979323846;
        for (int y = 0; y < height; ++y) {
            auto* row = pixels.data() + static_cast<size_t>(y) * width;
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
                    hue, distance / radius, value);
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

    static constexpr int kShadowFadeRowHeight = 32;

    bool ShowPersonalShadowFade() const {
        const auto& shadows = ActiveShadows();
        return hoverFadeEnabled_ && (shadows[0].enabled || shadows[1].enabled);
    }

    void ToggleShadowHoverFade() {
        const bool enabled = !fadeShadowOnHover_;
        if (!WriteConfigInt(L"opacity", L"fade_shadow_on_hover", enabled)) return;
        fadeShadowOnHover_ = enabled;
        if (borderPanelHwnd_) InvalidateRect(borderPanelHwnd_, nullptr, FALSE);
        if (shadowPanelHwnd_) InvalidateRect(shadowPanelHwnd_, nullptr, FALSE);
        resetFrameSchedule_ = true;
    }

    static int PersonalPanelHeight(const LayeredOverlay* overlay) {
        if (!overlay) return 560;
        const int baseHeight = overlay->borderMode_ == kBorderModeCustom
            ? 738 : 518;
        // The opacity restore-delay editor is always visible directly above
        // the hover-range slider.
        constexpr int opacityRestoreDelayRowHeight = 42;
        // Enabling hover expressions inserts the duration editor. Reserve a
        // real row for it instead of pushing the expression selector into the
        // footer buttons.
        return baseHeight + opacityRestoreDelayRowHeight +
            (overlay->ShowPersonalShadowFade() ? kShadowFadeRowHeight : 0) +
            (overlay->hoverExpressionEnabled_ ? 38 : 0);
    }

    static double PersonalPanelScale(const LayeredOverlay* overlay) {
        HMONITOR monitor = overlay && overlay->hwnd_
            ? MonitorFromWindow(overlay->hwnd_, MONITOR_DEFAULTTONEAREST)
            : nullptr;
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        const double userScale = overlay
            ? overlay->uiScalePercent_ / 100.0
            : 1.0;
        if (!monitor || !GetMonitorInfoW(monitor, &info)) return userScale;
        const int width = info.rcWork.right - info.rcWork.left;
        if (width < 3400) return userScale;
        // A fixed, mild enlargement is intentional. Following 150%/200% DPI
        // here made this already compact custom layout 1.5-1.6x larger and
        // squeezed labels/values. 1.15x remains readable on 4K without turning
        // the editor into a large modal sheet.
        return 1.15 * userScale;
    }

    static int PersonalPanelPixelWidth(const LayeredOverlay* overlay) {
        return static_cast<int>(std::lround(420 * PersonalPanelScale(overlay)));
    }

    static int PersonalPanelPixelHeight(const LayeredOverlay* overlay) {
        return static_cast<int>(std::lround(
            PersonalPanelHeight(overlay) * PersonalPanelScale(overlay)));
    }

    static POINT PersonalPanelLogicalPoint(
        HWND panel, const LayeredOverlay* overlay, int x, int y) {
        RECT client{};
        GetClientRect(panel, &client);
        const int width = (std::max)(1L, client.right - client.left);
        const int height = (std::max)(1L, client.bottom - client.top);
        return POINT{
            static_cast<LONG>(std::lround(x * 420.0 / width)),
            static_cast<LONG>(std::lround(
                y * 1.0 * PersonalPanelHeight(overlay) / height)) };
    }

    static int UiFontSize(HWND window, int baseSize) {
        (void)window;
        if (baseSize <= 0) return baseSize;
        // These are already logical UI sizes. Shrinking them according to the
        // monitor's pixel width made text smaller on high-resolution displays,
        // even after the user increased Windows display scaling.
        return baseSize;
    }

    static void PanelText(
        HDC dc, const std::wstring& text, RECT rect, COLORREF color,
        int pixelHeight = 15, bool bold = false, UINT flags = DT_LEFT | DT_VCENTER) {
        // Panel coordinates are logical units and the destination DC performs
        // the 4K mapping. Do not derive this font from the cursor monitor: that
        // made labels change size between repaints and forced bitmap scaling.
        HFONT font = CreateFontW(
            -pixelHeight, 0, 0, 0,
            bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE,
            FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_NATURAL_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
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
        RECT actualClient{};
        GetClientRect(panel, &actualClient);
        RECT client{ 0, 0, 420, PersonalPanelHeight(overlay) };
        HDC dc = CreateCompatibleDC(target);
        HBITMAP backBuffer = CreateCompatibleBitmap(
            target,
            (std::max)(1L, actualClient.right - actualClient.left),
            (std::max)(1L, actualClient.bottom - actualClient.top));
        HGDIOBJ oldBitmap = SelectObject(dc, backBuffer);
        SetMapMode(dc, MM_ANISOTROPIC);
        SetWindowExtEx(dc, client.right, client.bottom, nullptr);
        SetViewportExtEx(dc,
            actualClient.right - actualClient.left,
            actualClient.bottom - actualClient.top, nullptr);
        PanelFill(dc, client, RGB(14, 24, 39));

        RECT header{ 0, 0, client.right, 44 };
        PanelFill(dc, header, RGB(28, 48, 75));
        PanelText(dc, Tr(L"个性化"), RECT{ 16, 0, 300, 44 }, RGB(240, 246, 255), 18, true,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        PanelText(dc, L"×", RECT{ client.right - 44, 0, client.right - 8, 44 },
            RGB(240, 246, 255), 25, false, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        PanelText(dc, Tr(L"边框样式"), RECT{ 18, 53, kPersonalBorderModeLeft - 5, 83 }, RGB(166, 198, 232), 14, true,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        const int modeLeft = kPersonalBorderModeLeft;
        const int modeWidth = (client.right - modeLeft - 16) / 3;
        const wchar_t* modeLabels[] = {
            Tr(L"自定义"), Tr(L"跑马灯渐变"), Tr(L"普通渐变") };
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
            PanelText(dc, Tr(L"自定义颜色"), RECT{ 245, 105, 405, 135 },
                RGB(166, 198, 232), 14, true);
            RECT swatch{ 245, 145, 405, 178 };
            PanelFill(dc, swatch, color);
            wchar_t colorText[96]{};
            swprintf_s(colorText, L"#%02X%02X%02X  RGB(%u,%u,%u)",
                GetRValue(color), GetGValue(color), GetBValue(color),
                GetRValue(color), GetGValue(color), GetBValue(color));
            PanelText(dc, colorText, RECT{ 245, 182, 410, 205 },
                RGB(220, 232, 248), 12, false);
            PanelText(dc, Tr(L"亮度"), RECT{ 245, 220, 330, 243 }, RGB(166, 198, 232), 13, true,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            PanelSlider(dc, RECT{ 245, 250, 405, 260 }, state->colorValuePercent, 0, 100,
                state->activeHit == 4);
            y = 330;
        } else {
            y = 105;
        }

        PanelText(dc, Tr(L"边框粗细"), RECT{ 20, y, 125, y + 28 }, RGB(166, 198, 232), 13, true,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        PanelSlider(dc, RECT{ 110, y + 10, 355, y + 20 }, overlay->borderThickness_, 1,
            kMaximumBorderThickness,
            state->activeHit == 5);
        PanelText(dc, std::to_wstring(overlay->borderThickness_), RECT{ 365, y, 410, y + 28 },
            RGB(240, 246, 255), 13, true, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        y += 45;
        PanelText(dc, Tr(L"模型不透明度"), RECT{ 20, y, 145, y + 28 }, RGB(166, 198, 232), 13, true,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
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
        PanelText(dc, Tr(L"鼠标经过模型时将模型透明化"), RECT{ 48, y, 405, y + 28 },
            RGB(220, 232, 248), 13, false,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        if (overlay->ShowPersonalShadowFade()) {
            const RECT shadowCheck{20, y + 31, 38, y + 49};
            PanelFill(dc, shadowCheck, overlay->fadeShadowOnHover_ ? RGB(55, 139, 221) : RGB(41, 58, 80));
            if (overlay->fadeShadowOnHover_) {
                PanelText(dc, L"✓", shadowCheck, RGB(255, 255, 255), 14, true, DT_CENTER | DT_VCENTER);
            }
            PanelText(dc, Tr(L"悬停时同时淡化阴影"), RECT{48, y + 27, 405, y + 55}, RGB(220, 232, 248),
                13, false, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            y += kShadowFadeRowHeight;
        }

        // Keep the fade toggle and its opacity slider on separate rows.  These
        // controls used to share the same y coordinate, which made the labels
        // and slider appear on top of each other in the personalization panel.
        y += 42;
        PanelText(dc, Tr(L"悬停不透明度"), RECT{ 20, y, 145, y + 28 }, RGB(166, 198, 232), 13, true,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        PanelSlider(dc, RECT{ 130, y + 10, 355, y + 20 }, overlay->hoverOpacityPercent_, 0, 100,
            state->activeHit == 7);
        PanelText(dc, std::to_wstring(overlay->hoverOpacityPercent_) + L"%", RECT{ 365, y, 410, y + 28 },
            RGB(240, 246, 255), 13, true, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        y += 42;
        // Both delay editors use the same input column. Keep the default note
        // on the left so the value, unit and edit confirmation never shift.
        const RECT opacityDelayLabel{ 20, y + 2, 174, y + 28 };
        const RECT opacityDelayDefault{ 174, y + 2, 279, y + 28 };
        const RECT opacityDelayConfirm{ 174, y + 2, 279, y + 28 };
        const RECT opacityDelayInput{ 284, y + 2, 390, y + 28 };
        const RECT opacityDelayUnit{ 394, y + 2, 418, y + 28 };
        PanelText(dc, Tr(L"透明度恢复延时"), opacityDelayLabel,
            RGB(166, 198, 232), 13, true,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        PanelFill(dc, opacityDelayInput, state->editingHoverOpacityRestoreDelay
            ? RGB(34, 78, 124) : RGB(26, 46, 71));
        HPEN opacityDelayBorder = CreatePen(
            PS_SOLID, 1, state->editingHoverOpacityRestoreDelay
                ? RGB(68, 164, 255) : RGB(54, 82, 116));
        HGDIOBJ oldOpacityDelayPen = SelectObject(dc, opacityDelayBorder);
        HGDIOBJ oldOpacityDelayBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, opacityDelayInput.left, opacityDelayInput.top,
            opacityDelayInput.right, opacityDelayInput.bottom);
        SelectObject(dc, oldOpacityDelayBrush);
        SelectObject(dc, oldOpacityDelayPen);
        DeleteObject(opacityDelayBorder);
        const std::wstring opacityDelayText = state->editingHoverOpacityRestoreDelay
            ? state->hoverOpacityRestoreDelayInput
            : FormatNonNegativeDecimal(overlay->hoverOpacityRestoreDelaySeconds_);
        PanelText(dc, opacityDelayText, opacityDelayInput, RGB(240, 246, 255),
            13, false, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        PanelText(dc, Tr(L"秒"), opacityDelayUnit, RGB(220, 232, 248), 13, false,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (state->editingHoverOpacityRestoreDelay) {
            PanelText(dc, Tr(L"ENTER 确认"), opacityDelayConfirm,
                RGB(112, 181, 246), 10, true,
                DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            PanelText(dc, Tr(L"默认 0 秒"), opacityDelayDefault,
                RGB(124, 151, 181), 10, false,
                DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        y += 42;
        PanelText(dc, Tr(L"悬停扩展"), RECT{ 20, y, 125, y + 28 }, RGB(166, 198, 232), 13, true,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        PanelSlider(dc, RECT{ 130, y + 10, 330, y + 20 }, overlay->hoverExpandPx_, -500, 500,
            state->activeHit == 8);
        PanelText(dc, std::to_wstring(overlay->hoverExpandPx_) + L"px", RECT{ 340, y, 410, y + 28 },
            RGB(240, 246, 255), 13, true, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        y += 42;
        // The feature switch is deliberately separate from the selection
        // action. Re-selecting a subject must not silently enable the mode.
        RECT subjectCheck{ 20, y + 3, 38, y + 21 };
        PanelFill(dc, subjectCheck, overlay->excludeEffectsFromHover_
            ? RGB(55, 139, 221) : RGB(41, 58, 80));
        if (overlay->excludeEffectsFromHover_) {
            PanelText(dc, L"✓", subjectCheck, RGB(255, 255, 255), 14, true,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        PanelText(dc, Tr(L"启用手动框选范围"),
            RECT{ 48, y, 195, y + 28 }, RGB(220, 232, 248), 13, false,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        const RECT subjectReselectButton{ 198, y + 1, 402, y + 27 };
        PanelFill(dc, subjectReselectButton, state->activeHit == 17
            ? RGB(48, 122, 193) : RGB(26, 46, 71));
        PanelText(dc, Tr(L"手动框选悬停触发范围"),
            subjectReselectButton, RGB(166, 211, 255), 13, true,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        y += 42;
        RECT expressionCheck{ 20, y + 3, 38, y + 21 };
        PanelFill(dc, expressionCheck,
            overlay->hoverExpressionEnabled_ ? RGB(55, 139, 221) : RGB(41, 58, 80));
        if (overlay->hoverExpressionEnabled_) {
            PanelText(dc, L"✓", expressionCheck, RGB(255, 255, 255), 14, true,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        PanelText(dc, Tr(L"鼠标移入时触发表情，移出后立即恢复"),
            RECT{ 48, y, 390, y + 28 }, RGB(220, 232, 248), 13, false,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        y += 34;
        if (overlay->hoverExpressionEnabled_) {
            // Align the row with the other settings while keeping the value
            // centered inside its edit box.
            const RECT durationLabel{ 20, y + 2, 174, y + 28 };
            const RECT durationConfirm{ 174, y + 2, 279, y + 28 };
            const RECT durationInput{ 284, y + 2, 390, y + 28 };
            const RECT durationUnit{ 394, y + 2, 418, y + 28 };
            PanelText(dc, Tr(L"表情恢复延时"), durationLabel,
                RGB(166, 198, 232), 13, true,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            PanelFill(dc, durationInput, state->editingHoverExpressionDuration
                ? RGB(34, 78, 124) : RGB(26, 46, 71));
            HPEN durationBorder = CreatePen(
                PS_SOLID, 1, state->editingHoverExpressionDuration
                    ? RGB(68, 164, 255) : RGB(54, 82, 116));
            HGDIOBJ oldPen = SelectObject(dc, durationBorder);
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, durationInput.left, durationInput.top,
                durationInput.right, durationInput.bottom);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(durationBorder);
            const std::wstring durationText = state->editingHoverExpressionDuration
                ? state->hoverExpressionDurationInput
                : FormatNonNegativeDecimal(overlay->hoverExpressionDurationSeconds_);
            PanelText(dc, durationText, durationInput, RGB(240, 246, 255),
                13, false, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            PanelText(dc, Tr(L"秒"), durationUnit, RGB(220, 232, 248), 13, false,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (state->editingHoverExpressionDuration) {
                PanelText(dc, Tr(L"ENTER 确认"), durationConfirm,
                    RGB(112, 181, 246), 10, true,
                    DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            y += 38;
        }
        RECT expressionButton{ 20, y, 405, y + 32 };
        PanelFill(dc, expressionButton, RGB(26, 46, 71));
        std::wstring expressionLabel = Tr(L"选择表情");
        if (overlay->hoverExpressionFiles_.size() == 1) {
            expressionLabel = std::wstring(Tr(L"表情：")) + Utf8ToWide(
                StripExpressionSuffix(overlay->hoverExpressionFiles_.front()));
        } else if (!overlay->hoverExpressionFiles_.empty()) {
            expressionLabel = std::wstring(Tr(L"已选择 ")) +
                std::to_wstring(overlay->hoverExpressionFiles_.size()) + Tr(L" 个表情");
        }
        PanelText(dc, expressionLabel, expressionButton, RGB(240, 246, 255), 13, true,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        const int footerY = client.bottom - 48;
        PanelFill(dc, RECT{ 16, footerY - 8, client.right - 16, footerY - 7 }, RGB(38, 58, 82));
        const int buttonW = (client.right - 48) / 3;
        const wchar_t* footer[] = { Tr(L"恢复默认"), Tr(L"取消"), Tr(L"完成") };
        for (int i = 0; i < 3; ++i) {
            RECT button{ 16 + i * (buttonW + 8), footerY,
                16 + i * (buttonW + 8) + buttonW, footerY + 32 };
            PanelFill(dc, button, i == 2 ? RGB(32, 91, 151) : RGB(26, 46, 71));
            PanelText(dc, footer[i], button, RGB(240, 246, 255), 13, true,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        SetMapMode(dc, MM_TEXT);
        BitBlt(target, 0, 0,
            actualClient.right - actualClient.left,
            actualClient.bottom - actualClient.top,
            dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(backBuffer);
        DeleteDC(dc);
        EndPaint(panel, &paint);
    }

    static int PersonalPanelHitTest(
        HWND panel, const LayeredOverlay* overlay, int x, int y) {
        if (!overlay) return 0;
        const POINT logical = PersonalPanelLogicalPoint(panel, overlay, x, y);
        x = logical.x;
        y = logical.y;
        if (x >= 370 && x < 420 && y < 44) return 1; // close
        const int modeLeft = kPersonalBorderModeLeft;
        const int modeWidth = (420 - modeLeft - 16) / 3;
        if (y >= 52 && y < 84 && x >= modeLeft && x < modeLeft + 3 * modeWidth) {
            return 10 + (x - modeLeft) / modeWidth;
        }
        int base = overlay->borderMode_ == kBorderModeCustom ? 330 : 105;
        if (y >= base && y < base + 30) return 5;
        if (y >= base + 45 && y < base + 75) return 6;
        base -= 42;
        if (y >= base + 129 && y < base + (overlay->ShowPersonalShadowFade() ? 156 : 162) && x < 350) return 9;
        if (overlay->ShowPersonalShadowFade()) {
            if (y >= base + 156 && y < base + 185 && x < 405) return 23;
            base += kShadowFadeRowHeight;
        }
        if (y >= base + 171 && y < base + 204) return 7;
        if (y >= base + 215 && y < base + 241 && x >= 284 && x < 390) return 19;
        if (y >= base + 255 && y < base + 288) return 8;
        if (y >= base + 297 && y < base + 334) {
            return x < 198 ? 16 : 17;
        }
        if (y >= base + 339 && y < base + 374 && x < 395) return 13;
        const int expressionOptionsTop = base + 373;
        if (overlay->hoverExpressionEnabled_ &&
            y >= expressionOptionsTop + 2 && y < expressionOptionsTop + 28 &&
            x >= 284 && x < 390) {
            return 15;
        }
        const int expressionButtonTop = expressionOptionsTop +
            (overlay->hoverExpressionEnabled_ ? 38 : 0);
        if (y >= expressionButtonTop && y < expressionButtonTop + 40 &&
            x >= 20 && x < 405) {
            return 14;
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
        x = PersonalPanelLogicalPoint(panel, overlay, x, 0).x;
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
        const POINT logical = PersonalPanelLogicalPoint(
            panel, state->overlay, x, y);
        x = logical.x;
        y = logical.y;
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

    static void SavePersonalPanelAppearance(BorderDialogState& state) {
        auto& overlay = *state.overlay;
        // These preferences take effect immediately. Keep the panel's cancel
        // baseline in sync with each successful write.
        if (overlay.borderMode_ != state.originalMode && overlay.SaveBorderMode()) {
            state.originalMode = overlay.borderMode_;
        }
    }

    static void ResetPersonalPanel(HWND panel, BorderDialogState* state) {
        if (!state || !state->overlay) return;
        LayeredOverlay* overlay = state->overlay;

        CloseExpressionPicker(overlay->expressionPickerHwnd_, false);
        overlay->borderMode_ = kBorderModeNormal;
        overlay->customBorderColor_ = kDefaultCustomBorderColor;
        CustomColorHueSaturation(overlay->customBorderColor_, state->colorHue, state->colorSaturation);
        state->colorValuePercent = CustomColorValuePercent(overlay->customBorderColor_);
        overlay->borderThickness_ = kDefaultBorderThickness;
        overlay->hoverFadeEnabled_ = true;
        overlay->hoverOpacityPercent_ = kDefaultHoverOpacityPercent;
        overlay->hoverOpacityRestoreDelaySeconds_ =
            kDefaultHoverOpacityRestoreDelaySeconds;
        overlay->modelOpacityPercent_ = 100;
        SavePersonalPanelAppearance(*state);
        overlay->hoverExpandPx_ = 0;
        overlay->excludeEffectsFromHover_ = false;
        overlay->subjectHoverRegionConfigured_ = false;
        overlay->subjectHoverRegionLeft_ = 0;
        overlay->subjectHoverRegionTop_ = 0;
        overlay->subjectHoverRegionRight_ = 10000;
        overlay->subjectHoverRegionBottom_ = 10000;
        overlay->subjectHoverMaskGridWidth_ = 0;
        overlay->subjectHoverMaskGridHeight_ = 0;
        overlay->subjectHoverMaskBits_.clear();
        overlay->subjectHoverMaskDirty_ = true;
        overlay->subjectHoverTrackingReferenceValid_ = false;
        overlay->subjectHoverTrackingScale_ = 1.0;
        overlay->CancelHoverExpression();
        overlay->hoverExpressionEnabled_ = false;
        overlay->hoverExpressionFiles_.clear();
        overlay->hoverExpressionDurationSeconds_ =
            kDefaultHoverExpressionDurationSeconds;
        state->editingHoverExpressionDuration = false;
        state->replaceHoverExpressionDurationOnNextInput = false;
        state->hoverExpressionDurationInput = FormatNonNegativeDecimal(
            overlay->hoverExpressionDurationSeconds_);
        state->editingHoverOpacityRestoreDelay = false;
        state->replaceHoverOpacityRestoreDelayOnNextInput = false;
        state->hoverOpacityRestoreDelayInput = FormatNonNegativeDecimal(
            overlay->hoverOpacityRestoreDelaySeconds_);
        overlay->hoverOpacityCursorWasOverModel_ = false;
        overlay->hoverOpacityRestorePending_ = false;
        overlay->hoverOpacityRestoreAt_ = Clock::time_point{};
        overlay->RestorePanelExpressionPreview();
        overlay->hoverOpacityPreviewActive_ = true;
        SetTimer(panel, 91, 1000, nullptr);
        state->activeHit = 0;
        overlay->personalPanelManuallyPositioned_ = false;
        overlay->PositionBorderPanel();
        overlay->PositionToolbar();
        ++overlay->requested_;
        overlay->RenderFrame();
        RefreshPersonalPanel(panel);
    }

    static void RestorePersonalPanel(HWND panel, BorderDialogState* state) {
        if (!state || !state->overlay) return;
        LayeredOverlay* overlay = state->overlay;

        CloseExpressionPicker(overlay->expressionPickerHwnd_, false);
        overlay->borderMode_ = state->originalMode;
        overlay->customBorderColor_ = state->originalColor;
        overlay->borderThickness_ = state->originalThickness;
        overlay->hoverFadeEnabled_ = state->originalHoverFadeEnabled;
        overlay->hoverOpacityPercent_ = state->originalHoverOpacityPercent;
        overlay->hoverOpacityRestoreDelaySeconds_ =
            state->originalHoverOpacityRestoreDelaySeconds;
        overlay->modelOpacityPercent_ = state->originalModelOpacityPercent;
        overlay->hoverExpandPx_ = state->originalHoverExpandPx;
        overlay->excludeEffectsFromHover_ = state->originalExcludeEffectsFromHover;
        overlay->subjectHoverRegionConfigured_ = state->originalSubjectHoverRegionConfigured;
        overlay->subjectHoverRegionLeft_ = state->originalSubjectHoverRegionLeft;
        overlay->subjectHoverRegionTop_ = state->originalSubjectHoverRegionTop;
        overlay->subjectHoverRegionRight_ = state->originalSubjectHoverRegionRight;
        overlay->subjectHoverRegionBottom_ = state->originalSubjectHoverRegionBottom;
        overlay->subjectHoverMaskGridWidth_ = state->originalSubjectHoverMaskGridWidth;
        overlay->subjectHoverMaskGridHeight_ = state->originalSubjectHoverMaskGridHeight;
        overlay->subjectHoverMaskBits_ = state->originalSubjectHoverMaskBits;
        overlay->subjectHoverMaskDirty_ = true;
        overlay->subjectHoverTrackingReferenceValid_ =
            state->originalSubjectHoverTrackingReferenceValid;
        overlay->subjectHoverTrackingReferenceCenterX_ =
            state->originalSubjectHoverTrackingReferenceCenterX;
        overlay->subjectHoverTrackingReferenceCenterY_ =
            state->originalSubjectHoverTrackingReferenceCenterY;
        overlay->subjectHoverTrackingReferenceWidth_ =
            state->originalSubjectHoverTrackingReferenceWidth;
        overlay->subjectHoverTrackingReferenceHeight_ =
            state->originalSubjectHoverTrackingReferenceHeight;
        overlay->subjectHoverTrackingCurrentCenterX_ =
            overlay->subjectHoverTrackingReferenceCenterX_;
        overlay->subjectHoverTrackingCurrentCenterY_ =
            overlay->subjectHoverTrackingReferenceCenterY_;
        overlay->subjectHoverTrackingScale_ = 1.0;
        overlay->hoverExpressionEnabled_ = state->originalHoverExpressionEnabled;
        overlay->hoverExpressionFiles_ = state->originalHoverExpressionFiles;
        overlay->hoverExpressionDurationSeconds_ =
            state->originalHoverExpressionDurationSeconds;
        state->editingHoverOpacityRestoreDelay = false;
        state->replaceHoverOpacityRestoreDelayOnNextInput = false;
        state->hoverOpacityRestoreDelayInput = FormatNonNegativeDecimal(
            overlay->hoverOpacityRestoreDelaySeconds_);
        overlay->hoverOpacityCursorWasOverModel_ = false;
        overlay->hoverOpacityRestorePending_ = false;
        overlay->hoverOpacityRestoreAt_ = Clock::time_point{};
        overlay->RestorePanelExpressionPreview();
        overlay->hoverOpacityPreviewActive_ = false;
        // Subject selection is persisted immediately. Cancelling the panel
        // must therefore write the captured original mask/settings back too.
        overlay->SaveUiSettings();
        overlay->personalPanelManuallyPositioned_ = false;
        overlay->PositionBorderPanel();
        overlay->PositionToolbar();
        ++overlay->requested_;
        overlay->RenderFrame();
        Log("[toolbar] border_settings_cancel");
        DestroyWindow(panel);
    }

    static constexpr int kUiScalePickerWidth = 240;
    static constexpr int kUiScalePickerColumns = 2;
    static constexpr int kUiScalePickerRows = 7;
    static constexpr int kUiScalePickerRowHeight = 32;
    static constexpr int kUiScalePickerHeight =
        12 + kUiScalePickerRows * kUiScalePickerRowHeight;

    static POINT UiScalePickerLogicalPoint(HWND picker, int x, int y) {
        RECT client{};
        GetClientRect(picker, &client);
        const int width = (std::max)(1L, client.right - client.left);
        const int height = (std::max)(1L, client.bottom - client.top);
        return POINT{
            static_cast<LONG>(std::lround(x * kUiScalePickerWidth * 1.0 / width)),
            static_cast<LONG>(std::lround(y * kUiScalePickerHeight * 1.0 / height)) };
    }

    static RECT UiScalePickerItemRect(int index) {
        constexpr int padding = 6;
        constexpr int gap = 6;
        constexpr int columnWidth =
            (kUiScalePickerWidth - padding * 2 - gap) / kUiScalePickerColumns;
        const int column = index / kUiScalePickerRows;
        const int row = index % kUiScalePickerRows;
        const int left = padding + column * (columnWidth + gap);
        const int top = padding + row * kUiScalePickerRowHeight;
        return RECT{ left, top, left + columnWidth,
            top + kUiScalePickerRowHeight - 2 };
    }

    static int UiScalePickerHitTest(int x, int y) {
        constexpr int itemCount =
            (kMaximumUiScalePercent - kMinimumUiScalePercent) /
                kUiScaleStepPercent + 1;
        for (int index = 0; index < itemCount; ++index) {
            const RECT item = UiScalePickerItemRect(index);
            if (PtInRect(&item, POINT{ x, y })) return index;
        }
        return -1;
    }

    static void DrawUiScalePicker(HWND picker, UiScalePickerState* state) {
        PAINTSTRUCT paint{};
        HDC target = BeginPaint(picker, &paint);
        RECT actualClient{};
        GetClientRect(picker, &actualClient);
        RECT client{ 0, 0, kUiScalePickerWidth, kUiScalePickerHeight };
        HDC dc = CreateCompatibleDC(target);
        HBITMAP bitmap = CreateCompatibleBitmap(target,
            (std::max)(1L, actualClient.right - actualClient.left),
            (std::max)(1L, actualClient.bottom - actualClient.top));
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        SetMapMode(dc, MM_ANISOTROPIC);
        SetWindowExtEx(dc, client.right, client.bottom, nullptr);
        SetViewportExtEx(dc,
            actualClient.right - actualClient.left,
            actualClient.bottom - actualClient.top, nullptr);
        PanelFill(dc, client, RGB(14, 24, 39));
        HBRUSH border = CreateSolidBrush(RGB(48, 122, 193));
        FrameRect(dc, &client, border);
        DeleteObject(border);

        constexpr int itemCount =
            (kMaximumUiScalePercent - kMinimumUiScalePercent) /
                kUiScaleStepPercent + 1;
        for (int index = 0; index < itemCount; ++index) {
            const int value = kMinimumUiScalePercent +
                index * kUiScaleStepPercent;
            const bool selected = state && state->overlay &&
                state->overlay->uiScalePercent_ == value;
            const RECT item = UiScalePickerItemRect(index);
            PanelFill(dc, item,
                selected ? RGB(41, 105, 170)
                : (state && state->hoveredIndex == index
                    ? RGB(25, 55, 86) : RGB(22, 38, 59)));
            PanelText(dc, std::to_wstring(value) + L"%", item,
                selected ? RGB(255, 255, 255) : RGB(220, 233, 249),
                13, selected, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        SetMapMode(dc, MM_TEXT);
        BitBlt(target, 0, 0,
            actualClient.right - actualClient.left,
            actualClient.bottom - actualClient.top,
            dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
        EndPaint(picker, &paint);
    }

    static void CloseUiScalePicker(HWND picker) {
        if (picker && IsWindow(picker)) DestroyWindow(picker);
    }

    static LRESULT CALLBACK UiScaleOutsideClick(int code, WPARAM message, LPARAM data) {
        if (code == HC_ACTION && uiScaleClickWatcher_ &&
            (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN ||
             message == WM_NCLBUTTONDOWN || message == WM_NCRBUTTONDOWN || message == WM_NCMBUTTONDOWN)) {
            const auto* mouse = reinterpret_cast<const MOUSEHOOKSTRUCT*>(data);
            const HWND picker = uiScaleClickWatcher_;
            if (mouse->hwnd != picker && !IsChild(picker, mouse->hwnd)) CloseUiScalePicker(picker);
        }
        return CallNextHookEx(nullptr, code, message, data);
    }

    static LRESULT CALLBACK UiScalePickerProc(
        HWND picker, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* state = reinterpret_cast<UiScalePickerState*>(
            GetWindowLongPtrW(picker, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            state = reinterpret_cast<UiScalePickerState*>(create->lpCreateParams);
            SetWindowLongPtrW(picker, GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(state));
        }
        if (!state || !state->overlay) {
            return DefWindowProcW(picker, message, wParam, lParam);
        }
        LayeredOverlay* overlay = state->overlay;
        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            DrawUiScalePicker(picker, state);
            return 0;
        case WM_MOUSEMOVE: {
            const POINT point = UiScalePickerLogicalPoint(
                picker, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            const int hovered = UiScalePickerHitTest(point.x, point.y);
            if (hovered != state->hoveredIndex) {
                state->hoveredIndex = hovered;
                InvalidateRect(picker, nullptr, FALSE);
            }
            TRACKMOUSEEVENT tracking{};
            tracking.cbSize = sizeof(tracking);
            tracking.dwFlags = TME_LEAVE;
            tracking.hwndTrack = picker;
            TrackMouseEvent(&tracking);
            return 0;
        }
        case WM_MOUSELEAVE:
            state->hoveredIndex = -1;
            InvalidateRect(picker, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            const POINT point = UiScalePickerLogicalPoint(
                picker, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            const int index = UiScalePickerHitTest(point.x, point.y);
            if (index < 0) return 0;
            const int selected = kMinimumUiScalePercent +
                index * kUiScaleStepPercent;
            CloseUiScalePicker(picker);
            overlay->ApplyUiScale(selected);
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                CloseUiScalePicker(picker);
                return 0;
            }
            break;
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                CloseUiScalePicker(picker);
                return 0;
            }
            break;
        case WM_DESTROY:
            if (state->mouseHook) UnhookWindowsHookEx(state->mouseHook);
            if (uiScaleClickWatcher_ == picker) uiScaleClickWatcher_ = nullptr;
            overlay->uiScalePickerHwnd_ = nullptr;
            return 0;
        case WM_NCDESTROY:
            SetWindowLongPtrW(picker, GWLP_USERDATA, 0);
            delete state;
            return DefWindowProcW(picker, message, wParam, lParam);
        default:
            break;
        }
        return DefWindowProcW(picker, message, wParam, lParam);
    }

    void ShowUiScalePicker(HWND panel, const RECT& anchor) {
        auto* overlay = this;
        if (overlay->uiScalePickerHwnd_ &&
            IsWindow(overlay->uiScalePickerHwnd_)) {
            SetForegroundWindow(overlay->uiScalePickerHwnd_);
            SetFocus(overlay->uiScalePickerHwnd_);
            return;
        }
        CloseExpressionPicker(overlay->expressionPickerHwnd_, true);
        auto* pickerState = new UiScalePickerState{};
        pickerState->overlay = overlay;

        static const wchar_t kUiScalePickerClass[] = L"LilyVtsUiScalePicker";
        static bool registered = false;
        if (!registered) {
            WNDCLASSW wc{};
            wc.lpfnWndProc = &LayeredOverlay::UiScalePickerProc;
            wc.hInstance = overlay->instance_;
            wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
            wc.hbrBackground = nullptr;
            wc.lpszClassName = kUiScalePickerClass;
            registered = RegisterClassW(&wc) != 0 ||
                GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        }
        if (!registered) {
            delete pickerState;
            return;
        }

        POINT popup{anchor.left, anchor.bottom + UiScaled(3)};
        ClientToScreen(panel, &popup);
        int pickerWidth = UiScaled(kUiScalePickerWidth);
        int pickerHeight = UiScaled(kUiScalePickerHeight);
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (GetMonitorInfoW(MonitorFromWindow(panel, MONITOR_DEFAULTTONEAREST), &monitor)) {
            const RECT work = monitor.rcWork;
            pickerWidth = (std::min)(pickerWidth, static_cast<int>(work.right - work.left));
            pickerHeight = (std::min)(pickerHeight, static_cast<int>(work.bottom - work.top));
            popup.x = (std::clamp)(popup.x, work.left, work.right - pickerWidth);
            if (popup.y + pickerHeight > work.bottom) {
                POINT above{anchor.left, anchor.top - UiScaled(3)};
                ClientToScreen(panel, &above);
                popup.y = (std::clamp)(above.y - pickerHeight, work.top, work.bottom - pickerHeight);
            }
        }
        overlay->uiScalePickerHwnd_ = CreateWindowExW(
            overlay->desktopMode_ ? WS_EX_TOOLWINDOW
                : WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            kUiScalePickerClass, Tr(L"界面缩放"), WS_POPUP,
            popup.x, popup.y, pickerWidth, pickerHeight,
            panel, nullptr, overlay->instance_, pickerState);
        if (!overlay->uiScalePickerHwnd_) {
            delete pickerState;
            return;
        }
        pickerState->mouseHook = SetWindowsHookExW(WH_MOUSE, UiScaleOutsideClick, nullptr, GetCurrentThreadId());
        if (pickerState->mouseHook) uiScaleClickWatcher_ = overlay->uiScalePickerHwnd_;
        ShowWindow(overlay->uiScalePickerHwnd_, SW_SHOW);
        SetForegroundWindow(overlay->uiScalePickerHwnd_);
        SetFocus(overlay->uiScalePickerHwnd_);
        UpdateWindow(overlay->uiScalePickerHwnd_);
    }

    static constexpr int kExpressionPickerWidth = 460;
    static constexpr int kExpressionPickerRowHeight = 30;
    static constexpr int kExpressionPickerMaxVisibleRows = 8;

    static bool ExpressionSelected(
        const LayeredOverlay* overlay, const std::string& file) {
        return overlay && std::find(
            overlay->hoverExpressionFiles_.begin(),
            overlay->hoverExpressionFiles_.end(), file) !=
            overlay->hoverExpressionFiles_.end();
    }

    static int ExpressionPickerVisibleRows(const ExpressionPickerState* state) {
        if (!state) return 1;
        return (std::max)(1, (std::min)(
            static_cast<int>(state->expressions.size()),
            kExpressionPickerMaxVisibleRows));
    }

    static int ExpressionPickerHeight(const ExpressionPickerState* state) {
        return 10 + ExpressionPickerVisibleRows(state) *
            kExpressionPickerRowHeight + 44;
    }

    static int ExpressionPickerMaxScroll(const ExpressionPickerState* state) {
        if (!state) return 0;
        return (std::max)(0, static_cast<int>(state->expressions.size()) -
            ExpressionPickerVisibleRows(state));
    }

    static RECT ExpressionPickerScrollTrack(const ExpressionPickerState* state) {
        return RECT{ kExpressionPickerWidth - 13, 7,
            kExpressionPickerWidth - 5,
            7 + ExpressionPickerVisibleRows(state) * kExpressionPickerRowHeight - 2 };
    }

    static RECT ExpressionPickerScrollThumb(const ExpressionPickerState* state) {
        const RECT track = ExpressionPickerScrollTrack(state);
        const int trackHeight = track.bottom - track.top;
        const int total = state ? static_cast<int>(state->expressions.size()) : 0;
        const int visible = ExpressionPickerVisibleRows(state);
        if (total <= visible || total <= 0) return RECT{};
        const int thumbHeight = (std::max)(22, trackHeight * visible / total);
        const int travel = (std::max)(1, trackHeight - thumbHeight);
        const int maximum = ExpressionPickerMaxScroll(state);
        const int top = track.top + (maximum > 0
            ? state->scrollOffset * travel / maximum : 0);
        return RECT{ track.left, top, track.right, top + thumbHeight };
    }

    static void SetExpressionPickerScrollFromThumbY(
        ExpressionPickerState* state, int logicalY) {
        if (!state) return;
        const int maximum = ExpressionPickerMaxScroll(state);
        if (maximum <= 0) {
            state->scrollOffset = 0;
            return;
        }
        const RECT track = ExpressionPickerScrollTrack(state);
        const RECT thumb = ExpressionPickerScrollThumb(state);
        const int travel = (std::max)(1,
            static_cast<int>((track.bottom - track.top) -
                (thumb.bottom - thumb.top)));
        const int delta = logicalY - state->scrollDragStartY;
        state->scrollOffset = (std::clamp)(
            state->scrollDragStartOffset +
                static_cast<int>(std::lround(delta * maximum * 1.0 / travel)),
            0, maximum);
    }

    static POINT ExpressionPickerLogicalPoint(
        HWND picker, const ExpressionPickerState* state, int x, int y) {
        RECT client{};
        GetClientRect(picker, &client);
        const int width = (std::max)(1L, client.right - client.left);
        const int height = (std::max)(1L, client.bottom - client.top);
        return POINT{
            static_cast<LONG>(std::lround(
                x * kExpressionPickerWidth * 1.0 / width)),
            static_cast<LONG>(std::lround(
                y * ExpressionPickerHeight(state) * 1.0 / height)) };
    }

    static void DrawExpressionPicker(HWND picker, ExpressionPickerState* state) {
        PAINTSTRUCT paint{};
        HDC target = BeginPaint(picker, &paint);
        RECT actualClient{};
        GetClientRect(picker, &actualClient);
        RECT client{ 0, 0, kExpressionPickerWidth,
            ExpressionPickerHeight(state) };
        HDC dc = CreateCompatibleDC(target);
        HBITMAP bitmap = CreateCompatibleBitmap(target,
            (std::max)(1L, actualClient.right - actualClient.left),
            (std::max)(1L, actualClient.bottom - actualClient.top));
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        SetMapMode(dc, MM_ANISOTROPIC);
        SetWindowExtEx(dc, client.right, client.bottom, nullptr);
        SetViewportExtEx(dc,
            actualClient.right - actualClient.left,
            actualClient.bottom - actualClient.top, nullptr);
        PanelFill(dc, client, RGB(14, 24, 39));
        HBRUSH border = CreateSolidBrush(RGB(48, 122, 193));
        FrameRect(dc, &client, border);
        DeleteObject(border);

        const int visibleRows = ExpressionPickerVisibleRows(state);
        if (state->expressions.empty()) {
            PanelText(dc, Tr(L"暂无可用表情，请确认 VTube Studio API 已连接"),
                RECT{ 12, 10, client.right - 12, 10 + kExpressionPickerRowHeight },
                RGB(166, 198, 232), 13, false,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            const bool hasScroll = ExpressionPickerMaxScroll(state) > 0;
            for (int row = 0; row < visibleRows; ++row) {
                const int index = state->scrollOffset + row;
                if (index >= static_cast<int>(state->expressions.size())) break;
                const auto& expression = state->expressions[index];
                RECT rowRect{ 6, 6 + row * kExpressionPickerRowHeight,
                    client.right - (hasScroll ? 20 : 6),
                    6 + (row + 1) * kExpressionPickerRowHeight };
                if (row == state->hoveredRow) {
                    PanelFill(dc, rowRect, RGB(25, 55, 86));
                }
                RECT check{ 12, rowRect.top + 6, 30, rowRect.top + 24 };
                const bool selected = ExpressionSelected(state->overlay, expression.file);
                PanelFill(dc, check, selected ? RGB(55, 139, 221) : RGB(41, 58, 80));
                if (selected) {
                    PanelText(dc, L"✓", check, RGB(255, 255, 255), 14, true,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
                std::wstring label = Utf8ToWide(
                    expression.name.empty()
                        ? StripExpressionSuffix(expression.file)
                        : StripExpressionSuffix(expression.name));
                if (label.empty()) label = Tr(L"未命名表情");
                PanelText(dc, label,
                    RECT{ 40, rowRect.top, client.right - 14, rowRect.bottom },
                    RGB(232, 241, 253), 13, false,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            if (hasScroll) {
                const RECT track = ExpressionPickerScrollTrack(state);
                const RECT thumb = ExpressionPickerScrollThumb(state);
                PanelFill(dc, track, RGB(27, 43, 64));
                PanelFill(dc, thumb, state->draggingScrollThumb
                    ? RGB(91, 170, 240) : RGB(78, 139, 199));
            }
        }

        const int footerTop = client.bottom - 38;
        RECT separator{ 8, footerTop - 5, client.right - 8, footerTop - 4 };
        PanelFill(dc, separator, RGB(38, 58, 82));
        RECT finish{ client.right - 126, footerTop, client.right - 8, client.bottom - 7 };
        PanelFill(dc, finish, state->hoveredFinish
            ? RGB(45, 112, 180) : RGB(32, 91, 151));
        PanelText(dc, Tr(L"选择完成"), finish, RGB(244, 249, 255), 13, true,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        PanelText(dc,
            std::wstring(Tr(L"已选 ")) +
                std::to_wstring(state->overlay->hoverExpressionFiles_.size()) + Tr(L" 项"),
            RECT{ 12, footerTop, client.right - 140, client.bottom - 7 },
            RGB(166, 198, 232), 13, false,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SetMapMode(dc, MM_TEXT);
        BitBlt(target, 0, 0,
            actualClient.right - actualClient.left,
            actualClient.bottom - actualClient.top,
            dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
        EndPaint(picker, &paint);
    }

    static void CloseExpressionPicker(HWND picker, bool scheduleRestore) {
        if (!picker || !IsWindow(picker)) return;
        auto* state = reinterpret_cast<ExpressionPickerState*>(
            GetWindowLongPtrW(picker, GWLP_USERDATA));
        if (state) state->scheduleRestoreOnClose = scheduleRestore;
        DestroyWindow(picker);
    }

    static LRESULT CALLBACK ExpressionPickerProc(
        HWND picker, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* state = reinterpret_cast<ExpressionPickerState*>(
            GetWindowLongPtrW(picker, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            state = reinterpret_cast<ExpressionPickerState*>(create->lpCreateParams);
            SetWindowLongPtrW(picker, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        }
        if (!state || !state->overlay) {
            return DefWindowProcW(picker, message, wParam, lParam);
        }
        LayeredOverlay* overlay = state->overlay;
        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            DrawExpressionPicker(picker, state);
            return 0;
        case WM_MOUSEMOVE: {
            const POINT logical = ExpressionPickerLogicalPoint(
                picker, state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            const int y = logical.y;
            RECT client{ 0, 0, kExpressionPickerWidth,
                ExpressionPickerHeight(state) };
            const int visibleRows = ExpressionPickerVisibleRows(state);
            if (state->draggingScrollThumb && GetCapture() == picker) {
                SetExpressionPickerScrollFromThumbY(state, y);
                state->hoveredRow = -1;
                state->hoveredFinish = false;
                InvalidateRect(picker, nullptr, FALSE);
                return 0;
            }
            const RECT track = ExpressionPickerScrollTrack(state);
            const bool overScroll = ExpressionPickerMaxScroll(state) > 0 &&
                logical.x >= track.left - 4;
            const int row = !overScroll && y >= 6 &&
                y < 6 + visibleRows * kExpressionPickerRowHeight
                ? (y - 6) / kExpressionPickerRowHeight : -1;
            const bool finish = y >= client.bottom - 38 &&
                logical.x >= client.right - 126;
            if (row != state->hoveredRow || finish != state->hoveredFinish) {
                state->hoveredRow = row;
                state->hoveredFinish = finish;
                InvalidateRect(picker, nullptr, FALSE);
            }
            TRACKMOUSEEVENT tracking{};
            tracking.cbSize = sizeof(tracking);
            tracking.dwFlags = TME_LEAVE;
            tracking.hwndTrack = picker;
            TrackMouseEvent(&tracking);
            return 0;
        }
        case WM_MOUSELEAVE:
            state->hoveredRow = -1;
            state->hoveredFinish = false;
            InvalidateRect(picker, nullptr, FALSE);
            return 0;
        case WM_MOUSEWHEEL:
            if (state->expressions.size() > kExpressionPickerMaxVisibleRows) {
                const int maximum = static_cast<int>(state->expressions.size()) -
                    kExpressionPickerMaxVisibleRows;
                state->scrollOffset = (std::clamp)(state->scrollOffset +
                    (GET_WHEEL_DELTA_WPARAM(wParam) < 0 ? 1 : -1), 0, maximum);
                InvalidateRect(picker, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(picker);
            const POINT logical = ExpressionPickerLogicalPoint(
                picker, state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            const int x = logical.x;
            const int y = logical.y;
            RECT client{ 0, 0, kExpressionPickerWidth,
                ExpressionPickerHeight(state) };
            if (y >= client.bottom - 38 && x >= client.right - 126) {
                CloseExpressionPicker(picker, true);
                return 0;
            }
            if (ExpressionPickerMaxScroll(state) > 0) {
                const RECT track = ExpressionPickerScrollTrack(state);
                const RECT thumb = ExpressionPickerScrollThumb(state);
                if (x >= track.left - 4 && x <= track.right + 4 &&
                    y >= track.top && y <= track.bottom) {
                    if (!PtInRect(&thumb, POINT{ x, y })) {
                        const int thumbHeight = static_cast<int>(
                            thumb.bottom - thumb.top);
                        const int travel = (std::max)(1,
                            static_cast<int>(track.bottom - track.top) - thumbHeight);
                        const int position = (std::clamp)(
                            y - static_cast<int>(track.top) - thumbHeight / 2,
                            0, travel);
                        state->scrollOffset = static_cast<int>(std::lround(
                            position * ExpressionPickerMaxScroll(state) * 1.0 /
                            travel));
                    }
                    state->draggingScrollThumb = true;
                    state->scrollDragStartY = y;
                    state->scrollDragStartOffset = state->scrollOffset;
                    SetCapture(picker);
                    InvalidateRect(picker, nullptr, FALSE);
                    return 0;
                }
            }
            const int visibleRows = ExpressionPickerVisibleRows(state);
            if (y >= 6 && y < 6 + visibleRows * kExpressionPickerRowHeight) {
                const int index = state->scrollOffset +
                    (y - 6) / kExpressionPickerRowHeight;
                if (index >= 0 && index < static_cast<int>(state->expressions.size())) {
                    const std::string& file = state->expressions[index].file;
                    auto selected = std::find(overlay->hoverExpressionFiles_.begin(),
                        overlay->hoverExpressionFiles_.end(), file);
                    if (selected == overlay->hoverExpressionFiles_.end()) {
                        overlay->hoverExpressionFiles_.push_back(file);
                    } else {
                        overlay->hoverExpressionFiles_.erase(selected);
                    }
                    overlay->PreviewExpressionsFromPanel();
                    if (state->ownerPanel && IsWindow(state->ownerPanel)) {
                        RefreshPersonalPanel(state->ownerPanel);
                    }
                    InvalidateRect(picker, nullptr, FALSE);
                }
            }
            return 0;
        }
        case WM_LBUTTONUP:
            if (state->draggingScrollThumb) {
                state->draggingScrollThumb = false;
                if (GetCapture() == picker) ReleaseCapture();
                InvalidateRect(picker, nullptr, FALSE);
                return 0;
            }
            break;
        case WM_CAPTURECHANGED:
            state->draggingScrollThumb = false;
            InvalidateRect(picker, nullptr, FALSE);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_RETURN || wParam == VK_ESCAPE) {
                CloseExpressionPicker(picker, true);
                return 0;
            }
            break;
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                CloseExpressionPicker(picker, true);
                return 0;
            }
            break;
        case WM_DESTROY:
            overlay->expressionPickerHwnd_ = nullptr;
            if (state->scheduleRestoreOnClose && state->ownerPanel &&
                IsWindow(state->ownerPanel)) {
                // The 2.5 second preview wait begins only after the multi-select
                // dropdown is closed. While it is open, every checked expression
                // remains continuously applied.
                SetTimer(state->ownerPanel, 94,
                    kExpressionSelectionPreviewWaitMs, nullptr);
                overlay->suppressHoverExpressionUntilCursorLeaves_ = true;
                RefreshPersonalPanel(state->ownerPanel);
            }
            return 0;
        case WM_NCDESTROY:
            SetWindowLongPtrW(picker, GWLP_USERDATA, 0);
            delete state;
            return DefWindowProcW(picker, message, wParam, lParam);
        default:
            break;
        }
        return DefWindowProcW(picker, message, wParam, lParam);
    }

    static void ShowExpressionMenu(HWND panel, BorderDialogState* state) {
        if (!state || !state->overlay) return;
        LayeredOverlay* overlay = state->overlay;

        if (overlay->expressionPickerHwnd_ &&
            IsWindow(overlay->expressionPickerHwnd_)) {
            SetForegroundWindow(overlay->expressionPickerHwnd_);
            SetFocus(overlay->expressionPickerHwnd_);
            return;
        }
        KillTimer(panel, 94);
        overlay->CapturePanelExpressionState();
        auto* pickerState = new ExpressionPickerState{};
        pickerState->overlay = overlay;
        pickerState->expressions = overlay->vtsApi_.GetExpressions();
        if (pickerState->expressions.empty()) {
            overlay->vtsApi_.RequestExpressionState();
        }

        static const wchar_t kExpressionPickerClass[] =
            L"LilyVtsExpressionMultiPicker";
        static bool registered = false;
        if (!registered) {
            WNDCLASSW wc{};
            wc.lpfnWndProc = &LayeredOverlay::ExpressionPickerProc;
            wc.hInstance = overlay->instance_;
            wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
            wc.hbrBackground = nullptr;
            wc.lpszClassName = kExpressionPickerClass;
            registered = RegisterClassW(&wc) != 0 ||
                GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        }
        if (!registered) {
            delete pickerState;
            return;
        }

        const int base = overlay->borderMode_ == kBorderModeCustom ? 330 : 105;
        const int buttonTop = base +
            (overlay->hoverExpressionEnabled_ ? 411 : 373) - 42;
        const double panelScale = PersonalPanelScale(overlay);
        POINT popup{
            static_cast<LONG>(std::lround(20 * panelScale)),
            static_cast<LONG>(std::lround((buttonTop + 32) * panelScale)) };
        ClientToScreen(panel, &popup);
        const int pickerWidth = static_cast<int>(std::lround(
            kExpressionPickerWidth * panelScale));
        const int pickerHeight = static_cast<int>(std::lround(
            ExpressionPickerHeight(pickerState) * panelScale));
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        if (GetMonitorInfoW(MonitorFromPoint(popup, MONITOR_DEFAULTTONEAREST),
                &monitor)) {
            popup.x = static_cast<LONG>((std::clamp)(
                static_cast<int>(popup.x),
                static_cast<int>(monitor.rcWork.left),
                static_cast<int>(monitor.rcWork.right) - pickerWidth));
            if (popup.y + pickerHeight > monitor.rcWork.bottom) {
                POINT above{
                    static_cast<LONG>(std::lround(20 * panelScale)),
                    static_cast<LONG>(std::lround(buttonTop * panelScale)) };
                ClientToScreen(panel, &above);
                popup.y = above.y - pickerHeight;
            }
        }
        overlay->expressionPickerHwnd_ = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            kExpressionPickerClass, Tr(L"选择表情"), WS_POPUP,
            popup.x, popup.y, pickerWidth, pickerHeight,
            panel, nullptr, overlay->instance_, pickerState);
        if (!overlay->expressionPickerHwnd_) {
            delete pickerState;
            return;
        }
        overlay->PreviewExpressionsFromPanel();
        ShowWindow(overlay->expressionPickerHwnd_, SW_SHOW);
        SetForegroundWindow(overlay->expressionPickerHwnd_);
        SetFocus(overlay->expressionPickerHwnd_);
        UpdateWindow(overlay->expressionPickerHwnd_);
    }

    static void ApplyHoverExpressionDurationInput(
        HWND panel, BorderDialogState* state) {
        if (!state || !state->overlay) return;
        double seconds = 0.0;
        if (TryParseNonNegativeDecimal(state->hoverExpressionDurationInput, seconds)) {
            state->overlay->hoverExpressionDurationSeconds_ = seconds;
        }
        RefreshPersonalPanel(panel);
    }

    static void ApplyHoverOpacityRestoreDelayInput(
        HWND panel, BorderDialogState* state) {
        if (!state || !state->overlay) return;
        double seconds = 0.0;
        if (TryParseNonNegativeDecimal(
                state->hoverOpacityRestoreDelayInput, seconds)) {
            state->overlay->hoverOpacityRestoreDelaySeconds_ = seconds;
        }
        RefreshPersonalPanel(panel);
    }

    static void EditNonNegativeDecimal(
        std::wstring& input, bool& replaceOnNextInput, wchar_t character) {
        if (character == L'\b') {
            if (replaceOnNextInput) {
                input.clear();
                replaceOnNextInput = false;
            } else if (!input.empty()) {
                input.pop_back();
            }
            return;
        }
        const bool isDigit = character >= L'0' && character <= L'9';
        const bool isFirstDot = character == L'.' &&
            input.find(L'.') == std::wstring::npos;
        if (!isDigit && !isFirstDot) return;
        if (replaceOnNextInput) {
            input.clear();
            replaceOnNextInput = false;
        }
        input.push_back(character);
    }

    static void UpdatePersonalNumericDrag(HWND panel, BorderDialogState* state, int x) {
        auto* overlay = state->overlay;
        const int delta = x - state->numericPress.x;
        if (!state->numericDragged && std::abs(delta) < overlay->UiScaled(3)) return;
        state->numericDragged = true;
        const double value = (std::max)(0.0, state->numericBase + delta * .1);
        if (state->activeHit == 15) {
            state->hoverExpressionDurationInput = FormatNonNegativeDecimal(value);
            ApplyHoverExpressionDurationInput(panel, state);
        } else {
            state->hoverOpacityRestoreDelayInput = FormatNonNegativeDecimal(value);
            ApplyHoverOpacityRestoreDelayInput(panel, state);
        }
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
        case WM_GETDLGCODE:
            if (state->editingHoverExpressionDuration ||
                state->editingHoverOpacityRestoreDelay) {
                return DLGC_WANTALLKEYS | DLGC_WANTCHARS;
            }
            break;
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                POINT point{};
                GetCursorPos(&point);
                ScreenToClient(panel, &point);
                const int hit = PersonalPanelHitTest(
                    panel, overlay, point.x, point.y);
                if (hit == 15 || hit == 19) {
                    SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                    return TRUE;
                }
                const POINT logical = PersonalPanelLogicalPoint(
                    panel, overlay, point.x, point.y);
                if (logical.y < 44 && hit == 0) {
                    SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
                    return TRUE;
                }
            }
            break;
        case WM_KEYDOWN:
            if (state->editingHoverOpacityRestoreDelay) {
                if (wParam == VK_RETURN) {
                    state->editingHoverOpacityRestoreDelay = false;
                    state->replaceHoverOpacityRestoreDelayOnNextInput = false;
                    RefreshPersonalPanel(panel);
                    return 0;
                }
                if (wParam == VK_ESCAPE) {
                    state->editingHoverOpacityRestoreDelay = false;
                    state->replaceHoverOpacityRestoreDelayOnNextInput = false;
                    state->hoverOpacityRestoreDelayInput = FormatNonNegativeDecimal(
                        overlay->hoverOpacityRestoreDelaySeconds_);
                    RefreshPersonalPanel(panel);
                    return 0;
                }
            }
            if (state->editingHoverExpressionDuration) {
                if (wParam == VK_RETURN) {
                    state->editingHoverExpressionDuration = false;
                    state->replaceHoverExpressionDurationOnNextInput = false;
                    RefreshPersonalPanel(panel);
                    return 0;
                }
                if (wParam == VK_ESCAPE) {
                    state->editingHoverExpressionDuration = false;
                    state->replaceHoverExpressionDurationOnNextInput = false;
                    state->hoverExpressionDurationInput = FormatNonNegativeDecimal(
                        overlay->hoverExpressionDurationSeconds_);
                    RefreshPersonalPanel(panel);
                    return 0;
                }
            }
            break;
        case WM_CHAR:
            if (state->editingHoverOpacityRestoreDelay) {
                EditNonNegativeDecimal(
                    state->hoverOpacityRestoreDelayInput,
                    state->replaceHoverOpacityRestoreDelayOnNextInput,
                    static_cast<wchar_t>(wParam));
                ApplyHoverOpacityRestoreDelayInput(panel, state);
                return 0;
            }
            if (state->editingHoverExpressionDuration) {
                EditNonNegativeDecimal(
                    state->hoverExpressionDurationInput,
                    state->replaceHoverExpressionDurationOnNextInput,
                    static_cast<wchar_t>(wParam));
                ApplyHoverExpressionDurationInput(panel, state);
                // Deliberately consume minus signs, operators, spaces and
                // paste-style expressions: this field only accepts a plain
                // non-negative decimal number of seconds.
                return 0;
            }
            break;
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
                // Capture the user's complete expression setup as soon as
                // VTS returns it, rather than when a later preview happens.
                // This is the state we must restore after preview/cancel.
                overlay->CapturePanelExpressionState();
                overlay->UpdatePanelExpressionState();
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
            const int hit = PersonalPanelHitTest(panel, overlay, x, y);
            const POINT logical = PersonalPanelLogicalPoint(
                panel, overlay, x, y);
            if (hit != 15) {
                state->editingHoverExpressionDuration = false;
                state->replaceHoverExpressionDurationOnNextInput = false;
            }
            if (hit != 19) {
                state->editingHoverOpacityRestoreDelay = false;
                state->replaceHoverOpacityRestoreDelayOnNextInput = false;
            }
            if (logical.y < 44 && hit == 0) {
                RECT windowRect{};
                GetWindowRect(panel, &windowRect);
                POINT cursor{};
                GetCursorPos(&cursor);
                overlay->personalPanelManuallyPositioned_ = true;
                state->draggingPanel = true;
                state->dragStartCursor = cursor;
                state->dragStartWindow = POINT{ windowRect.left, windowRect.top };
                SetCapture(panel);
                return 0;
            }
            if (hit == 1) {
                RestorePersonalPanel(panel, state);
                return 0;
            }
            if (hit >= 10 && hit <= 12) {
                overlay->borderMode_ = hit - 10;
                SavePersonalPanelAppearance(*state);
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
            if (hit == 23) {
                overlay->ToggleShadowHoverFade();
                return 0;
            }
            if (hit == 9) {
                overlay->hoverFadeEnabled_ = !overlay->hoverFadeEnabled_;
                overlay->PositionBorderPanel();
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
                    overlay->hoverOpacityRestorePending_ = false;
                    overlay->hoverOpacityCursorWasOverModel_ = false;
                    overlay->hoverOpacityRestoreAt_ = Clock::time_point{};
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
            if (hit == 16) {
                if (!overlay->subjectHoverRegionConfigured_) {
                    overlay->showSubjectRegionPreview_ = false;
                    overlay->ShowSubjectSelectionOverlay();
                    return 0;
                }
                overlay->excludeEffectsFromHover_ = !overlay->excludeEffectsFromHover_;
                overlay->showSubjectRegionPreview_ = false;
                ++overlay->requested_;
                overlay->RenderFrame();
                RefreshPersonalPanel(panel);
                return 0;
            }
            if (hit == 17) {
                overlay->showSubjectRegionPreview_ = false;
                overlay->ShowSubjectSelectionOverlay();
                return 0;
            }
            if (hit == 13) {
                overlay->hoverExpressionEnabled_ = !overlay->hoverExpressionEnabled_;
                if (!overlay->hoverExpressionEnabled_) {
                    overlay->CancelHoverExpression();
                    state->editingHoverExpressionDuration = false;
                } else {
                    overlay->vtsApi_.RequestExpressionState();
                    state->hoverExpressionDurationInput = FormatNonNegativeDecimal(
                        overlay->hoverExpressionDurationSeconds_);
                }
                ++overlay->requested_;
                overlay->RenderFrame();
                // The duration editor is a real row, so grow/shrink the panel
                // immediately when the hover-expression option changes.
                overlay->PositionBorderPanel();
                return 0;
            }
            if (hit == 15) {
                state->activeHit = hit;
                state->numericPress = POINT{x, y};
                state->numericBase = overlay->hoverExpressionDurationSeconds_;
                state->numericDragged = false;
                SetCapture(panel);
                state->editingHoverExpressionDuration = true;
                state->replaceHoverExpressionDurationOnNextInput = true;
                state->hoverExpressionDurationInput = FormatNonNegativeDecimal(
                    overlay->hoverExpressionDurationSeconds_);
                SetForegroundWindow(panel);
                SetFocus(panel);
                RefreshPersonalPanel(panel);
                return 0;
            }
            if (hit == 19) {
                state->activeHit = hit;
                state->numericPress = POINT{x, y};
                state->numericBase = overlay->hoverOpacityRestoreDelaySeconds_;
                state->numericDragged = false;
                SetCapture(panel);
                state->editingHoverOpacityRestoreDelay = true;
                state->replaceHoverOpacityRestoreDelayOnNextInput = true;
                state->hoverOpacityRestoreDelayInput = FormatNonNegativeDecimal(
                    overlay->hoverOpacityRestoreDelaySeconds_);
                SetForegroundWindow(panel);
                SetFocus(panel);
                RefreshPersonalPanel(panel);
                return 0;
            }
            if (hit == 14) {
                ShowExpressionMenu(panel, state);
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

                CloseExpressionPicker(overlay->expressionPickerHwnd_, false);
                overlay->EndPanelExpressionSession();
                overlay->SaveUiSettings();
                Log("[toolbar] border_settings_saved mode=" + std::to_string(overlay->borderMode_) +
                    " thickness=" + std::to_string(overlay->borderThickness_));
                DestroyWindow(panel);
                return 0;
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            if (!state->trackingMouseLeave) {
                TRACKMOUSEEVENT tracking{};
                tracking.cbSize = sizeof(tracking);
                tracking.dwFlags = TME_LEAVE;
                tracking.hwndTrack = panel;
                state->trackingMouseLeave = TrackMouseEvent(&tracking) != FALSE;
            }
            if (state->draggingPanel && GetCapture() == panel) {
                POINT cursor{};
                GetCursorPos(&cursor);
                const int x = state->dragStartWindow.x +
                    cursor.x - state->dragStartCursor.x;
                const int y = state->dragStartWindow.y +
                    cursor.y - state->dragStartCursor.y;
                SetWindowPos(panel, HWND_TOPMOST, x, y, 0, 0,
                    SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                return 0;
            }
            {
                const POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                const int hoverHit = PersonalPanelHitTest(
                    panel, overlay, point.x, point.y);
                if (hoverHit != state->numericHintHit) {
                    state->numericHintHit = hoverHit;
                    if (state->numericHint) DestroyWindow(std::exchange(state->numericHint, nullptr));
                    if (hoverHit == 15 || hoverHit == 19) {
                        const int base = (overlay->borderMode_ == kBorderModeCustom ? 288 : 63) +
                            (overlay->ShowPersonalShadowFade() ? kShadowFadeRowHeight : 0);
                        RECT client{};
                        GetClientRect(panel, &client);
                        const int y = base + (hoverHit == 15 ? 375 : 215);
                        const auto px = [&](int value) { return MulDiv(value, client.right, 420); };
                        const RECT bounds{px(284), px(y), px(390), px(y + 26)};
                        state->numericHint = overlay->RegisterNumericHint(panel, &bounds);
                    }
                }
                const bool showSubjectPreview =
                    state->activeHit == 0 && overlay->subjectHoverRegionConfigured_ &&
                    (hoverHit == 16 || hoverHit == 17);
                if (showSubjectPreview != overlay->showSubjectRegionPreview_) {
                    overlay->showSubjectRegionPreview_ = showSubjectPreview;
                    ++overlay->requested_;
                    overlay->RenderFrame();
                }
            }
            if ((state->activeHit == 15 || state->activeHit == 19) && GetCapture() == panel) {
                UpdatePersonalNumericDrag(panel, state, GET_X_LPARAM(lParam));
                return 0;
            }
            if (state->activeHit == 3 && GetCapture() == panel) {
                POINT cursor{};
                GetCursorPos(&cursor);
                ScreenToClient(panel, &cursor);
                UpdatePersonalWheel(panel, state, cursor.x, cursor.y);
                return 0;
            }
            if (state->activeHit >= 4 && state->activeHit <= 8 &&
                GetCapture() == panel) {
                POINT cursor{};
                GetCursorPos(&cursor);
                ScreenToClient(panel, &cursor);
                UpdatePersonalSlider(panel, state, state->activeHit, cursor.x);
                return 0;
            }
            break;
        case WM_MOUSELEAVE:
            state->trackingMouseLeave = false;
            state->numericHintHit = 0;
            if (state->numericHint) DestroyWindow(std::exchange(state->numericHint, nullptr));
            if (overlay->showSubjectRegionPreview_) {
                overlay->showSubjectRegionPreview_ = false;
                ++overlay->requested_;
                overlay->RenderFrame();
            }
            return 0;
        case WM_LBUTTONUP:
            if (state->draggingPanel) {
                state->draggingPanel = false;
                if (GetCapture() == panel) ReleaseCapture();
                return 0;
            }
            if (state->activeHit != 0) {
                POINT cursor{};
                GetCursorPos(&cursor);
                ScreenToClient(panel, &cursor);
                const int x = cursor.x;
                const int y = cursor.y;
                if (state->activeHit == 15 || state->activeHit == 19) {
                    UpdatePersonalNumericDrag(panel, state, x);
                    if (state->numericDragged) overlay->SaveUiSettings();
                } else if (state->activeHit == 3) {
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
            state->draggingPanel = false;
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

                CloseExpressionPicker(overlay->expressionPickerHwnd_, false);
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

            CloseExpressionPicker(overlay->expressionPickerHwnd_, false);
            KillTimer(panel, 1);
            KillTimer(panel, 91);
            KillTimer(panel, 92);
            KillTimer(panel, 93);
            KillTimer(panel, 94);
            if (GetCapture() == panel) ReleaseCapture();
            // Do not let a lock-mode hover command survive the panel close and
            // overwrite the preview's final baseline restoration.
            overlay->CancelHoverExpression();
            overlay->EndPanelExpressionSession();
            overlay->borderDialogOpen_ = false;
            overlay->hoverOpacityPreviewActive_ = false;
            overlay->showHoverExpandPreview_ = false;
            overlay->hoverExpandEditing_ = false;
            overlay->hoverExpandPreviewAlpha_ = 0.0;
            overlay->showSubjectRegionPreview_ = false;
            overlay->personalPanelManuallyPositioned_ = false;
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
        state->originalHoverOpacityRestoreDelaySeconds =
            hoverOpacityRestoreDelaySeconds_;
        state->originalModelOpacityPercent = modelOpacityPercent_;
        state->originalHoverExpandPx = hoverExpandPx_;
        state->originalExcludeEffectsFromHover = excludeEffectsFromHover_;
        state->originalSubjectHoverRegionConfigured = subjectHoverRegionConfigured_;
        state->originalSubjectHoverRegionLeft = subjectHoverRegionLeft_;
        state->originalSubjectHoverRegionTop = subjectHoverRegionTop_;
        state->originalSubjectHoverRegionRight = subjectHoverRegionRight_;
        state->originalSubjectHoverRegionBottom = subjectHoverRegionBottom_;
        state->originalSubjectHoverMaskGridWidth = subjectHoverMaskGridWidth_;
        state->originalSubjectHoverMaskGridHeight = subjectHoverMaskGridHeight_;
        state->originalSubjectHoverMaskBits = subjectHoverMaskBits_;
        state->originalSubjectHoverTrackingReferenceValid =
            subjectHoverTrackingReferenceValid_;
        state->originalSubjectHoverTrackingReferenceCenterX =
            subjectHoverTrackingReferenceCenterX_;
        state->originalSubjectHoverTrackingReferenceCenterY =
            subjectHoverTrackingReferenceCenterY_;
        state->originalSubjectHoverTrackingReferenceWidth =
            subjectHoverTrackingReferenceWidth_;
        state->originalSubjectHoverTrackingReferenceHeight =
            subjectHoverTrackingReferenceHeight_;
        state->originalHoverExpressionEnabled = hoverExpressionEnabled_;
        state->originalHoverExpressionFiles = hoverExpressionFiles_;
        state->originalHoverExpressionDurationSeconds = hoverExpressionDurationSeconds_;
        state->hoverOpacityRestoreDelayInput = FormatNonNegativeDecimal(
            hoverOpacityRestoreDelaySeconds_);
        state->hoverExpressionDurationInput = FormatNonNegativeDecimal(
            hoverExpressionDurationSeconds_);
        CustomColorHueSaturation(customBorderColor_, state->colorHue, state->colorSaturation);
        state->colorValuePercent = CustomColorValuePercent(customBorderColor_);
        // Freeze any existing lock-mode hover session before state 1 is
        // captured for personalization previews.
        CancelHoverExpression();
        borderDialogOpen_ = true;
        personalPanelManuallyPositioned_ = false;
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
            // The custom duration input needs keyboard focus. TOOLWINDOW
            // keeps this panel out of Alt+Tab; unlike the model surface it
            // must be allowed to activate while the user edits a value.
            desktopMode_ ? 0 : WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            kPersonalPanelClass, Tr(L"个性化"), WS_POPUP,
            0, 0, PersonalPanelPixelWidth(this), PersonalPanelPixelHeight(this),
            toolbarHwnd_, nullptr, instance_, state);
        if (!borderPanelHwnd_) {
            Log("[toolbar] border_panel_failed error=" + std::to_string(GetLastError()));
            borderDialogOpen_ = false;
            delete state;
            return;
        }
        SetWindowPos(borderPanelHwnd_, WindowZOrder(), 0, 0,
            PersonalPanelPixelWidth(this), PersonalPanelPixelHeight(this),
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
        PositionBorderPanel();
        ShowWindow(borderPanelHwnd_, SW_SHOWNOACTIVATE);
        BeginPanelExpressionSession();
        vtsApi_.RequestExpressionState();
        SetTimer(borderPanelHwnd_, 93, kExpressionStatePollIntervalMs, nullptr);
        InvalidateRect(borderPanelHwnd_, nullptr, FALSE);
    }

    struct SubjectSelectionSnapshot {
        std::vector<std::vector<POINT>> polygons;
        std::vector<POINT> currentPolygon;
    };

    struct SubjectSelectionToolbarLayout {
        RECT panel{};
        RECT undo{};
        RECT redo{};
        RECT count{};
        RECT finish{};
    };

    static SubjectSelectionToolbarLayout SubjectSelectionToolbar() {
        constexpr int scale = 2;
        const int left = 0;
        const int top = 0;
        SubjectSelectionToolbarLayout layout;
        layout.panel = RECT{ left, top, left + 438 * scale, top + 56 * scale };
        layout.count = RECT{ left + 10 * scale, top + 8 * scale,
            left + 194 * scale, top + 48 * scale };
        layout.undo = RECT{ left + 202 * scale, top + 8 * scale,
            left + 242 * scale, top + 48 * scale };
        layout.redo = RECT{ left + 248 * scale, top + 8 * scale,
            left + 288 * scale, top + 48 * scale };
        layout.finish = RECT{ left + 298 * scale, top + 8 * scale,
            left + 428 * scale, top + 48 * scale };
        return layout;
    }

    void RefreshSubjectSelectionUi() {
        if (subjectSelectionHwnd_) {
            InvalidateRect(subjectSelectionHwnd_, nullptr, FALSE);
        }
        if (subjectSelectionToolbarHwnd_) {
            InvalidateRect(subjectSelectionToolbarHwnd_, nullptr, FALSE);
        }
    }

    SubjectSelectionSnapshot CurrentSubjectSelectionSnapshot() const {
        return SubjectSelectionSnapshot{
            subjectSelectionPolygons_, subjectSelectionPolygon_ };
    }

    void RestoreSubjectSelectionSnapshot(const SubjectSelectionSnapshot& snapshot) {
        subjectSelectionPolygons_ = snapshot.polygons;
        subjectSelectionPolygon_ = snapshot.currentPolygon;
        if (!subjectSelectionPolygon_.empty()) {
            subjectSelectionCursor_ = subjectSelectionPolygon_.back();
        }
    }

    void RecordSubjectSelectionEdit() {
        subjectSelectionUndo_.push_back(CurrentSubjectSelectionSnapshot());
        if (subjectSelectionUndo_.size() > 128) {
            subjectSelectionUndo_.erase(subjectSelectionUndo_.begin());
        }
        subjectSelectionRedo_.clear();
    }

    void UndoSubjectSelectionEdit() {
        if (subjectSelectionUndo_.empty()) return;
        subjectSelectionRedo_.push_back(CurrentSubjectSelectionSnapshot());
        RestoreSubjectSelectionSnapshot(subjectSelectionUndo_.back());
        subjectSelectionUndo_.pop_back();
        RefreshSubjectSelectionUi();
    }

    void RedoSubjectSelectionEdit() {
        if (subjectSelectionRedo_.empty()) return;
        subjectSelectionUndo_.push_back(CurrentSubjectSelectionSnapshot());
        RestoreSubjectSelectionSnapshot(subjectSelectionRedo_.back());
        subjectSelectionRedo_.pop_back();
        RefreshSubjectSelectionUi();
    }

    void CompleteCurrentSubjectPolygon() {
        if (subjectSelectionPolygon_.size() < 3) return;
        RecordSubjectSelectionEdit();
        subjectSelectionPolygons_.push_back(subjectSelectionPolygon_);
        subjectSelectionPolygon_.clear();
        subjectSelectionDragging_ = false;
        if (GetCapture() == subjectSelectionHwnd_) ReleaseCapture();
        RefreshSubjectSelectionUi();
    }

    void DrawSubjectSelectionOverlay(HWND window) {
        PAINTSTRUCT paint{};
        HDC target = BeginPaint(window, &paint);
        const int dirtyWidth = paint.rcPaint.right - paint.rcPaint.left;
        const int dirtyHeight = paint.rcPaint.bottom - paint.rcPaint.top;
        if (dirtyWidth <= 0 || dirtyHeight <= 0) {
            EndPaint(window, &paint);
            return;
        }
        // WM_MOUSEMOVE used to repaint the shade and the lasso directly onto
        // the full-screen layered window. The user could therefore see the
        // background erase before the new edge was drawn. Compose the dirty
        // area off-screen and publish it with one BitBlt instead.
        HDC dc = CreateCompatibleDC(target);
        HBITMAP backBuffer = CreateCompatibleBitmap(target, dirtyWidth, dirtyHeight);
        if (!dc || !backBuffer) {
            if (backBuffer) DeleteObject(backBuffer);
            if (dc) DeleteDC(dc);
            EndPaint(window, &paint);
            return;
        }
        HGDIOBJ oldBitmap = SelectObject(dc, backBuffer);
        RECT dirtyBuffer{ 0, 0, dirtyWidth, dirtyHeight };
        HBRUSH shade = CreateSolidBrush(RGB(8, 14, 24));
        FillRect(dc, &dirtyBuffer, shade);
        DeleteObject(shade);
        const int savedDc = SaveDC(dc);
        SetViewportOrgEx(dc, -paint.rcPaint.left, -paint.rcPaint.top, nullptr);
        RECT client{};
        GetClientRect(window, &client);

        HFONT titleFont = CreateFontW(-UiFontSize(window, 22), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE,
            FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        HFONT hintFont = CreateFontW(-UiFontSize(window, 15), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
            FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        HGDIOBJ oldFont = SelectObject(dc, titleFont);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(240, 248, 255));
        RECT title{ 28, 24, client.right - 28, 54 };
        DrawTextW(dc, Tr(L"手动框选悬停范围（支持多选）"), -1, &title,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, hintFont);
        SetTextColor(dc, RGB(185, 213, 244));
        RECT hint{ 28, 57, client.right - 28, 84 };
        DrawTextW(dc, Tr(L"逐点点击勾勒范围；双击或点击起点完成一个范围，然后可继续框选。回车完成全部，Esc 取消。"),
            -1, &hint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dc, oldFont);
        DeleteObject(titleFont);
        DeleteObject(hintFont);

        for (size_t polygonIndex = 0;
            polygonIndex < subjectSelectionPolygons_.size(); ++polygonIndex) {
            const auto& polygon = subjectSelectionPolygons_[polygonIndex];
            if (polygon.size() < 3) continue;
            HBRUSH clear = static_cast<HBRUSH>(GetStockObject(HOLLOW_BRUSH));
            HPEN completedPen = CreatePen(PS_SOLID, 3, RGB(112, 238, 137));
            HGDIOBJ oldBrush = SelectObject(dc, clear);
            HGDIOBJ oldPen = SelectObject(dc, completedPen);
            Polygon(dc, polygon.data(), static_cast<int>(polygon.size()));
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(completedPen);
            RECT numberRect{
                polygon.front().x + 10, polygon.front().y - 18,
                polygon.front().x + 70, polygon.front().y + 18 };
            PanelText(dc, std::to_wstring(polygonIndex + 1), numberRect,
                RGB(146, 255, 166), 15, true,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        if (!subjectSelectionPolygon_.empty()) {
            HBRUSH clear = static_cast<HBRUSH>(GetStockObject(HOLLOW_BRUSH));
            HPEN outline = CreatePen(PS_SOLID, 3, RGB(71, 189, 255));
            HGDIOBJ oldBrush = SelectObject(dc, clear);
            HGDIOBJ oldPen = SelectObject(dc, outline);
            std::vector<POINT> preview = subjectSelectionPolygon_;
            if (preview.back().x != subjectSelectionCursor_.x ||
                preview.back().y != subjectSelectionCursor_.y) {
                preview.push_back(subjectSelectionCursor_);
            }
            if (preview.size() >= 2) {
                Polyline(dc, preview.data(), static_cast<int>(preview.size()));
            }
            if (subjectSelectionPolygon_.size() >= 3) {
                POINT closing[2] = {
                    subjectSelectionCursor_, subjectSelectionPolygon_.front() };
                HPEN closePen = CreatePen(PS_DASH, 2, RGB(112, 238, 137));
                SelectObject(dc, closePen);
                Polyline(dc, closing, 2);
                SelectObject(dc, outline);
                DeleteObject(closePen);
            }
            for (size_t i = 0; i < subjectSelectionPolygon_.size(); ++i) {
                const POINT point = subjectSelectionPolygon_[i];
                HBRUSH vertexBrush = CreateSolidBrush(i == 0
                    ? RGB(112, 238, 137) : RGB(71, 189, 255));
                HGDIOBJ oldVertexBrush = SelectObject(dc, vertexBrush);
                const int radius = i == 0 ? 11 : 7;
                Ellipse(dc, point.x - radius, point.y - radius,
                    point.x + radius + 1, point.y + radius + 1);
                SelectObject(dc, oldVertexBrush);
                DeleteObject(vertexBrush);
            }
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(outline);
        }

        RestoreDC(dc, savedDc);
        BitBlt(target, paint.rcPaint.left, paint.rcPaint.top,
            dirtyWidth, dirtyHeight, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(backBuffer);
        DeleteDC(dc);
        EndPaint(window, &paint);
    }

    static void InvalidateSubjectSelectionSegment(
        HWND window, const POINT& from, const POINT& to) {
        RECT dirty{
            (std::min)(from.x, to.x),
            (std::min)(from.y, to.y),
            (std::max)(from.x, to.x) + 1,
            (std::max)(from.y, to.y) + 1 };
        // Includes the 3 px lasso stroke and the 11 px start anchor.
        InflateRect(&dirty, 14, 14);
        InvalidateRect(window, &dirty, FALSE);
    }

    void DrawSubjectSelectionToolbar(HWND window) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        const SubjectSelectionToolbarLayout toolbar = SubjectSelectionToolbar();
        PanelFill(dc, toolbar.panel, RGB(18, 31, 49));
        HPEN toolbarBorder = CreatePen(PS_SOLID, 2, RGB(66, 103, 143));
        HGDIOBJ oldToolbarPen = SelectObject(dc, toolbarBorder);
        HGDIOBJ oldToolbarBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, toolbar.panel.left, toolbar.panel.top,
            toolbar.panel.right, toolbar.panel.bottom);
        SelectObject(dc, oldToolbarBrush);
        SelectObject(dc, oldToolbarPen);
        DeleteObject(toolbarBorder);

        const bool canUndo = !subjectSelectionUndo_.empty();
        const bool canRedo = !subjectSelectionRedo_.empty();
        PanelFill(dc, toolbar.undo, canUndo ? RGB(30, 55, 84) : RGB(22, 38, 58));
        PanelFill(dc, toolbar.redo, canRedo ? RGB(30, 55, 84) : RGB(22, 38, 58));
        PanelText(dc, L"↶", toolbar.undo,
            canUndo ? RGB(230, 241, 255) : RGB(75, 94, 116), 46, false,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        PanelText(dc, L"↷", toolbar.redo,
            canRedo ? RGB(230, 241, 255) : RGB(75, 94, 116), 46, false,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        PanelText(dc,
            std::wstring(Tr(L"当前框选主体数量：")) +
                std::to_wstring(subjectSelectionPolygons_.size()),
            toolbar.count, RGB(210, 226, 245), 28, true,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        PanelFill(dc, toolbar.finish, RGB(36, 103, 171));
        PanelText(dc, Tr(L"完成  Enter"), toolbar.finish, RGB(245, 250, 255), 28, true,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(window, &paint);
    }

    struct SubjectAlphaComponent {
        bool valid = false;
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        int cells = 0;

        double CenterX() const { return (left + right) * 0.5; }
        double CenterY() const { return (top + bottom) * 0.5; }
        double Width() const { return static_cast<double>(right - left); }
        double Height() const { return static_cast<double>(bottom - top); }
    };

    static SubjectAlphaComponent LargestSubjectComponent(
        const std::vector<std::uint8_t>& bits, int gridWidth, int gridHeight) {
        SubjectAlphaComponent largest;
        if (gridWidth <= 0 || gridHeight <= 0 || bits.empty()) return largest;
        const size_t cellCount = static_cast<size_t>(gridWidth) * gridHeight;
        std::vector<std::uint8_t> visited(cellCount, 0);
        std::vector<int> pending;
        pending.reserve(1024);
        const auto occupied = [&bits, cellCount](int index) {
            return index >= 0 && static_cast<size_t>(index) < cellCount &&
                static_cast<size_t>(index) / 8 < bits.size() &&
                (bits[static_cast<size_t>(index) / 8] &
                    static_cast<std::uint8_t>(1u << (index % 8))) != 0;
        };
        for (int start = 0; start < static_cast<int>(cellCount); ++start) {
            if (visited[start] || !occupied(start)) continue;
            SubjectAlphaComponent component;
            component.valid = true;
            component.left = component.right = start % gridWidth;
            component.top = component.bottom = start / gridWidth;
            pending.clear();
            pending.push_back(start);
            visited[start] = 1;
            for (size_t cursor = 0; cursor < pending.size(); ++cursor) {
                const int index = pending[cursor];
                const int x = index % gridWidth;
                const int y = index / gridWidth;
                component.left = (std::min)(component.left, x);
                component.top = (std::min)(component.top, y);
                component.right = (std::max)(component.right, x + 1);
                component.bottom = (std::max)(component.bottom, y + 1);
                ++component.cells;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        const int nx = x + dx;
                        const int ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= gridWidth || ny >= gridHeight) continue;
                        const int neighbor = ny * gridWidth + nx;
                        if (!visited[neighbor] && occupied(neighbor)) {
                            visited[neighbor] = 1;
                            pending.push_back(neighbor);
                        }
                    }
                }
            }
            if (component.cells > largest.cells) largest = component;
        }
        return largest;
    }

    static std::vector<std::uint8_t> KeepLargestSubjectComponent(
        const std::vector<std::uint8_t>& bits, int gridWidth, int gridHeight) {
        const size_t cellCount = static_cast<size_t>(gridWidth) * gridHeight;
        std::vector<std::uint8_t> result((cellCount + 7) / 8, 0);
        if (gridWidth <= 0 || gridHeight <= 0 || bits.empty()) return result;
        std::vector<std::uint8_t> visited(cellCount, 0);
        std::vector<int> pending;
        std::vector<int> best;
        const auto occupied = [&bits, cellCount](int index) {
            return index >= 0 && static_cast<size_t>(index) < cellCount &&
                static_cast<size_t>(index) / 8 < bits.size() &&
                (bits[static_cast<size_t>(index) / 8] &
                    static_cast<std::uint8_t>(1u << (index % 8))) != 0;
        };
        for (int start = 0; start < static_cast<int>(cellCount); ++start) {
            if (visited[start] || !occupied(start)) continue;
            pending.clear();
            pending.push_back(start);
            visited[start] = 1;
            for (size_t cursor = 0; cursor < pending.size(); ++cursor) {
                const int index = pending[cursor];
                const int x = index % gridWidth;
                const int y = index / gridWidth;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        const int nx = x + dx;
                        const int ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= gridWidth || ny >= gridHeight) continue;
                        const int neighbor = ny * gridWidth + nx;
                        if (!visited[neighbor] && occupied(neighbor)) {
                            visited[neighbor] = 1;
                            pending.push_back(neighbor);
                        }
                    }
                }
            }
            if (pending.size() > best.size()) best = pending;
        }
        for (const int index : best) {
            result[static_cast<size_t>(index) / 8] |=
                static_cast<std::uint8_t>(1u << (index % 8));
        }
        return result;
    }

    // Select the component that best matches the lasso/previous tracked model,
    // instead of blindly taking the largest disconnected alpha island.  This
    // keeps small rain/snow/spark particles out while still retaining nearby
    // detached model pieces such as a tail or accessory.
    static std::vector<std::uint8_t> KeepScoredSubjectComponents(
        const std::vector<std::uint8_t>& bits, int gridWidth, int gridHeight,
        double preferredX, double preferredY, bool hasPreferred) {
        const size_t cellCount = static_cast<size_t>(gridWidth) * gridHeight;
        std::vector<std::uint8_t> result((cellCount + 7) / 8, 0);
        if (gridWidth <= 0 || gridHeight <= 0 || bits.empty()) return result;
        struct Candidate {
            SubjectAlphaComponent bounds;
            std::vector<int> cells;
            double distance = 0.0;
            double score = 0.0;
        };
        std::vector<std::uint8_t> visited(cellCount, 0);
        std::vector<int> pending;
        std::vector<Candidate> candidates;
        const auto occupied = [&bits, cellCount](int index) {
            return index >= 0 && static_cast<size_t>(index) < cellCount &&
                static_cast<size_t>(index) / 8 < bits.size() &&
                (bits[static_cast<size_t>(index) / 8] &
                    static_cast<std::uint8_t>(1u << (index % 8))) != 0;
        };
        for (int start = 0; start < static_cast<int>(cellCount); ++start) {
            if (visited[start] || !occupied(start)) continue;
            Candidate candidate;
            candidate.bounds.valid = true;
            candidate.bounds.left = candidate.bounds.right = start % gridWidth;
            candidate.bounds.top = candidate.bounds.bottom = start / gridWidth;
            pending.clear();
            pending.push_back(start);
            visited[start] = 1;
            for (size_t cursor = 0; cursor < pending.size(); ++cursor) {
                const int index = pending[cursor];
                const int x = index % gridWidth;
                const int y = index / gridWidth;
                candidate.cells.push_back(index);
                candidate.bounds.left = (std::min)(candidate.bounds.left, x);
                candidate.bounds.top = (std::min)(candidate.bounds.top, y);
                candidate.bounds.right = (std::max)(candidate.bounds.right, x + 1);
                candidate.bounds.bottom = (std::max)(candidate.bounds.bottom, y + 1);
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        const int nx = x + dx;
                        const int ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= gridWidth || ny >= gridHeight) continue;
                        const int neighbor = ny * gridWidth + nx;
                        if (!visited[neighbor] && occupied(neighbor)) {
                            visited[neighbor] = 1;
                            pending.push_back(neighbor);
                        }
                    }
                }
            }
            candidate.bounds.cells = static_cast<int>(candidate.cells.size());
            const double dx = candidate.bounds.CenterX() - preferredX;
            const double dy = candidate.bounds.CenterY() - preferredY;
            candidate.distance = std::hypot(dx, dy);
            // Area matters, but distance to the user-selected subject center
            // matters more than a huge isolated effect at the edge.
            candidate.score = std::log1p(static_cast<double>(candidate.cells.size())) * 55.0 -
                (hasPreferred ? candidate.distance * 2.0 : 0.0);
            candidates.push_back(std::move(candidate));
        }
        if (candidates.empty()) return result;
        size_t anchorIndex = 0;
        for (size_t i = 1; i < candidates.size(); ++i) {
            if (candidates[i].score > candidates[anchorIndex].score) anchorIndex = i;
        }
        const Candidate& anchor = candidates[anchorIndex];
        const int attachMinCells = (std::max)(2, anchor.bounds.cells / 250);
        const auto bboxGap = [&anchor](const Candidate& candidate) {
            const int gapX = (candidate.bounds.right < anchor.bounds.left)
                ? anchor.bounds.left - candidate.bounds.right
                : (anchor.bounds.right < candidate.bounds.left)
                    ? candidate.bounds.left - anchor.bounds.right : 0;
            const int gapY = (candidate.bounds.bottom < anchor.bounds.top)
                ? anchor.bounds.top - candidate.bounds.bottom
                : (anchor.bounds.bottom < candidate.bounds.top)
                    ? candidate.bounds.top - anchor.bounds.bottom : 0;
            return (std::max)(gapX, gapY);
        };
        for (size_t i = 0; i < candidates.size(); ++i) {
            const Candidate& candidate = candidates[i];
            const bool keep = i == anchorIndex ||
                (candidate.bounds.cells >= attachMinCells && bboxGap(candidate) <= 10);
            if (!keep) continue;
            for (const int index : candidate.cells) {
                result[static_cast<size_t>(index) / 8] |=
                    static_cast<std::uint8_t>(1u << (index % 8));
            }
        }
        return result;
    }

    SubjectAlphaComponent CurrentSubjectAlphaComponent(
        int gridWidth, int gridHeight) const {
        SubjectAlphaComponent empty;
        if (gridWidth <= 0 || gridHeight <= 0 || dibWidth_ <= 0 || dibHeight_ <= 0) {
            return empty;
        }
        const bool haveRawModel = !hoverPreviewBasePixels_.empty() &&
            hoverPreviewBaseWidth_ == dibWidth_ &&
            hoverPreviewBaseHeight_ == dibHeight_;
        if (!haveRawModel && !dibBits_) return empty;
        const auto* pixels = haveRawModel
            ? hoverPreviewBasePixels_.data()
            : static_cast<const std::uint8_t*>(dibBits_);
        std::vector<std::uint8_t> bits(
            (static_cast<size_t>(gridWidth) * gridHeight + 7) / 8, 0);
        for (int gy = 0; gy < gridHeight; ++gy) {
            const int top = gy * dibHeight_ / gridHeight;
            const int bottom = (std::max)(top + 1, (gy + 1) * dibHeight_ / gridHeight);
            for (int gx = 0; gx < gridWidth; ++gx) {
                const int left = gx * dibWidth_ / gridWidth;
                const int right = (std::max)(left + 1, (gx + 1) * dibWidth_ / gridWidth);
                bool occupied = false;
                for (int y = top; y < (std::min)(bottom, dibHeight_) && !occupied; ++y) {
                    for (int x = left; x < (std::min)(right, dibWidth_); ++x) {
                        if (pixels[(static_cast<size_t>(y) * dibWidth_ + x) * 4 + 3] > 8) {
                            occupied = true;
                            break;
                        }
                    }
                }
                if (occupied) {
                    const size_t index = static_cast<size_t>(gy) * gridWidth + gx;
                    bits[index / 8] |= static_cast<std::uint8_t>(1u << (index % 8));
                }
            }
        }
        const bool hasPreferred = subjectHoverTrackingReferenceValid_;
        double preferredX = gridWidth * 0.5;
        double preferredY = gridHeight * 0.5;
        if (hasPreferred) {
            preferredX = subjectHoverTrackingCurrentCenterX_;
            preferredY = subjectHoverTrackingCurrentCenterY_;
        } else if (subjectHoverRegionConfigured_) {
            preferredX = (subjectHoverRegionLeft_ + subjectHoverRegionRight_) *
                0.0001 * gridWidth * 0.5;
            preferredY = (subjectHoverRegionTop_ + subjectHoverRegionBottom_) *
                0.0001 * gridHeight * 0.5;
        }
        const auto selected = KeepScoredSubjectComponents(
            bits, gridWidth, gridHeight, preferredX, preferredY, true);
        SubjectAlphaComponent selectedComponent = LargestSubjectComponent(
            selected, gridWidth, gridHeight);
        if (selectedComponent.valid) return selectedComponent;
        return LargestSubjectComponent(bits, gridWidth, gridHeight);
    }

    void CaptureSubjectTrackingReference() {
        const SubjectAlphaComponent component = CurrentSubjectAlphaComponent(
            subjectHoverMaskGridWidth_, subjectHoverMaskGridHeight_);
        if (!component.valid || component.cells < 4) {
            subjectHoverTrackingReferenceValid_ = false;
            return;
        }
        subjectHoverTrackingReferenceCenterX_ = component.CenterX();
        subjectHoverTrackingReferenceCenterY_ = component.CenterY();
        subjectHoverTrackingReferenceWidth_ = component.Width();
        subjectHoverTrackingReferenceHeight_ = component.Height();
        subjectHoverTrackingCurrentCenterX_ = component.CenterX();
        subjectHoverTrackingCurrentCenterY_ = component.CenterY();
        subjectHoverTrackingScale_ = 1.0;
        subjectHoverTrackingReferenceValid_ = true;
    }

    void UpdateSubjectHoverTracking() {
        if (!excludeEffectsFromHover_ || !subjectHoverRegionConfigured_ ||
            subjectHoverMaskGridWidth_ <= 0 || subjectHoverMaskGridHeight_ <= 0 ||
            subjectHoverMaskBits_.empty()) {
            return;
        }
        const auto now = Clock::now();
        if (subjectHoverTrackingLastUpdate_ != Clock::time_point{} &&
            now - subjectHoverTrackingLastUpdate_ < std::chrono::milliseconds(33)) {
            return;
        }
        subjectHoverTrackingLastUpdate_ = now;
        const SubjectAlphaComponent component = CurrentSubjectAlphaComponent(
            subjectHoverMaskGridWidth_, subjectHoverMaskGridHeight_);
        if (!component.valid || component.cells < 4) return;
        if (!subjectHoverTrackingReferenceValid_) {
            CaptureSubjectTrackingReference();
            return;
        }
        const double scaleX = component.Width() /
            (std::max)(1.0, subjectHoverTrackingReferenceWidth_);
        const double scaleY = component.Height() /
            (std::max)(1.0, subjectHoverTrackingReferenceHeight_);
        const double targetScale = (scaleX + scaleY) * 0.5;
        // A transient full-screen effect must not be allowed to replace the
        // tracked model. Normal VTS model scaling stays well inside this range.
        if (targetScale < 0.35 || targetScale > 3.0) return;
        subjectHoverTrackingCurrentCenterX_ = component.CenterX();
        subjectHoverTrackingCurrentCenterY_ = component.CenterY();
        // Position follows the received frame immediately. Scale is smoothed
        // slightly because blinking and hair physics change the alpha bounds.
        subjectHoverTrackingScale_ +=
            (targetScale - subjectHoverTrackingScale_) * 0.25;
    }

    static bool PointInPolygon(const std::vector<POINT>& polygon, double x, double y) {
        if (polygon.size() < 3) return false;
        bool inside = false;
        for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
            const double xi = polygon[i].x;
            const double yi = polygon[i].y;
            const double xj = polygon[j].x;
            const double yj = polygon[j].y;
            const bool crosses = ((yi > y) != (yj > y)) &&
                (x < (xj - xi) * (y - yi) / ((yj - yi) == 0.0 ? 1.0 : (yj - yi)) + xi);
            if (crosses) inside = !inside;
        }
        return inside;
    }

    bool CaptureSubjectHoverMask(
        const std::vector<std::vector<POINT>>& localPolygons) {
        if (dibWidth_ <= 0 || dibHeight_ <= 0 || localPolygons.empty()) return false;
        const int gridWidth =
            (dibWidth_ + kSubjectMaskCellPx - 1) / kSubjectMaskCellPx;
        const int gridHeight =
            (dibHeight_ + kSubjectMaskCellPx - 1) / kSubjectMaskCellPx;
        std::vector<std::uint8_t> bits(
            (static_cast<size_t>(gridWidth) * gridHeight + 7) / 8, 0);
        size_t occupiedCells = 0;
        for (int gy = 0; gy < gridHeight; ++gy) {
            for (int gx = 0; gx < gridWidth; ++gx) {
                const auto output = vtsfloat::ImageOutputPoint(subjectImageMapping_,
                    (gx + 0.5) / gridWidth, (gy + 0.5) / gridHeight);
                bool inside = false;
                for (const auto& polygon : localPolygons) {
                    if (PointInPolygon(polygon, output[0] * dibWidth_, output[1] * dibHeight_)) {
                        inside = true;
                        break;
                    }
                }
                if (!inside) continue;
                const size_t index = static_cast<size_t>(gy) * gridWidth + gx;
                bits[index / 8] |= static_cast<std::uint8_t>(1u << (index % 8));
                ++occupiedCells;
            }
        }
        if (occupiedCells == 0) return false;
        subjectHoverMaskGridWidth_ = gridWidth;
        subjectHoverMaskGridHeight_ = gridHeight;
        subjectHoverMaskBits_ = std::move(bits);
        subjectHoverMaskDirty_ = true;
        subjectHoverTrackingReferenceValid_ = false;
        subjectHoverTrackingScale_ = 1.0;
        Log("[hover subject] manual regions=" +
            std::to_string(localPolygons.size()) +
            " cells=" + std::to_string(occupiedCells));
        return true;
    }

    void FinishSubjectSelection(bool commit) {
        if (!subjectSelectionHwnd_) return;
        if (commit) {
            std::vector<std::vector<POINT>> polygons = subjectSelectionPolygons_;
            if (subjectSelectionPolygon_.size() >= 3) {
                polygons.push_back(subjectSelectionPolygon_);
            }
            if (!polygons.empty()) {
            RECT overlayRect{};
            GetWindowRect(hwnd_, &overlayRect);
                const int width = (std::max)(1L, overlayRect.right - overlayRect.left);
                const int height = (std::max)(1L, overlayRect.bottom - overlayRect.top);
                std::vector<std::vector<POINT>> localPolygons;
                localPolygons.reserve(polygons.size());
                int localLeft = width;
                int localTop = height;
                int localRight = 0;
                int localBottom = 0;
                for (const auto& polygon : polygons) {
                    if (polygon.size() < 3) continue;
                    std::vector<POINT> localPolygon;
                    localPolygon.reserve(polygon.size());
                    for (const POINT point : polygon) {
                        const POINT localPoint{
                            point.x + subjectSelectionVirtualLeft_ - overlayRect.left,
                            point.y + subjectSelectionVirtualTop_ - overlayRect.top };
                        localPolygon.push_back(localPoint);
                        localLeft = (std::min)(localLeft, static_cast<int>(localPoint.x));
                        localTop = (std::min)(localTop, static_cast<int>(localPoint.y));
                        localRight = (std::max)(localRight, static_cast<int>(localPoint.x));
                        localBottom = (std::max)(localBottom, static_cast<int>(localPoint.y));
                    }
                    localPolygons.push_back(std::move(localPolygon));
                }
                localLeft = (std::clamp)(localLeft, 0, width);
                localTop = (std::clamp)(localTop, 0, height);
                localRight = (std::clamp)(localRight, 0, width);
                localBottom = (std::clamp)(localBottom, 0, height);
                const int newRegionLeft = (std::clamp)(
                    localLeft * 10000 / width, 0, 10000);
                const int newRegionTop = (std::clamp)(
                    localTop * 10000 / height, 0, 10000);
                const int newRegionRight = (std::clamp)(
                    localRight * 10000 / width, 0, 10000);
                const int newRegionBottom = (std::clamp)(
                    localBottom * 10000 / height, 0, 10000);
                const bool maskCaptured = localLeft < localRight && localTop < localBottom &&
                    CaptureSubjectHoverMask(localPolygons);
                if (maskCaptured && newRegionLeft < newRegionRight &&
                    newRegionTop < newRegionBottom) {
                    subjectHoverRegionLeft_ = newRegionLeft;
                    subjectHoverRegionTop_ = newRegionTop;
                    subjectHoverRegionRight_ = newRegionRight;
                    subjectHoverRegionBottom_ = newRegionBottom;
                    subjectHoverRegionConfigured_ = true;
                    SaveUiSettings();
                    Log("[hover subject] manual multi-region mask saved");
                }
            }
        }
        if (subjectSelectionToolbarHwnd_ && IsWindow(subjectSelectionToolbarHwnd_)) {
            const HWND toolbar = subjectSelectionToolbarHwnd_;
            subjectSelectionToolbarHwnd_ = nullptr;
            DestroyWindow(toolbar);
        }
        if (subjectSelectionHwnd_ && IsWindow(subjectSelectionHwnd_)) {
            DestroyWindow(subjectSelectionHwnd_);
        }
    }

    void ShowSubjectSelectionOverlay() {
        if (subjectSelectionHwnd_ && IsWindow(subjectSelectionHwnd_)) {
            SetForegroundWindow(subjectSelectionHwnd_);
            return;
        }
        // Remove temporary preview layers while the user manually defines the
        // hover regions. The received model frame remains frozen underneath.
        showSubjectRegionPreview_ = false;
        showHoverExpandPreview_ = false;
        hoverExpandPreviewAlpha_ = 0.0;
        subjectHoverTrackingReferenceValid_ = false;
        subjectHoverTrackingLastUpdate_ = Clock::time_point{};
        RenderFrame();
        subjectSelectionVirtualLeft_ = GetSystemMetrics(SM_XVIRTUALSCREEN);
        subjectSelectionVirtualTop_ = GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        subjectSelectionPolygons_.clear();
        subjectSelectionPolygon_.clear();
        subjectSelectionUndo_.clear();
        subjectSelectionRedo_.clear();
        subjectSelectionCursor_ = POINT{};
        subjectSelectionDragging_ = false;
        subjectSelectionHwnd_ = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
            kSubjectSelectionClass, kWindowTitle, WS_POPUP,
            subjectSelectionVirtualLeft_, subjectSelectionVirtualTop_, width, height,
            nullptr, nullptr, instance_, this);
        if (!subjectSelectionHwnd_) {
            Log("[hover subject] selection window failed error=" +
                std::to_string(GetLastError()));
            return;
        }
        RefreshDebugReadout(false);
        SetLayeredWindowAttributes(subjectSelectionHwnd_, 0, 128, LWA_ALPHA);
        SetWindowPos(subjectSelectionHwnd_, HWND_TOPMOST,
            subjectSelectionVirtualLeft_, subjectSelectionVirtualTop_, width, height,
            SWP_SHOWWINDOW);
        const SubjectSelectionToolbarLayout toolbar = SubjectSelectionToolbar();
        const int toolbarWidth = toolbar.panel.right - toolbar.panel.left;
        const int toolbarHeight = toolbar.panel.bottom - toolbar.panel.top;
        const int toolbarX = subjectSelectionVirtualLeft_ +
            (std::max)(16, width - toolbarWidth - 24);
        const int toolbarY = subjectSelectionVirtualTop_ +
            (std::max)(16, height - toolbarHeight - 24);
        subjectSelectionToolbarHwnd_ = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            kSubjectSelectionToolbarClass, kWindowTitle, WS_POPUP,
            toolbarX, toolbarY, toolbarWidth, toolbarHeight,
            subjectSelectionHwnd_, nullptr, instance_, this);
        if (subjectSelectionToolbarHwnd_) {
            SetWindowPos(subjectSelectionToolbarHwnd_, HWND_TOPMOST,
                toolbarX, toolbarY, toolbarWidth, toolbarHeight,
                SWP_NOACTIVATE | SWP_SHOWWINDOW);
        } else {
            Log("[hover subject] selection toolbar failed error=" +
                std::to_string(GetLastError()));
        }
        SetForegroundWindow(subjectSelectionHwnd_);
        SetFocus(subjectSelectionHwnd_);
    }

    static LRESULT CALLBACK SubjectSelectionWindowProc(
        HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* overlay = reinterpret_cast<LayeredOverlay*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            overlay = static_cast<LayeredOverlay*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(overlay));
        }
        if (!overlay) return DefWindowProcW(window, message, wParam, lParam);
        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            overlay->DrawSubjectSelectionOverlay(window);
            return 0;
        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr, IDC_CROSS));
            return TRUE;
        case WM_LBUTTONDOWN: {
            const POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (overlay->subjectSelectionPolygon_.size() >= 3) {
                const POINT first = overlay->subjectSelectionPolygon_.front();
                const int dx = point.x - first.x;
                const int dy = point.y - first.y;
                if (dx * dx + dy * dy <= 22 * 22) {
                    overlay->subjectSelectionCursor_ = first;
                    overlay->CompleteCurrentSubjectPolygon();
                    return 0;
                }
            }
            overlay->RecordSubjectSelectionEdit();
            overlay->subjectSelectionPolygon_.push_back(point);
            overlay->subjectSelectionCursor_ = point;
            overlay->subjectSelectionDragging_ = true;
            SetCapture(window);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            if (overlay->subjectSelectionPolygon_.size() >= 3) {
                overlay->CompleteCurrentSubjectPolygon();
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            const POINT nextCursor{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (!overlay->subjectSelectionPolygon_.empty()) {
                const POINT previousCursor = overlay->subjectSelectionCursor_;
                const POINT last = overlay->subjectSelectionPolygon_.back();
                InvalidateSubjectSelectionSegment(window, last, previousCursor);
                InvalidateSubjectSelectionSegment(window, last, nextCursor);
                if (overlay->subjectSelectionPolygon_.size() >= 3) {
                    const POINT first = overlay->subjectSelectionPolygon_.front();
                    InvalidateSubjectSelectionSegment(window, first, previousCursor);
                    InvalidateSubjectSelectionSegment(window, first, nextCursor);
                }
            }
            overlay->subjectSelectionCursor_ = nextCursor;
            return 0;
        }
        case WM_LBUTTONUP:
            // A lasso point is committed on button-down; button-up only stops
            // capture so the cursor can continue previewing the next edge.
            overlay->subjectSelectionDragging_ = false;
            if (GetCapture() == window) ReleaseCapture();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_RBUTTONDOWN:
            overlay->UndoSubjectSelectionEdit();
            return 0;
        case WM_KEYDOWN:
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && wParam == 'Z') {
                if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
                    overlay->RedoSubjectSelectionEdit();
                } else {
                    overlay->UndoSubjectSelectionEdit();
                }
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && wParam == 'Y') {
                overlay->RedoSubjectSelectionEdit();
                return 0;
            }
            if (wParam == VK_RETURN) {
                overlay->FinishSubjectSelection(true);
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                overlay->FinishSubjectSelection(false);
                return 0;
            }
            break;
        case WM_CLOSE:
            overlay->FinishSubjectSelection(false);
            return 0;
        case WM_DESTROY:
            if (GetCapture() == window) ReleaseCapture();
            if (overlay->subjectSelectionToolbarHwnd_ &&
                IsWindow(overlay->subjectSelectionToolbarHwnd_)) {
                const HWND toolbar = overlay->subjectSelectionToolbarHwnd_;
                overlay->subjectSelectionToolbarHwnd_ = nullptr;
                DestroyWindow(toolbar);
            }
            overlay->subjectSelectionHwnd_ = nullptr;
            overlay->subjectSelectionPolygons_.clear();
            overlay->subjectSelectionPolygon_.clear();
            overlay->subjectSelectionUndo_.clear();
            overlay->subjectSelectionRedo_.clear();
            overlay->subjectSelectionDragging_ = false;
            overlay->RenderFrame();
            if (overlay->borderPanelHwnd_) {
                RefreshPersonalPanel(overlay->borderPanelHwnd_);
            }
            return 0;
        default:
            break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    static LRESULT CALLBACK SubjectSelectionToolbarWindowProc(
        HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* overlay = reinterpret_cast<LayeredOverlay*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            overlay = static_cast<LayeredOverlay*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(overlay));
        }
        if (!overlay) return DefWindowProcW(window, message, wParam, lParam);
        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            overlay->DrawSubjectSelectionToolbar(window);
            return 0;
        case WM_SETCURSOR: {
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(window, &point);
            const auto toolbar = SubjectSelectionToolbar();
            const bool action = PtInRect(&toolbar.undo, point) ||
                PtInRect(&toolbar.redo, point) || PtInRect(&toolbar.finish, point);
            SetCursor(LoadCursorW(nullptr, action ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        case WM_LBUTTONDOWN: {
            SetFocus(window);
            const POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const auto toolbar = SubjectSelectionToolbar();
            if (PtInRect(&toolbar.undo, point)) {
                overlay->UndoSubjectSelectionEdit();
            } else if (PtInRect(&toolbar.redo, point)) {
                overlay->RedoSubjectSelectionEdit();
            } else if (PtInRect(&toolbar.finish, point)) {
                overlay->FinishSubjectSelection(true);
            }
            return 0;
        }
        case WM_KEYDOWN:
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && wParam == 'Z') {
                if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
                    overlay->RedoSubjectSelectionEdit();
                } else {
                    overlay->UndoSubjectSelectionEdit();
                }
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && wParam == 'Y') {
                overlay->RedoSubjectSelectionEdit();
                return 0;
            }
            if (wParam == VK_RETURN) {
                overlay->FinishSubjectSelection(true);
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                overlay->FinishSubjectSelection(false);
                return 0;
            }
            break;
        case WM_CLOSE:
            overlay->FinishSubjectSelection(false);
            return 0;
        case WM_DESTROY:
            if (overlay->subjectSelectionToolbarHwnd_ == window) {
                overlay->subjectSelectionToolbarHwnd_ = nullptr;
            }
            return 0;
        default:
            break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    static POINT SidePanelPosition(const RECT& anchor, const RECT& work, const RECT* toolbar,
                                   SIZE panel, int gap, LONG desiredY) {
        const LONG leftEdge = toolbar ? (std::min)(anchor.left, toolbar->left) : anchor.left;
        const LONG rightEdge = toolbar ? (std::max)(anchor.right, toolbar->right) : anchor.right;
        const LONG leftX = leftEdge - panel.cx - gap;
        const LONG rightX = rightEdge + gap;
        const bool leftFits = leftX >= work.left;
        const bool rightFits = rightX + panel.cx <= work.right;
        const bool preferRight = anchor.left + anchor.right < work.left + work.right;
        LONG x = rightFits && (preferRight || !leftFits) ? rightX : leftX;
        LONG y = desiredY;
        const LONG maxY = (std::max)(work.top, work.bottom - panel.cy);
        if (!leftFits && !rightFits) {
            x = preferRight ? work.right - panel.cx : work.left;
            if (toolbar && toolbar->bottom + gap <= maxY) y = (std::max)(y, toolbar->bottom + gap);
        }
        return POINT{(std::clamp)(x, work.left, (std::max)(work.left, work.right - panel.cx)),
            (std::clamp)(y, work.top, maxY)};
    }

    static POINT PersonalPanelPosition(const RECT& model, const RECT& work, const RECT* toolbar, SIZE size) {
        return SidePanelPosition(model, work, toolbar, size, 8, model.bottom - size.cy);
    }

    std::optional<POINT> DockedPersonalPanelPosition(const RECT& model, const RECT& work, const RECT* toolbar) const {
        if (cropEditing_ || personalPanelManuallyPositioned_ || !borderPanelHwnd_) return std::nullopt;
        RECT current{};
        if (!GetWindowRect(borderPanelHwnd_, &current) || !IsWindowVisible(borderPanelHwnd_)) return std::nullopt;
        const POINT position = PersonalPanelPosition(model, work, toolbar,
            SIZE{current.right - current.left, current.bottom - current.top});
        if (position.x == current.left && position.y == current.top) return std::nullopt;
        return position;
    }

    void PositionBorderPanel() {
        if (cropEditing_ || !borderPanelHwnd_ || !IsWindow(borderPanelHwnd_) || !hwnd_) return;
        const int panelW = PersonalPanelPixelWidth(this);
        const int panelH = PersonalPanelPixelHeight(this);
        RECT currentPanel{};
        GetWindowRect(borderPanelHwnd_, &currentPanel);
        const bool sizeChanged =
            currentPanel.right - currentPanel.left != panelW ||
            currentPanel.bottom - currentPanel.top != panelH;
        const UINT resizeFlags = sizeChanged ? SWP_NOCOPYBITS : 0;
        if (personalPanelManuallyPositioned_) {
            if (sizeChanged) {
                SetWindowPos(borderPanelHwnd_, WindowZOrder(), 0, 0, panelW, panelH,
                    SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW | resizeFlags);
                RefreshPersonalPanel(borderPanelHwnd_);
            }
            return;
        }
        RECT model{};
        GetWindowRect(hwnd_, &model);
        MONITORINFO mi{sizeof(MONITORINFO)};
        if (!GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi)) return;
        RECT toolbar{};
        const bool hasToolbar = IsWindowVisible(toolbarHwnd_) && GetWindowRect(toolbarHwnd_, &toolbar);
        const POINT position = PersonalPanelPosition(model, mi.rcWork, hasToolbar ? &toolbar : nullptr,
            SIZE{panelW, panelH});
        SetWindowPos(borderPanelHwnd_, WindowZOrder(), position.x, position.y, panelW, panelH,
            SWP_NOACTIVATE | SWP_SHOWWINDOW | resizeFlags);
        if (sizeChanged) {
            // Do not let Windows preserve the old footer pixels when an option
            // inserts or removes a row. Repaint the complete resized panel now.
            RefreshPersonalPanel(borderPanelHwnd_);
        }
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
                reinterpret_cast<LPARAM>(Tr(L"自定义")));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_MODE, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(Tr(L"跑马灯渐变")));
            SendDlgItemMessageW(
                dialog, IDC_BORDER_MODE, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(Tr(L"普通渐变")));
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

    bool CropActive() const {
        return !EqualRect(&cropSelection_, &kFullCrop);
    }

    static RECT CropPixels(const RECT& selection, int width, int height) {
        if (width <= 0 || height <= 0) return {};
        const int left = (std::min)(width - 1, MulDiv(selection.left, width, kCropExtent));
        const int top = (std::min)(height - 1, MulDiv(selection.top, height, kCropExtent));
        return RECT{left, top,
            (std::clamp)(MulDiv(selection.right, width, kCropExtent), left + 1, width),
            (std::clamp)(MulDiv(selection.bottom, height, kCropExtent), top + 1, height)};
    }

    RECT SourceViewPixels() const {
        return CropPixels(cropEditing_ ? kFullCrop : cropSelection_,
            static_cast<int>(sourceWidth_), static_cast<int>(sourceHeight_));
    }

    RECT CropDraftPixels() const {
        return CropPixels(cropDraft_, sourceWidth_, sourceHeight_);
    }

    void SetCropDraftPixels(const RECT& pixels) {
        cropDraft_ = RECT{MulDiv(pixels.left, kCropExtent, sourceWidth_),
            MulDiv(pixels.top, kCropExtent, sourceHeight_),
            MulDiv(pixels.right, kCropExtent, sourceWidth_),
            MulDiv(pixels.bottom, kCropExtent, sourceHeight_)};
        UpdateCropEdits();
        resetFrameSchedule_ = true;
    }

    RECT CropAspectBounds() const {
        return RECT{0, 0, cropAspectSize_.cx, cropAspectSize_.cy};
    }

    void LoadFeatherEdges() {
        const std::wstring value = ReadConfigString(L"crop", L"feather_edges");
        const wchar_t* cursor = value.c_str();
        for (size_t i = 0; i < cropFeather_.size(); ++i) {
            wchar_t* end = nullptr;
            const LONG stored = std::wcstol(cursor, &end, 10);
            const bool last = i + 1 == cropFeather_.size();
            if (end == cursor || *end != (last ? L'\0' : L',')) {
                const int legacy = (std::clamp)(ReadConfigInt(L"crop", L"feather_px", 0),
                    0, kMaximumFeatherPixels);
                cropFeather_.fill(FeatherEdge{legacy, legacy > 0});
                return;
            }
            const LONG limit = kMaximumFeatherPixels;
            const int pixels = static_cast<int>(std::abs((std::clamp)(stored, -limit, limit)));
            // "+0" marks an enabled edge whose feather width is still zero.
            cropFeather_[i] = FeatherEdge{pixels, stored > 0 || (stored == 0 && *cursor == L'+')};
            if (!last) cursor = end + 1;
        }
    }

    void SaveFeatherEdges() const {
        std::wstring stored;
        for (const FeatherEdge& edge : cropFeather_) {
            if (!stored.empty()) stored += L',';
            if (edge.enabled && edge.pixels == 0) stored += L'+';
            stored += std::to_wstring(edge.enabled ? edge.pixels : -edge.pixels);
        }
        WriteConfigString(L"crop", L"feather_edges", stored);
    }

    static SIZE BoundedCropSize(const RECT& source, const RECT& work, double requestedScale) {
        const LONG width = source.right - source.left;
        const LONG height = source.bottom - source.top;
        const double scale = (std::min)({requestedScale,
            (work.right - work.left) / static_cast<double>(width),
            (work.bottom - work.top) / static_cast<double>(height)});
        return SIZE{(std::max)(1L, std::lround(width * scale)),
            (std::max)(1L, std::lround(height * scale))};
    }

    void FitCropViewport(const RECT& requested, const RECT& source, HMONITOR monitor = nullptr) {
        if (IsRectEmpty(&source)) return;
        MONITORINFO info{sizeof(MONITORINFO)};
        if (!monitor) monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        if (!GetMonitorInfoW(monitor, &info)) return;
        RECT work = info.rcWork;
        if (IsRectEmpty(&work)) return;
        const double scale = (std::max)(1L, requested.right - requested.left) /
            static_cast<double>(source.right - source.left);
        if (cropEditing_) {
            const SIZE preview = BoundedCropSize(source, work, scale);
            const int width = (std::min)(static_cast<int>(work.right - work.left),
                (std::max)(static_cast<int>(preview.cx), UiScaled(680)));
            const int toolbarHeight = BuildCropToolbarLayout(width).rows * UiScaled(kToolbarHeight) + ViewSettingsWarningHeight();
            work.top += (std::min)(toolbarHeight + UiScaled(kToolbarGap),
                static_cast<int>(work.bottom - work.top - 1));
            const int firstRow = UiScaled(kCropReadoutRowHeight);
            work.bottom -= (std::min)(UiScaled(kCropReadoutGap) + firstRow,
                static_cast<int>(work.bottom - work.top - 1));
            const SIZE fitted = BoundedCropSize(source, work, scale);
            const auto readout = BuildCropReadoutLayout(fitted.cx, work.right - work.left);
            work.bottom -= (std::min)(static_cast<int>(readout.size.cy) - firstRow,
                static_cast<int>(work.bottom - work.top - 1));
        }
        const SIZE size = BoundedCropSize(source, work, scale);
        const int x = (std::clamp)(requested.left, work.left, work.right - size.cx);
        const int y = (std::clamp)(requested.top, work.top, work.bottom - size.cy);
        SetWindowPos(hwnd_, WindowZOrder(), x, y, size.cx, size.cy, SWP_NOACTIVATE);
    }

    void BeginViewEdit() {
        if (viewEditing_ || locked_ || !CropAvailable()) return;
        viewEditing_ = true;
        CancelFpsCapture();
        CancelHotkeyCapture();
        HideGithubDropdown();
        toolbarHovered_ = ToolbarButton::None;
        PositionToolbar();
    }

    void EndViewEdit() {
        if (!viewEditing_) return;
        viewEditing_ = false;
        CloseShadowPanel(false);
        CloseImagePanel(false);
        if (!cropEditing_) CloseFeatherPanel();
        CloseUiScalePicker(uiScalePickerHwnd_);
        if (cropEditing_) EndCropEdit(false);
        toolbarHovered_ = ToolbarButton::None;
        PositionToolbar();
    }

    void BeginCropEdit() {
        if (!viewEditing_ || cropEditing_ || locked_ || !CropAvailable()) return;
        CloseShadowPanel(false);
        CloseImagePanel(false);
        StopImageBackdrop();
        CloseFeatherPanel();
        if (!GetWindowRect(hwnd_, &cropBeforeEditBounds_)) return;
        if (!CreateCropEdits()) return;
        CancelFpsCapture();
        CancelHotkeyCapture();
        CancelHoverExpression();
        if (toolbarGuideHwnd_) FinishToolbarGuide(false);
        HideGithubDropdown();
        cropDraft_ = cropSelection_;
        cropBeforeEditSelection_ = cropSelection_;
        cropNumericSaved_ = false;
        cropFeatherDraft_ = cropFeather_;
        imageTransformDraft_ = imageTransform_;
        const RECT previous = SourceViewPixels();
        cropAspectSize_ = SIZE{previous.right - previous.left, previous.bottom - previous.top};
        RECT preview = cropBeforeEditBounds_;
        const RECT previousBounds = DisplayImageBounds();
        const int oldWidth = MulDiv(preview.right - preview.left, previous.right - previous.left, previousBounds.right);
        const int oldHeight = MulDiv(preview.bottom - preview.top, previous.bottom - previous.top, previousBounds.bottom);
        const int previousWidth = previous.right - previous.left;
        const int previousHeight = previous.bottom - previous.top;
        preview.left -= MulDiv(previous.left, oldWidth, previousWidth);
        preview.top -= MulDiv(previous.top, oldHeight, previousHeight);
        preview.right = preview.left + (std::max)(kMinimumWidth,
            MulDiv(oldWidth, static_cast<int>(sourceWidth_), previousWidth));
        cropEditing_ = true;
        UpdateCropEdits();
        if (borderPanelHwnd_) ShowWindow(borderPanelHwnd_, SW_HIDE);
        scaleReadbackPrimed_ = false;
        toolbarHovered_ = ToolbarButton::None;
        imagePreviewBounds_ = preview;
        RefreshCropTransform();
        PositionToolbar();
        SetFocus(hwnd_);
        resetFrameSchedule_ = true;
    }

    void SaveCropSelection() const {
        const auto stored = std::to_wstring(cropSelection_.left) + L"," +
            std::to_wstring(cropSelection_.top) + L"," +
            std::to_wstring(cropSelection_.right) + L"," + std::to_wstring(cropSelection_.bottom);
        WriteConfigString(L"crop", L"rect", stored);
    }

    void EndCropEdit(bool save) {
        if (!cropEditing_) return;
        CloseUiScalePicker(uiScalePickerHwnd_);
        if (save) {
            const HWND focused = GetFocus();
            if (focused == cropWidthHwnd_ || focused == cropHeightHwnd_) CommitCropEdit(focused);
        }
        CloseFeatherPanel();
        RECT preview{};
        GetWindowRect(hwnd_, &preview);
        if (!save) {
            if (cropNumericSaved_) cropDraft_ = cropSelection_;
            imageTransformDraft_ = imageTransform_;
        }
        const auto selection = CropDraftPixels();
        const RECT fullSource = DisplayImageRegion();
        const auto mapping = ImageMappingFor(fullSource, imageTransformDraft_);
        cropEditing_ = false;
        cropInputPressed_ = nullptr;
        cropInputDragging_ = false;
        DestroyCropEdits();
        cropDrag_ = 0;
        if (GetCapture() == hwnd_) ReleaseCapture();
        scaleReadbackPrimed_ = false;
        toolbarHovered_ = ToolbarButton::None;
        if (save && cropFeather_ != cropFeatherDraft_) {
            cropFeather_ = cropFeatherDraft_;
            SaveFeatherEdges();
        }
        const bool transformChanged = !(imageTransformDraft_ == imageTransform_);
        if (save && transformChanged) {
            imageTransform_ = imageTransformDraft_;
            WriteConfigInt(L"image", L"rotation_tenths", static_cast<int>(std::lround(imageTransform_.degrees * 10)));
            WriteConfigInt(L"image", L"flip_horizontal", imageTransform_.flipX);
            WriteConfigInt(L"image", L"flip_vertical", imageTransform_.flipY);
        }
        if (((save && transformChanged) || ((save || cropNumericSaved_) &&
                !EqualRect(&cropDraft_, &cropBeforeEditSelection_))) &&
            sourceWidth_ && sourceHeight_ && !IsRectEmpty(&preview)) {
            cropSelection_ = cropDraft_;
            SaveCropSelection();
            const auto corners = CropScreenCorners(selection, mapping,
                preview.right - preview.left, preview.bottom - preview.top);
            RECT screenSelection{corners[0].x, corners[0].y, corners[0].x, corners[0].y};
            for (const POINT point : corners) {
                screenSelection.left = (std::min)(screenSelection.left, point.x);
                screenSelection.top = (std::min)(screenSelection.top, point.y);
                screenSelection.right = (std::max)(screenSelection.right, point.x);
                screenSelection.bottom = (std::max)(screenSelection.bottom, point.y);
            }
            OffsetRect(&screenSelection, preview.left, preview.top);
            if (!CropActive() && !imageTransform_.active()) {
                screenSelection.right = (std::max)(screenSelection.right, screenSelection.left + kMinimumWidth);
            }
            FitCropViewport(screenSelection, DisplayImageBounds());
            SaveWindowPlacement();
        } else {
            const RECT bounds = cropBeforeEditBounds_;
            SetWindowPos(hwnd_, WindowZOrder(), bounds.left, bounds.top,
                bounds.right - bounds.left, bounds.bottom - bounds.top, SWP_NOACTIVATE);
        }
        PositionToolbar();
        if (borderPanelHwnd_) ShowWindow(borderPanelHwnd_, SW_SHOWNA);
        resetFrameSchedule_ = true;
    }

    #include "image_panel.inl"
    #include "shadow_panel.inl"

    void ToggleCropAspect() {
        cropAspectLocked_ = !cropAspectLocked_;
        const RECT draft = CropDraftPixels();
        cropAspectSize_ = SIZE{draft.right - draft.left, draft.bottom - draft.top};
        WriteConfigInt(L"crop", L"lock_aspect", cropAspectLocked_);
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }

    static LRESULT CALLBACK CropEditProc(HWND edit, UINT message, WPARAM wParam, LPARAM lParam,
                                         UINT_PTR, DWORD_PTR data) {
        auto* overlay = reinterpret_cast<LayeredOverlay*>(data);
        if (message == WM_SETCURSOR) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;
        }
        const HWND parent = GetParent(edit);
        if (parent && parent == overlay->featherPanelHwnd_) {
            if (const auto handled = overlay->HandleFeatherInputMouse(edit, message, wParam, lParam)) {
                return *handled;
            }
        } else if (const auto handled = overlay->HandleCropInputMouse(edit, message, wParam, lParam)) {
            return *handled;
        }
        if (message == WM_KEYDOWN) {
            switch (wParam) {
            case VK_ESCAPE:
                if (parent == overlay->featherPanelHwnd_) overlay->CloseFeatherPanel();
                else overlay->EndCropEdit(false);
                return 0;
            case VK_RETURN:
                SetFocus(parent);
                return 0;
            case VK_TAB: {
                if (const HWND next = GetNextDlgTabItem(parent, edit, GetKeyState(VK_SHIFT) < 0)) {
                    SetFocus(next);
                }
                return 0;
            }
            case 'A':
                if (GetKeyState(VK_CONTROL) < 0) {
                    SendMessageW(edit, EM_SETSEL, 0, -1);
                    return 0;
                }
                break;
            }
        }
        if (message == WM_SETFOCUS) {
            const LRESULT result = DefSubclassProc(edit, message, wParam, lParam);
            SendMessageW(edit, EM_SETSEL, 0, -1);
            return result;
        }
        if (message == WM_CHAR &&
            (wParam == VK_RETURN || wParam == VK_TAB || wParam == VK_ESCAPE || wParam == 1)) {
            return 0;
        }
        return DefSubclassProc(edit, message, wParam, lParam);
    }

    HWND CreateCropNumericEdit(HWND parent, UINT id, const wchar_t* cue, HFONT font) {
        const HWND edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_BORDER | WS_TABSTOP |
            ES_CENTER | ES_NUMBER | ES_AUTOHSCROLL,
            0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), instance_, nullptr);
        if (!edit) return nullptr;
        if (!SetWindowSubclass(edit, &LayeredOverlay::CropEditProc, id, reinterpret_cast<DWORD_PTR>(this))) {
            DestroyWindow(edit);
            return nullptr;
        }
        if (font) SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
        SendMessageW(edit, EM_LIMITTEXT, 6, 0);
        SendMessageW(edit, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(cue));
        RegisterNumericHint(edit);
        return edit;
    }

    HFONT CreateCropEditFont(int size = 15) const {
        return CreateFontW(-UiScaled(size), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
    }

    bool CreateCropEdits() {
        WNDCLASSW windowClass{};
        windowClass.hInstance = instance_;
        windowClass.lpfnWndProc = &LayeredOverlay::CropReadoutProc;
        windowClass.lpszClassName = kCropReadoutClass;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

        cropReadoutHwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_LAYERED | (desktopMode_ ? 0 : WS_EX_TOPMOST),
            kCropReadoutClass, L"", WS_POPUP | WS_CLIPCHILDREN, 0, 0, 1, 1,
            hwnd_, nullptr, instance_, this);
        if (!cropReadoutHwnd_) return false;
        SetLayeredWindowAttributes(cropReadoutHwnd_, 0, 234, LWA_ALPHA);
        cropReadoutFont_ = CreateCropEditFont(kCropReadoutFontHeight);
        cropWidthHwnd_ = CreateCropNumericEdit(cropReadoutHwnd_, kCropWidthEdit, Tr(L"宽度"), cropReadoutFont_);
        cropHeightHwnd_ = CreateCropNumericEdit(cropReadoutHwnd_, kCropHeightEdit, Tr(L"高度"), cropReadoutFont_);
        if (cropWidthHwnd_ && cropHeightHwnd_) return true;
        DestroyCropEdits();
        Log("[crop] could not create resolution readout fields");
        return false;
    }

    void DestroyCropEdits() {
        cropWidthHwnd_ = nullptr;
        cropHeightHwnd_ = nullptr;
        if (const HWND readout = std::exchange(cropReadoutHwnd_, nullptr); IsWindow(readout)) DestroyWindow(readout);
        if (const HFONT font = std::exchange(cropReadoutFont_, nullptr)) DeleteObject(font);
    }

    static LRESULT CALLBACK CropReadoutProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* overlay = reinterpret_cast<LayeredOverlay*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            overlay = static_cast<LayeredOverlay*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
        }
        if (!overlay) return DefWindowProcW(window, message, wParam, lParam);
        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            const HDC dc = BeginPaint(window, &paint);
            RECT client{};
            GetClientRect(window, &client);
            PanelFill(dc, client, RGB(18, 43, 32));
            const RECT topEdge{0, 0, client.right, 1};
            PanelFill(dc, topEdge, RGB(52, 105, 81));
            const auto& layout = overlay->cropReadoutLayout_;
            const auto text = [&](const std::wstring& value, const RECT& bounds) {
                PanelText(dc, value, bounds, RGB(181, 227, 200), overlay->UiScaled(kCropReadoutFontHeight), true,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            };
            text(overlay->CropSourceLabel(), layout.source);
            text(Tr(L"裁剪分辨率"), layout.label);
            text(L"×", layout.separator);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_COMMAND:
            if (overlay->cropEditing_ && (LOWORD(wParam) == kCropWidthEdit || LOWORD(wParam) == kCropHeightEdit)) {
                if (HIWORD(wParam) == EN_KILLFOCUS) overlay->CommitCropEdit(reinterpret_cast<HWND>(lParam));
                return 0;
            }
            break;
        case WM_CTLCOLOREDIT: {
            const HDC dc = reinterpret_cast<HDC>(wParam);
            constexpr COLORREF background = RGB(28, 62, 47);
            SetTextColor(dc, RGB(226, 248, 233));
            SetBkColor(dc, background);
            SetDCBrushColor(dc, background);
            return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                overlay->EndCropEdit(false);
                return 0;
            }
            break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    CropReadoutLayout BuildCropReadoutLayout(int outputWidth, int workWidth) const {
        CropReadoutLayout layout{};
        const int padding = UiScaled(8);
        const int gap = UiScaled(6);
        const int rowHeight = UiScaled(kCropReadoutRowHeight);
        const HDC dc = GetDC(cropReadoutHwnd_);
        const HGDIOBJ oldFont = cropReadoutFont_ ? SelectObject(dc, cropReadoutFont_) : nullptr;
        const auto measure = [&](const std::wstring& label) {
            SIZE size{};
            GetTextExtentPoint32W(dc, label.c_str(), static_cast<int>(label.size()), &size);
            return static_cast<int>(size.cx) + UiScaled(2);
        };
        const int sourceWidth = measure(CropSourceLabel());
        const int cropLabelWidth = measure(Tr(L"裁剪分辨率"));
        const int fieldWidth = (std::max)(UiScaled(44), measure(L"00000") + gap);
        const int separatorWidth = measure(L"×");
        if (oldFont) SelectObject(dc, oldFont);
        ReleaseDC(cropReadoutHwnd_, dc);

        const int dimensionsWidth = fieldWidth * 2 + separatorWidth + gap * 2;
        const int cropWidth = cropLabelWidth + gap + dimensionsWidth;
        const int singleWidth = padding * 2 + sourceWidth + gap * 2 + cropWidth;
        const bool singleRow = singleWidth <= (std::min)(outputWidth, workWidth);
        const int width = singleRow ? singleWidth :
            (std::min)(workWidth, padding * 2 + (std::max)(sourceWidth, cropWidth));
        layout.size = SIZE{width, singleRow ? rowHeight : rowHeight * 2};
        layout.source = RECT{padding, 0, (std::min)(padding + sourceWidth, width - padding), rowHeight};
        const int top = singleRow ? 0 : rowHeight;
        const int labelLeft = singleRow ? layout.source.right + gap * 2 : padding;
        const int fieldLeft = width - padding - dimensionsWidth;
        layout.label = RECT{labelLeft, top, fieldLeft - gap, top + rowHeight};
        const int fieldHeight = UiScaled(18);
        const int fieldTop = top + (rowHeight - fieldHeight) / 2;
        layout.widthEdit = RECT{fieldLeft, fieldTop, fieldLeft + fieldWidth, fieldTop + fieldHeight};
        const int separatorLeft = layout.widthEdit.right + gap;
        layout.separator = RECT{separatorLeft, top, separatorLeft + separatorWidth, top + rowHeight};
        const int heightLeft = layout.separator.right + gap;
        layout.heightEdit = RECT{heightLeft, fieldTop, heightLeft + fieldWidth, fieldTop + fieldHeight};
        return layout;
    }

    void PositionCropReadout(const RECT& output, const RECT& work) {
        if (!cropReadoutHwnd_ || !overlayVisible_ || IsIconic(hwnd_)) return;
        cropReadoutLayout_ = BuildCropReadoutLayout(output.right - output.left, work.right - work.left);
        const auto& layout = cropReadoutLayout_;
        const LONG x = (std::clamp)((output.left + output.right - layout.size.cx) / 2,
            work.left, work.right - layout.size.cx);
        const LONG y = output.bottom + UiScaled(kCropReadoutGap);
        SetWindowPos(cropReadoutHwnd_, nullptr, x, y, layout.size.cx, layout.size.cy,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        const auto place = [](HWND edit, const RECT& bounds) {
            SetWindowPos(edit, nullptr, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        };
        place(cropWidthHwnd_, layout.widthEdit);
        place(cropHeightHwnd_, layout.heightEdit);
        InvalidateRect(cropReadoutHwnd_, nullptr, FALSE);
    }

    static void SetCropNumericEdit(HWND edit, LONG pixels) {
        if (!edit) return;
        wchar_t current[16]{};
        GetWindowTextW(edit, current, ARRAYSIZE(current));
        const std::wstring text = std::to_wstring(pixels);
        if (text != current) SetWindowTextW(edit, text.c_str());
    }

    static std::optional<LONG> ReadCropNumericEdit(HWND edit) {
        wchar_t text[16]{};
        if (!GetWindowTextW(edit, text, ARRAYSIZE(text))) return std::nullopt;
        wchar_t* end = nullptr;
        const LONG value = std::wcstol(text, &end, 10);
        if (end == text || *end) return std::nullopt;
        return value;
    }

    void UpdateCropEdits() const {
        const RECT crop = CropDraftPixels();
        SetCropNumericEdit(cropWidthHwnd_, crop.right - crop.left);
        SetCropNumericEdit(cropHeightHwnd_, crop.bottom - crop.top);
    }

    void CommitCropEdit(HWND edit) {
        if (!cropEditing_) return;
        if (const auto value = ReadCropNumericEdit(edit)) SetCropDimension(edit == cropWidthHwnd_, *value);
        else UpdateCropEdits();
    }

    void SetCropDimension(bool width, LONG value) {
        const RECT canvas{0, 0, static_cast<LONG>(sourceWidth_), static_cast<LONG>(sourceHeight_)};
        const RECT previous = CropDraftPixels();
        SIZE size{previous.right - previous.left, previous.bottom - previous.top};
        LONG& dimension = width ? size.cx : size.cy;
        const LONG requested = (std::clamp)(value, 1L, width ? canvas.right : canvas.bottom);
        if (requested == dimension) {
            UpdateCropEdits();
            return;
        }
        if (cropAspectLocked_) {
            const LONG reference = width ? cropAspectSize_.cx : cropAspectSize_.cy;
            size = BoundedCropSize(CropAspectBounds(), canvas, requested / static_cast<double>(reference));
        } else {
            dimension = requested;
        }
        const LONG x = (std::min)(previous.left, canvas.right - size.cx);
        const LONG y = (std::min)(previous.top, canvas.bottom - size.cy);
        SetCropDraftPixels(RECT{x, y, x + size.cx, y + size.cy});
    }

    std::optional<LRESULT> HandleCropInputMouse(HWND edit, UINT message, WPARAM wParam, LPARAM lParam) {
        const bool angle = edit == cropAngleHwnd_;
        const bool width = edit == cropWidthHwnd_;
        switch (message) {
        case WM_LBUTTONDOWN: {
            SetFocus(edit);
            SendMessageW(edit, EM_SETSEL, 0, -1);
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(edit, &point);
            const RECT crop = CropDraftPixels();
            cropInputBase_ = angle ? imageTransformDraft_.degrees :
                width ? crop.right - crop.left : crop.bottom - crop.top;
            if (angle) {
                if (const auto input = ReadDecimalField(edit)) cropInputBase_ = (std::clamp)(*input, -180.0, 180.0);
            } else if (const auto input = ReadCropNumericEdit(edit)) {
                cropInputBase_ = (std::clamp)(*input, 1L, static_cast<LONG>(width ? sourceWidth_ : sourceHeight_));
            }
            cropInputStartX_ = point.x;
            cropInputPressed_ = edit;
            cropInputDragging_ = false;
            if (GetCapture() != edit) SetCapture(edit);
            return 0;
        }
        case WM_MOUSEMOVE:
        case WM_LBUTTONUP: {
            if (cropInputPressed_ != edit) break;
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(edit, &point);
            const LONG delta = point.x - cropInputStartX_;
            if (!cropInputDragging_ && std::abs(delta) >= UiScaled(4)) {
                cropInputDragging_ = true;
                SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            }
            if (!cropInputDragging_ && message == WM_MOUSEMOVE) break;
            if (cropInputDragging_) {
                if (angle) {
                    const double degrees = (std::clamp)(cropInputBase_ + MulDiv(delta, 100, uiScalePercent_), -180.0, 180.0);
                    if (std::exchange(imageTransformDraft_.degrees, degrees) != degrees) {
                        UpdateCropAngleField();
                        RefreshCropTransform();
                    }
                } else {
                    SetCropDimension(width, static_cast<LONG>(cropInputBase_) + MulDiv(delta, 100, uiScalePercent_));
                }
                if (message == WM_MOUSEMOVE) return 0;
                SendMessageW(edit, EM_SETSEL, 0, -1);
                SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                CommitNumericDrag(edit);
            }
            cropInputPressed_ = nullptr;
            cropInputDragging_ = false;
            const LRESULT result = DefSubclassProc(edit, message, wParam, lParam);
            if (GetCapture() == edit) ReleaseCapture();
            return result;
        }
        case WM_CAPTURECHANGED:
        case WM_CANCELMODE:
        case WM_KILLFOCUS:
        case WM_NCDESTROY:
            if (cropInputPressed_ == edit) {
                cropInputPressed_ = nullptr;
                cropInputDragging_ = false;
            }
            break;
        }
        return std::nullopt;
    }

    FeatherPanelLayout FeatherPanelGeometry(HWND panel) const {
        RECT client{};
        GetClientRect(panel, &client);
        const auto x = [&](int n) { return MulDiv(n, client.right, kFeatherPanelWidth); };
        const auto y = [&](int n) { return MulDiv(n, client.bottom, kFeatherPanelHeight); };
        const auto rect = [&](int l, int t, int r, int b) {
            return RECT{x(l), y(t), x(r), y(b)};
        };
        FeatherPanelLayout layout{};
        layout.header = rect(0, 0, 440, 40);
        layout.close = rect(394, 2, 438, 38);
        layout.hint = rect(16, 44, 424, 74);
        layout.canvas = rect(105, 115, 335, 236);
        layout.guide = rect(15, 414, 425, 453);
        layout.rotationTitle = rect(16, 283, 424, 308);
        layout.angleLabel = rect(16, 352, 82, 382);
        layout.angle = rect(84, 354, 148, 379);
        layout.angleUnit = rect(153, 352, 181, 382);
        layout.buttons = {rect(14, 312, 146, 343), rect(154, 312, 286, 343),
            rect(294, 312, 426, 343), rect(187, 351, 303, 382), rect(311, 351, 426, 382),
            rect(14, 389, 426, 413)};
        layout.edits = {rect(23, 163, 75, 189), rect(184, 82, 236, 108),
            rect(347, 163, 399, 189), rect(184, 243, 236, 269)};
        layout.units = {rect(79, 163, 103, 189), rect(241, 82, 265, 108),
            rect(403, 163, 427, 189), rect(241, 243, 265, 269)};
        constexpr POINT edges[FeatherSideCount][4] = {
            {{105, 119}, {123, 137}, {123, 214}, {105, 232}},
            {{109, 115}, {331, 115}, {313, 133}, {127, 133}},
            {{335, 119}, {335, 232}, {317, 214}, {317, 137}},
            {{109, 236}, {127, 218}, {313, 218}, {331, 236}},
        };
        for (size_t i = 0; i < layout.edges.size(); ++i) {
            for (size_t j = 0; j < layout.edges[i].size(); ++j) {
                layout.edges[i][j] = POINT{x(edges[i][j].x), y(edges[i][j].y)};
            }
        }
        return layout;
    }

    static int FeatherPanelHitTest(const FeatherPanelLayout& layout, POINT point) {
        if (PtInRect(&layout.close, point)) return ViewClose;
        for (size_t i = 0; i < layout.buttons.size(); ++i) {
            if (PtInRect(&layout.buttons[i], point)) return FeatherRotateLeft + static_cast<int>(i);
        }
        if (!PtInRect(&layout.canvas, point)) return FeatherNoAction;
        for (size_t i = 0; i < layout.edges.size(); ++i) {
            const auto& polygon = layout.edges[i];
            const HRGN edge = CreatePolygonRgn(polygon.data(), static_cast<int>(polygon.size()), WINDING);
            const bool hit = edge && PtInRegion(edge, point.x, point.y);
            if (edge) DeleteObject(edge);
            if (hit) return static_cast<int>(i);
        }
        return FeatherNoAction;
    }

    static HBRUSH CropNumericBrush(HDC dc, bool muted = false) {
        constexpr COLORREF field = RGB(25, 43, 65);
        SetTextColor(dc, muted ? RGB(152, 175, 202) : RGB(240, 246, 255));
        SetBkColor(dc, field);
        SetDCBrushColor(dc, field);
        return static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
    }

    void PaintFeatherPanel(HWND panel) const {
        PAINTSTRUCT paint{};
        const HDC target = BeginPaint(panel, &paint);
        RECT client{};
        GetClientRect(panel, &client);
        const HDC dc = CreateCompatibleDC(target);
        const HBITMAP buffer = CreateCompatibleBitmap(target, client.right, client.bottom);
        if (!dc || !buffer) {
            if (buffer) DeleteObject(buffer);
            if (dc) DeleteDC(dc);
            EndPaint(panel, &paint);
            return;
        }
        const HGDIOBJ oldBitmap = SelectObject(dc, buffer);
        const FeatherPanelLayout layout = FeatherPanelGeometry(panel);
        const auto px = [&](int n) { return ViewPanelPx(client.right, n); };
        PanelFill(dc, client, RGB(16, 25, 39));
        PanelFill(dc, layout.header, RGB(27, 45, 68));
        const HBRUSH frame = CreateSolidBrush(RGB(44, 72, 104));
        FrameRect(dc, &client, frame);
        DeleteObject(frame);
        RECT title = layout.header;
        title.left += px(13);
        title.right = layout.close.left;
        PanelText(dc, Tr(L"羽化与旋转"), title, RGB(241, 246, 255), px(18), true,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        const auto button = [&](int action, RECT bounds, const std::wstring& label, COLORREF color) {
            if (featherPanelHover_ == action) color = RGB(36, 83, 130);
            if (featherPanelPressed_ == action) color = RGB(20, 52, 87);
            PanelFill(dc, bounds, color);
            PanelText(dc, label, bounds, RGB(240, 246, 255), px(15), true,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        };
        button(ViewClose, layout.close, L"×", RGB(27, 45, 68));
        PaintCropSettings(dc, layout, client.right, button);
        BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(buffer);
        DeleteDC(dc);
        EndPaint(panel, &paint);
    }

    template <typename DrawButton>
    void PaintCropSettings(HDC dc, const FeatherPanelLayout& layout, int width, DrawButton button) const {
        const auto px = [&](int n) { return ViewPanelPx(width, n); };
        constexpr UINT line = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
        PanelText(dc, Tr(L"边缘羽化"), layout.hint, RGB(223, 238, 252), px(14), true, DT_LEFT | line);
        RECT hint = layout.hint;
        hint.left += MulDiv(width, 148, kFeatherPanelWidth);
        PanelText(dc, Tr(L"点击边缘开关羽化"), hint, RGB(170, 194, 221), px(12), false, DT_RIGHT | line);

        PanelFill(dc, layout.canvas, RGB(13, 21, 34));
        for (size_t i = 0; i < layout.edges.size(); ++i) {
            const auto& polygon = layout.edges[i];
            const bool active = cropFeatherDraft_[i].enabled;
            const bool hovered = featherPanelHover_ == static_cast<int>(i);
            const HRGN shape = CreatePolygonRgn(polygon.data(), static_cast<int>(polygon.size()), WINDING);
            if (shape) {
                RECT bounds{};
                GetRgnBox(shape, &bounds);
                SelectClipRgn(dc, shape);
                if (active) {
                    const bool vertical = i == FeatherLeft || i == FeatherRight;
                    const bool reverse = i == FeatherRight || i == FeatherBottom;
                    const LONG start = vertical ? bounds.left : bounds.top;
                    const LONG length = vertical ? bounds.right - bounds.left : bounds.bottom - bounds.top;
                    constexpr int steps = 12;
                    for (int step = 0; step < steps; ++step) {
                        const int intensity = reverse ? steps - 1 - step : step;
                        const LONG a = start + MulDiv(length, step, steps);
                        const LONG b = start + MulDiv(length, step + 1, steps);
                        const RECT band = vertical ? RECT{a, bounds.top, b, bounds.bottom}
                            : RECT{bounds.left, a, bounds.right, b};
                        PanelFill(dc, band, RGB(25 + 2 * intensity, 62 + 7 * intensity,
                            106 + 11 * intensity));
                    }
                } else {
                    PanelFill(dc, bounds, hovered ? RGB(36, 61, 89) : RGB(22, 38, 59));
                }
                SelectClipRgn(dc, nullptr);
                DeleteObject(shape);
            }
            const COLORREF outline = hovered ? RGB(149, 206, 255) :
                active ? RGB(98, 175, 249) : RGB(65, 94, 129);
            const HPEN pen = CreatePen(PS_SOLID, 1, outline);
            const HGDIOBJ oldPen = SelectObject(dc, pen);
            const HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Polygon(dc, polygon.data(), static_cast<int>(polygon.size()));
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(pen);
            PanelText(dc, L"px", layout.units[i], active ? RGB(198, 225, 253) : RGB(152, 175, 202),
                px(13), false, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        RECT divider = layout.rotationTitle;
        divider.bottom = divider.top;
        --divider.top;
        PanelFill(dc, divider, RGB(44, 72, 104));
        PanelText(dc, Tr(L"旋转与翻转"), layout.rotationTitle, RGB(223, 238, 252), px(14), true, DT_LEFT | line);
        PanelText(dc, Tr(L"角度"), layout.angleLabel, RGB(223, 238, 252), px(14), false, DT_LEFT | line);
        PanelText(dc, L"°", layout.angleUnit, RGB(223, 238, 252), px(14), false, DT_LEFT | line);
        const wchar_t* labels[]{L"↶ 90°", L"↷ 90°", L"180°", Tr(L"水平翻转"),
            Tr(L"垂直翻转"), Tr(L"重置旋转与翻转")};
        for (int action = FeatherRotateLeft; action < FeatherActionCount; ++action) {
            const bool active = (action == FeatherFlipX && imageTransformDraft_.flipX) ||
                (action == FeatherFlipY && imageTransformDraft_.flipY);
            const int index = action - FeatherRotateLeft;
            button(action, layout.buttons[index], labels[index], active ? RGB(31, 85, 141) : RGB(24, 42, 64));
        }
        PanelText(dc, Tr(L"按住输入框左右拖动可调整数值。"),
            layout.guide, RGB(170, 194, 221), px(13), false, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
    }

    void CommitFeatherEdit(size_t side) {
        FeatherEdge& edge = cropFeatherDraft_[side];
        const HWND edit = featherPanelEdits_[side];
        if (const auto value = ReadCropNumericEdit(edit)) {
            edge.pixels = (std::clamp)(*value, 0L, static_cast<LONG>(kMaximumFeatherPixels));
        }
        SetCropNumericEdit(edit, edge.pixels);
        InvalidateRect(featherPanelHwnd_, nullptr, FALSE);
        InvalidateRect(edit, nullptr, TRUE);
        if (featherScrubHwnd_) InvalidateRect(featherScrubHwnd_, nullptr, FALSE);
        resetFrameSchedule_ = true;
    }

    static const wchar_t* FeatherSideLabel(size_t side) {
        const wchar_t* labels[] = {Tr(L"左边缘"), Tr(L"上边缘"), Tr(L"右边缘"), Tr(L"下边缘")};
        return labels[side];
    }

    void PositionFeatherScrub(HWND edit) const {
        RECT panel{}, field{};
        if (!GetWindowRect(featherPanelHwnd_, &panel) || !GetWindowRect(edit, &field)) return;
        MONITORINFO info{sizeof(MONITORINFO)};
        if (!GetMonitorInfoW(MonitorFromWindow(featherPanelHwnd_, MONITOR_DEFAULTTONEAREST), &info)) return;
        const RECT work = info.rcWork;
        const SIZE size{(std::min)(static_cast<LONG>(UiScaled(kFeatherScrubWidth)), work.right - work.left),
            (std::min)(static_cast<LONG>(UiScaled(kFeatherScrubHeight)), work.bottom - work.top)};
        if (size.cx <= 0 || size.cy <= 0) return;
        const POINT position = SidePanelPosition(panel, work, nullptr, size, UiScaled(8),
            (field.top + field.bottom - size.cy) / 2);
        SetWindowPos(featherScrubHwnd_, WindowZOrder(), position.x, position.y, size.cx, size.cy,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    void ShowFeatherScrub(HWND edit, size_t side) {
        if (!featherScrubHwnd_) {
            WNDCLASSW windowClass{};
            windowClass.lpfnWndProc = &LayeredOverlay::FeatherScrubProc;
            windowClass.hInstance = instance_;
            windowClass.lpszClassName = kFeatherScrubClass;
            if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
            featherScrubHwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                kFeatherScrubClass, L"", WS_POPUP, 0, 0, 0, 0,
                featherPanelHwnd_, nullptr, instance_, this);
            if (!featherScrubHwnd_) return;
        }
        if (featherScrubSide_ == static_cast<int>(side) && IsWindowVisible(featherScrubHwnd_)) return;
        featherScrubSide_ = static_cast<int>(side);
        PositionFeatherScrub(edit);
        InvalidateRect(featherScrubHwnd_, nullptr, FALSE);
    }

    void StopFeatherPress() {
        featherPressedEdit_ = nullptr;
        featherDragging_ = false;
        if (std::exchange(featherScrubSide_, FeatherNoAction) != FeatherNoAction && featherScrubHwnd_) {
            ShowWindow(featherScrubHwnd_, SW_HIDE);
        }
    }

    void UpdateFeatherScrub(HWND edit, size_t side, LONG cursorX) {
        const int steps = (cursorX - featherPressX_) / UiScaled(kFeatherScrubDragStep);
        FeatherEdge value = featherDragBase_;
        if (steps != 0) {
            // Manual entries can be between ticks. Move to the next tick in the drag direction.
            const int startTick = (value.pixels + (steps < 0 ? kFeatherScrubStep - 1 : 0)) / kFeatherScrubStep;
            value.pixels = (std::clamp)((startTick + steps) * kFeatherScrubStep, 0, kMaximumFeatherPixels);
        }
        if (value == cropFeatherDraft_[side]) return;
        cropFeatherDraft_[side] = value;
        SetCropNumericEdit(edit, value.pixels);
        InvalidateRect(edit, nullptr, TRUE);
        InvalidateRect(featherPanelHwnd_, nullptr, FALSE);
        if (featherScrubHwnd_) InvalidateRect(featherScrubHwnd_, nullptr, FALSE);
        resetFrameSchedule_ = true;
    }

    std::optional<LRESULT> HandleFeatherInputMouse(HWND edit, UINT message, WPARAM wParam, LPARAM lParam) {
        const size_t side = static_cast<UINT>(GetDlgCtrlID(edit)) - kFirstFeatherEdit;
        if (side >= featherPanelEdits_.size()) return std::nullopt;
        switch (message) {
        case WM_LBUTTONDOWN: {
            StopFeatherPress();
            SetFocus(edit);
            SendMessageW(edit, EM_SETSEL, 0, -1);
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(edit, &point);
            featherPressX_ = point.x;
            featherPressedEdit_ = edit;
            if (GetCapture() != edit) SetCapture(edit);
            return 0;
        }
        case WM_MOUSEMOVE:
        case WM_LBUTTONUP: {
            if (featherPressedEdit_ != edit) break;
            const POINT local{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            POINT screen = local;
            ClientToScreen(edit, &screen);
            // A full step of horizontal movement distinguishes a drag from a click.
            if (!featherDragging_ && std::abs(screen.x - featherPressX_) >= UiScaled(kFeatherScrubDragStep)) {
                CommitFeatherEdit(side);
                featherDragBase_ = cropFeatherDraft_[side];
                featherDragging_ = true;
                SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            }
            const bool dragging = featherDragging_;
            if (dragging) {
                UpdateFeatherScrub(edit, side, screen.x);
                if (message == WM_MOUSEMOVE) {
                    ShowFeatherScrub(edit, side);
                    return 0;
                }
            }
            if (message == WM_LBUTTONUP) {
                StopFeatherPress();
                const LRESULT result = DefSubclassProc(edit, message, wParam, lParam);
                if (GetCapture() == edit) ReleaseCapture();
                if (dragging) {
                    SendMessageW(edit, EM_SETSEL, 0, -1);
                    CommitNumericDrag(edit);
                }
                RECT client{};
                GetClientRect(edit, &client);
                if (PtInRect(&client, local)) SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                return result;
            }
            break;
        }
        case WM_CAPTURECHANGED:
        case WM_CANCELMODE:
        case WM_KILLFOCUS:
            if (featherPressedEdit_ == edit) StopFeatherPress();
            break;
        }
        return std::nullopt;
    }

    void PaintFeatherScrub(HWND window) const {
        PAINTSTRUCT paint{};
        const HDC target = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        const HDC dc = CreateCompatibleDC(target);
        const HBITMAP buffer = CreateCompatibleBitmap(target, client.right, client.bottom);
        if (!dc || !buffer) {
            if (buffer) DeleteObject(buffer);
            if (dc) DeleteDC(dc);
            EndPaint(window, &paint);
            return;
        }
        const HGDIOBJ oldBitmap = SelectObject(dc, buffer);
        PanelFill(dc, client, RGB(16, 25, 39));
        const HBRUSH frame = CreateSolidBrush(RGB(65, 119, 176));
        FrameRect(dc, &client, frame);
        DeleteObject(frame);
        if (featherScrubSide_ != FeatherNoAction) {
            const size_t side = featherScrubSide_;
            const int value = cropFeatherDraft_[side].pixels;
            const auto rect = [&](int l, int t, int r, int b) {
                return RECT{MulDiv(l, client.right, kFeatherScrubWidth),
                    MulDiv(t, client.bottom, kFeatherScrubHeight),
                    MulDiv(r, client.right, kFeatherScrubWidth),
                    MulDiv(b, client.bottom, kFeatherScrubHeight)};
            };
            constexpr UINT line = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
            constexpr COLORREF valueColor = RGB(120, 193, 255);
            PanelText(dc, FeatherSideLabel(side), rect(12, 5, 142, 29),
                RGB(170, 194, 221), UiScaled(13), false, DT_LEFT | line);
            PanelText(dc, std::to_wstring(value) + L" px", rect(142, 5, 214, 29),
                valueColor, UiScaled(16), true, DT_RIGHT | line);
            const RECT track = rect(14, 35, 212, 39);
            PanelFill(dc, track, RGB(37, 58, 83));
            const LONG sliderX = track.left + MulDiv((std::min)(value, kMaximumFeatherPixels),
                track.right - track.left, kMaximumFeatherPixels);
            PanelFill(dc, RECT{track.left, track.top, sliderX, track.bottom}, RGB(55, 133, 217));
            for (int mark = 0; mark <= 10; ++mark) {
                const int x = 14 + MulDiv(198, mark, 10);
                PanelFill(dc, rect(x, 44, x + 1, mark % 5 == 0 ? 49 : 47), RGB(91, 121, 156));
            }
            const int thumb = UiScaled(3);
            PanelFill(dc, RECT{sliderX - thumb, track.top - UiScaled(3),
                sliderX + thumb, track.bottom + UiScaled(3)}, valueColor);
            PanelText(dc, L"0 px", rect(12, 49, 70, 67),
                RGB(170, 194, 221), UiScaled(12), false, DT_LEFT | line);
            PanelText(dc, std::to_wstring(kMaximumFeatherPixels) + L" px", rect(150, 49, 214, 67),
                RGB(170, 194, 221), UiScaled(12), false, DT_RIGHT | line);
        }
        BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(buffer);
        DeleteDC(dc);
        EndPaint(window, &paint);
    }

    static LRESULT CALLBACK FeatherScrubProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* overlay = reinterpret_cast<LayeredOverlay*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        switch (message) {
        case WM_NCCREATE: {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            break;
        }
        case WM_PAINT:
            if (overlay) {
                overlay->PaintFeatherScrub(window);
                return 0;
            }
            break;
        case WM_ERASEBKGND: return 1;
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_NCHITTEST: return HTTRANSPARENT;
        case WM_NCDESTROY:
            if (overlay && overlay->featherScrubHwnd_ == window) {
                overlay->featherScrubHwnd_ = nullptr;
                overlay->featherScrubSide_ = FeatherNoAction;
            }
            break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    void UpdateCropAngleField() const {
        wchar_t text[24]{};
        swprintf_s(text, L"%.1f", imageTransformDraft_.degrees);
        SetWindowTextW(cropAngleHwnd_, text);
    }

    void ActivateCropTransform(int action) {
        switch (action) {
        case FeatherRotateLeft: case FeatherRotateRight: case FeatherRotateHalf: {
            const double delta = action == FeatherRotateLeft ? -90 : action == FeatherRotateRight ? 90 : 180;
            imageTransformDraft_.degrees = std::remainder(imageTransformDraft_.degrees + delta, 360);
            break;
        }
        case FeatherFlipX: imageTransformDraft_.flipX = !imageTransformDraft_.flipX; break;
        case FeatherFlipY: imageTransformDraft_.flipY = !imageTransformDraft_.flipY; break;
        case FeatherResetRotation: imageTransformDraft_ = {}; break;
        }
        UpdateCropAngleField();
        RefreshCropTransform();
    }

    void CloseFeatherPanel() {
        const HWND panel = featherPanelHwnd_;
        if (!panel) return;
        const HWND focused = GetFocus();
        const auto edit = std::find(featherPanelEdits_.begin(), featherPanelEdits_.end(), focused);
        if (focused && edit != featherPanelEdits_.end()) CommitFeatherEdit(edit - featherPanelEdits_.begin());
        StopFeatherPress();
        if (const HWND scrub = std::exchange(featherScrubHwnd_, nullptr)) DestroyWindow(scrub);
        featherScrubSide_ = FeatherNoAction;
        featherPanelHwnd_ = nullptr;
        cropAngleHwnd_ = nullptr;
        featherPanelEdits_.fill(nullptr);
        DestroyWindow(panel);
        if (const HFONT font = std::exchange(featherEditFont_, nullptr)) DeleteObject(font);
        if (!cropEditing_) {
            if (cropFeather_ != cropFeatherDraft_) {
                cropFeather_ = cropFeatherDraft_;
                SaveFeatherEdges();
            }
            WriteConfigInt(L"image", L"rotation_tenths", static_cast<int>(std::lround(imageTransform_.degrees * 10)));
            WriteConfigInt(L"image", L"flip_horizontal", imageTransform_.flipX);
            WriteConfigInt(L"image", L"flip_vertical", imageTransform_.flipY);
            SaveWindowPlacement();
        }
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
        resetFrameSchedule_ = true;
    }

    static int ViewPanelPx(int width, int size) {
        return (std::max)(1, MulDiv(width, size, kFeatherPanelWidth));
    }

    SIZE ViewPanelSize(const RECT& work) const {
        const int width = (std::min)({UiScaled(kFeatherPanelWidth), int(work.right - work.left),
            MulDiv(work.bottom - work.top, kFeatherPanelWidth, kFeatherPanelHeight)});
        return SIZE{(std::max)(1, width), ViewPanelPx(width, kFeatherPanelHeight)};
    }

    void UpdateFeatherFieldFont(int width) {
        const HFONT font = CreateFontW(-ViewPanelPx(width, 15), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        if (!font) return;
        for (const HWND edit : featherPanelEdits_) {
            if (edit) SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
        if (cropAngleHwnd_) SendMessageW(cropAngleHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        if (const HFONT old = std::exchange(featherEditFont_, font)) DeleteObject(old);
    }

    void ResizeViewPanel() {
        if (!featherPanelHwnd_) return;
        RECT toolbar{}, panel{};
        const HWND anchor = featherPanelDetached_ ? featherPanelHwnd_ : toolbarHwnd_;
        MONITORINFO info{sizeof(MONITORINFO)};
        if (!GetWindowRect(toolbarHwnd_, &toolbar) || !GetWindowRect(featherPanelHwnd_, &panel) ||
            !GetMonitorInfoW(MonitorFromWindow(anchor, MONITOR_DEFAULTTONEAREST), &info)) return;
        const RECT work = info.rcWork;
        const SIZE size = ViewPanelSize(work);
        const POINT position = featherPanelDetached_
            ? POINT{(std::clamp)(panel.left, work.left, work.right - size.cx),
                (std::clamp)(panel.top, work.top, work.bottom - size.cy)}
            : SidePanelPosition(toolbar, work, nullptr, size, UiScaled(8), toolbar.top);
        SetWindowPos(featherPanelHwnd_, nullptr, position.x, position.y, size.cx, size.cy, SWP_NOZORDER | SWP_NOACTIVATE);
        if (size.cx != panel.right - panel.left) UpdateFeatherFieldFont(size.cx);
        const auto layout = FeatherPanelGeometry(featherPanelHwnd_);
        const auto place = [](HWND field, const RECT& bounds) {
            if (field) MoveWindow(field, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, TRUE);
        };
        for (size_t i = 0; i < featherPanelEdits_.size(); ++i) place(featherPanelEdits_[i], layout.edits[i]);
        place(cropAngleHwnd_, layout.angle);
        InvalidateRect(featherPanelHwnd_, nullptr, FALSE);
    }

    void ApplyUiScale(int selected) {
        uiScalePercent_ = selected;
        WriteConfigInt(L"ui", L"scale_percent", uiScalePercent_);
        if (cropEditing_) {
            if (const HFONT font = CreateCropEditFont(kCropReadoutFontHeight)) {
                SendMessageW(cropWidthHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                SendMessageW(cropHeightHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                if (const HFONT previous = std::exchange(cropReadoutFont_, font)) DeleteObject(previous);
            }
            RefreshCropTransform();
        }
        personalPanelManuallyPositioned_ = false;
        statusFrameDirty_ = true;
        PositionToolbar();
        PositionBorderPanel();
        if (borderPanelHwnd_) RefreshPersonalPanel(borderPanelHwnd_);
        ResizeViewPanel();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    std::optional<POINT> DockedPanelPosition(HWND window, bool detached, const RECT& toolbar, const RECT& work) const {
        if (!window || detached) return std::nullopt;
        RECT panel{};
        if (!GetWindowRect(window, &panel)) return std::nullopt;
        const SIZE size{panel.right - panel.left, panel.bottom - panel.top};
        const POINT position = SidePanelPosition(toolbar, work, nullptr, size, UiScaled(8), toolbar.top);
        if (position.x == panel.left && position.y == panel.top) return std::nullopt;
        return position;
    }

    void ShowFeatherPanel() {
        if (featherPanelHwnd_) {
            CloseFeatherPanel();
            return;
        }
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = &LayeredOverlay::FeatherPanelProc;
        windowClass.hInstance = instance_;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.lpszClassName = kFeatherPanelClass;
        if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
        RECT toolbar{};
        if (!GetWindowRect(toolbarHwnd_, &toolbar)) return;
        MONITORINFO info{sizeof(MONITORINFO)};
        if (!GetMonitorInfoW(MonitorFromWindow(toolbarHwnd_, MONITOR_DEFAULTTONEAREST), &info)) return;
        const RECT work = info.rcWork;
        const SIZE size = ViewPanelSize(work);
        const POINT position = SidePanelPosition(toolbar, work, nullptr, size, UiScaled(8), toolbar.top);
        featherPanelHover_ = featherPanelPressed_ = FeatherNoAction;
        featherPanelDetached_ = false;
        featherPanelHwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | (desktopMode_ ? 0 : WS_EX_TOPMOST),
            kFeatherPanelClass, Tr(L"羽化与旋转"), WS_POPUP | WS_CLIPCHILDREN,
            position.x, position.y, size.cx, size.cy, toolbarHwnd_, nullptr, instance_, this);
        if (!featherPanelHwnd_) return;
        if (!cropEditing_) {
            cropFeatherDraft_ = cropFeather_;
            imageTransformDraft_ = imageTransform_;
        }
        UpdateFeatherFieldFont(size.cx);
        if (!CreateFeatherFields()) {
            CloseFeatherPanel();
            return;
        }
        ShowWindow(featherPanelHwnd_, SW_SHOW);
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }

    bool CreateFeatherFields() {
        const auto layout = FeatherPanelGeometry(featherPanelHwnd_);
        for (size_t i = 0; i < featherPanelEdits_.size(); ++i) {
            const HWND edit = CreateCropNumericEdit(featherPanelHwnd_, kFirstFeatherEdit + static_cast<UINT>(i),
                FeatherSideLabel(i), featherEditFont_);
            if (!edit) return false;
            featherPanelEdits_[i] = edit;
            SetCropNumericEdit(edit, cropFeatherDraft_[i].pixels);
            const RECT bounds = layout.edits[i];
            SetWindowPos(edit, nullptr, bounds.left, bounds.top, bounds.right - bounds.left,
                bounds.bottom - bounds.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
        const RECT angle = layout.angle;
        cropAngleHwnd_ = CreateImageField(featherPanelHwnd_, kCropAngleEdit, angle, featherEditFont_, Tr(L"角度"));
        if (!cropAngleHwnd_) return false;
        UpdateCropAngleField();
        return true;
    }

    static LRESULT CALLBACK FeatherPanelProc(HWND panel, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* overlay = reinterpret_cast<LayeredOverlay*>(GetWindowLongPtrW(panel, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            overlay = static_cast<LayeredOverlay*>(create->lpCreateParams);
            SetWindowLongPtrW(panel, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
        }
        if (!overlay) return DefWindowProcW(panel, message, wParam, lParam);
        if (message == WM_PAINT) {
            overlay->PaintFeatherPanel(panel);
            return 0;
        }
        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_COMMAND:
        case WM_CTLCOLOREDIT:
            if (overlay->featherPanelHwnd_ == panel) {
                const UINT id = message == WM_COMMAND ? LOWORD(wParam) :
                    GetDlgCtrlID(reinterpret_cast<HWND>(lParam));
                if (id == kCropAngleEdit) {
                    if (message == WM_CTLCOLOREDIT) {
                        return reinterpret_cast<LRESULT>(CropNumericBrush(reinterpret_cast<HDC>(wParam)));
                    }
                    if (overlay->cropAngleHwnd_ && HIWORD(wParam) == EN_CHANGE) {
                        if (const auto value = ReadDecimalField(overlay->cropAngleHwnd_)) {
                            const double angle = std::round((std::clamp)(*value, -180.0, 180.0) * 10) / 10;
                            if (std::exchange(overlay->imageTransformDraft_.degrees, angle) != angle) {
                                overlay->RefreshCropTransform();
                            }
                        }
                    }
                    if (overlay->cropAngleHwnd_ && HIWORD(wParam) == EN_KILLFOCUS) overlay->UpdateCropAngleField();
                    return 0;
                }
                if (id >= kFirstFeatherEdit && id < kFirstFeatherEdit + FeatherSideCount) {
                    const size_t side = id - kFirstFeatherEdit;
                    if (message == WM_CTLCOLOREDIT) {
                        return reinterpret_cast<LRESULT>(CropNumericBrush(reinterpret_cast<HDC>(wParam),
                            !overlay->cropFeatherDraft_[side].enabled));
                    }
                    if (HIWORD(wParam) == EN_KILLFOCUS) overlay->CommitFeatherEdit(side);
                    return 0;
                }
            }
            break;
        case WM_NCHITTEST: {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(panel, &point);
            const auto layout = overlay->FeatherPanelGeometry(panel);
            if (PtInRect(&layout.header, point) && !PtInRect(&layout.close, point)) return HTCAPTION;
            return HTCLIENT;
        }
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                POINT point{};
                GetCursorPos(&point);
                ScreenToClient(panel, &point);
                const bool action = FeatherPanelHitTest(overlay->FeatherPanelGeometry(panel), point) != FeatherNoAction;
                SetCursor(LoadCursorW(nullptr, action ? IDC_HAND : IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_MOUSEMOVE: {
            const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const int hover = FeatherPanelHitTest(overlay->FeatherPanelGeometry(panel), point);
            if (std::exchange(overlay->featherPanelHover_, hover) != hover) InvalidateRect(panel, nullptr, FALSE);
            TRACKMOUSEEVENT track{sizeof(TRACKMOUSEEVENT), TME_LEAVE, panel, 0};
            TrackMouseEvent(&track);
            return 0;
        }
        case WM_MOUSELEAVE:
            overlay->featherPanelHover_ = FeatherNoAction;
            InvalidateRect(panel, nullptr, FALSE);
            return 0;
        case WM_WINDOWPOSCHANGED:
            if (overlay->featherScrubSide_ != FeatherNoAction && IsWindowVisible(overlay->featherScrubHwnd_)) {
                overlay->PositionFeatherScrub(overlay->featherPanelEdits_[overlay->featherScrubSide_]);
            }
            break;
        case WM_MOVING: {
            const auto* proposed = reinterpret_cast<const RECT*>(lParam);
            RECT current{};
            if (proposed && GetWindowRect(panel, &current) &&
                (proposed->left != current.left || proposed->top != current.top)) {
                overlay->featherPanelDetached_ = true;
            }
            return TRUE;
        }
        case WM_LBUTTONDOWN: {
            SetFocus(panel);
            const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            overlay->featherPanelPressed_ = FeatherPanelHitTest(overlay->FeatherPanelGeometry(panel), point);
            if (overlay->featherPanelPressed_ != FeatherNoAction) SetCapture(panel);
            InvalidateRect(panel, nullptr, FALSE);
            return 0;
        }
        case WM_CAPTURECHANGED:
            overlay->featherPanelPressed_ = FeatherNoAction;
            InvalidateRect(panel, nullptr, FALSE);
            return 0;
        case WM_LBUTTONUP: {
            const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const int hit = FeatherPanelHitTest(overlay->FeatherPanelGeometry(panel), point);
            const int pressed = std::exchange(overlay->featherPanelPressed_, FeatherNoAction);
            if (GetCapture() == panel) ReleaseCapture();
            if (hit != pressed || hit == FeatherNoAction) return 0;
            if (hit == ViewClose) {
                overlay->CloseFeatherPanel();
            } else if (hit >= FeatherRotateLeft) {
                overlay->ActivateCropTransform(hit);
            } else {
                FeatherEdge& edge = overlay->cropFeatherDraft_[hit];
                edge.enabled = !edge.enabled;
                const HWND edit = overlay->featherPanelEdits_[hit];
                SetCropNumericEdit(edit, edge.pixels);
                InvalidateRect(edit, nullptr, TRUE);
                InvalidateRect(panel, nullptr, FALSE);
                overlay->resetFrameSchedule_ = true;
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                overlay->CloseFeatherPanel();
                return 0;
            }
            break;
        case WM_CLOSE:
            overlay->CloseFeatherPanel();
            return 0;
        case WM_NCDESTROY:
            if (overlay->featherPanelHwnd_ == panel) {
                overlay->StopFeatherPress();
                overlay->featherPanelHwnd_ = nullptr;
                overlay->cropAngleHwnd_ = nullptr;
                overlay->featherPanelEdits_.fill(nullptr);
            }
            break;
        }
        return DefWindowProcW(panel, message, wParam, lParam);
    }

    enum CropDrag : unsigned {
        CropLeft = 1, CropTop = 2, CropRight = 4, CropBottom = 8, CropMove = 16, CropDraw = 32,
    };

    unsigned CropDragAt(POINT point) const {
        if (EqualRect(&cropDraft_, &kFullCrop)) return CropDraw;
        RECT client{};
        GetClientRect(hwnd_, &client);
        const auto mapped = CropSourcePoint(point, client);
        point = {std::lround(mapped[0] * client.right), std::lround(mapped[1] * client.bottom)};
        const RECT frame = CropPixels(cropDraft_, client.right, client.bottom);
        const int grip = UiScaled(8);
        RECT hit = frame;
        InflateRect(&hit, grip, grip);
        if (!PtInRect(&hit, point)) return CropDraw;
        unsigned edges = 0;
        if (abs(point.x - frame.left) <= grip || abs(point.x - frame.right) <= grip) {
            edges |= abs(point.x - frame.left) <= abs(point.x - frame.right) ? CropLeft : CropRight;
        }
        if (abs(point.y - frame.top) <= grip || abs(point.y - frame.bottom) <= grip) {
            edges |= abs(point.y - frame.top) <= abs(point.y - frame.bottom) ? CropTop : CropBottom;
        }
        if (edges) return edges;
        return CropMove;
    }

    std::array<double, 2> CropSourcePoint(POINT point, const RECT& client) const {
        const auto map = ImageMappingFor(DisplayImageRegion(), imageTransformDraft_);
        const double u = double(point.x) / (std::max)(1L, client.right) - 0.5;
        const double v = double(point.y) / (std::max)(1L, client.bottom) - 0.5;
        return {map.xx * u + map.xy * v + 0.5, map.yx * u + map.yy * v + 0.5};
    }

    LPCWSTR CropResizeCursor(unsigned action) const {
        if (action == CropMove) return IDC_SIZEALL;
        const int x = action & CropLeft ? -1 : action & CropRight ? 1 : 0;
        const int y = action & CropTop ? -1 : action & CropBottom ? 1 : 0;
        if (!x && !y) return IDC_CROSS;
        const double radians = imageTransformDraft_.degrees * 3.14159265358979323846 / 180.0;
        const int dx = imageTransformDraft_.flipX ? -x : x;
        const int dy = imageTransformDraft_.flipY ? -y : y;
        const double angle = std::atan2(dy, dx) + radians;
        const auto axis = (std::lround(angle * 4 / 3.14159265358979323846) + 8) % 4;
        const LPCWSTR cursors[]{IDC_SIZEWE, IDC_SIZENWSE, IDC_SIZENS, IDC_SIZENESW};
        return cursors[axis];
    }

    POINT NormalizedCropPoint(LPARAM lParam) const {
        RECT client{};
        GetClientRect(hwnd_, &client);
        const auto point = CropSourcePoint(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, client);
        return POINT{std::lround((std::clamp)(point[0], 0.0, 1.0) * kCropExtent),
            std::lround((std::clamp)(point[1], 0.0, 1.0) * kCropExtent)};
    }

    void ResizeLockedCrop(const RECT& requested, unsigned edges, POINT anchor) {
        const LONG sourceWidth = sourceWidth_;
        const LONG sourceHeight = sourceHeight_;
        const bool horizontal = (edges & (CropLeft | CropRight)) != 0;
        const bool vertical = (edges & (CropTop | CropBottom)) != 0;
        const LONG availableWidth = !horizontal ? sourceWidth :
            (edges & CropLeft) ? anchor.x : sourceWidth - anchor.x;
        const LONG availableHeight = !vertical ? sourceHeight :
            (edges & CropTop) ? anchor.y : sourceHeight - anchor.y;
        if (availableWidth <= 0 || availableHeight <= 0) return;
        const LONG w = requested.right - requested.left;
        const LONG h = requested.bottom - requested.top;
        const double aspectWidth = cropAspectSize_.cx;
        const double aspectHeight = cropAspectSize_.cy;
        // Project a corner drag onto the locked aspect; edge drags use that edge's axis.
        const double scale = horizontal && vertical
            ? (w * aspectWidth + h * aspectHeight) /
                (aspectWidth * aspectWidth + aspectHeight * aspectHeight)
            : horizontal ? w / aspectWidth : h / aspectHeight;
        const SIZE size = BoundedCropSize(CropAspectBounds(),
            RECT{0, 0, availableWidth, availableHeight}, scale);
        const LONG x = (std::clamp)(horizontal ?
            ((edges & CropLeft) ? anchor.x - size.cx : anchor.x) : anchor.x - size.cx / 2,
            0L, sourceWidth - size.cx);
        const LONG y = (std::clamp)(vertical ?
            ((edges & CropTop) ? anchor.y - size.cy : anchor.y) : anchor.y - size.cy / 2,
            0L, sourceHeight - size.cy);
        SetCropDraftPixels(RECT{x, y, x + size.cx, y + size.cy});
    }

    void UpdateCropDrag(LPARAM lParam) {
        if (!cropDrag_) return;
        const POINT cursor = NormalizedCropPoint(lParam);
        const LONG dx = cursor.x - cropDragStart_.x;
        const LONG dy = cursor.y - cropDragStart_.y;
        RECT draft = cropDragStartRect_;
        if (cropDrag_ == CropDraw) {
            if (dx && dy) {
                draft = RECT{(std::min)(cursor.x, cropDragStart_.x), (std::min)(cursor.y, cropDragStart_.y),
                    (std::max)(cursor.x, cropDragStart_.x), (std::max)(cursor.y, cropDragStart_.y)};
            }
        } else if (cropDrag_ == CropMove) {
            OffsetRect(&draft, (std::clamp)(dx, -draft.left, kCropExtent - draft.right),
                (std::clamp)(dy, -draft.top, kCropExtent - draft.bottom));
        } else {
            if (cropDrag_ & CropLeft) draft.left = (std::clamp)(draft.left + dx, 0L, draft.right - 1);
            if (cropDrag_ & CropTop) draft.top = (std::clamp)(draft.top + dy, 0L, draft.bottom - 1);
            if (cropDrag_ & CropRight) draft.right = (std::clamp)(draft.right + dx, draft.left + 1, kCropExtent);
            if (cropDrag_ & CropBottom) draft.bottom = (std::clamp)(draft.bottom + dy, draft.top + 1, kCropExtent);
        }
        if (cropAspectLocked_ && cropDrag_ != CropMove &&
            !EqualRect(&draft, &cropDragStartRect_)) {
            const RECT pixels = CropPixels(draft, sourceWidth_, sourceHeight_);
            if (cropDrag_ == CropDraw) {
                const unsigned edges = (dx < 0 ? CropLeft : CropRight) |
                    (dy < 0 ? CropTop : CropBottom);
                ResizeLockedCrop(pixels, edges, POINT{
                    MulDiv(cropDragStart_.x, sourceWidth_, kCropExtent),
                    MulDiv(cropDragStart_.y, sourceHeight_, kCropExtent)});
            } else {
                const RECT original = CropPixels(cropDragStartRect_, sourceWidth_, sourceHeight_);
                const LONG x = (cropDrag_ & CropLeft) ? original.right :
                    (cropDrag_ & CropRight) ? original.left : (original.left + original.right) / 2;
                const LONG y = (cropDrag_ & CropTop) ? original.bottom :
                    (cropDrag_ & CropBottom) ? original.top : (original.top + original.bottom) / 2;
                ResizeLockedCrop(pixels, cropDrag_, POINT{x, y});
            }
            return;
        }
        cropDraft_ = draft;
        UpdateCropEdits();
        resetFrameSchedule_ = true;
    }

    void ToggleAspectLock() {
        aspectLocked_ = !aspectLocked_;
        if (aspectLocked_ && sourceWidth_ && sourceHeight_) {
            RECT rect{};
            GetWindowRect(hwnd_, &rect);
            if (CropActive() || imageTransform_.active()) {
                FitCropViewport(rect, DisplayImageBounds());
            } else {
                const int width = rect.right - rect.left;
                const int height = (std::max)(1, MulDiv(width, sourceHeight_, sourceWidth_));
                SetWindowPos(hwnd_, WindowZOrder(), rect.left, rect.top, width, height,
                    SWP_NOACTIVATE | SWP_SHOWWINDOW);
            }
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
                    if (desktopOutputPaused_.load(std::memory_order_relaxed)) { Sleep(50); nextFrame = Clock::now(); continue; }
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
                if (desktopOutputPaused_.load(std::memory_order_relaxed)) { Sleep(50); nextFrame = Clock::now(); continue; }
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
        if (ScreenImageBlendActive()) {
            const std::lock_guard lock(imageBackdropMutex_);
            nextImageBackdrop_ = {};
        }
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
            return Tr(L"核显/节能");
        }
        if (adapterIndex == highPerformanceGpuIndex_) {
            return Tr(L"高性能");
        }
        return Tr(L"Windows 自动");
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
            { 1, Tr(L"从 Steam 启动 VTube Studio") },
            { 2, Tr(L"从外部启动 start_without_steam.bat") },
        };
        TASKDIALOGCONFIG config{};
        config.cbSize = sizeof(config);
        config.hwndParent = toolbarHwnd_;
        config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
        config.pszWindowTitle = kWindowTitle;
        config.pszMainInstruction = Tr(L"VTube Studio 已关闭，请选择重启方式");
        config.pszContent = Tr(
            L"Steam 选项会直接打开 VTube Studio.exe；外部选项会运行安装目录中的 start_without_steam.bat。");
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
                    Tr(L"Windows 只能一键指定“节能”或“高性能”GPU，无法可靠指定这张额外显卡。\n\n本次没有修改设置。"),
                    kWindowTitle,
                    MB_OK | MB_ICONINFORMATION);
            });
            return false;
        }

        const auto directory = FindVtsDirectory();
        if (!directory) {
            RunModalWhileRendering([this]() {
                return MessageBoxW(
                    toolbarHwnd_,
                    Tr(L"没有找到 VTube Studio 安装目录。\n\n程序已经检查正在运行的 VTS、Steam 库和默认安装目录。请先启动一次 VTube Studio 后再尝试。"),
                    kWindowTitle,
                    MB_OK | MB_ICONWARNING);
            });
            return false;
        }

        const std::filesystem::path vtsExecutable = *directory / kVtsExecutableName;
        const std::filesystem::path startBatch = *directory / kVtsBatchName;
        std::wstring confirmation =
            std::wstring(Tr(L"将把 VTube Studio 切换到：\n\n")) + GpuName(adapterIndex) +
            L" (" + GpuPreferenceLabel(adapterIndex) + L")" +
            Tr(L"\n\n这会暂时中断面捕，并依次执行：\n1. 正常关闭 VTube Studio\n2. 保存 Windows 显卡偏好\n3. 运行 VTube Studio\n4. 自动重启 VTSFloat_Meow\n\n是否继续？");
        const int answer = RunModalWhileRendering([this, &confirmation]() {
            return MessageBoxW(
                toolbarHwnd_, confirmation.c_str(),
                kWindowTitle,
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
                Tr(L"写入 Windows 显卡偏好失败，本次没有关闭 VTube Studio。"),
                kWindowTitle,
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
                    Tr(L"无法向 VTube Studio 发送正常退出命令。为了保护当前状态，程序不会强制结束它。"),
                    kWindowTitle,
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
                    Tr(L"VTube Studio 在 15 秒内没有退出。为了避免丢失状态，程序没有强制关闭它，GPU 设置也已还原。"),
                    kWindowTitle,
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
                Tr(L"未能运行 start_without_steam.bat。显卡偏好已还原，请手动启动 VTube Studio。"),
                kWindowTitle,
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
                Tr(L"VTube Studio 已开始重启，但覆盖层自动重启助手启动失败。\n\n请稍后手动运行 start_overlay.cmd。"),
                kWindowTitle,
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
        std::wstring recommendedLabel = Tr(L"推荐（使用核显/节能 GPU）");
        if (hasRecommendedGpu) {
            recommendedLabel += std::wstring(Tr(L"：")) + GpuName(minimumPowerGpuIndex_);
        } else {
            recommendedLabel += Tr(L"（未检测到）");
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
            Tr(L"建议：优先使用核显/节能 GPU 驱动覆盖层（若可用）"));
        AppendMenuW(
            menu,
            MF_STRING | MF_GRAYED,
            0,
            Tr(L"高性能 GPU 可能与游戏争用调度资源，造成卡顿"));
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
                label += Tr(L"  [Spout 当前]");
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
        const int currentGpu = senderGpuIndex_ >= 0 ? senderGpuIndex_ : activeGpuIndex_;
        if (requested >= 0 &&
            (gpuAdapters_.size() <= 1 || requested == currentGpu ||
             requested == activeGpuIndex_)) {
            MessageBoxW(
                toolbarHwnd_,
                Tr(L"当前已经在使用这个 GPU，无需重启。"),
                L"VTSFloat_Meow",
                MB_OK | MB_ICONINFORMATION);
            return;
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

    void RefreshSpoutSender() {
        const auto now = Clock::now();
        if (now < nextSpoutScan_) return;
        nextSpoutScan_ = now + std::chrono::milliseconds(500);
        const auto senders = receiver_.GetSenderList();
        const auto contains = [&](std::string_view name) {
            return std::find(senders.begin(), senders.end(), name) != senders.end();
        };
        constexpr std::string_view preferred = "VTubeStudioSpout";
        if (!manualSpoutSender_.empty() && !contains(manualSpoutSender_)) manualSpoutSender_.clear();
        std::string next = manualSpoutSender_;
        if (next.empty() && contains(preferred)) next = preferred;
        if (next.empty() && senders.size() == 1) next = senders.front();
        const bool needsChoice = next.empty() && senders.size() > 1;
        if (std::exchange(spoutNeedsChoice_, needsChoice) != needsChoice) statusFrameDirty_ = true;
        if (next == spoutSender_) return;
        receiver_.ReleaseReceiver();
        spoutSender_ = std::move(next);
        receiver_.SetReceiverName(spoutSender_.c_str());
        spoutFrameSyncDisabled_ = false;
        scaleReadbackPrimed_ = false;
        lastSpoutRebindCheck_ = {};
        if (!spoutSender_.empty()) TryRebindGraphicsToSpoutSender();
    }

    void CreateGraphics() {
        receiver_.SetReceiverName(spoutSender_.c_str());

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
                    static_cast<UINT64>(description.DedicatedVideoMemory),
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
        if (!spoutSender_.empty()) {
            senderAdapter = receiver_.GetSenderAdapter(spoutSender_.c_str());
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

    // The overlay can be started before VTube Studio.  In that case there is
    // no Spout sender yet, so CreateGraphics() initially chooses the saved or
    // minimum-power adapter.  If VTube Studio is subsequently launched on a
    // different adapter, the DX11 share handle cannot be opened by that
    // already-created device.  Recreate the receiver/device once the sender
    // becomes visible so a script restart is not required.
    bool TryRebindGraphicsToSpoutSender() {
        if (GetCurrentThreadId() != uiThreadId_ || spoutRebindInProgress_) {
            return false;
        }
        const auto now = Clock::now();
        if (now - lastSpoutRebindCheck_ < std::chrono::milliseconds(500)) {
            return false;
        }
        lastSpoutRebindCheck_ = now;

        if (spoutSender_.empty()) return false;
        const int senderAdapterIndex = receiver_.GetSenderAdapter(spoutSender_.c_str());
        if (senderAdapterIndex < 0) {
            return false;
        }

        ComPtr<IDXGIFactory6> factory;
        ComPtr<IDXGIAdapter1> senderAdapter;
        if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))) ||
            FAILED(factory->EnumAdapters1(
                static_cast<UINT>(senderAdapterIndex), &senderAdapter))) {
            return false;
        }
        DXGI_ADAPTER_DESC1 senderDescription{};
        senderAdapter->GetDesc1(&senderDescription);

        // Record the sender even when the current D3D device is already on
        // the correct adapter.
        const auto normalizedSender = std::find_if(
            gpuAdapters_.begin(), gpuAdapters_.end(),
            [&senderDescription](const GpuAdapterInfo& info) {
                return info.luid.HighPart == senderDescription.AdapterLuid.HighPart &&
                       info.luid.LowPart == senderDescription.AdapterLuid.LowPart;
            });
        senderGpuIndex_ = normalizedSender == gpuAdapters_.end()
            ? senderAdapterIndex
            : static_cast<int>(normalizedSender->index);

        DXGI_ADAPTER_DESC1 activeDescription{};
        bool sameAdapter = false;
        if (activeAdapter3_ && SUCCEEDED(activeAdapter3_->GetDesc1(&activeDescription))) {
            sameAdapter = activeDescription.AdapterLuid.HighPart ==
                    senderDescription.AdapterLuid.HighPart &&
                activeDescription.AdapterLuid.LowPart ==
                    senderDescription.AdapterLuid.LowPart;
        }
        if (sameAdapter) {
            Log("[spout] sender confirmed on active GPU index=" +
                std::to_string(senderGpuIndex_));
            return false;
        }

        Log("[spout] sender appeared on a different GPU; rebinding receiver to " +
            WideToUtf8(senderDescription.Description));
        spoutRebindInProgress_ = true;
        receiver_.ReleaseReceiver();
        receiver_.CloseDirectX11();

        // Release all resources that retain the old adapter/device before
        // CreateGraphics() selects the sender's adapter.
        staging_.Reset();
        scaleInputView_.Reset();
        scaleInput_.Reset();
        scaleOutputView_.Reset();
        scaleOutput_.Reset();
        for (auto& readback : scaleOutputReadbacks_) {
            readback.Reset();
        }
        scaleReadbackWriteIndex_ = 0;
        scaleReadbackPrimed_ = false;
        scaleVertexShader_.Reset();
        scalePixelShader_.Reset();
        scaleSampler_.Reset();
        scalePointSampler_.Reset();
        scaleSettingsBuffer_.Reset();
        toneSettingsBuffer_.Reset();
        toneGpuDirty_ = true;
        scaleRasterizer_.Reset();
        scaleMapX_.clear();
        scaleMapY_.clear();
        scaleMapWidth_ = scaleMapHeight_ = 0;
        scaleMapSourceWidth_ = scaleMapSourceHeight_ = 0;
        scaleSourceWidth_ = scaleSourceHeight_ = 0;
        scaleSourceFormat_ = DXGI_FORMAT_UNKNOWN;
        scaleOutputWidth_ = scaleOutputHeight_ = 0;
        gpuScalerUnavailable_ = false;
        sourceWidth_ = sourceHeight_ = 0;
        sourceFormat_ = DXGI_FORMAT_UNKNOWN;
        initialAspectApplied_ = false;
        spoutFrameSyncDisabled_ = false;
        if (gpuUsageQuery_) {
            PdhCloseQuery(gpuUsageQuery_);
            gpuUsageQuery_ = nullptr;
            gpuUsageCounter_ = nullptr;
            gpuProcessMemoryCounter_ = nullptr;
        }
        activeAdapter3_.Reset();
        StopImageBackdrop();
        context_.Reset();
        device_.Reset();

        bool rebound = false;
        try {
            CreateGraphics();
            rebound = true;
        } catch (const std::exception& error) {
            Log(std::string("[spout] automatic GPU rebind failed: ") + error.what());
        }
        spoutRebindInProgress_ = false;
        return rebound;
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
        const bool sourceSizeChanged = previousSourceWidth != sourceWidth_ ||
            previousSourceHeight != sourceHeight_;
        if (cropEditing_ || ((CropActive() || imageTransform_.active()) && aspectLocked_ &&
                (!initialAspectApplied_ || sourceSizeChanged))) {
            RECT window{};
            GetWindowRect(hwnd_, &window);
            FitCropViewport(window, DisplayImageBounds());
            if (cropEditing_ && GetCurrentThreadId() == uiThreadId_) UpdateCropEdits();
            initialAspectApplied_ = true;
        } else if (aspectLocked_ && sourceWidth_ && sourceHeight_ &&
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
                    hwnd_, WindowZOrder(), 0, 0, width, height,
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
            scalePointSampler_ && scaleSettingsBuffer_ && toneSettingsBuffer_ && scaleRasterizer_) {
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
    uint featherEnabled;
    float4 featherPixels;
    float4 imageMapping;
    uint toneEnabled;
    uint colorGradeEnabled;
    uint2 padding;
};
cbuffer ToneSettings : register(b1) { float4 toneTable[256]; float4 colorGrade[3]; };
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
    const float2 centered = uv - 0.5;
    uv = float2(dot(imageMapping.xy, centered), dot(imageMapping.zw, centered)) + 0.5;
    if (any(uv < 0.0) || any(uv > 1.0)) return 0.0;
    float4 color;
    if (qualityMode == 0) color = Premultiply(sourceTexture.Sample(pointSampler, uv));
    else if (qualityMode == 2) color = BicubicSample(uv);
    else color = Premultiply(sourceTexture.Sample(linearSampler, uv));
    [branch]
    if (toneEnabled != 0 && color.a > 0.0) {
        const float3 index = saturate(color.rgb / color.a) * 255.0;
        const uint3 lo = (uint3)index;
        const uint3 hi = min(lo + 1, 255);
        color.rgb = color.a * lerp(float3(toneTable[lo.r].r, toneTable[lo.g].g, toneTable[lo.b].b),
            float3(toneTable[hi.r].r, toneTable[hi.g].g, toneTable[hi.b].b), frac(index));
    }
    [branch]
    if (colorGradeEnabled != 0) {
        color.rgb = clamp(float3(dot(colorGrade[0], color), dot(colorGrade[1], color),
            dot(colorGrade[2], color)), 0.0, color.a);
    }
    [branch]
    if (featherEnabled != 0) {
        const float4 edgeDistance = float4(uv, 1.0 - uv) * float4(sourceSize, sourceSize);
        const float4 fades = smoothstep(0.0, max(featherPixels, 1.0), edgeDistance);
        const float4 mask = lerp(1.0, fades, step(1.0, featherPixels));
        color *= min(min(mask.x, mask.y), min(mask.z, mask.w));
    }
    return color;
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
        settings.ByteWidth = 64;
        settings.Usage = D3D11_USAGE_DEFAULT;
        settings.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        result = device_->CreateBuffer(&settings, nullptr, &scaleSettingsBuffer_);
        if (FAILED(result)) return fail("settings_buffer_create", result);
        settings.ByteWidth = sizeof(toneSettings_);
        result = device_->CreateBuffer(&settings, nullptr, &toneSettingsBuffer_);
        if (FAILED(result)) return fail("tone_buffer_create", result);
        toneGpuDirty_ = true;

        D3D11_RASTERIZER_DESC rasterizer{};
        rasterizer.FillMode = D3D11_FILL_SOLID;
        rasterizer.CullMode = D3D11_CULL_NONE;
        rasterizer.DepthClipEnable = TRUE;
        result = device_->CreateRasterizerState(&rasterizer, &scaleRasterizer_);
        if (FAILED(result)) return fail("rasterizer_create", result);
        Log("[scaler] gpu_bilinear initialized");
        return true;
    }

    bool EnsureGpuScalerTextures(const RECT& region, int width, int height) {
        if (!InitializeGpuScaler()) {
            return false;
        }
        const auto regionWidth = static_cast<UINT>(region.right - region.left);
        const auto regionHeight = static_cast<UINT>(region.bottom - region.top);
        if (scaleInput_ && scaleSourceWidth_ == regionWidth &&
            scaleSourceHeight_ == regionHeight &&
            scaleSourceFormat_ == sourceFormat_ &&
            scaleOutputWidth_ == width && scaleOutputHeight_ == height) {
            return true;
        }

        scaleInput_.Reset();
        scaleInputView_.Reset();
        scaleOutput_.Reset();
        scaleOutputView_.Reset();
        for (auto& readback : scaleOutputReadbacks_) {
            readback.Reset();
        }
        scaleReadbackWriteIndex_ = 0;
        scaleReadbackPrimed_ = false;

        D3D11_TEXTURE2D_DESC input{};
        input.Width = regionWidth;
        input.Height = regionHeight;
        input.MipLevels = 1;
        input.ArraySize = 1;
        input.Format = sourceFormat_;
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
        for (size_t index = 0; index < scaleOutputReadbacks_.size(); ++index) {
            result = device_->CreateTexture2D(
                &output, nullptr, &scaleOutputReadbacks_[index]);
            if (FAILED(result)) {
                Log("[scaler] readback_texture failed index=" +
                    std::to_string(index) + " hr=" +
                    std::to_string(static_cast<long>(result)));
                gpuScalerUnavailable_ = true;
                return false;
            }
        }
        scaleReadbackWriteIndex_ = 0;
        scaleReadbackPrimed_ = false;
        scaleSourceWidth_ = regionWidth;
        scaleSourceHeight_ = regionHeight;
        scaleSourceFormat_ = sourceFormat_;
        scaleOutputWidth_ = width;
        scaleOutputHeight_ = height;
        Log("[scaler] gpu_bilinear resources=" + std::to_string(width) + "x" +
            std::to_string(height));
        return true;
    }

    bool GpuScaleIntoDib(ID3D11Texture2D* source, const RECT& region, int width, int height,
                         const FeatherRadii& feather, const vtsfloat::ImageMapping& mapping) {
        if (!EnsureGpuScalerTextures(region, width, height)) {
            return false;
        }
        if (scaleSourceWidth_ == sourceWidth_ && scaleSourceHeight_ == sourceHeight_) {
            context_->CopyResource(scaleInput_.Get(), source);
        } else {
            const D3D11_BOX box{static_cast<UINT>(region.left), static_cast<UINT>(region.top), 0,
                static_cast<UINT>(region.right), static_cast<UINT>(region.bottom), 1};
            context_->CopySubresourceRegion(scaleInput_.Get(), 0, 0, 0, 0, source, 0, &box);
        }
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
            UINT featherEnabled;
            std::array<float, FeatherSideCount> featherPixels;
            std::array<float, 4> mapping;
            UINT toneEnabled;
            UINT colorGradeEnabled;
            UINT padding[2];
        } settings{
            scaleSourceWidth_,
            scaleSourceHeight_,
            static_cast<UINT>(scalingQuality_),
            feather != FeatherRadii{} ? 1u : 0u,
            {static_cast<float>(feather[FeatherLeft]), static_cast<float>(feather[FeatherTop]),
             static_cast<float>(feather[FeatherRight]), static_cast<float>(feather[FeatherBottom])},
            {mapping.xx, mapping.xy, mapping.yx, mapping.yy},
            toneEnabled_ ? 1u : 0u, colorGradeEnabled_ ? 1u : 0u, {},
        };
        static_assert(sizeof(ScaleSettings) == 64);
        context_->UpdateSubresource(scaleSettingsBuffer_.Get(), 0, nullptr, &settings, 0, 0);
        if ((toneEnabled_ || colorGradeEnabled_) && toneGpuDirty_) {
            context_->UpdateSubresource(toneSettingsBuffer_.Get(), 0, nullptr, &toneSettings_, 0, 0);
            toneGpuDirty_ = false;
        }
        ID3D11Buffer* settingsBuffers[]{scaleSettingsBuffer_.Get(), toneSettingsBuffer_.Get()};
        context_->PSSetConstantBuffers(0, ARRAYSIZE(settingsBuffers), settingsBuffers);
        context_->Draw(3, 0);
        ID3D11ShaderResourceView* nullView = nullptr;
        context_->PSSetShaderResources(0, 1, &nullView);
        // Copy the new frame into one staging texture while reading the
        // previous frame from the other. This gives the GPU a full frame to
        // finish the copy instead of forcing an immediate pipeline stall.
        const size_t writeIndex = scaleReadbackWriteIndex_;
        const size_t readIndex = scaleReadbackPrimed_
            ? (writeIndex + scaleOutputReadbacks_.size() - 1) %
                scaleOutputReadbacks_.size()
            : writeIndex;
        context_->CopyResource(
            scaleOutputReadbacks_[writeIndex].Get(), scaleOutput_.Get());

        D3D11_MAPPED_SUBRESOURCE mapped{};
        const HRESULT mapResult = context_->Map(
            scaleOutputReadbacks_[readIndex].Get(), 0, D3D11_MAP_READ, 0, &mapped);
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
        context_->Unmap(scaleOutputReadbacks_[readIndex].Get(), 0);
        scaleReadbackPrimed_ = true;
        scaleReadbackWriteIndex_ =
            (writeIndex + 1) % scaleOutputReadbacks_.size();
        return true;
    }

    RECT SubjectHoverRegionPixels(int width, int height) const {
        if (!excludeEffectsFromHover_ || !subjectHoverRegionConfigured_ ||
            width <= 0 || height <= 0) {
            return RECT{ 0, 0, width, height };
        }
        return RECT{
            subjectHoverRegionLeft_ * width / 10000,
            subjectHoverRegionTop_ * height / 10000,
            subjectHoverRegionRight_ * width / 10000,
            subjectHoverRegionBottom_ * height / 10000,
        };
    }

    bool IsInsideSubjectHoverRegion(int x, int y, int width, int height) const {
        const RECT region = SubjectHoverRegionPixels(width, height);
        return x >= region.left && x < region.right &&
            y >= region.top && y < region.bottom;
    }

    bool IsLockedSubjectMaskPixel(int x, int y, int width, int height) const {
        if (!excludeEffectsFromHover_ || !subjectHoverRegionConfigured_) {
            return false;
        }
        if (x < 0 || y < 0 || x >= width || y >= height ||
            subjectHoverMaskGridWidth_ <= 0 || subjectHoverMaskGridHeight_ <= 0 ||
            subjectHoverMaskBits_.empty()) {
            return false;
        }
        const double u = (x + 0.5) / width - 0.5, v = (y + 0.5) / height - 0.5;
        const double maskGridX = (subjectImageMapping_.xx * u + subjectImageMapping_.xy * v + 0.5) *
            subjectHoverMaskGridWidth_;
        const double maskGridY = (subjectImageMapping_.yx * u + subjectImageMapping_.yy * v + 0.5) *
            subjectHoverMaskGridHeight_;
        const int gridX = static_cast<int>(std::floor(maskGridX));
        const int gridY = static_cast<int>(std::floor(maskGridY));
        if (gridX < 0 || gridY < 0 ||
            gridX >= subjectHoverMaskGridWidth_ ||
            gridY >= subjectHoverMaskGridHeight_) {
            return false;
        }
        const size_t index = static_cast<size_t>(gridY) *
            subjectHoverMaskGridWidth_ + gridX;
        return index / 8 < subjectHoverMaskBits_.size() &&
            (subjectHoverMaskBits_[index / 8] &
                static_cast<std::uint8_t>(1u << (index % 8))) != 0;
    }

    bool IsCursorOverModel() {
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
            if (excludeEffectsFromHover_ && subjectHoverRegionConfigured_) {
                // Use the tracked subject shape. The reference mask is moved
                // and scaled from the live dominant connected component, so
                // it follows VTS model movement without accepting isolated FX.
                return IsLockedSubjectMaskPixel(xx, yy, w, h) ? 255 : 0;
            }
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
        const bool panelOpacity = borderDialogOpen_ && currentOverlayAlpha_ < 255;
        if ((!hoverOpacityPreviewActive_ && !panelOpacity) || !dibBits_ ||
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
                if (PreviewBorderPixel(x, y)) {
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
        const bool panelOpacity = borderDialogOpen_ && currentOverlayAlpha_ < 255;
        // The expansion indicator is derived from the model's alpha shape. Keep
        // this raw snapshot in sync with every received frame while the range is
        // being edited or its fade preview is visible; otherwise an old opacity
        // preview snapshot makes the red/blue band stick to its first frame.
        const bool hoverRangePreview =
            !interactiveRendering_.load(std::memory_order_relaxed) &&
            borderDialogOpen_ && hoverFadeEnabled_ &&
            (hoverExpandEditing_ || showHoverExpandPreview_ ||
             hoverExpandPreviewAlpha_ > 0.01);
        if ((!hoverOpacityPreviewActive_ && !panelOpacity &&
             !hoverRangePreview) || !dibBits_ ||
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
        hoverOpacityCursorWasOverModel_ = false;
        hoverOpacityRestorePending_ = false;
        hoverOpacityRestoreAt_ = Clock::time_point{};
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
            hoverOpacityCursorWasOverModel_ = false;
            hoverOpacityRestorePending_ = false;
            hoverOpacityRestoreAt_ = Clock::time_point{};
            return changed;
        }
        // Model opacity is a normal target even while the personalization
        // panel is open. The render path below keeps the panel itself opaque.
        int baseAlpha = modelOpacityPercent_ * 255 / 100;
        int target = baseAlpha;
        const auto now = Clock::now();
        const bool hoverInteractionAvailable =
            (locked_ || borderDialogOpen_) && hoverFadeEnabled_;
        const bool cursorOverModel =
            hoverInteractionAvailable && IsCursorOverModel();
        if (hoverOpacityPreviewActive_) {
            target = hoverFadeEnabled_
                ? hoverOpacityPercent_ * 255 / 100
                : modelOpacityPercent_ * 255 / 100;
            hoverOpacityRestorePending_ = false;
            hoverOpacityRestoreAt_ = Clock::time_point{};
        } else if (hoverInteractionAvailable) {
            const int hoverAlpha = hoverOpacityPercent_ * 255 / 100;
            if (cursorOverModel) {
                target = hoverAlpha;
                hoverOpacityRestorePending_ = false;
                hoverOpacityRestoreAt_ = Clock::time_point{};
            } else {
                if (hoverOpacityCursorWasOverModel_) {
                    hoverOpacityRestorePending_ =
                        hoverOpacityRestoreDelaySeconds_ > 0.0;
                    hoverOpacityRestoreAt_ = hoverOpacityRestorePending_
                        ? DeadlineAfterSeconds(
                            now, hoverOpacityRestoreDelaySeconds_)
                        : Clock::time_point{};
                }
                if (hoverOpacityRestorePending_ && now < hoverOpacityRestoreAt_) {
                    target = hoverAlpha;
                } else {
                    hoverOpacityRestorePending_ = false;
                    hoverOpacityRestoreAt_ = Clock::time_point{};
                }
            }
        } else {
            hoverOpacityRestorePending_ = false;
            hoverOpacityRestoreAt_ = Clock::time_point{};
        }
        hoverOpacityCursorWasOverModel_ = cursorOverModel;

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

    void RestoreExpressionsBeforeShutdown() {
        if (expressionShutdownPrepared_) return;
        expressionShutdownPrepared_ = true;

        // Stop every UI/render path from starting another preview while the
        // main thread waits for the API worker to acknowledge the restore.
        suppressHoverExpressionUntilCursorLeaves_ = true;

        std::vector<VtsExpressionState> restoreStates;
        const char* restoreSource = nullptr;
        if (expressionPanelPreviewActive_ && expressionPanelStateCaptured_ &&
            !expressionPanelInitialStates_.empty()) {
            // The picker can temporarily change the complete expression set,
            // therefore its pre-preview snapshot is authoritative.
            restoreStates = expressionPanelInitialStates_;
            restoreSource = "panel_preview";
        } else if (hoverExpressionHovered_ && !hoverExpressionRestored_ &&
                   !hoverExpressionOriginalStates_.empty()) {
            restoreStates = hoverExpressionOriginalStates_;
            restoreSource = "model_hover";
        }

        expressionPanelPreviewActive_ = false;
        hoverExpressionRestored_ = true;
        hoverExpressionHovered_ = false;
        hoverExpressionLeaving_ = false;
        hoverExpressionRestoreAt_ = Clock::time_point{};

        if (restoreStates.empty()) return;

        // A model may expose dozens of expressions. Send only entries changed
        // by this program so exit is quick while the complete user baseline is
        // still preserved.
        const auto current = vtsApi_.GetExpressions();
        size_t queued = 0;
        for (const auto& state : restoreStates) {
            const auto existing = std::find_if(
                current.begin(), current.end(),
                [&state](const VtsApiClient::ExpressionInfo& expression) {
                    return expression.file == state.file;
                });
            if (existing != current.end() && existing->active == state.active) {
                continue;
            }
            // Shutdown restoration is immediate; waiting for a visual fade
            // would keep the process alive without improving state safety.
            vtsApi_.SetExpressionActive(state.file, state.active, 0.0);
            ++queued;
        }
        if (queued == 0) return;

        const bool delivered = vtsApi_.WaitForExpressionCommands(
            std::chrono::milliseconds(4000));
        Log(std::string("[expression] shutdown restore source=") +
            (restoreSource ? restoreSource : "unknown") +
            " commands=" + std::to_string(queued) +
            " delivered=" + (delivered ? "1" : "0"));
    }

    void RestoreHoverExpression() {
        if (!hoverExpressionHovered_ || hoverExpressionRestored_ ||
            hoverExpressionOriginalStates_.empty()) {
            return;
        }
        for (const auto& state : hoverExpressionOriginalStates_) {
            vtsApi_.SetExpressionActive(
                state.file, state.active, kHoverExpressionFadeOutSeconds);
        }
        hoverExpressionRestored_ = true;
        hoverExpressionReentryRefreshPending_ = false;
        if (borderDialogOpen_) {
            // Ignore the API cache while our restore command is in flight;
            // otherwise the low-frequency baseline scan could mistake the
            // hover transition for a user edit.
            expressionPanelIgnoreUntil_ = Clock::now() +
                std::chrono::milliseconds(1200);
            expressionPanelNextPollAt_ = expressionPanelIgnoreUntil_;
        }
    }

    void CancelHoverExpression() {
        RestoreHoverExpression();
        hoverExpressionHovered_ = false;
        hoverExpressionRestored_ = false;
        hoverExpressionOriginalStates_.clear();
        hoverExpressionBaselineStates_.clear();
        hoverExpressionExpectedStates_.clear();
        hoverExpressionCapturePending_ = false;
        hoverExpressionStatePollPending_ = false;
        hoverExpressionReentryRefreshPending_ = false;
        hoverExpressionCaptureRevision_ = 0;
        hoverExpressionStatePollRevision_ = 0;
        hoverExpressionNextStatePollAt_ = Clock::time_point{};
        hoverExpressionLeaving_ = false;
        hoverExpressionRestoreAt_ = Clock::time_point{};
    }

    bool IsConfiguredHoverExpression(const std::string& file) const {
        return std::find(
            hoverExpressionFiles_.begin(), hoverExpressionFiles_.end(), file) !=
            hoverExpressionFiles_.end();
    }

    static VtsExpressionState* FindExpressionState(
        std::vector<VtsExpressionState>& states, const std::string& file) {
        const auto found = std::find_if(
            states.begin(), states.end(), [&file](const VtsExpressionState& state) {
                return state.file == file;
            });
        return found == states.end() ? nullptr : &*found;
    }

    static const VtsExpressionState* FindExpressionState(
        const std::vector<VtsExpressionState>& states, const std::string& file) {
        const auto found = std::find_if(
            states.begin(), states.end(), [&file](const VtsExpressionState& state) {
                return state.file == file;
            });
        return found == states.end() ? nullptr : &*found;
    }

    // Merge a fresh API snapshot into the restore baseline while a hover
    // expression is active.  The state produced by our own activation is
    // represented by hoverExpressionExpectedStates_; matching values are not
    // mistaken for user edits.  Any other change is treated as a new VTS
    // state (state N) and becomes the next restore baseline (state 2).
    void MergeHoverExpressionState(
        const std::vector<VtsApiClient::ExpressionInfo>& expressions) {
        if (hoverExpressionBaselineStates_.empty() ||
            hoverExpressionExpectedStates_.empty() || expressions.empty()) {
            return;
        }

        const std::vector<VtsExpressionState> actual =
            MakeExpressionStateSnapshot(expressions);
        if (actual.empty()) return;

        std::vector<VtsExpressionState> nextBaseline = actual;
        bool userStateChanged = false;
        for (auto& state : nextBaseline) {
            const VtsExpressionState* expected = FindExpressionState(
                hoverExpressionExpectedStates_, state.file);
            const VtsExpressionState* previousBaseline = FindExpressionState(
                hoverExpressionBaselineStates_, state.file);
            if (!expected || !previousBaseline) {
                userStateChanged = true;
                continue;
            }

            if (state.active != expected->active) {
                // The current state differs from the state produced by this
                // program, so preserve the user's new value for restoration.
                userStateChanged = true;
            } else if (IsConfiguredHoverExpression(state.file)) {
                // A selected expression still matches our temporary hover
                // activation. Keep its real pre-hover value instead of
                // recording our own "active=true" as the user's new setup.
                state.active = previousBaseline->active;
            }
        }
        if (actual.size() != hoverExpressionExpectedStates_.size()) {
            userStateChanged = true;
        }

        hoverExpressionBaselineStates_ = std::move(nextBaseline);
        hoverExpressionExpectedStates_ = actual;

        // Only the configured hover expressions are restored on mouse leave;
        // update those restore values from the newest user baseline without
        // changing the configured selection itself.
        for (auto& original : hoverExpressionOriginalStates_) {
            if (const VtsExpressionState* latest = FindExpressionState(
                    hoverExpressionBaselineStates_, original.file)) {
                original.active = latest->active;
            }
        }

        if (borderDialogOpen_) {
            // State N becomes state 2 for later picker previews/cancel. This
            // update does not touch hoverExpressionFiles_.
            expressionPanelInitialStates_ = hoverExpressionBaselineStates_;
            expressionPanelStateCaptured_ = !expressionPanelInitialStates_.empty();
        }
        if (userStateChanged) {
            Log("[expression] user state changed during hover; restore baseline updated");
        }
    }

    void UpdateHoverExpressionStateTracking(const Clock::time_point now) {
        if (!hoverExpressionHovered_ || hoverExpressionRestored_ ||
            !vtsApi_.IsConnected()) {
            return;
        }

        if (hoverExpressionStatePollPending_) {
            std::vector<VtsApiClient::ExpressionInfo> expressions;
            std::uint64_t revision = 0;
            if (vtsApi_.GetExpressionStateAfter(
                    hoverExpressionStatePollRevision_, expressions, revision)) {
                hoverExpressionStatePollPending_ = false;
                hoverExpressionStatePollRevision_ = revision;
                MergeHoverExpressionState(expressions);
                hoverExpressionNextStatePollAt_ = now +
                    std::chrono::milliseconds(kHoverExpressionStatePollIntervalMs);
            }
        }

        // A real leave -> enter edge after the two-second cooldown requests a
        // response newer than the edge itself. If an older periodic request is
        // still in flight, consume it first and then issue this one; otherwise
        // the pre-entry response could be mistaken for the requested refresh.
        if (hoverExpressionReentryRefreshPending_ &&
            !hoverExpressionStatePollPending_ &&
            vtsApi_.WaitForExpressionCommands(std::chrono::milliseconds(0))) {
            hoverExpressionStatePollRevision_ =
                vtsApi_.GetExpressionStateRevision();
            hoverExpressionStatePollPending_ = true;
            hoverExpressionReentryRefreshPending_ = false;
            hoverExpressionNextStatePollAt_ = now +
                std::chrono::milliseconds(kHoverExpressionStatePollIntervalMs);
            vtsApi_.RequestExpressionState();
            return;
        }

        if (!hoverExpressionStatePollPending_ &&
            now >= hoverExpressionNextStatePollAt_ &&
            vtsApi_.WaitForExpressionCommands(std::chrono::milliseconds(0))) {
            hoverExpressionStatePollRevision_ =
                vtsApi_.GetExpressionStateRevision();
            hoverExpressionStatePollPending_ = true;
            hoverExpressionNextStatePollAt_ = now +
                std::chrono::milliseconds(kHoverExpressionStatePollIntervalMs);
            vtsApi_.RequestExpressionState();
        }
    }

    void UpdateHoverExpression() {
        // Hover preview is also available while personalization is open. The
        // explicit 2.5-second selection preview has priority; once it restores
        // the saved baseline, the configured region behaves like locked mode.
        const bool hoverInteractionAvailable = locked_ || borderDialogOpen_;
        const bool cursorOverModel = IsCursorOverModel();
        const bool cursorEnteredModel =
            cursorOverModel && !hoverExpressionCursorWasOverModel_;
        hoverExpressionCursorWasOverModel_ = cursorOverModel;
        if (suppressHoverExpressionUntilCursorLeaves_ && !cursorOverModel) {
            suppressHoverExpressionUntilCursorLeaves_ = false;
        }
        const bool shouldHover = hoverInteractionAvailable &&
            !expressionPanelPreviewActive_ &&
            !suppressHoverExpressionUntilCursorLeaves_ &&
            hoverExpressionEnabled_ && !hoverExpressionFiles_.empty() &&
            hasReceivedModel_ && vtsApi_.IsConnected() && cursorOverModel;
        const auto now = Clock::now();

        if (!shouldHover && hoverExpressionCapturePending_) {
            // The pointer left before the fresh pre-play snapshot arrived.
            // Discard the pending entry; no expression has been activated.
            hoverExpressionCapturePending_ = false;
            hoverExpressionCaptureRevision_ = 0;
        }

        if (shouldHover && (!hoverExpressionHovered_ || hoverExpressionRestored_)) {
            if (now < hoverExpressionNextTriggerAt_) {
                // A completed hover session cannot be restarted repeatedly.
                // Latch suppression until the pointer leaves, so merely
                // waiting over the model for the 2-second CD to expire does
                // not trigger a request without a real new entry.
                suppressHoverExpressionUntilCursorLeaves_ = true;
                hoverExpressionCapturePending_ = false;
                hoverExpressionCaptureRevision_ = 0;
                return;
            }
            // Every hover session starts from a new ExpressionStateResponse.
            // Never play from the API cache: requesting/capturing state 1 has
            // strict priority over sending ExpressionActivationRequest.
            if (!hoverExpressionCapturePending_) {
                if (!vtsApi_.WaitForExpressionCommands(
                        std::chrono::milliseconds(0))) {
                    return;
                }
                hoverExpressionCaptureRevision_ =
                    vtsApi_.GetExpressionStateRevision();
                hoverExpressionCapturePending_ = true;
                hoverExpressionNextStatePollAt_ = now +
                    std::chrono::milliseconds(kHoverExpressionStatePollIntervalMs);
                vtsApi_.RequestExpressionState();
                return;
            }

            std::vector<VtsApiClient::ExpressionInfo> freshExpressions;
            std::uint64_t freshRevision = 0;
            if (!vtsApi_.GetExpressionStateAfter(
                    hoverExpressionCaptureRevision_, freshExpressions,
                    freshRevision)) {
                if (now >= hoverExpressionNextStatePollAt_) {
                    hoverExpressionNextStatePollAt_ = now +
                        std::chrono::milliseconds(
                            kHoverExpressionStatePollIntervalMs);
                    vtsApi_.RequestExpressionState();
                }
                return;
            }

            const std::vector<VtsExpressionState> freshState =
                MakeExpressionStateSnapshot(freshExpressions);
            if (freshState.empty()) {
                hoverExpressionCaptureRevision_ = freshRevision;
                vtsApi_.RequestExpressionState();
                return;
            }

            std::vector<VtsExpressionState> originalStates;
            originalStates.reserve(hoverExpressionFiles_.size());
            bool allStatesKnown = true;
            for (const std::string& file : hoverExpressionFiles_) {
                const VtsExpressionState* state = FindExpressionState(
                    freshState, file);
                if (!state) {
                    allStatesKnown = false;
                    break;
                }
                originalStates.push_back(*state);
            }
            if (!allStatesKnown) {
                hoverExpressionCaptureRevision_ = freshRevision;
                vtsApi_.RequestExpressionState();
                return;
            }

            if (borderDialogOpen_) {
                // The fresh pre-play snapshot also closes the race where the
                // user changes VTS immediately before moving over the model.
                expressionPanelInitialStates_ = freshState;
                expressionPanelStateCaptured_ = true;
            }
            hoverExpressionOriginalStates_ = std::move(originalStates);
            hoverExpressionBaselineStates_ = freshState;
            hoverExpressionExpectedStates_ = freshState;
            for (const std::string& file : hoverExpressionFiles_) {
                if (VtsExpressionState* expected = FindExpressionState(
                        hoverExpressionExpectedStates_, file)) {
                    expected->active = true;
                }
            }
            hoverExpressionHovered_ = true;
            hoverExpressionRestored_ = false;
            hoverExpressionLeaving_ = false;
            hoverExpressionRestoreAt_ = Clock::time_point{};
            hoverExpressionCapturePending_ = false;
            hoverExpressionCaptureRevision_ = 0;
            hoverExpressionStatePollPending_ = false;
            hoverExpressionStatePollRevision_ = freshRevision;
            hoverExpressionNextStatePollAt_ = now +
                std::chrono::milliseconds(kHoverExpressionStatePollIntervalMs);
            hoverExpressionNextTriggerAt_ = now +
                std::chrono::milliseconds(kHoverExpressionRetriggerCooldownMs);
            for (const std::string& file : hoverExpressionFiles_) {
                vtsApi_.SetExpressionActive(
                    file, true, kHoverExpressionFadeInSeconds);
            }
        } else if (shouldHover && hoverExpressionHovered_) {
            // Re-entering before the leave delay expires cancels the pending
            // restore, so the expression remains active without a new toggle.
            hoverExpressionLeaving_ = false;
            hoverExpressionRestoreAt_ = Clock::time_point{};
            if (cursorEnteredModel && now >= hoverExpressionNextTriggerAt_) {
                hoverExpressionNextTriggerAt_ = now +
                    std::chrono::milliseconds(
                        kHoverExpressionRetriggerCooldownMs);
                hoverExpressionReentryRefreshPending_ = true;
            }
            UpdateHoverExpressionStateTracking(now);
        } else if (!shouldHover && hoverExpressionHovered_) {
            UpdateHoverExpressionStateTracking(now);
            if (!hoverExpressionLeaving_) {
                // The configured delay starts only after the pointer leaves
                // the model hit region. It does not shorten the expression's
                // visible time while the pointer remains over the model.
                hoverExpressionLeaving_ = true;
                hoverExpressionRestoreAt_ = DeadlineAfterSeconds(
                    now, hoverExpressionDurationSeconds_);
            } else if (!hoverExpressionRestored_ &&
                       now >= hoverExpressionRestoreAt_) {
                // If a 250 ms state refresh is already in flight, consume it
                // before restoring. This closes the boundary race where the
                // user changes an expression just as the leave delay expires.
                // Do not wait forever if the API connection itself is gone.
                if (vtsApi_.IsConnected() &&
                    hoverExpressionStatePollPending_) {
                    return;
                }
                // Restore the exact state captured on entry with the smooth
                // one-second fade-out. Keep the session latched until re-entry
                // so a delayed API response cannot trigger a second toggle.
                RestoreHoverExpression();
            }
        }
    }

    static std::vector<VtsExpressionState> MakeExpressionStateSnapshot(
        const std::vector<VtsApiClient::ExpressionInfo>& expressions) {
        std::vector<VtsExpressionState> snapshot;
        snapshot.reserve(expressions.size());
        for (const auto& expression : expressions) {
            if (!expression.file.empty()) {
                snapshot.push_back(VtsExpressionState{
                    expression.file, expression.active });
            }
        }
        return snapshot;
    }

    static bool ExpressionStateSnapshotsEqual(
        const std::vector<VtsExpressionState>& left,
        const std::vector<VtsExpressionState>& right) {
        if (left.size() != right.size()) return false;
        for (const auto& state : left) {
            const auto match = std::find_if(
                right.begin(), right.end(),
                [&state](const VtsExpressionState& other) {
                    return other.file == state.file;
                });
            if (match == right.end() || match->active != state.active) {
                return false;
            }
        }
        return true;
    }

    // VTS does not emit a general expression-state-changed event. While the
    // personalization panel is open, refresh at a low rate and treat changes
    // that are not caused by our preview/hover commands as the new restore
    // baseline (state 2 replaces state 1).
    void UpdatePanelExpressionState() {
        if (!borderDialogOpen_ || !vtsApi_.IsConnected()) return;
        const auto now = Clock::now();
        if (now < expressionPanelNextPollAt_) return;
        expressionPanelNextPollAt_ = now +
            std::chrono::milliseconds(kExpressionStatePollIntervalMs);
        vtsApi_.RequestExpressionState();

        if (!expressionPanelStateCaptured_ || expressionPanelPreviewActive_ ||
            (hoverExpressionHovered_ && !hoverExpressionRestored_) ||
            now < expressionPanelIgnoreUntil_) {
            return;
        }
        const auto expressions = vtsApi_.GetExpressions();
        const auto current = MakeExpressionStateSnapshot(expressions);
        if (current.empty() || ExpressionStateSnapshotsEqual(
                current, expressionPanelInitialStates_)) {
            return;
        }
        // This is the user's externally changed configuration. Keep the
        // latest complete set so subsequent previews restore state 2 exactly.
        expressionPanelInitialStates_ = current;
    }

    // The personalization panel must never leave expressions "worn" after a
    // preview. Snapshot the complete state that existed when the panel opened,
    // preview any checked set, then restore every file as one transaction.
    void BeginPanelExpressionSession() {
        RestorePanelExpressionPreview();
        expressionPanelInitialStates_.clear();
        expressionPanelStateCaptured_ = false;
        expressionPanelPreviewActive_ = false;
        expressionPanelNextPollAt_ = Clock::now();
        expressionPanelIgnoreUntil_ = Clock::time_point{};
    }

    bool CapturePanelExpressionState() {
        if (expressionPanelStateCaptured_) {
            return true;
        }
        const auto expressions = vtsApi_.GetExpressions();
        if (expressions.empty()) {
            // State refresh is asynchronous. Keep the panel usable, but do
            // not apply a preview until there is a reliable restore point.
            vtsApi_.RequestExpressionState();
            return false;
        }
        expressionPanelInitialStates_ = MakeExpressionStateSnapshot(expressions);
        expressionPanelStateCaptured_ = !expressionPanelInitialStates_.empty();
        if (!expressionPanelStateCaptured_) {
            vtsApi_.RequestExpressionState();
        } else {
            expressionPanelNextPollAt_ = Clock::now() +
                std::chrono::milliseconds(kExpressionStatePollIntervalMs);
        }
        return expressionPanelStateCaptured_;
    }

    void RestorePanelExpressionPreview() {
        if (!expressionPanelPreviewActive_ || !expressionPanelStateCaptured_) {
            return;
        }
        for (const auto& state : expressionPanelInitialStates_) {
            vtsApi_.SetExpressionActive(
                state.file, state.active, kHoverExpressionFadeOutSeconds);
        }
        expressionPanelPreviewActive_ = false;
        expressionPanelIgnoreUntil_ = Clock::now() +
            std::chrono::milliseconds(1200);
        expressionPanelNextPollAt_ = expressionPanelIgnoreUntil_;
    }

    void EndPanelExpressionSession() {
        RestorePanelExpressionPreview();
        expressionPanelPreviewActive_ = false;
        expressionPanelStateCaptured_ = false;
        expressionPanelInitialStates_.clear();
        expressionPanelNextPollAt_ = Clock::time_point{};
        expressionPanelIgnoreUntil_ = Clock::time_point{};
    }

    bool PreviewExpressionsFromPanel() {
        if (!vtsApi_.IsConnected()) {
            vtsApi_.RequestExpressionState();
            return false;
        }
        if (!CapturePanelExpressionState()) {
            return false;
        }
        for (const std::string& file : hoverExpressionFiles_) {
            const auto selected = std::find_if(
                expressionPanelInitialStates_.begin(),
                expressionPanelInitialStates_.end(),
                [&file](const VtsExpressionState& state) {
                    return state.file == file;
                });
            if (selected == expressionPanelInitialStates_.end()) {
                // VTS changed model or expression list while the panel was
                // open. Refresh first instead of losing the restore baseline.
                expressionPanelStateCaptured_ = false;
                expressionPanelInitialStates_.clear();
                vtsApi_.RequestExpressionState();
                return false;
            }
        }

        // Keep the full checked set continuously active while the dropdown is
        // open. Unchecked expressions are temporarily disabled, and the exact
        // pre-picker configuration is restored after the close countdown.
        expressionPanelPreviewActive_ = true;
        for (const auto& state : expressionPanelInitialStates_) {
            vtsApi_.SetExpressionActive(
                state.file, std::find(
                    hoverExpressionFiles_.begin(), hoverExpressionFiles_.end(),
                    state.file) != hoverExpressionFiles_.end(),
                kHoverExpressionFadeInSeconds);
        }
        return true;
    }

    void DrawBgWarningOverlay(int width, int height) {
        if (locked_ || statusMode_ != VtsStatusMode::Hidden ||
            !opaqueBackgroundDetected_ || bgWarningPermanentlyDismissed_) {
            bgWarningCloseRect_ = RECT{};
            bgWarningDontShowRect_ = RECT{};
            return;
        }
        if (!memoryDc_ || !dibBits_ || width < 320 || height < 90) return;

        const int margin = UiScaled(12);
        const int panelW = (std::min)(UiScaled(470), width - margin * 2);
        const int panelH = (std::min)(UiScaled(62), height - margin * 2);
        const int panelX = width - panelW - margin;
        const int panelY = margin;

        HDC dc = memoryDc_;
        RECT panel{ panelX, panelY, panelX + panelW, panelY + panelH };
        HBRUSH bg = CreateSolidBrush(RGB(40, 12, 28));
        FillRect(dc, &panel, bg);
        DeleteObject(bg);
        HBRUSH border = CreateSolidBrush(RGB(255, 120, 180));
        for (int inset = 0; inset < UiScaled(1); ++inset) {
            RECT borderRect{
                panel.left + inset, panel.top + inset,
                panel.right - inset, panel.bottom - inset };
            FrameRect(dc, &borderRect, border);
        }
        DeleteObject(border);

        HFONT font = CreateFontW(
            -UiFontSize(nullptr, UiScaled(14)), 0, 0, 0,
            FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        HGDIOBJ oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);

        SetTextColor(dc, RGB(255, 220, 235));
        RECT line1{
            panelX + UiScaled(8), panelY + UiScaled(4),
            panelX + panelW - UiScaled(8), panelY + UiScaled(22) };
        DrawTextW(dc, Tr(L"检测到当前 Spout 非透明推流："),
            -1, &line1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT line2{
            panelX + UiScaled(8), panelY + UiScaled(20),
            panelX + panelW - UiScaled(8), panelY + UiScaled(38) };
        DrawTextW(dc, Tr(L"请在 VTS 主界面更改背景为 \"ColorPicker\" 并启用 \"透明推流\""),
            -1, &line2,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Buttons at bottom-right. English and Russian action labels are much
        // wider than Chinese, so size each hit target from the active font.
        const auto actionWidth = [this, dc](const wchar_t* text) {
            SIZE size{};
            GetTextExtentPoint32W(dc, text, static_cast<int>(wcslen(text)), &size);
            return (std::max)(
                UiScaled(90), static_cast<int>(size.cx) + UiScaled(14));
        };
        const int closeW = actionWidth(Tr(L"[关闭提示]"));
        const int dontShowW = actionWidth(Tr(L"[不再提示]"));
        const int btnH = UiScaled(18);
        const int btnY = panelY + panelH - btnH - UiScaled(4);
        const int gap = UiScaled(4);
        bgWarningCloseRect_ = RECT{
            panelX + panelW - UiScaled(8) - closeW, btnY,
            panelX + panelW - UiScaled(8), btnY + btnH };
        bgWarningDontShowRect_ = RECT{
            bgWarningCloseRect_.left - gap - dontShowW, btnY,
            bgWarningCloseRect_.left - gap, btnY + btnH };
        SetTextColor(dc, RGB(160, 200, 255));
        DrawTextW(dc, Tr(L"[不再提示]"), -1, &bgWarningDontShowRect_,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(dc, RGB(200, 200, 200));
        DrawTextW(dc, Tr(L"[关闭提示]"), -1, &bgWarningCloseRect_,
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
        // Windows runs moving/resizing in a modal UI loop. The expansion mask
        // performs several full-frame distance-transform passes, so running it
        // on every interactive resize frame can starve the mouse-up message and
        // leave the system resize cursor captured. Keep the live model render,
        // but defer this diagnostic layer until the move/resize has finished.
        if (interactiveRendering_.load(std::memory_order_relaxed)) {
            return;
        }
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
            if (excludeEffectsFromHover_ && subjectHoverRegionConfigured_) {
                return IsLockedSubjectMaskPixel(x, y, width, height) ? 255 : 0;
            }
            return modelPixels[(static_cast<size_t>(y) * width + x) * 4 + 3];
        };
        auto isModel = [&](int x, int y) { return alphaAt(x, y) > 128; };
        const bool outside = r > 0;
        auto setHighlight = [&](int x, int y) {
            if (x < 0 || y < 0 || x >= width || y >= height) return;
            if (isBorderRegion(x, y)) return;
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
        // Never dim the whole model to make the inset preview stand out.
        // Model opacity must always come from the user's normal/hover opacity
        // settings; this layer only paints the red/blue boundary indicator.
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

    static std::vector<Gdiplus::PointF> SimplifySubjectContour(
        const std::vector<POINT>& closed, float scaleX, float scaleY) {
        // The mask stores cell occupancy. Recover straight runs across
        // stair steps before the contour is rasterized at screen resolution.
        constexpr double toleranceSquared = 0.9 * 0.9;
        std::vector<bool> keep(closed.size());
        keep.front() = keep.back() = true;
        std::vector<std::pair<size_t, size_t>> pending{{0, closed.size() - 1}};
        while (!pending.empty()) {
            const auto [first, last] = pending.back();
            pending.pop_back();
            const POINT start = closed[first];
            const double dx = closed[last].x - start.x, dy = closed[last].y - start.y;
            const double lengthSquared = dx * dx + dy * dy;
            double farthest = toleranceSquared;
            size_t split = last;
            for (size_t i = first + 1; i < last; ++i) {
                const double x = closed[i].x - start.x, y = closed[i].y - start.y;
                const double t = lengthSquared > 0
                    ? (std::clamp)((x * dx + y * dy) / lengthSquared, 0.0, 1.0) : 0.0;
                const double ex = x - dx * t, ey = y - dy * t;
                const double distance = ex * ex + ey * ey;
                if (distance > farthest) {
                    farthest = distance;
                    split = i;
                }
            }
            if (split != last) {
                keep[split] = true;
                pending.emplace_back(first, split);
                pending.emplace_back(split, last);
            }
        }
        if (std::count(keep.begin(), keep.end(), true) < 4) {
            std::fill(keep.begin(), keep.end(), true);
        }
        std::vector<Gdiplus::PointF> contour;
        for (size_t i = 0; i + 1 < closed.size(); ++i) {
            if (keep[i]) contour.emplace_back(closed[i].x * scaleX, closed[i].y * scaleY);
        }
        return contour;
    }

    void BuildSubjectRegionOutline(int width, int height) {
        const int gridWidth = subjectHoverMaskGridWidth_;
        const int gridHeight = subjectHoverMaskGridHeight_;
        subjectOutlineWidth_ = width;
        subjectOutlineHeight_ = height;
        subjectOutlineGridWidth_ = gridWidth;
        subjectOutlineGridHeight_ = gridHeight;
        subjectOutlineMapping_ = subjectImageMapping_;
        subjectOutlineMask_ = subjectHoverMaskBits_;
        subjectOutlinePixels_.clear();
        if (gridWidth <= 0 || gridHeight <= 0 || subjectOutlineMask_.empty()) return;

        const auto occupied = [&](int x, int y) {
            if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return false;
            const size_t bit = static_cast<size_t>(y) * gridWidth + x;
            return bit / 8 < subjectOutlineMask_.size() &&
                (subjectOutlineMask_[bit / 8] & (1u << (bit % 8))) != 0;
        };
        const int stride = gridWidth + 1;
        std::vector<std::uint8_t> edges(static_cast<size_t>(stride) * (gridHeight + 1));
        const auto vertex = [stride](int x, int y) { return static_cast<size_t>(y) * stride + x; };
        // Keep occupied cells on the right of every directed edge, including
        // the inner contours of holes.
        for (int y = 0; y < gridHeight; ++y) {
            for (int x = 0; x < gridWidth; ++x) {
                if (!occupied(x, y)) continue;
                if (!occupied(x, y - 1)) edges[vertex(x, y)] |= 1 << 0;
                if (!occupied(x + 1, y)) edges[vertex(x + 1, y)] |= 1 << 1;
                if (!occupied(x, y + 1)) edges[vertex(x + 1, y + 1)] |= 1 << 2;
                if (!occupied(x - 1, y)) edges[vertex(x, y + 1)] |= 1 << 3;
            }
        }

        ULONG_PTR token{};
        Gdiplus::GdiplusStartupInput startup{};
        if (Gdiplus::GdiplusStartup(&token, &startup, nullptr) != Gdiplus::Ok) return;
        {
            constexpr POINT directions[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
            constexpr int turns[] = {1, 0, 3, 2};
            const float scaleX = static_cast<float>(width) / gridWidth;
            const float scaleY = static_cast<float>(height) / gridHeight;
            Gdiplus::GraphicsPath path(Gdiplus::FillModeWinding);
            for (size_t start = 0; start < edges.size(); ++start) {
                while (edges[start]) {
                    POINT point{static_cast<LONG>(start % stride), static_cast<LONG>(start / stride)};
                    size_t index = start;
                    int direction = 0;
                    while (!(edges[start] & (1 << direction))) ++direction;
                    std::vector<POINT> contour{point};
                    do {
                        edges[index] &= static_cast<std::uint8_t>(~(1 << direction));
                        point.x += directions[direction].x;
                        point.y += directions[direction].y;
                        index = vertex(point.x, point.y);
                        contour.push_back(point);
                        if (index == start || !edges[index]) break;
                        // At a corner shared by two diagonal islands, follow
                        // the current island instead of crossing to the other.
                        for (const int turn : turns) {
                            const int next = (direction + turn) % 4;
                            if (edges[index] & (1 << next)) {
                                direction = next;
                                break;
                            }
                        }
                    } while (true);
                    if (index != start || contour.size() < 4) continue;
                    auto smooth = SimplifySubjectContour(contour, scaleX, scaleY);
                    for (auto& point : smooth) {
                        const auto output = vtsfloat::ImageOutputPoint(subjectOutlineMapping_, point.X / width, point.Y / height);
                        point.X = static_cast<float>(output[0] * width);
                        point.Y = static_cast<float>(output[1] * height);
                    }
                    if (smooth.size() >= 3) path.AddPolygon(smooth.data(), static_cast<INT>(smooth.size()));
                }
            }
            if (path.GetPointCount() > 0) {
                Gdiplus::RectF bounds{};
                path.GetBounds(&bounds);
                constexpr int padding = 8;
                const int left = (std::max)(0, static_cast<int>(std::floor(bounds.X)) - padding);
                const int top = (std::max)(0, static_cast<int>(std::floor(bounds.Y)) - padding);
                const int right = (std::min)(width, static_cast<int>(std::ceil(bounds.GetRight())) + padding);
                const int bottom = (std::min)(height, static_cast<int>(std::ceil(bounds.GetBottom())) + padding);
                const int rowWidth = right - left;
                const int rowCount = bottom - top;
                if (rowWidth > 0 && rowCount > 0) {
                    std::vector<std::uint32_t> image(static_cast<size_t>(rowWidth) * rowCount);
                    Gdiplus::Bitmap bitmap(rowWidth, rowCount, rowWidth * 4, PixelFormat32bppPARGB,
                        reinterpret_cast<BYTE*>(image.data()));
                    {
                        Gdiplus::Graphics graphics(&bitmap);
                        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                        graphics.TranslateTransform(static_cast<float>(-left), static_cast<float>(-top));
                        Gdiplus::Pen pen(Gdiplus::Color(255, 70, 255, 70), 12.0f);
                        pen.SetLineJoin(Gdiplus::LineJoinRound);
                        graphics.DrawPath(&pen, &path);
                        // Only keep the outside half of the stroke.
                        graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
                        const Gdiplus::SolidBrush clear(Gdiplus::Color(0, 0, 0, 0));
                        graphics.FillPath(&clear, &path);
                        graphics.Flush(Gdiplus::FlushIntentionSync);
                    }
                    for (int y = 0; y < rowCount; ++y) {
                        for (int x = 0; x < rowWidth; ++x) {
                            const auto alpha = static_cast<std::uint8_t>(image[static_cast<size_t>(y) * rowWidth + x] >> 24);
                            if (alpha) subjectOutlinePixels_.push_back(SubjectOutlinePixel{left + x, top + y, alpha});
                        }
                    }
                }
            }
        }
        Gdiplus::GdiplusShutdown(token);
    }

    void DrawSubjectRegionPreview(int width, int height) {
        if (!showSubjectRegionPreview_ || !subjectHoverRegionConfigured_ ||
            !excludeEffectsFromHover_ || !dibBits_ || width <= 0 || height <= 0) {
            return;
        }
        if (width != subjectOutlineWidth_ || height != subjectOutlineHeight_ ||
            subjectHoverMaskGridWidth_ != subjectOutlineGridWidth_ ||
            subjectHoverMaskGridHeight_ != subjectOutlineGridHeight_ ||
            subjectHoverMaskBits_ != subjectOutlineMask_ || !(subjectOutlineMapping_ == subjectImageMapping_)) {
            BuildSubjectRegionOutline(width, height);
        }
        for (const auto& pixel : subjectOutlinePixels_) {
            if (!PreviewBorderPixel(pixel.x, pixel.y)) BlendPixel(pixel.x, pixel.y, 70, 255, 70, pixel.alpha);
        }
    }

    void DrawGpuWarningOverlay(int width, int height) {
        if (locked_ || statusMode_ != VtsStatusMode::Hidden ||
            !gpuWarningShown_ || gpuWarningPermanentlyDismissed_) {
            gpuWarningDontShowRect_ = RECT{};
            return;
        }
        if (!memoryDc_ || !dibBits_ || width < 320 || height < 60) return;

        const int margin = UiScaled(12);
        const int panelW = (std::min)(UiScaled(620), width - margin * 2);
        const int panelH = UiScaled(34);
        const int panelX = width - panelW - margin;
        int panelY = margin;
        if (opaqueBackgroundDetected_ && !bgWarningPermanentlyDismissed_) {
            panelY = margin + UiScaled(62 + 8);  // below BG warning
        }
        if (panelY >= height) return;
        HDC dc = memoryDc_;
        RECT panel{ panelX, panelY, panelX + panelW, panelY + panelH };
        HBRUSH bg = CreateSolidBrush(RGB(50, 40, 15));
        FillRect(dc, &panel, bg);
        DeleteObject(bg);
        HBRUSH border = CreateSolidBrush(RGB(255, 200, 100));
        for (int inset = 0; inset < UiScaled(1); ++inset) {
            RECT borderRect{
                panel.left + inset, panel.top + inset,
                panel.right - inset, panel.bottom - inset };
            FrameRect(dc, &borderRect, border);
        }
        DeleteObject(border);

        HFONT font = CreateFontW(
            -UiFontSize(nullptr, UiScaled(14)), 0, 0, 0,
            FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        HGDIOBJ oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);

        SIZE dontShowSize{};
        const wchar_t* dontShowText = Tr(L"[不再提示]");
        GetTextExtentPoint32W(
            dc, dontShowText, static_cast<int>(wcslen(dontShowText)), &dontShowSize);
        const int dontShowWidth = (std::max)(
            UiScaled(90), static_cast<int>(dontShowSize.cx) + UiScaled(14));
        gpuWarningDontShowRect_ = RECT{
            panelX + UiScaled(6), panelY + UiScaled(6),
            panelX + UiScaled(6) + dontShowWidth,
            panelY + panelH - UiScaled(6) };
        SetTextColor(dc, RGB(160, 200, 255));
        DrawTextW(dc, dontShowText, -1, &gpuWarningDontShowRect_,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SetTextColor(dc, RGB(255, 230, 170));
        RECT textRect{ gpuWarningDontShowRect_.right + UiScaled(6), panelY,
            panelX + panelW - UiScaled(6), panelY + panelH };
        DrawTextW(dc, Tr(L"当前运行在高性能显卡中，高负载场景性能将会受限"),
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

    bool PresentLayeredFrame(const RECT& rect, int width, int height, HWND target = nullptr) {
        const bool modelFrame = !target;
        if (!memoryDc_ || !dibBits_ || width != dibWidth_ || height != dibHeight_) {
            return false;
        }
        // Status pages share the DIB, but contain UI rather than model alpha.
        // Never expand their pixels or paint model diagnostics into the cache.
        if (modelFrame && !cropEditing_) {
            DrawBgWarningOverlay(width, height);
            DrawGpuWarningOverlay(width, height);
            DrawHoverExpandPreview(width, height);
            DrawSubjectRegionPreview(width, height);
        } else {
            showHoverExpandPreview_ = false;
            hoverExpandPreviewUntil_ = Clock::time_point{};
            hoverExpandPreviewAlpha_ = 0.0;
            hoverExpandFadeStarted_ = Clock::now();
        }
        const auto updateStarted = Clock::now();
        POINT destination{ rect.left, rect.top };
        SIZE size{ width, height };
        POINT sourcePoint{ 0, 0 };
        BLENDFUNCTION blend{
            AC_SRC_OVER, 0,
            static_cast<BYTE>(
                (!modelFrame || cropEditing_)
                    ? 255
                    : (borderDialogOpen_ || hoverOpacityPreviewActive_ || shadowMaskReady_[1] ? 255 : currentOverlayAlpha_)),
            AC_SRC_ALPHA };
        HDC screen = GetDC(nullptr);
        const BOOL updated = UpdateLayeredWindow(
            target ? target : hwnd_, screen, &destination, &size, memoryDc_, &sourcePoint,
            0, &blend, ULW_ALPHA);
        ReleaseDC(nullptr, screen);

        const double updateMs = std::chrono::duration<double, std::milli>(
            Clock::now() - updateStarted).count();
        updateMs_ += updateMs;
        maxUpdateMs_ = (std::max)(maxUpdateMs_, updateMs);
        if (updated) {
            ++updated_;
            if (modelFrame) {
                lastModelPresentation_ = updateStarted;
                if (++modelPresentations_ == 1) firstModelPresentation_ = updateStarted;
            }
        } else {
            ++errors_;
        }
        return updated != FALSE;
    }

    RECT VtsStatusBounds(const RECT& model, const RECT& anchor, const RECT& work, bool docked) const {
        const int preferredWidth = docked ? anchor.right - anchor.left
            : PreferredWideToolbarWidth(model.right - model.left);
        const int width = (std::min)(preferredWidth, static_cast<int>(work.right - work.left));
        int height = (std::min)(UiScaled(280), static_cast<int>(work.bottom - work.top));
        const int gap = UiScaled(kToolbarGap);
        int top = model.top;
        if (docked) {
            const int below = work.bottom - anchor.bottom - gap;
            const int above = anchor.top - gap - work.top;
            const bool placeAbove = below < height && above > below;
            const int available = placeAbove ? above : below;
            if (available > 0) height = (std::min)(height, available);
            top = placeAbove ? anchor.top - gap - height : anchor.bottom + gap;
        }
        if (width <= 0 || height <= 0) return {};
        top = (std::clamp)(top, static_cast<int>(work.top), static_cast<int>(work.bottom) - height);
        const int left = (std::clamp)(static_cast<int>(anchor.left),
            static_cast<int>(work.left), static_cast<int>(work.right) - width);
        return RECT{left, top, left + width, top + height};
    }

    void PresentVtsStatus() {
        if (GetCurrentThreadId() != uiThreadId_ || statusMode_ == VtsStatusMode::Hidden ||
            statusDismissed_) return;
        if (!statusHwnd_) CreateVtsStatusWindow();
        if (!statusHwnd_) return;

        RECT model{};
        if (!GetWindowRect(hwnd_, &model)) return;
        RECT anchor = model;
        const bool docked = !locked_ && toolbarHwnd_ && IsWindowVisible(toolbarHwnd_) &&
            GetWindowRect(toolbarHwnd_, &anchor);
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (!GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitor)) return;
        const RECT bounds = VtsStatusBounds(model, anchor, monitor.rcWork, docked);
        if (IsRectEmpty(&bounds)) return;
        const int width = bounds.right - bounds.left;
        const int height = bounds.bottom - bounds.top;

        const bool visible = IsWindowVisible(statusHwnd_) != FALSE;
        if (!visible) {
            // The model window keeps its saved bounds while the waiting page
            // is visible. Clear any previous model pixels before showing it.
            const int modelWidth = model.right - model.left;
            const int modelHeight = model.bottom - model.top;
            RecreateDib(modelWidth, modelHeight);
            std::memset(dibBits_, 0, static_cast<size_t>(modelWidth) * modelHeight * 4);
            PresentLayeredFrame(model, modelWidth, modelHeight, hwnd_);
            statusFrameDirty_ = true;
        }
        RECT current{};
        GetWindowRect(statusHwnd_, &current);
        const bool redraw = statusFrameDirty_ || width != dibWidth_ || height != dibHeight_;
        if (redraw) {
            RecreateDib(width, height);
            DrawVtsStatusPanel(width, height);
        }
        if ((redraw || !EqualRect(&bounds, &current)) &&
            !PresentLayeredFrame(bounds, width, height, statusHwnd_)) {
            statusFrameDirty_ = true;
            return;
        }
        const bool topmost = (GetWindowLongPtrW(statusHwnd_, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
        if (!visible || topmost == desktopMode_) {
            SetWindowPos(statusHwnd_, WindowZOrder(), 0, 0, 0, 0,
                SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        }
    }

    void RenderFrame() {
        // The subject-picker intentionally freezes the last model frame while
        // the user draws the hit region, just like a screenshot selection.
        if (desktopOutputPaused_.load(std::memory_order_relaxed) ||
            !overlayVisible_ || subjectSelectionHwnd_) {
            return;
        }
        if (!cropEditing_) {
            UpdateHoverOpacity();
            UpdateHoverExpression();
        }
        if (GetCurrentThreadId() == uiThreadId_) RefreshSpoutSender();
        const auto receiveStarted = Clock::now();
        if (spoutSender_.empty() || !receiver_.ReceiveTexture()) {
            SetSpoutFrameAvailable(false);
            // VTS may have been launched after this process and can publish
            // Spout from another GPU.  Rebind before presenting the waiting
            // page; the next frame can then be received without restarting.
            TryRebindGraphicsToSpoutSender();
            PollVtsStatus();
            PresentVtsStatus();
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
            SetSpoutFrameAvailable(false);
            PollVtsStatus();
            PresentVtsStatus();
            return;
        }
        // Modal menus render on a worker thread.  Do not change window styles
        // from that thread; ApplyClickThrough must stay on the UI thread or
        // TrackPopupMenu can deadlock and make the process appear hung.
        if (GetCurrentThreadId() == uiThreadId_) {
            HideVtsStatus();
        } else if (statusMode_ != VtsStatusMode::Hidden && !statusDismissed_) {
            return;
        }
        ++newFrames_;
        SetSpoutFrameAvailable(true);
        const bool firstModelFrame = !hasReceivedModel_;
        hasReceivedModel_ = true;
        if (firstModelFrame && GetCurrentThreadId() == uiThreadId_) {
            if (!apiOptedOut_ && !apiStartedAfterModel_ && toolbarHwnd_) {
                apiStartScheduled_ = true;
                SetTimer(toolbarHwnd_, 48, kVtsApiModelReadyDelayMs, nullptr);
                if (ReadConfigString(L"VtsApi", L"Token").empty()) {
                    awaitingUserApproval_ = true;
                    SetTimer(toolbarHwnd_, 45, 500, nullptr);
                    SetTimer(toolbarHwnd_, 46, kVtsApiAuthorizationWaitMs + kVtsApiModelReadyDelayMs, nullptr);
                }
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

        const RECT region = DisplayImageRegion();
        const auto transform = DisplayImageTransform();
        const auto mapping = ImageMappingFor(region, transform);
        if (!cropEditing_) subjectImageMapping_ = mapping;
        const FeatherRadii feather = cropEditing_ ? FeatherRadii{} :
            CropFeatherRadii(region, featherPanelHwnd_ ? cropFeatherDraft_ : cropFeather_);
        const auto mapStarted = Clock::now();
        // The GPU path also performs premultiplied-alpha conversion. Use it
        // at 1:1 as well so the normal path no longer walks every pixel on
        // the CPU merely to premultiply RGB by alpha.
        const bool scaledOnGpu = GpuScaleIntoDib(source, region, width, height, feather, mapping);
        if (!scaledOnGpu) {
            context_->CopyResource(staging_.Get(), source);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            const HRESULT mapResult = context_->Map(
                staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
            if (FAILED(mapResult)) {
                ++errors_;
                return;
            }
            if (transform.active()) ScaleTransformedCpu(mapped, region, width, height, mapping, feather);
            else {
                ConvertAndScale(mapped, region, width, height);
                if (feather != FeatherRadii{}) ApplyFeather(RECT{0, 0, width, height}, region, feather);
            }
            context_->Unmap(staging_.Get(), 0);
            if (toneEnabled_ || colorGradeEnabled_) ApplyImageToneCpu(width, height);
        }
        ApplyImageDetail(width, height);
        if (!cropEditing_) {
            PrepareShadow(width, height);
            ApplyImageBlend(rect, width);
        }
        ApplyImageVignette(width, height);
        mapMs_ += std::chrono::duration<double, std::milli>(Clock::now() - mapStarted).count();

        if (cropEditing_) {
            DrawCropEditor(width, height, mapping);
        } else {
            // Capture the model before painting controls, so hover opacity only affects the model.
            if (shadowMaskReady_[0]) CompositeShadow(width, height, 0);
            CaptureHoverPreviewBase();
            if (shadowMaskReady_[1]) {
                if (!borderDialogOpen_ && !hoverOpacityPreviewActive_ && currentOverlayAlpha_ < 255) {
                    auto* pixels = static_cast<uint8_t*>(dibBits_);
                    const size_t bytes = static_cast<size_t>(width) * height * 4;
                    for (size_t i = 0; i < bytes; ++i) {
                        pixels[i] = static_cast<uint8_t>((pixels[i] * currentOverlayAlpha_ + 127) / 255);
                    }
                }
                CompositeShadow(width, height, 1);
            }
            if (!locked_ || debugMode_) DrawControls(width, height);
        }
        PresentLayeredFrame(rect, width, height);
    }

    void EnsureBilinearScaleMaps(int width, int height, UINT regionWidth, UINT regionHeight) {
        if (scaleMapWidth_ == width && scaleMapHeight_ == height &&
            scaleMapSourceWidth_ == regionWidth && scaleMapSourceHeight_ == regionHeight) {
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
            scaleMapX_[x] = makeSample(x, width, regionWidth);
        }
        for (int y = 0; y < height; ++y) {
            scaleMapY_[y] = makeSample(y, height, regionHeight);
        }
        scaleMapWidth_ = width;
        scaleMapHeight_ = height;
        scaleMapSourceWidth_ = regionWidth;
        scaleMapSourceHeight_ = regionHeight;
    }

    void ConvertAndScale(const D3D11_MAPPED_SUBRESOURCE& mapped, const RECT& region,
                         int width, int height) {
        auto* destination = static_cast<std::uint8_t*>(dibBits_);
        const auto* sourceBase = static_cast<const std::uint8_t*>(mapped.pData) +
            static_cast<size_t>(region.top) * mapped.RowPitch + static_cast<size_t>(region.left) * 4;
        const auto regionWidth = static_cast<UINT>(region.right - region.left);
        const auto regionHeight = static_cast<UINT>(region.bottom - region.top);
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
        if (static_cast<UINT>(width) == regionWidth && static_cast<UINT>(height) == regionHeight) {
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

        EnsureBilinearScaleMaps(width, height, regionWidth, regionHeight);
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

    static FeatherRadii CropFeatherRadii(const RECT& source, const FeatherEdges& settings) {
        const int halfWidth = (source.right - source.left) / 2;
        const int halfHeight = (source.bottom - source.top) / 2;
        FeatherRadii radii{};
        for (size_t i = 0; i < radii.size(); ++i) {
            if (settings[i].enabled) {
                const int limit = i == FeatherLeft || i == FeatherRight ? halfWidth : halfHeight;
                radii[i] = (std::min)(settings[i].pixels, limit);
            }
        }
        return radii;
    }

    static const std::vector<std::uint16_t>& FeatherOpacity(
        FeatherRamp& ramp, int outputExtent, int sourceExtent, int radius) {
        if (ramp.outputExtent != outputExtent || ramp.sourceExtent != sourceExtent ||
            ramp.radius != radius) {
            ramp.outputExtent = outputExtent;
            ramp.sourceExtent = sourceExtent;
            ramp.radius = radius;
            const double sourcePixelsPerOutputPixel = sourceExtent / static_cast<double>(outputExtent);
            const int band = (std::clamp)(static_cast<int>(
                std::ceil(radius / sourcePixelsPerOutputPixel - 0.5)), 0, (outputExtent + 1) / 2);
            ramp.opacity.resize(band);
            for (int i = 0; i < band; ++i) {
                const double t = (i + 0.5) * sourcePixelsPerOutputPixel / radius;
                ramp.opacity[i] = static_cast<std::uint16_t>(std::lround(256 * t * t * (3 - 2 * t)));
            }
        }
        return ramp.opacity;
    }

    void ApplyFeather(const RECT& output, const RECT& source, const FeatherRadii& radii) {
        const int width = output.right - output.left;
        const int height = output.bottom - output.top;
        const int sourceWidth = source.right - source.left;
        const int sourceHeight = source.bottom - source.top;
        const auto& left = FeatherOpacity(featherRamps_[FeatherLeft], width, sourceWidth, radii[FeatherLeft]);
        const auto& top = FeatherOpacity(featherRamps_[FeatherTop], height, sourceHeight, radii[FeatherTop]);
        const auto& right = FeatherOpacity(featherRamps_[FeatherRight], width, sourceWidth, radii[FeatherRight]);
        const auto& bottom = FeatherOpacity(featherRamps_[FeatherBottom], height, sourceHeight, radii[FeatherBottom]);
        const int leftEnd = static_cast<int>(left.size());
        const int rightStart = width - static_cast<int>(right.size());
        const int topEnd = static_cast<int>(top.size());
        const int bottomStart = height - static_cast<int>(bottom.size());
        auto* base = static_cast<std::uint8_t*>(dibBits_);
        const auto fade = [](std::uint8_t* pixel, unsigned opacity) {
            // RGB must fade with alpha to keep the DIB premultiplied.
            for (int channel = 0; channel < 4; ++channel) {
                pixel[channel] = static_cast<std::uint8_t>((pixel[channel] * opacity + 128) >> 8);
            }
        };
        for (int y = 0; y < height; ++y) {
            const unsigned rowOpacity = y < topEnd ? top[y] : y >= bottomStart ? bottom[height - 1 - y] : 256;
            auto* row = base + (static_cast<size_t>(output.top + y) * dibWidth_ + output.left) * 4;
            for (int x = 0; x < leftEnd; ++x) {
                fade(row + x * 4, (std::min)(rowOpacity, static_cast<unsigned>(left[x])));
            }
            if (rowOpacity < 256) {
                for (int x = leftEnd; x < rightStart; ++x) fade(row + x * 4, rowOpacity);
            }
            for (int x = rightStart; x < width; ++x) {
                fade(row + x * 4, (std::min)(rowOpacity, static_cast<unsigned>(right[width - 1 - x])));
            }
        }
    }

    static void ShadeOutsideCrop(std::uint8_t* pixel) {
        const unsigned transparency = 255 - pixel[3];
        const unsigned gray = (28 * transparency + 127) / 255;
        for (int i = 0; i < 3; ++i) pixel[i] = (pixel[i] * 112 >> 8) + gray;
        pixel[3] += (144 * transparency + 127) / 255;
    }

    void DrawTransformedCropMask(int width, int height, const vtsfloat::ImageMapping& map,
                                 const RECT& crop) {
        const float dx = map.xx * sourceWidth_ / width;
        const float dy = map.yx * sourceHeight_ / width;
        auto* pixel = static_cast<std::uint8_t*>(dibBits_);
        for (int y = 0; y < height; ++y) {
            const float v = (y + 0.5f) / height - 0.5f;
            const float u = 0.5f / width - 0.5f;
            float sx = (map.xx * u + map.xy * v + 0.5f) * sourceWidth_;
            float sy = (map.yx * u + map.yy * v + 0.5f) * sourceHeight_;
            for (int x = 0; x < width; ++x, pixel += 4, sx += dx, sy += dy) {
                if ((sx < crop.left || sx >= crop.right || sy < crop.top || sy >= crop.bottom) &&
                    sx >= 0 && sx < sourceWidth_ && sy >= 0 && sy < sourceHeight_) {
                    ShadeOutsideCrop(pixel);
                }
            }
        }
    }

    void DrawCropEditor(int width, int height, const vtsfloat::ImageMapping& mapping) {
        const bool hasSelection = !EqualRect(&cropDraft_, &kFullCrop);
        const RECT selection = CropPixels(cropDraft_, width, height);
        const RECT source = CropDraftPixels();
        const bool transformed = imageTransformDraft_.active();
        if (!hasSelection) {
            auto* pixels = static_cast<uint8_t*>(dibBits_);
            for (size_t i = 0, count = size_t(width) * height; i < count; ++i, pixels += 4) {
                ShadeOutsideCrop(pixels);
            }
        } else if (transformed) {
            DrawTransformedCropMask(width, height, mapping, source);
        } else {
            auto* pixels = static_cast<std::uint8_t*>(dibBits_);
            for (int y = 0; y < height; ++y) {
                const bool insideRows = y >= selection.top && y < selection.bottom;
                for (int x = 0; x < width; ++x, pixels += 4) {
                    if (!insideRows || x < selection.left || x >= selection.right) {
                        ShadeOutsideCrop(pixels);
                    }
                }
            }
        }

        auto* previewPixel = static_cast<uint8_t*>(dibBits_);
        constexpr unsigned backdropOpacity = 150;
        constexpr unsigned previewGray = 104;
        constexpr unsigned premultipliedGray = (previewGray * backdropOpacity + 127) / 255;
        for (size_t i = 0, count = size_t(width) * height; i < count; ++i, previewPixel += 4) {
            const unsigned transparency = 255 - previewPixel[3];
            if (!transparency) continue;
            const unsigned gray = (premultipliedGray * transparency + 127) / 255;
            for (int c = 0; c < 3; ++c) {
                previewPixel[c] += gray;
            }
            previewPixel[3] += (backdropOpacity * transparency + 127) / 255;
        }
        if (!hasSelection) return;

        const auto fill = [&](RECT bounds, std::uint32_t color) {
            const RECT canvas{0, 0, width, height};
            if (!IntersectRect(&bounds, &bounds, &canvas)) return;
            auto* canvasPixels = static_cast<std::uint32_t*>(dibBits_);
            for (LONG y = bounds.top; y < bounds.bottom; ++y) {
                std::fill_n(canvasPixels + static_cast<size_t>(y) * width + bounds.left,
                    bounds.right - bounds.left, color);
            }
        };
        const int stroke = UiScaled(2);
        const int grip = UiScaled(4);
        constexpr std::uint32_t border = 0xff64bcff;
        const LONG left = selection.left, right = selection.right;
        const LONG top = selection.top, bottom = selection.bottom;
        std::array<POINT, 4> corners{{{left, top}, {right, top}, {right, bottom}, {left, bottom}}};
        if (transformed) {
            corners = CropScreenCorners(source, mapping, width, height);
            for (size_t i = 0; i < corners.size(); ++i) {
                const POINT a = corners[i], b = corners[(i + 1) % corners.size()];
                const LONG steps = (std::max)({1L, std::abs(b.x - a.x), std::abs(b.y - a.y)});
                for (LONG step = 0; step <= steps; ++step) {
                    const LONG x = (std::clamp)(a.x + MulDiv(step, b.x - a.x, steps), 0L, width - 1L);
                    const LONG y = (std::clamp)(a.y + MulDiv(step, b.y - a.y, steps), 0L, height - 1L);
                    fill(RECT{x - stroke / 2, y - stroke / 2,
                        x - stroke / 2 + stroke, y - stroke / 2 + stroke}, border);
                }
            }
        } else {
            fill(RECT{left, top, right, top + stroke}, border);
            fill(RECT{left, bottom - stroke, right, bottom}, border);
            fill(RECT{left, top, left + stroke, bottom}, border);
            fill(RECT{right - stroke, top, right, bottom}, border);
        }
        std::array<POINT, 8> handles{};
        for (size_t i = 0; i < corners.size(); ++i) {
            const POINT a = corners[i], b = corners[(i + 1) % corners.size()];
            handles[i * 2] = a;
            handles[i * 2 + 1] = POINT{(a.x + b.x) / 2, (a.y + b.y) / 2};
        }
        for (POINT point : handles) {
            point.x = (std::clamp)(point.x, 0L, width - 1L);
            point.y = (std::clamp)(point.y, 0L, height - 1L);
            fill(RECT{point.x - grip, point.y - grip, point.x + grip + 1, point.y + grip + 1},
                0xffe6f4ff);
        }
    }

    void DrawControls(int width, int height) {
        const bool cropped = CropActive();
        const int thickness = cropped
            ? (std::min)(borderThickness_, ((std::min)(width, height) - 1) / 4)
            : borderThickness_;
        if (cropped && thickness <= 0) return;
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

        for (int n = 0; n < thickness; ++n) {
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
        for (int n = thickness; n < thickness + 2; ++n) {
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

    void RefreshDebugReadout(bool redraw) {
        if (GetCurrentThreadId() != uiThreadId_) return;
        if (!debugMode_ || !overlayVisible_ || !hasReceivedModel_ || subjectSelectionHwnd_ || cropEditing_ ||
            desktopOutputPaused_.load(std::memory_order_relaxed) ||
            (statusMode_ != VtsStatusMode::Hidden && !statusDismissed_)) {
            if (debugReadoutHwnd_ && IsWindowVisible(debugReadoutHwnd_)) {
                ShowWindow(debugReadoutHwnd_, SW_HIDE);
            }
            return;
        }
        RECT model{};
        if (!GetWindowRect(hwnd_, &model)) return;
        const int width = (std::min)(static_cast<int>(model.right - model.left), UiScaled(520));
        const int height = (std::min)(static_cast<int>(model.bottom - model.top), UiScaled(270));
        if (width < UiScaled(180) || height < UiScaled(90)) {
            if (debugReadoutHwnd_) ShowWindow(debugReadoutHwnd_, SW_HIDE);
            return;
        }
        if (!debugReadoutHwnd_) {
            debugReadoutHwnd_ = CreateWindowExW(
                WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                kDebugReadoutClass, L"VTSFloat_Meow", WS_POPUP,
                model.left, model.top, width, height, hwnd_, nullptr, instance_, nullptr);
            if (!debugReadoutHwnd_) return;
        }
        RECT current{};
        GetWindowRect(debugReadoutHwnd_, &current);
        if (!redraw && IsWindowVisible(debugReadoutHwnd_) &&
            current.right - current.left == width && current.bottom - current.top == height) {
            if (current.left != model.left || current.top != model.top) {
                SetWindowPos(debugReadoutHwnd_, nullptr, model.left, model.top, 0, 0,
                    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return;
        }

        HDC dc = CreateCompatibleDC(nullptr);
        if (!dc) return;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!bitmap) {
            DeleteDC(dc);
            return;
        }
        HGDIOBJ previous = SelectObject(dc, bitmap);
        const size_t count = static_cast<size_t>(width) * height;
        std::memset(pixels, 0, count * 4);
        DrawDebugReadout(dc, width, height);
        GdiFlush();
        auto* bgra = static_cast<BYTE*>(pixels);
        for (size_t index = 0; index < count; ++index, bgra += 4) {
            // GDI draws onto black. Use the brightest channel as coverage
            // to keep the text antialiased and its RGB premultiplied.
            bgra[3] = (std::max)({bgra[0], bgra[1], bgra[2]});
        }
        POINT position{model.left, model.top}, origin{};
        SIZE size{width, height};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        if (UpdateLayeredWindow(debugReadoutHwnd_, nullptr, &position, &size,
                                dc, &origin, 0, &blend, ULW_ALPHA)) {
            SetWindowPos(debugReadoutHwnd_, WindowZOrder(), 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
        }
        SelectObject(dc, previous);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }

    void DrawDebugReadout(HDC dc, int width, int height) {
        const int left = UiScaled(12);
        const int top = UiScaled(10);
        const int lineHeight = UiScaled(17);
        const int chartTop = top + lineHeight * 9 + UiScaled(5);
        const int chartWidth = (std::min)(UiScaled(340), width - left * 2);
        const int chartHeight = (std::min)(
            UiScaled(86), height - chartTop - UiScaled(12));

        std::wostringstream first;
        first << std::fixed << std::setprecision(1)
              << L"FPS " << lastUpdateFps_
              << Tr(L"  目标 ") << EffectiveRequestFps();
        std::wostringstream firstB;
        firstB << Tr(L"VTS配置 ");
        if (vtsConfiguredFps_ > 0) {
            if (!vtsConfiguredMode_.empty() &&
                vtsConfiguredMode_ != L"FPS_" + std::to_wstring(vtsConfiguredFps_)) {
                firstB << vtsConfiguredMode_ << L" ";
            }
            firstB << vtsConfiguredFps_;
        } else {
            firstB << L"--";
        }
        firstB << Tr(L"  VTS实时 ");
        if (vtsApiFps_ > 0) {
            firstB << vtsApiFps_;
        } else if (apiWasConnected_ && !vtsApi_.IsConnected()) {
            firstB << Tr(L"读取失败");
        } else {
            firstB << Tr(L"未启用");
        }
        std::wostringstream second;
        second << std::fixed << std::setprecision(2)
               << Tr(L"接收 ") << lastReceiveMs_ << L"ms"
               << Tr(L"  缩放 ") << lastMapScaleMs_ << L"ms"
               << Tr(L"  上屏 ") << lastUpdateMs_ << L"ms";
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
              << Tr(L"显存 ")
              << static_cast<double>(gpuMemoryCurrentBytes_) / (1024.0 * 1024.0)
              << Tr(L" MB  内存 ")
              << static_cast<double>(processMemoryBytes_) / (1024.0 * 1024.0)
              << L"/" << systemMemoryMb_ << L" MB";

        std::wostringstream sixth;
        const auto extra = vtsApi_.GetExtraStats();
        if (vtsApi_.IsConnected() && extra.uptimeMs > 0) {
            const long long totalSec = extra.uptimeMs / 1000;
            const long long h = totalSec / 3600;
            const long long m = (totalSec % 3600) / 60;
            const long long s = totalSec % 60;
            sixth << Tr(L"VTS运行 ") << h << L":"
                  << std::setw(2) << std::setfill(L'0') << m << L":"
                  << std::setw(2) << std::setfill(L'0') << s
                  << std::setfill(L' ')
                  << Tr(L"  模型 ")
                  << (extra.modelId.empty() ? L"--" : Utf8ToWide(extra.modelId))
                  << Tr(L"  面数 ") << extra.artmeshCount
                  << Tr(L"  道具 ") << extra.itemCount
                  << L"  API " << std::fixed << std::setprecision(1) << extra.apiLatencyMs << L"ms";
        } else {
            sixth << Tr(L"VTube Studio API 未连接");
        }

        std::wostringstream seventh;
        std::wostringstream eighth;
        if (sourceWidth_ > 0 && sourceHeight_ > 0 && dibWidth_ > 0 && dibHeight_ > 0) {
            const double srcPixels = static_cast<double>(sourceWidth_) * sourceHeight_;
            const double dstPixels = static_cast<double>(dibWidth_) * dibHeight_;
            const double ratio = (std::min)(srcPixels, dstPixels) / (std::max)(srcPixels, dstPixels);
            const double pct = ratio * 100.0;
            seventh << Tr(L"渲染 ") << dibWidth_ << L"x" << dibHeight_
                    << L"  VTS " << sourceWidth_ << L"x" << sourceHeight_
                    << Tr(L"  对齐率 ") << std::fixed << std::setprecision(1)
                    << pct << L"%";

            const wchar_t* rating;
            if (pct >= 90.0) {
                rating = Tr(L"完美（推荐对齐率区间）");
            } else if (dstPixels < srcPixels) {
                // Render < VTS: downscaling, quality direction
                if (pct < 10.0) rating = Tr(L"绝顶质量");
                else if (pct < 30.0) rating = Tr(L"极高质量");
                else if (pct < 50.0) rating = Tr(L"超高质量");
                else if (pct < 70.0) rating = Tr(L"较高质量");
                else rating = Tr(L"质量");
            } else {
                // Render > VTS: upscaling, performance direction
                if (pct < 10.0) rating = Tr(L"究极性能");
                else if (pct < 30.0) rating = Tr(L"极高性能");
                else if (pct < 50.0) rating = Tr(L"超高性能");
                else if (pct < 70.0) rating = Tr(L"较高性能");
                else rating = Tr(L"性能");
            }
            eighth << Tr(L"当前性能评级：") << rating;
        } else {
            seventh << Tr(L"渲染 --  VTS --  对齐率 --");
            eighth << Tr(L"当前性能评级：--");
        }

        HFONT font = CreateFontW(
            -UiFontSize(nullptr, UiScaled(14)), 0, 0, 0, FW_SEMIBOLD,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        HGDIOBJ oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);

        const auto drawText = [this, dc, width, height](
            const std::wstring& value, int x, int y, COLORREF color) {
            RECT text{x, y, width - UiScaled(8), (std::min)(height, y + UiScaled(16))};
            if (text.right <= text.left || text.bottom <= text.top) return;
            SetTextColor(dc, color);
            DrawTextW(dc, value.c_str(), -1, &text,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
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
            drawText(Tr(L"FPS / 帧时"), left,
                chartTop - UiScaled(2), RGB(226, 239, 255));
            const int graphTop = chartTop + UiScaled(14);
            const int graphHeight = chartHeight - UiScaled(14);
            const double fpsScale = (std::max)(60.0, static_cast<double>(EffectiveRequestFps()));
            const double frameScale = 33.3;
            const auto drawSeries = [this, dc, left, chartWidth, graphTop, graphHeight](
                const std::vector<double>& values, double scale, COLORREF color) {
                if (values.size() < 2) return;
                HPEN pen = CreatePen(PS_SOLID, UiScaled(1), color);
                if (!pen) return;
                HGDIOBJ previous = SelectObject(dc, pen);
                for (size_t index = 0; index < values.size(); ++index) {
                    const double normalized = (std::clamp)(values[index] / scale, 0.0, 1.0);
                    const int x = left + static_cast<int>(
                        (index * (chartWidth - 1)) / (values.size() - 1));
                    const int y = graphTop + graphHeight - 1 -
                        static_cast<int>(normalized * (graphHeight - 1));
                    if (index == 0) MoveToEx(dc, x, y, nullptr);
                    else LineTo(dc, x, y);
                }
                SelectObject(dc, previous);
                DeleteObject(pen);
            };
            drawSeries(debugFpsHistory_, fpsScale, RGB(255, 220, 40));
            drawSeries(debugFrameMsHistory_, frameScale, RGB(255, 170, 80));
        }
        SelectObject(dc, oldFont);
        DeleteObject(font);
    }

    void DrawVtsStatusPanel(int width, int height) {
        if (statusMode_ == VtsStatusMode::Hidden || statusDismissed_ || !dibBits_) {
            return;
        }
        std::memset(dibBits_, 0, static_cast<size_t>(width) * height * 4);
        const double scale = (std::min)({
            uiScalePercent_ / 100.0, width / static_cast<double>(kMinimumWidth), height / 280.0});
        const auto scaleStatus = [scale](int pixels) {
            return (std::max)(1, static_cast<int>(std::lround(pixels * scale)));
        };
        const int panelWidth = width;
        const int panelHeight = height;
        const int left = 0;
        const int top = 0;
        const RECT panel{ left, top, left + panelWidth, top + panelHeight };
        statusExeRect_ = RECT{};
        statusBatchRect_ = RECT{};
        statusAddPathRect_ = RECT{};
        statusManualPathRect_ = RECT{};
        statusDocsRect_ = RECT{};
        statusShortcutRect_ = statusRemoveShortcutRect_ = RECT{};
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
        const int borderWidth = scaleStatus(1);
        for (int inset = 0; inset < borderWidth; ++inset) {
            for (int x = panel.left + inset; x < panel.right - inset; ++x) {
                PutPixel(x, panel.top + inset, 24, 132, 255);
                PutPixel(x, panel.bottom - 1 - inset, 24, 132, 255);
            }
            for (int y = panel.top + inset; y < panel.bottom - inset; ++y) {
                PutPixel(panel.left + inset, y, 24, 132, 255);
                PutPixel(panel.right - 1 - inset, y, 24, 132, 255);
            }
        }

        const bool canLaunch = statusMode_ == VtsStatusMode::LaunchChoices &&
            !statusDirectory_.empty();
        const bool hasExternalLauncher = canLaunch &&
            HasVtsExternalLauncher(statusDirectory_);
        const bool needsPath = statusDirectory_.empty();
        const int horizontalMargin = scaleStatus(24);
        const int buttonTop = panel.bottom - scaleStatus(94);
        const int buttonHeight = scaleStatus(42);
        const int gap = scaleStatus(14);
        const int count = 1 + (canLaunch || needsPath ? 1 : 0) + (hasExternalLauncher ? 1 : 0);
        const int shortcutGap = scaleStatus(4);
        const int clearSpace = shortcutApp_.empty() ? 0 : buttonHeight + shortcutGap;
        const int buttonWidth = (std::max)(1,
            (panelWidth - horizontalMargin * 2 - gap * (count - 1) - clearSpace) / count);
        const int first = panel.left + horizontalMargin;
        const auto launchButton = [&](int index) {
            const int x = first + index * (buttonWidth + gap);
            return RECT{x, buttonTop, x + buttonWidth, buttonTop + buttonHeight};
        };
        statusShortcutRect_ = launchButton(count - 1);
        if (shortcutHoverAction_ && !shortcutApp_.empty()) {
            const LONG clearLeft = statusShortcutRect_.right + shortcutGap;
            statusRemoveShortcutRect_ = RECT{clearLeft, buttonTop, clearLeft + buttonHeight, buttonTop + buttonHeight};
            if (shortcutHoverAction_ == 6) fill(statusRemoveShortcutRect_, 194, 48, 57, 255);
            else fill(statusRemoveShortcutRect_, 53, 78, 111, 255);
        }
        fill(statusShortcutRect_, 33, 74, 118, 255);
        if (canLaunch) {
            statusExeRect_ = launchButton(0);
            fill(statusExeRect_, 33, 74, 118, 255);
            if (hasExternalLauncher) {
                statusBatchRect_ = launchButton(1);
                fill(statusBatchRect_, 33, 74, 118, 255);
            }
        } else if (needsPath) {
            statusAddPathRect_ = launchButton(0);
            fill(statusAddPathRect_, 33, 74, 118, 255);
        }

        HFONT titleFont = CreateFontW(
            -UiFontSize(nullptr, scaleStatus(22)), 0, 0, 0,
            FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        HFONT bodyFont = CreateFontW(
            -UiFontSize(nullptr, scaleStatus(16)), 0, 0, 0,
            FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        HFONT docsFont = CreateFontW(
            -UiFontSize(nullptr, scaleStatus(14)), 0, 0, 0,
            FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        HFONT docsLinkFont = CreateFontW(
            -UiFontSize(nullptr, scaleStatus(14)), 0, 0, 0,
            FW_SEMIBOLD, FALSE, TRUE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        HGDIOBJ oldFont = SelectObject(memoryDc_, titleFont);
        SetBkMode(memoryDc_, TRANSPARENT);
        SetTextColor(memoryDc_, RGB(240, 246, 255));
        RECT titleRect{ panel.left + horizontalMargin, panel.top + scaleStatus(22),
                        panel.right - horizontalMargin, panel.top + scaleStatus(54) };
        const std::wstring title = statusMode_ == VtsStatusMode::LaunchChoices
            ? Tr(L"已找到 VTube Studio，但当前尚未运行")
            : Tr(L"等待 VTube Studio 启动中");
        DrawTextW(memoryDc_, title.c_str(), -1, &titleRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        SelectObject(memoryDc_, bodyFont);
        RECT instructionRect{
            panel.left + horizontalMargin, panel.top + scaleStatus(64),
            panel.right - horizontalMargin, panel.top + scaleStatus(124) };
        const std::wstring instruction =
            spoutNeedsChoice_ ? Tr(L"请在图形设置中选择 Spout2 发送源") : statusMode_ == VtsStatusMode::LaunchChoices
            ? Tr(L"或使用其他支持Spout2的任意应用")
            : std::wstring(Tr(L"如已启动，请在 设置 - 相机 中打开“激活 Spout2”开关，\r\n")) +
                Tr(L"将背景调整成“ColorPicker”，然后启动透明推流。");
        DrawTextW(memoryDc_, instruction.c_str(), -1, &instructionRect,
                  DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);

        // Keep the surrounding sentence passive and expose only the project
        // name as the hyperlink. At narrow widths the link moves to a second
        // line so translated text never overlaps the launch controls.
        const std::wstring docsPrefix = Tr(L"你可以访问该教程页面：");
        const std::wstring docsLink = Tr(L"GitHub-VTSFloat_Meow_DOCS");
        const std::wstring docsSuffix = Tr(L" 页面查看更多");
        SIZE prefixSize{};
        SIZE linkSize{};
        SIZE suffixSize{};
        SelectObject(memoryDc_, docsFont);
        GetTextExtentPoint32W(memoryDc_, docsPrefix.c_str(),
                              static_cast<int>(docsPrefix.size()), &prefixSize);
        GetTextExtentPoint32W(memoryDc_, docsSuffix.c_str(),
                              static_cast<int>(docsSuffix.size()), &suffixSize);
        SelectObject(memoryDc_, docsLinkFont);
        GetTextExtentPoint32W(memoryDc_, docsLink.c_str(),
                              static_cast<int>(docsLink.size()), &linkSize);
        const int docsLeft = panel.left + horizontalMargin;
        const int docsRight = panel.right - horizontalMargin;
        const int docsTop = panel.top + scaleStatus(126);
        const int docsLineHeight = scaleStatus(22);
        const int docsWidth = docsRight - docsLeft;
        const bool docsWrap = prefixSize.cx + linkSize.cx + suffixSize.cx > docsWidth;
        int docsLinkLeft = docsLeft + (docsWrap ? 0 : prefixSize.cx);
        int docsLinkTop = docsTop + (docsWrap ? docsLineHeight : 0);
        RECT docsPrefixRect{
            docsLeft, docsTop, docsRight, docsTop + docsLineHeight };
        SelectObject(memoryDc_, docsFont);
        SetTextColor(memoryDc_, RGB(164, 204, 240));
        DrawTextW(memoryDc_, docsPrefix.c_str(), -1, &docsPrefixRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        statusDocsRect_ = RECT{
            docsLinkLeft, docsLinkTop,
            (std::min)(docsRight, docsLinkLeft + static_cast<int>(linkSize.cx)),
            docsLinkTop + docsLineHeight };
        SelectObject(memoryDc_, docsLinkFont);
        SetTextColor(memoryDc_, statusPressedButton_ == 4
            ? RGB(131, 205, 255) : RGB(54, 157, 255));
        DrawTextW(memoryDc_, docsLink.c_str(), -1, &statusDocsRect_,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT docsSuffixRect{
            statusDocsRect_.right, docsLinkTop, docsRight,
            docsLinkTop + docsLineHeight };
        SelectObject(memoryDc_, docsFont);
        SetTextColor(memoryDc_, RGB(164, 204, 240));
        DrawTextW(memoryDc_, docsSuffix.c_str(), -1, &docsSuffixRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT pathRect{
            panel.left + horizontalMargin, panel.bottom - scaleStatus(42),
            panel.right - horizontalMargin, panel.bottom - scaleStatus(14) };
        const std::wstring pathText = statusDirectory_.empty()
            ? (statusPathLookupFailed_
                ? Tr(L"所选位置未找到 VTube Studio，请重新选择路径。")
                : Tr(L"尚未找到 VTube Studio 安装目录，请先安装或启动一次 VTube Studio。"))
            : std::wstring(Tr(L"VTS 安装路径：")) + statusDirectory_.wstring();
        const std::wstring manualPathText = Tr(L"手动选择路径");
        HFONT manualPathFont = CreateFontW(
            -UiFontSize(nullptr, scaleStatus(16)), 0, 0, 0,
            FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        SelectObject(memoryDc_, manualPathFont);
        SIZE manualPathSize{};
        GetTextExtentPoint32W(
            memoryDc_, manualPathText.c_str(),
            static_cast<int>(manualPathText.size()), &manualPathSize);
        const int manualLeft = (std::max)(
            pathRect.left + scaleStatus(160),
            pathRect.right - static_cast<int>(manualPathSize.cx));
        statusManualPathRect_ = RECT{
            manualLeft, pathRect.top, pathRect.right, pathRect.bottom };
        RECT pathTextRect = pathRect;
        pathTextRect.right = (std::max)(
            static_cast<int>(pathTextRect.left), manualLeft - scaleStatus(12));
        SelectObject(memoryDc_, bodyFont);
        SetTextColor(memoryDc_, RGB(164, 204, 240));
        DrawTextW(memoryDc_, pathText.c_str(), -1, &pathTextRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(memoryDc_, manualPathFont);
        SetTextColor(memoryDc_, RGB(54, 157, 255));
        DrawTextW(memoryDc_, manualPathText.c_str(), -1, &statusManualPathRect_,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        if (canLaunch) {
            SetTextColor(memoryDc_, RGB(240, 246, 255));
            const std::wstring exeLabel = Tr(L"从 Steam 启动 VTube Studio");
            const std::wstring batchLabel = Tr(L"从外部启动 VTS");
            DrawTextW(memoryDc_, exeLabel.c_str(), -1, &statusExeRect_,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (hasExternalLauncher) {
                DrawTextW(memoryDc_, batchLabel.c_str(), -1, &statusBatchRect_,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
        } else if (needsPath) {
            SetTextColor(memoryDc_, RGB(240, 246, 255));
            const std::wstring addPathLabel = Tr(L"添加启动路径");
            DrawTextW(memoryDc_, addPathLabel.c_str(), -1, &statusAddPathRect_,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        SetTextColor(memoryDc_, RGB(240, 246, 255));
        SelectObject(memoryDc_, manualPathFont);
        constexpr UINT shortcutTextFlags = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
        RECT shortcutText = statusShortcutRect_;
        InflateRect(&shortcutText, -scaleStatus(6), 0);
        if (shortcutApp_.empty()) {
            DrawTextW(memoryDc_, Tr(L"启动其他应用"), -1, &shortcutText, shortcutTextFlags | DT_CENTER);
        } else {
            const wchar_t* prefix = Tr(L"第三方");
            const std::wstring name = L"(" + shortcutApp_.stem().wstring() + L")";
            SIZE prefixSize{}, nameSize{};
            GetTextExtentPoint32W(memoryDc_, prefix, static_cast<int>(std::wcslen(prefix)), &prefixSize);
            SelectObject(memoryDc_, docsFont);
            GetTextExtentPoint32W(memoryDc_, name.c_str(), static_cast<int>(name.size()), &nameSize);
            shortcutText.left += (std::max)(0L, (shortcutText.right - shortcutText.left - prefixSize.cx - nameSize.cx) / 2);
            RECT prefixRect = shortcutText;
            prefixRect.right = (std::min)(shortcutText.right, shortcutText.left + prefixSize.cx);
            SelectObject(memoryDc_, manualPathFont);
            DrawTextW(memoryDc_, prefix, -1, &prefixRect, shortcutTextFlags | DT_LEFT);
            shortcutText.left = prefixRect.right;
            SelectObject(memoryDc_, docsFont);
            DrawTextW(memoryDc_, name.c_str(), -1, &shortcutText, shortcutTextFlags | DT_LEFT);
        }
        SelectObject(memoryDc_, docsFont);
        if (!IsRectEmpty(&statusRemoveShortcutRect_)) {
            DrawTextW(memoryDc_, L"×", -1, &statusRemoveShortcutRect_,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        SelectObject(memoryDc_, oldFont);
        DeleteObject(titleFont);
        DeleteObject(bodyFont);
        DeleteObject(docsFont);
        DeleteObject(docsLinkFont);
        DeleteObject(manualPathFont);

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
        const PDH_STATUS memoryCounterStatus = PdhAddEnglishCounterW(
            gpuUsageQuery_,
            L"\\GPU Process Memory(*)\\Dedicated Usage",
            0,
            &gpuProcessMemoryCounter_);
        if (memoryCounterStatus != ERROR_SUCCESS) {
            gpuProcessMemoryCounter_ = nullptr;
            Log("[gpu telemetry] GPU process memory counter unavailable=" +
                std::to_string(memoryCounterStatus));
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

        const DWORD processId = GetCurrentProcessId();
        const LUID expectedLuid = ActiveGpuLuid();
        processGpuMemoryBytes_ = 0;
        if (gpuProcessMemoryCounter_) {
            DWORD memoryBytes = 0;
            DWORD memoryItems = 0;
            PDH_STATUS memoryStatus = PdhGetFormattedCounterArrayW(
                gpuProcessMemoryCounter_, PDH_FMT_LARGE,
                &memoryBytes, &memoryItems, nullptr);
            if (memoryStatus == PDH_MORE_DATA && memoryBytes > 0) {
                std::vector<BYTE> memoryStorage(memoryBytes);
                auto* memoryValues =
                    reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(
                        memoryStorage.data());
                memoryStatus = PdhGetFormattedCounterArrayW(
                    gpuProcessMemoryCounter_, PDH_FMT_LARGE,
                    &memoryBytes, &memoryItems, memoryValues);
                if (memoryStatus == ERROR_SUCCESS) {
                    for (DWORD index = 0; index < memoryItems; ++index) {
                        const auto& value = memoryValues[index];
                        if (value.FmtValue.CStatus != ERROR_SUCCESS ||
                            !value.szName || value.FmtValue.largeValue <= 0) {
                            continue;
                        }
                        unsigned long pid = 0;
                        unsigned long high = 0;
                        unsigned long low = 0;
                        if (swscanf_s(
                                value.szName,
                                L"pid_%lu_luid_0x%lx_0x%lx",
                                &pid, &high, &low) != 3) {
                            continue;
                        }
                        if (pid == processId &&
                            static_cast<DWORD>(high) ==
                                static_cast<DWORD>(expectedLuid.HighPart) &&
                            static_cast<DWORD>(low) == expectedLuid.LowPart) {
                            processGpuMemoryBytes_ += static_cast<UINT64>(
                                value.FmtValue.largeValue);
                        }
                    }
                }
            }
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

        if (!locked_ || debugMode_) UpdateGpuTelemetry();
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
        vtsApiFps_ = apiOptedOut_ ? 0 : vtsApi_.GetRealtimeFps();
        const bool authorizationDenied =
            !apiOptedOut_ && vtsApi_.WasAuthorizationDenied();
        if (authorizationDenied != lastAuthorizationDenied_) {
            lastAuthorizationDenied_ = authorizationDenied;
            if (authorizationDenied) {
                KillTimer(toolbarHwnd_, 45);
                KillTimer(toolbarHwnd_, 46);
                awaitingUserApproval_ = false;
                showingApiSuccess_ = false;
            }
            PositionToolbar();
            if (toolbarHwnd_) {
                InvalidateRect(toolbarHwnd_, nullptr, FALSE);
            }
        }
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
        if (vtsApiFps_ == 0 && prevApiFps > 0 && !awaitingUserApproval_) {
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
        if (toolbarHwnd_ && !locked_) {
            InvalidateRect(toolbarHwnd_, nullptr, FALSE);
        }

        RefreshDebugReadout(true);
        requested_ = received_ = newFrames_ = updated_ = errors_ = 0;
        receiveMs_ = mapMs_ = updateMs_ = maxUpdateMs_ = 0.0;
        wakeLateMs_ = maxWakeLateMs_ = 0.0;
        statsStarted_ = Clock::now();
    }

    int UiScaled(int logicalPixels) const {
        if (logicalPixels == 0) return 0;
        return (std::max)(1, static_cast<int>(std::lround(
            logicalPixels * uiScalePercent_ / 100.0)));
    }

    int ToolbarControlsHeight() const {
        return UiScaled(toolbarLayout_.rows * kToolbarHeight) + TotalNotificationOffset();
    }

    RECT ApiOptOutRect(int width) const {
        const int top = UiScaled(1) + UpdateNotificationHeight();
        return RECT{width - (std::min)(UiScaled(340), width / 2), top,
            width - UiScaled(8), top + UiScaled(kToolbarNotificationHeight)};
    }

    void SetApiOptOut(bool disabled) {
        apiOptedOut_ = disabled;
        WriteConfigInt(L"VtsApi", L"disabled", disabled);
        for (UINT_PTR timer = 42; timer <= 48; ++timer) KillTimer(toolbarHwnd_, timer);
        apiStartScheduled_ = !disabled;
        apiStartedAfterModel_ = false;
        awaitingUserApproval_ = !disabled;
        showingApiSuccess_ = false;
        apiWasConnected_ = false;
        holdNotificationForScanResult_ = false;
        scanStartedObserved_ = false;
        if (disabled) {
            CancelHoverExpression();
            vtsApi_.RequestStop();
        } else {
            SetTimer(toolbarHwnd_, 48, 100, nullptr);
            SetTimer(toolbarHwnd_, 45, 500, nullptr);
            SetTimer(toolbarHwnd_, 46, kVtsApiAuthorizationWaitMs, nullptr);
        }
        PositionToolbar();
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }

    bool ShowApiNotification() const {
        if (locked_ || viewEditing_ || apiOptedOut_) return false;
        // During the VTS/Spout waiting page, an API disconnect is expected;
        // that page already tells the user what needs to be started.
        if (statusMode_ != VtsStatusMode::Hidden) return false;
        if (!hasReceivedModel_) return false;
        if (!apiStartedAfterModel_) return awaitingUserApproval_;
        if (vtsApi_.WasAuthorizationDenied()) return true;
        if (holdNotificationForScanResult_) return true;
        if (awaitingUserApproval_ || vtsApi_.IsAwaitingAuthorization()) return true;
        if (showingApiSuccess_) return true;
        return apiWasConnected_ && !vtsApi_.IsConnected();
    }

    int TotalNotificationOffset() const {
        return UpdateNotificationHeight() +
            (ShowApiNotification() ? UiScaled(kToolbarNotificationHeight) : 0);
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

    ToolbarLayout BuildToolbarLayout(int width) const {
        struct ButtonWidth {
            ToolbarButton button;
            int width;
        };
        constexpr int margin = 5;
        constexpr int gap = 4;
        const int available = (std::max)(1, width - 2 * margin);
        const ButtonWidth graphics{ToolbarButton::GraphicsSettings, 34};
        const ButtonWidth mode{ToolbarButton::WindowMode, 34};
        const ButtonWidth hotkey{ToolbarButton::Hotkey, 195};
        const ButtonWidth aspect{ToolbarButton::Aspect, 34};
        const ButtonWidth scale{ToolbarButton::CropUiScale, 34};
        const ButtonWidth language{ToolbarButton::Language, 34};
        const ButtonWidth crop{ToolbarButton::Crop, 130};
        const ButtonWidth style{ToolbarButton::Border, 88};
        const ButtonWidth done{ToolbarButton::Lock, 70};
        const ButtonWidth connect{ToolbarButton::ConnectApi, apiOptedOut_ && spoutFrameAvailable_ ? 228 : 0};
        const ButtonWidth appTitle{ToolbarButton::AppTitle,
            (std::clamp)(available - language.width - scale.width - mode.width - aspect.width - gap * 4, 1, kToolbarAppTitleWidth)};
        const int leftEdge = margin + appTitle.width + language.width + scale.width + mode.width + aspect.width + gap * 5;
        const std::array left{graphics, hotkey, crop, style};
        const std::array right{ButtonWidth{ToolbarButton::Close, 46},
            ButtonWidth{ToolbarButton::Hide, 46}, ButtonWidth{ToolbarButton::Github, 46},
            done, connect};
        const std::array compactRight{ButtonWidth{ToolbarButton::Close, 40},
            ButtonWidth{ToolbarButton::Hide, 40}, ButtonWidth{ToolbarButton::Github, 40},
            done, connect};
        const ButtonWidth compactStyle{ToolbarButton::Border, 82};
        const std::array compactSettings{graphics, crop, compactStyle};
        ToolbarLayout layout{};
        const auto place = [&layout](const ButtonWidth& item, int x, int row, int buttonWidth) {
            const int top = row * kToolbarHeight;
            layout.buttons[static_cast<size_t>(item.button)] =
                RECT{x, top + 5, x + buttonWidth, top + kToolbarHeight - 5};
        };
        place(appTitle, margin, 0, appTitle.width);
        const int languageLeft = margin + appTitle.width + gap;
        place(language, languageLeft, 0, language.width);
        const int scaleLeft = languageLeft + language.width + gap;
        place(scale, scaleLeft, 0, scale.width);
        const int modeLeft = scaleLeft + scale.width + gap;
        place(mode, modeLeft, 0, mode.width);
        place(aspect, modeLeft + mode.width + gap, 0, aspect.width);
        const auto placeRight = [&](const auto& items) {
            int x = width - margin;
            int row = 0;
            for (const auto& item : items) {
                if (!item.width) continue;
                const int buttonWidth = (std::min)(item.width, available);
                const int left = row == 0 ? leftEdge : margin;
                const int followingWidth = item.button == ToolbarButton::Lock && connect.width
                    ? (std::min)(connect.width, (std::max)(0, available - buttonWidth - gap)) + gap : 0;
                if (x - buttonWidth - followingWidth < left) {
                    x = width - margin;
                    ++row;
                }
                x -= buttonWidth;
                place(item, x, row, buttonWidth);
                x -= gap;
            }
            layout.rows = row + 1;
            return x;
        };
        if (width >= kToolbarWideMinimumWidth + connect.width) {
            int x = leftEdge;
            for (const auto& item : left) {
                if (!item.width) continue;
                place(item, x, 0, item.width);
                x += item.width + gap;
            }
            placeRight(right);
            return layout;
        }

        const auto placeRows = [&](const auto& items, int row) {
            std::array<ButtonWidth, 11> pending{};
            size_t count = 0;
            int used = 0;
            const auto finishRow = [&]() {
                int x = (width - used) / 2;
                for (size_t i = 0; i < count; ++i) {
                    const auto& item = pending[i];
                    place(item, x, row, item.width);
                    x += item.width + gap;
                }
                ++row;
                count = 0;
                used = 0;
            };
            for (auto item : items) {
                if (!item.width) continue;
                item.width = (std::min)(item.width, available);
                if (count && used + gap + item.width > available) finishRow();
                if (count) used += gap;
                pending[count++] = item;
                used += item.width;
            }
            if (count) finishRow();
            layout.rows = row;
        };

        const int rightEdge = placeRight(compactRight);
        const int hotkeyWidth = rightEdge - leftEdge;
        if (layout.rows == 1 && hotkeyWidth >= hotkey.width) {
            place(hotkey, leftEdge, 0, hotkeyWidth);
            placeRows(compactSettings, 1);
        } else {
            placeRows(std::array{graphics, hotkey, crop, compactStyle},
                layout.rows);
        }
        return layout;
    }

    std::wstring CropSourceLabel() const {
        return std::wstring(Tr(L"原始分辨率")) + L"  " +
            std::to_wstring(sourceWidth_) + L" × " + std::to_wstring(sourceHeight_);
    }

    const wchar_t* CropAspectLabel(bool locked) const {
        return locked ? Tr(L"裁剪比例已锁定") : Tr(L"锁定裁剪比例");
    }

    CropToolbarLayout BuildCropToolbarLayout(int width) const {
        CropToolbarLayout layout{};
        const int margin = UiScaled(8);
        const int gap = UiScaled(6);
        const int groupGap = UiScaled(14);
        const int rowHeight = UiScaled(kToolbarHeight);
        const int available = (std::max)(1, width - 2 * margin);
        HDC dc = GetDC(toolbarHwnd_);
        const HFONT font = CreateFontW(-UiFontSize(toolbarHwnd_, UiScaled(15)), 0, 0, 0, FW_SEMIBOLD,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        const HGDIOBJ previous = font ? SelectObject(dc, font) : nullptr;
        const auto measure = [&](const std::wstring& label) {
            SIZE size{};
            GetTextExtentPoint32W(dc, label.c_str(), static_cast<int>(label.size()), &size);
            return static_cast<int>(size.cx) + UiScaled(2);
        };
        const auto button = [&](const std::wstring& label, int minimum) {
            return (std::max)(measure(label) + UiScaled(16), UiScaled(minimum));
        };
        const int appTitleWidth = UiScaled(kToolbarAppTitleWidth);
        const int titleWidth = button(Tr(L"画面编辑菜单-"), 86);
        const int aspectWidth = (std::max)(button(CropAspectLabel(false), 80), button(CropAspectLabel(true), 80));
        const int cropWidth = button(Tr(L"裁剪"), 80);
        const int imageWidth = button(Tr(L"图像"), 80);
        const int blendWidth = button(Tr(L"画面混合"), 100);
        const int shadowWidth = button(Tr(L"阴影"), 72);
        const int doneWidth = button(Tr(L"完成"), 64);
        const int settingsWidth = button(Tr(L"羽化与旋转"), 98);
        const int restoreWidth = button(Tr(L"重置"), 64);
        const int cancelWidth = button(Tr(L"取消"), 64);
        const int saveWidth = button(Tr(L"保存"), 64);
        if (previous) SelectObject(dc, previous);
        if (font) DeleteObject(font);
        ReleaseDC(toolbarHwnd_, dc);

        struct Item { RECT& rect; int width; };
        int x = margin;
        int row = 0;
        const auto place = [&](std::initializer_list<Item> items, bool right = false) {
            int naturalWidth = gap * (static_cast<int>(items.size()) - 1);
            for (const auto& item : items) naturalWidth += item.width;
            const int groupWidth = (std::min)(naturalWidth, available);
            if (x + groupWidth > margin + available && x > margin) {
                x = margin;
                ++row;
            }
            if (right) x = margin + available - groupWidth;
            int offset = 0;
            for (const auto& item : items) {
                const int left = x + MulDiv(offset, groupWidth, naturalWidth);
                offset += item.width;
                const int rightEdge = x + MulDiv(offset, groupWidth, naturalWidth);
                item.rect = RECT{left, row * rowHeight + UiScaled(5),
                    rightEdge, (row + 1) * rowHeight - UiScaled(5)};
                offset += gap;
            }
            x += groupWidth + groupGap;
        };
        place({{layout.appTitle, appTitleWidth}});
        place({{layout.title, titleWidth}});
        if (cropEditing_) {
            place({{layout.aspect, aspectWidth}});
            place({{layout.restore, restoreWidth}, {layout.cancel, cancelWidth}, {layout.save, saveWidth}}, true);
        } else {
            place({{layout.crop, cropWidth}, {layout.image, imageWidth}});
            place({{layout.blend, blendWidth}});
            place({{layout.shadow, shadowWidth}});
            place({{layout.settings, settingsWidth}});
            place({{layout.done, doneWidth}}, true);
        }
        layout.rows = row + 1;
        return layout;
    }

    int ViewSettingsWarningHeight() const {
        return viewEditing_ && !viewSettingsWarningDismissed_ ? UiScaled(kToolbarNotificationHeight) : 0;
    }

    RECT ViewSettingsWarningActionRect(int width) const {
        const LONG right = (std::max)(0, width - UiScaled(8));
        return {right - (std::min)(UiScaled(170), width / 2), 0, right, ViewSettingsWarningHeight()};
    }

    int ToolbarCurrentHeight() const {
        const int noticeHeight = TotalNotificationOffset();
        if (viewEditing_) return cropToolbarLayout_.rows * UiScaled(kToolbarHeight) + ViewSettingsWarningHeight();
        if (locked_) {
            return noticeHeight + (debugMode_ || noticeHeight == 0
                ? UiScaled(kToolbarLockedHeight) : 2 * UiScaled(1));
        }
        const int baseHeight = debugMode_ ? kToolbarDebugModeHeight : kToolbarDebugHeight;
        const int extraHeight = toolbarLayout_.rows > 1
            ? (toolbarLayout_.rows - 1) * kToolbarHeight + 12 : 0;
        return UiScaled(baseHeight + extraHeight) + noticeHeight;
    }

    RECT ToolbarButtonRect(ToolbarButton button) const {
        const int offset = TotalNotificationOffset();
        if (viewEditing_) {
            const auto offsetWarning = [this](RECT rect) {
                if (!IsRectEmpty(&rect)) OffsetRect(&rect, 0, ViewSettingsWarningHeight());
                return rect;
            };
            switch (button) {
            case ToolbarButton::ViewCrop: return offsetWarning(cropToolbarLayout_.crop);
            case ToolbarButton::ViewImage: return offsetWarning(cropToolbarLayout_.image);
            case ToolbarButton::ViewBlend: return offsetWarning(cropToolbarLayout_.blend);
            case ToolbarButton::ViewShadow: return offsetWarning(cropToolbarLayout_.shadow);
            case ToolbarButton::ViewDone: return offsetWarning(cropToolbarLayout_.done);
            case ToolbarButton::CropAspect: return offsetWarning(cropToolbarLayout_.aspect);
            case ToolbarButton::ViewSettings: return offsetWarning(cropToolbarLayout_.settings);
            case ToolbarButton::AppTitle: return offsetWarning(cropToolbarLayout_.appTitle);
            case ToolbarButton::CropRestore: return offsetWarning(cropToolbarLayout_.restore);
            case ToolbarButton::CropCancel: return offsetWarning(cropToolbarLayout_.cancel);
            case ToolbarButton::CropSave: return offsetWarning(cropToolbarLayout_.save);
            default: return {};
            }
        }
        if (locked_) {
            if (button != ToolbarButton::Unlock || !debugMode_) return {};
            RECT client{};
            GetClientRect(toolbarHwnd_, &client);
            return RECT{UiScaled(1), offset + UiScaled(1),
                (std::max)(1L, client.right - UiScaled(1) - UiScaled(kLockedDebugLabelWidth)),
                offset + UiScaled(kToolbarLockedHeight - 1)};
        }
        const RECT logical = toolbarLayout_.buttons[static_cast<size_t>(button)];
        if (IsRectEmpty(&logical)) return {};
        return RECT{UiScaled(logical.left), offset + UiScaled(logical.top),
            UiScaled(logical.right), offset + UiScaled(logical.bottom)};
    }

    static const wchar_t* ToolbarHintLabel(ToolbarButton button) {
        switch (button) {
        case ToolbarButton::WindowMode: return Tr(L"窗口模式");
        case ToolbarButton::Aspect: return Tr(L"比例锁定");
        case ToolbarButton::CropUiScale: return Tr(L"界面缩放");
        case ToolbarButton::Language: return L"Language";
        case ToolbarButton::GraphicsSettings: return Tr(L"图形设置");
        default: return nullptr;
        }
    }

    void HideToolbarHint() {
        toolbarHintButton_ = ToolbarButton::None;
        if (toolbarHintHwnd_ && IsWindowVisible(toolbarHintHwnd_)) ShowWindow(toolbarHintHwnd_, SW_HIDE);
    }

    void PositionToolbarHint() {
        if (toolbarHintButton_ == ToolbarButton::None) return;
        if (locked_ || !overlayVisible_ || updateClosing_ || !IsWindowVisible(toolbarHwnd_)) {
            HideToolbarHint();
            return;
        }
        RECT anchor = ToolbarButtonRect(toolbarHintButton_);
        MapWindowPoints(toolbarHwnd_, nullptr, reinterpret_cast<POINT*>(&anchor), 2);
        const wchar_t* label = ToolbarHintLabel(toolbarHintButton_);
        const HDC dc = GetDC(toolbarHintHwnd_);
        const HFONT font = CreateFontW(-UiScaled(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_NATURAL_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        const HGDIOBJ previous = font ? SelectObject(dc, font) : nullptr;
        SIZE text{};
        GetTextExtentPoint32W(dc, label, static_cast<int>(wcslen(label)), &text);
        if (previous) SelectObject(dc, previous);
        if (font) DeleteObject(font);
        ReleaseDC(toolbarHintHwnd_, dc);
        const auto popup = ToolbarPopoverBounds(anchor, text.cx + UiScaled(24), UiScaled(28));
        if (!popup) {
            HideToolbarHint();
            return;
        }
        SetWindowPos(toolbarHintHwnd_, WindowZOrder(), popup->left, popup->top,
            popup->right - popup->left, popup->bottom - popup->top,
            SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        InvalidateRect(toolbarHintHwnd_, nullptr, FALSE);
    }

    void ShowToolbarHint(ToolbarButton button) {
        const wchar_t* label = ToolbarHintLabel(button);
        if (!label || toolbarPressed_ != ToolbarButton::None) {
            HideToolbarHint();
            return;
        }
        if (toolbarHintButton_ == button && IsWindowVisible(toolbarHintHwnd_)) return;
        HideGithubDropdown();
        if (!toolbarHintHwnd_) {
            toolbarHintHwnd_ = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
                kToolbarHintClass, L"", WS_POPUP, 0, 0, 1, 1, toolbarHwnd_, nullptr, instance_, this);
            if (!toolbarHintHwnd_) return;
        }
        toolbarHintButton_ = button;
        SetWindowTextW(toolbarHintHwnd_, label);
        PositionToolbarHint();
    }

    static LRESULT CALLBACK ToolbarHintProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* overlay = reinterpret_cast<LayeredOverlay*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            overlay = static_cast<LayeredOverlay*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
        }
        if (!overlay) return DefWindowProcW(window, message, wParam, lParam);
        switch (message) {
        case WM_NCHITTEST: return HTTRANSPARENT;
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            const HDC dc = BeginPaint(window, &paint);
            RECT bounds{};
            GetClientRect(window, &bounds);
            PaintToolbarPopoverBackground(dc, bounds);
            InflateRect(&bounds, -overlay->UiScaled(6), 0);
            if (const auto* label = ToolbarHintLabel(overlay->toolbarHintButton_)) {
                PanelText(dc, label, bounds, kToolbarPopoverText, overlay->UiScaled(14),
                    false, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            }
            EndPaint(window, &paint);
            return 0;
        }
        case WM_NCDESTROY:
            overlay->toolbarHintHwnd_ = nullptr;
            overlay->toolbarHintButton_ = ToolbarButton::None;
            break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    void LoadToolbarToggleIcons(int size) {
        if (toolbarToggleIconSize_ == size) return;
        constexpr int resources[]{IDI_TOOLBAR_FLOATING, IDI_TOOLBAR_DESKTOP, IDI_TOOLBAR_ASPECT_LOCKED,
            IDI_TOOLBAR_ASPECT_FREE, IDI_TOOLBAR_UI_SCALE, IDI_TOOLBAR_LANGUAGE, IDI_TOOLBAR_GRAPHICS};
        for (size_t index = 0; index < toolbarToggleIcons_.size(); ++index) {
            auto& icon = toolbarToggleIcons_[index];
            if (icon) DestroyIcon(icon);
            icon = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(resources[index]),
                IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
        }
        toolbarToggleIconSize_ = size;
    }

    bool CropAvailable() const {
        return hasReceivedModel_ && statusMode_ == VtsStatusMode::Hidden &&
            sourceWidth_ && sourceHeight_;
    }

    void SetSpoutFrameAvailable(bool available) {
        if (GetCurrentThreadId() != uiThreadId_ || spoutFrameAvailable_ == available) return;
        spoutFrameAvailable_ = available;
        if (!available && toolbarGuideHwnd_) DestroyWindow(toolbarGuideHwnd_);
        if (apiOptedOut_) PositionToolbar();
        if (toolbarHwnd_) InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }

    ToolbarButton HitTestToolbar(POINT point) const {
        if (viewEditing_) {
            for (auto button : {ToolbarButton::ViewCrop, ToolbarButton::ViewImage, ToolbarButton::ViewBlend, ToolbarButton::ViewShadow, ToolbarButton::ViewDone,
                                ToolbarButton::CropAspect, ToolbarButton::ViewSettings, ToolbarButton::CropRestore,
                                ToolbarButton::CropCancel, ToolbarButton::CropSave}) {
                const RECT rect = ToolbarButtonRect(button);
                if (PtInRect(&rect, point)) return button;
            }
            return ToolbarButton::None;
        }
        if (locked_) {
            RECT unlock = ToolbarButtonRect(ToolbarButton::Unlock);
            return PtInRect(&unlock, point) ? ToolbarButton::Unlock : ToolbarButton::None;
        }
        constexpr ToolbarButton buttons[] = {
            ToolbarButton::CropUiScale,
            ToolbarButton::GraphicsSettings,
            ToolbarButton::Aspect,
            ToolbarButton::Crop,
            ToolbarButton::Border,
            ToolbarButton::WindowMode,
            ToolbarButton::Hotkey,
            ToolbarButton::Language,
            ToolbarButton::ConnectApi,
            ToolbarButton::Lock,
            ToolbarButton::Github,
            ToolbarButton::Hide,
            ToolbarButton::Close,
        };
        for (ToolbarButton button : buttons) {
            if ((button == ToolbarButton::Crop && !CropAvailable()) ||
                (button == ToolbarButton::Lock && !spoutFrameAvailable_)) continue;
            // While the status page is waiting for VTS/Spout, the overlay
            // must not be hidden through the toolbar. The user can still
            // close it with X, or hide/show it from the tray double-click.
            if (button == ToolbarButton::Hide &&
                statusMode_ != VtsStatusMode::Hidden && !statusDismissed_) {
                continue;
            }
            RECT rect = ToolbarButtonRect(button);
            if (PtInRect(&rect, point)) {
                return button;
            }
        }
        return ToolbarButton::None;
    }

    std::array<int, 4> ApiNotificationActionWidths() const {
        HDC dc = GetDC(toolbarHwnd_);
        if (!dc) {
            return { UiScaled(90), UiScaled(85), UiScaled(90), UiScaled(150) };
        }
        HFONT font = CreateFontW(
            -UiFontSize(nullptr, UiScaled(15)), 0, 0, 0, FW_SEMIBOLD,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        HGDIOBJ previous = SelectObject(dc, font);
        const auto width = [this, dc](const wchar_t* value, int minimum) {
            SIZE size{};
            GetTextExtentPoint32W(dc, value, static_cast<int>(wcslen(value)), &size);
            return (std::max)(UiScaled(minimum),
                static_cast<int>(size.cx) + UiScaled(16));
        };
        const int scan = width(Tr(L"[扫描端口]"), 90);
        const int how = width(Tr(L"[如何开启]"), 85);
        const int retryAuthorization = width(Tr(L"[重新发起授权]"), 150);
        int result = UiScaled(90);
        for (const std::wstring value : {
                 std::wstring(Tr(L"扫描中")) + L"...",
                 std::wstring(Tr(L"扫描成功")),
                 std::wstring(Tr(L"获取失败")) }) {
            result = (std::max)(result, width(value.c_str(), 90));
        }
        SelectObject(dc, previous);
        DeleteObject(font);
        ReleaseDC(toolbarHwnd_, dc);
        return { scan, how, result, retryAuthorization };
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

        const int notificationHeight = UiScaled(kToolbarNotificationHeight);
        const int notifY = UiScaled(1) + UpdateNotificationHeight();
        if (ShowApiNotification()) {
            const bool authorizationDenied =
                vtsApi_.WasAuthorizationDenied();
            const bool awaitingAuthorization =
                !authorizationDenied &&
                (awaitingUserApproval_ || vtsApi_.IsAwaitingAuthorization());
            RECT notifRect{ UiScaled(1), notifY,
                client.right - UiScaled(1), notifY + notificationHeight };
            HBRUSH notifBg = CreateSolidBrush(
                authorizationDenied ? RGB(58, 22, 22) :
                ((awaitingAuthorization || showingApiSuccess_)
                    ? RGB(15, 50, 22) : RGB(50, 30, 10)));
            FillRect(buffer, &notifRect, notifBg);
            DeleteObject(notifBg);
            HFONT notifFont = CreateFontW(
                -UiFontSize(nullptr, UiScaled(15)), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
            HGDIOBJ oldNotifFont = SelectObject(buffer, notifFont);
            SetBkMode(buffer, TRANSPARENT);
            SetTextColor(buffer,
                authorizationDenied ? RGB(255, 145, 145) :
                ((awaitingAuthorization || showingApiSuccess_)
                    ? RGB(130, 255, 150) : RGB(255, 200, 100)));
            const bool disconnected = apiWasConnected_ && !vtsApi_.IsConnected();
            std::wstring notifBuf;
            const wchar_t* notifText;
            if (showingApiSuccess_) {
                notifText = Tr(L"添加成功！");
            } else if (authorizationDenied) {
                notifText = Tr(L"你刚刚拒绝了VTSFloat_Meow");
            } else if (awaitingAuthorization) {
                const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                    Clock::now().time_since_epoch()).count();
                const int dotCount = static_cast<int>(seconds % 4);
                notifBuf = vtsApi_.IsAwaitingAuthorization()
                    ? Tr(L"请在 VTube Studio 中授权插件")
                    : Tr(L"正在连接 VTube Studio API");
                for (int i = 0; i < dotCount; ++i) notifBuf += L".";
                notifText = notifBuf.c_str();
            } else if (disconnected) {
                notifText = Tr(L"VTube Studio API 连接已断开");
            } else {
                notifText = Tr(L"VTube Studio API 未连接");
            }
            if (awaitingAuthorization || showingApiSuccess_) {
                RECT textRect{ 0, notifY, client.right, notifY + notificationHeight };
                if (awaitingAuthorization && !showingApiSuccess_) {
                    RECT action = ApiOptOutRect(client.right);
                    textRect.left = UiScaled(8);
                    textRect.right = action.left - UiScaled(8);
                    PanelText(buffer, Tr(L"我不想连接VTube Studio API进行使用"), action, RGB(255, 255, 255),
                        UiScaled(13), false, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
                    SetTextColor(buffer, RGB(130, 255, 150));
                }
                if (!vtsfloat::DrawFittedToolbarText(buffer, textRect, notifText, UiFontFace(),
                        UiScaled(15), UiScaled(10), false)) {
                    DrawTextW(buffer, notifText, -1, &textRect,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
            } else {
                const auto [scanWidth, howWidth, resultWidth,
                            retryAuthorizationWidth] =
                    ApiNotificationActionWidths();
                const int howRight = client.right - UiScaled(5);
                const int howLeft = howRight - howWidth;
                const int scanRight = howLeft - UiScaled(5);
                const int scanLeft = scanRight - scanWidth;
                const int retryAuthorizationRight = scanLeft - UiScaled(5);
                const int retryAuthorizationLeft =
                    retryAuthorizationRight - retryAuthorizationWidth;
                const int firstActionLeft = authorizationDenied
                    ? retryAuthorizationLeft : scanLeft;
                const int resultRight = firstActionLeft - UiScaled(5);
                const int resultLeft = resultRight - resultWidth;
                const bool showScanStatus = vtsApi_.IsScanning() ||
                    vtsApi_.GetScanResult() != 0;
                const int textRight = showScanStatus
                    ? resultLeft - UiScaled(5)
                    : firstActionLeft - UiScaled(5);
                RECT textRect{ UiScaled(8), notifY, textRight,
                    notifY + notificationHeight };
                DrawTextW(buffer, notifText, -1, &textRect,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                const int scanResult = vtsApi_.GetScanResult();
                if (vtsApi_.IsScanning()) {
                    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                        Clock::now().time_since_epoch()).count();
                    const int dotCount = static_cast<int>(seconds % 4);
                    std::wstring scanText = Tr(L"扫描中");
                    for (int i = 0; i < dotCount; ++i) scanText += L".";
                    SetTextColor(buffer, RGB(255, 220, 130));
                    RECT scanRect{ resultLeft, notifY, resultRight,
                        notifY + notificationHeight };
                    DrawTextW(buffer, scanText.c_str(), -1, &scanRect,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                } else if (scanResult == 1) {
                    SetTextColor(buffer, RGB(130, 255, 150));
                    RECT scanRect{ resultLeft, notifY, resultRight,
                        notifY + notificationHeight };
                    DrawTextW(buffer, Tr(L"扫描成功"), -1, &scanRect,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                } else if (scanResult == 2) {
                    SetTextColor(buffer, RGB(255, 130, 130));
                    RECT scanRect{ resultLeft, notifY, resultRight,
                        notifY + notificationHeight };
                    DrawTextW(buffer, Tr(L"获取失败"), -1, &scanRect,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
                SetTextColor(buffer, RGB(100, 180, 255));
                if (authorizationDenied) {
                    RECT retryAuthorizationRect{
                        retryAuthorizationLeft, notifY,
                        retryAuthorizationRight, notifY + notificationHeight };
                    DrawTextW(buffer, Tr(L"[重新发起授权]"), -1,
                        &retryAuthorizationRect,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE |
                        DT_END_ELLIPSIS);
                }
                RECT scanBtnRect{ scanLeft, notifY, scanRight,
                    notifY + notificationHeight };
                DrawTextW(buffer, Tr(L"[扫描端口]"), -1, &scanBtnRect,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                RECT howRect{ howLeft, notifY, howRight,
                    notifY + notificationHeight };
                DrawTextW(buffer, Tr(L"[如何开启]"), -1, &howRect,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            SelectObject(buffer, oldNotifFont);
            DeleteObject(notifFont);
        }

        HFONT font = CreateFontW(
            -UiFontSize(toolbarHwnd_, UiScaled(15)), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        HFONT closeFont = CreateFontW(
            -UiFontSize(toolbarHwnd_, UiScaled(25)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
        HGDIOBJ oldFont = SelectObject(buffer, font);
        SetBkMode(buffer, TRANSPARENT);
        SetTextColor(buffer, RGB(230, 239, 252));

        const auto drawButton = [&](ToolbarButton button, const std::wstring& label) {
            RECT rect = ToolbarButtonRect(button);
            const bool disabled = (button == ToolbarButton::Hide &&
                statusMode_ != VtsStatusMode::Hidden && !statusDismissed_) ||
                (button == ToolbarButton::Crop && !CropAvailable()) ||
                (button == ToolbarButton::Lock && !spoutFrameAvailable_);
            const bool active = (button == ToolbarButton::CropAspect && cropAspectLocked_) ||
                (button == ToolbarButton::ViewSettings && featherPanelHwnd_) ||
                (button == ToolbarButton::ViewShadow && shadowPanelHwnd_) ||
                (button == ToolbarButton::WindowMode && desktopMode_) ||
                (button == ToolbarButton::Aspect && aspectLocked_);
            const bool primary = button == ToolbarButton::Lock || button == ToolbarButton::CropSave || active;
            COLORREF color = RGB(20, 36, 58);
            if (button == ToolbarButton::CropSave || active) color = RGB(31, 87, 142);
            else if (primary) color = RGB(33, 49, 72);
            if (disabled) color = RGB(14, 25, 40);
            if (!disabled && button == toolbarHovered_) {
                if (button == ToolbarButton::Close) {
                    color = RGB(205, 55, 62);
                } else if (primary) {
                    color = RGB(41, 91, 151);
                } else {
                    color = RGB(26, 70, 116);
                }
            }
            if (!disabled && button == toolbarPressed_) {
                if (button == ToolbarButton::Close) {
                    color = RGB(170, 35, 42);
                } else if (primary) {
                    color = RGB(29, 72, 123);
                } else {
                    color = RGB(18, 50, 88);
                }
            }
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(buffer, &rect, brush);
            DeleteObject(brush);
            if (toolbarGuideHwnd_ && button == ToolbarGuideTarget()) {
                HBRUSH highlight = CreateSolidBrush(RGB(71, 153, 231));
                FrameRect(buffer, &rect, highlight);
                DeleteObject(highlight);
            }
            SetTextColor(buffer, disabled ? RGB(105, 126, 151) : RGB(240, 246, 255));
            HGDIOBJ previousFont = nullptr;
            if (button == ToolbarButton::Close) {
                previousFont = SelectObject(buffer, closeFont);
            }
            if ((button == ToolbarButton::Hotkey && capturingHotkey_) || cropEditing_ ||
                button == ToolbarButton::Crop ||
                button == ToolbarButton::ViewSettings || button == ToolbarButton::ViewShadow) {
                RECT prompt = rect;
                InflateRect(&prompt, -UiScaled(4), 0);
                if (vtsfloat::DrawFittedToolbarText(buffer, prompt, label,
                        UiFontFace(), UiScaled(15), UiScaled(11))) return;
            }
            DrawTextW(buffer, label.c_str(), -1, &rect,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (previousFont) {
                SelectObject(buffer, previousFont);
            }
        };

        const auto drawGithubButton = [&]() {
            drawButton(ToolbarButton::Github, L"");
            if (!githubGraphicsToken_) return;
            const RECT rect = ToolbarButtonRect(ToolbarButton::Github);
            const LONG size = (std::min)({static_cast<LONG>(UiScaled(32)),
                rect.right - rect.left, rect.bottom - rect.top});
            if (size <= 0) return;
            const LONG left = rect.left + (rect.right - rect.left - size) / 2;
            const LONG top = rect.top + (rect.bottom - rect.top - size) / 2;
            vtsfloat::DrawGithubMark(buffer, RECT{left, top, left + size, top + size});
        };

        const RECT appTitle = ToolbarButtonRect(ToolbarButton::AppTitle);
        if (!IsRectEmpty(&appTitle)) {
            PanelText(buffer, kToolbarAppName, appTitle, RGB(91, 134, 190), UiScaled(kToolbarAppTitleFontHeight), true,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }

        if (viewEditing_) {
            const auto label = [&](const std::wstring& text, RECT bounds, COLORREF color) {
                SetTextColor(buffer, color);
                DrawTextW(buffer, text.c_str(), -1, &bounds,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            };
            const int warningHeight = ViewSettingsWarningHeight();
            if (warningHeight > 0) {
                PanelFill(buffer, RECT{1, 1, client.right - 1, warningHeight}, RGB(58, 36, 12));
                const RECT action = ViewSettingsWarningActionRect(client.right);
                PanelText(buffer, Tr(L"进行画面设置时可能会增加性能开销"),
                    RECT{UiScaled(8), 0, action.left - UiScaled(4), warningHeight}, RGB(255, 198, 99),
                    UiScaled(13), true, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                PanelText(buffer, Tr(L"[不再提示]"), action, RGB(255, 221, 160), UiScaled(13),
                    false, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            }
            RECT title = cropToolbarLayout_.title;
            OffsetRect(&title, 0, warningHeight);
            label(Tr(L"画面编辑菜单-"), title, RGB(240, 246, 255));
            if (cropEditing_) {
                drawButton(ToolbarButton::CropAspect, CropAspectLabel(cropAspectLocked_));
                drawButton(ToolbarButton::CropRestore, Tr(L"重置"));
                drawButton(ToolbarButton::CropCancel, Tr(L"取消"));
                drawButton(ToolbarButton::CropSave, Tr(L"保存"));
            } else {
                drawButton(ToolbarButton::ViewCrop, Tr(L"裁剪"));
                drawButton(ToolbarButton::ViewImage, Tr(L"图像"));
                drawButton(ToolbarButton::ViewBlend, Tr(L"画面混合"));
                drawButton(ToolbarButton::ViewShadow, Tr(L"阴影"));
                drawButton(ToolbarButton::ViewSettings, Tr(L"羽化与旋转"));
                drawButton(ToolbarButton::ViewDone, Tr(L"完成"));
            }
        } else if (locked_ && debugMode_) {
            drawButton(ToolbarButton::Unlock, Tr(L"解锁"));
            RECT debugLabel{
                client.right - UiScaled(kLockedDebugLabelWidth),
                UiScaled(1) + UpdateNotificationHeight(),
                client.right - UiScaled(1),
                client.bottom - UiScaled(1) };
            SetTextColor(buffer, RGB(157, 210, 255));
            DrawTextW(buffer, Tr(L"调试模式已打开"), -1, &debugLabel,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else if (!locked_) {
            drawButton(
                ToolbarButton::Hotkey,
                capturingHotkey_
                    ? Tr(L"请按组合键（Esc取消）")
                    : std::wstring(Tr(L"快捷键")) + L" " + HotkeyText());
            drawButton(ToolbarButton::Crop, Tr(L"画面设置"));
            drawButton(ToolbarButton::Border, Tr(L"个性化"));
            const int toggleSize = UiScaled(20);
            LoadToolbarToggleIcons(toggleSize);
            const auto drawToggle = [&](ToolbarButton button, HICON icon) {
                drawButton(button, L"");
                const RECT rect = ToolbarButtonRect(button);
                if (!icon || IsRectEmpty(&rect)) return;
                const int size = (std::min)({toggleSize, static_cast<int>(rect.right - rect.left),
                    static_cast<int>(rect.bottom - rect.top)});
                DrawIconEx(buffer, (rect.left + rect.right - size) / 2, (rect.top + rect.bottom - size) / 2,
                    icon, size, size, 0, nullptr, DI_NORMAL);
            };
            drawToggle(ToolbarButton::WindowMode, toolbarToggleIcons_[desktopMode_ ? 1 : 0]);
            drawToggle(ToolbarButton::CropUiScale, toolbarToggleIcons_[4]);
            drawToggle(ToolbarButton::Language, toolbarToggleIcons_[5]);
            drawToggle(ToolbarButton::GraphicsSettings, toolbarToggleIcons_[6]);
            drawToggle(ToolbarButton::Aspect, toolbarToggleIcons_[aspectLocked_ ? 2 : 3]);
            RECT apiConnect = ToolbarButtonRect(ToolbarButton::ConnectApi);
            if (!IsRectEmpty(&apiConnect)) {
                PanelText(buffer, Tr(L"连接VTube Studio API"), apiConnect,
                    toolbarHovered_ == ToolbarButton::ConnectApi ? RGB(255, 255, 255) : RGB(171, 183, 198),
                    UiScaled(13), false, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            }
            drawButton(ToolbarButton::Lock, Tr(L"完成"));
            drawGithubButton();
            drawButton(ToolbarButton::Hide, L"—");
            drawButton(ToolbarButton::Close, L"×");
        }

        if (!locked_ && !viewEditing_) {
            HFONT debugFont = CreateFontW(
                -UiFontSize(toolbarHwnd_, UiScaled(13)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
            SelectObject(buffer, debugFont);
            SetTextColor(
                buffer,
                capturingFps_ ? RGB(255, 214, 102) : RGB(157, 210, 255));
            RECT debugRect{
                UiScaled(8), ToolbarControlsHeight(),
                (std::max)(static_cast<LONG>(UiScaled(8)), client.right - UiScaled(6)),
                client.bottom - UiScaled(2) };
            const std::wstring status = capturingFps_
                ? Tr(L"直接输入 1–240；Enter 确认，Esc 取消（确认前不会修改当前帧率）")
                : DebugStatusText(toolbarLayout_.rows > 1);
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
                Tr(L"这个快捷键已被其他程序占用，请换一个组合。"),
                kWindowTitle,
                MB_OK | MB_ICONWARNING);
        } else {
            SaveHotkeySettings();
            Log("[layered] hotkey=" + WideToUtf8(HotkeyText().c_str()));
        }
        capturingHotkey_ = false;
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }

    void ActivateToolbarButton(ToolbarButton button) {
        if (button == ToolbarButton::Lock && spoutFrameAvailable_ && !toolbarGuideSeen_) {
            if (toolbarGuideHwnd_) {
                if (toolbarGuideStep_ == ToolbarGuideStep::Done) ContinueToolbarGuide();
                return;
            }
            if (BeginToolbarGuide()) return;
        }
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
        case ToolbarButton::Crop:
            BeginViewEdit();
            break;
        case ToolbarButton::ViewCrop:
            BeginCropEdit();
            break;
        case ToolbarButton::CropAspect:
                ToggleCropAspect();
            break;
        case ToolbarButton::CropRestore:
                cropDraft_ = kFullCrop;
            cropAspectSize_ = SIZE{static_cast<LONG>(sourceWidth_), static_cast<LONG>(sourceHeight_)};
            UpdateCropEdits();
            resetFrameSchedule_ = true;
            break;
        case ToolbarButton::CropCancel:
            EndCropEdit(false);
            break;
        case ToolbarButton::CropSave:
            EndCropEdit(true);
            break;
        case ToolbarButton::Border:
            CancelFpsCapture();
            CancelHotkeyCapture();
            Log("[toolbar] action=border_settings");
            ShowBorderSettingsDialog();
            break;
        case ToolbarButton::WindowMode:
            CancelFpsCapture();
            ToggleDesktopMode();
            break;
        case ToolbarButton::ViewImage:
            ShowImagePanel();
            break;
        case ToolbarButton::ViewBlend:
            ShowImagePanel(true);
            break;
        case ToolbarButton::ConnectApi:
            SetApiOptOut(false);
            break;
        case ToolbarButton::ViewShadow:
            ShowShadowPanel();
            break;
        case ToolbarButton::ViewDone:
            EndViewEdit();
            break;
        case ToolbarButton::CropUiScale:
            ShowUiScalePicker(toolbarHwnd_, ToolbarButtonRect(ToolbarButton::CropUiScale));
            break;
        case ToolbarButton::ViewSettings:
            CloseShadowPanel(false);
            CloseImagePanel(false);
            ShowFeatherPanel();
            break;
        case ToolbarButton::Hotkey:
            if (toolbarGuideHwnd_) FinishToolbarGuide(false);
            CancelFpsCapture();
            Log("[toolbar] action=capture_hotkey");
            if (capturingHotkey_) {
                CancelHotkeyCapture();
            } else {
                BeginHotkeyCapture();
            }
            break;
        case ToolbarButton::Language:
            CancelFpsCapture();
            CancelHotkeyCapture();
            Log("[toolbar] action=language_menu");
            ShowLanguageMenu();
            break;
        case ToolbarButton::Lock:
            if (!spoutFrameAvailable_) break;
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
            if (statusMode_ != VtsStatusMode::Hidden && !statusDismissed_) {
                Log("[toolbar] hide blocked while waiting for VTS");
                break;
            }
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
            ShowToolbarHint(hovered);
            if (hovered == ToolbarButton::Github && toolbarPressed_ == ToolbarButton::None) {
                ShowGithubDropdown();
            }
            TRACKMOUSEEVENT tracking{};
            tracking.cbSize = sizeof(tracking);
            tracking.dwFlags = TME_LEAVE;
            tracking.hwndTrack = toolbarHwnd_;
            TrackMouseEvent(&tracking);
            return 0;
        }
        case WM_MOUSELEAVE: {
            HideToolbarHint();
            toolbarHovered_ = ToolbarButton::None;
            InvalidateRect(toolbarHwnd_, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            HideToolbarHint();
            HideGithubDropdown();
            SetForegroundWindow(toolbarHwnd_);
            SetFocus(toolbarHwnd_);
            POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (point.y < ViewSettingsWarningHeight()) {
                RECT client{};
                GetClientRect(toolbarHwnd_, &client);
                const RECT action = ViewSettingsWarningActionRect(client.right);
                if (PtInRect(&action, point) && WriteConfigInt(L"warnings", L"suppress_view_settings", 1)) {
                    viewSettingsWarningDismissed_ = true;
                    PositionToolbar();
                }
                return 0;
            }
            if (point.y < UpdateNotificationHeight()) return 0;
            if (ShowApiNotification() && point.y < TotalNotificationOffset()) {
                const bool authorizationDenied =
                    vtsApi_.WasAuthorizationDenied();
                if (!authorizationDenied &&
                    (awaitingUserApproval_ || vtsApi_.IsAwaitingAuthorization())) {
                    RECT client{};
                    GetClientRect(toolbarHwnd_, &client);
                    const RECT action = ApiOptOutRect(client.right);
                    if (PtInRect(&action, point)) SetApiOptOut(true);
                    return 0;
                }
                RECT clientR{};
                GetClientRect(toolbarHwnd_, &clientR);
                const auto [scanWidth, howWidth, resultWidth,
                            retryAuthorizationWidth] =
                    ApiNotificationActionWidths();
                (void)resultWidth;
                const int howLeft = clientR.right - UiScaled(5) - howWidth;
                const int scanRight = howLeft - UiScaled(5);
                const int scanLeft = scanRight - scanWidth;
                const int retryAuthorizationRight = scanLeft - UiScaled(5);
                const int retryAuthorizationLeft =
                    retryAuthorizationRight - retryAuthorizationWidth;
                if (point.x >= howLeft) {
                    ShowVtsApiGuide();
                } else if (point.x >= scanLeft &&
                           point.x < scanRight) {
                    vtsApi_.RequestScan();
                    holdNotificationForScanResult_ = true;
                    scanStartedObserved_ = false;
                    SetTimer(toolbarHwnd_, 42, 300, nullptr);
                    InvalidateRect(toolbarHwnd_, nullptr, FALSE);
                } else if (authorizationDenied &&
                           point.x >= retryAuthorizationLeft &&
                           point.x < retryAuthorizationRight) {
                    KillTimer(toolbarHwnd_, 45);
                    KillTimer(toolbarHwnd_, 46);
                    showingApiSuccess_ = false;
                    awaitingUserApproval_ = true;
                    vtsApi_.RequestAuthorizationRetry();
                    SetTimer(toolbarHwnd_, 45, 500, nullptr);
                    SetTimer(toolbarHwnd_, 46, 60000, nullptr);
                    PositionToolbar();
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
            if (viewEditing_ && wParam == VK_ESCAPE) {
                if (cropEditing_) EndCropEdit(false);
                else EndViewEdit();
                return 0;
            }
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
            if (wParam == kToolbarGuideTimer) {
                PositionToolbarGuide();
                return 0;
            }
            if (wParam == kGithubDropdownTimer) {
                UpdateGithubDropdown();
                return 0;
            }
            if (wParam == 48) {
                if (!vtsApi_.IsStopped()) return 0;
                KillTimer(toolbarHwnd_, 48);
                apiStartScheduled_ = false;
                if (hasReceivedModel_ && !apiOptedOut_ && !apiStartedAfterModel_) {
                    apiStartedAfterModel_ = true;
                    vtsApi_.Start(instance_);
                    PositionToolbar();
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
                if (vtsApiFps_ == 0 &&
                    !vtsApi_.WasAuthorizationDenied()) {
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
                    if (vtsApi_.WasAuthorizationDenied()) {
                        KillTimer(toolbarHwnd_, 45);
                        KillTimer(toolbarHwnd_, 46);
                        awaitingUserApproval_ = false;
                        PositionToolbar();
                        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
                    } else if (vtsApiFps_ > 0) {
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
                if (point.y < ViewSettingsWarningHeight()) {
                    RECT client{};
                    GetClientRect(toolbarHwnd_, &client);
                    const RECT action = ViewSettingsWarningActionRect(client.right);
                    cursor = PtInRect(&action, point) ? IDC_HAND : IDC_ARROW;
                } else if (point.y < TotalNotificationOffset()) {
                    RECT clientR{};
                    GetClientRect(toolbarHwnd_, &clientR);
                    const auto [scanWidth, howWidth, resultWidth,
                                retryAuthorizationWidth] =
                        ApiNotificationActionWidths();
                    (void)resultWidth;
                    const int scanLeft = clientR.right - UiScaled(10) -
                        howWidth - scanWidth;
                    const bool authorizationDenied =
                        vtsApi_.WasAuthorizationDenied();
                    const int firstActionLeft = authorizationDenied
                        ? scanLeft - UiScaled(5) - retryAuthorizationWidth
                        : scanLeft;
                    const RECT optOut = ApiOptOutRect(clientR.right);
                    if (ShowApiNotification() && !authorizationDenied &&
                        (awaitingUserApproval_ || vtsApi_.IsAwaitingAuthorization()) &&
                        PtInRect(&optOut, point)) {
                        cursor = IDC_HAND;
                    } else if (point.y >= UpdateNotificationHeight() && ShowApiNotification() &&
                        (authorizationDenied ||
                         (!awaitingUserApproval_ &&
                          !vtsApi_.IsAwaitingAuthorization())) &&
                        point.x >= firstActionLeft) {
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
        if (locked) {
            CloseImagePanel(false);
            CloseFeatherPanel();
        }
        if (locked) EndViewEdit();
        if (locked && toolbarGuideHwnd_) FinishToolbarGuide(false);
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
        if (toolbarHwnd_) {
            LONG_PTR style = GetWindowLongPtrW(toolbarHwnd_, GWL_EXSTYLE);
            if (desktopMode_) style &= ~(static_cast<LONG_PTR>(WS_EX_TOOLWINDOW) | WS_EX_TOPMOST);
            else style |= WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
            SetWindowLongPtrW(toolbarHwnd_, GWL_EXSTYLE, style);
            SetWindowPos(toolbarHwnd_, WindowZOrder(), 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }

        LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
        style |= WS_EX_LAYERED;
        if (desktopMode_) {
            style &= ~(static_cast<LONG_PTR>(WS_EX_TOPMOST) | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
            style |= WS_EX_APPWINDOW;
        } else {
            style &= ~static_cast<LONG_PTR>(WS_EX_APPWINDOW);
            style |= WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
        }
        const bool statusInteractive =
            statusMode_ != VtsStatusMode::Hidden && !statusDismissed_;
        if (locked_ && !statusInteractive) {
            style |= WS_EX_TRANSPARENT;
            if (!desktopMode_) style |= WS_EX_NOACTIVATE;
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
            hwnd_, WindowZOrder(), 0, 0, 0, 0,
            overlayFlags);
        PositionToolbar();
        if (!overlayVisible_) ShowWindow(hwnd_, SW_HIDE);
    }

    LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        if (taskbarCreatedMessage_ && message == taskbarCreatedMessage_) {
            trayIconAdded_ = false;
            AddTrayIcon();
            return 0;
        }
        switch (message) {
        case kActivateExistingMessage: {
            if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
            UpdateDesktopPause();
            if (!overlayVisible_) SetOverlayVisible(true);
            const HWND target = IsWindowVisible(toolbarHwnd_) ? toolbarHwnd_ : hwnd_;
            if (!SetForegroundWindow(target)) {
                SetWindowPos(target, HWND_TOP, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            return 0;
        }
        case WM_TIMER:
            if (wParam == 67) { UpdateDesktopPause(); return 0; }
            if (wParam == kUpdateTimer) { PollUpdate(); return 0; }
            if (wParam == kUpdateNoticeDismissTimer) {
                if (updateNoticeDismissAt_ == Clock::time_point{}) return 0;
                const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
                    updateNoticeDismissAt_ - Clock::now());
                if (remaining.count() <= 0) {
                    ShowUpdateBanner({});
                } else {
                    SetTimer(hwnd_, kUpdateNoticeDismissTimer, static_cast<UINT>(remaining.count()), nullptr);
                }
                return 0;
            }
            break;
        case kUpdateStartupMessage:
            if (updater_) {
                updater_->OnStartup(updateStartupTask_, updateStartupToken_);
                PollUpdate();
                if (updater_->Poll().phase == vtsfloat::update::Phase::Idle) {
                    KillTimer(hwnd_, kUpdateTimer);
                }
                updateStartupTask_.clear();
                updateStartupToken_.clear();
            }
            return 0;
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
        case WM_KEYDOWN:
            if (viewEditing_ && wParam == VK_ESCAPE) {
                if (cropEditing_) EndCropEdit(false);
                else EndViewEdit();
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
                if (cropEditing_ && LOWORD(lParam) == HTCLIENT) {
                    const unsigned action = cropDrag_ ? cropDrag_ : CropDragAt(p);
                    SetCursor(LoadCursorW(nullptr, CropResizeCursor(action)));
                    return TRUE;
                }
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
            if (cropEditing_) {
                SetFocus(hwnd_);
                const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                cropDrag_ = CropDragAt(point);
                cropDragStart_ = NormalizedCropPoint(lParam);
                cropDragStartRect_ = cropDraft_;
                SetCapture(hwnd_);
                return 0;
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
        case WM_MOUSEMOVE:
            if (cropEditing_ && cropDrag_) {
                UpdateCropDrag(lParam);
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (cropEditing_ && cropDrag_) {
                UpdateCropDrag(lParam);
                cropDrag_ = 0;
                if (GetCapture() == hwnd_) ReleaseCapture();
                return 0;
            }
            break;
        case WM_CAPTURECHANGED:
            cropDrag_ = 0;
            break;
        case WM_NCHITTEST: {
            if (cropEditing_) return HTCLIENT;
            const bool statusInteractive =
                statusMode_ != VtsStatusMode::Hidden && !statusDismissed_;
            if (locked_ || statusInteractive) return HTTRANSPARENT;
            POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd_, &point);
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
            const bool editedBounds = CropActive() || imageTransform_.active();
            info->ptMinTrackSize.x = editedBounds ? 1 : kMinimumWidth;
            info->ptMinTrackSize.y = editedBounds ? 1 : 96;
            return 0;
        }
        case WM_MOVING: {
            if (!cropEditing_) break;
            auto* proposed = reinterpret_cast<RECT*>(lParam);
            MONITORINFO info{sizeof(MONITORINFO)};
            if (GetMonitorInfoW(MonitorFromRect(proposed, MONITOR_DEFAULTTONEAREST), &info)) {
                const RECT work = info.rcWork;
                const auto readout = BuildCropReadoutLayout(proposed->right - proposed->left, work.right - work.left);
                const LONG overflow = proposed->bottom + UiScaled(kCropReadoutGap) + readout.size.cy - work.bottom;
                if (overflow > 0) OffsetRect(proposed, 0, -overflow);
            }
            return TRUE;
        }
        case WM_SIZING: {
            if (!aspectLocked_ || !sourceWidth_ || !sourceHeight_) {
                break;
            }
            auto* rect = reinterpret_cast<RECT*>(lParam);
            const RECT region = DisplayImageBounds();
            const bool cropped = CropActive() || imageTransform_.active();
            const int regionWidth = region.right - region.left;
            const int regionHeight = region.bottom - region.top;
            const double aspect = static_cast<double>(regionWidth) / regionHeight;
            const int minimumWidth = cropped ? 1 : kMinimumWidth;
            int width = (std::max)(minimumWidth, static_cast<int>(rect->right - rect->left));
            const bool vertical = wParam == WMSZ_TOP || wParam == WMSZ_BOTTOM;
            const int requestedHeight = (std::max)(1L, rect->bottom - rect->top);
            if (vertical) width = (std::max)(minimumWidth, static_cast<int>(requestedHeight * aspect + 0.5));
            int height = (std::max)(1, static_cast<int>(width / aspect + 0.5));
            MONITORINFO info{sizeof(MONITORINFO)};
            if (cropped && GetMonitorInfoW(MonitorFromRect(rect, MONITOR_DEFAULTTONEAREST), &info)) {
                const double scale = vertical ? requestedHeight / static_cast<double>(regionHeight)
                    : width / static_cast<double>(regionWidth);
                const SIZE size = BoundedCropSize(region, info.rcWork, scale);
                width = size.cx;
                height = size.cy;
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
            break;
        case WM_DISPLAYCHANGE:
            UpdateMonitorRefreshRate(true);
            PositionToolbar();
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
            if (!updateClosing_) {
                SaveWindowPlacement();
                exportDebugLogOnExit_ = debugMode_;
            }
            RestoreExpressionsBeforeShutdown();
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

    int VtsStatusActionAt(POINT point) const {
        if (PtInRect(&statusExeRect_, point)) return 1;
        if (PtInRect(&statusBatchRect_, point)) return 2;
        if (PtInRect(&statusAddPathRect_, point) || PtInRect(&statusManualPathRect_, point)) return 3;
        if (PtInRect(&statusShortcutRect_, point)) return 5;
        if (PtInRect(&statusRemoveShortcutRect_, point)) return 6;
        if (!IsRectEmpty(&statusRemoveShortcutRect_) && point.x >= statusShortcutRect_.right &&
            point.x < statusRemoveShortcutRect_.left && point.y >= statusShortcutRect_.top &&
            point.y < statusShortcutRect_.bottom) return 7;
        if (PtInRect(&statusDocsRect_, point)) return 4;
        return 0;
    }

    static LRESULT CALLBACK StatusWindowProc(
        HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* overlay = reinterpret_cast<LayeredOverlay*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            overlay = static_cast<LayeredOverlay*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
        }
        if (!overlay) return DefWindowProcW(window, message, wParam, lParam);
        switch (message) {
        case WM_MOUSEMOVE: {
            const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const int action = overlay->VtsStatusActionAt(point);
            const int hover = !overlay->shortcutApp_.empty() && action >= 5 ? (action == 6 ? 6 : 5) : 0;
            if (std::exchange(overlay->shortcutHoverAction_, hover) != hover) overlay->statusFrameDirty_ = true;
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
            TrackMouseEvent(&track);
            return 0;
        }
        case WM_MOUSELEAVE:
            if (std::exchange(overlay->shortcutHoverAction_, 0)) overlay->statusFrameDirty_ = true;
            return 0;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_NCHITTEST: {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(window, &point);
            return overlay->VtsStatusActionAt(point) != 0 ? HTCLIENT : HTTRANSPARENT;
        }
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
            break;
        case WM_LBUTTONDOWN: {
            const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            overlay->statusPressedButton_ = overlay->VtsStatusActionAt(point);
            if (overlay->statusPressedButton_) {
                SetCapture(window);
                overlay->statusFrameDirty_ = true;
            }
            return 0;
        }
        case WM_CAPTURECHANGED:
            overlay->statusPressedButton_ = 0;
            overlay->statusFrameDirty_ = true;
            return 0;
        case WM_LBUTTONUP: {
            const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const int pressed = std::exchange(overlay->statusPressedButton_, 0);
            if (GetCapture() == window) ReleaseCapture();
            overlay->statusFrameDirty_ = true;
            if (pressed == 0 || pressed != overlay->VtsStatusActionAt(point)) return 0;
            switch (pressed) {
            case 1: overlay->LaunchVtsFromStatus(false); break;
            case 2: overlay->LaunchVtsFromStatus(true); break;
            case 3: overlay->ChooseVtsLaunchPath(); break;
            case 5: overlay->ActivateShortcutApp(); break;
            case 6: overlay->SaveShortcutApp({}); break;
            case 4: {
                Log("[vts status] action=open_docs");
                const INT_PTR result = reinterpret_cast<INT_PTR>(
                    ShellExecuteW(nullptr, L"open", kDocsUrl, nullptr, nullptr, SW_SHOWNORMAL));
                if (result <= 32) Log("[vts status] open_docs failed=" + std::to_string(result));
                break;
            }
            }
            return 0;
        }
        case WM_CLOSE:
            overlay->statusDismissed_ = true;
            ShowWindow(window, SW_HIDE);
            return 0;
        case WM_NCDESTROY:
            if (overlay->statusHwnd_ == window) overlay->statusHwnd_ = nullptr;
            break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND toolbarHwnd_ = nullptr;
    HWND statusHwnd_ = nullptr;
    HWND debugReadoutHwnd_ = nullptr;
    HWND toolbarGuideHwnd_ = nullptr;
    HWND toolbarGuideTitle_ = nullptr;
    HWND toolbarGuideText_ = nullptr;
    HWND toolbarGuideNext_ = nullptr;
    HWND toolbarGuideClose_ = nullptr;
    HFONT toolbarGuideTitleFont_ = nullptr;
    HFONT toolbarGuideBodyFont_ = nullptr;
    int toolbarGuideWidth_ = 0;
    int toolbarGuideHeight_ = 0;
    bool toolbarGuideSeen_ = false;
    ToolbarGuideStep toolbarGuideStep_ = ToolbarGuideStep::Done;
    POINT toolbarGuideFrom_{};
    Clock::time_point toolbarGuideAnimationStarted_{};
    HWND githubDropdownHwnd_ = nullptr;
    HWND githubVersionLabel_ = nullptr;
    HWND githubUpdateButton_ = nullptr;
    HWND githubDebugButton_ = nullptr;
    HFONT githubDropdownFont_ = nullptr;
    int githubDropdownWidth_ = 0;
    Clock::time_point githubDropdownLastHover_{};
    HWND githubHoveredButton_ = nullptr;
    HWND updateBannerHwnd_ = nullptr;
    HWND updateBannerLabel_ = nullptr;
    HWND updateLaterButton_ = nullptr;
    HWND updateInstallButton_ = nullptr;
    HWND updateDismissButton_ = nullptr;
    HFONT updateBannerFont_ = nullptr;
    int updateBannerFontHeight_ = 0;
    std::wstring updateBannerFontFace_;
    std::wstring updateBannerText_;
    std::filesystem::path updateStartupTask_;
    std::wstring updateStartupToken_;
    std::string updateLastDiagnostic_;
    std::unique_ptr<vtsfloat::update::Updater> updater_;
    std::uint64_t updateRevision_ = 0;
    UpdateNoticeStyle updateNoticeStyle_ = UpdateNoticeStyle::Standard;
    UpdateNoticeDismissal updateNoticeDismissal_ = UpdateNoticeDismissal::None;
    Clock::time_point updateNoticeDismissAt_{};
    bool updateDismissed_ = false;
    bool updateClosing_ = false;
    RECT statusExeRect_{};
    RECT statusBatchRect_{};
    RECT statusAddPathRect_{};
    RECT statusManualPathRect_{};
    RECT statusDocsRect_{};
    RECT statusShortcutRect_{}, statusRemoveShortcutRect_{};
    std::filesystem::path shortcutApp_;
    int shortcutHoverAction_ = 0;
    int statusPressedButton_ = 0;
    bool statusPathLookupFailed_ = false;
    bool statusFrameDirty_ = true;
    HICON appIconLarge_ = nullptr;
    HICON appIconSmall_ = nullptr;
    ULONG_PTR githubGraphicsToken_ = 0;
    std::array<HICON, 7> toolbarToggleIcons_{};
    int toolbarToggleIconSize_ = 0;
    HWND toolbarHintHwnd_ = nullptr;
    ToolbarButton toolbarHintButton_ = ToolbarButton::None;
    HANDLE instanceMutex_ = nullptr;
    DWORD uiThreadId_ = 0;
    Clock::time_point lastVtsStatusCheck_{};
    VtsStatusMode statusMode_ = VtsStatusMode::Hidden;
    std::filesystem::path statusDirectory_;
    bool statusDismissed_ = false;
    bool hasReceivedModel_ = false;
    bool apiOptedOut_ = false;
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
    bool exportDebugLogOnExit_ = false;
    bool aspectLocked_ = true;
    bool fadeShadowOnHover_ = false;
    bool hoverFadeEnabled_ = true;
    int hoverOpacityPercent_ = kDefaultHoverOpacityPercent;
    double hoverOpacityRestoreDelaySeconds_ =
        kDefaultHoverOpacityRestoreDelaySeconds;
    bool hoverOpacityCursorWasOverModel_ = false;
    bool hoverOpacityRestorePending_ = false;
    Clock::time_point hoverOpacityRestoreAt_{};
    int modelOpacityPercent_ = 100;
    int uiScalePercent_ = kDefaultUiScalePercent;
    ToolbarLayout toolbarLayout_{};
    CropToolbarLayout cropToolbarLayout_{};
    CropReadoutLayout cropReadoutLayout_{};
    HWND cropReadoutHwnd_ = nullptr;
    HWND cropWidthHwnd_ = nullptr;
    HWND cropHeightHwnd_ = nullptr;
    HWND numericDragEdit_ = nullptr;
    LONG numericDragStart_ = 0;
    double numericDragBase_ = 0;
    bool numericDragging_ = false;
    HWND shadowPanelHwnd_ = nullptr;
    HFONT shadowPanelFont_ = nullptr;
    std::array<HWND, 6> shadowFields_{};
    std::vector<std::uint32_t> shadowWheelPixels_;
    double shadowHue_ = 0, shadowSaturation_ = 0;
    int shadowColorValue_ = 0;
    bool shadowColorOpen_ = false;
    std::array<vtsfloat::ImageShadow, 2> shadow_{}, shadowDraft_{};
    std::array<vtsfloat::ShadowMask, 2> shadowMasks_;
    std::array<double, 2> shadowOffsetX_{}, shadowOffsetY_{};
    std::array<bool, 2> shadowMaskReady_{};
    size_t shadowTab_ = 1;
    int shadowDrag_ = -1;
    double shadowPanelScale_ = 1;
    bool shadowUpdatingFields_ = false;
    bool imagePanelDetached_ = false, shadowPanelDetached_ = false;
    HWND imagePanelHwnd_ = nullptr;
    HFONT imagePanelFont_ = nullptr;
    std::array<HWND, 15> imagePanelEdits_{};
    vtsfloat::ImageTransform imageTransform_{}, imageTransformDraft_{};
    vtsfloat::ImageTone imageTone_{}, imageToneDraft_{};
    vtsfloat::ImageMapping subjectImageMapping_{}, subjectOutlineMapping_{};
    struct {
        vtsfloat::ToneLut lut{};
        vtsfloat::ColorGrade grade{};
    } toneSettings_;
    vtsfloat::ImageEffects imageEffects_{}, imageEffectsDraft_{};
    vtsfloat::ImageBlend imageBlendMode_ = vtsfloat::ImageBlend::Normal;
    vtsfloat::ImageBlendTable imageBlendTable_{};
    vtsfloat::ImageBackdrop imageBackdrop_;
    std::array<vtsfloat::Rgb, 100> imageBackdropColors_{};
    Clock::time_point imageBackdropColorTime_{};
    int imageBlendGrid_ = 3;
    std::mutex imageBackdropMutex_;
    int imageBlendStrengthHundredths_ = 10000;
    bool imageBackdropUnavailable_ = false;
    Clock::time_point nextImageBackdrop_{};
    std::vector<uint8_t> imageSharpenBuffer_;
    int imageClarity_ = 0, imageVignette_ = 0;
    int imageSharpen_ = 0, imageEffectDrag_ = -1;
    double imagePanelScale_ = 1;
    bool colorGradeEnabled_ = false, imageEffectsTab_ = false;
    RECT imagePreviewBounds_{};
    int imageCurveChannel_ = 0, imageCurveKnot_ = -1, imagePanelHover_ = 0;
    int imageLevelsDrag_ = -1;
    bool imageUpdatingFields_ = false;
    bool toneEnabled_ = false, toneGpuDirty_ = true;
    HWND featherPanelHwnd_ = nullptr;
    HWND cropAngleHwnd_ = nullptr;
    HWND cropInputPressed_ = nullptr;
    LONG cropInputStartX_ = 0;
    double cropInputBase_ = 0;
    bool cropInputDragging_ = false;
    HWND featherScrubHwnd_ = nullptr;
    HWND featherPressedEdit_ = nullptr;
    std::array<HWND, FeatherSideCount> featherPanelEdits_{};
    HFONT featherEditFont_ = nullptr;
    HFONT cropReadoutFont_ = nullptr;
    RECT cropSelection_ = kFullCrop;
    RECT cropBeforeEditSelection_{};
    bool cropNumericSaved_ = false;
    RECT cropDraft_ = kFullCrop;
    RECT cropBeforeEditBounds_{};
    RECT cropDragStartRect_{};
    POINT cropDragStart_{};
    SIZE cropAspectSize_{};
    FeatherEdges cropFeather_{};
    FeatherEdges cropFeatherDraft_{};
    FeatherEdge featherDragBase_{};
    LONG featherPressX_ = 0;
    std::array<FeatherRamp, FeatherSideCount> featherRamps_{};
    int featherPanelHover_ = FeatherNoAction;
    int featherPanelPressed_ = FeatherNoAction;
    int featherScrubSide_ = FeatherNoAction;
    bool featherDragging_ = false;
    bool featherPanelDetached_ = false;
    unsigned cropDrag_ = 0;
    bool viewEditing_ = false;
    bool cropEditing_ = false;
    bool cropAspectLocked_ = false;
    int hoverExpandPx_ = 0;
    bool excludeEffectsFromHover_ = false;
    bool subjectHoverRegionConfigured_ = false;
    int subjectHoverRegionLeft_ = 0;
    int subjectHoverRegionTop_ = 0;
    int subjectHoverRegionRight_ = 10000;
    int subjectHoverRegionBottom_ = 10000;
    int subjectHoverMaskGridWidth_ = 0;
    int subjectHoverMaskGridHeight_ = 0;
    std::vector<std::uint8_t> subjectHoverMaskBits_;
    mutable bool subjectHoverMaskDirty_ = false;
    bool subjectHoverTrackingReferenceValid_ = false;
    double subjectHoverTrackingReferenceCenterX_ = 0.0;
    double subjectHoverTrackingReferenceCenterY_ = 0.0;
    double subjectHoverTrackingReferenceWidth_ = 0.0;
    double subjectHoverTrackingReferenceHeight_ = 0.0;
    double subjectHoverTrackingCurrentCenterX_ = 0.0;
    double subjectHoverTrackingCurrentCenterY_ = 0.0;
    double subjectHoverTrackingScale_ = 1.0;
    Clock::time_point subjectHoverTrackingLastUpdate_{};
    HWND subjectSelectionHwnd_ = nullptr;
    HWND subjectSelectionToolbarHwnd_ = nullptr;
    std::vector<std::vector<POINT>> subjectSelectionPolygons_;
    std::vector<POINT> subjectSelectionPolygon_;
    std::vector<SubjectSelectionSnapshot> subjectSelectionUndo_;
    std::vector<SubjectSelectionSnapshot> subjectSelectionRedo_;
    POINT subjectSelectionCursor_{};
    bool subjectSelectionDragging_ = false;
    int subjectSelectionVirtualLeft_ = 0;
    int subjectSelectionVirtualTop_ = 0;
    bool hoverExpressionEnabled_ = false;
    std::vector<std::string> hoverExpressionFiles_;
    double hoverExpressionDurationSeconds_ =
        kDefaultHoverExpressionDurationSeconds;
    bool hoverExpressionHovered_ = false;
    bool hoverExpressionLeaving_ = false;
    bool hoverExpressionRestored_ = false;
    std::vector<VtsExpressionState> hoverExpressionOriginalStates_;
    std::vector<VtsExpressionState> hoverExpressionBaselineStates_;
    std::vector<VtsExpressionState> hoverExpressionExpectedStates_;
    bool hoverExpressionCapturePending_ = false;
    bool hoverExpressionStatePollPending_ = false;
    bool hoverExpressionReentryRefreshPending_ = false;
    std::uint64_t hoverExpressionCaptureRevision_ = 0;
    std::uint64_t hoverExpressionStatePollRevision_ = 0;
    Clock::time_point hoverExpressionNextStatePollAt_{};
    Clock::time_point hoverExpressionNextTriggerAt_{};
    bool hoverExpressionCursorWasOverModel_ = false;
    bool suppressHoverExpressionUntilCursorLeaves_ = false;
    Clock::time_point hoverExpressionRestoreAt_{};
    bool expressionPanelPreviewActive_ = false;
    bool expressionPanelStateCaptured_ = false;
    std::vector<VtsExpressionState> expressionPanelInitialStates_;
    bool expressionShutdownPrepared_ = false;
    Clock::time_point expressionPanelNextPollAt_{};
    Clock::time_point expressionPanelIgnoreUntil_{};
    bool borderDialogOpen_ = false;
    HWND borderPanelHwnd_ = nullptr;
    HWND uiScalePickerHwnd_ = nullptr;
    HWND expressionPickerHwnd_ = nullptr;
    bool personalPanelManuallyPositioned_ = false;
    bool showHoverExpandPreview_ = false;
    bool hoverExpandEditing_ = false;
    bool showSubjectRegionPreview_ = false;
    int subjectOutlineWidth_ = 0;
    int subjectOutlineHeight_ = 0;
    int subjectOutlineGridWidth_ = 0;
    int subjectOutlineGridHeight_ = 0;
    std::vector<std::uint8_t> subjectOutlineMask_;
    std::vector<SubjectOutlinePixel> subjectOutlinePixels_;
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
    bool desktopMode_ = false;
    bool pauseOnFullscreen_ = true;
    std::atomic<bool> desktopOutputPaused_{ false };
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
    bool viewSettingsWarningDismissed_ = false;
    bool gpuWarningPermanentlyDismissed_ = false;
    bool gpuWarningShown_ = false;
    Clock::time_point gpuWarningStart_{};
    RECT gpuWarningDontShowRect_{};
    bool holdNotificationForScanResult_ = false;
    bool scanStartedObserved_ = false;
    bool awaitingUserApproval_ = false;
    bool showingApiSuccess_ = false;
    bool lastAuthorizationDenied_ = false;
    ULONG_PTR gdiplusToken_ = 0;
    std::vector<GpuAdapterInfo> gpuAdapters_;
    std::wstring fpsInput_;
    ToolbarButton toolbarHovered_ = ToolbarButton::None;
    ToolbarButton toolbarPressed_ = ToolbarButton::None;

    spoutDX receiver_;
    bool spoutFrameSyncDisabled_ = false;
    bool spoutFrameAvailable_ = false;
    bool spoutNeedsChoice_ = false;
    std::string spoutSender_, manualSpoutSender_;
    Clock::time_point nextSpoutScan_{};
    bool spoutRebindInProgress_ = false;
    Clock::time_point lastSpoutRebindCheck_{};
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGIAdapter3> activeAdapter3_;
    PDH_HQUERY gpuUsageQuery_ = nullptr;
    PDH_HCOUNTER gpuUsageCounter_ = nullptr;
    PDH_HCOUNTER gpuProcessMemoryCounter_ = nullptr;
    std::optional<double> gpuUsagePercent_;
    std::optional<double> cpuUsagePercent_;
    UINT64 processGpuMemoryBytes_ = 0;
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
    std::array<ComPtr<ID3D11Texture2D>, 2> scaleOutputReadbacks_;
    size_t scaleReadbackWriteIndex_ = 0;
    bool scaleReadbackPrimed_ = false;
    ComPtr<ID3D11VertexShader> scaleVertexShader_;
    ComPtr<ID3D11PixelShader> scalePixelShader_;
    ComPtr<ID3D11SamplerState> scaleSampler_;
    ComPtr<ID3D11SamplerState> scalePointSampler_;
    ComPtr<ID3D11Buffer> scaleSettingsBuffer_;
    ComPtr<ID3D11Buffer> toneSettingsBuffer_;
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
    std::uint64_t updated_ = 0, modelPresentations_ = 0;
    Clock::time_point firstModelPresentation_{}, lastModelPresentation_{};
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

};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetUnhandledExceptionFilter(&UnhandledExceptionHandler);
    AddVectoredExceptionHandler(0, &VectoredExceptionHandler);
    _set_purecall_handler(&PurecallHandler);
    _set_invalid_parameter_handler(&InvalidParamHandler);
    try {
        int argc = 0;
        std::unique_ptr<LPWSTR, decltype(&LocalFree)> argv(
            CommandLineToArgvW(GetCommandLineW(), &argc), LocalFree);
        std::filesystem::path updateTask;
        std::wstring updateToken;
        if (argv && argc > 1) {
            const std::wstring_view mode(argv.get()[1]);
            if (mode == L"--stop") {
                return StopRunningOverlay();
            }
            if (mode == L"--restart-after" && argc > 2) {
                wchar_t* end = nullptr;
                const DWORD parentProcessId = wcstoul(argv.get()[2], &end, 10);
                if (parentProcessId && end && !*end) return RunDelayedRestart(parentProcessId);
            }
            if (mode == L"--update-task" && argc == 5 &&
                std::wstring_view(argv.get()[3]) == L"--update-token") {
                updateTask = argv.get()[2];
                updateToken = argv.get()[4];
            }
        }
        struct Apartment {
            HRESULT result = RoInitialize(RO_INIT_SINGLETHREADED);
            ~Apartment() { if (SUCCEEDED(result)) RoUninitialize(); }
        } apartment;
        LayeredOverlay overlay;
        return overlay.Run(instance, std::move(updateTask), std::move(updateToken));
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
