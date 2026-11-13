#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace vtsfloat {

struct ImageTransform {
    double degrees = 0;
    bool flipX = false;
    bool flipY = false;

    bool active() const { return degrees || flipX || flipY; }
    bool operator==(const ImageTransform& other) const {
        return degrees == other.degrees && flipX == other.flipX && flipY == other.flipY;
    }
};

struct ImageMapping {
    float xx = 1, xy = 0, yx = 0, yy = 1;
    int width = 0, height = 0;
    bool operator==(const ImageMapping& other) const {
        return xx == other.xx && xy == other.xy && yx == other.yx && yy == other.yy &&
            width == other.width && height == other.height;
    }
};

inline std::array<double, 2> ImageOutputPoint(const ImageMapping& map, double u, double v) {
    const double determinant = map.xx * map.yy - map.xy * map.yx;
    return {0.5 + (map.yy * (u - 0.5) - map.xy * (v - 0.5)) / determinant,
        0.5 + (-map.yx * (u - 0.5) + map.xx * (v - 0.5)) / determinant};
}

inline ImageMapping MakeImageMapping(int width, int height, ImageTransform transform) {
    if (width <= 0 || height <= 0) return {};
    const double radians = transform.degrees * 3.14159265358979323846 / 180.0;
    const auto snap = [](double value) { return std::abs(value) < 1e-9 ? 0.0 : value; };
    const double c = snap(std::cos(radians)), s = snap(std::sin(radians));
    const double boxWidth = std::abs(c) * width + std::abs(s) * height;
    const double boxHeight = std::abs(s) * width + std::abs(c) * height;
    const double fx = transform.flipX ? -1.0 : 1.0;
    const double fy = transform.flipY ? -1.0 : 1.0;
    return {static_cast<float>(fx * c * boxWidth / width),
        static_cast<float>(fx * s * boxHeight / width),
        static_cast<float>(-fy * s * boxWidth / height),
        static_cast<float>(fy * c * boxHeight / height),
        static_cast<int>(std::ceil(boxWidth - 1e-9)), static_cast<int>(std::ceil(boxHeight - 1e-9))};
}

enum class ImageBlend {
    Normal = 0, Darken = 1, Multiply = 2, Screen = 4, Dodge = 5, Burn = 6, Add = 7,
    LinearBurn = 15, DarkerColor = 16, HardMix = 21, Hue = 23, Saturation = 24,
    Color = 25, Luminosity = 26, Separator = -1
};

using ImageBlendTable = std::array<std::array<uint8_t, 256>, 256>;

inline bool UsesImageBackdrop(ImageBlend mode) {
    return mode != ImageBlend::Normal;
}

inline bool UsesRgbBlend(ImageBlend mode) {
    return mode == ImageBlend::DarkerColor || (mode >= ImageBlend::Hue && mode <= ImageBlend::Luminosity);
}

inline float BlendComponent(ImageBlend mode, float background, float source) {
    const auto dodge = [](float base, float blend) {
        return base == 0 ? 0 : blend == 1 ? 1 : (std::min)(1.0f, base / (1 - blend));
    };
    const auto burn = [](float base, float blend) {
        return base == 1 ? 1 : blend == 0 ? 0 : 1 - (std::min)(1.0f, (1 - base) / blend);
    };
    switch (mode) {
    case ImageBlend::Darken: return (std::min)(background, source);
    case ImageBlend::Multiply: return background * source;
    case ImageBlend::Screen: return background + source - background * source;
    case ImageBlend::Dodge: return dodge(background, source);
    case ImageBlend::Burn: return burn(background, source);
    case ImageBlend::LinearBurn: return (std::max)(0.0f, background + source - 1);
    case ImageBlend::HardMix: return background + source < 1 ? 0.0f : 1.0f;
    case ImageBlend::Add: return (std::min)(1.0f, background + source);
    default: return source;
    }
}

using Rgb = std::array<float, 3>;

inline float ImageLuminosity(const Rgb& color) {
    return .3f * color[0] + .59f * color[1] + .11f * color[2];
}

inline Rgb SetImageLuminosity(Rgb color, float target) {
    const float delta = target - ImageLuminosity(color);
    for (float& value : color) value += delta;
    const float minimum = (std::min)({color[0], color[1], color[2]});
    const float maximum = (std::max)({color[0], color[1], color[2]});
    if (minimum < 0) {
        for (float& value : color) value = target + (value - target) * target / (target - minimum);
    }
    if (maximum > 1) {
        for (float& value : color) value = target + (value - target) * (1 - target) / (maximum - target);
    }
    return color;
}

inline float ImageSaturation(const Rgb& color) {
    return (std::max)({color[0], color[1], color[2]}) - (std::min)({color[0], color[1], color[2]});
}

inline Rgb SetImageSaturation(Rgb color, float target) {
    const float minimum = (std::min)({color[0], color[1], color[2]});
    const float spread = ImageSaturation(color);
    if (spread == 0) return {};
    for (float& value : color) value = (value - minimum) * target / spread;
    return color;
}

inline Rgb BlendRgb(ImageBlend mode, const Rgb& base, const Rgb& source) {
    switch (mode) {
    case ImageBlend::DarkerColor: {
        const bool darker = source[0] + source[1] + source[2] < base[0] + base[1] + base[2];
        return darker ? source : base;
    }
    case ImageBlend::Hue:
        return SetImageLuminosity(SetImageSaturation(source, ImageSaturation(base)), ImageLuminosity(base));
    case ImageBlend::Saturation:
        return SetImageLuminosity(SetImageSaturation(base, ImageSaturation(source)), ImageLuminosity(base));
    case ImageBlend::Color:
        return SetImageLuminosity(source, ImageLuminosity(base));
    case ImageBlend::Luminosity:
        return SetImageLuminosity(base, ImageLuminosity(source));
    default: return source;
    }
}

inline void FillImageBlendTable(ImageBlendTable& table, ImageBlend mode) {
    for (size_t background = 0; background < table.size(); ++background) {
        for (size_t source = 0; source < table[background].size(); ++source) {
            table[background][source] = static_cast<uint8_t>(std::lround(255 *
                (std::clamp)(BlendComponent(mode, background / 255.0f, source / 255.0f), 0.0f, 1.0f)));
        }
    }
}

struct ImageEffects {
    // Color adjustments are stored in hundredths of their displayed units.
    int brightness = 0;
    int contrast = 0;
    int saturation = 0;
    int hue = 0;
    int warmth = 0;
    int sharpen = 0;
    int exposure = 0; // Ten-thousandths of an EV.
    int clarity = 0;
    int vignette = 0;
    ImageBlend blend = ImageBlend::Normal;
    int blendStrengthHundredths = 10000;
    int blendGrid = 3;

    bool HasColorGrade() const { return brightness || contrast || saturation || hue || warmth || exposure; }
    bool operator==(const ImageEffects& other) const {
        return brightness == other.brightness && contrast == other.contrast &&
            saturation == other.saturation && hue == other.hue && warmth == other.warmth &&
            sharpen == other.sharpen && blend == other.blend && blendStrengthHundredths == other.blendStrengthHundredths &&
            blendGrid == other.blendGrid && exposure == other.exposure &&
            clarity == other.clarity && vignette == other.vignette;
    }
};

using ColorGrade = std::array<std::array<float, 4>, 3>;

inline ColorGrade MakeColorGrade(const ImageEffects& effects) {
    const double angle = effects.hue * 3.14159265358979323846 / 18000;
    const double saturation = (10000 + effects.saturation) / 10000.0;
    const double cosHue = std::cos(angle) * saturation;
    const double sinHue = std::sin(angle) * saturation;
    const double contrast = (10000 + effects.contrast) / 10000.0;
    const double brightness = effects.brightness / 10000.0 + (1 - contrast) * 0.5;
    const double warmth = effects.warmth / 50000.0;
    const double exposure = std::exp2(effects.exposure / 10000.0);
    ColorGrade grade{};
    for (size_t i = 0; i < 3; ++i) {
        const double r = i == 0, g = i == 1, b = i == 2;
        const double luma = .2126 * r + .7152 * g + .0722 * b;
        const double u = b - luma, v = r - luma;
        const double outR = luma + v * cosHue + u * sinHue;
        const double outB = luma + u * cosHue - v * sinHue;
        const double outG = (luma - .2126 * outR - .0722 * outB) / .7152;
        grade[0][i] = static_cast<float>(contrast * outR);
        grade[1][i] = static_cast<float>(contrast * outG);
        grade[2][i] = static_cast<float>(contrast * outB);
    }
    grade[0][3] = static_cast<float>(brightness + warmth);
    grade[1][3] = static_cast<float>(brightness);
    grade[2][3] = static_cast<float>(brightness - warmth);
    for (auto& row : grade) {
        for (auto& coefficient : row) coefficient *= static_cast<float>(exposure);
    }
    return grade;
}

struct ImageTone {
    double exposure = 0;
    double black = 0, white = 255;
    double gamma = 1;
    // Composite, red, green, blue. Each row is sampled at five fixed x positions.
    std::array<std::array<int, 5>, 4> curves{{
        {{0, 64, 128, 191, 255}}, {{0, 64, 128, 191, 255}},
        {{0, 64, 128, 191, 255}}, {{0, 64, 128, 191, 255}}}};

    bool operator==(const ImageTone& other) const {
        return exposure == other.exposure && black == other.black && white == other.white &&
            gamma == other.gamma && curves == other.curves;
    }
};

using ToneLut = std::array<std::array<float, 4>, 256>;

inline float SampleImageCurve(const std::array<int, 5>& knots, double input) {
    constexpr int positions[]{0, 64, 128, 191, 255};
    const double x = (std::clamp)(input, 0.0, 1.0) * 255;
    const int right = static_cast<int>(std::upper_bound(positions + 1, positions + 4, x) - positions);
    const double t = (x - positions[right - 1]) / (positions[right] - positions[right - 1]);
    return static_cast<float>((knots[right - 1] + (knots[right] - knots[right - 1]) * t) / 255);
}

inline ToneLut MakeToneLut(const ImageTone& tone) {
    ToneLut lut{};
    const double gain = std::exp2(tone.exposure);
    for (size_t i = 0; i < lut.size(); ++i) {
        const double levels = (std::clamp)((i * gain - tone.black) / (tone.white - tone.black), 0.0, 1.0);
        const auto composite = SampleImageCurve(tone.curves[0], std::pow(levels, 1 / tone.gamma));
        for (size_t channel = 0; channel < 3; ++channel) {
            lut[i][channel] = SampleImageCurve(tone.curves[channel + 1], composite);
        }
    }
    return lut;
}

}  // namespace vtsfloat
