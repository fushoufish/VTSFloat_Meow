#pragma once

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <new>
#include <utility>

namespace vtsfloat {

// Keep the title bar on the owned window and clip the scrolling body, including
// its native edit controls, inside a child window.
class SidePanelViewport {
public:
    static constexpr UINT PaintHeaderMessage = WM_APP + 207;

    static HWND Host(HWND panel) { return panel ? GetAncestor(panel, GA_ROOT) : nullptr; }

    static int ContentWidthLimit(const RECT& work) {
        return (std::max)(1L, work.right - work.left - kGutter);
    }

    static SIZE WindowSize(SIZE content, int limit) {
        const LONG height = (std::clamp)(static_cast<LONG>(limit), 1L, content.cy);
        return {content.cx + (height < content.cy ? kGutter : 0), height};
    }

    static HWND Create(HINSTANCE instance, HWND owner, DWORD style, const wchar_t* windowClass,
                       const wchar_t* title, POINT position, SIZE content, int limit,
                       int headerHeight, void* context) {
        constexpr wchar_t hostClass[] = L"VTSFloatMeowSidePanelViewport";
        WNDCLASSW window{};
        window.hInstance = instance;
        window.lpszClassName = hostClass;
        window.lpfnWndProc = &WindowProc;
        window.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        if (!RegisterClassW(&window) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return nullptr;

        const SIZE size = WindowSize(content, limit);
        const HWND host = CreateWindowExW(style, hostClass, title, WS_POPUP | WS_CLIPCHILDREN,
            position.x, position.y, size.cx, size.cy, owner, nullptr, instance, nullptr);
        if (!host) return nullptr;
        auto* state = State(host);
        state->contentSize = content;
        state->headerHeight = headerHeight;
        state->clip = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, 0, 0, host, nullptr, instance, nullptr);
        if (!state->clip) {
            DestroyWindow(host);
            return nullptr;
        }
        state->panel = CreateWindowExW(0, windowClass, title, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, content.cx, content.cy, state->clip, nullptr, instance, context);
        if (!state->panel) {
            DestroyWindow(host);
            return nullptr;
        }
        state->Layout(host);
        return state->panel;
    }

    static void Resize(HWND panel, SIZE content, int heightLimit, int headerHeight) {
        const HWND host = Host(panel);
        auto* state = State(host);
        if (!state) return;
        RECT bounds{};
        GetClientRect(host, &bounds);
        const SIZE size = WindowSize(content, heightLimit);
        const bool contentChanged = content.cx != state->contentSize.cx || content.cy != state->contentSize.cy ||
            headerHeight != state->headerHeight;
        state->contentSize = content;
        state->headerHeight = headerHeight;
        if (bounds.right != size.cx || bounds.bottom != size.cy) {
            SetWindowPos(host, nullptr, 0, 0, size.cx, size.cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        } else if (contentChanged) {
            state->Layout(host);
            state->PositionFooter(host);
            InvalidateFooter(panel);
        }
    }

    static void LimitHeight(HWND panel, int heightLimit) {
        if (const auto* state = State(Host(panel))) {
            Resize(panel, state->contentSize, heightLimit, state->headerHeight);
        }
    }

    static void AttachFooter(HWND panel, HWND footer, int referenceWidth, int height) {
        const HWND host = Host(panel);
        if (auto* state = State(host)) {
            state->footer = footer;
            state->footerReferenceWidth = referenceWidth;
            state->footerHeight = height;
            state->PositionFooter(host);
        }
    }

    static int FooterSpace(HWND panel) {
        const auto* state = State(Host(panel));
        return state && state->footer ? state->FooterPx(state->footerHeight) : 0;
    }

    static void InvalidateFooter(HWND panel) {
        const auto* state = State(Host(panel));
        if (state && state->footer) InvalidateRect(state->footer, nullptr, FALSE);
    }

    static bool IsPanelWindow(HWND panel, HWND window) {
        const HWND host = Host(panel);
        const auto* state = State(host);
        return state && window && (window == host || window == state->footer);
    }

    static void RevealField(HWND field) {
        const HWND host = Host(GetParent(field));
        auto* state = State(host);
        if (!state) return;
        RECT viewport{}, bounds{};
        if (!GetWindowRect(state->clip, &viewport) || !GetWindowRect(field, &bounds)) return;
        if (bounds.top < viewport.top) {
            state->PlaceContent(host, state->offset + bounds.top - viewport.top);
        } else if (bounds.bottom > viewport.bottom) {
            state->PlaceContent(host, state->offset + bounds.bottom - viewport.bottom);
        }
    }

    static bool RoutingHeader(HWND panel) {
        const auto* state = State(Host(panel));
        return state && state->routingHeader;
    }

    static POINT CursorPoint(HWND panel) {
        POINT point{};
        GetCursorPos(&point);
        ScreenToClient(RoutingHeader(panel) ? Host(panel) : panel, &point);
        return point;
    }

    static void InvalidateHeader(HWND panel) {
        const HWND host = Host(panel);
        const auto* state = State(host);
        if (!state) return;
        RECT header{};
        GetClientRect(host, &header);
        header.bottom = state->VisibleHeaderHeight(header);
        InvalidateRect(host, &header, FALSE);
    }

    static void DrawScrollbar(HDC dc, const RECT& track, const RECT& thumb, bool dragging) {
        Fill(dc, track, RGB(27, 43, 64));
        Fill(dc, thumb, dragging ? RGB(91, 170, 240) : RGB(78, 139, 199));
    }

private:
    static constexpr LONG kGutter = 22;
    static constexpr LONG kTrackWidth = 11;
    static constexpr wchar_t kStateProperty[] = L"VTSFloatMeowSidePanelViewportState";
    HWND panel = nullptr;
    HWND clip = nullptr;
    HWND footer = nullptr;
    SIZE contentSize{};
    int headerHeight = 0;
    int footerReferenceWidth = 1;
    int footerHeight = 0;
    int offset = 0;
    int thumbGrab = 0;
    bool dragging = false;
    bool routingHeader = false;
    bool headerHovered = false;
    bool headerPressed = false;

    static SidePanelViewport* State(HWND host) {
        return host ? static_cast<SidePanelViewport*>(GetPropW(host, kStateProperty)) : nullptr;
    }

    static void Fill(HDC dc, const RECT& rect, COLORREF color) {
        SetDCBrushColor(dc, color);
        FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    }

    LONG VisibleHeaderHeight(const RECT& bounds) const {
        return (std::min)(static_cast<LONG>(headerHeight), (std::max)(0L, bounds.bottom - 1));
    }

    int MaxOffset(const RECT& bounds) const {
        return (std::max)(0L, contentSize.cy - headerHeight - (bounds.bottom - VisibleHeaderHeight(bounds)));
    }

    bool InHeader(const RECT& bounds, POINT point) const {
        return point.x >= 0 && point.x < contentSize.cx &&
            point.y >= 0 && point.y < VisibleHeaderHeight(bounds);
    }

    RECT Track(const RECT& bounds) const {
        const LONG header = VisibleHeaderHeight(bounds);
        const LONG top = (std::min)(header + 3, bounds.bottom - 1);
        const LONG left = contentSize.cx + (kGutter - kTrackWidth) / 2;
        return {left, top, left + kTrackWidth, (std::max)(top + 1, bounds.bottom - 3)};
    }

    RECT Thumb(const RECT& bounds, const RECT& track) const {
        const int trackHeight = track.bottom - track.top;
        const int thumbHeight = (std::clamp)(MulDiv(trackHeight,
            bounds.bottom - VisibleHeaderHeight(bounds), contentSize.cy - headerHeight),
            (std::min)(14, trackHeight), trackHeight);
        const LONG top = track.top + MulDiv(offset, trackHeight - thumbHeight, MaxOffset(bounds));
        return {track.left, top, track.right, top + thumbHeight};
    }

    void PlaceContent(HWND host, int target) {
        if (!panel) return;
        RECT bounds{}, current{};
        GetClientRect(host, &bounds);
        GetClientRect(panel, &current);
        offset = (std::clamp)(target, 0, MaxOffset(bounds));
        const UINT sizeFlags = current.right == contentSize.cx && current.bottom == contentSize.cy ? SWP_NOSIZE : 0;
        SetWindowPos(panel, nullptr, 0, -headerHeight - offset, contentSize.cx, contentSize.cy,
            SWP_NOZORDER | SWP_NOACTIVATE | sizeFlags);
        RECT gutter{contentSize.cx, VisibleHeaderHeight(bounds), bounds.right, bounds.bottom};
        InvalidateRect(host, &gutter, FALSE);
    }

    void Layout(HWND host) {
        if (!panel) return;
        RECT bounds{};
        GetClientRect(host, &bounds);
        const LONG header = VisibleHeaderHeight(bounds);
        SetWindowPos(clip, nullptr, 0, header, contentSize.cx, bounds.bottom - header,
            SWP_NOZORDER | SWP_NOACTIVATE);
        PlaceContent(host, offset);
        InvalidateRect(host, nullptr, FALSE);
    }

    int FooterPx(int logical) const { return MulDiv(logical, contentSize.cx, footerReferenceWidth); }

    void PositionFooter(HWND host) const {
        if (!footer) return;
        if (!IsWindowVisible(host)) {
            if (IsWindowVisible(footer)) ShowWindow(footer, SW_HIDE);
            return;
        }
        RECT viewport{};
        if (GetWindowRect(host, &viewport)) {
            SetWindowPos(footer, nullptr, viewport.left, viewport.bottom,
                viewport.right - viewport.left, FooterPx(footerHeight), SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
        }
    }

    LRESULT SendHeader(HWND host, UINT message, WPARAM wParam, LPARAM lParam) {
        const bool previous = std::exchange(routingHeader, true);
        const LRESULT result = SendMessageW(panel, message, wParam, lParam);
        if (State(host) == this) routingHeader = previous;
        return result;
    }

    void DragTo(HWND host, int y, const RECT& bounds, const RECT& track, const RECT& thumb) {
        const int travel = (track.bottom - track.top) - (thumb.bottom - thumb.top);
        if (travel <= 0) return;
        PlaceContent(host, MulDiv((std::clamp)(y - thumbGrab - static_cast<int>(track.top), 0, travel),
            MaxOffset(bounds), travel));
    }

    static LRESULT CALLBACK WindowProc(HWND host, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_NCCREATE) {
            auto* state = new (std::nothrow) SidePanelViewport;
            if (!state) return FALSE;
            if (!SetPropW(host, kStateProperty, state)) {
                delete state;
                return FALSE;
            }
            return TRUE;
        }
        auto* state = State(host);
        if (!state) return DefWindowProcW(host, message, wParam, lParam);
        switch (message) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(host, &paint);
            RECT bounds{};
            GetClientRect(host, &bounds);
            Fill(dc, bounds, RGB(16, 25, 39));
            if (state->MaxOffset(bounds)) {
                const RECT track = state->Track(bounds);
                DrawScrollbar(dc, track, state->Thumb(bounds, track), state->dragging);
            }
            if (state->panel && paint.rcPaint.top < state->VisibleHeaderHeight(bounds)) {
                const int savedDc = SaveDC(dc);
                if (savedDc) {
                    IntersectClipRect(dc, 0, 0, bounds.right, state->VisibleHeaderHeight(bounds));
                    SendMessageW(state->panel, PaintHeaderMessage, reinterpret_cast<WPARAM>(dc), bounds.right);
                    RestoreDC(dc, savedDc);
                }
            }
            EndPaint(host, &paint);
            return 0;
        }
        case WM_SIZE:
            state->Layout(host);
            return 0;
        case WM_WINDOWPOSCHANGED: {
            const LRESULT result = DefWindowProcW(host, message, wParam, lParam);
            if (State(host) == state) state->PositionFooter(host);
            return result;
        }
        case WM_NCHITTEST: {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT bounds{};
            ScreenToClient(host, &point);
            GetClientRect(host, &bounds);
            if (state->panel && state->InHeader(bounds, point)) {
                ClientToScreen(state->panel, &point);
                return SendMessageW(state->panel, message, wParam, MAKELPARAM(point.x, point.y));
            }
            break;
        }
        case WM_SETCURSOR: {
            RECT bounds{};
            POINT point{};
            GetClientRect(host, &bounds);
            GetCursorPos(&point);
            ScreenToClient(host, &point);
            if (LOWORD(lParam) == HTCLIENT && state->panel && !state->routingHeader &&
                state->InHeader(bounds, point) &&
                state->SendHeader(host, message, reinterpret_cast<WPARAM>(state->panel), lParam)) return TRUE;
            break;
        }
        case WM_SETFOCUS:
            if (state->panel) SetFocus(state->panel);
            return 0;
        case WM_MOUSEWHEEL:
        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE: {
            RECT bounds{};
            GetClientRect(host, &bounds);
            if (message == WM_MOUSEWHEEL) {
                if (state->MaxOffset(bounds)) {
                    state->PlaceContent(host, state->offset - MulDiv(GET_WHEEL_DELTA_WPARAM(wParam), 48, WHEEL_DELTA));
                }
                return 0;
            }
            const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const bool inHeader = state->panel && state->InHeader(bounds, point);
            if (!state->dragging && inHeader) {
                if (message == WM_LBUTTONDOWN) {
                    state->headerPressed = true;
                    SetCapture(host);
                } else if (!state->headerHovered) {
                    TRACKMOUSEEVENT track{sizeof(TRACKMOUSEEVENT), TME_LEAVE, host, 0};
                    state->headerHovered = TrackMouseEvent(&track) != FALSE;
                }
                return state->SendHeader(host, message, wParam, lParam);
            }
            if (state->headerHovered) {
                state->headerHovered = false;
                state->SendHeader(host, WM_MOUSELEAVE, 0, 0);
            }
            if (!state->MaxOffset(bounds)) return 0;
            const RECT track = state->Track(bounds);
            const RECT thumb = state->Thumb(bounds, track);
            if (message == WM_LBUTTONDOWN && point.x >= state->contentSize.cx &&
                point.y >= state->VisibleHeaderHeight(bounds)) {
                state->thumbGrab = point.y >= thumb.top && point.y < thumb.bottom
                    ? point.y - thumb.top : (thumb.bottom - thumb.top) / 2;
                state->dragging = true;
                SetCapture(host);
            }
            if (state->dragging) state->DragTo(host, point.y, bounds, track, thumb);
            return 0;
        }
        case WM_MOUSELEAVE:
            if (std::exchange(state->headerHovered, false)) state->SendHeader(host, message, wParam, lParam);
            return 0;
        case WM_LBUTTONUP:
            if (std::exchange(state->headerPressed, false)) {
                RECT bounds{};
                GetClientRect(host, &bounds);
                const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                if (state->InHeader(bounds, point)) {
                    state->SendHeader(host, message, wParam, lParam);
                } else {
                    SendMessageW(state->panel, WM_CAPTURECHANGED, 0, 0);
                }
            }
            if (GetCapture() == host) ReleaseCapture();
            return 0;
        case WM_CAPTURECHANGED: {
            if (std::exchange(state->dragging, false)) {
                RECT gutter{};
                GetClientRect(host, &gutter);
                gutter.left = state->contentSize.cx;
                gutter.top = state->VisibleHeaderHeight(gutter);
                InvalidateRect(host, &gutter, FALSE);
            }
            if (std::exchange(state->headerPressed, false) && !state->routingHeader && state->panel) {
                SendMessageW(state->panel, message, wParam, lParam);
            }
            return 0;
        }
        case WM_MOVING:
            if (state->panel) SendMessageW(state->panel, message, wParam, lParam);
            return TRUE;
        case WM_CLOSE:
            if (state->panel) SendMessageW(state->panel, message, wParam, lParam);
            return 0;
        case WM_NCDESTROY:
            RemovePropW(host, kStateProperty);
            delete state;
            break;
        }
        return DefWindowProcW(host, message, wParam, lParam);
    }
};

}  // namespace vtsfloat
