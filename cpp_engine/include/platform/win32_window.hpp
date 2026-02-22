#pragma once

#ifdef _WIN32

#define NOMINMAX
#include <Windows.h>

#include <functional>
#include <string>

#include "engine/types.hpp"

namespace svl {

class Win32Window {
public:
    using EventCallback = std::function<void(const InputEvent&)>;

    Win32Window() = default;
    ~Win32Window();

    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    bool create(
        HINSTANCE instance,
        const char* title,
        int width,
        int height,
        EventCallback callback);

    bool pump_events();
    void swap_buffers() const;
    void set_title(const std::string& title) const;
    void request_close();

    bool should_close() const { return should_close_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    static LRESULT CALLBACK static_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT wnd_proc(UINT msg, WPARAM wparam, LPARAM lparam);

    void emit_event(const InputEvent& event) const;
    void cleanup();

    HWND hwnd_ = nullptr;
    HDC hdc_ = nullptr;
    HGLRC hglrc_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool should_close_ = false;
    EventCallback event_callback_{};
};

}  // namespace svl

#endif  // _WIN32

