    RECT DisplayImageRegion() const {
        return SourceViewPixels();
    }

    vtsfloat::ImageTransform DisplayImageTransform() const {
        return cropEditing_ ? imageTransformDraft_ : imageTransform_;
    }

    static vtsfloat::ImageMapping ImageMappingFor(const RECT& region, vtsfloat::ImageTransform transform) {
        return vtsfloat::MakeImageMapping(region.right - region.left, region.bottom - region.top, transform);
    }

    RECT DisplayImageBounds() const {
        const auto map = ImageMappingFor(DisplayImageRegion(), DisplayImageTransform());
        return RECT{0, 0, map.width, map.height};
    }

    static RECT TransformedViewport(const RECT& bounds, const RECT& source,
                                    vtsfloat::ImageTransform transform) {
        if (!transform.active() || IsRectEmpty(&source)) return bounds;
        const auto map = ImageMappingFor(source, transform);
        const LONG width = (std::max)(1, MulDiv(bounds.right - bounds.left, map.width, source.right - source.left));
        const LONG height = (std::max)(1, MulDiv(bounds.bottom - bounds.top, map.height, source.bottom - source.top));
        const LONG left = (bounds.left + bounds.right - width) / 2;
        const LONG top = (bounds.top + bounds.bottom - height) / 2;
        return RECT{left, top, left + width, top + height};
    }

    void RefreshImageTone() {
        const auto& tone = imagePanelHwnd_ ? imageToneDraft_ : imageTone_;
        toneEnabled_ = !(tone == vtsfloat::ImageTone{});
        if (toneEnabled_) toneSettings_.lut = vtsfloat::MakeToneLut(tone);
        const auto& effects = imagePanelHwnd_ ? imageEffectsDraft_ : imageEffects_;
        colorGradeEnabled_ = effects.HasColorGrade();
        if (colorGradeEnabled_) toneSettings_.grade = vtsfloat::MakeColorGrade(effects);
        imageSharpen_ = effects.sharpen;
        imageClarity_ = effects.clarity;
        imageVignette_ = effects.vignette;
        const auto previousBlend = std::exchange(imageBlendMode_, effects.blend);
        if (previousBlend != effects.blend &&
            vtsfloat::UsesImageBackdrop(effects.blend) && !vtsfloat::UsesRgbBlend(effects.blend)) {
            vtsfloat::FillImageBlendTable(imageBlendTable_, effects.blend);
        }
        imageBlendStrengthHundredths_ = effects.blendStrengthHundredths;
        if (std::exchange(imageBlendGrid_, effects.blendGrid) != effects.blendGrid) imageBackdropColorTime_ = {};
        if (!ScreenImageBlendActive()) StopImageBackdrop();
        toneGpuDirty_ = true;
        scaleReadbackPrimed_ = false;
        resetFrameSchedule_ = true;
    }

    void LoadImageSettings() {
        imageTransform_.degrees = (std::clamp)(ReadConfigInt(L"image", L"rotation_tenths", 0),
            -1800, 1800) / 10.0;
        imageTransform_.flipX = ReadConfigInt(L"image", L"flip_horizontal", 0) != 0;
        imageTransform_.flipY = ReadConfigInt(L"image", L"flip_vertical", 0) != 0;
        vtsfloat::ImageTone stored;
        std::wistringstream stream(ReadConfigString(L"image", L"tone"));
        stream.imbue(std::locale::classic());
        int ev{}, gamma{};
        if (stream >> ev >> stored.black >> stored.white >> gamma) {
            stored.exposure = (std::clamp)(ev, -300, 300) / 100.0;
            stored.black = (std::clamp)(stored.black, 0.0, 254.0);
            stored.white = (std::clamp)(stored.white, stored.black + 1, 255.0);
            stored.gamma = (std::clamp)(gamma, 10, 300) / 100.0;
            for (auto& curve : stored.curves) {
                for (int& knot : curve) {
                    if (int value{}; stream >> value) knot = (std::clamp)(value, 0, 255);
                }
            }
            if (int preciseEv{}; stream >> preciseEv) {
                stored.exposure = (std::clamp)(preciseEv, -30000, 30000) / 10000.0;
            }
            imageTone_ = stored;
        }
        std::wistringstream effects(ReadConfigString(L"image", L"effects"));
        effects.imbue(std::locale::classic());
        double brightness{}, contrast{}, saturation{}, hue{}, warmth{}, sharpen{};
        int mode{};
        if (effects >> brightness >> contrast >> saturation >> hue >> warmth >> sharpen >> mode) {
            const auto toHundredths = [](double value, double minimum, double maximum) {
                return std::isfinite(value) ? static_cast<int>(std::lround((std::clamp)(value, minimum, maximum) * 100)) : 0;
            };
            imageEffects_.brightness = toHundredths(brightness, -15, 15);
            imageEffects_.contrast = toHundredths(contrast, -15, 15);
            imageEffects_.saturation = toHundredths(saturation, -100, 100);
            imageEffects_.hue = toHundredths(hue, -180, 180);
            imageEffects_.warmth = toHundredths(warmth, -100, 100);
            imageEffects_.sharpen = toHundredths(sharpen, 0, 200);
            double strength = 100.0;
            if (effects >> strength && std::isfinite(strength)) {
                imageEffects_.blendStrengthHundredths = static_cast<int>(std::lround((std::clamp)(strength, 0.0, 100.0) * 100));
            } else {
                imageEffects_.blendStrengthHundredths = 10000;
            }
            if (!(effects >> imageEffects_.blendGrid)) imageEffects_.blendGrid = 3;
            double exposure{}, clarity{}, vignette{};
            if (effects >> exposure >> clarity >> vignette) {
                imageEffects_.exposure = std::isfinite(exposure) ? static_cast<int>(std::lround((std::clamp)(exposure, -3.0, 3.0) * 10000)) : 0;
                imageEffects_.clarity = toHundredths(clarity, -100, 100);
                imageEffects_.vignette = toHundredths(vignette, -100, 100);
            }
            for (const auto& control : ImageEffectControls()) {
                int& value = imageEffects_.*control.value;
                value = (std::clamp)(value, control.minimum, control.maximum);
            }
            const auto storedBlend = static_cast<vtsfloat::ImageBlend>(mode);
            imageEffects_.blend = ImageBlendName(storedBlend) ? storedBlend : vtsfloat::ImageBlend::Normal;
        } else {
            imageEffects_ = {};
        }
        RefreshImageTone();
    }

    void SaveImageTone() const {
        std::wostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::lround(imageTone_.exposure * 100) << std::fixed << std::setprecision(2) << L' ' << imageTone_.black
            << L' ' << imageTone_.white << L' ' << std::lround(imageTone_.gamma * 100);
        for (const auto& curve : imageTone_.curves) {
            for (const int knot : curve) stream << L' ' << knot;
        }
        stream << L' ' << std::lround(imageTone_.exposure * 10000);
        WriteConfigString(L"image", L"tone", stream.str());
        const auto& effects = imageEffects_;
        std::wostringstream settings;
        settings.imbue(std::locale::classic());
        settings << std::fixed << std::setprecision(2) << effects.brightness / 100.0 << L' ' << effects.contrast / 100.0 << L' ' <<
            effects.saturation / 100.0 << L' ' << effects.hue / 100.0 << L' ' << effects.warmth / 100.0 << L' ' <<
            effects.sharpen / 100.0 << L' ' << static_cast<int>(effects.blend) << L' ' <<
            effects.blendStrengthHundredths / 100.0 << L' ' << effects.blendGrid << L' ' <<
            std::setprecision(4) << effects.exposure / 10000.0 << std::setprecision(2) << L' ' << effects.clarity / 100.0 << L' ' << effects.vignette / 100.0;
        WriteConfigString(L"image", L"effects", settings.str());
    }

    void RefreshCropTransform() {
        if (cropEditing_) {
            FitCropViewport(TransformedViewport(imagePreviewBounds_, DisplayImageRegion(), imageTransformDraft_),
                DisplayImageBounds());
        } else {
            if (imageTransform_ == imageTransformDraft_) return;
            RECT bounds{};
            if (!GetWindowRect(hwnd_, &bounds)) return;
            const RECT previous = DisplayImageBounds();
            imageTransform_ = imageTransformDraft_;
            const RECT next = DisplayImageBounds();
            bounds.right = bounds.left + MulDiv(bounds.right - bounds.left, next.right, (std::max)(1L, previous.right));
            bounds.bottom = bounds.top + MulDiv(bounds.bottom - bounds.top, next.bottom, (std::max)(1L, previous.bottom));
            FitCropViewport(bounds, next);
        }
        scaleReadbackPrimed_ = false;
        resetFrameSchedule_ = true;
    }

    std::array<POINT, 4> CropScreenCorners(const RECT& selection, const vtsfloat::ImageMapping& mapping,
                                           int width, int height) const {
        const POINT corners[]{{selection.left, selection.top}, {selection.right, selection.top},
            {selection.right, selection.bottom}, {selection.left, selection.bottom}};
        std::array<POINT, 4> result{};
        for (size_t i = 0; i < result.size(); ++i) {
            const auto point = vtsfloat::ImageOutputPoint(mapping,
                double(corners[i].x) / sourceWidth_, double(corners[i].y) / sourceHeight_);
            result[i] = {std::lround(point[0] * width), std::lround(point[1] * height)};
        }
        return result;
    }

    void ScaleTransformedCpu(const D3D11_MAPPED_SUBRESOURCE& mapped, const RECT& region,
                             int width, int height, const vtsfloat::ImageMapping& map,
                             const FeatherRadii& feather) {
        const int sourceWidth = region.right - region.left, sourceHeight = region.bottom - region.top;
        const bool bgra = sourceFormat_ == DXGI_FORMAT_B8G8R8A8_UNORM ||
            sourceFormat_ == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        const bool featherEnabled = feather != FeatherRadii{};
        const auto* base = static_cast<const std::uint8_t*>(mapped.pData) +
            static_cast<size_t>(region.top) * mapped.RowPitch + region.left * 4;
        auto* pixel = static_cast<std::uint8_t*>(dibBits_);
        for (int y = 0; y < height; ++y) {
            const float v = (y + 0.5f) / height - 0.5f;
            for (int x = 0; x < width; ++x, pixel += 4) {
                const float u = (x + 0.5f) / width - 0.5f;
                const float sx = map.xx * u + map.xy * v + 0.5f;
                const float sy = map.yx * u + map.yy * v + 0.5f;
                if (sx < 0 || sx > 1 || sy < 0 || sy > 1) {
                    std::fill_n(pixel, 4, 0);
                    continue;
                }
                const float fx = (std::clamp)(sx * sourceWidth - 0.5f, 0.0f, float(sourceWidth - 1));
                const float fy = (std::clamp)(sy * sourceHeight - 0.5f, 0.0f, float(sourceHeight - 1));
                const int x0 = static_cast<int>(fx), y0 = static_cast<int>(fy);
                const int x1 = (std::min)(x0 + 1, sourceWidth - 1), y1 = (std::min)(y0 + 1, sourceHeight - 1);
                const float tx = fx - x0, ty = fy - y0;
                const float weights[]{(1 - tx) * (1 - ty), tx * (1 - ty), (1 - tx) * ty, tx * ty};
                const std::uint8_t* samples[]{base + y0 * size_t(mapped.RowPitch) + x0 * 4,
                    base + y0 * size_t(mapped.RowPitch) + x1 * 4,
                    base + y1 * size_t(mapped.RowPitch) + x0 * 4,
                    base + y1 * size_t(mapped.RowPitch) + x1 * 4};
                float fade = 1;
                if (featherEnabled) {
                    const float distances[]{sx * sourceWidth, sy * sourceHeight,
                        (1 - sx) * sourceWidth, (1 - sy) * sourceHeight};
                    for (size_t i = 0; i < feather.size(); ++i) {
                        if (feather[i]) {
                            const float t = (std::min)(1.0f, distances[i] / feather[i]);
                            fade = (std::min)(fade, t * t * (3 - 2 * t));
                        }
                    }
                }
                float out[4]{};
                for (size_t i = 0; i < 4; ++i) {
                    const auto* sample = samples[i];
                    const float alpha = sample[3] * weights[i] * fade;
                    out[0] += sample[bgra ? 0 : 2] * alpha / 255;
                    out[1] += sample[1] * alpha / 255;
                    out[2] += sample[bgra ? 2 : 0] * alpha / 255;
                    out[3] += alpha;
                }
                for (size_t i = 0; i < 4; ++i) pixel[i] = static_cast<std::uint8_t>(
                    (std::clamp)(std::lround(out[i]), 0L, 255L));
            }
        }
    }

    void ApplyImageToneCpu(int width, int height) {
        auto* pixel = static_cast<std::uint8_t*>(dibBits_);
        for (size_t i = 0, count = size_t(width) * height; i < count; ++i, pixel += 4) {
            const unsigned alpha = pixel[3];
            if (!alpha) continue;
            if (toneEnabled_) {
                for (size_t c = 0; c < 3; ++c) {
                    const unsigned index = (std::min)(255u, (pixel[c] * 255u + alpha / 2) / alpha);
                    pixel[c] = static_cast<std::uint8_t>(std::lround(toneSettings_.lut[index][2 - c] * alpha));
                }
            }
            if (colorGradeEnabled_) {
                const float r = pixel[2], g = pixel[1], b = pixel[0];
                for (size_t c = 0; c < 3; ++c) {
                    const auto& row = toneSettings_.grade[c];
                    pixel[2 - c] = static_cast<uint8_t>((std::clamp)(std::lround(
                        row[0] * r + row[1] * g + row[2] * b + row[3] * alpha), 0L, long(alpha)));
                }
            }
        }
    }

    void ApplyImageDetail(int width, int height) {
        if ((!imageSharpen_ && !imageClarity_) || width < 3 || height < 3) return;
        auto* pixels = static_cast<uint8_t*>(dibBits_);
        const size_t stride = size_t(width) * 4;
        imageSharpenBuffer_.assign(pixels, pixels + stride * height);
        const auto luma = [](const uint8_t* pixel) { return (54 * pixel[2] + 183 * pixel[1] + 19 * pixel[0]) >> 8; };
        const int radius = (std::clamp)((std::min)(width, height) / 50, 3, 24);
        for (int y = 1; y + 1 < height; ++y) {
            for (int x = 1; x + 1 < width; ++x) {
                const size_t i = size_t(y) * stride + x * 4;
                auto* result = pixels + i;
                const auto* center = imageSharpenBuffer_.data() + i;
                if (!center[3]) continue;
                const uint8_t* neighbors[]{center - 4, center + 4, center - stride, center + stride};
                int sumLuma = 0, sumAlpha = 0;
                for (const auto* neighbor : neighbors) {
                    sumLuma += luma(neighbor);
                    sumAlpha += neighbor[3];
                }
                if (!sumAlpha) continue;
                const int detail = luma(center) - (sumLuma * center[3] + sumAlpha / 2) / sumAlpha;
                int adjustment = detail * imageSharpen_ / 10000;
                if (imageClarity_) {
                    const uint8_t* distant[]{
                        center - 4 * (std::min)(x, radius),
                        center + 4 * (std::min)(width - 1 - x, radius),
                        center - stride * (std::min)(y, radius),
                        center + stride * (std::min)(height - 1 - y, radius)};
                    int localLuma = 0, localAlpha = 0;
                    for (const auto* neighbor : distant) {
                        localLuma += luma(neighbor);
                        localAlpha += neighbor[3];
                    }
                    if (localAlpha) {
                        const int midtone = 255 - std::abs(2 * luma(center) * 255 / center[3] - 255);
                        const int localDetail = luma(center) - localLuma * center[3] / localAlpha;
                        adjustment += localDetail * imageClarity_ * midtone / 2550000;
                    }
                }
                for (int c = 0; c < 3; ++c) {
                    result[c] = static_cast<uint8_t>((std::clamp)(center[c] + adjustment, 0, int(center[3])));
                }
            }
        }
    }

    void ApplyImageVignette(int width, int height) {
        if (!imageVignette_) return;
        const float strength = std::abs(imageVignette_) / 10000.0f;
        const bool lighten = imageVignette_ < 0;
        auto* pixel = static_cast<uint8_t*>(dibBits_);
        for (int y = 0; y < height; ++y) {
            const float v = (2.0f * y + 1) / height - 1;
            for (int x = 0; x < width; ++x, pixel += 4) {
                if (!pixel[3]) continue;
                const float u = (2.0f * x + 1) / width - 1;
                const float edge = (std::clamp)((u * u + v * v - .2f) / 1.3f, 0.0f, 1.0f);
                const float amount = strength * edge * edge * (3 - 2 * edge);
                const int target = lighten ? pixel[3] : 0;
                for (int c = 0; c < 3; ++c) {
                    pixel[c] = static_cast<uint8_t>(pixel[c] + (target - pixel[c]) * amount + .5f);
                }
            }
        }
    }

    void StopImageBackdrop() {
        const std::lock_guard lock(imageBackdropMutex_);
        imageBackdrop_.Stop();
        imageBackdropColorTime_ = {};
        nextImageBackdrop_ = {};
        imageBackdropUnavailable_ = false;
    }

    bool ScreenImageBlendActive() const {
        return !cropEditing_ && ((imageBlendStrengthHundredths_ && vtsfloat::UsesImageBackdrop(imageBlendMode_)) ||
            ShadowUsesBackdrop());
    }

    void UpdateImageBackdropColor(const RECT& output, size_t width, const uint8_t* model) {
        constexpr UINT size = vtsfloat::kBackdropSampleSize;
        const int cells = imageBlendGrid_, stride = cells + 1;
        const size_t height = output.bottom - output.top;
        struct Corner { std::array<double, 3> sum{}; double weight = 0; };
        std::array<Corner, 100> corners{};
        Corner overall;
        const size_t inset = locked_ && !debugMode_ ? 0 : borderThickness_ + 2;
        imageBackdrop_.ForEachRegion([&](const RECT& bounds, const vtsfloat::BackdropSamples& grid) {
            const UINT w = bounds.right - bounds.left, h = bounds.bottom - bounds.top;
            const double weight = double(w) * h;
            for (UINT y = 0; y < size; ++y) {
                const size_t row = size_t(bounds.top - output.top) + (2 * y + 1) * h / (2 * size);
                for (UINT x = 0; x < size; ++x) {
                    const size_t col = size_t(bounds.left - output.left) + (2 * x + 1) * w / (2 * size);
                    const double gx = (col + .5) * cells / width, gy = (row + .5) * cells / height;
                    const int left = static_cast<int>(gx), top = static_cast<int>(gy);
                    const double fx = gx - left, fy = gy - top;
                    // Only use transparent cells so the rendered model does not feed back.
                    const size_t x0 = size_t(bounds.left - output.left) + x * w / size;
                    const size_t x1 = size_t(bounds.left - output.left) + ((x + 1) * w + size - 1) / size;
                    const size_t y0 = size_t(bounds.top - output.top) + y * h / size;
                    const size_t y1 = size_t(bounds.top - output.top) + ((y + 1) * h + size - 1) / size;
                    if (x0 < inset || y0 < inset || x1 + inset > width || y1 + inset > height) continue;
                    bool transparent = true;
                    for (size_t sy = y0; sy < y1 && transparent; ++sy) {
                        for (size_t sx = x0; sx < x1; ++sx) {
                            if (model[(sy * width + sx) * 4 + 3]) {
                                transparent = false;
                                break;
                            }
                        }
                    }
                    if (!transparent || ShadowAffectsBackdropCell(x0, y0, x1, y1)) continue;
                    const auto* color = grid.data() + (y * size + x) * 4;
                    for (int c = 0; c < 3; ++c) overall.sum[c] += color[c] * weight;
                    overall.weight += weight;
                    for (int dy = 0; dy < 2; ++dy) {
                        for (int dx = 0; dx < 2; ++dx) {
                            auto& corner = corners[(top + dy) * stride + left + dx];
                            const double contribution = weight * (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy);
                            for (int c = 0; c < 3; ++c) corner.sum[c] += color[c] * contribution;
                            corner.weight += contribution;
                        }
                    }
                }
            }
        });
        if (!overall.weight) {
            imageBackdropColorTime_ = {};
            return;
        }
        const float localMix = cells == 1 ? 0.0f : 0.5f;
        vtsfloat::Rgb average{};
        for (int c = 0; c < 3; ++c) average[c] = static_cast<float>(overall.sum[c] / overall.weight);
        const auto now = Clock::now();
        const float mix = imageBackdropColorTime_ == Clock::time_point{} ? 1.0f :
            1.0f - std::exp(-std::chrono::duration<float>(now - imageBackdropColorTime_).count() / .09f);
        for (int i = 0; i < stride * stride; ++i) {
            const auto& corner = corners[i];
            for (int c = 0; c < 3; ++c) {
                auto& channel = imageBackdropColors_[i][c];
                const float local = corner.weight ? static_cast<float>(corner.sum[c] / corner.weight) : average[c];
                const float target = average[c] + (local - average[c]) * localMix;
                channel += (target - channel) * mix;
            }
        }
        imageBackdropColorTime_ = now;
    }

    vtsfloat::Rgb ImageBackdropColorAt(int x, int y, int width, int height) const {
        const int cells = imageBlendGrid_, stride = cells + 1;
        const float gx = (x + .5f) * cells / width, gy = (y + .5f) * cells / height;
        const int left = (std::min)(cells - 1, int(gx)), top = (std::min)(cells - 1, int(gy));
        const auto* corner = imageBackdropColors_.data() + top * stride + left;
        vtsfloat::Rgb color{};
        for (int c = 0; c < 3; ++c) {
            const float a = corner[0][c] + (corner[1][c] - corner[0][c]) * (gx - left);
            const float b = corner[stride][c] + (corner[stride + 1][c] - corner[stride][c]) * (gx - left);
            color[c] = (std::clamp)(a + (b - a) * (gy - top), 0.0f, 255.0f);
        }
        return color;
    }

    void ApplyImageBlend(const RECT& output, int width) {
        const auto mode = imageBlendMode_;
        const unsigned strength = imageBlendStrengthHundredths_;
        if (!ScreenImageBlendActive()) return;
        auto* pixels = static_cast<uint8_t*>(dibBits_);
        const std::lock_guard lock(imageBackdropMutex_);
        const bool refreshBackdrop = GetCurrentThreadId() == uiThreadId_ && Clock::now() >= nextImageBackdrop_;
        if (refreshBackdrop) {
            const bool ready = imageBackdrop_.Update(output, device_.Get(), context_.Get());
            nextImageBackdrop_ = Clock::now() + std::chrono::milliseconds(ready ? 16 : 2000);
            if (!ready) imageBackdropColorTime_ = {};
            if (std::exchange(imageBackdropUnavailable_, !ready) != !ready && imagePanelHwnd_) {
                InvalidateRect(imagePanelHwnd_, nullptr, FALSE);
            }
        }
        if (imageBackdrop_.Matches(output) && (GetCurrentThreadId() != uiThreadId_ || refreshBackdrop)) {
            UpdateImageBackdropColor(output, width, pixels);
        }
        if (!strength || mode == vtsfloat::ImageBlend::Normal) return;
        if (imageBackdropColorTime_ == Clock::time_point{}) return;
        const bool rgbBlend = vtsfloat::UsesRgbBlend(mode);
        const int cells = imageBlendGrid_, stride = cells + 1;
        const float stepX = float(cells) / width, stepY = float(cells) / (output.bottom - output.top);
        const auto unpremultiply = [](unsigned channel, unsigned alpha) {
            return alpha == 255 ? channel : (std::min)(255u, (channel * 255u + alpha / 2) / alpha);
        };
        std::array<unsigned, 3> uniform{};
        if (cells == 1) {
            for (size_t c = 0; c < uniform.size(); ++c) {
                uniform[c] = static_cast<unsigned>((std::clamp)(imageBackdropColors_[0][c] + .5f, 0.0f, 255.0f));
            }
        }
        const int height = output.bottom - output.top;
        for (int row = 0; row < height; ++row) {
            const float gy = (row + .5f) * stepY;
            const int y = (std::min)(cells - 1, int(gy));
            const float fy = gy - y;
            const auto* corners = imageBackdropColors_.data() + y * stride;
            for (int col = 0; col < width; ++col, pixels += 4) {
                const unsigned alpha = pixels[3];
                if (!alpha) continue;
                auto backdrop = uniform;
                if (cells > 1) {
                    const float gx = (col + .5f) * stepX;
                    const int x = (std::min)(cells - 1, int(gx));
                    const float fx = gx - x;
                    const auto* corner = corners + x;
                    for (int c = 0; c < 3; ++c) {
                        const float top = corner[0][c] + (corner[1][c] - corner[0][c]) * fx;
                        const float bottom = corner[stride][c] + (corner[stride + 1][c] - corner[stride][c]) * fx;
                        backdrop[c] = static_cast<unsigned>((std::clamp)(top + (bottom - top) * fy + .5f, 0.0f, 255.0f));
                    }
                }
                const std::array<unsigned, 3> source{
                    unpremultiply(pixels[0], alpha), unpremultiply(pixels[1], alpha), unpremultiply(pixels[2], alpha)};
                vtsfloat::Rgb rgb{};
                if (rgbBlend) {
                    const vtsfloat::Rgb base{backdrop[2] / 255.0f, backdrop[1] / 255.0f, backdrop[0] / 255.0f};
                    rgb = vtsfloat::BlendRgb(mode, base, {source[2] / 255.0f, source[1] / 255.0f, source[0] / 255.0f});
                }
                for (int c = 0; c < 3; ++c) {
                    const unsigned blended = rgbBlend
                        ? static_cast<unsigned>(std::lround(255 * (std::clamp)(rgb[2 - c], 0.0f, 1.0f)))
                        : imageBlendTable_[backdrop[c]][source[c]];
                    const unsigned mixed = (blended * alpha + 127) / 255;
                    pixels[c] = static_cast<uint8_t>((mixed * strength + pixels[c] * (10000 - strength) + 5000) / 10000);
                }
            }
        }
    }

    struct ImageEffectControl {
        const wchar_t* name;
        int vtsfloat::ImageEffects::*value;
        int minimum;
        int maximum;
        int scale = 1;
    };

    static std::array<ImageEffectControl, 11> ImageEffectControls() {
        using E = vtsfloat::ImageEffects;
        return {{{Tr(L"亮度"), &E::brightness, -1500, 1500, 100}, {Tr(L"对比度"), &E::contrast, -1500, 1500, 100},
            {Tr(L"饱和度"), &E::saturation, -10000, 10000, 100}, {Tr(L"色相"), &E::hue, -18000, 18000, 100},
            {Tr(L"色温"), &E::warmth, -10000, 10000, 100}, {Tr(L"锐化"), &E::sharpen, 0, 20000, 100},
            {Tr(L"曝光"), &E::exposure, -30000, 30000, 10000}, {Tr(L"清晰度"), &E::clarity, -10000, 10000, 100},
            {Tr(L"暗角"), &E::vignette, -10000, 10000, 100}, {Tr(L"混合强度"), &E::blendStrengthHundredths, 0, 10000, 100}, {Tr(L"渐变网格"), &E::blendGrid, 1, 9}}};
    }

    struct ImageBlendOption {
        vtsfloat::ImageBlend mode;
        const wchar_t* label;
    };

    static std::array<ImageBlendOption, 18> ImageBlendOptions() {
        using B = vtsfloat::ImageBlend;
        return {{{B::Normal, Tr(L"正常")}, {B::Separator, nullptr},
            {B::Darken, Tr(L"变暗")}, {B::Multiply, Tr(L"正片叠底")}, {B::Burn, Tr(L"颜色加深")},
            {B::LinearBurn, Tr(L"线性加深")}, {B::DarkerColor, Tr(L"深色")}, {B::Separator, nullptr},
            {B::Screen, Tr(L"滤色")}, {B::Dodge, Tr(L"颜色减淡")}, {B::Add, Tr(L"线性减淡（添加）")},
            {B::Separator, nullptr}, {B::HardMix, Tr(L"实色混合")}, {B::Separator, nullptr},
            {B::Hue, Tr(L"色相")}, {B::Saturation, Tr(L"饱和度")}, {B::Color, Tr(L"颜色")}, {B::Luminosity, Tr(L"明度")}}};
    }

    static const wchar_t* ImageBlendName(vtsfloat::ImageBlend mode) {
        for (const auto& option : ImageBlendOptions()) {
            if (option.mode == mode) return option.label;
        }
        return nullptr;
    }

    void SelectImageBlend(const RECT& anchor) {
        const HMENU menu = CreatePopupMenu();
        if (!menu) return;
        for (const auto& [mode, label] : ImageBlendOptions()) {
            if (!label) AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            else AppendMenuW(menu, MF_STRING | (imageEffectsDraft_.blend == mode ? MF_CHECKED : 0),
                static_cast<UINT>(mode) + 1, label);
        }
        POINT position{anchor.left, anchor.bottom};
        ClientToScreen(imagePanelHwnd_, &position);
        const UINT choice = RunModalWhileRendering([&] {
            return TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, position.x, position.y, 0, imagePanelHwnd_, nullptr);
        });
        DestroyMenu(menu);
        if (choice && imagePanelHwnd_) imageEffectsDraft_.blend = static_cast<vtsfloat::ImageBlend>(choice - 1);
    }

    enum ImagePanelAction {
        ImageClose = 1, ImageReset, ImageCancel, ImageSave,
        ImageCurveAll, ImageCurveRed, ImageCurveGreen, ImageCurveBlue,
        ImageBlendPicker
    };

    int ImagePanelPx(int logical) const { return static_cast<int>(std::lround(logical * imagePanelScale_)); }

    RECT ImagePanelRect(int l, int t, int r, int b) const {
        return RECT{ImagePanelPx(l), ImagePanelPx(t), ImagePanelPx(r), ImagePanelPx(b)};
    }

    static int ImageEffectRowY(size_t index) {
        return index >= 9 ? 395 + static_cast<int>(index - 9) * 32 : 52 + static_cast<int>(index) * 34;
    }

    RECT ImageEffectTrack(size_t index) const {
        const int y = ImageEffectRowY(index);
        return ImagePanelRect(135, y + 7, 354, y + 14);
    }

    RECT ImageEffectHitArea(size_t index) const {
        RECT bounds = ImageEffectTrack(index);
        InflateRect(&bounds, ImagePanelPx(3), ImagePanelPx(8));
        return bounds;
    }

    int ImageEffectHit(POINT point) const {
        if (!imageEffectsTab_) return -1;
        for (size_t i = 0; i < imagePanelEdits_.size() - 4; ++i) {
            const RECT bounds = ImageEffectHitArea(i);
            if (PtInRect(&bounds, point)) return static_cast<int>(i);
        }
        return -1;
    }

    static double ImageSliderValue(double position, double minimum, double maximum, double detail) {
        if (minimum == -detail && maximum == detail) return minimum + position * (maximum - minimum);
        if (minimum < 0) {
            if (position < .25) return minimum + (-detail - minimum) * position * 4;
            if (position < .75) return (position - .5) * detail * 4;
            return detail + (maximum - detail) * (position - .75) * 4;
        }
        return position <= .5 ? minimum + (detail - minimum) * position * 2 :
            detail + (maximum - detail) * (position - .5) * 2;
    }

    static double ImageSliderPosition(double value, double minimum, double maximum, double detail) {
        if (minimum == -detail && maximum == detail) return (value - minimum) / (maximum - minimum);
        if (minimum < 0) {
            if (value < -detail) return .25 * (value - minimum) / (-detail - minimum);
            if (value <= detail) return .5 + .25 * value / detail;
            return .75 + .25 * (value - detail) / (maximum - detail);
        }
        return value <= detail ? .5 * (value - minimum) / (detail - minimum) :
            .5 + .5 * (value - detail) / (maximum - detail);
    }

    static double ImageEffectDetailLimit(const ImageEffectControl& control) {
        return (std::min)(20.0, double(control.maximum) / control.scale);
    }

    static double RoundImageSliderValue(double value, double detail) {
        const double scale = std::abs(value) <= detail ? 100 : 10;
        return std::round(value * scale) / scale;
    }

    void UpdateImageEffect(int x) {
        const size_t index = static_cast<size_t>(imageEffectDrag_);
        const auto control = ImageEffectControls()[index];
        const RECT track = ImageEffectTrack(index);
        const double position = (std::clamp)(double(x - track.left) / (track.right - track.left), 0.0, 1.0);
        const double detail = ImageEffectDetailLimit(control);
        const int value = control.scale != 100 ? static_cast<int>(std::lround(
            control.minimum + position * (control.maximum - control.minimum))) :
            static_cast<int>(std::lround(RoundImageSliderValue(ImageSliderValue(position,
                double(control.minimum) / control.scale, double(control.maximum) / control.scale, detail),
                detail) * control.scale));
        if (std::exchange(imageEffectsDraft_.*control.value, value) == value) return;
        UpdateImageFields();
        RefreshImageTone();
        InvalidateRect(imagePanelHwnd_, nullptr, FALSE);
    }

    struct ImagePanelButton {
        int action;
        RECT bounds;
        const wchar_t* label;
        bool active = false;
    };

    std::vector<ImagePanelButton> ImagePanelButtons() const {
        const int footer = imageEffectsTab_ ? 485 : 398;
        std::vector<ImagePanelButton> buttons{
            {ImageClose, ImagePanelRect(394, 2, 438, 38), L"×"},
            {ImageReset, ImagePanelRect(14, footer, 142, footer + 32), Tr(L"重置")},
            {ImageCancel, ImagePanelRect(152, footer, 284, footer + 32), Tr(L"取消")},
            {ImageSave, ImagePanelRect(294, footer, 426, footer + 32), Tr(L"保存"), true}};
        if (imageEffectsTab_) {
            buttons.push_back({ImageBlendPicker, ImagePanelRect(135, 355, 426, 383), ImageBlendName(imageEffectsDraft_.blend)});
            return buttons;
        }
        constexpr const wchar_t* names[]{L"RGB", L"R", L"G", L"B"};
        for (int i = 0; i < 4; ++i) {
            buttons.push_back({ImageCurveAll + i, ImagePanelRect(193 + i * 59, 162, 249 + i * 59, 187),
                names[i], imageCurveChannel_ == i});
        }
        return buttons;
    }

    RECT ImageCurveBounds() const { return ImagePanelRect(18, 197, 423, 350); }

    int ImagePanelHit(POINT point) const {
        for (const auto& button : ImagePanelButtons()) {
            if (PtInRect(&button.bounds, point)) return button.action;
        }
        return 0;
    }

    void PaintImagePanel(HWND panel) const {
        PAINTSTRUCT paint{};
        const HDC target = BeginPaint(panel, &paint);
        RECT client{};
        GetClientRect(panel, &client);
        const HDC dc = CreateCompatibleDC(target);
        const HBITMAP buffer = CreateCompatibleBitmap(target, client.right, client.bottom);
        if (!dc || !buffer) {
            if (buffer) DeleteObject(buffer);
            if (dc) DeleteDC(dc);
            EndPaint(panel, &paint);
            return;
        }
        const HGDIOBJ previous = SelectObject(dc, buffer);
        PanelFill(dc, client, RGB(16, 25, 39));
        PanelFill(dc, ImagePanelRect(0, 0, 440, 40), RGB(27, 45, 68));
        constexpr UINT line = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
        const auto text = [&](const wchar_t* label, RECT rect, int size = 14, bool bold = false) {
            PanelText(dc, label, rect, RGB(223, 238, 252), ImagePanelPx(size), bold, DT_LEFT | line);
        };
        text(ImagePanelTitle(), ImagePanelRect(14, 0, 388, 40), 18, true);
        for (const auto& button : ImagePanelButtons()) {
            PanelFill(dc, button.bounds, button.action == imagePanelHover_ ? RGB(39, 86, 136)
                : button.active ? RGB(31, 85, 141) : RGB(24, 42, 64));
            PanelText(dc, button.label, button.bounds, RGB(240, 246, 255), ImagePanelPx(14), true, DT_CENTER | line);
        }
        if (imageEffectsTab_) {
            const auto controls = ImageEffectControls();
            for (size_t i = 0; i < controls.size(); ++i) {
                const int y = ImageEffectRowY(i);
                const auto& control = controls[i];
                text(control.name, ImagePanelRect(15, y - 2, 132, y + 22));
                const int value = imageEffectsDraft_.*control.value;
                const int position = control.scale == 1 ? MulDiv(value - control.minimum, 10000, control.maximum - control.minimum) :
                    static_cast<int>(std::lround(10000 * ImageSliderPosition(double(value) / control.scale,
                        double(control.minimum) / control.scale, double(control.maximum) / control.scale,
                        ImageEffectDetailLimit(control))));
                PanelSlider(dc, ImageEffectTrack(i), position, 0, 10000, imageEffectDrag_ == static_cast<int>(i));
                if (i != 10) text(i == 3 ? L"°" : i == 6 ? L"EV" : L"%", ImagePanelRect(418, y - 2, 440, y + 22), 11);
            }
            text(Tr(L"混合模式"), ImagePanelRect(15, 355, 129, 383));
            if (vtsfloat::UsesImageBackdrop(imageEffectsDraft_.blend)) {
                text(imageBackdropUnavailable_ ? Tr(L"无法捕获屏幕背景") : Tr(L"以角色下方的平均色作为混合底色"),
                    ImagePanelRect(15, 454, 422, 479), 12);
            }
        } else {
            text(Tr(L"曝光"), ImagePanelRect(15, 46, 122, 72));
            text(L"EV", ImagePanelRect(195, 46, 225, 72));
            text(Tr(L"色阶"), ImagePanelRect(15, 79, 82, 105), 14, true);
            text(Tr(L"黑场"), ImagePanelRect(77, 79, 141, 105));
            text(L"γ", ImagePanelRect(203, 79, 226, 105));
            text(Tr(L"白场"), ImagePanelRect(296, 79, 358, 105));
            text(Tr(L"曲线"), ImagePanelRect(15, 162, 190, 187), 14, true);
            PaintImageLevels(dc);
            const RECT graph = ImageCurveBounds();
            PanelFill(dc, graph, RGB(11, 19, 31));
            for (int i = 0; i <= 4; ++i) {
                const LONG x = graph.left + MulDiv(graph.right - graph.left - 1, i, 4);
                const LONG y = graph.top + MulDiv(graph.bottom - graph.top - 1, i, 4);
                PanelFill(dc, RECT{x, graph.top, x + 1, graph.bottom}, RGB(36, 57, 78));
                PanelFill(dc, RECT{graph.left, y, graph.right, y + 1}, RGB(36, 57, 78));
            }
            const HPEN diagonal = CreatePen(PS_DOT, 1, RGB(77, 105, 133));
            const HGDIOBJ previousPen = SelectObject(dc, diagonal);
            MoveToEx(dc, graph.left, graph.bottom - 1, nullptr);
            LineTo(dc, graph.right - 1, graph.top);
            SelectObject(dc, previousPen);
            DeleteObject(diagonal);
            const COLORREF colors[]{RGB(143, 203, 255), RGB(255, 116, 116), RGB(100, 216, 155), RGB(115, 157, 255)};
            const auto& knots = imageToneDraft_.curves[imageCurveChannel_];
            constexpr int positions[]{0, 64, 128, 191, 255};
            POINT points[5]{};
            for (int i = 0; i < 5; ++i) {
                points[i] = POINT{graph.left + MulDiv(graph.right - graph.left - 1, positions[i], 255),
                    graph.bottom - 1 - MulDiv(graph.bottom - graph.top - 1, knots[i], 255)};
            }
            const HPEN pen = CreatePen(PS_SOLID, ImagePanelPx(2), colors[imageCurveChannel_]);
            const auto oldPen = SelectObject(dc, pen);
            Polyline(dc, points, ARRAYSIZE(points));
            SelectObject(dc, oldPen);
            DeleteObject(pen);
            const LONG grip = ImagePanelPx(3);
            for (const POINT point : points) PanelFill(dc, RECT{point.x - grip, point.y - grip,
                point.x + grip + 1, point.y + grip + 1}, colors[imageCurveChannel_]);
        }
        BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, previous);
        DeleteObject(buffer);
        DeleteDC(dc);
        EndPaint(panel, &paint);
    }

    void UpdateImageFields() {
        imageUpdatingFields_ = true;
        const auto set = [&](size_t index, double value, int precision) {
            if (!imagePanelEdits_[index]) return;
            wchar_t text[24]{};
            swprintf_s(text, L"%.*f", precision, value);
            SetWindowTextW(imagePanelEdits_[index], text);
        };
        set(0, imageToneDraft_.exposure, 4);
        set(1, imageToneDraft_.black, imageToneDraft_.black <= 20 ? 2 : 1);
        set(2, imageToneDraft_.gamma, 2);
        set(3, imageToneDraft_.white, imageToneDraft_.white <= 20 ? 2 : 1);
        const auto controls = ImageEffectControls();
        for (size_t i = 0; i < controls.size(); ++i) {
            const auto& control = controls[i];
            const double value = double(imageEffectsDraft_.*control.value) / control.scale;
            set(4 + i, value, control.scale == 1 ? 0 : control.scale == 10000 ? 4 : std::abs(value) <= ImageEffectDetailLimit(control) ? 2 : 1);
        }
        imageUpdatingFields_ = false;
    }

    static std::optional<double> ReadDecimalField(HWND edit) {
        wchar_t input[32]{};
        GetWindowTextW(edit, input, ARRAYSIZE(input));
        if (!std::wcschr(input, L'.')) {
            if (auto* comma = std::wcschr(input, L','); comma && !std::wcschr(comma + 1, L',')) *comma = L'.';
        }
        wchar_t* end = nullptr;
        const double value = std::wcstod(input, &end);
        if (end == input || *end || !std::isfinite(value)) return std::nullopt;
        return value;
    }

    void ReadImageField(size_t index) {
        const auto input = ReadDecimalField(imagePanelEdits_[index]);
        if (!input) return;
        const double value = *input;
        if (index >= 4) {
            const auto control = ImageEffectControls()[index - 4];
            int& setting = imageEffectsDraft_.*control.value;
            const double adjusted = control.scale != 100 ? value : RoundImageSliderValue(value, ImageEffectDetailLimit(control));
            const int next = static_cast<int>(std::lround((std::clamp)(adjusted * control.scale,
                double(control.minimum), double(control.maximum))));
            if (std::exchange(setting, next) != next) {
                RefreshImageTone();
                InvalidateRect(imagePanelHwnd_, nullptr, FALSE);
            }
            return;
        }
        auto tone = imageToneDraft_;
        switch (index) {
        case 0: tone.exposure = std::round((std::clamp)(value, -3.0, 3.0) * 10000) / 10000; break;
        case 1: tone.black = (std::clamp)(RoundImageSliderValue(value, 20), 0.0, tone.white - 1); break;
        case 2: tone.gamma = std::round((std::clamp)(value, 0.10, 3.0) * 100) / 100; break;
        case 3: tone.white = (std::clamp)(RoundImageSliderValue(value, 20), tone.black + 1, 255.0); break;
        }
        if (!(tone == imageToneDraft_)) {
            imageToneDraft_ = tone;
            RefreshImageTone();
            InvalidateRect(imagePanelHwnd_, nullptr, FALSE);
        }
    }

    RECT ImageLevelsTrack() const { return ImagePanelRect(18, 114, 423, 130); }
    RECT ImageLevelsHitArea() const { return ImagePanelRect(11, 112, 430, 153); }

    std::array<int, 3> ImageLevelsHandles() const {
        const RECT track = ImageLevelsTrack();
        const double mid = std::pow(0.5, imageToneDraft_.gamma);
        const double center = imageToneDraft_.black + mid * (imageToneDraft_.white - imageToneDraft_.black);
        const auto position = [&](double value) {
            return static_cast<int>(track.left + std::lround(ImageSliderPosition(value, 0, 255, 20) * (track.right - track.left - 1)));
        };
        return {position(imageToneDraft_.black), position(center), position(imageToneDraft_.white)};
    }

    void PaintImageLevels(HDC dc) const {
        const RECT track = ImageLevelsTrack();
        const int width = track.right - track.left;
        constexpr int steps = 64;
        for (int i = 0; i < steps; ++i) {
            const int gray = static_cast<int>(ImageSliderValue(double(i) / (steps - 1), 0, 255, 20));
            PanelFill(dc, RECT{track.left + MulDiv(i, width, steps), track.top,
                track.left + MulDiv(i + 1, width, steps), track.bottom}, RGB(gray, gray, gray));
        }
        const auto handles = ImageLevelsHandles();
        const COLORREF colors[]{RGB(25, 27, 30), RGB(127, 130, 137), RGB(238, 242, 246)};
        const LONG radius = ImagePanelPx(6);
        for (size_t i = 0; i < handles.size(); ++i) {
            const LONG x = handles[i];
            const POINT triangle[]{{x, track.bottom}, {x - radius, track.bottom + ImagePanelPx(12)},
                {x + radius, track.bottom + ImagePanelPx(12)}};
            const HPEN pen = CreatePen(PS_SOLID, 1, imageLevelsDrag_ == static_cast<int>(i) ?
                RGB(137, 199, 255) : RGB(178, 190, 205));
            const HBRUSH brush = CreateSolidBrush(colors[i]);
            const HGDIOBJ oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, brush);
            Polygon(dc, triangle, ARRAYSIZE(triangle));
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(brush);
            DeleteObject(pen);
        }
    }

    void UpdateImageLevel(int x) {
        const RECT track = ImageLevelsTrack();
        const double position = (std::clamp)(double(x - track.left) / (track.right - track.left - 1), 0.0, 1.0);
        const double value = RoundImageSliderValue(ImageSliderValue(position, 0, 255, 20), 20);
        const auto previous = imageToneDraft_;
        if (imageLevelsDrag_ == 0) imageToneDraft_.black = (std::min)(value, imageToneDraft_.white - 1);
        else if (imageLevelsDrag_ == 2) imageToneDraft_.white = (std::max)(value, imageToneDraft_.black + 1);
        else {
            const double ratio = double(value - imageToneDraft_.black) / (imageToneDraft_.white - imageToneDraft_.black);
            const double gamma = std::log2((std::clamp)(ratio, 0.125, std::exp2(-0.10)));
            imageToneDraft_.gamma = std::round(-gamma * 100) / 100;
        }
        if (imageToneDraft_ == previous) return;
        UpdateImageFields();
        RefreshImageTone();
        InvalidateRect(imagePanelHwnd_, nullptr, FALSE);
    }

    void UpdateImageCurve(POINT point) {
        const RECT graph = ImageCurveBounds();
        const int value = (std::clamp)(MulDiv(graph.bottom - 1 - point.y, 255, graph.bottom - graph.top - 1), 0, 255);
        int& knot = imageToneDraft_.curves[imageCurveChannel_][imageCurveKnot_];
        if (knot == value) return;
        knot = value;
        RefreshImageTone();
        InvalidateRect(imagePanelHwnd_, &graph, FALSE);
    }

    void CloseImagePanel(bool keep) {
        const HWND panel = imagePanelHwnd_;
        if (!panel) return;
        imagePanelHwnd_ = nullptr;
        imageCurveKnot_ = imageLevelsDrag_ = imageEffectDrag_ = -1;
        imagePanelEdits_.fill(nullptr);
        if (IsWindow(panel)) DestroyWindow(panel);
        if (const HFONT font = std::exchange(imagePanelFont_, nullptr)) DeleteObject(font);
        if (keep) {
            imageTone_ = imageToneDraft_;
            imageEffects_ = imageEffectsDraft_;
            SaveImageTone();
        }
        RefreshImageTone();
    }

    void ActivateImagePanelButton(int action) {
        switch (action) {
        case ImageClose: case ImageCancel: CloseImagePanel(false); return;
        case ImageSave: CloseImagePanel(true); return;
        case ImageReset:
            if (imageEffectsTab_) imageEffectsDraft_ = {};
            else imageToneDraft_ = {};
            break;
        case ImageBlendPicker:
            SelectImageBlend(ImagePanelRect(135, 268, 426, 296));
            break;
        default: imageCurveChannel_ = action - ImageCurveAll; break;
        }
        if (!imagePanelHwnd_) return;
        UpdateImageFields();
        RefreshImageTone();
        InvalidateRect(imagePanelHwnd_, nullptr, FALSE);
    }

    HWND RegisterNumericHint(HWND edit, const RECT* bounds = nullptr) const {
        const HWND tip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, GetParent(edit), nullptr, instance_, nullptr);
        if (!tip) return nullptr;
        TOOLINFOW info{sizeof(info)};
        info.uFlags = TTF_SUBCLASS | (bounds ? 0 : TTF_IDISHWND);
        info.hwnd = bounds ? edit : GetParent(edit);
        info.uId = bounds ? static_cast<UINT_PTR>(bounds->top + 1) : reinterpret_cast<UINT_PTR>(edit);
        if (bounds) info.rect = *bounds;
        info.lpszText = const_cast<wchar_t*>(Tr(L"可左右拖拽调整数值"));
        if (!SendMessageW(tip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info))) {
            DestroyWindow(tip);
            return nullptr;
        }
        SendMessageW(tip, TTM_SETDELAYTIME, TTDT_INITIAL, 200);
        return tip;
    }

    void CommitNumericDrag(HWND edit) {
        const HWND parent = GetParent(edit);
        if (parent == imagePanelHwnd_) {
            imageTone_ = imageToneDraft_;
            imageEffects_ = imageEffectsDraft_;
            SaveImageTone();
        } else if (parent == shadowPanelHwnd_) {
            shadow_ = shadowDraft_;
            SaveShadowSettings();
        } else if (edit == cropWidthHwnd_ || edit == cropHeightHwnd_) {
            cropSelection_ = cropDraft_;
            cropNumericSaved_ = true;
            SaveCropSelection();
        } else if (parent == featherPanelHwnd_) {
            cropFeather_ = cropFeatherDraft_;
            SaveFeatherEdges();
            if (!cropEditing_) {
                WriteConfigInt(L"image", L"rotation_tenths", static_cast<int>(std::lround(imageTransform_.degrees * 10)));
                SaveWindowPlacement();
            }
        } else if (GetDlgCtrlID(edit) == IDC_FPS_EDIT) {
            SendMessageW(parent, WM_COMMAND, IDOK, 0);
        }
    }

    double NumericDragStep(HWND edit) const {
        const int id = GetDlgCtrlID(edit);
        if (GetParent(edit) == imagePanelHwnd_) {
            const size_t index = id - 4200;
            if (index == 2) return .01;
            if (index == 0) return .0001;
            if (index < 4) return std::abs(numericDragBase_) <= 20 ? .01 : .1;
            const auto control = ImageEffectControls()[index - 4];
            if (control.scale != 100) return 1.0 / control.scale;
            return std::abs(numericDragBase_) <= ImageEffectDetailLimit(control) ? .01 : .1;
        }
        if (GetParent(edit) == shadowPanelHwnd_) return 1.0 / ShadowFields()[id - kShadowFirstField].scale;
        return 1;
    }

    std::optional<LRESULT> HandleNumericDrag(HWND edit, UINT message, WPARAM wParam, LPARAM lParam) {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        switch (message) {
        case WM_LBUTTONDOWN: {
            SetFocus(edit);
            SendMessageW(edit, EM_SETSEL, 0, -1);
            ClientToScreen(edit, &point);
            numericDragEdit_ = edit;
            numericDragStart_ = point.x;
            numericDragBase_ = ReadDecimalField(edit).value_or(0);
            numericDragging_ = false;
            SetCapture(edit);
            return 0;
        }
        case WM_MOUSEMOVE:
        case WM_LBUTTONUP:
            if (numericDragEdit_ != edit) break;
            ClientToScreen(edit, &point);
            if (!numericDragging_ && std::abs(point.x - numericDragStart_) < UiScaled(3)) {
                if (message == WM_MOUSEMOVE) break;
                numericDragEdit_ = nullptr;
                if (GetCapture() == edit) ReleaseCapture();
                break;
            }
            numericDragging_ = true;
            {
                const double step = NumericDragStep(edit);
                double value = numericDragBase_ + (point.x - numericDragStart_) * step;
                if (GetDlgCtrlID(edit) == IDC_FPS_EDIT) value = (std::clamp)(value, double(kMinimumFps), double(kMaximumFps));
                wchar_t formatted[32]{};
                swprintf_s(formatted, L"%.*f", step < .01 ? 4 : step < .1 ? 2 : step < 1 ? 1 : 0, value);
                SetWindowTextW(edit, formatted);
                const HWND parent = GetParent(edit);
                if (parent == imagePanelHwnd_) ReadImageField(GetDlgCtrlID(edit) - 4200);
                if (parent == shadowPanelHwnd_) ReadShadowField(GetDlgCtrlID(edit) - kShadowFirstField);
            }
            if (message == WM_LBUTTONUP) {
                numericDragEdit_ = nullptr;
                numericDragging_ = false;
                if (GetCapture() == edit) ReleaseCapture();
                if (GetParent(edit) == imagePanelHwnd_) UpdateImageFields();
                if (GetParent(edit) == shadowPanelHwnd_) RefreshShadowPanel(true);
                CommitNumericDrag(edit);
            }
            return 0;
        case WM_CAPTURECHANGED:
        case WM_CANCELMODE:
        case WM_NCDESTROY:
            if (numericDragEdit_ == edit) {
                numericDragEdit_ = nullptr;
                numericDragging_ = false;
            }
            break;
        }
        return std::nullopt;
    }

    static LRESULT CALLBACK ImageFieldProc(HWND edit, UINT message, WPARAM wParam, LPARAM lParam,
                                           UINT_PTR, DWORD_PTR data) {
        auto* overlay = reinterpret_cast<LayeredOverlay*>(data);
        const HWND parent = GetParent(edit);
        const bool angle = edit == overlay->cropAngleHwnd_;
        if (angle) {
            if (const auto handled = overlay->HandleCropInputMouse(edit, message, wParam, lParam)) return *handled;
        } else if (const auto handled = overlay->HandleNumericDrag(edit, message, wParam, lParam)) {
            return *handled;
        }
        if (message == WM_SETCURSOR) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;
        }
        if (GetDlgCtrlID(edit) == IDC_FPS_EDIT) return DefSubclassProc(edit, message, wParam, lParam);
        if (message == WM_KEYDOWN) {
            if (wParam == VK_ESCAPE) {
                if (angle) overlay->CloseFeatherPanel();
                else if (parent == overlay->shadowPanelHwnd_) overlay->CloseShadowPanel(false);
                else if (parent == overlay->imagePanelHwnd_) overlay->CloseImagePanel(false);
                else return DefSubclassProc(edit, message, wParam, lParam);
                return 0;
            }
            if (wParam == VK_RETURN) { SetFocus(parent); return 0; }
            if (wParam == VK_TAB) {
                if (const HWND next = GetNextDlgTabItem(parent, edit, GetKeyState(VK_SHIFT) < 0)) SetFocus(next);
                return 0;
            }
            if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
                SendMessageW(edit, EM_SETSEL, 0, -1);
                return 0;
            }
        }
        if (message == WM_CHAR && (wParam == VK_RETURN || wParam == VK_TAB || wParam == 1)) return 0;
        return DefSubclassProc(edit, message, wParam, lParam);
    }

    HWND CreateImageField(HWND panel, UINT id, const RECT& bounds, HFONT font, const wchar_t* label = L"") {
        const HWND edit = CreateWindowExW(0, L"EDIT", label, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP |
            ES_CENTER | ES_AUTOHSCROLL, bounds.left, bounds.top, bounds.right - bounds.left,
            bounds.bottom - bounds.top, panel, reinterpret_cast<HMENU>(UINT_PTR(id)), instance_, nullptr);
        if (!edit) return nullptr;
        if (!SetWindowSubclass(edit, &LayeredOverlay::ImageFieldProc, 1, reinterpret_cast<DWORD_PTR>(this))) {
            DestroyWindow(edit);
            return nullptr;
        }
        RegisterNumericHint(edit);
        SendMessageW(edit, EM_LIMITTEXT, 12, 0);
        SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
        return edit;
    }

    const wchar_t* ImagePanelTitle() const {
        return imageEffectsTab_ ? Tr(L"画面混合") : Tr(L"图像");
    }

    void ShowImagePanel(bool blend = false) {
        CloseShadowPanel(false);
        if (!CropAvailable()) return;
        CloseFeatherPanel();
        if (imagePanelHwnd_) {
            if (imageEffectsTab_ != blend) {
                imageEffectsTab_ = blend;
                imagePanelHover_ = 0;
                SetWindowTextW(imagePanelHwnd_, ImagePanelTitle());
                for (size_t i = 0; i < imagePanelEdits_.size(); ++i) {
                    ShowWindow(imagePanelEdits_[i], (i >= 4) == blend ? SW_SHOWNA : SW_HIDE);
                }
                SetWindowPos(imagePanelHwnd_, nullptr, 0, 0, ImagePanelPx(440), ImagePanelPx(blend ? 529 : 442),
                    SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                InvalidateRect(imagePanelHwnd_, nullptr, FALSE);
            }
            SetForegroundWindow(imagePanelHwnd_);
            return;
        }
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = &LayeredOverlay::ImagePanelProc;
        windowClass.hInstance = instance_;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.lpszClassName = L"VTSFloatMeowImagePanel";
        if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
        RECT toolbar{};
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (!GetWindowRect(toolbarHwnd_, &toolbar) ||
            !GetMonitorInfoW(MonitorFromWindow(toolbarHwnd_, MONITOR_DEFAULTTONEAREST), &monitor)) return;
        imagePanelScale_ = (std::min)({uiScalePercent_ / 100.0,
            (monitor.rcWork.right - monitor.rcWork.left) / 440.0,
            (monitor.rcWork.bottom - monitor.rcWork.top) / 529.0});
        const SIZE size{ImagePanelPx(440), ImagePanelPx(blend ? 529 : 442)};
        const POINT position = SidePanelPosition(toolbar, monitor.rcWork, nullptr, size, UiScaled(8), toolbar.top);
        imageToneDraft_ = imageTone_;
        imageEffectsDraft_ = imageEffects_;
        imageEffectsTab_ = blend;
        imageCurveChannel_ = 0;
        imagePanelHover_ = 0;
        imagePanelDetached_ = false;
        imagePanelHwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | (desktopMode_ ? 0 : WS_EX_TOPMOST),
            windowClass.lpszClassName, ImagePanelTitle(),
            WS_POPUP | WS_CLIPCHILDREN, position.x, position.y, size.cx, size.cy, toolbarHwnd_, nullptr, instance_, this);
        if (!imagePanelHwnd_) return;
        imagePanelFont_ = CreateFontW(-ImagePanelPx(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        const RECT fields[]{ImagePanelRect(124, 47, 188, 72), ImagePanelRect(142, 80, 193, 105),
            ImagePanelRect(227, 80, 282, 105), ImagePanelRect(360, 80, 425, 105)};
        for (UINT i = 0; i < imagePanelEdits_.size(); ++i) {
            RECT bounds{};
            if (i < ARRAYSIZE(fields)) bounds = fields[i];
            else {
                const int y = ImageEffectRowY(i - ARRAYSIZE(fields));
                bounds = ImagePanelRect(356, y - 2, 416, y + 22);
            }
            imagePanelEdits_[i] = CreateImageField(imagePanelHwnd_, 4200 + i, bounds, imagePanelFont_);
            if (!imagePanelEdits_[i]) {
                CloseImagePanel(false);
                return;
            }
            if ((i >= ARRAYSIZE(fields)) != blend) ShowWindow(imagePanelEdits_[i], SW_HIDE);
        }
        UpdateImageFields();
        ShowWindow(imagePanelHwnd_, SW_SHOW);
        SetForegroundWindow(imagePanelHwnd_);
    }

    static LRESULT CALLBACK ImagePanelProc(HWND panel, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* overlay = reinterpret_cast<LayeredOverlay*>(GetWindowLongPtrW(panel, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            overlay = static_cast<LayeredOverlay*>(create->lpCreateParams);
            SetWindowLongPtrW(panel, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
        }
        if (!overlay) return DefWindowProcW(panel, message, wParam, lParam);
        const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        switch (message) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: overlay->PaintImagePanel(panel); return 0;
        case WM_CTLCOLOREDIT: return reinterpret_cast<LRESULT>(CropNumericBrush(reinterpret_cast<HDC>(wParam)));
        case WM_COMMAND: {
            const UINT id = LOWORD(wParam);
            if (overlay->imagePanelHwnd_ == panel && !overlay->imageUpdatingFields_ && id >= 4200 && id - 4200 < overlay->imagePanelEdits_.size()) {
                if (HIWORD(wParam) == EN_CHANGE) overlay->ReadImageField(id - 4200);
                if (HIWORD(wParam) == EN_KILLFOCUS) overlay->UpdateImageFields();
                return 0;
            }
            break;
        }
        case WM_MOVING: {
            const auto* proposed = reinterpret_cast<const RECT*>(lParam);
            RECT current{};
            if (proposed && GetWindowRect(panel, &current) &&
                (proposed->left != current.left || proposed->top != current.top)) {
                overlay->imagePanelDetached_ = true;
            }
            return TRUE;
        }
        case WM_NCHITTEST: {
            POINT local = point;
            ScreenToClient(panel, &local);
            if (local.y < overlay->ImagePanelPx(40) && !overlay->ImagePanelHit(local)) return HTCAPTION;
            return HTCLIENT;
        }
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                POINT local{};
                GetCursorPos(&local);
                ScreenToClient(panel, &local);
                const RECT graph = overlay->ImageCurveBounds();
                const RECT levels = overlay->ImageLevelsHitArea();
                SetCursor(LoadCursorW(nullptr, overlay->ImagePanelHit(local) ? IDC_HAND :
                    overlay->imageEffectsTab_ ? (overlay->ImageEffectHit(local) >= 0 ? IDC_SIZEWE : IDC_ARROW) :
                    PtInRect(&levels, local) ? IDC_SIZEWE : PtInRect(&graph, local) ? IDC_CROSS : IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_LBUTTONDOWN: {
            SetFocus(panel);
            const int action = overlay->ImagePanelHit(point);
            const RECT graph = overlay->ImageCurveBounds();
            const RECT levels = overlay->ImageLevelsHitArea();
            if (action) overlay->ActivateImagePanelButton(action);
            else if (overlay->imageEffectsTab_) {
                const int effect = overlay->ImageEffectHit(point);
                if (effect >= 0) {
                    overlay->imageEffectDrag_ = effect;
                    SetCapture(panel);
                    overlay->UpdateImageEffect(point.x);
                }
            } else if (PtInRect(&levels, point)) {
                const auto handles = overlay->ImageLevelsHandles();
                const auto nearest = std::min_element(handles.begin(), handles.end(), [&](int a, int b) {
                    return std::abs(a - point.x) < std::abs(b - point.x);
                });
                overlay->imageLevelsDrag_ = static_cast<int>(nearest - handles.begin());
                SetCapture(panel);
                overlay->UpdateImageLevel(point.x);
            } else if (PtInRect(&graph, point)) {
                overlay->imageCurveKnot_ = (std::clamp)(MulDiv(point.x - graph.left, 4, graph.right - graph.left - 1), 0, 4);
                SetCapture(panel);
                overlay->UpdateImageCurve(point);
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (GetCapture() == panel) {
                if (overlay->imageCurveKnot_ >= 0) overlay->UpdateImageCurve(point);
                if (overlay->imageLevelsDrag_ >= 0) overlay->UpdateImageLevel(point.x);
                if (overlay->imageEffectDrag_ >= 0) overlay->UpdateImageEffect(point.x);
            }
            const int hover = overlay->ImagePanelHit(point);
            if (std::exchange(overlay->imagePanelHover_, hover) != hover) InvalidateRect(panel, nullptr, FALSE);
            TRACKMOUSEEVENT track{sizeof(TRACKMOUSEEVENT), TME_LEAVE, panel, 0};
            TrackMouseEvent(&track);
            return 0;
        }
        case WM_MOUSELEAVE:
            overlay->imagePanelHover_ = 0;
            InvalidateRect(panel, nullptr, FALSE);
            return 0;
        case WM_LBUTTONUP:
            if (GetCapture() == panel) ReleaseCapture();
            return 0;
        case WM_CAPTURECHANGED:
            overlay->imageCurveKnot_ = overlay->imageLevelsDrag_ = overlay->imageEffectDrag_ = -1;
            InvalidateRect(panel, nullptr, FALSE);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) { overlay->CloseImagePanel(false); return 0; }
            break;
        case WM_CLOSE: overlay->CloseImagePanel(false); return 0;
        case WM_NCDESTROY:
            if (overlay->imagePanelHwnd_ == panel) {
                overlay->imagePanelHwnd_ = nullptr;
                overlay->imagePanelEdits_.fill(nullptr);
                if (const HFONT font = std::exchange(overlay->imagePanelFont_, nullptr)) DeleteObject(font);
            }
            break;
        }
        return DefWindowProcW(panel, message, wParam, lParam);
    }
