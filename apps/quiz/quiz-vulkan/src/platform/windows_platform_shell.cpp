#include "platform/platform_shell.h"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan {
namespace {

constexpr wchar_t window_class_name[] = L"quiz_vulkan_window";

std::wstring widen_ascii(std::string_view value)
{
    return std::wstring(value.begin(), value.end());
}

platform_client_size configured_client_size(const platform_shell_config& config)
{
    return {
        std::max(config.width, 1),
        std::max(config.height, 1),
    };
}

LRESULT CALLBACK window_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param);

platform_client_size client_size_for_window(HWND window_handle, platform_client_size fallback)
{
    if (window_handle == nullptr) {
        return fallback;
    }

    RECT client_rect{};
    if (GetClientRect(window_handle, &client_rect) == 0) {
        return fallback;
    }

    return {
        std::max(static_cast<int>(client_rect.right - client_rect.left), 0),
        std::max(static_cast<int>(client_rect.bottom - client_rect.top), 0),
    };
}

class windows_platform_shell final : public platform_shell {
public:
    LRESULT handle_message(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
    {
        (void)w_param;

        switch (message) {
        case WM_LBUTTONUP: {
            const auto x = static_cast<float>(static_cast<short>(LOWORD(l_param)));
            const auto y = static_cast<float>(static_cast<short>(HIWORD(l_param)));
            input_events_.push_back(platform_input_event{platform_input_event_type::pointer_press, x, y});
            return 0;
        }
        case WM_PAINT:
            paint_framebuffer(window_handle);
            return 0;
        case WM_SIZE:
            state_.client_size = client_size_for_window(window_handle_, state_.client_size);
            InvalidateRect(window_handle_, nullptr, FALSE);
            return 0;
        case WM_CLOSE:
            DestroyWindow(window_handle);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window_handle, message, w_param, l_param);
        }
    }

    bool create(const platform_shell_config& config) override
    {
        instance_handle_ = GetModuleHandleW(nullptr);
        window_title_ = config.title;
        state_.client_size = configured_client_size(config);

        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.lpfnWndProc = window_proc;
        window_class.hInstance = instance_handle_;
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.lpszClassName = window_class_name;

        if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }

        const DWORD window_style = WS_OVERLAPPEDWINDOW;
        RECT window_rect{ 0, 0, state_.client_size.width, state_.client_size.height };
        if (AdjustWindowRect(&window_rect, window_style, FALSE) == 0) {
            return false;
        }

        const int window_width = window_rect.right - window_rect.left;
        const int window_height = window_rect.bottom - window_rect.top;
        const std::wstring window_title = widen_ascii(window_title_);

        window_handle_ = CreateWindowExW(
            0,
            window_class_name,
            window_title.c_str(),
            window_style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            window_width,
            window_height,
            nullptr,
            nullptr,
            instance_handle_,
            nullptr);

        if (window_handle_ == nullptr) {
            return false;
        }

        SetWindowLongPtrW(window_handle_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        state_.client_size = client_size_for_window(window_handle_, state_.client_size);
        ShowWindow(window_handle_, SW_SHOW);
        UpdateWindow(window_handle_);
        return true;
    }

    platform_shell_status pump_events() override
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
            if (message.message == WM_QUIT) {
                return platform_shell_status::exit_requested;
            }

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        return platform_shell_status::keep_running;
    }

    [[nodiscard]] std::vector<platform_input_event> drain_input_events() override
    {
        std::vector<platform_input_event> drained;
        drained.swap(input_events_);
        return drained;
    }

    [[nodiscard]] platform_shell_state state() const override
    {
        platform_shell_state current_state = state_;
        current_state.client_size = client_size_for_window(window_handle_, state_.client_size);
        return current_state;
    }

    void present_framebuffer(std::size_t width, std::size_t height, const unsigned char* rgba) override
    {
        framebuffer_width_ = width;
        framebuffer_height_ = height;
        framebuffer_bgra_.clear();

        if (rgba != nullptr && width > 0 && height > 0) {
            framebuffer_bgra_.resize(width * height * 4);
            for (std::size_t index = 0; index < width * height; ++index) {
                const std::size_t offset = index * 4;
                framebuffer_bgra_[offset] = rgba[offset + 2];
                framebuffer_bgra_[offset + 1] = rgba[offset + 1];
                framebuffer_bgra_[offset + 2] = rgba[offset];
                framebuffer_bgra_[offset + 3] = rgba[offset + 3];
            }
        }

        if (window_handle_ != nullptr) {
            InvalidateRect(window_handle_, nullptr, FALSE);
            UpdateWindow(window_handle_);
        }
    }

    void set_frame_status(std::string_view status) override
    {
        const std::string next_status(status);
        if (state_.frame_status == next_status) {
            return;
        }

        state_.frame_status = next_status;

        if (window_handle_ == nullptr) {
            return;
        }

        std::string title = window_title_;
        if (!state_.frame_status.empty()) {
            title.append(" - ").append(state_.frame_status);
        }

        SetWindowTextW(window_handle_, widen_ascii(title).c_str());
    }

    void show_message(std::string_view message) override
    {
        std::cout << message << '\n';
        OutputDebugStringA(std::string(message).append("\n").c_str());
    }

private:
    void paint_framebuffer(HWND window_handle)
    {
        PAINTSTRUCT paint{};
        HDC device_context = BeginPaint(window_handle, &paint);
        if (device_context == nullptr) {
            return;
        }

        const platform_client_size client_size = client_size_for_window(window_handle, state_.client_size);
        if (framebuffer_bgra_.empty() || framebuffer_width_ == 0 || framebuffer_height_ == 0) {
            RECT client_rect{0, 0, client_size.width, client_size.height};
            FillRect(device_context, &client_rect, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            EndPaint(window_handle, &paint);
            return;
        }

        BITMAPINFO bitmap_info{};
        bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap_info.bmiHeader.biWidth = static_cast<LONG>(framebuffer_width_);
        bitmap_info.bmiHeader.biHeight = -static_cast<LONG>(framebuffer_height_);
        bitmap_info.bmiHeader.biPlanes = 1;
        bitmap_info.bmiHeader.biBitCount = 32;
        bitmap_info.bmiHeader.biCompression = BI_RGB;

        StretchDIBits(
            device_context,
            0,
            0,
            client_size.width,
            client_size.height,
            0,
            0,
            static_cast<int>(framebuffer_width_),
            static_cast<int>(framebuffer_height_),
            framebuffer_bgra_.data(),
            &bitmap_info,
            DIB_RGB_COLORS,
            SRCCOPY);

        EndPaint(window_handle, &paint);
    }

    HINSTANCE instance_handle_ = nullptr;
    HWND window_handle_ = nullptr;
    std::string window_title_;
    platform_shell_state state_;
    std::vector<platform_input_event> input_events_;
    std::size_t framebuffer_width_ = 0;
    std::size_t framebuffer_height_ = 0;
    std::vector<unsigned char> framebuffer_bgra_;
};

LRESULT CALLBACK window_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
    auto* shell = reinterpret_cast<windows_platform_shell*>(GetWindowLongPtrW(window_handle, GWLP_USERDATA));
    if (shell != nullptr) {
        return shell->handle_message(window_handle, message, w_param, l_param);
    }

    switch (message) {
    case WM_CLOSE:
        DestroyWindow(window_handle);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window_handle, message, w_param, l_param);
    }
}

} // namespace

std::unique_ptr<platform_shell> create_platform_shell()
{
    return std::make_unique<windows_platform_shell>();
}

} // namespace quiz_vulkan
