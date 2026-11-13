#pragma once

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

namespace vtsfloat {

inline constexpr UINT kBackdropSampleSize = 32;
using BackdropSamples = std::array<uint8_t, kBackdropSampleSize * kBackdropSampleSize * 4>;

class MonitorImageCapture {
public:
    void Stop() noexcept {
        try { if (session_) session_.Close(); } catch (...) {}
        frame_ = nullptr;
        try { if (pool_) pool_.Close(); } catch (...) {}
        session_ = nullptr;
        pool_ = nullptr;
        device_ = nullptr;
        rect_ = {};
        sourceBounds_ = {};
        grid_ = {};
    }

    ~MonitorImageCapture() { Stop(); }

    bool Start(HMONITOR monitor, ID3D11Device* graphics) {
        Stop();
        if (!monitor || !graphics) return false;
        try {
            using namespace winrt::Windows::Graphics::Capture;
            if (!GraphicsCaptureSession::IsSupported()) return false;
            const auto factory = winrt::get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
            GraphicsCaptureItem item{nullptr};
            winrt::check_hresult(factory->CreateForMonitor(monitor,
                winrt::guid_of<GraphicsCaptureItem>(), winrt::put_abi(item)));
            winrt::com_ptr<IDXGIDevice> dxgi;
            winrt::check_hresult(graphics->QueryInterface(dxgi.put()));
            winrt::com_ptr<::IInspectable> inspectable;
            winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(), inspectable.put()));
            device_ = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
            frameSize_ = item.Size();
            pool_ = Direct3D11CaptureFramePool::CreateFreeThreaded(device_, kFormat, 2, frameSize_);
            session_ = pool_.CreateCaptureSession(item);
            try { session_.IsCursorCaptureEnabled(false); } catch (const winrt::hresult_error&) {}
            session_.StartCapture();
            return true;
        } catch (const winrt::hresult_error&) {
            Stop();
            return false;
        }
    }

    bool Active() const { return session_ != nullptr; }

    void Update(const RECT& output, RECT monitor, ID3D11Device* graphics, ID3D11DeviceContext* context) {
        const RECT previous = rect_;
        rect_ = {};
        if (!session_) return;
        try {
            bool updated = false;
            for (int i = 0; i < 2; ++i) {
                auto next = pool_.TryGetNextFrame();
                if (!next) break;
                updated = true;
                const auto size = next.ContentSize();
                if (size.Width <= 0 || size.Height <= 0) return;
                if (size.Width != frameSize_.Width || size.Height != frameSize_.Height) {
                    next = nullptr;
                    frame_ = nullptr;
                    frameSize_ = size;
                    pool_.Recreate(device_, kFormat, 2, frameSize_);
                    return;
                }
                frame_ = std::move(next);
            }
            if (!frame_) return;
            monitor.right = (std::min)(monitor.right, monitor.left + frameSize_.Width);
            monitor.bottom = (std::min)(monitor.bottom, monitor.top + frameSize_.Height);
            RECT overlap{};
            if (!IntersectRect(&overlap, &monitor, &output)) return;
            if (!updated && EqualRect(&overlap, &previous) && EqualRect(&monitor, &sourceBounds_)) {
                rect_ = previous;
                return;
            }

            using Access = ::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess;
            const auto access = frame_.Surface().as<Access>();
            winrt::com_ptr<ID3D11Texture2D> source;
            winrt::check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D), source.put_void()));
            const D3D11_BOX sourceRect{UINT(overlap.left - monitor.left), UINT(overlap.top - monitor.top), 0,
                UINT(overlap.right - monitor.left), UINT(overlap.bottom - monitor.top), 1};
            ReadSampleGrid(source.get(), sourceRect, graphics, context);
            rect_ = overlap;
            sourceBounds_ = monitor;
        } catch (const winrt::hresult_error&) {
            Stop();
        }
    }

    const RECT& Bounds() const { return rect_; }
    const BackdropSamples& Samples() const { return samples_; }

private:
    void ReadSampleGrid(ID3D11Texture2D* source, const D3D11_BOX& box,
                         ID3D11Device* graphics, ID3D11DeviceContext* context) {
        if (!grid_.shader) {
            static constexpr char shader[] = R"(
                Texture2D<float4> backdrop : register(t0);
                RWTexture2D<float4> grid : register(u0);
                [numthreads(8, 8, 1)]
                void main(uint3 id : SV_DispatchThreadID) {
                    uint2 sourceSize, gridSize;
                    backdrop.GetDimensions(sourceSize.x, sourceSize.y);
                    grid.GetDimensions(gridSize.x, gridSize.y);
                    float2 scale = float2(sourceSize) / float2(gridSize);
                    float4 color = 0;
                    [unroll] for (uint y = 0; y < 4; ++y) {
                        [unroll] for (uint x = 0; x < 4; ++x) {
                            uint2 coord = min(sourceSize - 1, uint2((id.xy + (float2(x, y) + .5) / 4) * scale));
                            color += backdrop.Load(int3(coord, 0));
                        }
                    }
                    grid[id.xy] = float4(color.bgr / 16, 1);
                })";
            winrt::com_ptr<ID3DBlob> compiled;
            winrt::check_hresult(D3DCompile(shader, sizeof(shader) - 1, nullptr, nullptr, nullptr,
                "main", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, compiled.put(), nullptr));
            winrt::check_hresult(graphics->CreateComputeShader(compiled->GetBufferPointer(), compiled->GetBufferSize(),
                nullptr, grid_.shader.put()));
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = desc.Height = kBackdropSampleSize;
            desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
            winrt::check_hresult(graphics->CreateTexture2D(&desc, nullptr, grid_.grid.put()));
            winrt::check_hresult(graphics->CreateUnorderedAccessView(grid_.grid.get(), nullptr, grid_.gridView.put()));
            desc.BindFlags = 0;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            winrt::check_hresult(graphics->CreateTexture2D(&desc, nullptr, grid_.readback.put()));
        }
        const UINT width = box.right - box.left, height = box.bottom - box.top;
        if (!grid_.texture || width != grid_.width || height != grid_.height) {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            grid_.texture = nullptr;
            grid_.view = nullptr;
            winrt::check_hresult(graphics->CreateTexture2D(&desc, nullptr, grid_.texture.put()));
            winrt::check_hresult(graphics->CreateShaderResourceView(grid_.texture.get(), nullptr, grid_.view.put()));
            grid_.width = width;
            grid_.height = height;
        }
        context->CopySubresourceRegion(grid_.texture.get(), 0, 0, 0, 0, source, 0, &box);
        ID3D11ShaderResourceView* input = grid_.view.get();
        ID3D11UnorderedAccessView* output = grid_.gridView.get();
        context->CSSetShader(grid_.shader.get(), nullptr, 0);
        context->CSSetShaderResources(0, 1, &input);
        context->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
        context->Dispatch(kBackdropSampleSize / 8, kBackdropSampleSize / 8, 1);
        input = nullptr;
        output = nullptr;
        context->CSSetShaderResources(0, 1, &input);
        context->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
        context->CSSetShader(nullptr, nullptr, 0);
        context->CopyResource(grid_.readback.get(), grid_.grid.get());
        constexpr size_t rowSize = kBackdropSampleSize * 4;
        D3D11_MAPPED_SUBRESOURCE mapped{};
        winrt::check_hresult(context->Map(grid_.readback.get(), 0, D3D11_MAP_READ, 0, &mapped));
        const auto* bits = static_cast<const uint8_t*>(mapped.pData);
        for (UINT y = 0; y < kBackdropSampleSize; ++y) {
            std::memcpy(samples_.data() + y * rowSize, bits + size_t(y) * mapped.RowPitch, rowSize);
        }
        context->Unmap(grid_.readback.get(), 0);
    }

    struct SampleGrid {
        winrt::com_ptr<ID3D11ComputeShader> shader;
        winrt::com_ptr<ID3D11Texture2D> texture, grid, readback;
        winrt::com_ptr<ID3D11ShaderResourceView> view;
        winrt::com_ptr<ID3D11UnorderedAccessView> gridView;
        UINT width = 0, height = 0;
    } grid_;
    static constexpr auto kFormat = winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice device_{nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool pool_{nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession session_{nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame frame_{nullptr};
    winrt::Windows::Graphics::SizeInt32 frameSize_{};
    RECT rect_{};
    RECT sourceBounds_{};
    BackdropSamples samples_{};
};

class ImageBackdrop {
public:
    ~ImageBackdrop() { Stop(); }

    void Stop() noexcept {
        monitors_.clear();
        compositableBounds_ = {};
    }

    bool Update(const RECT& output, ID3D11Device* graphics, ID3D11DeviceContext* context) {
        compositableBounds_ = {};
        if (!graphics || !context) return false;
        struct OutputMonitor { HMONITOR handle; RECT bounds; };
        std::vector<OutputMonitor> displays;
        EnumDisplayMonitors(nullptr, &output, [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
            MONITORINFO info{sizeof(MONITORINFO)};
            if (GetMonitorInfoW(monitor, &info)) {
                reinterpret_cast<std::vector<OutputMonitor>*>(data)->push_back({monitor, info.rcMonitor});
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&displays));
        for (auto it = monitors_.begin(); it != monitors_.end();) {
            const bool present = std::any_of(displays.begin(), displays.end(),
                [&](const auto& display) { return display.handle == it->first; });
            if (present) ++it;
            else it = monitors_.erase(it);
        }
        if (displays.empty()) {
            Stop();
            return true;
        }

        const auto now = std::chrono::steady_clock::now();
        bool active = false;
        bool complete = true;
        for (const auto& display : displays) {
            auto& slot = monitors_[display.handle];
            if (!slot.capture.Active() && now >= slot.retryAt) {
                if (!slot.capture.Start(display.handle, graphics)) slot.retryAt = now + std::chrono::seconds(2);
            }
            if (slot.capture.Active()) slot.capture.Update(output, display.bounds, graphics, context);
            active |= slot.capture.Active();
            RECT expected{};
            IntersectRect(&expected, &display.bounds, &output);
            complete &= EqualRect(&expected, &slot.capture.Bounds()) != FALSE;
        }
        if (!active) Stop();
        else if (complete) compositableBounds_ = output;
        return active;
    }

    bool Matches(const RECT& output) const { return EqualRect(&compositableBounds_, &output) != FALSE; }

    template <typename Callback>
    void ForEachRegion(Callback&& apply) const {
        for (const auto& [monitor, slot] : monitors_) {
            if (!IsRectEmpty(&slot.capture.Bounds())) apply(slot.capture.Bounds(), slot.capture.Samples());
        }
    }

private:
    struct Slot {
        MonitorImageCapture capture;
        std::chrono::steady_clock::time_point retryAt{};
    };
    std::map<HMONITOR, Slot> monitors_;
    RECT compositableBounds_{};
};

}  // namespace vtsfloat
