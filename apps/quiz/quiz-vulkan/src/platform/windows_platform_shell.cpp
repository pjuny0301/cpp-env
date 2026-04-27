#include "platform/platform_shell.h"

#include <windows.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

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

LRESULT CALLBACK window_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
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

class windows_platform_shell final : public platform_shell {
public:
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

    [[nodiscard]] platform_shell_state state() const override
    {
        platform_shell_state current_state = state_;
        current_state.client_size = client_size_for_window(window_handle_, state_.client_size);
        return current_state;
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
    HINSTANCE instance_handle_ = nullptr;
    HWND window_handle_ = nullptr;
    std::string window_title_;
    platform_shell_state state_;
};

} // namespace

std::unique_ptr<platform_shell> create_platform_shell()
{
    return std::make_unique<windows_platform_shell>();
}

} // namespace quiz_vulkan
