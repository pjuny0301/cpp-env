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
        const int client_width = std::max(config.width, 1);
        const int client_height = std::max(config.height, 1);
        RECT window_rect{ 0, 0, client_width, client_height };
        if (AdjustWindowRect(&window_rect, window_style, FALSE) == 0) {
            return false;
        }

        const int window_width = window_rect.right - window_rect.left;
        const int window_height = window_rect.bottom - window_rect.top;
        const std::wstring window_title = widen_ascii(config.title);

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

    void show_message(std::string_view message) override
    {
        std::cout << message << '\n';
        OutputDebugStringA(std::string(message).append("\n").c_str());
    }

private:
    HINSTANCE instance_handle_ = nullptr;
    HWND window_handle_ = nullptr;
};

} // namespace

std::unique_ptr<platform_shell> create_platform_shell()
{
    return std::make_unique<windows_platform_shell>();
}

} // namespace quiz_vulkan
