#pragma once

#include <windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace vtsfloat::update {

using Version = std::array<unsigned, 3>;
std::optional<Version> ParseVersion(std::wstring_view text);

struct Release {
    Version version{};
    std::wstring tag;
    std::wstring url;
    std::string sha256;
    std::uint64_t size = 0;
};

std::optional<Release> ParseRelease(std::string_view json);
std::wstring RouteUrl(std::wstring_view githubUrl, bool proxy);

struct ReleaseLookup {
    std::optional<Release> release;
    bool proxy = false;
    std::string diagnostic;
};

ReleaseLookup LookupLatest(const std::function<std::string(bool proxy)>& request,
                           const std::atomic<bool>& cancelled);

enum class Phase {
    Idle, Checking, Available, Current, CheckFailed, Downloading,
    DownloadFailed, Preparing, ReadyToExit, WaitingForSuccess,
    ShowSuccess, Success, ApplyFailed, RolledBack, RecoveryNeeded,
};

struct Status {
    Phase phase = Phase::Idle;
    std::uint64_t revision = 0;
    std::wstring tag;
    int percent = 0;
    bool proxy = false;
    std::string diagnostic;
};

// UI methods run on the window thread. Network operations run on one worker.
// The transaction directory holds atomic, token-bound messages for PowerShell.
class Updater {
public:
    Updater(HINSTANCE instance, std::filesystem::path config,
            std::filesystem::path executable, std::wstring version,
            std::function<void(const std::string&)> log = {});
    ~Updater();
    Updater(const Updater&) = delete;
    Updater& operator=(const Updater&) = delete;

    bool Check(std::filesystem::path executable);
    bool Download();
    void Dismiss();
    void Cancel();
    Status Poll();

    // Called only after the GUI and its message loop are ready.
    void OnStartup(const std::filesystem::path& taskFile, std::wstring_view token);
    bool Commit();
    void AcknowledgeSuccess();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vtsfloat::update
