#include "app/app.h"

#include "app/app_action_router.h"
#include "app/app_demo.h"
#include "app/app_state.h"
#include "core/domain/deck_artifact_loader.hpp"
#include "core/layout/input_hit_test.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <iostream>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

namespace quiz_vulkan {
namespace {

scene::scene_rect viewport_for_shell(const platform_shell_state& state, const platform_shell_config& fallback)
{
    const int width = state.client_size.width > 0 ? state.client_size.width : fallback.width;
    const int height = state.client_size.height > 0 ? state.client_size.height : fallback.height;
    return scene::scene_rect{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
}

app_render_frame render_and_report(
    platform_shell& shell,
    const platform_shell_config& shell_config,
    std::string_view label,
    const domain::app_snapshot& snapshot)
{
    app_render_frame frame = render_app_frame(snapshot, viewport_for_shell(shell.state(), shell_config));
    shell.present_frame(
        frame.framebuffer.width,
        frame.framebuffer.height,
        frame.framebuffer.rgba.empty() ? nullptr : frame.framebuffer.rgba.data(),
        frame.text_overlays);
    const std::string line = format_render_report(label, frame.report);
    shell.show_message(line);
    shell.set_frame_status(line);
    return frame;
}

std::vector<domain::deck> load_initial_decks(const app_config& config, platform_shell& shell)
{
    if (config.deck_artifacts.empty()) {
        return {make_demo_deck()};
    }

    std::vector<domain::deck> decks;
    for (const std::filesystem::path& artifact_path : config.deck_artifacts) {
        const domain::deck_artifact_load_result result = domain::load_deck_artifact_file(artifact_path);
        if (result.ok() && result.value.has_value()) {
            shell.show_message("loaded deck artifact: " + artifact_path.string());
            decks.push_back(*result.value);
            continue;
        }

        std::ostringstream message;
        message << "failed to load deck artifact: " << artifact_path.string();
        shell.show_message(message.str());

        for (const domain::deck_artifact_parse_error& error : result.errors) {
            std::ostringstream error_line;
            error_line << "  line " << error.line << ": " << error.message;
            shell.show_message(error_line.str());
        }
    }

    return decks;
}

std::int64_t now_ms()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

bool dispatch_platform_input(
    app_state& quiz_state,
    const platform_input_event& event,
    const scene::placed_scene& placed_scene,
    platform_shell& shell)
{
    if (event.type != platform_input_event_type::pointer_press) {
        return false;
    }

    const scene::scene_input_region* region = hit_test_input_region(
        placed_scene,
        event.x,
        event.y,
        scene::scene_action_trigger::press);
    if (region == nullptr) {
        return false;
    }

    const app_action_route_result routed_action = route_scene_action(region->action);
    if (!routed_action.ok() || !routed_action.action.has_value()) {
        shell.show_message("input action rejected: " + routed_action.error);
        return false;
    }

    quiz_state.dispatch(*routed_action.action, now_ms());
    shell.show_message("input action: " + std::string(domain::to_string(domain::type_of(*routed_action.action))));
    return true;
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

    std::vector<domain::deck> decks = load_initial_decks(config_, *shell_);
    if (decks.empty()) {
        shell_->show_message("no valid quiz deck is available");
        return 1;
    }

    app_state quiz_state(std::move(decks));
    app_render_frame latest_frame = render_and_report(*shell_, config_.shell, "deck-list", quiz_state.snapshot());

    while (shell_->pump_events() == platform_shell_status::keep_running) {
        bool should_render = false;
        for (const platform_input_event& event : shell_->drain_input_events()) {
            should_render = dispatch_platform_input(quiz_state, event, latest_frame.placed_scene, *shell_) || should_render;
        }
        if (should_render) {
            latest_frame = render_and_report(*shell_, config_.shell, "input", quiz_state.snapshot());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}

} // namespace quiz_vulkan
