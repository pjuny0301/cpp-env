#pragma once

#include "core/input/input_engine.h"
#include "core/input/platform_input_translator.h"

#include <span>
#include <utility>
#include <vector>

namespace quiz_vulkan::input {

struct platform_input_dispatch_result {
    platform_input_translation_result translation;
    bool dispatched_to_engine = false;
    std::vector<input_event> input_events;
    input_routing_diagnostics routing_diagnostics;
};

struct platform_input_dispatch_batch_result {
    std::vector<platform_input_dispatch_result> items;
    input_diagnostic_summary summary;
};

[[nodiscard]] inline platform_input_dispatch_result dispatch_translated_platform_input(
    input_engine& engine,
    platform_input_translation_result translation)
{
    platform_input_dispatch_result result{
        .translation = std::move(translation),
        .dispatched_to_engine = false,
        .input_events = {},
        .routing_diagnostics = engine.routing_diagnostics(),
    };

    if (!result.translation.event.has_value()) {
        return result;
    }

    result.input_events = engine.process_raw_event(*result.translation.event);
    result.dispatched_to_engine = true;
    result.routing_diagnostics = engine.routing_diagnostics();
    return result;
}

[[nodiscard]] inline platform_input_dispatch_result translate_and_dispatch_platform_input(
    input_engine& engine,
    const platform_input_translator& translator,
    const platform_input_translation_request& request)
{
    return dispatch_translated_platform_input(engine, translator.translate(request));
}

[[nodiscard]] inline platform_input_dispatch_batch_result translate_and_dispatch_platform_input_batch(
    input_engine& engine,
    const platform_input_translator& translator,
    std::span<const platform_input_translation_request> requests)
{
    platform_input_dispatch_batch_result batch;
    batch.items.reserve(requests.size());
    for (const platform_input_translation_request& request : requests) {
        platform_input_dispatch_result item = translate_and_dispatch_platform_input(engine, translator, request);
        if (item.dispatched_to_engine) {
            accumulate_input_diagnostic_summary(batch.summary, item.routing_diagnostics.summary);
        }
        batch.items.push_back(std::move(item));
    }
    return batch;
}

} // namespace quiz_vulkan::input
