#include "anr_frame_generation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
#include <emmintrin.h>

#ifdef VTSFLOAT_ANR_NGX
#include <windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXPackedVector.h>
#include <wrl/client.h>

#include <nvsdk_ngx_defs_dlssg.h>
#endif

namespace vtsfloat {
namespace {

struct ScaleSample {
    int first;
    int second;
    unsigned fraction;
};

std::vector<ScaleSample> AnrScaleSamples(int sourceExtent, int outputExtent) {
    std::vector<ScaleSample> samples(outputExtent);
    for (int i = 0; i < outputExtent; ++i) {
        const auto position = (std::clamp)(
            (2LL * i + 1) * sourceExtent * 128 / outputExtent - 128,
            0LL, (sourceExtent - 1) * 256LL);
        const int first = static_cast<int>(position / 256);
        samples[i] = {first, (std::min)(first + 1, sourceExtent - 1),
            static_cast<unsigned>(position % 256)};
    }
    return samples;
}

// Blend each BGRA channel with the same weights so RGB stays <= alpha.
__m128i BlendAnrPixels(__m128i first, __m128i second, unsigned fraction) {
    const auto sum = _mm_add_epi16(_mm_mullo_epi16(first, _mm_set1_epi16(256 - fraction)),
        _mm_mullo_epi16(second, _mm_set1_epi16(fraction)));
    return _mm_srli_epi16(_mm_add_epi16(sum, _mm_set1_epi16(128)), 8);
}

}  // namespace

void ResizeAnrFrame(const AnrFrame& source, int width, int height, void* destination) {
    if (source.width == width && source.height == height) {
        std::memcpy(destination, source.pixels.data(), source.pixels.size());
        return;
    }
    const auto columns = AnrScaleSamples(source.width, width);
    const auto rows = AnrScaleSamples(source.height, height);
    const auto zero = _mm_setzero_si128();
    const size_t stride = static_cast<size_t>(width) * 4;
    auto* output = static_cast<std::uint8_t*>(destination);
    // At most two input rows are needed per output row. Reuse rows when
    // enlarging, and interpolate the contiguous result four pixels at a time.
    std::array<std::vector<std::uint32_t>, 2> horizontal{
        std::vector<std::uint32_t>(width), std::vector<std::uint32_t>(width)};
    std::array<int, 2> cached{-1, -1};
    const auto rowAt = [&](int row) {
        const auto index = row % 2;
        auto* pixels = horizontal[index].data();
        if (cached[index] != row) {
            const auto* input = source.pixels.data() + static_cast<size_t>(row) * source.width * 4;
            for (int x = 0; x < width; ++x) {
                const auto [left, right, fraction] = columns[x];
                int first, second;
                std::memcpy(&first, input + static_cast<size_t>(left) * 4, sizeof(first));
                std::memcpy(&second, input + static_cast<size_t>(right) * 4, sizeof(second));
                const auto mixed = BlendAnrPixels(_mm_unpacklo_epi8(_mm_cvtsi32_si128(first), zero),
                    _mm_unpacklo_epi8(_mm_cvtsi32_si128(second), zero), fraction);
                pixels[x] = static_cast<std::uint32_t>(_mm_cvtsi128_si32(_mm_packus_epi16(mixed, zero)));
            }
            cached[index] = row;
        }
        return pixels;
    };
    for (const auto [top, bottom, fraction] : rows) {
        const auto* first = rowAt(top);
        if (!fraction) {
            std::memcpy(output, first, stride);
        } else {
            const auto* second = rowAt(bottom);
            int x = 0;
            for (; x + 4 <= width; x += 4) {
                const auto a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(first + x));
                const auto b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(second + x));
                const auto low = BlendAnrPixels(_mm_unpacklo_epi8(a, zero), _mm_unpacklo_epi8(b, zero), fraction);
                const auto high = BlendAnrPixels(_mm_unpackhi_epi8(a, zero), _mm_unpackhi_epi8(b, zero), fraction);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(output + size_t(x) * 4), _mm_packus_epi16(low, high));
            }
            for (; x < width; ++x) {
                const auto a = _mm_unpacklo_epi8(_mm_cvtsi32_si128(static_cast<int>(first[x])), zero);
                const auto b = _mm_unpacklo_epi8(_mm_cvtsi32_si128(static_cast<int>(second[x])), zero);
                const int mixed = _mm_cvtsi128_si32(_mm_packus_epi16(BlendAnrPixels(a, b, fraction), zero));
                std::memcpy(output + size_t(x) * 4, &mixed, sizeof(mixed));
            }
        }
        output += stride;
    }
}

#ifdef VTSFLOAT_ANR_NGX
namespace {

using Microsoft::WRL::ComPtr;
using Byte = std::uint8_t;

void Check(HRESULT result, const char* operation) {
    if (FAILED(result)) throw std::runtime_error(std::string(operation) + " (" + std::to_string(result) + ")");
}

void CheckNgx(NVSDK_NGX_Result result, const char* operation) {
    if (NVSDK_NGX_FAILED(result)) {
        throw std::runtime_error(std::string(operation) + " (" + std::to_string(static_cast<unsigned>(result)) + ")");
    }
}

// The provider uses the public NGX parameter interface and looks up values by
// name. The application owns these parameters as well as the D3D12 resources.
class Parameters final : public NVSDK_NGX_Parameter {
    void Set(const char* name, unsigned long long value) override { numbers_[name] = static_cast<double>(value); }
    void Set(const char* name, float value) override { numbers_[name] = value; }
    void Set(const char* name, double value) override { numbers_[name] = value; }
    void Set(const char* name, unsigned value) override { numbers_[name] = value; }
    void Set(const char* name, int value) override { numbers_[name] = value; }
    void Set(const char* name, ID3D11Resource* value) override { pointers_[name] = value; }
    void Set(const char* name, ID3D12Resource* value) override { pointers_[name] = value; }
    void Set(const char* name, void* value) override { pointers_[name] = value; }
    NVSDK_NGX_Result Get(const char* name, unsigned long long* value) const override { return Read(numbers_, name, value); }
    NVSDK_NGX_Result Get(const char* name, float* value) const override { return Read(numbers_, name, value); }
    NVSDK_NGX_Result Get(const char* name, double* value) const override { return Read(numbers_, name, value); }
    NVSDK_NGX_Result Get(const char* name, unsigned* value) const override { return Read(numbers_, name, value); }
    NVSDK_NGX_Result Get(const char* name, int* value) const override { return Read(numbers_, name, value); }
    NVSDK_NGX_Result Get(const char* name, ID3D11Resource** value) const override { return Read(pointers_, name, value); }
    NVSDK_NGX_Result Get(const char* name, ID3D12Resource** value) const override { return Read(pointers_, name, value); }
    NVSDK_NGX_Result Get(const char* name, void** value) const override { return Read(pointers_, name, value); }
    void Reset() override { numbers_.clear(); pointers_.clear(); }

    template<class Map, class T>
    static NVSDK_NGX_Result Read(const Map& map, const char* name, T* value) {
        const auto at = map.find(name);
        if (at == map.end() || !value) return NVSDK_NGX_Result_Fail;
        *value = static_cast<T>(at->second);
        return NVSDK_NGX_Result_Success;
    }

    std::unordered_map<std::string, double> numbers_;
    std::unordered_map<std::string, void*> pointers_;
};

struct Event {
    HANDLE handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ~Event() { if (handle) CloseHandle(handle); }
};

struct Module {
    HMODULE handle = nullptr;
    ~Module() { if (handle) FreeLibrary(handle); }
    template<class T> T Require(const char* name) const {
        const auto method = reinterpret_cast<T>(GetProcAddress(handle, name));
        if (!method) throw std::runtime_error(std::string("ANR provider missing ") + name);
        return method;
    }
};

struct Surface {
    ComPtr<ID3D12Resource> resource;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
};

POINTF AlphaCenter(const AnrFrame& frame) {
    float xSum = 0, ySum = 0, total = 0;
    for (int y = 0; y < frame.height; y += 4) {
        for (int x = 0; x < frame.width; x += 4) {
            const float alpha = frame.pixels[(static_cast<size_t>(y) * frame.width + x) * 4 + 3];
            xSum += x * alpha;
            ySum += y * alpha;
            total += alpha;
        }
    }
    return total > 0 ? POINTF{xSum / total, ySum / total} : POINTF{};
}

class AnrProvider {
public:
    explicit AnrProvider(const std::filesystem::path& dll) {
        module_.handle = LoadLibraryExW(dll.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module_.handle) throw std::runtime_error("Cannot load ANR provider (" + std::to_string(GetLastError()) + ")");
        create_ = module_.Require<Create>("NVSDK_NGX_D3D12_CreateFeature");
        evaluate_ = module_.Require<Evaluate>("NVSDK_NGX_D3D12_EvaluateFeature");
        release_ = module_.Require<Release>("NVSDK_NGX_D3D12_ReleaseFeature");
        shutdown_ = reinterpret_cast<Shutdown>(GetProcAddress(module_.handle, "NVSDK_NGX_D3D12_Shutdown1"));
        const auto initialize = module_.Require<Initialize>("NVSDK_NGX_D3D12_Init_Ext");

        ComPtr<IDXGIFactory6> factory;
        Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "ANR DXGI");
        for (UINT i = 0;; ++i) {
            ComPtr<IDXGIAdapter1> candidate;
            if (FAILED(factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&candidate)))) break;
            DXGI_ADAPTER_DESC1 desc{};
            if (FAILED(candidate->GetDesc1(&desc)) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) continue;
            if (SUCCEEDED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device_)))) {
                adapter_ = desc.Description;
                break;
            }
        }
        if (!device_) throw std::runtime_error("No D3D12 adapter for ANR");
        D3D12_COMMAND_QUEUE_DESC queueDescription{};
        queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Check(device_->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue_)), "ANR command queue");
        Check(device_->CreateCommandAllocator(queueDescription.Type, IID_PPV_ARGS(&allocator_)), "ANR command allocator");
        Check(device_->CreateCommandList(0, queueDescription.Type, allocator_.Get(), nullptr, IID_PPV_ARGS(&list_)),
            "ANR command list");
        Check(list_->Close(), "ANR close command list");
        Check(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "ANR fence");
        if (!event_.handle) throw std::runtime_error("ANR fence event");
        wchar_t temporary[MAX_PATH]{};
        GetTempPathW(MAX_PATH, temporary);
        CheckNgx(initialize(1, temporary, device_.Get(), NVSDK_NGX_Version_API, nullptr), "ANR initialization");
        using Populate = NVSDK_NGX_Result(NVSDK_CONV*)(NVSDK_NGX_Parameter*);
        if (const auto populate = reinterpret_cast<Populate>(
                GetProcAddress(module_.handle, "NVSDK_NGX_D3D12_PopulateParameters_Impl"))) {
            Parameters capabilityValues;
            NVSDK_NGX_Parameter& capabilities = capabilityValues;
            unsigned maxGenerated = 0;
            if (!NVSDK_NGX_FAILED(populate(&capabilities)) &&
                !NVSDK_NGX_FAILED(capabilities.Get(NVSDK_NGX_DLSSG_Parameter_MultiFrameCountMax, &maxGenerated))) {
                // The bundled V.B15 model has no 1/7 and 1/8 phases even
                // if its provider INI advertises a larger capability.
                maxMultiplier_ = 1 + static_cast<int>((std::clamp)(
                    maxGenerated, 1u, static_cast<unsigned>(kAnrMaxMultiplier - 1)));
            }
        }
    }

    ~AnrProvider() {
        if (feature_) release_(feature_);
        if (shutdown_) shutdown_(device_.Get());
    }

    const std::wstring& Adapter() const { return adapter_; }
    int MaxMultiplier() const { return maxMultiplier_; }

    void Generate(const AnrFrame& input, bool reset, int multiplier, std::vector<AnrFrame>& output) {
        if (input.width != width_ || input.height != height_) {
            Configure(input.width, input.height);
            reset = true;
        }
        const unsigned count = (std::min)(multiplier, maxMultiplier_) - 1;
        EnsureGeneratedOutput(count);
        const auto center = AlphaCenter(input);
        const float dx = reset ? 0.0f : (std::clamp)(previousCenter_.x - center.x, -64.f, 64.f);
        const float dy = reset ? 0.0f : (std::clamp)(previousCenter_.y - center.y, -64.f, 64.f);
        previousCenter_ = center;
        Upload(input, dx, dy);
        Begin();
        CopyInto(color_);
        CopyInto(depth_);
        CopyInto(motion_);
        Transition(color_.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        NVSDK_NGX_Parameter& parameters = parameters_;
        parameters.Set(NVSDK_NGX_DLSSG_Parameter_Reset, reset ? 1u : 0u);
        parameters.Set(NVSDK_NGX_DLSSG_Parameter_MultiFrameCount, count);
        for (unsigned i = 0; i < count; ++i) {
            parameters.Set(NVSDK_NGX_DLSSG_Parameter_MultiFrameIndex, i + 1);
            parameters.Set(NVSDK_NGX_DLSSG_Parameter_OutputInterpolated, generated_[i].resource.Get());
            CheckNgx(evaluate_(list_.Get(), feature_, &parameters_, nullptr), "ANR evaluate");
        }
        if (!reset) {
            for (unsigned i = 0; i < count; ++i) {
                CopyGeneratedFrame(generated_[i]);
            }
        }
        Transition(color_.resource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        Execute();
        if (!reset) Readback(count, output);
    }

private:
    using Initialize = NVSDK_NGX_Result(NVSDK_CONV*)(unsigned long long, const wchar_t*, ID3D12Device*,
        NVSDK_NGX_Version, const NVSDK_NGX_FeatureCommonInfo*);
    using Create = NVSDK_NGX_Result(NVSDK_CONV*)(ID3D12GraphicsCommandList*, NVSDK_NGX_Feature,
        NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
    using Evaluate = NVSDK_NGX_Result(NVSDK_CONV*)(ID3D12GraphicsCommandList*, const NVSDK_NGX_Handle*,
        NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback);
    using Release = NVSDK_NGX_Result(NVSDK_CONV*)(NVSDK_NGX_Handle*);
    using Shutdown = NVSDK_NGX_Result(NVSDK_CONV*)(ID3D12Device*);

    void Begin() {
        Check(allocator_->Reset(), "ANR reset allocator");
        Check(list_->Reset(allocator_.Get(), nullptr), "ANR reset list");
    }

    void Execute() {
        Check(list_->Close(), "ANR close list");
        ID3D12CommandList* submitted = list_.Get();
        queue_->ExecuteCommandLists(1, &submitted);
        Check(queue_->Signal(fence_.Get(), ++sequence_), "ANR signal fence");
        Check(fence_->SetEventOnCompletion(sequence_, event_.handle), "ANR fence completion");
        if (WaitForSingleObject(event_.handle, 5000) != WAIT_OBJECT_0) {
            throw std::runtime_error("ANR GPU timed out");
        }
        Check(device_->GetDeviceRemovedReason(), "ANR device");
    }

    void Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
        list_->ResourceBarrier(1, &barrier);
    }

    ComPtr<ID3D12Resource> Resource(const D3D12_RESOURCE_DESC& desc, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state) {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = type;
        heap.CreationNodeMask = heap.VisibleNodeMask = 1;
        ComPtr<ID3D12Resource> resource;
        Check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr,
            IID_PPV_ARGS(&resource)), "Allocate ANR resource");
        return resource;
    }

    Surface Texture(DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, UINT64& next) {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = atlasWidth_;
        desc.Height = atlasHeight_;
        desc.DepthOrArraySize = desc.MipLevels = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Flags = flags;
        Surface surface;
        surface.resource = Resource(desc, D3D12_HEAP_TYPE_DEFAULT, state);
        UINT64 total = 0;
        device_->GetCopyableFootprints(&desc, 0, 1, next, &surface.layout, nullptr, nullptr, &total);
        next = (next + total + 511) & ~UINT64{511};
        return surface;
    }

    ComPtr<ID3D12Resource> Buffer(UINT64 bytes, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state) {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = bytes;
        desc.Height = desc.DepthOrArraySize = desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        return Resource(desc, type, state);
    }

    void Configure(int width, int height) {
        width_ = width;
        height_ = height;
        // Place the model and its grayscale matte in the same generation
        // pass with a black strip between them.
        matteX_ = (width + 32 + 7) & ~7;
        atlasWidth_ = (std::max)(640, matteX_ + ((width + 7) & ~7));
        atlasHeight_ = (std::max)(360, (height + 7) & ~7);
        if (atlasWidth_ > 7680 || atlasHeight_ > 4320) throw std::runtime_error("ANR atlas exceeds provider limits");
        if (feature_) release_(std::exchange(feature_, nullptr));
        UINT64 uploadSize = 0, unused = 0;
        color_ = Texture(DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, uploadSize);
        depth_ = Texture(DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
            D3D12_RESOURCE_STATE_COPY_DEST, uploadSize);
        motion_ = Texture(DXGI_FORMAT_R16G16_FLOAT, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, uploadSize);
        generated_.clear();
        download_.Reset();
        real_ = Texture(DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, unused);
        upload_ = Buffer(uploadSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        NVSDK_NGX_Parameter& parameters = parameters_;
        parameters.Reset();
        const auto w = static_cast<unsigned>(atlasWidth_);
        const auto h = static_cast<unsigned>(atlasHeight_);
        parameters.Set(NVSDK_NGX_Parameter_CreationNodeMask, 1u);
        parameters.Set(NVSDK_NGX_Parameter_VisibilityNodeMask, 1u);
        parameters.Set(NVSDK_NGX_Parameter_Width, w);
        parameters.Set(NVSDK_NGX_Parameter_Height, h);
        parameters.Set(NVSDK_NGX_DLSSG_Parameter_Width, w);
        parameters.Set(NVSDK_NGX_DLSSG_Parameter_Height, h);
        parameters.Set(NVSDK_NGX_DLSSG_Parameter_InternalWidth, w);
        parameters.Set(NVSDK_NGX_DLSSG_Parameter_InternalHeight, h);
        parameters.Set(NVSDK_NGX_DLSSG_Parameter_BackbufferFormat, static_cast<unsigned>(DXGI_FORMAT_R8G8B8A8_UNORM));
        parameters.Set(NVSDK_NGX_DLSSG_Parameter_DynamicResolution, 0u);
        Begin();
        CheckNgx(create_(list_.Get(), NVSDK_NGX_Feature_FrameGeneration, &parameters_, &feature_), "ANR create feature");
        if (!feature_) throw std::runtime_error("ANR returned no feature");
        Execute();
        parameters.Set(NVSDK_NGX_DLSSG_Parameter_Backbuffer, color_.resource.Get());
        parameters.Set(NVSDK_NGX_DLSSG_Parameter_Depth, depth_.resource.Get());
        parameters.Set(NVSDK_NGX_DLSSG_Parameter_MVecs, motion_.resource.Get());
        parameters.Set(NVSDK_NGX_DLSSG_Parameter_MvecScaleX, 1.f);
        parameters.Set(NVSDK_NGX_DLSSG_Parameter_MvecScaleY, 1.f);
        parameters.Set(NVSDK_NGX_DLSSG_Parameter_OutputReal, real_.resource.Get());
    }

    void EnsureGeneratedOutput(unsigned count) {
        if (generated_.size() >= count) return;
        generated_.clear();
        UINT64 bytes = 0;
        for (unsigned i = 0; i < count; ++i) {
            generated_.push_back(Texture(DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, bytes));
        }
        download_ = Buffer(bytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
    }

    void CopyGeneratedFrame(const Surface& surface) {
        Transition(surface.resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION from{};
        from.pResource = surface.resource.Get();
        from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION to{};
        to.pResource = download_.Get();
        to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        to.PlacedFootprint = surface.layout;
        list_->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
        Transition(surface.resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    void CopyInto(const Surface& surface) {
        D3D12_TEXTURE_COPY_LOCATION to{};
        to.pResource = surface.resource.Get();
        to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION from{};
        from.pResource = upload_.Get();
        from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        from.PlacedFootprint = surface.layout;
        list_->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
    }

    static Byte* Row(Byte* base, const Surface& surface, int y) {
        return base + surface.layout.Offset + static_cast<size_t>(y) * surface.layout.Footprint.RowPitch;
    }

    void Upload(const AnrFrame& input, float dx, float dy) {
        Byte* mapped = nullptr;
        const D3D12_RANGE noReads{0, 0};
        Check(upload_->Map(0, &noReads, reinterpret_cast<void**>(&mapped)), "ANR map upload");
        const auto motion = static_cast<std::uint32_t>(DirectX::PackedVector::XMConvertFloatToHalf(dx)) |
            (static_cast<std::uint32_t>(DirectX::PackedVector::XMConvertFloatToHalf(dy)) << 16);
        for (int y = 0; y < atlasHeight_; ++y) {
            auto* color = Row(mapped, color_, y);
            auto* depth = reinterpret_cast<float*>(Row(mapped, depth_, y));
            auto* vectors = reinterpret_cast<std::uint32_t*>(Row(mapped, motion_, y));
            std::fill_n(reinterpret_cast<std::uint32_t*>(color), atlasWidth_, 0xff000000);
            std::fill_n(depth, atlasWidth_, 1.f);
            std::fill_n(vectors, atlasWidth_, 0u);
            if (y >= input.height) continue;
            const auto* inputRow = input.pixels.data() + static_cast<size_t>(y) * input.width * 4;
            auto* mask = color + matteX_ * 4;
            for (int x = 0; x < input.width; ++x) {
                const int index = x * 4;
                const Byte alpha = inputRow[index + 3];
                color[index + 0] = inputRow[index + 2];
                color[index + 1] = inputRow[index + 1];
                color[index + 2] = inputRow[index + 0];
                mask[index + 0] = mask[index + 1] = mask[index + 2] = alpha;
                if (alpha > 0) {
                    depth[x] = depth[matteX_ + x] = .5f;
                    vectors[x] = vectors[matteX_ + x] = motion;
                }
            }
        }
        upload_->Unmap(0, nullptr);
    }

    void Readback(unsigned count, std::vector<AnrFrame>& output) {
        output.resize(count);
        for (auto& frame : output) {
            frame.width = width_;
            frame.height = height_;
            frame.pixels.resize(static_cast<size_t>(width_) * height_ * 4);
        }
        Byte* mapped = nullptr;
        Check(download_->Map(0, nullptr, reinterpret_cast<void**>(&mapped)), "ANR map generated frame");
        for (unsigned i = 0; i < count; ++i) {
            for (int y = 0; y < height_; ++y) {
                const auto* color = Row(mapped, generated_[i], y);
                const auto* mask = color + matteX_ * 4;
                auto* destination = output[i].pixels.data() + static_cast<size_t>(y) * width_ * 4;
                for (int x = 0; x < width_ * 4; x += 4) {
                    const Byte alpha = mask[x];
                    destination[x + 0] = (std::min)(color[x + 2], alpha);
                    destination[x + 1] = (std::min)(color[x + 1], alpha);
                    destination[x + 2] = (std::min)(color[x + 0], alpha);
                    destination[x + 3] = alpha;
                }
            }
        }
        const D3D12_RANGE noWrites{0, 0};
        download_->Unmap(0, &noWrites);
    }

    Module module_;
    Create create_ = nullptr;
    Evaluate evaluate_ = nullptr;
    Release release_ = nullptr;
    Shutdown shutdown_ = nullptr;
    NVSDK_NGX_Handle* feature_ = nullptr;
    Parameters parameters_;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<ID3D12CommandAllocator> allocator_;
    ComPtr<ID3D12GraphicsCommandList> list_;
    ComPtr<ID3D12Fence> fence_;
    Event event_;
    UINT64 sequence_ = 0;
    ComPtr<ID3D12Resource> upload_, download_;
    Surface color_, depth_, motion_, real_;
    std::vector<Surface> generated_;
    std::wstring adapter_;
    POINTF previousCenter_{};
    int width_ = 0, height_ = 0, atlasWidth_ = 0, atlasHeight_ = 0, matteX_ = 0;
    int maxMultiplier_ = 2;
};

}  // namespace
#endif

struct AnrFrameStream::State {
    struct Input {
        AnrFrame frame;
        std::uint64_t epoch;
        int multiplier;
        int sourceLongestEdge;
    };

    void Run(const std::filesystem::path& providerPath) {
#ifdef VTSFLOAT_ANR_NGX
        try {
            AnrProvider provider(providerPath);
            {
                std::lock_guard lock(mutex);
                adapter = provider.Adapter();
                maxMultiplier = provider.MaxMultiplier();
            }
            std::optional<Input> previous;
            AnrFrame inference;
            for (;;) {
                Input input;
                {
                    std::unique_lock lock(mutex);
                    ready.wait(lock, [&] { return stopped || pending.has_value(); });
                    if (stopped) return;
                    input = std::move(*pending);
                    pending.reset();
                }
                const auto started = AnrClock::now();
                const AnrFrame& frame = input.frame;
                const double longest = (std::max)(frame.width, frame.height);
                const double shortest = (std::min)(frame.width, frame.height);
                const double scale = (std::min)({1.0, input.sourceLongestEdge / longest,
                    960.0 / longest, 540.0 / shortest});
                const int inferenceWidth = (std::max)(1, static_cast<int>(std::lround(frame.width * scale)));
                const int inferenceHeight = (std::max)(1, static_cast<int>(std::lround(frame.height * scale)));
                const bool reset = !previous || previous->epoch != input.epoch ||
                    previous->frame.width != frame.width || previous->frame.height != frame.height ||
                    inference.width != inferenceWidth || inference.height != inferenceHeight ||
                    frame.captured - previous->frame.captured > std::chrono::milliseconds(125);
                if (!reset && previous->frame.pixels == frame.pixels) continue;
                inference.width = inferenceWidth;
                inference.height = inferenceHeight;
                inference.pixels.resize(static_cast<size_t>(inferenceWidth) * inferenceHeight * 4);
                ResizeAnrFrame(frame, inferenceWidth, inferenceHeight, inference.pixels.data());
                AnrFramePair pair;
                provider.Generate(inference, reset, input.multiplier, pair.generated);
                if (!reset && !pair.generated.empty()) {
                    pair.interval = frame.captured - previous->frame.captured;
                    const auto steps = static_cast<int>(pair.generated.size() + 1);
                    const auto readyInterval = frame.ready - previous->frame.ready;
                    for (int i = 1; i < steps; ++i) {
                        pair.generated[i - 1].captured = previous->frame.captured + pair.interval * i / steps;
                        pair.generated[i - 1].ready = previous->frame.ready + readyInterval * i / steps;
                    }
                    pair.real = std::move(previous->frame);
                    pair.processingMs = std::chrono::duration<double, std::milli>(AnrClock::now() - started).count();
                    std::lock_guard lock(mutex);
                    if (epoch == input.epoch) completed = std::move(pair);
                }
                previous = std::move(input);
            }
        } catch (const std::exception& failure) {
            std::lock_guard lock(mutex);
            error = failure.what();
            pending.reset();
            completed.reset();
        }
#else
        (void)providerPath;
#endif
    }

    mutable std::mutex mutex;
    std::condition_variable ready;
    std::optional<Input> pending;
    std::optional<AnrFramePair> completed;
    std::wstring adapter;
    int maxMultiplier = 2;
    std::string error;
    std::uint64_t epoch = 0;
    bool stopped = false;
    std::thread worker;
};

AnrFrameStream::AnrFrameStream(std::filesystem::path provider) : state_(std::make_unique<State>()) {
    if (!Built()) throw std::runtime_error("ANR support was not built");
    state_->worker = std::thread([state = state_.get(), path = std::move(provider)] { state->Run(path); });
}

AnrFrameStream::~AnrFrameStream() {
    {
        std::lock_guard lock(state_->mutex);
        state_->stopped = true;
    }
    state_->ready.notify_one();
    if (state_->worker.joinable()) state_->worker.join();
}

bool AnrFrameStream::Built() {
#ifdef VTSFLOAT_ANR_NGX
    return true;
#else
    return false;
#endif
}

void AnrFrameStream::Submit(int width, int height, int sourceLongestEdge, const void* source,
    AnrClock::time_point captured, int multiplier) {
    if (!source || width <= 0 || height <= 0 || sourceLongestEdge <= 0) return;
    const auto* pixels = static_cast<const std::uint8_t*>(source);
    AnrFrame frame{width, height, captured, AnrClock::now(), {pixels, pixels + static_cast<size_t>(width) * height * 4}};
    {
        std::lock_guard lock(state_->mutex);
        if (!state_->error.empty()) return;
        state_->pending = State::Input{std::move(frame), state_->epoch, multiplier, sourceLongestEdge};
    }
    state_->ready.notify_one();
}

bool AnrFrameStream::Take(AnrFramePair& pair) {
    std::lock_guard lock(state_->mutex);
    if (!state_->completed) return false;
    pair = std::move(*state_->completed);
    state_->completed.reset();
    return true;
}

void AnrFrameStream::Reset() {
    std::lock_guard lock(state_->mutex);
    ++state_->epoch;
    state_->pending.reset();
    state_->completed.reset();
}

std::string AnrFrameStream::Error() const {
    std::lock_guard lock(state_->mutex);
    return state_->error;
}

std::wstring AnrFrameStream::Adapter() const {
    std::lock_guard lock(state_->mutex);
    return state_->adapter;
}

int AnrFrameStream::MaxMultiplier() const {
    std::lock_guard lock(state_->mutex);
    return state_->maxMultiplier;
}

}  // namespace vtsfloat
