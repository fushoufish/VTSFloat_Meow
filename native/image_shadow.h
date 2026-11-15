#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <ppl.h>

namespace vtsfloat {

struct ImageShadow {
    bool enabled = false;
    bool fadeOnHover = false;
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
        ForBands(height_, [&](int begin, int end) {
            for (int y = begin; y < end; ++y) {
                const auto* top = pixels + size_t(y * scale) * width * 4 + 3;
                const auto* bottom = y * scale + 1 < height ? top + size_t(width) * 4 : top;
                auto* row = mask_.data() + size_t(y) * width_;
                for (int x = 0; x < width_; ++x) {
                    const size_t left = size_t(x) * scale * 4;
                    const size_t right = x * scale + 1 < width ? left + 4 : left;
                    const unsigned alpha = (top[left] + top[right] + bottom[left] + bottom[right] + 2) / 4;
                    row[x] = static_cast<uint8_t>(inside ? 255 - alpha : alpha);
                }
            }
        });
        if (!spread && !blur) return;
        scratch_.resize(count);
        if (spread) {
            const int radius = (spread + 1) / scale;
            if (radius <= 2) {
                SpreadSmall<true>(mask_, scratch_, radius);
                SpreadSmall<false>(scratch_, mask_, radius);
            } else {
                queue_.resize((std::max)(width_, height_));
                Spread(mask_, scratch_, width_, height_, false, radius);
                Spread(scratch_, mask_, height_, width_, true, radius);
            }
        }
        if (blur) {
            const int radius = (blur + scale) / (scale + 1);
            for (int pass = 0; pass < 2; ++pass) {
                BlurHorizontal(radius);
                BlurVertical(radius);
            }
        }
    }

    void Stabilize(float weight) {
        if (history_.size() != mask_.size() || weight >= 1.0f) {
            history_.assign(mask_.begin(), mask_.end());
            return;
        }
        ForBands(height_, [&](int begin, int end) {
            for (size_t i = size_t(begin) * width_, last = size_t(end) * width_; i < last; ++i) {
                float& previous = history_[i];
                previous += (mask_[i] - previous) * weight;
                mask_[i] = static_cast<uint8_t>(previous + 0.5f);
            }
        });
    }

    unsigned Sample(int left, int top, unsigned fx, unsigned fy) const {
        if (left >= 0 && top >= 0 && left < width_ - 1 && top < height_ - 1) {
            const auto* cell = mask_.data() + size_t(top) * width_ + left;
            const unsigned a = cell[0] * (256 - fx) + cell[1] * fx;
            const unsigned b = cell[width_] * (256 - fx) + cell[width_ + 1] * fx;
            return (a * (256 - fy) + b * fy + 32768) / 65536;
        }
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
    template<class Work>
    void ForBands(int length, const Work& work) const {
        constexpr int bands = 4;
        if (size_t(width_) * height_ < 512 * 512 || length < bands) {
            work(0, length);
            return;
        }
        concurrency::parallel_for(0, bands, [&](int band) {
            work(length * band / bands, length * (band + 1) / bands);
        });
    }

    unsigned At(int x, int y) const {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) return outside_;
        return mask_[size_t(y) * width_ + x];
    }

    template<bool Horizontal>
    void SpreadSmall(const std::vector<uint8_t>& input, std::vector<uint8_t>& output, int radius) const {
        const int stride = Horizontal ? 1 : width_;
        ForBands(height_, [&](int begin, int end) {
            for (int y = begin; y < end; ++y) {
                const auto* row = input.data() + size_t(y) * width_;
                auto* destination = output.data() + size_t(y) * width_;
                const int left = Horizontal ? radius : 0;
                const int right = Horizontal ? width_ - radius : width_;
                const bool edgeRow = !Horizontal && (y < radius || y >= height_ - radius);
                for (int x = 0; x < width_; ++x) {
                    if (edgeRow || x < left || x >= right) {
                        unsigned maximum = outside_;
                        for (int tap = -radius; tap <= radius; ++tap) {
                            const int index = Horizontal ? x + tap : y + tap;
                            if (index >= 0 && index < (Horizontal ? width_ : height_)) {
                                maximum = (std::max)(maximum, unsigned(row[x + tap * stride]));
                            }
                        }
                        destination[x] = static_cast<uint8_t>(maximum);
                    } else {
                        auto maximum = (std::max)({row[x - stride], row[x], row[x + stride]});
                        if (radius == 2) maximum = (std::max)({maximum, row[x - 2 * stride], row[x + 2 * stride]});
                        destination[x] = maximum;
                    }
                }
            }
        });
    }

    void Spread(const std::vector<uint8_t>& input, std::vector<uint8_t>& output,
                int length, int lines, bool vertical, int radius) {
        const int stride = vertical ? width_ : 1;
        for (int line = 0; line < lines; ++line) {
            const size_t offset = vertical ? line : size_t(line) * width_;
            const auto value = [&](int index) { return input[offset + size_t(index) * stride]; };
            int head = 0, tail = 0, right = 0;
            for (int i = 0; i < length; ++i) {
                const int begin = i - radius;
                if (head < tail && queue_[head] < begin) ++head;
                while (right < length && right <= i + radius) {
                    while (tail > head && value(queue_[tail - 1]) <= value(right)) --tail;
                    queue_[tail++] = right++;
                }
                const auto maximum = value(queue_[head]);
                output[offset + size_t(i) * stride] = static_cast<uint8_t>(begin < 0 || i + radius >= length
                    ? (std::max)(int(maximum), outside_) : maximum);
            }
        }
    }

    void BlurHorizontal(int radius) {
        const unsigned divisor = 2 * radius + 1;
        // With the supported blur radii all sums fit in 16 bits. This
        // reciprocal gives the same integer quotient as division.
        const std::uint64_t reciprocal = ((1ULL << 32) + divisor - 1) / divisor;
        ForBands(height_, [&](int begin, int end) {
            for (int y = begin; y < end; ++y) {
                const auto offset = size_t(y) * width_;
                const auto* row = mask_.data() + offset;
                auto* output = scratch_.data() + offset;
                const int initial = (std::min)(radius, width_);
                int sum = outside_ * (divisor - initial);
                for (int x = 0; x < initial; ++x) sum += row[x];
                for (int x = 0; x < width_; ++x) {
                    const int right = x + radius;
                    const int left = x - radius - 1;
                    sum += (right < width_ ? row[right] : outside_) - (left >= 0 ? row[left] : outside_);
                    output[x] = static_cast<uint8_t>((sum + radius) * reciprocal >> 32);
                }
            }
        });
    }

    void BlurVertical(int radius) {
        const unsigned divisor = 2 * radius + 1;
        const std::uint64_t reciprocal = ((1ULL << 32) + divisor - 1) / divisor;
        const int initial = (std::min)(radius, height_);
        sums_.assign(width_, outside_ * (divisor - initial));
        edge_.assign(width_, static_cast<uint8_t>(outside_));
        ForBands(width_, [&](int begin, int end) {
            for (int y = 0; y < initial; ++y) {
                const auto* row = scratch_.data() + size_t(y) * width_;
                for (int x = begin; x < end; ++x) sums_[x] += row[x];
            }
            for (int y = 0; y < height_; ++y) {
                const int bottom = y + radius;
                const int top = y - radius - 1;
                const auto* add = bottom < height_ ? scratch_.data() + size_t(bottom) * width_ : edge_.data();
                const auto* subtract = top >= 0 ? scratch_.data() + size_t(top) * width_ : edge_.data();
                auto* output = mask_.data() + size_t(y) * width_;
                for (int x = begin; x < end; ++x) {
                    auto& sum = sums_[x];
                    sum += int(add[x]) - subtract[x];
                    output[x] = static_cast<uint8_t>((sum + radius) * reciprocal >> 32);
                }
            }
        });
    }

    int outside_ = 0;
    int width_ = 0, height_ = 0;
    std::vector<uint8_t> mask_, scratch_, edge_;
    std::vector<float> history_;
    std::vector<int> queue_, sums_;
};

}  // namespace vtsfloat
