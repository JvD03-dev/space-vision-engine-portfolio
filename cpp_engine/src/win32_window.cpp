#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>

#include "platform/win32_window.hpp"

#ifdef _WIN32

namespace svl {

namespace {

constexpr const char* kWindowClassName = "SpaceVisionEngineWindowClass";

MouseButton to_mouse_button(UINT msg) {
    switch (msg) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            return MouseButton::Left;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            return MouseButton::Middle;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            return MouseButton::Right;
        default:
            return MouseButton::Unknown;
    }
}

}  // namespace

Win32Window::~Win32Window() {
    cleanup();
}

bool Win32Window::create(
    HINSTANCE instance,
    const char* title,
    int width,
    int height,
    EventCallback callback) {
    event_callback_ = std::move(callback);
    width_ = width;
    height_ = height;

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = &Win32Window::static_wnd_proc;
    wc.hInstance = instance;
    wc.lpszClassName = kWindowClassName;

    const ATOM klass = RegisterClassExA(&wc);
    if (klass == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExA(
        0,
        kWindowClassName,
        title,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
        nullptr,
        nullptr,
        instance,
        this);
    if (hwnd_ == nullptr) {
        cleanup();
        return false;
    }

    hdc_ = GetDC(hwnd_);
    if (hdc_ == nullptr) {
        cleanup();
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    const int pixel_format = ChoosePixelFormat(hdc_, &pfd);
    if (pixel_format == 0 || !SetPixelFormat(hdc_, pixel_format, &pfd)) {
        cleanup();
        return false;
    }

    hglrc_ = wglCreateContext(hdc_);
    if (hglrc_ == nullptr || !wglMakeCurrent(hdc_, hglrc_)) {
        cleanup();
        return false;
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    should_close_ = false;

    emit_event(InputEvent{
        InputEventType::Resize,
        0,
        MouseButton::Unknown,
        0,
        0,
        0,
        width_,
        height_,
    });

    return true;
}

bool Win32Window::pump_events() {
    MSG msg{};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            should_close_ = true;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return !should_close_;
}

void Win32Window::swap_buffers() const {
    if (hdc_ != nullptr) {
        SwapBuffers(hdc_);
    }
}

void Win32Window::set_title(const std::string& title) const {
    if (hwnd_ != nullptr) {
        SetWindowTextA(hwnd_, title.c_str());
    }
}

void Win32Window::request_close() {
    should_close_ = true;
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
    }
}

LRESULT CALLBACK Win32Window::static_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    Win32Window* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCT*>(lparam);
        self = reinterpret_cast<Win32Window*>(create->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self != nullptr) {
            self->hwnd_ = hwnd;
        }
    } else {
        self = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (self != nullptr) {
        return self->wnd_proc(msg, wparam, lparam);
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

LRESULT Win32Window::wnd_proc(UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_SIZE: {
            width_ = LOWORD(lparam);
            height_ = HIWORD(lparam);
            emit_event(InputEvent{
                InputEventType::Resize,
                0,
                MouseButton::Unknown,
                0,
                0,
                0,
                width_,
                height_,
            });
            return 0;
        }
        case WM_CLOSE:
            emit_event(InputEvent{InputEventType::CloseRequested});
            should_close_ = true;
            DestroyWindow(hwnd_);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            emit_event(InputEvent{
                InputEventType::KeyDown,
                static_cast<int>(wparam),
                MouseButton::Unknown,
                0,
                0,
                0,
                0,
                0,
                (lparam & (1 << 30)) != 0,
            });
            return 0;
        case WM_KEYUP:
            emit_event(InputEvent{
                InputEventType::KeyUp,
                static_cast<int>(wparam),
            });
            return 0;
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
            emit_event(InputEvent{
                InputEventType::MouseButtonDown,
                0,
                to_mouse_button(msg),
                GET_X_LPARAM(lparam),
                GET_Y_LPARAM(lparam),
            });
            if (msg == WM_MBUTTONDOWN) {
                SetCapture(hwnd_);
            }
            return 0;
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            emit_event(InputEvent{
                InputEventType::MouseButtonUp,
                0,
                to_mouse_button(msg),
                GET_X_LPARAM(lparam),
                GET_Y_LPARAM(lparam),
            });
            if (msg == WM_MBUTTONUP) {
                ReleaseCapture();
            }
            return 0;
        case WM_MOUSEMOVE:
            emit_event(InputEvent{
                InputEventType::MouseMove,
                0,
                MouseButton::Unknown,
                GET_X_LPARAM(lparam),
                GET_Y_LPARAM(lparam),
            });
            return 0;
        case WM_MOUSEWHEEL:
            emit_event(InputEvent{
                InputEventType::MouseWheel,
                0,
                MouseButton::Unknown,
                GET_X_LPARAM(lparam),
                GET_Y_LPARAM(lparam),
                GET_WHEEL_DELTA_WPARAM(wparam),
            });
            return 0;
        default:
            return DefWindowProc(hwnd_, msg, wparam, lparam);
    }
}

void Win32Window::emit_event(const InputEvent& event) const {
    if (event_callback_) {
        event_callback_(event);
    }
}

void Win32Window::cleanup() {
    if (hglrc_ != nullptr) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(hglrc_);
        hglrc_ = nullptr;
    }
    if (hdc_ != nullptr && hwnd_ != nullptr) {
        ReleaseDC(hwnd_, hdc_);
        hdc_ = nullptr;
    }
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

}  // namespace svl

#endif  // _WIN32
