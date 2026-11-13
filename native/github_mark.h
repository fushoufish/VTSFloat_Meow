#pragma once

#include <windows.h>
#include <gdiplus.h>

namespace vtsfloat {

inline void DrawGithubMark(HDC dc, const RECT& bounds) {
    // Path coordinates from github.svg, in its original 24 × 24 view box.
    Gdiplus::GraphicsPath path(Gdiplus::FillModeAlternate);
    path.AddBezier(12.0f, 0.0f, 18.63f, 0.0f, 24.0f, 5.276f, 24.0f, 11.79f);
    path.AddBezier(24.0f, 11.79f, 23.999f, 16.857f, 20.71f, 21.357f, 15.825f, 22.977f);
    path.AddBezier(15.825f, 22.977f, 15.225f, 23.095f, 15.0f, 22.727f, 15.0f, 22.417f);
    path.AddBezier(15.0f, 22.417f, 15.0f, 22.019f, 15.015f, 20.752f, 15.015f, 19.175f);
    path.AddBezier(15.015f, 19.175f, 15.015f, 18.07f, 14.64f, 17.362f, 14.205f, 16.994f);
    path.AddBezier(14.205f, 16.994f, 16.875f, 16.699f, 19.68f, 15.697f, 19.68f, 11.172f);
    path.AddBezier(19.68f, 11.172f, 19.68f, 9.875f, 19.215f, 8.828f, 18.45f, 8.003f);
    path.AddBezier(18.45f, 8.003f, 18.57f, 7.708f, 18.99f, 6.5f, 18.33f, 4.878f);
    path.AddBezier(18.33f, 4.878f, 18.33f, 4.878f, 17.325f, 4.554f, 15.03f, 6.087f);
    path.AddBezier(15.03f, 6.087f, 14.0523f, 5.8207f, 13.0433f, 5.6868f, 12.03f, 5.689f);
    path.AddBezier(12.03f, 5.689f, 11.01f, 5.689f, 9.99f, 5.822f, 9.03f, 6.087f);
    path.AddBezier(9.03f, 6.087f, 6.735f, 4.569f, 5.73f, 4.878f, 5.73f, 4.878f);
    path.AddBezier(5.73f, 4.878f, 5.07f, 6.5f, 5.49f, 7.708f, 5.61f, 8.003f);
    path.AddBezier(5.61f, 8.003f, 4.845f, 8.828f, 4.38f, 9.89f, 4.38f, 11.172f);
    path.AddBezier(4.38f, 11.172f, 4.38f, 15.682f, 7.17f, 16.699f, 9.84f, 16.994f);
    path.AddBezier(9.84f, 16.994f, 9.495f, 17.288f, 9.18f, 17.804f, 9.075f, 18.571f);
    path.AddBezier(9.075f, 18.571f, 8.385f, 18.881f, 6.66f, 19.381f, 5.58f, 17.598f);
    path.AddBezier(5.58f, 17.598f, 5.355f, 17.244f, 4.68f, 16.375f, 3.735f, 16.389f);
    path.AddBezier(3.735f, 16.389f, 2.73f, 16.404f, 3.33f, 16.949f, 3.75f, 17.17f);
    path.AddBezier(3.75f, 17.17f, 4.26f, 17.45f, 4.845f, 18.497f, 4.98f, 18.836f);
    path.AddBezier(4.98f, 18.836f, 5.22f, 19.499f, 6.0f, 20.766f, 9.015f, 20.221f);
    path.AddBezier(9.015f, 20.221f, 9.015f, 21.209f, 9.03f, 22.137f, 9.03f, 22.417f);
    path.AddBezier(9.03f, 22.417f, 9.03f, 22.727f, 8.805f, 23.081f, 8.205f, 22.977f);
    path.AddBezier(8.205f, 22.977f, 3.303f, 21.374f, -0.003f, 16.867f, 0.0f, 11.791f);
    path.AddBezier(0.0f, 11.791f, 0.0f, 5.276f, 5.37f, 0.0f, 12.0f, 0.0f);
    path.CloseFigure();
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.TranslateTransform(static_cast<float>(bounds.left), static_cast<float>(bounds.top));
    graphics.ScaleTransform((bounds.right - bounds.left) / 24.0f, (bounds.bottom - bounds.top) / 24.0f);
    const Gdiplus::SolidBrush white(Gdiplus::Color(255, 255, 255));
    graphics.FillPath(&white, &path);
}

}  // namespace vtsfloat
