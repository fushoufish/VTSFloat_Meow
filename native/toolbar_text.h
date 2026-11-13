#pragma once

#include <windows.h>

#include <string>

namespace vtsfloat {

// Measure and paint using the same flags so wrapped lines fit the actual button.
inline bool DrawFittedToolbarText(HDC dc, RECT bounds, const std::wstring& text,
                                 const wchar_t* fontFace, int preferredHeight,
                                 int minimumHeight, bool paint = true) {
    constexpr UINT flags = DT_CENTER | DT_WORDBREAK | DT_NOPREFIX;
    for (int height = preferredHeight; height >= minimumHeight; --height) {
        HFONT font = CreateFontW(
            -height, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontFace);
        if (!font) return false;
        HGDIOBJ previous = SelectObject(dc, font);
        RECT measured{0, 0, bounds.right - bounds.left, 0};
        DrawTextW(dc, text.c_str(), -1, &measured, flags | DT_CALCRECT);
        const int availableHeight = bounds.bottom - bounds.top;
        const bool fits = measured.right <= bounds.right - bounds.left &&
            measured.bottom <= availableHeight;
        if (fits && paint) {
            RECT centered = bounds;
            centered.top += (availableHeight - measured.bottom) / 2;
            DrawTextW(dc, text.c_str(), -1, &centered, flags);
        }
        SelectObject(dc, previous);
        DeleteObject(font);
        if (fits) return true;
    }
    return false;
}

} // namespace vtsfloat
