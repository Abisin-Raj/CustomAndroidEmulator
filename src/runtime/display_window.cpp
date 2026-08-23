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
                // Dispatch if callback registered
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (window && (wparam & MK_LBUTTON)) {
                int x = LOWORD(lparam);
                int y = HIWORD(lparam);
                auto events = window->GetMouseTranslator().OnMouseMove(x, y);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (window) {
                auto events = window->GetMouseTranslator().OnMouseUp();
            }
            return 0;
        }
        case WM_KEYDOWN: {
            if (window) {
                auto events = window->GetKeyboardTranslator().OnKeyDown(static_cast<uint32_t>(wparam));
            }
            return 0;
        }
        case WM_KEYUP: {
            if (window) {
                auto events = window->GetKeyboardTranslator().OnKeyUp(static_cast<uint32_t>(wparam));
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

bool DisplayWindow::Create() {
#if defined(_WIN32) || defined(_WIN64)
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "AndroidEmulatorWindowClass";

    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        title_.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
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

    auto buffer = surface.GetBackBuffer();
    if (buffer.empty()) return;

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
