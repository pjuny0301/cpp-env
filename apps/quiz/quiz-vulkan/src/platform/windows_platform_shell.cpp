#include "platform/platform_shell.h"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

std::string narrow_utf16(std::wstring_view value)
{
    if (value.empty()) {
        return {};
    }

    const int narrow_length = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (narrow_length <= 0) {
        return {};
    }

    std::string narrow(static_cast<std::size_t>(narrow_length), '\0');
    const int converted_length = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        narrow.data(),
        narrow_length,
        nullptr,
        nullptr);
    if (converted_length <= 0) {
        return {};
    }

    narrow.resize(static_cast<std::size_t>(converted_length));
    return narrow;
}

bool is_high_surrogate(wchar_t value)
{
    return value >= 0xD800 && value <= 0xDBFF;
}

bool is_low_surrogate(wchar_t value)
{
    return value >= 0xDC00 && value <= 0xDFFF;
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

float client_x_from_lparam(LPARAM l_param)
{
    return static_cast<float>(static_cast<short>(LOWORD(l_param)));
}

float client_y_from_lparam(LPARAM l_param)
{
    return static_cast<float>(static_cast<short>(HIWORD(l_param)));
}

POINT client_point_from_screen_lparam(HWND window_handle, LPARAM l_param)
{
    POINT point{
        static_cast<LONG>(static_cast<short>(LOWORD(l_param))),
        static_cast<LONG>(static_cast<short>(HIWORD(l_param))),
    };
    if (window_handle != nullptr) {
        ScreenToClient(window_handle, &point);
    }
    return point;
}

bool virtual_key_down(int virtual_key)
{
    return (GetKeyState(virtual_key) & 0x8000) != 0;
}

std::string logical_key_for_virtual_key(WPARAM virtual_key)
{
    switch (virtual_key) {
    case VK_TAB:
        return "Tab";
    case VK_ESCAPE:
        return "Escape";
    case VK_LEFT:
        return "ArrowLeft";
    case VK_RIGHT:
        return "ArrowRight";
    case VK_HOME:
        return "Home";
    case VK_END:
        return "End";
    case VK_DELETE:
        return "Delete";
    case 0x41:
        return "KeyA";
    default:
        return {};
    }
}

class windows_platform_shell final : public platform_shell {
public:
    LRESULT handle_message(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
    {
        switch (message) {
        case WM_LBUTTONDOWN:
            left_button_down_ = true;
            SetCapture(window_handle);
            push_pointer_event(
                platform_input_event_type::pointer_down,
                client_x_from_lparam(l_param),
                client_y_from_lparam(l_param));
            return 0;
        case WM_MOUSEMOVE:
            if (left_button_down_ || (w_param & MK_LBUTTON) != 0) {
                push_pointer_event(
                    platform_input_event_type::pointer_move,
                    client_x_from_lparam(l_param),
                    client_y_from_lparam(l_param));
                return 0;
            }
            return DefWindowProcW(window_handle, message, w_param, l_param);
        case WM_LBUTTONUP: {
            left_button_down_ = false;
            push_pointer_event(
                platform_input_event_type::pointer_up,
                client_x_from_lparam(l_param),
                client_y_from_lparam(l_param));
            if (GetCapture() == window_handle) {
                ReleaseCapture();
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            const POINT point = client_point_from_screen_lparam(window_handle, l_param);
            input_events_.push_back(platform_input_event{
                .type = platform_input_event_type::mouse_wheel,
                .x = static_cast<float>(point.x),
                .y = static_cast<float>(point.y),
                .text = {},
                .delta_y = static_cast<float>(GET_WHEEL_DELTA_WPARAM(w_param)) / static_cast<float>(WHEEL_DELTA),
                .scroll_unit = raw_platform_scroll_delta_unit::lines,
                .logical_key = {},
                .alt = virtual_key_down(VK_MENU),
                .ctrl = virtual_key_down(VK_CONTROL),
                .shift = virtual_key_down(VK_SHIFT),
                .meta = virtual_key_down(VK_LWIN) || virtual_key_down(VK_RWIN),
            });
            return 0;
        }
        case WM_CAPTURECHANGED:
            if (left_button_down_ && reinterpret_cast<HWND>(l_param) != window_handle) {
                left_button_down_ = false;
                push_pointer_event(platform_input_event_type::pointer_cancel, 0.0f, 0.0f);
            }
            return DefWindowProcW(window_handle, message, w_param, l_param);
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (handle_key_input(platform_input_event_type::key_down, w_param, l_param)) {
                return 0;
            }
            return DefWindowProcW(window_handle, message, w_param, l_param);
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (handle_key_input(platform_input_event_type::key_up, w_param, l_param)) {
                return 0;
            }
            return DefWindowProcW(window_handle, message, w_param, l_param);
        case WM_CHAR:
            handle_character_input(static_cast<wchar_t>(w_param));
            return 0;
        case WM_SETFOCUS:
            input_events_.push_back(platform_input_event{
                .type = platform_input_event_type::focus_gained,
                .text = {},
                .logical_key = {},
            });
            return 0;
        case WM_KILLFOCUS:
            input_events_.push_back(platform_input_event{
                .type = platform_input_event_type::focus_lost,
                .text = {},
                .logical_key = {},
            });
            if (left_button_down_) {
                left_button_down_ = false;
                push_pointer_event(platform_input_event_type::pointer_cancel, 0.0f, 0.0f);
            }
            return 0;
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
        state_.native_window = native_window_handle();
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
        current_state.native_window = native_window_handle();
        return current_state;
    }

    [[nodiscard]] platform_native_window_handle native_window_handle() const override
    {
        return platform_native_window_handle{
            .kind = window_handle_ == nullptr
                ? platform_native_window_kind::none
                : platform_native_window_kind::win32_hwnd,
            .value = reinterpret_cast<std::uintptr_t>(window_handle_),
            .display = reinterpret_cast<std::uintptr_t>(instance_handle_),
        };
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
    void push_pointer_event(platform_input_event_type type, float x, float y)
    {
        input_events_.push_back(platform_input_event{
            .type = type,
            .x = x,
            .y = y,
            .text = {},
            .pointer_id = 0,
            .pointer_button = raw_platform_pointer_button::primary,
            .logical_key = {},
        });
    }

    bool handle_key_input(platform_input_event_type type, WPARAM virtual_key, LPARAM l_param)
    {
        std::string logical_key = logical_key_for_virtual_key(virtual_key);
        if (logical_key.empty()) {
            return false;
        }

        input_events_.push_back(platform_input_event{
            .type = type,
            .text = {},
            .key_code = static_cast<std::int32_t>(virtual_key),
            .logical_key = std::move(logical_key),
            .alt = virtual_key_down(VK_MENU),
            .ctrl = virtual_key_down(VK_CONTROL),
            .shift = virtual_key_down(VK_SHIFT),
            .meta = virtual_key_down(VK_LWIN) || virtual_key_down(VK_RWIN),
            .repeat = type == platform_input_event_type::key_down && (l_param & (1L << 30)) != 0,
        });
        return true;
    }

    void push_text_event(std::wstring_view text)
    {
        std::string utf8_text = narrow_utf16(text);
        if (!utf8_text.empty()) {
            input_events_.push_back(platform_input_event{
                .type = platform_input_event_type::text_input,
                .text = std::move(utf8_text),
                .logical_key = {},
            });
        }
    }

    void handle_character_input(wchar_t code_unit)
    {
        if (code_unit == L'\b') {
            pending_high_surrogate_ = 0;
            input_events_.push_back(platform_input_event{
                .type = platform_input_event_type::text_backspace,
                .text = {},
                .logical_key = {},
            });
            return;
        }

        if (code_unit == L'\r' || code_unit == L'\n') {
            pending_high_surrogate_ = 0;
            input_events_.push_back(platform_input_event{
                .type = platform_input_event_type::text_submit,
                .text = {},
                .logical_key = {},
            });
            return;
        }

        if (code_unit < L' ' || code_unit == 0x7F) {
            return;
        }

        if (is_high_surrogate(code_unit)) {
            pending_high_surrogate_ = code_unit;
            return;
        }

        if (is_low_surrogate(code_unit)) {
            if (pending_high_surrogate_ != 0) {
                const wchar_t surrogate_pair[] = { pending_high_surrogate_, code_unit };
                pending_high_surrogate_ = 0;
                push_text_event(std::wstring_view(surrogate_pair, 2));
            }
            return;
        }

        pending_high_surrogate_ = 0;
        push_text_event(std::wstring_view(&code_unit, 1));
    }

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
    wchar_t pending_high_surrogate_ = 0;
    bool left_button_down_ = false;
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
