#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace vtsfloat {

struct ImageShadow {
    bool enabled = false;
    int angle = 45;
    int distance = 900;  // Hundredths of an output pixel.
    int spread = 3;
    int blur = 10;
    int opacity = 14;
    unsigned color = 0;
    bool sampleBackground = false;
    int darkness = 70;
};

class ShadowMask {
public:
    static constexpr int scale = 2;

    void Build(const uint8_t* pixels, int width, int height, int spread, int blur, bool inside) {
        outside_ = inside ? 255 : 0;
        width_ = (width + scale - 1) / scale;
        height_ = (height + scale - 1) / scale;
        const size_t count = size_t(width_) * height_;
        mask_.resize(count);
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                unsigned sum = 0, samples = 0;
                for (int dy = 0; dy < scale && y * scale + dy < height; ++dy) {
                    for (int dx = 0; dx < scale && x * scale + dx < width; ++dx) {
                        sum += pixels[(size_t(y * scale + dy) * width + x * scale + dx) * 4 + 3];
                        ++samples;
                    }
                }
                mask_[size_t(y) * width_ + x] = static_cast<uint8_t>(inside ? 255 - (sum + samples / 2) / samples : (sum + samples / 2) / samples);
            }
        }
        if (!spread && !blur) return;
        scratch_.resize(count);
        if (spread) {
            queue_.resize((std::max)(width_, height_));
            Filter<true>(mask_, scratch_, width_, height_, false, (spread + 1) / scale);
            Filter<true>(scratch_, mask_, height_, width_, true, (spread + 1) / scale);
        }
        if (blur) {
            const int radius = (blur + scale) / (scale + 1);
            for (int pass = 0; pass < 2; ++pass) {
                Filter<false>(mask_, scratch_, width_, height_, false, radius);
                Filter<false>(scratch_, mask_, height_, width_, true, radius);
            }
        }
    }

    unsigned Sample(int left, int top, unsigned fx, unsigned fy) const {
        const unsigned a = At(left, top) * (256 - fx) + At(left + 1, top) * fx;
        const unsigned b = At(left, top + 1) * (256 - fx) + At(left + 1, top + 1) * fx;
        return (a * (256 - fy) + b * fy + 32768) / 65536;
    }

    bool Touches(int left, int top, int right, int bottom) const {
        if (right < -scale || bottom < -scale || left >= (width_ + 1) * scale ||
            top >= (height_ + 1) * scale) return false;
        for (int y = (std::max)(0, top / scale - 1); y < (std::min)(height_, bottom / scale + 2); ++y) {
            for (int x = (std::max)(0, left / scale - 1); x < (std::min)(width_, right / scale + 2); ++x) {
                if (At(x, y)) return true;
            }
        }
        return false;
    }

private:
    unsigned At(int x, int y) const {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) return outside_;
        return mask_[size_t(y) * width_ + x];
    }

    template<bool Maximum>
    void Filter(const std::vector<uint8_t>& input, std::vector<uint8_t>& output,
                int length, int lines, bool vertical, int radius) {
        const int stride = vertical ? width_ : 1;
        for (int line = 0; line < lines; ++line) {
            const size_t offset = vertical ? line : size_t(line) * width_;
            const auto value = [&](int index) { return input[offset + size_t(index) * stride]; };
            int head = 0, tail = 0, sum = 0, right = 0;
            for (int i = 0; i < length; ++i) {
                const int begin = i - radius;
                if constexpr (Maximum) {
                    if (head < tail && queue_[head] < begin) ++head;
                    while (right < length && right <= i + radius) {
                        while (tail > head && value(queue_[tail - 1]) <= value(right)) --tail;
                        queue_[tail++] = right++;
                    }
                } else {
                    if (begin > 0) sum -= value(begin - 1);
                    while (right < length && right <= i + radius) sum += value(right++);
                }
                output[offset + size_t(i) * stride] = static_cast<uint8_t>(
                    Maximum ? (begin < 0 || i + radius >= length ? (std::max)(int(value(queue_[head])), outside_) :
                        value(queue_[head])) : (sum + radius + outside_ * (2 * radius + 1 -
                            (right - (std::max)(0, begin)))) / (2 * radius + 1));
            }
        }
    }

    int outside_ = 0;
    int width_ = 0, height_ = 0;
    std::vector<uint8_t> mask_, scratch_;
    std::vector<int> queue_;
};

}  // namespace vtsfloat
