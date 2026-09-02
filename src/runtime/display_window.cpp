#include "display_window.h"
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

namespace emulator::runtime {

#if defined(_WIN32) || defined(_WIN64)
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    DisplayWindow* window = reinterpret_cast<DisplayWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_LBUTTONDOWN: {
            if (window) {
                int x = LOWORD(lparam);
                int y = HIWORD(lparam);
                auto events = window->GetMouseTranslator().OnMouseDown(x, y);
                auto pt = window->GetMouseTranslator().MapWindowToTouch(x, y);
                if (pt.is_valid) {
                    window->SetActiveTouch(pt.x, pt.y, true);
                }
                window->DispatchEvents(events);
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (window && (wparam & MK_LBUTTON)) {
                int x = LOWORD(lparam);
                int y = HIWORD(lparam);
                auto events = window->GetMouseTranslator().OnMouseMove(x, y);
                auto pt = window->GetMouseTranslator().MapWindowToTouch(x, y);
                if (pt.is_valid) {
                    window->SetActiveTouch(pt.x, pt.y, true);
                }
                window->DispatchEvents(events);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (window) {
                auto events = window->GetMouseTranslator().OnMouseUp();
                window->SetActiveTouch(-1, -1, false);
                window->DispatchEvents(events);
            }
            return 0;
        }
        case WM_KEYDOWN: {
            if (window) {
                uint32_t vk = static_cast<uint32_t>(wparam);
                if (vk == 0x76 || vk == 0x7B) { // F7 or F12 -> Power button
                    window->ToggleScreen();
                }
                auto events = window->GetKeyboardTranslator().OnKeyDown(vk);
                window->DispatchEvents(events);
            }
            return 0;
        }
        case WM_KEYUP: {
            if (window) {
                uint32_t vk = static_cast<uint32_t>(wparam);
                auto events = window->GetKeyboardTranslator().OnKeyUp(vk);
                window->DispatchEvents(events);
            }
            return 0;
        }
        case WM_SIZE: {
            if (window) {
                UINT width = LOWORD(lparam);
                UINT height = HIWORD(lparam);
                window->GetMouseTranslator().SetWindowViewport(width, height);
            }
            return 0;
        }
        case WM_DESTROY:
        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}
#endif

DisplayWindow::DisplayWindow(const std::string& title, uint32_t width, uint32_t height)
    : title_(title), window_width_(width), window_height_(height),
      mouse_translator_(1080, 2400) {
    mouse_translator_.SetWindowViewport(width, height);
}

DisplayWindow::~DisplayWindow() {
    Destroy();
}

void DisplayWindow::DispatchEvents(const std::vector<emulator::input::InputEventRecord>& events) {
    if (input_callback_ && !events.empty()) {
        input_callback_(events);
    }
}

bool DisplayWindow::Create() {
#if defined(_WIN32) || defined(_WIN64)
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "AndroidEmulatorWindowClass";

    RegisterClassEx(&wc);

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int pos_x = (screen_w > static_cast<int>(window_width_)) ? (screen_w - window_width_) / 2 : 50;
    int pos_y = (screen_h > static_cast<int>(window_height_)) ? (screen_h - window_height_) / 2 : 40;

    HWND hwnd = CreateWindowEx(
        WS_EX_APPWINDOW | WS_EX_TOPMOST,
        wc.lpszClassName,
        title_.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        pos_x, pos_y,
        window_width_, window_height_,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!hwnd) {
        is_open_ = false;
        return false;
    }

    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    window_handle_ = hwnd;
    device_context_ = GetDC(hwnd);
    is_open_ = true;

    // Bring window to absolute foreground on user's active desktop
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetWindowPos(hwnd, HWND_TOPMOST, pos_x, pos_y, window_width_, window_height_, SWP_SHOWWINDOW);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    UpdateWindow(hwnd);
    return true;

#else
    is_open_ = true;
    return true;
#endif
}

void DisplayWindow::Destroy() {
    if (!is_open_) return;

#if defined(_WIN32) || defined(_WIN64)
    if (window_handle_) {
        if (device_context_) {
            ReleaseDC(static_cast<HWND>(window_handle_), static_cast<HDC>(device_context_));
            device_context_ = nullptr;
        }
        DestroyWindow(static_cast<HWND>(window_handle_));
        window_handle_ = nullptr;
    }
#endif
    is_open_ = false;
}

bool DisplayWindow::ProcessEvents() {
    if (!is_open_) return false;

#if defined(_WIN32) || defined(_WIN64)
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            is_open_ = false;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
#else
    return true;
#endif
}

void DisplayWindow::RenderFrame(const FramebufferSurface& surface) {
    if (!is_open_) return;

#if defined(_WIN32) || defined(_WIN64)
    if (!device_context_ || !window_handle_) return;

    HWND hwnd = static_cast<HWND>(window_handle_);
    HDC hdc = static_cast<HDC>(device_context_);

    RECT rc;
    GetClientRect(hwnd, &rc);
    int win_w = rc.right - rc.left;
    int win_h = rc.bottom - rc.top;

    if (!is_screen_on_) {
        // Screen off: Black display
        HBRUSH black_brush = (HBRUSH)GetStockObject(BLACK_BRUSH);
        FillRect(hdc, &rc, black_brush);
        return;
    }

    auto buffer = surface.GetBackBuffer();
    if (buffer.empty()) return;

    uint64_t frame_id = surface.GetBackBufferFrameId();
    std::string sha = surface.GetBackBufferSha256();
    if (frame_id > 0 && !sha.empty()) {
        static uint64_t last_logged_display_frame = 0;
        if (frame_id != last_logged_display_frame) {
            last_logged_display_frame = frame_id;
            std::cout << "[DISPLAY] frame=" << frame_id << " sha256=" << sha << "\n" << std::flush;
        }
    }

    // Draw active touch ripple circle directly into frame buffer if active
    if (active_touch_.active && active_touch_.x >= 0 && active_touch_.y >= 0) {
        int cx = active_touch_.x;
        int cy = active_touch_.y;
        int r = active_touch_.ripple_radius;
        int w = surface.GetWidth();
        int h = surface.GetHeight();

        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy <= r * r) {
                    int px = cx + dx;
                    int py = cy + dy;
                    if (px >= 0 && px < w && py >= 0 && py < h) {
                        size_t idx = (py * w + px) * 4;
                        // Alpha blend white/cyan glow
                        buffer[idx + 0] = std::min(255, buffer[idx + 0] + 120); // B
                        buffer[idx + 1] = std::min(255, buffer[idx + 1] + 180); // G
                        buffer[idx + 2] = std::min(255, buffer[idx + 2] + 200); // R
                    }
                }
            }
        }
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = surface.GetWidth();
    bmi.bmiHeader.biHeight = -static_cast<int32_t>(surface.GetHeight()); // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(
        hdc,
        0, 0, win_w, win_h,
        0, 0, surface.GetWidth(), surface.GetHeight(),
        buffer.data(),
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY
    );
#endif
}

} // namespace emulator::runtime
