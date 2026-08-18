#include <windows.h>
#include <appmodel.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <sddl.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

#include "SpoutDX.h"

using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;

namespace {

constexpr wchar_t kTextureName[] = L"Local\\LilyVtsOverlaySpoutTexture";
constexpr wchar_t kInstanceName[] = L"Local\\LilyVtsOverlaySpoutBridge";
constexpr double kTargetFps = 60.0;
constexpr INT kGpuThreadPriority = 7;

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

std::filesystem::path LogPath() {
    wchar_t localAppData[MAX_PATH]{};
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, ARRAYSIZE(localAppData))) {
        return L"vts_spout_bridge.log";
    }

    UINT32 familyLength = 0;
    const LONG familyResult = GetCurrentPackageFamilyName(&familyLength, nullptr);
    if (familyResult != ERROR_INSUFFICIENT_BUFFER || familyLength == 0) {
        return std::filesystem::path(localAppData) / L"vts_spout_bridge.log";
    }

    std::wstring family(familyLength, L'\0');
    if (GetCurrentPackageFamilyName(&familyLength, family.data()) != ERROR_SUCCESS) {
        return std::filesystem::path(localAppData) / L"vts_spout_bridge.log";
    }
    family.resize(wcslen(family.c_str()));

    auto path = std::filesystem::path(localAppData) / L"Packages" / family / L"LocalState";
    std::error_code ignored;
    std::filesystem::create_directories(path, ignored);
    return path / L"spout_bridge.log";
}

class Logger {
public:
    Logger() : path_(LogPath()) {}

    template <typename T>
    Logger& operator<<(T const& value) {
        buffer_ << value;
        return *this;
    }

    void Flush() {
        const std::string line = buffer_.str();
        // The bridge is an obsolete diagnostic path, but keep it quiet if it
        // is launched by an older helper or during troubleshooting.  Frame
        // counters belong in the in-app debug view, not a growing disk log.
        if (line.rfind("[bridge perf]", 0) == 0 ||
            line.rfind("[layered perf]", 0) == 0 ||
            line.rfind("[dx11 perf]", 0) == 0 ||
            line.rfind("[native perf]", 0) == 0 ||
            line.rfind("[perf]", 0) == 0) {
            buffer_.str({});
            buffer_.clear();
            return;
        }
        std::ofstream output(path_, std::ios::app);
        output << line << std::endl;
        buffer_.str({});
        buffer_.clear();
    }

private:
    std::filesystem::path path_;
    std::ostringstream buffer_;
};

class Bridge {
public:
    int Run() {
        ConfigureProcess();
        CreateGraphics();

        {
            Logger log;
            log << "[bridge] started texture=Local\\LilyVtsOverlaySpoutTexture";
            log.Flush();
        }

        auto nextFrame = Clock::now();
        statsStarted_ = nextFrame;
        for (;;) {
            const auto now = Clock::now();
            if (now < nextFrame) {
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(nextFrame - now);
                Sleep(static_cast<DWORD>((std::max)(1LL, remaining.count())));
                continue;
            }

            ReceiveAndPublish();
            PrintStats();
            nextFrame += std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(1.0 / kTargetFps));
            if (now - nextFrame > std::chrono::milliseconds(100)) {
                nextFrame = now;
            }
        }
    }

    ~Bridge() {
        receiver_.ReleaseReceiver();
        if (sharedHandle_) {
            CloseHandle(sharedHandle_);
        }
    }

private:
    void ConfigureProcess() {
        SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    }

    void CreateGraphics() {
        ComPtr<IDXGIFactory6> factory;
        Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");

        int senderAdapter = -1;
        char activeSender[256]{};
        if (receiver_.GetActiveSender(activeSender)) {
            senderAdapter = receiver_.GetSenderAdapter(activeSender);
        }

        ComPtr<IDXGIAdapter1> receiverAdapter;
        if (senderAdapter >= 0) {
            factory->EnumAdapters1(static_cast<UINT>(senderAdapter), &receiverAdapter);
        }
        if (!receiverAdapter) {
            for (UINT index = 0;
                 factory->EnumAdapterByGpuPreference(
                     index,
                     DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                     IID_PPV_ARGS(&receiverAdapter)) != DXGI_ERROR_NOT_FOUND;
                 ++index) {
                DXGI_ADAPTER_DESC1 description{};
                receiverAdapter->GetDesc1(&description);
                if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
                    break;
                }
                receiverAdapter.Reset();
            }
        }
        if (!receiverAdapter) {
            Check(factory->EnumAdapters1(0, &receiverAdapter), "EnumAdapters1(receiver)");
        }

        ComPtr<IDXGIAdapter1> outputAdapter;
        for (UINT index = 0;; ++index) {
            ComPtr<IDXGIAdapter1> candidate;
            if (factory->EnumAdapters1(index, &candidate) == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            DXGI_ADAPTER_DESC1 description{};
            candidate->GetDesc1(&description);
            if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                description.VendorId == 0x1002) {
                outputAdapter = std::move(candidate);
                break;
            }
        }
        if (!outputAdapter) {
            outputAdapter = receiverAdapter;
        }

        DXGI_ADAPTER_DESC1 receiverDescription{};
        DXGI_ADAPTER_DESC1 outputDescription{};
        receiverAdapter->GetDesc1(&receiverDescription);
        outputAdapter->GetDesc1(&outputDescription);

        constexpr D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };
        D3D_FEATURE_LEVEL selected{};
        Check(
            D3D11CreateDevice(
                receiverAdapter.Get(),
                D3D_DRIVER_TYPE_UNKNOWN,
                nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                levels,
                ARRAYSIZE(levels),
                D3D11_SDK_VERSION,
                &receiverDevice_,
                &selected,
                &receiverContext_),
            "D3D11CreateDevice(receiver)");

        sameAdapter_ =
            receiverDescription.AdapterLuid.HighPart == outputDescription.AdapterLuid.HighPart &&
            receiverDescription.AdapterLuid.LowPart == outputDescription.AdapterLuid.LowPart;
        // Keep receive and publish on separate devices even when both use the
        // AMD adapter.  Some AMD drivers deadlock when a Spout-owned source is
        // copied directly into a keyed shared texture on the same context.
        // CPU staging is deterministic here and, unlike the old RTX readback,
        // cannot be starved by the uncapped game on the discrete GPU.
        selected = {};
        Check(
            D3D11CreateDevice(
                outputAdapter.Get(),
                D3D_DRIVER_TYPE_UNKNOWN,
                nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                levels,
                ARRAYSIZE(levels),
                D3D11_SDK_VERSION,
                &outputDevice_,
                &selected,
                &outputContext_),
            "D3D11CreateDevice(output)");

        ComPtr<IDXGIDevice> receiverDxgiDevice;
        ComPtr<IDXGIDevice> outputDxgiDevice;
        Check(receiverDevice_.As(&receiverDxgiDevice), "Query receiver IDXGIDevice");
        Check(outputDevice_.As(&outputDxgiDevice), "Query output IDXGIDevice");
        Check(receiverDxgiDevice->SetGPUThreadPriority(kGpuThreadPriority), "Set receiver GPU priority");
        Check(outputDxgiDevice->SetGPUThreadPriority(kGpuThreadPriority), "Set output GPU priority");

        if (!receiver_.OpenDirectX11(receiverDevice_.Get())) {
            throw std::runtime_error("SpoutDX could not use the D3D11 device");
        }

        Logger log;
        log << "[bridge] receive_adapter=" << WideToUtf8(receiverDescription.Description)
            << " output_adapter=" << WideToUtf8(outputDescription.Description)
            << " mode=" << (sameAdapter_ ? "cpu_same_adapter" : "cpu_cross_adapter")
            << " gpu_priority=" << kGpuThreadPriority;
        log.Flush();
    }

    void CreateSharedTexture(ID3D11Texture2D* source) {
        D3D11_TEXTURE2D_DESC sourceDescription{};
        source->GetDesc(&sourceDescription);

        D3D11_TEXTURE2D_DESC stagingDescription = sourceDescription;
        stagingDescription.MipLevels = 1;
        stagingDescription.ArraySize = 1;
        stagingDescription.SampleDesc.Count = 1;
        stagingDescription.SampleDesc.Quality = 0;
        stagingDescription.Usage = D3D11_USAGE_STAGING;
        stagingDescription.BindFlags = 0;
        stagingDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDescription.MiscFlags = 0;
        Check(
            receiverDevice_->CreateTexture2D(&stagingDescription, nullptr, &readbackTexture_),
            "CreateTexture2D(readback)");

        D3D11_TEXTURE2D_DESC outputDescription = sourceDescription;
        outputDescription.MipLevels = 1;
        outputDescription.ArraySize = 1;
        outputDescription.SampleDesc.Count = 1;
        outputDescription.SampleDesc.Quality = 0;
        outputDescription.Usage = D3D11_USAGE_DEFAULT;
        outputDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        outputDescription.CPUAccessFlags = 0;
        outputDescription.MiscFlags =
            D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
            D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

        ComPtr<ID3D11Texture2D> texture;
        Check(outputDevice_->CreateTexture2D(&outputDescription, nullptr, &texture), "CreateTexture2D(shared output)");

        PSECURITY_DESCRIPTOR securityDescriptor = nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
                L"D:(A;;GA;;;WD)(A;;GA;;;AC)S:(ML;;NW;;;LW)",
                SDDL_REVISION_1,
                &securityDescriptor,
                nullptr)) {
            throw std::runtime_error("ConvertStringSecurityDescriptorToSecurityDescriptorW failed");
        }

        SECURITY_ATTRIBUTES securityAttributes{};
        securityAttributes.nLength = sizeof(securityAttributes);
        securityAttributes.lpSecurityDescriptor = securityDescriptor;
        securityAttributes.bInheritHandle = FALSE;

        ComPtr<IDXGIResource1> resource;
        Check(texture.As(&resource), "Query IDXGIResource1");
        HANDLE handle = nullptr;
        const HRESULT createResult = resource->CreateSharedHandle(
            &securityAttributes,
            DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
            kTextureName,
            &handle);
        LocalFree(securityDescriptor);
        Check(createResult, "CreateSharedHandle");

        ComPtr<IDXGIKeyedMutex> mutex;
        Check(texture.As(&mutex), "Query IDXGIKeyedMutex");

        sharedTexture_ = std::move(texture);
        sharedMutex_ = std::move(mutex);
        sharedHandle_ = handle;
        width_ = outputDescription.Width;
        height_ = outputDescription.Height;
        format_ = outputDescription.Format;

        Logger log;
        log << "[bridge] shared_texture=" << width_ << "x" << height_
            << " format=" << static_cast<unsigned>(format_);
        log.Flush();
    }

    void ReceiveAndPublish() {
        const auto receiveStarted = Clock::now();
        if (!receiver_.ReceiveTexture()) {
            return;
        }
        receiveMs_ += std::chrono::duration<double, std::milli>(Clock::now() - receiveStarted).count();
        ++received_;

        ID3D11Texture2D* source = receiver_.GetSenderTexture();
        if (!source) {
            return;
        }

        D3D11_TEXTURE2D_DESC sourceDescription{};
        source->GetDesc(&sourceDescription);
        if (!sharedTexture_) {
            CreateSharedTexture(source);
        }
        if (sourceDescription.Width != width_ ||
            sourceDescription.Height != height_ ||
            sourceDescription.Format != format_) {
            if (!resizeWarningWritten_) {
                Logger log;
                log << "[bridge] sender changed to " << sourceDescription.Width << "x"
                    << sourceDescription.Height << "; restart widget to recreate shared texture";
                log.Flush();
                resizeWarningWritten_ = true;
            }
            return;
        }

        if (!receiver_.IsFrameNew()) {
            return;
        }
        ++newFrames_;

        const auto copyStarted = Clock::now();
        // The Game Bar consumer never waits.  This full-trust bridge may wait
        // for at most one frame so two independent 60 Hz clocks do not drop
        // frames merely because their phases cross.
        receiverContext_->CopyResource(readbackTexture_.Get(), source);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const HRESULT mapResult = receiverContext_->Map(
            readbackTexture_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(mapResult)) {
            ++errors_;
            return;
        }

        const HRESULT acquire = sharedMutex_->AcquireSync(0, 20);
        if (acquire == WAIT_TIMEOUT) {
            receiverContext_->Unmap(readbackTexture_.Get(), 0);
            ++busy_;
            return;
        }
        if (FAILED(acquire)) {
            receiverContext_->Unmap(readbackTexture_.Get(), 0);
            ++errors_;
            return;
        }

        outputContext_->UpdateSubresource(
            sharedTexture_.Get(), 0, nullptr, mapped.pData, mapped.RowPitch, 0);
        receiverContext_->Unmap(readbackTexture_.Get(), 0);
        outputContext_->Flush();
        sharedMutex_->ReleaseSync(1);
        copyMs_ += std::chrono::duration<double, std::milli>(Clock::now() - copyStarted).count();
        ++published_;
    }

    void PrintStats() {
        const auto now = Clock::now();
        const double seconds = std::chrono::duration<double>(now - statsStarted_).count();
        if (seconds < 2.0) {
            return;
        }

        Logger log;
        log << std::fixed << std::setprecision(1)
            << "[bridge perf] receive=" << received_ / seconds
            << " fps new=" << newFrames_ / seconds
            << " fps publish=" << published_ / seconds
            << " fps busy=" << busy_ / seconds
            << " fps errors=" << errors_
            << std::setprecision(3)
            << " receive_ms=" << (received_ ? receiveMs_ / received_ : 0.0)
            << " copy_ms=" << (published_ ? copyMs_ / published_ : 0.0);
        log.Flush();

        statsStarted_ = now;
        received_ = newFrames_ = published_ = busy_ = errors_ = 0;
        receiveMs_ = copyMs_ = 0.0;
    }

    spoutDX receiver_;
    ComPtr<ID3D11Device> receiverDevice_;
    ComPtr<ID3D11DeviceContext> receiverContext_;
    ComPtr<ID3D11Device> outputDevice_;
    ComPtr<ID3D11DeviceContext> outputContext_;
    ComPtr<ID3D11Texture2D> readbackTexture_;
    ComPtr<ID3D11Texture2D> sharedTexture_;
    ComPtr<IDXGIKeyedMutex> sharedMutex_;
    HANDLE sharedHandle_ = nullptr;
    bool sameAdapter_ = false;
    UINT width_ = 0;
    UINT height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
    bool resizeWarningWritten_ = false;

    Clock::time_point statsStarted_{};
    std::uint64_t received_ = 0;
    std::uint64_t newFrames_ = 0;
    std::uint64_t published_ = 0;
    std::uint64_t busy_ = 0;
    std::uint64_t errors_ = 0;
    double receiveMs_ = 0.0;
    double copyMs_ = 0.0;
};

}  // namespace

extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    HANDLE instance = CreateMutexW(nullptr, TRUE, kInstanceName);
    if (!instance || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (instance) {
            CloseHandle(instance);
        }
        return 0;
    }

    int result = 0;
    try {
        result = Bridge().Run();
    }
    catch (std::exception const& error) {
        Logger log;
        log << "[bridge fatal] " << error.what();
        log.Flush();
        result = 1;
    }

    ReleaseMutex(instance);
    CloseHandle(instance);
    return result;
}
