#pragma once

#include "app/app_quiz_screens.h"
#include "core/domain/app_snapshot.hpp"
#include "core/layout/layout_placer.h"
#include "core/scene/placed_scene.h"
#include "core/scene/scene_layout_data.h"
#include "core/scene/scene_layout_patch.h"

#include <optional>
#include <string>
#include <utility>

namespace quiz_vulkan::presentation {

struct app_scene_preview_request {
    const app_scene_script_document* document = nullptr;
    const domain::app_snapshot* snapshot = nullptr;
    scene::scene_rect viewport{0.0f, 0.0f, 360.0f, 640.0f};
    const scene::text_metrics_interface* text_metrics = nullptr;
    std::string scene_name = "app_scene_preview";
};

struct app_scene_preview_result {
    std::optional<scene::scene_layout_patch> patch;
    scene::scene_layout_data layout;
    scene::placed_scene placed;
    std::string error;

    explicit app_scene_preview_result(std::string scene_name = "app_scene_preview")
        : layout(std::move(scene_name))
    {
    }

    bool ok() const
    {
        return patch.has_value() && error.empty();
    }
};

inline app_scene_preview_result preview_app_scene_script(const app_scene_preview_request& request)
{
    app_scene_preview_result result(request.scene_name.empty() ? "app_scene_preview" : request.scene_name);

    if (request.document == nullptr) {
        result.error = "preview request is missing a scene script document";
        return result;
    }
    if (request.snapshot == nullptr) {
        result.error = "preview request is missing an app snapshot";
        return result;
    }
    if (request.text_metrics == nullptr) {
        result.error = "preview request is missing text metrics";
        return result;
    }

    app_scene_script_compile_result compiled = compile_quiz_screen_script(*request.document, *request.snapshot);
    if (!compiled.ok()) {
        result.error = compiled.error.empty() ? "scene script compile failed" : compiled.error;
        return result;
    }

    result.patch = std::move(*compiled.patch);
    const scene::scene_layout_apply_result applied = result.patch->apply_to(result.layout);
    if (!applied.applied()) {
        result.error = applied.errors.empty() ? "scene preview patch apply failed" : applied.errors.front();
        return result;
    }

    result.placed = scene::layout_placer().place(result.layout, request.viewport, *request.text_metrics);
    return result;
}

inline app_scene_preview_result preview_app_scene_script(
    const app_scene_script_document& document,
    const domain::app_snapshot& snapshot,
    scene::scene_rect viewport,
    const scene::text_metrics_interface& text_metrics,
    std::string scene_name = "app_scene_preview")
{
    return preview_app_scene_script(app_scene_preview_request{
        .document = &document,
        .snapshot = &snapshot,
        .viewport = viewport,
        .text_metrics = &text_metrics,
        .scene_name = std::move(scene_name),
    });
}

} // namespace quiz_vulkan::presentation
