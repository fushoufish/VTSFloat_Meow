#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#include "SpoutDX.h"

using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;

extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int main(int argc, char** argv) {
    constexpr UINT width = 1280;
    constexpr UINT height = 773;
    const int seconds = argc > 1 ? (std::max)(1, std::atoi(argv[1])) : 60;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL featureLevel{};
    const HRESULT created = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &device,
        &featureLevel,
        &context);
    if (FAILED(created)) {
        std::cerr << "D3D11CreateDevice failed\n";
        return 1;
    }

    D3D11_TEXTURE2D_DESC description{};
    description.Width = width;
    description.Height = height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(device->CreateTexture2D(&description, nullptr, &texture))) {
        std::cerr << "CreateTexture2D failed\n";
        return 1;
    }

    spoutDX sender;
    if (!sender.OpenDirectX11(device.Get())) {
        std::cerr << "Spout OpenDirectX11 failed\n";
        return 1;
    }
    sender.SetSenderFormat(description.Format);
    if (!sender.SetSenderName("VTubeStudioSpout")) {
        std::cerr << "Spout SetSenderName failed\n";
        return 1;
    }

    std::vector<std::uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    const auto started = Clock::now();
    auto nextFrame = started;
    std::uint64_t frame = 0;
    while (Clock::now() - started < std::chrono::seconds(seconds)) {
        const auto now = Clock::now();
        if (now < nextFrame) {
            std::this_thread::sleep_until(nextFrame);
        }
        nextFrame += std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(1.0 / 60.0));

        std::fill(pixels.begin(), pixels.end(), 0);
        const double phase = static_cast<double>(frame) / 60.0;
        const int centerX = static_cast<int>(width * (0.5 + 0.28 * std::sin(phase * 1.7)));
        const int centerY = static_cast<int>(height * (0.5 + 0.22 * std::cos(phase * 1.3)));
        constexpr int radius = 105;
        for (int y = (std::max)(0, centerY - radius); y < (std::min)(static_cast<int>(height), centerY + radius); ++y) {
            for (int x = (std::max)(0, centerX - radius); x < (std::min)(static_cast<int>(width), centerX + radius); ++x) {
                const int dx = x - centerX;
                const int dy = y - centerY;
                const int distanceSquared = dx * dx + dy * dy;
                if (distanceSquared >= radius * radius) {
                    continue;
                }
                const float edge = 1.0f - std::sqrt(static_cast<float>(distanceSquared)) / radius;
                const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
                pixels[offset + 0] = static_cast<std::uint8_t>(50 + 100 * edge);
                pixels[offset + 1] = static_cast<std::uint8_t>(170 + 80 * edge);
                pixels[offset + 2] = 255;
                pixels[offset + 3] = static_cast<std::uint8_t>(80 + 175 * edge);
            }
        }

        context->UpdateSubresource(texture.Get(), 0, nullptr, pixels.data(), width * 4, 0);
        if (!sender.SendTexture(texture.Get())) {
            std::cerr << "Spout SendTexture failed at frame " << frame << "\n";
            sender.ReleaseSender();
            return 1;
        }
        ++frame;
    }

    sender.ReleaseSender();
    std::cout << "sent " << frame << " frames\n";
    return 0;
}
