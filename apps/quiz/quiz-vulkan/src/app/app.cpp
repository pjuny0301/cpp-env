#include "app/app.h"

#include "app/app_demo.h"
#include "app/app_state.h"

#include <chrono>
#include <string>
#include <iostream>
#include <thread>
#include <utility>

namespace quiz_vulkan {
namespace {

scene::scene_rect viewport_for_shell(const platform_shell_state& state, const platform_shell_config& fallback)
{
    const int width = state.client_size.width > 0 ? state.client_size.width : fallback.width;
    const int height = state.client_size.height > 0 ? state.client_size.height : fallback.height;
    return scene::scene_rect{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
}

app_render_report render_and_report(
    platform_shell& shell,
    const platform_shell_config& shell_config,
    std::string_view label,
    const domain::app_snapshot& snapshot)
{
    const app_render_report report = render_app_snapshot(snapshot, viewport_for_shell(shell.state(), shell_config));
    const std::string line = format_render_report(label, report);
    shell.show_message(line);
    shell.set_frame_status(line);
    return report;
}

} // namespace

app::app(app_config config)
    : config_(std::move(config))
{
}

int app::run()
{
    shell_ = create_platform_shell();
    if (!shell_) {
        std::cerr << "failed to create platform shell\n";
        return 1;
    }

    if (!shell_->create(config_.shell)) {
        std::cerr << "failed to initialize platform shell\n";
        return 1;
    }

    app_state quiz_state({make_demo_deck()});
    render_and_report(*shell_, config_.shell, "deck-list", quiz_state.snapshot());

    quiz_state.dispatch(domain::make_select_deck_action("demo_deck"));
    quiz_state.dispatch(domain::make_select_day_action("day_1"));
    quiz_state.dispatch(domain::make_start_quiz_action(domain::quiz_mode::normal), 100);
    render_and_report(*shell_, config_.shell, "quiz-active", quiz_state.snapshot());

    quiz_state.dispatch(domain::make_submit_option_action(0), 200);
    render_and_report(*shell_, config_.shell, "quiz-feedback", quiz_state.snapshot());

    while (shell_->pump_events() == platform_shell_status::keep_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}

} // namespace quiz_vulkan
