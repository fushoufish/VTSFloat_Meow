    vtsfloat::ImageShadow& EditedShadow() { return shadowDraft_[shadowTab_]; }
    const vtsfloat::ImageShadow& EditedShadow() const { return shadowDraft_[shadowTab_]; }

    const std::array<vtsfloat::ImageShadow, 2>& ActiveShadows() const {
        return shadowPanelHwnd_ ? shadowDraft_ : shadow_;
    }

    static bool ShadowUsesBackdrop(const vtsfloat::ImageShadow& shadow) {
        return shadow.enabled && shadow.opacity && shadow.sampleBackground && shadow.darkness;
    }

    bool ShadowUsesBackdrop() const {
        const auto& shadows = ActiveShadows();
        return ShadowUsesBackdrop(shadows[0]) || ShadowUsesBackdrop(shadows[1]);
    }

    static constexpr const wchar_t* kShadowSections[]{L"shadow_inner", L"shadow_outer"};

    void LoadShadowSettings() {
        const auto legacy = [](const wchar_t* key, int fallback) { return ReadConfigInt(L"shadow", key, fallback); };
        const bool legacyEnabled = legacy(L"enabled", 0) != 0;
        const bool legacyInside = legacy(L"inside", 0) != 0;
        for (size_t i = 0; i < shadow_.size(); ++i) {
            auto& shadow = shadow_[i];
            const vtsfloat::ImageShadow defaults{};
            const auto read = [&](const wchar_t* key, int fallback, int minimum, int maximum) {
                return (std::clamp)(ReadConfigInt(kShadowSections[i], key, legacy(key, fallback)), minimum, maximum);
            };
            const bool enabled = legacy(i == 0 ? L"inner_enabled" : L"outer_enabled",
                legacyEnabled && (legacyInside == (i == 0))) != 0;
            shadow.enabled = ReadConfigInt(kShadowSections[i], L"enabled", enabled) != 0;
            shadow.angle = read(L"angle", defaults.angle, 0, 359);
            shadow.distance = read(L"distance_hundredths", (std::clamp)(legacy(L"distance_tenths", defaults.distance / 10), 0, 5000) * 10, 0, 50000);
            shadow.spread = read(L"spread", defaults.spread, 0, 100);
            shadow.blur = read(L"blur", defaults.blur, 0, 200);
            shadow.opacity = read(L"opacity", defaults.opacity, 0, 100);
            shadow.color = read(L"color", defaults.color, 0, 0xffffff);
            shadow.sampleBackground = read(L"sample_background", 0, 0, 1) != 0;
            shadow.darkness = read(L"darkness", defaults.darkness, 0, 100);
        }
    }

    void SaveShadowSettings() const {
        for (size_t i = 0; i < shadow_.size(); ++i) {
            const auto& shadow = shadow_[i];
            const auto* section = kShadowSections[i];
            WriteConfigInt(section, L"enabled", shadow.enabled);
            WriteConfigInt(section, L"angle", shadow.angle);
            WriteConfigInt(section, L"distance_hundredths", shadow.distance);
            WriteConfigInt(section, L"spread", shadow.spread);
            WriteConfigInt(section, L"blur", shadow.blur);
            WriteConfigInt(section, L"opacity", shadow.opacity);
            WriteConfigInt(section, L"color", shadow.color);
            WriteConfigInt(section, L"sample_background", shadow.sampleBackground);
            WriteConfigInt(section, L"darkness", shadow.darkness);
        }
    }

    void PrepareShadow(int width, int height) {
        const auto* model = static_cast<const uint8_t*>(dibBits_);
        const auto& shadows = ActiveShadows();
        for (size_t i = 0; i < shadows.size(); ++i) {
            const auto& shadow = shadows[i];
            shadowMaskReady_[i] = shadow.enabled && shadow.opacity;
            if (!shadowMaskReady_[i]) continue;
            const double angle = shadow.angle * 3.14159265358979323846 / 180;
            shadowOffsetX_[i] = std::cos(angle) * shadow.distance / 100;
            shadowOffsetY_[i] = std::sin(angle) * shadow.distance / 100;
            shadowMasks_[i].Build(model, width, height, shadow.spread, shadow.blur, i == 0);
        }
    }

    bool ShadowAffectsBackdropCell(size_t x0, size_t y0, size_t x1, size_t y1) const {
        if (!shadowMaskReady_[1]) return false;
        const int dx = static_cast<int>(std::floor(shadowOffsetX_[1]));
        const int dy = static_cast<int>(std::floor(shadowOffsetY_[1]));
        return shadowMasks_[1].Touches(static_cast<int>(x0) - dx - 1, static_cast<int>(y0) - dy - 1,
            static_cast<int>(x1) - dx, static_cast<int>(y1) - dy);
    }

    void CompositeShadow(int width, int height, size_t index) {
        const auto& shadow = ActiveShadows()[index];
        const auto& shadowMask = shadowMasks_[index];
        const bool inside = index == 0;
        const int baseAlpha = modelOpacityPercent_ * 255 / 100;
        const int shadowAlpha = fadeShadowOnHover_ && hoverFadeEnabled_ ? currentOverlayAlpha_ : baseAlpha;
        const unsigned color[]{GetBValue(shadow.color), GetGValue(shadow.color), GetRValue(shadow.color)};
        const bool sampled = ShadowUsesBackdrop(shadow) && imageBackdropColorTime_ != Clock::time_point{};
        // Adjacent output pixels alternate between two sampling phases.
        const auto phases = [](double offset) {
            std::array<std::pair<int, unsigned>, 2> result{};
            for (int parity = 0; parity < 2; ++parity) {
                const double position = (parity - offset - .5) / vtsfloat::ShadowMask::scale;
                const int base = static_cast<int>(std::floor(position));
                result[parity] = {base, static_cast<unsigned>(std::lround((position - base) * 256))};
            }
            return result;
        };
        const auto columns = phases(shadowOffsetX_[index]);
        const auto rows = phases(shadowOffsetY_[index]);
        auto* pixel = static_cast<uint8_t*>(dibBits_);
        for (int y = 0; y < height; ++y) {
            const auto [top, fy] = rows[y & 1];
            for (int x = 0; x < width; ++x, pixel += 4) {
                const unsigned alpha = pixel[3];
                if (inside ? alpha == 0 : alpha == 255) continue;
                const auto [left, fx] = columns[x & 1];
                const unsigned mask = shadowMask.Sample(left + x / 2, top + y / 2, fx, fy);
                if (!mask) continue;
                const unsigned coverage = inside ? (mask * shadow.opacity + 50) / 100 :
                    (mask * shadow.opacity * shadowAlpha + 12750) / 25500;
                const unsigned applied = (coverage * (inside ? alpha : 255 - alpha) + 127) / 255;
                if (!applied) continue;
                const auto backdrop = sampled ? ImageBackdropColorAt(x, y, width, height) : vtsfloat::Rgb{};
                for (int c = 0; c < 3; ++c) {
                    const unsigned tint = sampled ? static_cast<unsigned>(
                        backdrop[c] * (100 - shadow.darkness) / 100 + .5f) : color[c];
                    const unsigned original = inside ? (pixel[c] * (255 - coverage) + 127) / 255 : pixel[c];
                    pixel[c] = static_cast<uint8_t>(original + (tint * applied + 127) / 255);
                }
                if (!inside) pixel[3] = static_cast<uint8_t>(alpha + applied);
            }
        }
    }

    struct ShadowField {
        const wchar_t* label;
        int vtsfloat::ImageShadow::*member;
        int limit;
        int scale;
        const wchar_t* unit;
    };

    static std::array<ShadowField, 6> ShadowFields() {
        using S = vtsfloat::ImageShadow;
        return {{{Tr(L"角度"), &S::angle, 359, 1, L"°"},
            {Tr(L"阴影距离"), &S::distance, 50000, 100, L"px"},
            {Tr(L"阴影扩展"), &S::spread, 100, 1, L"px"},
            {Tr(L"阴影模糊"), &S::blur, 200, 1, L"px"},
            {Tr(L"不透明度"), &S::opacity, 100, 1, L"%"},
            {Tr(L"加深程度"), &S::darkness, 100, 1, L"%"}}};
    }

    enum ShadowAction { ShadowClose = 1, ShadowInside, ShadowOutside, ShadowToggle,
        ShadowColor, ShadowSample, ShadowHoverFade, ShadowReset, ShadowCancel, ShadowSave };
    static constexpr int kShadowWidth = 440, kShadowHeight = 448, kShadowFirstField = 4350;

    int ShadowPx(int px) const { return static_cast<int>(std::lround(px * shadowPanelScale_)); }
    RECT ShadowRect(int left, int top, int right, int bottom) const {
        return {ShadowPx(left), ShadowPx(top), ShadowPx(right), ShadowPx(bottom)};
    }

    struct ShadowButton { ShadowAction action; RECT bounds; const wchar_t* label; bool selected = false; };
    std::array<ShadowButton, 10> ShadowButtons() const {
        return {{{ShadowClose, ShadowRect(394, 2, 438, 38), L"×"},
            {ShadowHoverFade, ShadowRect(124, 6, 384, 34), Tr(L"悬停时同时淡化阴影"), fadeShadowOnHover_},
            {ShadowInside, ShadowRect(14, 48, 143, 86), Tr(L"内部阴影"), shadowTab_ == 0},
            {ShadowOutside, ShadowRect(149, 48, 278, 86), Tr(L"外部阴影"), shadowTab_ == 1},
            {ShadowToggle, ShadowRect(286, 48, 426, 86), Tr(L"启用阴影"), EditedShadow().enabled},
            {ShadowColor, ShadowRect(14, 314, 145, 347), Tr(L"阴影颜色"), shadowColorOpen_},
            {ShadowSample, ShadowRect(153, 314, 426, 347), Tr(L"参考背景像素"), EditedShadow().sampleBackground},
            {ShadowReset, ShadowRect(14, 402, 144, 434), Tr(L"重置")},
            {ShadowCancel, ShadowRect(152, 402, 284, 434), Tr(L"取消")},
            {ShadowSave, ShadowRect(292, 402, 426, 434), Tr(L"保存"), true}}};
    }

    int ShadowActionAt(POINT point) const {
        for (const auto& button : ShadowButtons()) {
            if (PtInRect(&button.bounds, point)) return button.action;
        }
        return 0;
    }

    static int ShadowRow(size_t index) { return index == 5 ? 369 : 112 + 43 * static_cast<int>(index); }

    void RefreshShadowPanel(bool fields) {
        if (!shadowPanelHwnd_) return;
        if (fields) {
            const auto controls = ShadowFields();
            shadowUpdatingFields_ = true;
            for (size_t i = 0; i < controls.size(); ++i) {
                const auto& field = controls[i];
                const int value = EditedShadow().*field.member;
                wchar_t label[24]{};
                if (field.scale == 100) swprintf_s(label, L"%d.%02d", value / 100, value % 100);
                else swprintf_s(label, L"%d", value);
                SetWindowTextW(shadowFields_[i], label);
            }
            shadowUpdatingFields_ = false;
            ShowWindow(shadowFields_.back(), EditedShadow().sampleBackground ? SW_SHOWNA : SW_HIDE);
        }
        if (!ScreenImageBlendActive()) StopImageBackdrop();
        PositionBorderPanel();
        InvalidateRect(shadowPanelHwnd_, nullptr, FALSE);
        resetFrameSchedule_ = true;
    }

    RECT ShadowColorWheel() const { return ShadowRect(32, 206, 116, 290); }
    RECT ShadowColorBrightness() const { return ShadowRect(31, 301, 117, 307); }
    static constexpr int kShadowColorWheelDrag = 6, kShadowColorBrightnessDrag = 7;

    void PaintShadowColor(HDC dc) const {
        constexpr int size = 96;
        const RECT wheel = ShadowColorWheel();
        BITMAPINFO bitmap{};
        bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap.bmiHeader.biWidth = size;
        bitmap.bmiHeader.biHeight = -size;
        bitmap.bmiHeader.biPlanes = 1;
        bitmap.bmiHeader.biBitCount = 32;
        StretchDIBits(dc, wheel.left, wheel.top, wheel.right - wheel.left, wheel.bottom - wheel.top,
            0, 0, size, size, shadowWheelPixels_.data(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
        const double angle = shadowHue_ * 6.283185307179586;
        const double radius = (wheel.right - wheel.left) * (43.0 / size) * shadowSaturation_;
        const int x = static_cast<int>((wheel.left + wheel.right) / 2 + std::cos(angle) * radius);
        const int y = static_cast<int>((wheel.top + wheel.bottom) / 2 + std::sin(angle) * radius);
        const int marker = (std::max)(3, ShadowPx(5));
        const HGDIOBJ oldPen = SelectObject(dc, GetStockObject(WHITE_PEN));
        const HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Ellipse(dc, x - marker, y - marker, x + marker, y + marker);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        PanelText(dc, Tr(L"亮度"), ShadowRect(16, 284, 132, 299), RGB(184, 207, 231),
            ShadowPx(11), false, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        PanelSlider(dc, ShadowColorBrightness(), shadowColorValue_, 0, 100,
            shadowDrag_ == kShadowColorBrightnessDrag);
    }

    void UpdateShadowColor(POINT point) {
        if (shadowDrag_ == kShadowColorWheelDrag) {
            const RECT wheel = ShadowColorWheel();
            const double dx = point.x - (wheel.left + wheel.right) / 2.0;
            const double dy = point.y - (wheel.top + wheel.bottom) / 2.0;
            const double radius = (wheel.right - wheel.left) * (43.0 / 96);
            shadowHue_ = std::atan2(dy, dx) / 6.283185307179586;
            if (shadowHue_ < 0) shadowHue_ += 1;
            shadowSaturation_ = (std::min)(1.0, std::hypot(dx, dy) / radius);
            if (!shadowColorValue_) shadowColorValue_ = 100;
        } else {
            const RECT track = ShadowColorBrightness();
            shadowColorValue_ = MulDiv((std::clamp)(point.x, track.left, track.right) - track.left,
                100, track.right - track.left);
        }
        const auto color = HsvWheelColor(shadowHue_, shadowSaturation_, shadowColorValue_ / 100.0);
        EditedShadow().color = RGB(color.red, color.green, color.blue);
        RefreshShadowPanel(false);
    }

    void PaintShadowPanel(HWND panel) const {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(panel, &paint);
        RECT client{};
        GetClientRect(panel, &client);
        PanelFill(dc, client, RGB(16, 25, 39));
        PanelFill(dc, ShadowRect(0, 0, 440, 40), RGB(27, 45, 68));
        constexpr UINT line = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
        const auto text = [&](const wchar_t* label, RECT rect, bool center = false, bool bold = false) {
            PanelText(dc, label, rect, RGB(227, 239, 252), ShadowPx(14), bold,
                line | (center ? DT_CENTER : DT_LEFT));
        };
        text(Tr(L"阴影"), ShadowRect(14, 0, 117, 40), false, true);
        PanelFill(dc, ShadowRect(14, 85, 278, 86), RGB(48, 71, 97));
        for (const auto& button : ShadowButtons()) {
            if (button.action == ShadowInside || button.action == ShadowOutside) {
                if (button.selected) {
                    PanelFill(dc, button.bounds, RGB(32, 57, 85));
                    RECT underline = button.bounds;
                    underline.top = underline.bottom - (std::max)(2, ShadowPx(3));
                    PanelFill(dc, underline, RGB(109, 190, 255));
                }
                PanelText(dc, button.label, button.bounds,
                    button.selected ? RGB(242, 249, 255) : RGB(153, 177, 202),
                    ShadowPx(13), button.selected, line | DT_CENTER);
                continue;
            }
            if (button.action == ShadowToggle) {
                RECT label = button.bounds;
                label.right = ShadowPx(381);
                PanelText(dc, button.label, label, RGB(197, 217, 238),
                    ShadowPx(12), false, line | DT_RIGHT);
                const RECT track = ShadowRect(389, 59, 423, 76);
                SetDCBrushColor(dc, button.selected ? RGB(50, 124, 193) : RGB(50, 68, 89));
                const HGDIOBJ brush = SelectObject(dc, GetStockObject(DC_BRUSH));
                const HGDIOBJ pen = SelectObject(dc, GetStockObject(NULL_PEN));
                RoundRect(dc, track.left, track.top, track.right, track.bottom, ShadowPx(17), ShadowPx(17));
                SetDCBrushColor(dc, button.selected ? RGB(241, 249, 255) : RGB(165, 184, 203));
                const RECT knob = button.selected ? ShadowRect(408, 61, 421, 74) : ShadowRect(391, 61, 404, 74);
                Ellipse(dc, knob.left, knob.top, knob.right, knob.bottom);
                SelectObject(dc, pen);
                SelectObject(dc, brush);
                continue;
            }
            PanelFill(dc, button.bounds, button.selected ? RGB(31, 85, 141) : RGB(24, 42, 64));
            RECT label = button.bounds;
            if (button.action == ShadowColor) {
                label.right -= ShadowPx(27);
                PanelFill(dc, ShadowRect(119, 320, 138, 341), RGB(131, 161, 192));
                PanelFill(dc, ShadowRect(121, 322, 136, 339), EditedShadow().color);
            }
            text(button.label, label, true, true);
        }

        const int cx = ShadowPx(74), cy = ShadowPx(153), radius = ShadowPx(46);
        const HPEN rim = CreatePen(PS_SOLID, (std::max)(1, ShadowPx(1)), RGB(77, 111, 151));
        const HGDIOBJ previousPen = SelectObject(dc, rim);
        const HGDIOBJ previousBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Ellipse(dc, cx - radius, cy - radius, cx + radius, cy + radius);
        const double angle = EditedShadow().angle * 3.14159265358979323846 / 180;
        const HPEN needle = CreatePen(PS_SOLID, (std::max)(1, ShadowPx(3)), RGB(108, 188, 255));
        SelectObject(dc, needle);
        MoveToEx(dc, cx, cy, nullptr);
        LineTo(dc, cx + static_cast<int>(radius * std::cos(angle)), cy + static_cast<int>(radius * std::sin(angle)));
        SelectObject(dc, previousBrush);
        SelectObject(dc, previousPen);
        DeleteObject(needle);
        DeleteObject(rim);

        if (shadowColorOpen_) PaintShadowColor(dc);

        const auto controls = ShadowFields();
        for (size_t i = 0; i < controls.size(); ++i) {
            if (i == 5 && !EditedShadow().sampleBackground) continue;
            const int y = ShadowRow(i);
            const auto& field = controls[i];
            text(field.label, ShadowRect(143, y - 17, 346, y + 2));
            text(field.unit, ShadowRect(399, y + 1, 428, y + 25));
            if (i > 0) {
                const int value = EditedShadow().*field.member;
                const int position = i == 1 ? DistanceTrackPosition(value) : value;
                PanelSlider(dc, ShadowRect(145, y + 10, 332, y + 16), position,
                    0, i == 1 ? 10000 : field.limit, shadowDrag_ == static_cast<int>(i));
            }
        }
        EndPaint(panel, &paint);
    }

    static int DistanceTrackPosition(int distance) {
        return distance <= 2000 ? MulDiv(distance, 5000, 2000) :
            5000 + MulDiv((std::min)(distance, 10000) - 2000, 5000, 8000);
    }

    static int DistanceFromTrackPosition(int position) {
        return position <= 5000 ? MulDiv(position, 2000, 5000) :
            2000 + MulDiv(position - 5000, 8000, 5000);
    }

    void UpdateShadowDrag(POINT point) {
        if (shadowDrag_ == kShadowColorWheelDrag || shadowDrag_ == kShadowColorBrightnessDrag) {
            UpdateShadowColor(point);
            return;
        }
        if (shadowDrag_ == 0) {
            const double angle = std::atan2(point.y - ShadowPx(153), point.x - ShadowPx(74));
            EditedShadow().angle = (static_cast<int>(std::lround(angle * 180 / 3.14159265358979323846)) + 360) % 360;
        } else {
            const auto field = ShadowFields()[shadowDrag_];
            const int left = ShadowPx(145), right = ShadowPx(332);
            const int x = (std::clamp)(static_cast<int>(point.x), left, right);
            if (shadowDrag_ == 1) {
                const int middle = (left + right) / 2;
                const int position = x <= middle ? MulDiv(x - left, 5000, middle - left) :
                    5000 + MulDiv(x - middle, 5000, right - middle);
                EditedShadow().distance = DistanceFromTrackPosition(position);
            } else {
                EditedShadow().*field.member = MulDiv(x - left, field.limit, right - left);
            }
        }
        RefreshShadowPanel(true);
    }

    void CloseShadowPanel(bool save) {
        if (!shadowPanelHwnd_) return;
        if (save) {
            shadow_ = shadowDraft_;
            SaveShadowSettings();
        }
        DestroyWindow(std::exchange(shadowPanelHwnd_, nullptr));
        PositionBorderPanel();
        if (!ScreenImageBlendActive()) StopImageBackdrop();
        resetFrameSchedule_ = true;
        if (toolbarHwnd_) InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }

    void ShowShadowPanel() {
        if (shadowPanelHwnd_) {
            SetForegroundWindow(shadowPanelHwnd_);
            return;
        }
        if (!viewEditing_ || cropEditing_ || !CropAvailable()) return;
        CloseImagePanel(false);
        CloseFeatherPanel();
        RECT toolbar{};
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (!GetWindowRect(toolbarHwnd_, &toolbar) ||
            !GetMonitorInfoW(MonitorFromWindow(toolbarHwnd_, MONITOR_DEFAULTTONEAREST), &monitor)) return;
        shadowPanelScale_ = (std::min)({uiScalePercent_ / 100.0,
            (monitor.rcWork.right - monitor.rcWork.left) / double(kShadowWidth),
            (monitor.rcWork.bottom - monitor.rcWork.top) / double(kShadowHeight)});
        const SIZE size{ShadowPx(kShadowWidth), ShadowPx(kShadowHeight)};
        const POINT position = SidePanelPosition(toolbar, monitor.rcWork, nullptr, size, UiScaled(8), toolbar.top);
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = &LayeredOverlay::ShadowPanelProc;
        windowClass.hInstance = instance_;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.lpszClassName = L"VTSFloatMeowShadowPanel";
        if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
        shadowDraft_ = shadow_;
        shadowColorOpen_ = false;
        shadowPanelDetached_ = false;
        shadowPanelHwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | (desktopMode_ ? 0 : WS_EX_TOPMOST),
            windowClass.lpszClassName, Tr(L"阴影"), WS_POPUP | WS_CLIPCHILDREN,
            position.x, position.y, size.cx, size.cy, toolbarHwnd_, nullptr, instance_, this);
        if (!shadowPanelHwnd_) return;
        shadowPanelFont_ = CreateFontW(-ShadowPx(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, UiFontFace());
        for (size_t i = 0; i < shadowFields_.size(); ++i) {
            const int y = ShadowRow(i);
            shadowFields_[i] = CreateImageField(shadowPanelHwnd_, kShadowFirstField + static_cast<UINT>(i),
                ShadowRect(340, y + 2, 397, y + 24), shadowPanelFont_);
            if (!shadowFields_[i]) { CloseShadowPanel(false); return; }
        }
        RefreshShadowPanel(true);
        ShowWindow(shadowPanelHwnd_, SW_SHOW);
        SetForegroundWindow(shadowPanelHwnd_);
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }

    void ActivateShadowButton(int action) {
        switch (action) {
        case ShadowClose: case ShadowCancel: CloseShadowPanel(false); return;
        case ShadowSave: CloseShadowPanel(true); return;
        case ShadowInside:
        case ShadowOutside:
            shadowTab_ = action == ShadowInside ? 0 : 1;
            shadowColorOpen_ = false;
            break;
        case ShadowToggle: EditedShadow().enabled = !EditedShadow().enabled; break;
        case ShadowHoverFade: ToggleShadowHoverFade(); return;
        case ShadowSample: EditedShadow().sampleBackground = !EditedShadow().sampleBackground; break;
        case ShadowReset:
            EditedShadow() = {};
            shadowColorOpen_ = false;
            break;
        case ShadowColor:
            shadowColorOpen_ = !shadowColorOpen_;
            if (shadowColorOpen_) {
                CustomColorHueSaturation(EditedShadow().color, shadowHue_, shadowSaturation_);
                shadowColorValue_ = CustomColorValuePercent(EditedShadow().color);
                if (shadowWheelPixels_.empty()) BuildColorWheelPixels(shadowWheelPixels_, 96, 96, 1, 0x101927);
            }
            break;
        }
        RefreshShadowPanel(true);
    }

    void ReadShadowField(size_t index) {
        if (const auto value = ReadDecimalField(shadowFields_[index])) {
            const auto field = ShadowFields()[index];
            const int next = static_cast<int>(std::lround((std::clamp)(*value * field.scale, 0.0, double(field.limit))));
            if (std::exchange(EditedShadow().*field.member, next) != next) RefreshShadowPanel(false);
        }
    }

    static LRESULT CALLBACK ShadowPanelProc(HWND panel, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* overlay = reinterpret_cast<LayeredOverlay*>(GetWindowLongPtrW(panel, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            overlay = static_cast<LayeredOverlay*>(reinterpret_cast<const CREATESTRUCTW*>(lParam)->lpCreateParams);
            SetWindowLongPtrW(panel, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
        }
        if (!overlay) return DefWindowProcW(panel, message, wParam, lParam);
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        switch (message) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: overlay->PaintShadowPanel(panel); return 0;
        case WM_CTLCOLOREDIT: return reinterpret_cast<LRESULT>(CropNumericBrush(reinterpret_cast<HDC>(wParam)));
        case WM_COMMAND: {
            const UINT id = LOWORD(wParam);
            if (id < kShadowFirstField || id >= kShadowFirstField + overlay->shadowFields_.size() ||
                overlay->shadowUpdatingFields_ || overlay->shadowPanelHwnd_ != panel) break;
            if (HIWORD(wParam) == EN_CHANGE) overlay->ReadShadowField(id - kShadowFirstField);
            else if (HIWORD(wParam) == EN_KILLFOCUS) overlay->RefreshShadowPanel(true);
            return 0;
        }
        case WM_MOVING: {
            const auto* proposed = reinterpret_cast<const RECT*>(lParam);
            RECT current{};
            if (proposed && GetWindowRect(panel, &current) &&
                (proposed->left != current.left || proposed->top != current.top)) {
                overlay->shadowPanelDetached_ = true;
            }
            return TRUE;
        }
        case WM_NCHITTEST:
            ScreenToClient(panel, &point);
            return point.y < overlay->ShadowPx(40) && !overlay->ShadowActionAt(point) ? HTCAPTION : HTCLIENT;
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                POINT cursor{};
                GetCursorPos(&cursor);
                ScreenToClient(panel, &cursor);
                if (overlay->ShadowActionAt(cursor)) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }
            }
            break;
        case WM_LBUTTONDOWN:
            SetFocus(panel);
            if (const int action = overlay->ShadowActionAt(point)) {
                overlay->ActivateShadowButton(action);
                return 0;
            }
            RECT brightness = overlay->ShadowColorBrightness();
            InflateRect(&brightness, 0, overlay->ShadowPx(4));
            const RECT wheel = overlay->ShadowColorWheel();
            if (overlay->shadowColorOpen_ && PtInRect(&brightness, point)) {
                overlay->shadowDrag_ = kShadowColorBrightnessDrag;
            } else if (overlay->shadowColorOpen_ && PtInRect(&wheel, point)) {
                overlay->shadowDrag_ = kShadowColorWheelDrag;
            } else if (const RECT dial = overlay->ShadowRect(25, 104, 124, 203); PtInRect(&dial, point)) {
                overlay->shadowDrag_ = 0;
            } else {
                for (size_t i = 1; i < overlay->shadowFields_.size(); ++i) {
                    if (i == 5 && !overlay->EditedShadow().sampleBackground) break;
                    const int y = ShadowRow(i);
                    const RECT track = overlay->ShadowRect(140, y + 1, 337, y + 25);
                    if (PtInRect(&track, point)) overlay->shadowDrag_ = static_cast<int>(i);
                }
            }
            if (overlay->shadowDrag_ >= 0) {
                SetCapture(panel);
                overlay->UpdateShadowDrag(point);
            }
            return 0;
        case WM_MOUSEMOVE:
            if (GetCapture() == panel && overlay->shadowDrag_ >= 0) overlay->UpdateShadowDrag(point);
            return 0;
        case WM_LBUTTONUP:
            if (GetCapture() == panel) ReleaseCapture();
            return 0;
        case WM_CAPTURECHANGED:
            overlay->shadowDrag_ = -1;
            InvalidateRect(panel, nullptr, FALSE);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) { overlay->CloseShadowPanel(false); return 0; }
            break;
        case WM_CLOSE: overlay->CloseShadowPanel(false); return 0;
        case WM_DESTROY:
            if (overlay->shadowPanelHwnd_ == panel) overlay->shadowPanelHwnd_ = nullptr;
            overlay->shadowFields_.fill(nullptr);
            if (overlay->shadowPanelFont_) DeleteObject(std::exchange(overlay->shadowPanelFont_, nullptr));
            return 0;
        }
        return DefWindowProcW(panel, message, wParam, lParam);
    }
