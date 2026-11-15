#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>

namespace vtsfloat {

inline constexpr std::uint64_t kAnrPackageSize = 3231158;

struct AnrDownloadProgress {
    std::atomic<bool> cancelled{false};
    std::atomic<bool> installing{false};
    std::atomic<std::uint64_t> downloaded{0};
};

std::filesystem::path CachedAnrProvider();
std::filesystem::path InstallAnrPackage(AnrDownloadProgress& progress);

} // namespace vtsfloat
