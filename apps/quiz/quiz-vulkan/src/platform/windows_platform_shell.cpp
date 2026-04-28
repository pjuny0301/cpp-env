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

std::wstring widen_utf8(std::string_view value)
{
    if (value.empty()) {
        return {};
    }

    const int wide_length = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (wide_length <= 0) {
        return widen_ascii(value);
    }

    std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
    const int converted_length = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        wide.data(),
        wide_length);
    if (converted_length <= 0) {
        return widen_ascii(value);
    }

    wide.resize(static_cast<std::size_t>(converted_length));
    return wide;
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

int scale_framebuffer_coordinate(float value, int client_extent, std::size_t framebuffer_extent)
{
    if (client_extent <= 0 || framebuffer_extent == 0) {
        return 0;
    }

    const float scaled = value * static_cast<float>(client_extent) / static_cast<float>(framebuffer_extent);
    return std::clamp(static_cast<int>(scaled + 0.5f), 0, client_extent);
}

COLORREF text_color_ref(const platform_text_overlay& overlay)
{
    return RGB(overlay.red, overlay.green, overlay.blue);
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
        switch (message) {
        case WM_LBUTTONUP: {
            const auto x = static_cast<float>(static_cast<short>(LOWORD(l_param)));
            const auto y = static_cast<float>(static_cast<short>(HIWORD(l_param)));
            input_events_.push_back(platform_input_event{platform_input_event_type::pointer_press, x, y, {}});
            return 0;
        }
        case WM_CHAR:
            handle_character_input(static_cast<wchar_t>(w_param));
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
        present_frame(width, height, rgba, {});
    }

    void present_frame(
        std::size_t width,
        std::size_t height,
        const unsigned char* rgba,
        const std::vector<platform_text_overlay>& text_overlays) override
    {
        framebuffer_width_ = width;
        framebuffer_height_ = height;
        framebuffer_bgra_.clear();
        text_overlays_ = text_overlays;

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
    void push_text_event(std::wstring_view text)
    {
        std::string utf8_text = narrow_utf16(text);
        if (!utf8_text.empty()) {
            input_events_.push_back(
                platform_input_event{platform_input_event_type::text_input, 0.0f, 0.0f, std::move(utf8_text)});
        }
    }

    void handle_character_input(wchar_t code_unit)
    {
        if (code_unit == L'\b') {
            pending_high_surrogate_ = 0;
            input_events_.push_back(platform_input_event{platform_input_event_type::text_backspace, 0.0f, 0.0f, {}});
            return;
        }

        if (code_unit == L'\r' || code_unit == L'\n') {
            pending_high_surrogate_ = 0;
            input_events_.push_back(platform_input_event{platform_input_event_type::text_submit, 0.0f, 0.0f, {}});
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

        paint_text_overlays(device_context, client_size);

        EndPaint(window_handle, &paint);
    }

    void paint_text_overlays(HDC device_context, platform_client_size client_size)
    {
        if (text_overlays_.empty() || framebuffer_width_ == 0 || framebuffer_height_ == 0
            || client_size.width <= 0 || client_size.height <= 0) {
            return;
        }

        const int previous_background_mode = SetBkMode(device_context, TRANSPARENT);
        const COLORREF previous_text_color = GetTextColor(device_context);

        for (const platform_text_overlay& overlay : text_overlays_) {
            if (overlay.text.empty() || overlay.alpha == 0 || overlay.width <= 0.0f || overlay.height <= 0.0f) {
                continue;
            }

            RECT text_rect{
                scale_framebuffer_coordinate(overlay.x, client_size.width, framebuffer_width_),
                scale_framebuffer_coordinate(overlay.y, client_size.height, framebuffer_height_),
                scale_framebuffer_coordinate(overlay.x + overlay.width, client_size.width, framebuffer_width_),
                scale_framebuffer_coordinate(overlay.y + overlay.height, client_size.height, framebuffer_height_),
            };
            if (text_rect.right <= text_rect.left || text_rect.bottom <= text_rect.top) {
                continue;
            }

            const int rect_height = std::max(static_cast<int>(text_rect.bottom - text_rect.top), 1);
            const int font_height = std::clamp(static_cast<int>(static_cast<float>(rect_height) * 0.82f), 13, 28);
            HFONT font = CreateFontW(
                -font_height,
                0,
                0,
                0,
                FW_SEMIBOLD,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                L"Malgun Gothic");
            HGDIOBJ previous_font = nullptr;
            if (font != nullptr) {
                previous_font = SelectObject(device_context, font);
            }

            const std::wstring text = widen_utf8(overlay.text);
            SetTextColor(device_context, text_color_ref(overlay));
            DrawTextW(
                device_context,
                text.c_str(),
                -1,
                &text_rect,
                DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

            if (font != nullptr) {
                if (previous_font != nullptr) {
                    SelectObject(device_context, previous_font);
                }
                DeleteObject(font);
            }
        }

        SetTextColor(device_context, previous_text_color);
        if (previous_background_mode != 0) {
            SetBkMode(device_context, previous_background_mode);
        }
    }

    HINSTANCE instance_handle_ = nullptr;
    HWND window_handle_ = nullptr;
    std::string window_title_;
    platform_shell_state state_;
    std::vector<platform_input_event> input_events_;
    std::size_t framebuffer_width_ = 0;
    std::size_t framebuffer_height_ = 0;
    std::vector<unsigned char> framebuffer_bgra_;
    std::vector<platform_text_overlay> text_overlays_;
    wchar_t pending_high_surrogate_ = 0;
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
