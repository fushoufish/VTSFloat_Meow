#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dcomp.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <mmsystem.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "SpoutDX.h"

using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;

extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace {

constexpr wchar_t kWindowClass[] = L"VtsOverlayDx11Window";
constexpr wchar_t kWindowTitle[] = L"VTS Overlay (D3D11)";
constexpr UINT kHotkeyId = 1;
constexpr double kTargetFps = 60.0;
constexpr double kFrameSeconds = 1.0 / kTargetFps;
constexpr double kStatsSeconds = 2.0;
constexpr INT kGpuThreadPriority = 7;
constexpr int kMinimumWidth = 160;
constexpr int kInitialWidth = 640;
constexpr int kInitialHeight = 386;
constexpr int kCloseButtonWidth = 32;
constexpr int kCloseButtonHeight = 26;
constexpr UINT kSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

const char* kVertexShader = R"hlsl(
struct VsOut {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VsOut main(uint vertexId : SV_VertexID) {
    VsOut output;
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.uv = uv;
    return output;
}
)hlsl";

const char* kPixelShader = R"hlsl(
Texture2D sourceTexture : register(t0);
SamplerState sourceSampler : register(s0);

cbuffer FrameConstants : register(b0) {
    float2 viewport;
    uint showBorder;
    uint hasTexture;
    uint showCloseButton;
    uint closeButtonHovered;
    float2 padding;
};

struct PsIn {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(PsIn input) : SV_Target {
    float4 color = float4(0.0, 0.0, 0.0, 0.0);
    if (hasTexture != 0) {
        color = sourceTexture.Sample(sourceSampler, input.uv);
        color.rgb *= color.a;
    }

    if (showBorder != 0) {
        float edge = min(
            min(input.position.x, viewport.x - input.position.x),
            min(input.position.y, viewport.y - input.position.y)
        );
        float dash = fmod(input.position.x + input.position.y, 18.0);
        if (edge <= 2.5 && dash < 10.0) {
            color = float4(0.0, 0.6708, 0.86, 0.86);
        }
    }

    if (showCloseButton != 0) {
        float2 buttonMin = float2(viewport.x - 32.0, 0.0);
        float2 buttonMax = float2(viewport.x, 26.0);
        bool insideButton =
            input.position.x >= buttonMin.x && input.position.x < buttonMax.x &&
            input.position.y >= buttonMin.y && input.position.y < buttonMax.y;
        if (insideButton) {
            color = closeButtonHovered != 0
                ? float4(0.86, 0.055, 0.055, 0.92)
                : float4(0.025, 0.055, 0.075, 0.78);
            float2 cross = input.position.xy - float2(viewport.x - 16.0, 13.0);
            float diagonal = min(abs(cross.x - cross.y), abs(cross.x + cross.y));
            if (max(abs(cross.x), abs(cross.y)) <= 7.0 && diagonal <= 1.6) {
                color = float4(0.96, 0.98, 1.0, 1.0);
            }
        }
    }
    return color;
}
)hlsl";

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

ComPtr<ID3DBlob> CompileShader(const char* source, const char* profile) {
    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
    const HRESULT result = D3DCompile(
        source,
        std::strlen(source),
        nullptr,
        nullptr,
        nullptr,
        "main",
        profile,
        flags,
        0,
        &shader,
        &errors
    );
    if (FAILED(result)) {
        std::string detail;
        if (errors) {
            detail.assign(
                static_cast<const char*>(errors->GetBufferPointer()),
                errors->GetBufferSize()
            );
        }
        throw std::runtime_error(
            std::string("Shader compilation failed: ") + HresultText(result) + " " + detail
        );
    }
    return shader;
}

struct FrameConstants {
    float viewport[2];
    std::uint32_t showBorder;
    std::uint32_t hasTexture;
    std::uint32_t showCloseButton;
    std::uint32_t closeButtonHovered;
    float padding[2];
};
static_assert(sizeof(FrameConstants) == 32);

struct Metric {
    double totalMs = 0.0;
    double maxMs = 0.0;

    void Add(double valueMs) {
        totalMs += valueMs;
        maxMs = std::max(maxMs, valueMs);
    }

    void Reset() {
        totalMs = 0.0;
        maxMs = 0.0;
    }
};

double Milliseconds(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

}  // namespace

class OverlayApp {
public:
    int Run(HINSTANCE instance) {
        instance_ = instance;
        ConfigureProcess();
        CreateOverlayWindow();
        CreateGraphics();

        RegisterHotKey(hwnd_, kHotkeyId, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'L');
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        SetWindowPos(
            hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW
        );
        std::cout << "Ctrl+Alt+L to toggle click-through lock.\n";
        std::cout << "[dx11] DirectComposition + non-blocking DXGI Present active\n";

        statsStarted_ = Clock::now();
        auto nextFrame = statsStarted_;
        MSG message{};

        while (running_) {
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    running_ = false;
                    break;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (!running_) {
                break;
            }

            const auto now = Clock::now();
            if (now >= nextFrame) {
                RenderFrame();
                nextFrame += std::chrono::duration_cast<Clock::duration>(
                    std::chrono::duration<double>(kFrameSeconds)
                );
                if (now - nextFrame > std::chrono::milliseconds(50)) {
                    nextFrame = now + std::chrono::duration_cast<Clock::duration>(
                        std::chrono::duration<double>(kFrameSeconds)
                    );
                }
                continue;
            }

            const auto waitDuration = nextFrame - Clock::now();
            const auto waitMs = std::clamp<LONG>(
                static_cast<LONG>(std::ceil(
                    std::chrono::duration<double, std::milli>(waitDuration).count()
                )),
                0,
                16
            );
            MsgWaitForMultipleObjectsEx(
                0, nullptr, static_cast<DWORD>(waitMs), QS_ALLINPUT,
                MWMO_INPUTAVAILABLE | MWMO_ALERTABLE
            );
        }

        Shutdown();
        return 0;
    }

    HWND Window() const noexcept {
        return hwnd_;
    }

private:
    void ConfigureProcess() {
        SetConsoleOutputCP(CP_UTF8);
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        timeBeginPeriod(1);
        timerResolutionActive_ = true;

        PROCESS_POWER_THROTTLING_STATE throttling{};
        throttling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
        throttling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        throttling.StateMask = 0;
        SetProcessInformation(
            GetCurrentProcess(), ProcessPowerThrottling,
            &throttling, sizeof(throttling)
        );
    }

    void CreateOverlayWindow() {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = &OverlayApp::StaticWndProc;
        windowClass.hInstance = instance_;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.lpszClassName = kWindowClass;
        if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            throw std::runtime_error("RegisterClassExW failed");
        }

        const DWORD extendedStyle =
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP;
        hwnd_ = CreateWindowExW(
            extendedStyle,
            kWindowClass,
            kWindowTitle,
            WS_POPUP,
            100,
            100,
            kInitialWidth,
            kInitialHeight,
            nullptr,
            nullptr,
            instance_,
            this
        );
        if (!hwnd_) {
            throw std::runtime_error("CreateWindowExW failed");
        }
    }

    void CreateGraphics() {
        UINT factoryFlags = 0;
        ComPtr<IDXGIFactory6> factory6;
        Check(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory6)), "CreateDXGIFactory2");

        int senderAdapter = -1;
        char activeSender[256]{};
        if (receiver_.GetActiveSender(activeSender)) {
            senderAdapter = receiver_.GetSenderAdapter(activeSender);
        }

        ComPtr<IDXGIAdapter1> adapter;
        if (senderAdapter >= 0) {
            factory6->EnumAdapters1(static_cast<UINT>(senderAdapter), &adapter);
        }
        if (!adapter) {
            for (UINT index = 0;
                 factory6->EnumAdapterByGpuPreference(
                     index,
                     DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                     IID_PPV_ARGS(&adapter)
                 ) != DXGI_ERROR_NOT_FOUND;
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
            Check(factory6->EnumAdapters1(0, &adapter), "EnumAdapters1");
        }

        DXGI_ADAPTER_DESC1 adapterDescription{};
        adapter->GetDesc1(&adapterDescription);
        std::wcout << L"[dx11] adapter=" << adapterDescription.Description << L"\n";

        constexpr D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };
        D3D_FEATURE_LEVEL selectedFeatureLevel{};
        Check(
            D3D11CreateDevice(
                adapter.Get(),
                D3D_DRIVER_TYPE_UNKNOWN,
                nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                featureLevels,
                ARRAYSIZE(featureLevels),
                D3D11_SDK_VERSION,
                &device_,
                &selectedFeatureLevel,
                &context_
            ),
            "D3D11CreateDevice"
        );

        if (!receiver_.OpenDirectX11(device_.Get())) {
            throw std::runtime_error("SpoutDX could not use the D3D11 device");
        }

        RECT client{};
        GetClientRect(hwnd_, &client);
        renderWidth_ = std::max<LONG>(1, client.right - client.left);
        renderHeight_ = std::max<LONG>(1, client.bottom - client.top);

        DXGI_SWAP_CHAIN_DESC1 swapDescription{};
        swapDescription.Width = static_cast<UINT>(renderWidth_);
        swapDescription.Height = static_cast<UINT>(renderHeight_);
        swapDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swapDescription.Stereo = FALSE;
        swapDescription.SampleDesc.Count = 1;
        swapDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDescription.BufferCount = 2;
        swapDescription.Scaling = DXGI_SCALING_STRETCH;
        swapDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swapDescription.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        swapDescription.Flags = kSwapChainFlags;

        Check(
            factory6->CreateSwapChainForComposition(
                device_.Get(), &swapDescription, nullptr, &swapChain_
            ),
            "CreateSwapChainForComposition"
        );
        swapChain_.As(&swapChain2_);
        swapChain_.As(&swapChain3_);
        if (swapChain2_) {
            swapChain2_->SetMaximumFrameLatency(1);
        }

        ComPtr<IDXGIDevice> dxgiDevice;
        Check(device_.As(&dxgiDevice), "Query IDXGIDevice");

        // Match VTube Studio's GPUManagementPlugin. CPU process priority and
        // GPU context priority are independent; an uncapped game can otherwise
        // starve this device even while our receive/draw work stays sub-ms.
        Check(
            dxgiDevice->SetGPUThreadPriority(kGpuThreadPriority),
            "SetGPUThreadPriority(7)"
        );
        INT appliedGpuPriority = 0;
        Check(
            dxgiDevice->GetGPUThreadPriority(&appliedGpuPriority),
            "GetGPUThreadPriority"
        );
        if (appliedGpuPriority != kGpuThreadPriority) {
            throw std::runtime_error(
                "Driver did not retain GPU priority 7; actual=" +
                std::to_string(appliedGpuPriority)
            );
        }
        std::cout << "[dx11] GPU thread priority=" << appliedGpuPriority
                  << " (VTube Studio mode)\n";

        Check(
            DCompositionCreateDevice(
                dxgiDevice.Get(), IID_PPV_ARGS(&compositionDevice_)
            ),
            "DCompositionCreateDevice"
        );
        Check(
            compositionDevice_->CreateTargetForHwnd(hwnd_, TRUE, &compositionTarget_),
            "CreateTargetForHwnd"
        );
        Check(compositionDevice_->CreateVisual(&compositionVisual_), "CreateVisual");
        Check(compositionVisual_->SetContent(swapChain_.Get()), "SetContent");
        Check(compositionTarget_->SetRoot(compositionVisual_.Get()), "SetRoot");
        Check(compositionDevice_->Commit(), "DirectComposition Commit");

        CreateRenderTargets();
        CreatePipeline();
    }

    void CreateRenderTargets() {
        for (auto& target : renderTargets_) {
            target.Reset();
        }
        // Direct3D 11 remaps flip-model buffer zero after Present. Buffers with
        // higher indexes are read-only, so a single RTV for index zero is the
        // correct D3D11 model (unlike D3D12's explicit per-buffer tracking).
        ComPtr<ID3D11Texture2D> buffer;
        Check(swapChain_->GetBuffer(0, IID_PPV_ARGS(&buffer)), "GetBuffer[0]");
        Check(
            device_->CreateRenderTargetView(buffer.Get(), nullptr, &renderTargets_[0]),
            "CreateRenderTargetView[0]"
        );
    }

    void CreatePipeline() {
        const auto vertexBytecode = CompileShader(kVertexShader, "vs_5_0");
        const auto pixelBytecode = CompileShader(kPixelShader, "ps_5_0");
        Check(
            device_->CreateVertexShader(
                vertexBytecode->GetBufferPointer(), vertexBytecode->GetBufferSize(),
                nullptr, &vertexShader_
            ),
            "CreateVertexShader"
        );
        Check(
            device_->CreatePixelShader(
                pixelBytecode->GetBufferPointer(), pixelBytecode->GetBufferSize(),
                nullptr, &pixelShader_
            ),
            "CreatePixelShader"
        );

        D3D11_SAMPLER_DESC samplerDescription{};
        samplerDescription.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDescription.MaxLOD = D3D11_FLOAT32_MAX;
        Check(
            device_->CreateSamplerState(&samplerDescription, &sampler_),
            "CreateSamplerState"
        );

        D3D11_BLEND_DESC blendDescription{};
        auto& targetBlend = blendDescription.RenderTarget[0];
        targetBlend.BlendEnable = TRUE;
        targetBlend.SrcBlend = D3D11_BLEND_ONE;
        targetBlend.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        targetBlend.BlendOp = D3D11_BLEND_OP_ADD;
        targetBlend.SrcBlendAlpha = D3D11_BLEND_ONE;
        targetBlend.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        targetBlend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        targetBlend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        Check(
            device_->CreateBlendState(&blendDescription, &blendState_),
            "CreateBlendState"
        );

        D3D11_BUFFER_DESC constantDescription{};
        constantDescription.ByteWidth = sizeof(FrameConstants);
        constantDescription.Usage = D3D11_USAGE_DYNAMIC;
        constantDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constantDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        Check(
            device_->CreateBuffer(&constantDescription, nullptr, &constantBuffer_),
            "CreateBuffer"
        );
    }

    void ResizeSwapChainIfNeeded() {
        RECT client{};
        if (!GetClientRect(hwnd_, &client)) {
            return;
        }
        const LONG width = std::max<LONG>(1, client.right - client.left);
        const LONG height = std::max<LONG>(1, client.bottom - client.top);
        if (width == renderWidth_ && height == renderHeight_) {
            return;
        }

        context_->OMSetRenderTargets(0, nullptr, nullptr);
        for (auto& target : renderTargets_) {
            target.Reset();
        }
        const HRESULT result = swapChain_->ResizeBuffers(
            2,
            static_cast<UINT>(width),
            static_cast<UINT>(height),
            DXGI_FORMAT_B8G8R8A8_UNORM,
            kSwapChainFlags
        );
        if (FAILED(result)) {
            std::cerr << "[dx11] ResizeBuffers failed: " << HresultText(result) << "\n";
            return;
        }
        renderWidth_ = width;
        renderHeight_ = height;
        CreateRenderTargets();
    }

    void UpdateReceiver() {
        const auto started = Clock::now();
        const bool received = receiver_.ReceiveTexture();
        receiveMetric_.Add(Milliseconds(started, Clock::now()));
        if (!received) {
            sourceView_.Reset();
            sourceTexture_ = nullptr;
            senderConnected_ = false;
            return;
        }

        ++spoutOkCount_;
        if (receiver_.IsFrameNew()) {
            ++spoutNewCount_;
        }
        ID3D11Texture2D* texture = receiver_.GetSenderTexture();
        const bool changed = receiver_.IsUpdated() || texture != sourceTexture_;
        if (changed) {
            sourceView_.Reset();
            sourceTexture_ = texture;
            if (texture) {
                D3D11_SHADER_RESOURCE_VIEW_DESC viewDescription{};
                viewDescription.Format = receiver_.GetSenderFormat();
                viewDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                viewDescription.Texture2D.MostDetailedMip = 0;
                viewDescription.Texture2D.MipLevels = 1;
                const HRESULT result = device_->CreateShaderResourceView(
                    texture, &viewDescription, &sourceView_
                );
                if (FAILED(result)) {
                    std::cerr << "[dx11] CreateShaderResourceView failed: "
                              << HresultText(result) << "\n";
                    sourceView_.Reset();
                }
            }

            const UINT width = receiver_.GetSenderWidth();
            const UINT height = receiver_.GetSenderHeight();
            if (width > 16 && height > 16 &&
                (width != senderWidth_ || height != senderHeight_)) {
                senderWidth_ = width;
                senderHeight_ = height;
                aspectRatio_ = static_cast<double>(width) / static_cast<double>(height);
                ApplySenderAspect();
                RECT client{};
                GetClientRect(hwnd_, &client);
                std::cout << "[dx11 spout] sender=" << receiver_.GetSenderName()
                          << " " << width << "x" << height
                          << ", window=" << (client.right - client.left)
                          << "x" << (client.bottom - client.top)
                          << ", format=" << static_cast<unsigned>(receiver_.GetSenderFormat())
                          << "\n";
            }
        }
        senderConnected_ = sourceView_ != nullptr;
    }

    void ApplySenderAspect() {
        if (aspectRatio_ <= 0.0 || inSizeMove_) {
            return;
        }
        RECT window{};
        if (!GetWindowRect(hwnd_, &window)) {
            return;
        }
        const int width = window.right - window.left;
        const int height = std::max(1, static_cast<int>(std::lround(width / aspectRatio_)));
        SetWindowPos(
            hwnd_, nullptr, 0, 0, width, height,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE
        );
    }

    bool ShouldShowBorder() const {
        if (locked_) {
            return false;
        }
        POINT cursor{};
        RECT window{};
        return GetCursorPos(&cursor) && GetWindowRect(hwnd_, &window) &&
               PtInRect(&window, cursor);
    }

    bool IsCloseButtonScreenPoint(POINT cursor) const {
        if (locked_) {
            return false;
        }
        RECT window{};
        return GetWindowRect(hwnd_, &window) &&
               cursor.x >= window.right - kCloseButtonWidth && cursor.x < window.right &&
               cursor.y >= window.top && cursor.y < window.top + kCloseButtonHeight;
    }

    bool IsCloseButtonClientPoint(POINT cursor) const {
        if (locked_) {
            return false;
        }
        RECT client{};
        return GetClientRect(hwnd_, &client) &&
               cursor.x >= client.right - kCloseButtonWidth && cursor.x < client.right &&
               cursor.y >= client.top && cursor.y < client.top + kCloseButtonHeight;
    }

    bool IsCloseButtonHovered() const {
        POINT cursor{};
        return GetCursorPos(&cursor) && IsCloseButtonScreenPoint(cursor);
    }

    void DrawFrame() {
        const auto started = Clock::now();
        auto* target = renderTargets_[0].Get();
        constexpr float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        context_->OMSetRenderTargets(1, &target, nullptr);
        context_->ClearRenderTargetView(target, clearColor);

        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(renderWidth_);
        viewport.Height = static_cast<float>(renderHeight_);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        context_->RSSetViewports(1, &viewport);

        D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(
            context_->Map(constantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped),
            "Map frame constants"
        );
        auto* constants = static_cast<FrameConstants*>(mapped.pData);
        constants->viewport[0] = static_cast<float>(renderWidth_);
        constants->viewport[1] = static_cast<float>(renderHeight_);
        const bool showBorder = ShouldShowBorder();
        constants->showBorder = showBorder ? 1U : 0U;
        constants->hasTexture = senderConnected_ ? 1U : 0U;
        constants->showCloseButton = showBorder ? 1U : 0U;
        constants->closeButtonHovered = IsCloseButtonHovered() ? 1U : 0U;
        constants->padding[0] = 0.0f;
        constants->padding[1] = 0.0f;
        context_->Unmap(constantBuffer_.Get(), 0);

        context_->IASetInputLayout(nullptr);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
        context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
        auto* sampler = sampler_.Get();
        context_->PSSetSamplers(0, 1, &sampler);
        auto* source = sourceView_.Get();
        context_->PSSetShaderResources(0, 1, &source);
        auto* constantsBuffer = constantBuffer_.Get();
        context_->PSSetConstantBuffers(0, 1, &constantsBuffer);
        constexpr float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        context_->OMSetBlendState(blendState_.Get(), blendFactor, 0xFFFFFFFF);
        context_->Draw(3, 0);

        ID3D11ShaderResourceView* noSource = nullptr;
        context_->PSSetShaderResources(0, 1, &noSource);
        drawMetric_.Add(Milliseconds(started, Clock::now()));
    }

    void RenderFrame() {
        try {
            ++requestCount_;
            ResizeSwapChainIfNeeded();
            if (!renderTargets_[0]) {
                return;
            }
            UpdateReceiver();
            DrawFrame();

            const auto presentStarted = Clock::now();
            const HRESULT result = swapChain_->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
            presentMetric_.Add(Milliseconds(presentStarted, Clock::now()));
            if (result == DXGI_ERROR_WAS_STILL_DRAWING) {
                ++presentBusyCount_;
            }
            else if (SUCCEEDED(result)) {
                ++presentOkCount_;
            }
            else if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET) {
                std::cerr << "[dx11] graphics device lost: " << HresultText(result) << "\n";
                PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            }
            else {
                ++presentErrorCount_;
            }
            PrintStatsIfNeeded();
        }
        catch (const std::exception& error) {
            std::cerr << "[dx11] render failed: " << error.what() << "\n";
            PostMessageW(hwnd_, WM_CLOSE, 0, 0);
        }
    }

    void PrintStatsIfNeeded() {
        const auto now = Clock::now();
        const double seconds = std::chrono::duration<double>(now - statsStarted_).count();
        if (seconds < kStatsSeconds) {
            return;
        }
        const auto average = [this](const Metric& metric) {
            return requestCount_ ? metric.totalMs / static_cast<double>(requestCount_) : 0.0;
        };
        std::cout << std::fixed << std::setprecision(1)
                  << "[dx11 perf] render=" << presentOkCount_ / seconds << " fps"
                  << " request=" << requestCount_ / seconds << " fps"
                  << " spout_ok=" << spoutOkCount_ / seconds << " fps"
                  << " spout_new=" << spoutNewCount_ / seconds << " fps"
                  << " present_busy=" << presentBusyCount_ / seconds << " fps"
                  << " present_err=" << presentErrorCount_
                  << std::setprecision(2)
                  << " | receive=" << average(receiveMetric_) << "/" << receiveMetric_.maxMs << " ms"
                  << " draw=" << average(drawMetric_) << "/" << drawMetric_.maxMs << " ms"
                  << " present=" << average(presentMetric_) << "/" << presentMetric_.maxMs << " ms"
                  << " avg/max\n";

        statsStarted_ = now;
        requestCount_ = 0;
        spoutOkCount_ = 0;
        spoutNewCount_ = 0;
        presentOkCount_ = 0;
        presentBusyCount_ = 0;
        presentErrorCount_ = 0;
        receiveMetric_.Reset();
        drawMetric_.Reset();
        presentMetric_.Reset();
    }

    void ToggleLocked() {
        locked_ = !locked_;
        LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
        if (locked_) {
            style |= WS_EX_TRANSPARENT;
        }
        else {
            style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
        }
        SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, style);
        SetWindowPos(
            hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED
        );
        std::cout << "[dx11] click-through " << (locked_ ? "locked" : "unlocked") << "\n";
    }

    LRESULT HitTest(POINT cursor) const {
        if (locked_) {
            return HTTRANSPARENT;
        }
        if (IsCloseButtonScreenPoint(cursor)) {
            return HTCLIENT;
        }
        RECT window{};
        GetWindowRect(hwnd_, &window);
        const UINT dpi = GetDpiForWindow(hwnd_);
        const int grip = std::max(8, MulDiv(10, static_cast<int>(dpi), 96));
        const bool left = cursor.x < window.left + grip;
        const bool right = cursor.x >= window.right - grip;
        const bool top = cursor.y < window.top + grip;
        const bool bottom = cursor.y >= window.bottom - grip;
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

    void EnforceSizingAspect(WPARAM edge, RECT& rectangle) const {
        if (aspectRatio_ <= 0.0) {
            return;
        }
        int width = rectangle.right - rectangle.left;
        int height = rectangle.bottom - rectangle.top;
        const bool verticalDriven = edge == WMSZ_TOP || edge == WMSZ_BOTTOM;
        if (verticalDriven) {
            height = std::max(height, static_cast<int>(std::lround(kMinimumWidth / aspectRatio_)));
            width = std::max(kMinimumWidth, static_cast<int>(std::lround(height * aspectRatio_)));
        }
        else {
            width = std::max(width, kMinimumWidth);
            height = std::max(1, static_cast<int>(std::lround(width / aspectRatio_)));
        }

        if (edge == WMSZ_LEFT || edge == WMSZ_TOPLEFT || edge == WMSZ_BOTTOMLEFT) {
            rectangle.left = rectangle.right - width;
        }
        else {
            rectangle.right = rectangle.left + width;
        }
        if (edge == WMSZ_TOP || edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT) {
            rectangle.top = rectangle.bottom - height;
        }
        else {
            rectangle.bottom = rectangle.top + height;
        }
    }

    LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_HOTKEY:
            if (wParam == kHotkeyId) {
                ToggleLocked();
                return 0;
            }
            break;
        case WM_NCHITTEST: {
            POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            return HitTest(cursor);
        }
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_LBUTTONDOWN: {
            POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (IsCloseButtonClientPoint(cursor)) {
                closeButtonPressed_ = true;
                SetCapture(hwnd_);
                return 0;
            }
            break;
        }
        case WM_LBUTTONUP:
            if (closeButtonPressed_) {
                POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                const bool shouldClose = IsCloseButtonClientPoint(cursor);
                closeButtonPressed_ = false;
                if (GetCapture() == hwnd_) {
                    ReleaseCapture();
                }
                if (shouldClose) {
                    PostMessageW(hwnd_, WM_CLOSE, 0, 0);
                }
                return 0;
            }
            break;
        case WM_CAPTURECHANGED:
            closeButtonPressed_ = false;
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_ENTERSIZEMOVE:
            inSizeMove_ = true;
            return 0;
        case WM_EXITSIZEMOVE:
            inSizeMove_ = false;
            return 0;
        case WM_SIZING:
            EnforceSizingAspect(wParam, *reinterpret_cast<RECT*>(lParam));
            return TRUE;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = kMinimumWidth;
            info->ptMinTrackSize.y = std::max(
                1, static_cast<int>(std::lround(kMinimumWidth / aspectRatio_))
            );
            return 0;
        }
        case WM_DPICHANGED: {
            const auto* suggested = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(
                hwnd_, nullptr,
                suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE
            );
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd_);
            return 0;
        case WM_DESTROY:
            running_ = false;
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }

    static LRESULT CALLBACK StaticWndProc(
        HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam
    ) {
        OverlayApp* app = nullptr;
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            app = static_cast<OverlayApp*>(create->lpCreateParams);
            app->hwnd_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }
        else {
            app = reinterpret_cast<OverlayApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }
        return app ? app->WndProc(message, wParam, lParam)
                   : DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void Shutdown() {
        if (hwnd_) {
            UnregisterHotKey(hwnd_, kHotkeyId);
        }
        sourceView_.Reset();
        sourceTexture_ = nullptr;
        receiver_.ReleaseReceiver();
        receiver_.CloseDirectX11();
        if (context_) {
            context_->ClearState();
            context_->Flush();
        }
        if (timerResolutionActive_) {
            timeEndPeriod(1);
            timerResolutionActive_ = false;
        }
    }

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    bool running_ = true;
    bool locked_ = false;
    bool inSizeMove_ = false;
    bool closeButtonPressed_ = false;
    bool timerResolutionActive_ = false;
    bool senderConnected_ = false;
    LONG renderWidth_ = kInitialWidth;
    LONG renderHeight_ = kInitialHeight;
    UINT senderWidth_ = 0;
    UINT senderHeight_ = 0;
    double aspectRatio_ = static_cast<double>(kInitialWidth) / kInitialHeight;

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain1> swapChain_;
    ComPtr<IDXGISwapChain2> swapChain2_;
    ComPtr<IDXGISwapChain3> swapChain3_;
    ComPtr<IDCompositionDevice> compositionDevice_;
    ComPtr<IDCompositionTarget> compositionTarget_;
    ComPtr<IDCompositionVisual> compositionVisual_;
    ComPtr<ID3D11RenderTargetView> renderTargets_[2];
    ComPtr<ID3D11VertexShader> vertexShader_;
    ComPtr<ID3D11PixelShader> pixelShader_;
    ComPtr<ID3D11SamplerState> sampler_;
    ComPtr<ID3D11BlendState> blendState_;
    ComPtr<ID3D11Buffer> constantBuffer_;
    ComPtr<ID3D11ShaderResourceView> sourceView_;
    ID3D11Texture2D* sourceTexture_ = nullptr;
    spoutDX receiver_;

    Clock::time_point statsStarted_{};
    std::uint64_t requestCount_ = 0;
    std::uint64_t spoutOkCount_ = 0;
    std::uint64_t spoutNewCount_ = 0;
    std::uint64_t presentOkCount_ = 0;
    std::uint64_t presentBusyCount_ = 0;
    std::uint64_t presentErrorCount_ = 0;
    Metric receiveMetric_;
    Metric drawMetric_;
    Metric presentMetric_;
};

namespace {

OverlayApp* gApp = nullptr;

BOOL WINAPI ConsoleHandler(DWORD event) {
    if ((event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT) &&
        gApp && gApp->Window()) {
        PostMessageW(gApp->Window(), WM_CLOSE, 0, 0);
        return TRUE;
    }
    return FALSE;
}

}  // namespace

int wmain() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    std::wcout << std::unitbuf;
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    try {
        OverlayApp app;
        gApp = &app;
        SetConsoleCtrlHandler(ConsoleHandler, TRUE);
        const int result = app.Run(GetModuleHandleW(nullptr));
        SetConsoleCtrlHandler(ConsoleHandler, FALSE);
        gApp = nullptr;
        if (SUCCEEDED(comResult)) {
            CoUninitialize();
        }
        return result;
    }
    catch (const std::exception& error) {
        std::cerr << "[dx11] startup failed: " << error.what() << "\n";
        if (SUCCEEDED(comResult)) {
            CoUninitialize();
        }
        return 1;
    }
}
