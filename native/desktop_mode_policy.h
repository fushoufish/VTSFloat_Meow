#pragma once
#include <windows.h>

namespace vtsfloat {
// Use full monitor bounds, not the work area: ordinary maximized windows
// that leave room for the taskbar must not suspend the desktop model.
inline bool CoversMonitor(const RECT& window, const RECT& monitor) {
    constexpr LONG tolerance = 1;
    return monitor.right > monitor.left && monitor.bottom > monitor.top &&
        window.left <= monitor.left + tolerance && window.top <= monitor.top + tolerance &&
        window.right >= monitor.right - tolerance && window.bottom >= monitor.bottom - tolerance;
}
}
