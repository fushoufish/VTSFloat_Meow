#pragma once

#include <windows.h>
#include <shlobj.h>

#include <filesystem>

namespace vtsfloat {

inline const std::filesystem::path& CacheDirectory() {
    static const auto directory = [] {
        wchar_t localAppData[32768]{};
        const DWORD length = GetEnvironmentVariableW(
            L"LOCALAPPDATA", localAppData, ARRAYSIZE(localAppData));
        std::filesystem::path base;
        if (length > 0 && length < ARRAYSIZE(localAppData)) {
            base = localAppData;
        }
        std::error_code error;
        if (!base.is_absolute()) {
            if (SUCCEEDED(SHGetFolderPathW(
                    nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, localAppData))) {
                base = localAppData;
            } else {
                base = std::filesystem::temp_directory_path(error);
            }
        }
        auto path = base / L"VTSFloat_Meow";
        std::filesystem::create_directories(path, error);
        return path;
    }();
    return directory;
}

}  // namespace vtsfloat
