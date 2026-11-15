#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace vtsfloat {

using AnrClock = std::chrono::steady_clock;
inline constexpr int kAnrMaxMultiplier = 6;

struct AnrFrame {
    int width = 0;
    int height = 0;
    AnrClock::time_point captured{};
    AnrClock::time_point ready{};
    std::vector<std::uint8_t> pixels;
};

struct AnrFramePair {
    AnrFrame real;
    std::vector<AnrFrame> generated;
    AnrClock::duration interval{};
    double processingMs = 0.0;
};

// Both source and destination are tightly packed, premultiplied BGRA.
void ResizeAnrFrame(const AnrFrame& source, int width, int height, void* destination);

// The render thread owns the stream. The provider and its device run on a
// worker with a single pending input and a single completed pair.
class AnrFrameStream {
public:
    explicit AnrFrameStream(std::filesystem::path provider);
    ~AnrFrameStream();
    static bool Built();
    void Submit(int width, int height, int sourceLongestEdge, const void* premultipliedBgra,
        AnrClock::time_point captured, int multiplier);
    bool Take(AnrFramePair& pair);
    void Reset();
    std::string Error() const;
    std::wstring Adapter() const;
    int MaxMultiplier() const;

private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace vtsfloat
