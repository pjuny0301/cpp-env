#include "app/app.h"

#include "assets/asset_resolver.h"
#include "assets/deck_source_asset_adapter.h"
#include "app/app_deck_loader.h"
#include "app/app_demo.h"
#include "app/app_input_router.h"
#include "app/app_render_pipeline.h"
#include "app/app_state.h"
#include "core/input/input_engine.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
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
    app_render_pipeline_interface& render_pipeline,
    platform_shell& shell,
    const platform_shell_config& shell_config,
    std::string_view label,
    const domain::app_snapshot& snapshot,
    std::string_view typed_text_answer = {})
{
    app_render_frame frame = render_pipeline.render(app_render_request{
        .snapshot = &snapshot,
        .viewport = viewport_for_shell(shell.state(), shell_config),
        .view_state = app_render_view_state{.typed_text_answer = typed_text_answer},
    });
    shell.present_framebuffer(
        frame.framebuffer.width,
        frame.framebuffer.height,
        frame.framebuffer.rgba.empty() ? nullptr : frame.framebuffer.rgba.data());
    const std::string line = format_render_report(label, frame.report);
    shell.show_message(line);
    shell.set_frame_status(line);
    return frame;
}

std::vector<domain::deck> load_initial_decks(const app_config& config, platform_shell& shell)
{
    if (config.deck_artifacts.empty() && config.deck_sources.requests.empty()) {
        return {make_demo_deck()};
    }

    assets::normalizing_asset_resolver asset_resolver;
    assets::asset_deck_source_provider deck_source_provider(
        asset_resolver,
        assets::deck_source_asset_adapter_config{
            .local_root = config.deck_sources.local_root,
            .asset_root = config.deck_sources.asset_root,
        });

    app_deck_load_result load_result = load_app_decks(app_deck_load_request{
        .deck_artifacts = config.deck_artifacts,
        .deck_sources = config.deck_sources.requests,
        .deck_source_provider = &deck_source_provider,
    });
    for (const std::string& message : load_result.messages) {
        shell.show_message(message);
    }

    return load_result.decks;
}

std::filesystem::path image_base_directory_for_config(const app_config& config)
{
    if (!config.deck_sources.asset_root.empty()) {
        return config.deck_sources.asset_root;
    }
    return config.deck_sources.local_root;
}

std::int64_t now_ms()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

void sync_text_focus(input::input_engine& input_engine, const scene::placed_scene& placed_scene)
{
    const std::optional<std::string> target_id = keyboard_input_target(placed_scene);
    if (target_id.has_value()) {
        if (!input_engine.has_text_focus() || input_engine.text_focus_id() != *target_id) {
            input_engine.focus_text_target(*target_id);
        }
        return;
    }

    if (input_engine.has_text_focus()) {
        input_engine.clear_text_focus();
    }
}

bool dispatch_normalized_input_events(
    app_state& quiz_state,
    const std::vector<input::input_event>& input_events,
    const scene::placed_scene& placed_scene,
    input::input_engine& input_engine,
    platform_shell& shell);

bool dispatch_platform_input(
    app_state& quiz_state,
    const platform_input_event& event,
    const scene::placed_scene& placed_scene,
    input::input_engine& input_engine,
    platform_shell& shell)
{
    bool should_render = false;
    sync_text_focus(input_engine, placed_scene);

    const std::vector<raw_platform_input_event> raw_events = normalize_platform_input_event(event, now_ms());
    for (const raw_platform_input_event& raw_event : raw_events) {
        should_render = dispatch_normalized_input_events(
            quiz_state,
            input_engine.process_raw_event(raw_event),
            placed_scene,
            input_engine,
            shell) || should_render;
    }

    return should_render;
}

bool dispatch_normalized_input_events(
    app_state& quiz_state,
    const std::vector<input::input_event>& input_events,
    const scene::placed_scene& placed_scene,
    input::input_engine& input_engine,
    platform_shell& shell)
{
    bool should_render = false;
    for (const input::input_event& input_event : input_events) {
        const app_input_route_result route_result = route_normalized_input_event(
            input_event,
            placed_scene,
            input_engine.text_model().text());

        if (!route_result.handled) {
            continue;
        }
        if (!route_result.ok()) {
            shell.show_message("input action rejected: " + route_result.error);
            continue;
        }

        should_render = route_result.needs_render || should_render;
        if (!route_result.action.has_value()) {
            shell.show_message("typed answer: " + input_engine.text_model().display_text());
            continue;
        }

        quiz_state.dispatch(*route_result.action, now_ms());
        if (route_result.clear_text_after_action) {
            input_engine.reset();
        }
        shell.show_message("input action: " + std::string(domain::to_string(domain::type_of(*route_result.action))));
    }

    return should_render;
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
    default_app_render_pipeline render_pipeline(default_app_render_pipeline_config{
        .image_base_directory = image_base_directory_for_config(config_),
        .native_window = shell_->native_window_handle(),
        .renderer_options = {},
    });
    input::input_engine input_engine;
    app_render_frame latest_frame = render_and_report(
        render_pipeline,
        *shell_,
        config_.shell,
        "deck-list",
        quiz_state.snapshot(),
        input_engine.text_model().display_text());
    sync_text_focus(input_engine, latest_frame.placed_scene);

    while (shell_->pump_events() == platform_shell_status::keep_running) {
        bool should_render = false;
        for (const platform_input_event& event : shell_->drain_input_events()) {
            should_render = dispatch_platform_input(
                quiz_state,
                event,
                latest_frame.placed_scene,
                input_engine,
                *shell_) || should_render;
        }
        sync_text_focus(input_engine, latest_frame.placed_scene);
        should_render = dispatch_normalized_input_events(
            quiz_state,
            input_engine.update_time(now_ms()),
            latest_frame.placed_scene,
            input_engine,
            *shell_) || should_render;
        if (should_render) {
            latest_frame = render_and_report(
                render_pipeline,
                *shell_,
                config_.shell,
                "input",
                quiz_state.snapshot(),
                input_engine.text_model().display_text());
            sync_text_focus(input_engine, latest_frame.placed_scene);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}

} // namespace quiz_vulkan
