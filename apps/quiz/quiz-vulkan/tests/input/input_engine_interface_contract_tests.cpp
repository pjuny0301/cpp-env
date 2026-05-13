#include "core/input/gesture_recognizer.h"
#include "core/input/input_action_candidate_plan.h"
#include "core/input/input_routing_diagnostics.h"
#include "core/input/input_engine.h"
#include "core/input/normalized_input_replay_diff.h"
#include "core/input/normalized_input_replay.h"
#include "core/input/platform_input_engine_adapter.h"
#include "core/input/platform_input_replay.h"
#include "core/input/platform_input_translator.h"
#include "core/input/text_edit_transaction_diagnostics.h"
#include "core/input/text_edit_transaction_replay.h"
#include "core/input/text_input_model.h"
#include "core/input/text_input_presentation.h"
#include "platform/platform_input_event.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept GestureRecognizerInterface = requires(
    T& recognizer,
    const input::pointer_event& pointer,
    std::int64_t timestamp_ms) {
    { recognizer.process_pointer_event(pointer) } -> std::same_as<std::vector<input::gesture_event>>;
    { recognizer.update_time(timestamp_ms) } -> std::same_as<std::vector<input::gesture_event>>;
    { recognizer.capture_snapshot() } -> std::same_as<input::pointer_capture_snapshot>;
    { recognizer.policy_snapshots() } -> std::same_as<const std::vector<input::gesture_policy_snapshot>&>;
    { recognizer.reset() } -> std::same_as<void>;
};

template <typename T>
concept PlatformInputTranslatorInterface = requires(
    const T& translator,
    const input::platform_input_translation_request& request) {
    { translator.translate(request) } -> std::same_as<input::platform_input_translation_result>;
};

template <typename T>
concept PlatformMouseSampleInterface = requires(T sample) {
    { sample.timestamp_ms } -> std::same_as<std::int64_t&>;
    { sample.pointer_id } -> std::same_as<std::int32_t&>;
    { sample.phase } -> std::same_as<input::platform_pointer_sample_phase&>;
    { sample.button } -> std::same_as<input::platform_mouse_button&>;
    { sample.x } -> std::same_as<float&>;
    { sample.y } -> std::same_as<float&>;
};

template <typename T>
concept PlatformTouchSampleInterface = requires(T sample) {
    { sample.timestamp_ms } -> std::same_as<std::int64_t&>;
    { sample.contact_id } -> std::same_as<std::int32_t&>;
    { sample.phase } -> std::same_as<input::platform_pointer_sample_phase&>;
    { sample.x } -> std::same_as<float&>;
    { sample.y } -> std::same_as<float&>;
};

template <typename T>
concept PlatformWheelSampleInterface = requires(T sample) {
    { sample.timestamp_ms } -> std::same_as<std::int64_t&>;
    { sample.x } -> std::same_as<float&>;
    { sample.y } -> std::same_as<float&>;
    { sample.delta_x } -> std::same_as<float&>;
    { sample.delta_y } -> std::same_as<float&>;
    { sample.unit } -> std::same_as<input::platform_scroll_delta_unit&>;
};

template <typename T>
concept PlatformKeySampleInterface = requires(T sample) {
    { sample.timestamp_ms } -> std::same_as<std::int64_t&>;
    { sample.phase } -> std::same_as<input::platform_key_sample_phase&>;
    { sample.key_code } -> std::same_as<std::int32_t&>;
    { sample.logical_key } -> std::same_as<std::string&>;
    { sample.alt } -> std::same_as<bool&>;
    { sample.ctrl } -> std::same_as<bool&>;
    { sample.shift } -> std::same_as<bool&>;
    { sample.meta } -> std::same_as<bool&>;
    { sample.repeat } -> std::same_as<bool&>;
};

template <typename T>
concept PlatformCharacterSampleInterface = requires(T sample) {
    { sample.timestamp_ms } -> std::same_as<std::int64_t&>;
    { sample.utf8_text } -> std::same_as<std::string&>;
};

template <typename T>
concept PlatformImeCompositionSampleInterface = requires(T sample) {
    { sample.timestamp_ms } -> std::same_as<std::int64_t&>;
    { sample.phase } -> std::same_as<input::platform_ime_sample_phase&>;
    { sample.utf8_text } -> std::same_as<std::string&>;
};

template <typename T>
concept PlatformInputTranslationDiagnosticInterface = requires(T diagnostic) {
    { diagnostic.source } -> std::same_as<input::platform_input_source_kind&>;
    { diagnostic.status } -> std::same_as<input::platform_input_translation_status&>;
    { diagnostic.rejection_reason } -> std::same_as<input::platform_input_rejection_reason&>;
    { diagnostic.timestamp_ms } -> std::same_as<std::int64_t&>;
    { diagnostic.emitted_event } -> std::same_as<bool&>;
};

template <typename T>
concept PlatformInputTranslationResultInterface = requires(T result) {
    { result.event } -> std::same_as<std::optional<raw_platform_input_event>&>;
    { result.diagnostic } -> std::same_as<input::platform_input_translation_diagnostic&>;
};

template <typename T>
concept PlatformInputDispatchResultInterface = requires(T result) {
    { result.translation } -> std::same_as<input::platform_input_translation_result&>;
    { result.dispatched_to_engine } -> std::same_as<bool&>;
    { result.input_events } -> std::same_as<std::vector<input::input_event>&>;
    { result.routing_diagnostics } -> std::same_as<input::input_routing_diagnostics&>;
};

template <typename T>
concept PlatformInputDispatchBatchResultInterface = requires(T result) {
    { result.items } -> std::same_as<std::vector<input::platform_input_dispatch_result>&>;
    { result.summary } -> std::same_as<input::input_diagnostic_summary&>;
};

template <typename T>
concept PlatformInputEngineAdapterFunctions = requires(
    input::input_engine& engine,
    const input::platform_input_translator& translator,
    input::platform_input_translation_result translation,
    const input::platform_input_translation_request& request,
    std::span<const input::platform_input_translation_request> requests) {
    { input::dispatch_translated_platform_input(engine, std::move(translation)) }
        -> std::same_as<input::platform_input_dispatch_result>;
    { input::translate_and_dispatch_platform_input(engine, translator, request) }
        -> std::same_as<input::platform_input_dispatch_result>;
    { input::translate_and_dispatch_platform_input_batch(engine, translator, requests) }
        -> std::same_as<input::platform_input_dispatch_batch_result>;
};

template <typename T>
concept PlatformInputReplayStepInterface = requires(T step) {
    { step.label } -> std::same_as<std::string&>;
    { step.action } -> std::same_as<input::platform_input_replay_action&>;
};

template <typename T>
concept PlatformInputReplayOptionsInterface = requires(T options) {
    { options.initial_focus_target_id } -> std::same_as<std::string&>;
};

template <typename T>
concept PlatformInputReplayEventCountsInterface = requires(T counts) {
    { counts.pointer } -> std::same_as<std::size_t&>;
    { counts.mouse } -> std::same_as<std::size_t&>;
    { counts.touch } -> std::same_as<std::size_t&>;
    { counts.wheel } -> std::same_as<std::size_t&>;
    { counts.key } -> std::same_as<std::size_t&>;
    { counts.character } -> std::same_as<std::size_t&>;
    { counts.focus } -> std::same_as<std::size_t&>;
    { counts.ime } -> std::same_as<std::size_t&>;
    { counts.time_update } -> std::same_as<std::size_t&>;
    { counts.total } -> std::same_as<std::size_t&>;
};

template <typename T>
concept PlatformInputReplayRejectionCountsInterface = requires(T counts) {
    { counts.invalid_timestamp } -> std::same_as<std::size_t&>;
    { counts.invalid_pointer_id } -> std::same_as<std::size_t&>;
    { counts.invalid_coordinates } -> std::same_as<std::size_t&>;
    { counts.invalid_wheel_delta } -> std::same_as<std::size_t&>;
    { counts.invalid_key } -> std::same_as<std::size_t&>;
    { counts.empty_text } -> std::same_as<std::size_t&>;
    { counts.invalid_utf8 } -> std::same_as<std::size_t&>;
};

template <typename T>
concept PlatformInputReplayTranslationCountsInterface = requires(T counts) {
    { counts.total } -> std::same_as<std::size_t&>;
    { counts.accepted } -> std::same_as<std::size_t&>;
    { counts.rejected } -> std::same_as<std::size_t&>;
    { counts.emitted_event } -> std::same_as<std::size_t&>;
    { counts.dispatched_to_engine } -> std::same_as<std::size_t&>;
    { counts.rejected_without_side_effect } -> std::same_as<std::size_t&>;
    { counts.rejections } -> std::same_as<input::platform_input_replay_rejection_counts&>;
};

template <typename T>
concept PlatformInputReplayStepDiagnosticsInterface = requires(T diagnostics) {
    { diagnostics.label } -> std::same_as<std::string&>;
    { diagnostics.source } -> std::same_as<input::platform_input_replay_source_kind&>;
    { diagnostics.has_translation } -> std::same_as<bool&>;
    { diagnostics.translation } -> std::same_as<input::platform_input_translation_diagnostic&>;
    { diagnostics.dispatched_to_engine } -> std::same_as<bool&>;
    { diagnostics.rejected_without_side_effect } -> std::same_as<bool&>;
    { diagnostics.normalized_batch } -> std::same_as<input::normalized_input_replay_batch&>;
    { diagnostics.before_presentation } -> std::same_as<input::text_input_presentation_snapshot&>;
    { diagnostics.after_presentation } -> std::same_as<input::text_input_presentation_snapshot&>;
    { diagnostics.text_presentation_diff } -> std::same_as<input::text_input_presentation_diff&>;
    { diagnostics.produced_text_edit_transaction } -> std::same_as<bool&>;
    { diagnostics.text_edit } -> std::same_as<input::text_edit_transaction_replay_step_diagnostics&>;
};

template <typename T>
concept PlatformInputReplaySummaryInterface = requires(T summary) {
    { summary.steps } -> std::same_as<std::vector<input::platform_input_replay_step_diagnostics>&>;
    { summary.events } -> std::same_as<input::platform_input_replay_event_counts&>;
    { summary.translations } -> std::same_as<input::platform_input_replay_translation_counts&>;
    { summary.normalized } -> std::same_as<input::normalized_input_replay_recording&>;
    { summary.text_edits } -> std::same_as<input::text_edit_transaction_replay_summary&>;
    { summary.pointer } -> std::same_as<input::normalized_input_replay_pointer_summary&>;
    { summary.gesture_policies } -> std::same_as<input::normalized_input_replay_gesture_policy_summary&>;
    { summary.focus } -> std::same_as<input::normalized_input_replay_focus_summary&>;
    { summary.ime } -> std::same_as<input::normalized_input_replay_ime_summary&>;
    { summary.keyboard } -> std::same_as<input::normalized_input_replay_keyboard_summary&>;
    { summary.route_summary } -> std::same_as<input::input_diagnostic_summary&>;
    { summary.final_text_presentation } -> std::same_as<input::text_input_presentation_snapshot&>;
};

template <typename T>
concept PlatformInputReplayEventCountDeltasInterface = requires(T deltas) {
    { deltas.pointer } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.mouse } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.touch } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.wheel } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.key } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.character } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.focus } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.ime } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.time_update } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.total } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.changed } -> std::same_as<bool&>;
};

template <typename T>
concept PlatformInputReplayRejectionCountDeltasInterface = requires(T deltas) {
    { deltas.invalid_timestamp } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.invalid_pointer_id } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.invalid_coordinates } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.invalid_wheel_delta } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.invalid_key } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.empty_text } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.invalid_utf8 } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.changed } -> std::same_as<bool&>;
};

template <typename T>
concept PlatformInputReplayTranslationCountDeltasInterface = requires(T deltas) {
    { deltas.total } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.accepted } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.rejected } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.emitted_event } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.dispatched_to_engine } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.rejected_without_side_effect } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.rejections } -> std::same_as<input::platform_input_replay_rejection_count_deltas&>;
    { deltas.changed } -> std::same_as<bool&>;
};

template <typename T>
concept PlatformInputReplayDiffInterface = requires(T diff) {
    { diff.events } -> std::same_as<input::platform_input_replay_event_count_deltas&>;
    { diff.translations } -> std::same_as<input::platform_input_replay_translation_count_deltas&>;
    { diff.normalized } -> std::same_as<input::normalized_input_replay_diff&>;
    { diff.text_edits } -> std::same_as<input::text_edit_transaction_replay_diff&>;
    { diff.final_text_presentation } -> std::same_as<input::text_input_presentation_diff&>;
    { diff.event_counts_changed } -> std::same_as<bool&>;
    { diff.translation_counts_changed } -> std::same_as<bool&>;
    { diff.normalized_changed } -> std::same_as<bool&>;
    { diff.text_edit_changed } -> std::same_as<bool&>;
    { diff.gesture_capture_focus_changed } -> std::same_as<bool&>;
    { diff.final_text_presentation_changed } -> std::same_as<bool&>;
    { diff.changed_category_count } -> std::same_as<std::size_t&>;
    { diff.changed } -> std::same_as<bool&>;
};

template <typename T>
concept PlatformInputReplayFunctions = requires(
    input::input_engine& engine,
    const input::platform_input_translator& translator,
    const input::platform_input_translation_request& request,
    input::platform_input_replay_event_counts& event_counts,
    input::platform_input_replay_rejection_counts& rejection_counts,
    input::platform_input_replay_translation_counts& translation_counts,
    const input::platform_input_translation_diagnostic& translation,
    input::platform_input_replay_source_kind source,
    input::action_route_policy_kind route_kind,
    std::span<const input::action_route_policy_diagnostic> routes,
    input::platform_input_replay_summary& summary,
    input::platform_input_replay_step_diagnostics& step_diagnostics,
    input::normalized_input_replay_recording& recording,
    input::normalized_input_replay_batch batch,
    const input::text_edit_transaction_diagnostics& transaction,
    const input::text_input_presentation_snapshot& presentation,
    const input::input_routing_diagnostics& routing,
    const input::platform_input_replay_step& step,
    std::span<const input::platform_input_replay_step> steps,
    const input::platform_input_replay_options& options) {
    { input::platform_input_replay_source_for(request) }
        -> std::same_as<input::platform_input_replay_source_kind>;
    { input::count_platform_input_replay_event(event_counts, source) } -> std::same_as<void>;
    { input::count_platform_input_replay_rejection(
        rejection_counts,
        input::platform_input_rejection_reason{}) } -> std::same_as<void>;
    { input::count_platform_input_replay_translation(translation_counts, translation, bool{}) }
        -> std::same_as<void>;
    { input::platform_input_replay_route_has_text_transaction(route_kind) } -> std::same_as<bool>;
    { input::platform_input_replay_routes_have_text_transaction(routes) } -> std::same_as<bool>;
    { input::make_platform_input_replay_text_edit_step(
        std::string{},
        transaction,
        presentation,
        presentation) } -> std::same_as<input::text_edit_transaction_replay_step_diagnostics>;
    { input::make_platform_input_replay_empty_batch(engine, std::string{}) }
        -> std::same_as<input::normalized_input_replay_batch>;
    { input::append_platform_input_replay_normalized_batch(recording, std::move(batch)) }
        -> std::same_as<void>;
    { input::append_platform_input_replay_text_edit_if_present(
        summary,
        step_diagnostics,
        engine,
        routing) } -> std::same_as<void>;
    { input::finalize_platform_input_replay_summary(summary, engine) } -> std::same_as<void>;
    { input::replay_platform_input_step(engine, translator, summary, step) }
        -> std::same_as<input::platform_input_replay_step_diagnostics>;
    { input::replay_platform_input_batch(engine, translator, steps, options) }
        -> std::same_as<input::platform_input_replay_summary>;
    { input::replay_platform_input_batch(steps, options) }
        -> std::same_as<input::platform_input_replay_summary>;
    { input::diff_platform_input_replay_event_counts(summary.events, summary.events) }
        -> std::same_as<input::platform_input_replay_event_count_deltas>;
    { input::diff_platform_input_replay_rejection_counts(
        summary.translations.rejections,
        summary.translations.rejections) }
        -> std::same_as<input::platform_input_replay_rejection_count_deltas>;
    { input::diff_platform_input_replay_translation_counts(summary.translations, summary.translations) }
        -> std::same_as<input::platform_input_replay_translation_count_deltas>;
    { input::diff_platform_input_replay_summaries(summary, summary) }
        -> std::same_as<input::platform_input_replay_diff>;
};

template <typename T>
concept NormalizedInputReplayEndStateInterface = requires(T state) {
    { state.pointer_capture } -> std::same_as<input::pointer_capture_snapshot&>;
    { state.has_text_focus } -> std::same_as<bool&>;
    { state.focus_id } -> std::same_as<std::string&>;
    { state.text } -> std::same_as<std::string&>;
    { state.display_text } -> std::same_as<std::string&>;
    { state.caret_byte_offset } -> std::same_as<std::size_t&>;
    { state.has_selection } -> std::same_as<bool&>;
    { state.selection } -> std::same_as<input::text_range&>;
    { state.preedit_text } -> std::same_as<std::string&>;
    { state.composition } -> std::same_as<input::ime_composition_state&>;
    { state.pointer_capture_clean } -> std::same_as<bool&>;
    { state.focus_clean } -> std::same_as<bool&>;
    { state.preedit_clean } -> std::same_as<bool&>;
    { state.text_presentation } -> std::same_as<input::text_input_presentation_snapshot&>;
};

template <typename T>
concept NormalizedInputReplayBatchInterface = requires(T batch) {
    { batch.label } -> std::same_as<std::string&>;
    { batch.input_events } -> std::same_as<std::vector<input::input_event>&>;
    { batch.normalized_events } -> std::same_as<std::vector<input::normalized_input_event_summary>&>;
    { batch.summary } -> std::same_as<input::input_diagnostic_summary&>;
    { batch.keyboard } -> std::same_as<input::normalized_input_replay_keyboard_summary&>;
    { batch.ime } -> std::same_as<input::normalized_input_replay_ime_summary&>;
    { batch.pointer } -> std::same_as<input::normalized_input_replay_pointer_summary&>;
    { batch.gesture_policies } -> std::same_as<input::normalized_input_replay_gesture_policy_summary&>;
    { batch.focus } -> std::same_as<input::normalized_input_replay_focus_summary&>;
    { batch.end_state } -> std::same_as<input::normalized_input_replay_end_state&>;
    { batch.text_presentation_diff } -> std::same_as<input::text_input_presentation_diff&>;
};

template <typename T>
concept NormalizedInputReplayRecordingInterface = requires(T recording) {
    { recording.batches } -> std::same_as<std::vector<input::normalized_input_replay_batch>&>;
    { recording.summary } -> std::same_as<input::input_diagnostic_summary&>;
    { recording.keyboard } -> std::same_as<input::normalized_input_replay_keyboard_summary&>;
    { recording.ime } -> std::same_as<input::normalized_input_replay_ime_summary&>;
    { recording.pointer } -> std::same_as<input::normalized_input_replay_pointer_summary&>;
    { recording.gesture_policies } -> std::same_as<input::normalized_input_replay_gesture_policy_summary&>;
    { recording.focus } -> std::same_as<input::normalized_input_replay_focus_summary&>;
    { recording.final_state } -> std::same_as<input::normalized_input_replay_end_state&>;
};

template <typename T>
concept NormalizedInputReplayStepInterface = requires(T step) {
    { step.label } -> std::same_as<std::string&>;
    { step.action } -> std::same_as<input::normalized_input_replay_action&>;
};

template <typename T>
concept NormalizedInputReplayKeyboardIntentCountsInterface = requires(T counts) {
    { counts.none } -> std::same_as<std::size_t&>;
    { counts.focus_traversal_next } -> std::same_as<std::size_t&>;
    { counts.focus_traversal_previous } -> std::same_as<std::size_t&>;
    { counts.submit } -> std::same_as<std::size_t&>;
    { counts.cancel } -> std::same_as<std::size_t&>;
    { counts.caret_previous } -> std::same_as<std::size_t&>;
    { counts.caret_next } -> std::same_as<std::size_t&>;
    { counts.caret_home } -> std::same_as<std::size_t&>;
    { counts.caret_end } -> std::same_as<std::size_t&>;
    { counts.selection_previous } -> std::same_as<std::size_t&>;
    { counts.selection_next } -> std::same_as<std::size_t&>;
    { counts.select_all } -> std::same_as<std::size_t&>;
    { counts.delete_backward } -> std::same_as<std::size_t&>;
    { counts.delete_forward } -> std::same_as<std::size_t&>;
};

template <typename T>
concept NormalizedInputReplayKeyboardModifierCountsInterface = requires(T counts) {
    { counts.unmodified } -> std::same_as<std::size_t&>;
    { counts.alt } -> std::same_as<std::size_t&>;
    { counts.ctrl } -> std::same_as<std::size_t&>;
    { counts.shift } -> std::same_as<std::size_t&>;
    { counts.meta } -> std::same_as<std::size_t&>;
};

template <typename T>
concept NormalizedInputReplayKeyboardRepeatPolicyCountsInterface = requires(T counts) {
    { counts.not_repeat } -> std::same_as<std::size_t&>;
    { counts.allowed } -> std::same_as<std::size_t&>;
    { counts.ignored } -> std::same_as<std::size_t&>;
};

template <typename T>
concept NormalizedInputReplayKeyboardSummaryInterface = requires(T summary) {
    { summary.chords } -> std::same_as<std::vector<input::keyboard_chord_diagnostic>&>;
    { summary.intents } -> std::same_as<input::normalized_input_replay_keyboard_intent_counts&>;
    { summary.modifiers } -> std::same_as<input::normalized_input_replay_keyboard_modifier_counts&>;
    { summary.repeat_policies }
        -> std::same_as<input::normalized_input_replay_keyboard_repeat_policy_counts&>;
    { summary.total } -> std::same_as<std::size_t&>;
    { summary.emitted_input_event_routes } -> std::same_as<std::size_t&>;
    { summary.diagnostic_only_routes } -> std::same_as<std::size_t&>;
};

template <typename T>
concept NormalizedInputReplayGesturePolicySummaryInterface = requires(T summary) {
    { summary.routes } -> std::same_as<std::vector<input::action_route_policy_diagnostic>&>;
    { summary.total } -> std::same_as<std::size_t&>;
    { summary.emitted_input_event_routes } -> std::same_as<std::size_t&>;
    { summary.diagnostic_only_routes } -> std::same_as<std::size_t&>;
};

template <typename T>
concept NormalizedInputReplayImePhaseCountsInterface = requires(T counts) {
    { counts.composition_start } -> std::same_as<std::size_t&>;
    { counts.preedit_update } -> std::same_as<std::size_t&>;
    { counts.commit } -> std::same_as<std::size_t&>;
    { counts.cancel } -> std::same_as<std::size_t&>;
};

template <typename T>
concept NormalizedInputReplayImeTimelineEntryInterface = requires(T entry) {
    { entry.phase } -> std::same_as<input::normalized_input_replay_ime_timeline_phase&>;
    { entry.timestamp_ms } -> std::same_as<std::int64_t&>;
    { entry.emits_input_event } -> std::same_as<bool&>;
    { entry.event_index } -> std::same_as<std::size_t&>;
    { entry.target_id } -> std::same_as<std::string&>;
    { entry.utf8_text } -> std::same_as<std::string&>;
    { entry.committed_text } -> std::same_as<std::string&>;
    { entry.composition } -> std::same_as<input::ime_composition_state&>;
    { entry.text_byte_count } -> std::same_as<std::size_t&>;
    { entry.text_byte_count_before } -> std::same_as<std::size_t&>;
    { entry.text_byte_count_after } -> std::same_as<std::size_t&>;
    { entry.caret_before } -> std::same_as<input::text_range&>;
    { entry.caret_after } -> std::same_as<input::text_range&>;
    { entry.had_selection_before } -> std::same_as<bool&>;
    { entry.has_selection_after } -> std::same_as<bool&>;
    { entry.selection_before } -> std::same_as<input::text_range&>;
    { entry.selection_after } -> std::same_as<input::text_range&>;
    { entry.preedit_text_valid } -> std::same_as<bool&>;
    { entry.preedit_range_valid } -> std::same_as<bool&>;
    { entry.stale_preedit_cleared_after } -> std::same_as<bool&>;
    { entry.committed_text_after } -> std::same_as<std::string&>;
    { entry.display_text_after } -> std::same_as<std::string&>;
    { entry.preedit_text_after } -> std::same_as<std::string&>;
};

template <typename T>
concept NormalizedInputReplayImeSummaryInterface = requires(T summary) {
    { summary.timeline } -> std::same_as<std::vector<input::normalized_input_replay_ime_timeline_entry>&>;
    { summary.phases } -> std::same_as<input::normalized_input_replay_ime_phase_counts&>;
    { summary.total } -> std::same_as<std::size_t&>;
    { summary.emitted_input_event_routes } -> std::same_as<std::size_t&>;
    { summary.diagnostic_only_routes } -> std::same_as<std::size_t&>;
    { summary.all_preedit_text_valid } -> std::same_as<bool&>;
    { summary.all_preedit_ranges_valid } -> std::same_as<bool&>;
    { summary.stale_preedit_cleared } -> std::same_as<bool&>;
    { summary.final_committed_text } -> std::same_as<std::string&>;
    { summary.final_display_text } -> std::same_as<std::string&>;
    { summary.final_preedit_text } -> std::same_as<std::string&>;
    { summary.final_caret } -> std::same_as<input::text_range&>;
    { summary.final_has_selection } -> std::same_as<bool&>;
    { summary.final_selection } -> std::same_as<input::text_range&>;
    { summary.final_preedit_clean } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputReplayPointerTimelineCountsInterface = requires(T counts) {
    { counts.pointer_capture_arbitration } -> std::same_as<std::size_t&>;
    { counts.pointer_capture_reset } -> std::same_as<std::size_t&>;
    { counts.tap } -> std::same_as<std::size_t&>;
    { counts.long_press } -> std::same_as<std::size_t&>;
    { counts.swipe_left } -> std::same_as<std::size_t&>;
    { counts.swipe_right } -> std::same_as<std::size_t&>;
    { counts.drag_start } -> std::same_as<std::size_t&>;
    { counts.drag_update } -> std::same_as<std::size_t&>;
    { counts.drag_end } -> std::same_as<std::size_t&>;
    { counts.drag_cancel } -> std::same_as<std::size_t&>;
    { counts.wheel } -> std::same_as<std::size_t&>;
    { counts.gesture_suppressed } -> std::same_as<std::size_t&>;
};

template <typename T>
concept NormalizedInputReplayPointerContactCountsInterface = requires(T counts) {
    { counts.unknown } -> std::same_as<std::size_t&>;
    { counts.mouse_like } -> std::same_as<std::size_t&>;
    { counts.touch_like } -> std::same_as<std::size_t&>;
};

template <typename T>
concept NormalizedInputReplayPointerCaptureLifecycleCountsInterface = requires(T counts) {
    { counts.idle } -> std::same_as<std::size_t&>;
    { counts.tracking } -> std::same_as<std::size_t&>;
    { counts.captured } -> std::same_as<std::size_t&>;
};

template <typename T>
concept NormalizedInputReplayPointerDecisionCountsInterface = requires(T counts) {
    { counts.none } -> std::same_as<std::size_t&>;
    { counts.tracked } -> std::same_as<std::size_t&>;
    { counts.captured } -> std::same_as<std::size_t&>;
    { counts.ignored_by_capture } -> std::same_as<std::size_t&>;
    { counts.canceled } -> std::same_as<std::size_t&>;
    { counts.released } -> std::same_as<std::size_t&>;
    { counts.restarted } -> std::same_as<std::size_t&>;
};

template <typename T>
concept NormalizedInputReplayPointerTimelineEntryInterface = requires(T entry) {
    { entry.kind } -> std::same_as<input::normalized_input_replay_pointer_timeline_kind&>;
    { entry.timestamp_ms } -> std::same_as<std::int64_t&>;
    { entry.emits_input_event } -> std::same_as<bool&>;
    { entry.event_index } -> std::same_as<std::size_t&>;
    { entry.pointer_id } -> std::same_as<std::int32_t&>;
    { entry.event_phase } -> std::same_as<input::pointer_phase&>;
    { entry.contact } -> std::same_as<input::pointer_contact_kind&>;
    { entry.decision } -> std::same_as<input::pointer_arbitration_decision&>;
    { entry.capture_before } -> std::same_as<input::pointer_capture_snapshot&>;
    { entry.capture_after } -> std::same_as<input::pointer_capture_snapshot&>;
    { entry.tracked_pointer_count_before } -> std::same_as<std::size_t&>;
    { entry.tracked_pointer_count_after } -> std::same_as<std::size_t&>;
    { entry.normalized_event } -> std::same_as<input::normalized_input_event_summary&>;
    { entry.gesture_policy } -> std::same_as<input::gesture_policy_snapshot&>;
    { entry.capture_changed } -> std::same_as<bool&>;
    { entry.capture_ended_cleanly_after } -> std::same_as<bool&>;
    { entry.duration_ms } -> std::same_as<std::int64_t&>;
    { entry.start_x } -> std::same_as<float&>;
    { entry.start_y } -> std::same_as<float&>;
    { entry.x } -> std::same_as<float&>;
    { entry.y } -> std::same_as<float&>;
    { entry.delta_x } -> std::same_as<float&>;
    { entry.delta_y } -> std::same_as<float&>;
    { entry.pixel_delta_x } -> std::same_as<float&>;
    { entry.pixel_delta_y } -> std::same_as<float&>;
    { entry.line_delta_x } -> std::same_as<float&>;
    { entry.line_delta_y } -> std::same_as<float&>;
};

template <typename T>
concept NormalizedInputReplayPointerSummaryInterface = requires(T summary) {
    { summary.timeline } -> std::same_as<std::vector<input::normalized_input_replay_pointer_timeline_entry>&>;
    { summary.pointer_ids } -> std::same_as<std::vector<std::int32_t>&>;
    { summary.mouse_pointer_ids } -> std::same_as<std::vector<std::int32_t>&>;
    { summary.touch_pointer_ids } -> std::same_as<std::vector<std::int32_t>&>;
    { summary.kinds } -> std::same_as<input::normalized_input_replay_pointer_timeline_counts&>;
    { summary.contacts } -> std::same_as<input::normalized_input_replay_pointer_contact_counts&>;
    { summary.capture_before_lifecycles }
        -> std::same_as<input::normalized_input_replay_pointer_capture_lifecycle_counts&>;
    { summary.capture_after_lifecycles }
        -> std::same_as<input::normalized_input_replay_pointer_capture_lifecycle_counts&>;
    { summary.decisions } -> std::same_as<input::normalized_input_replay_pointer_decision_counts&>;
    { summary.total } -> std::same_as<std::size_t&>;
    { summary.emitted_input_event_routes } -> std::same_as<std::size_t&>;
    { summary.diagnostic_only_routes } -> std::same_as<std::size_t&>;
    { summary.capture_transition_count } -> std::same_as<std::size_t&>;
    { summary.wheel_routes } -> std::same_as<std::size_t&>;
    { summary.saw_multipointer_touch } -> std::same_as<bool&>;
    { summary.final_capture } -> std::same_as<input::pointer_capture_snapshot&>;
    { summary.final_capture_clean } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputReplayFocusTimelineCountsInterface = requires(T counts) {
    { counts.focus_gain } -> std::same_as<std::size_t&>;
    { counts.focus_loss } -> std::same_as<std::size_t&>;
    { counts.focus_traversal_next } -> std::same_as<std::size_t&>;
    { counts.focus_traversal_previous } -> std::same_as<std::size_t&>;
    { counts.caret_moved } -> std::same_as<std::size_t&>;
    { counts.selection_changed } -> std::same_as<std::size_t&>;
};

template <typename T>
concept NormalizedInputReplayFocusTimelineEntryInterface = requires(T entry) {
    { entry.kind } -> std::same_as<input::normalized_input_replay_focus_timeline_kind&>;
    { entry.timestamp_ms } -> std::same_as<std::int64_t&>;
    { entry.emits_input_event } -> std::same_as<bool&>;
    { entry.event_index } -> std::same_as<std::size_t&>;
    { entry.target_id } -> std::same_as<std::string&>;
    { entry.target_id_before } -> std::same_as<std::string&>;
    { entry.target_id_after } -> std::same_as<std::string&>;
    { entry.had_focus_before } -> std::same_as<bool&>;
    { entry.has_focus_after } -> std::same_as<bool&>;
    { entry.target_changed } -> std::same_as<bool&>;
    { entry.text_byte_count_before } -> std::same_as<std::size_t&>;
    { entry.text_byte_count_after } -> std::same_as<std::size_t&>;
    { entry.caret_before } -> std::same_as<input::text_range&>;
    { entry.caret_after } -> std::same_as<input::text_range&>;
    { entry.caret_changed } -> std::same_as<bool&>;
    { entry.had_selection_before } -> std::same_as<bool&>;
    { entry.has_selection_after } -> std::same_as<bool&>;
    { entry.selection_before } -> std::same_as<input::text_range&>;
    { entry.selection_after } -> std::same_as<input::text_range&>;
    { entry.selection_changed } -> std::same_as<bool&>;
    { entry.keyboard } -> std::same_as<input::keyboard_chord_diagnostic&>;
    { entry.focus_clean_after } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputReplayFocusSummaryInterface = requires(T summary) {
    { summary.timeline } -> std::same_as<std::vector<input::normalized_input_replay_focus_timeline_entry>&>;
    { summary.kinds } -> std::same_as<input::normalized_input_replay_focus_timeline_counts&>;
    { summary.total } -> std::same_as<std::size_t&>;
    { summary.emitted_input_event_routes } -> std::same_as<std::size_t&>;
    { summary.diagnostic_only_routes } -> std::same_as<std::size_t&>;
    { summary.target_transition_count } -> std::same_as<std::size_t&>;
    { summary.caret_transition_count } -> std::same_as<std::size_t&>;
    { summary.selection_transition_count } -> std::same_as<std::size_t&>;
    { summary.final_has_focus } -> std::same_as<bool&>;
    { summary.final_focus_id } -> std::same_as<std::string&>;
    { summary.final_text_byte_count } -> std::same_as<std::size_t&>;
    { summary.final_caret } -> std::same_as<input::text_range&>;
    { summary.final_has_selection } -> std::same_as<bool&>;
    { summary.final_selection } -> std::same_as<input::text_range&>;
    { summary.final_focus_clean } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputReplayCountDeltaInterface = requires(T delta) {
    { delta.before_count } -> std::same_as<std::size_t&>;
    { delta.after_count } -> std::same_as<std::size_t&>;
    { delta.delta } -> std::same_as<std::int64_t&>;
    { delta.changed } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputReplayBoolDeltaInterface = requires(T delta) {
    { delta.before_value } -> std::same_as<bool&>;
    { delta.after_value } -> std::same_as<bool&>;
    { delta.changed } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputReplayRangeDeltaInterface = requires(T delta) {
    { delta.before_range } -> std::same_as<input::text_range&>;
    { delta.after_range } -> std::same_as<input::text_range&>;
    { delta.changed } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputReplayStringDeltaInterface = requires(T delta) {
    { delta.before_value } -> std::same_as<std::string&>;
    { delta.after_value } -> std::same_as<std::string&>;
    { delta.byte_delta } -> std::same_as<std::int64_t&>;
    { delta.changed } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputReplayPointerCaptureDeltaInterface = requires(T delta) {
    { delta.before_capture } -> std::same_as<input::pointer_capture_snapshot&>;
    { delta.after_capture } -> std::same_as<input::pointer_capture_snapshot&>;
    { delta.before_clean } -> std::same_as<bool&>;
    { delta.after_clean } -> std::same_as<bool&>;
    { delta.changed } -> std::same_as<bool&>;
    { delta.clean_changed } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputReplayFinalStateDiffInterface = requires(T diff) {
    { diff.has_focus } -> std::same_as<input::normalized_input_replay_bool_delta&>;
    { diff.focus_id } -> std::same_as<input::normalized_input_replay_string_delta&>;
    { diff.text } -> std::same_as<input::normalized_input_replay_string_delta&>;
    { diff.display_text } -> std::same_as<input::normalized_input_replay_string_delta&>;
    { diff.preedit_text } -> std::same_as<input::normalized_input_replay_string_delta&>;
    { diff.caret } -> std::same_as<input::normalized_input_replay_range_delta&>;
    { diff.has_selection } -> std::same_as<input::normalized_input_replay_bool_delta&>;
    { diff.selection } -> std::same_as<input::normalized_input_replay_range_delta&>;
    { diff.focus_clean } -> std::same_as<input::normalized_input_replay_bool_delta&>;
    { diff.preedit_clean } -> std::same_as<input::normalized_input_replay_bool_delta&>;
    { diff.pointer_capture } -> std::same_as<input::normalized_input_replay_pointer_capture_delta&>;
    { diff.text_presentation } -> std::same_as<input::text_input_presentation_diff&>;
    { diff.focus_changed } -> std::same_as<bool&>;
    { diff.caret_changed } -> std::same_as<bool&>;
    { diff.selection_changed } -> std::same_as<bool&>;
    { diff.text_changed } -> std::same_as<bool&>;
    { diff.display_text_changed } -> std::same_as<bool&>;
    { diff.preedit_changed } -> std::same_as<bool&>;
    { diff.pointer_capture_changed } -> std::same_as<bool&>;
    { diff.text_presentation_changed } -> std::same_as<bool&>;
    { diff.changed } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputReplayKeyboardDiffInterface = requires(T diff) {
    { diff.chord_count } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.total } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.emitted_input_event_routes } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.diagnostic_only_routes } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.intents } -> std::same_as<input::normalized_input_replay_keyboard_intent_count_deltas&>;
    { diff.modifiers } -> std::same_as<input::normalized_input_replay_keyboard_modifier_count_deltas&>;
    { diff.repeat_policies }
        -> std::same_as<input::normalized_input_replay_keyboard_repeat_policy_count_deltas&>;
    { diff.changed } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputReplayPointerDiffInterface = requires(T diff) {
    { diff.timeline_entries } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.total } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.emitted_input_event_routes } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.diagnostic_only_routes } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.capture_transition_count } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.wheel_routes } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.pointer_id_count } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.mouse_pointer_id_count } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.touch_pointer_id_count } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.kinds } -> std::same_as<input::normalized_input_replay_pointer_timeline_count_deltas&>;
    { diff.saw_multipointer_touch } -> std::same_as<input::normalized_input_replay_bool_delta&>;
    { diff.final_capture } -> std::same_as<input::normalized_input_replay_pointer_capture_delta&>;
    { diff.timeline_changed } -> std::same_as<bool&>;
    { diff.capture_changed } -> std::same_as<bool&>;
    { diff.changed } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputReplayImeDiffInterface = requires(T diff) {
    { diff.timeline_entries } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.total } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.emitted_input_event_routes } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.diagnostic_only_routes } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.phases } -> std::same_as<input::normalized_input_replay_ime_phase_count_deltas&>;
    { diff.all_preedit_text_valid } -> std::same_as<input::normalized_input_replay_bool_delta&>;
    { diff.all_preedit_ranges_valid } -> std::same_as<input::normalized_input_replay_bool_delta&>;
    { diff.stale_preedit_cleared } -> std::same_as<input::normalized_input_replay_bool_delta&>;
    { diff.final_committed_text } -> std::same_as<input::normalized_input_replay_string_delta&>;
    { diff.final_display_text } -> std::same_as<input::normalized_input_replay_string_delta&>;
    { diff.final_preedit_text } -> std::same_as<input::normalized_input_replay_string_delta&>;
    { diff.final_caret } -> std::same_as<input::normalized_input_replay_range_delta&>;
    { diff.final_has_selection } -> std::same_as<input::normalized_input_replay_bool_delta&>;
    { diff.final_selection } -> std::same_as<input::normalized_input_replay_range_delta&>;
    { diff.final_preedit_clean } -> std::same_as<input::normalized_input_replay_bool_delta&>;
    { diff.timeline_changed } -> std::same_as<bool&>;
    { diff.final_text_changed } -> std::same_as<bool&>;
    { diff.final_preedit_changed } -> std::same_as<bool&>;
    { diff.changed } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputReplayFocusDiffInterface = requires(T diff) {
    { diff.timeline_entries } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.total } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.emitted_input_event_routes } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.diagnostic_only_routes } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.target_transition_count } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.caret_transition_count } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.selection_transition_count } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.kinds } -> std::same_as<input::normalized_input_replay_focus_timeline_count_deltas&>;
    { diff.final_has_focus } -> std::same_as<input::normalized_input_replay_bool_delta&>;
    { diff.final_focus_id } -> std::same_as<input::normalized_input_replay_string_delta&>;
    { diff.final_text_byte_count } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.final_caret } -> std::same_as<input::normalized_input_replay_range_delta&>;
    { diff.final_has_selection } -> std::same_as<input::normalized_input_replay_bool_delta&>;
    { diff.final_selection } -> std::same_as<input::normalized_input_replay_range_delta&>;
    { diff.final_focus_clean } -> std::same_as<input::normalized_input_replay_bool_delta&>;
    { diff.timeline_changed } -> std::same_as<bool&>;
    { diff.final_focus_changed } -> std::same_as<bool&>;
    { diff.final_caret_changed } -> std::same_as<bool&>;
    { diff.final_selection_changed } -> std::same_as<bool&>;
    { diff.changed } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputReplayRegressionSummaryInterface = requires(T summary) {
    { summary.batch_count } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { summary.normalized_event_count } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { summary.route_count } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { summary.final_state_changed } -> std::same_as<bool&>;
    { summary.focus_caret_selection_changed } -> std::same_as<bool&>;
    { summary.pointer_capture_changed } -> std::same_as<bool&>;
    { summary.pointer_timeline_changed } -> std::same_as<bool&>;
    { summary.gesture_policy_changed } -> std::same_as<bool&>;
    { summary.ime_timeline_changed } -> std::same_as<bool&>;
    { summary.keyboard_changed } -> std::same_as<bool&>;
    { summary.text_or_preedit_changed } -> std::same_as<bool&>;
    { summary.focus_timeline_changed } -> std::same_as<bool&>;
    { summary.changed_category_count } -> std::same_as<std::size_t&>;
    { summary.changed } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputReplayDiffInterface = requires(T diff) {
    { diff.final_state } -> std::same_as<input::normalized_input_replay_final_state_diff&>;
    { diff.keyboard } -> std::same_as<input::normalized_input_replay_keyboard_diff&>;
    { diff.pointer } -> std::same_as<input::normalized_input_replay_pointer_diff&>;
    { diff.gesture_policies } -> std::same_as<input::input_routing_gesture_policy_diff&>;
    { diff.ime } -> std::same_as<input::normalized_input_replay_ime_diff&>;
    { diff.focus } -> std::same_as<input::normalized_input_replay_focus_diff&>;
    { diff.regression } -> std::same_as<input::normalized_input_replay_regression_summary&>;
};

template <typename T>
concept NormalizedInputReplayFunctions = requires(
    input::input_engine& engine,
    const input::normalized_input_replay_action& action,
    std::span<const input::normalized_input_replay_step> steps,
    std::span<const input::action_route_policy_diagnostic> routes,
    std::span<const input::input_event> events,
    const input::action_route_policy_diagnostic& route,
    input::action_route_policy_kind route_kind,
    input::action_route_policy_kind focus_route_kind,
    input::gesture_kind gesture_kind,
    input::input_event_summary_kind event_summary_kind,
    input::text_event_kind text_event_kind,
    const input::gesture_policy_snapshot& gesture_policy,
    const input::pointer_capture_snapshot& pointer_capture_before,
    const input::pointer_capture_snapshot& pointer_capture_after,
    const input::keyboard_chord_diagnostic& keyboard,
    const input::ime_composition_state& composition,
    const input::normalized_input_replay_end_state& end_state,
    input::normalized_input_replay_keyboard_summary& keyboard_target,
    const input::normalized_input_replay_keyboard_summary& keyboard_source,
    input::normalized_input_replay_ime_summary& ime_target,
    const input::normalized_input_replay_ime_summary& ime_source,
    input::normalized_input_replay_pointer_summary& pointer_target,
    const input::normalized_input_replay_pointer_summary& pointer_source,
    input::normalized_input_replay_gesture_policy_summary& gesture_policy_target,
    const input::normalized_input_replay_gesture_policy_summary& gesture_policy_source,
    input::normalized_input_replay_focus_summary& focus_target,
    const input::normalized_input_replay_focus_summary& focus_source,
    const input::normalized_input_replay_recording& before_recording,
    const input::normalized_input_replay_recording& after_recording,
    const input::normalized_input_replay_options& options) {
    { input::pointer_capture_snapshot_clean(input::pointer_capture_snapshot{}) } -> std::same_as<bool>;
    { input::keyboard_chord_present(keyboard) } -> std::same_as<bool>;
    { input::normalized_input_replay_preedit_text_valid(composition) } -> std::same_as<bool>;
    { input::normalized_input_replay_composition_range_valid(composition) } -> std::same_as<bool>;
    { input::normalized_input_replay_size_delta(std::size_t{}, std::size_t{}) } -> std::same_as<std::int64_t>;
    { input::normalized_input_replay_diff_count(std::size_t{}, std::size_t{}) }
        -> std::same_as<input::normalized_input_replay_count_delta>;
    { input::normalized_input_replay_diff_bool(false, true) }
        -> std::same_as<input::normalized_input_replay_bool_delta>;
    { input::normalized_input_replay_diff_range(input::text_range{}, input::text_range{}) }
        -> std::same_as<input::normalized_input_replay_range_delta>;
    { input::normalized_input_replay_diff_string(std::string{}, std::string{}) }
        -> std::same_as<input::normalized_input_replay_string_delta>;
    { input::normalized_input_replay_diff_pointer_capture(
        input::pointer_capture_snapshot{},
        input::pointer_capture_snapshot{}) } -> std::same_as<input::normalized_input_replay_pointer_capture_delta>;
    { input::normalized_input_replay_caret_range_for_offset(std::size_t{}) }
        -> std::same_as<input::text_range>;
    { input::normalized_input_replay_same_text_range(input::text_range{}, input::text_range{}) }
        -> std::same_as<bool>;
    { input::normalized_input_replay_focus_route_kind(focus_route_kind) } -> std::same_as<bool>;
    { input::normalized_input_replay_focus_event_kind(text_event_kind) } -> std::same_as<bool>;
    { input::normalized_input_replay_focus_kind_for_route(focus_route_kind) }
        -> std::same_as<input::normalized_input_replay_focus_timeline_kind>;
    { input::normalized_input_replay_focus_kind_for_event(text_event_kind) }
        -> std::same_as<input::normalized_input_replay_focus_timeline_kind>;
    { input::normalized_input_replay_pointer_route_kind(route_kind) } -> std::same_as<bool>;
    { input::normalized_input_replay_pointer_kind_for_gesture(gesture_kind) }
        -> std::same_as<input::normalized_input_replay_pointer_timeline_kind>;
    { input::normalized_input_replay_pointer_kind_for_summary(event_summary_kind) }
        -> std::same_as<input::normalized_input_replay_pointer_timeline_kind>;
    { input::normalized_input_replay_pointer_kind_for_policy(gesture_policy) }
        -> std::same_as<input::normalized_input_replay_pointer_timeline_kind>;
    { input::normalized_input_replay_pointer_kind_for_route(route) }
        -> std::same_as<input::normalized_input_replay_pointer_timeline_kind>;
    { input::normalized_input_replay_pointer_capture_changed(pointer_capture_before, pointer_capture_after) }
        -> std::same_as<bool>;
    { input::normalized_input_replay_pointer_id_for_route(route) } -> std::same_as<std::int32_t>;
    { input::summarize_normalized_input_replay_ime_routes(routes, events, end_state) }
        -> std::same_as<input::normalized_input_replay_ime_summary>;
    { input::accumulate_normalized_input_replay_ime_summary(ime_target, ime_source) }
        -> std::same_as<void>;
    { input::summarize_normalized_input_replay_pointer_routes(routes, end_state) }
        -> std::same_as<input::normalized_input_replay_pointer_summary>;
    { input::accumulate_normalized_input_replay_pointer_summary(pointer_target, pointer_source) }
        -> std::same_as<void>;
    { input::summarize_normalized_input_replay_gesture_policy_routes(routes) }
        -> std::same_as<input::normalized_input_replay_gesture_policy_summary>;
    { input::accumulate_normalized_input_replay_gesture_policy_summary(
        gesture_policy_target,
        gesture_policy_source) } -> std::same_as<void>;
    { input::normalized_input_replay_gesture_policy_diagnostics(gesture_policy_source) }
        -> std::same_as<input::input_routing_diagnostics>;
    { input::summarize_normalized_input_replay_focus_routes(routes, events, end_state, end_state) }
        -> std::same_as<input::normalized_input_replay_focus_summary>;
    { input::accumulate_normalized_input_replay_focus_summary(focus_target, focus_source) }
        -> std::same_as<void>;
    { input::diff_normalized_input_replay_final_state(end_state, end_state) }
        -> std::same_as<input::normalized_input_replay_final_state_diff>;
    { input::diff_normalized_input_replay_keyboard(keyboard_source, keyboard_source) }
        -> std::same_as<input::normalized_input_replay_keyboard_diff>;
    { input::diff_normalized_input_replay_pointer(pointer_source, pointer_source) }
        -> std::same_as<input::normalized_input_replay_pointer_diff>;
    { input::diff_normalized_input_replay_ime(ime_source, ime_source) }
        -> std::same_as<input::normalized_input_replay_ime_diff>;
    { input::diff_normalized_input_replay_focus(focus_source, focus_source) }
        -> std::same_as<input::normalized_input_replay_focus_diff>;
    { input::diff_normalized_input_replay_recordings(before_recording, after_recording) }
        -> std::same_as<input::normalized_input_replay_diff>;
    { input::summarize_normalized_input_replay_keyboard_routes(routes) }
        -> std::same_as<input::normalized_input_replay_keyboard_summary>;
    { input::accumulate_normalized_input_replay_keyboard_summary(keyboard_target, keyboard_source) }
        -> std::same_as<void>;
    { input::capture_normalized_input_replay_end_state(engine) }
        -> std::same_as<input::normalized_input_replay_end_state>;
    { input::record_normalized_input_batch(engine, std::string{}, action) }
        -> std::same_as<input::normalized_input_replay_batch>;
    { input::replay_normalized_input_fixture(engine, steps, options) }
        -> std::same_as<input::normalized_input_replay_recording>;
};

template <typename T>
concept InputActionCandidateCountsInterface = requires(T counts) {
    { counts.text_edit } -> std::same_as<std::size_t&>;
    { counts.focus_move } -> std::same_as<std::size_t&>;
    { counts.pointer_capture } -> std::same_as<std::size_t&>;
    { counts.gesture_candidate } -> std::same_as<std::size_t&>;
    { counts.wheel_scroll } -> std::same_as<std::size_t&>;
    { counts.ime_composition_start } -> std::same_as<std::size_t&>;
    { counts.ime_preedit } -> std::same_as<std::size_t&>;
    { counts.ime_commit } -> std::same_as<std::size_t&>;
    { counts.ime_cancel } -> std::same_as<std::size_t&>;
    { counts.total } -> std::same_as<std::size_t&>;
};

template <typename T>
concept InputActionCandidateInterface = requires(T candidate) {
    { candidate.kind } -> std::same_as<input::input_action_candidate_kind&>;
    { candidate.batch_label } -> std::same_as<std::string&>;
    { candidate.batch_index } -> std::same_as<std::size_t&>;
    { candidate.timeline_index } -> std::same_as<std::size_t&>;
    { candidate.timestamp_ms } -> std::same_as<std::int64_t&>;
    { candidate.emits_input_event } -> std::same_as<bool&>;
    { candidate.event_index } -> std::same_as<std::size_t&>;
    { candidate.target_id } -> std::same_as<std::string&>;
    { candidate.focus_kind } -> std::same_as<input::normalized_input_replay_focus_timeline_kind&>;
    { candidate.target_id_before } -> std::same_as<std::string&>;
    { candidate.target_id_after } -> std::same_as<std::string&>;
    { candidate.had_focus_before } -> std::same_as<bool&>;
    { candidate.has_focus_after } -> std::same_as<bool&>;
    { candidate.target_changed } -> std::same_as<bool&>;
    { candidate.pointer_kind } -> std::same_as<input::normalized_input_replay_pointer_timeline_kind&>;
    { candidate.pointer_id } -> std::same_as<std::int32_t&>;
    { candidate.event_phase } -> std::same_as<input::pointer_phase&>;
    { candidate.pointer_contact } -> std::same_as<input::pointer_contact_kind&>;
    { candidate.pointer_decision } -> std::same_as<input::pointer_arbitration_decision&>;
    { candidate.capture_before } -> std::same_as<input::pointer_capture_snapshot&>;
    { candidate.capture_after } -> std::same_as<input::pointer_capture_snapshot&>;
    { candidate.capture_changed } -> std::same_as<bool&>;
    { candidate.capture_ended_cleanly_after } -> std::same_as<bool&>;
    { candidate.tracked_pointer_count_before } -> std::same_as<std::size_t&>;
    { candidate.tracked_pointer_count_after } -> std::same_as<std::size_t&>;
    { candidate.ime_phase } -> std::same_as<input::normalized_input_replay_ime_timeline_phase&>;
    { candidate.composition } -> std::same_as<input::ime_composition_state&>;
    { candidate.utf8_text } -> std::same_as<std::string&>;
    { candidate.committed_text } -> std::same_as<std::string&>;
    { candidate.preedit_text_valid } -> std::same_as<bool&>;
    { candidate.preedit_range_valid } -> std::same_as<bool&>;
    { candidate.stale_preedit_cleared_after } -> std::same_as<bool&>;
    { candidate.keyboard } -> std::same_as<input::keyboard_chord_diagnostic&>;
    { candidate.normalized_event } -> std::same_as<input::normalized_input_event_summary&>;
    { candidate.gesture_policy } -> std::same_as<input::gesture_policy_snapshot&>;
    { candidate.text_presentation_diff } -> std::same_as<input::text_input_presentation_diff&>;
    { candidate.end_state } -> std::same_as<input::normalized_input_replay_end_state&>;
    { candidate.text_byte_count } -> std::same_as<std::size_t&>;
    { candidate.text_byte_count_before } -> std::same_as<std::size_t&>;
    { candidate.text_byte_count_after } -> std::same_as<std::size_t&>;
    { candidate.caret_before } -> std::same_as<input::text_range&>;
    { candidate.caret_after } -> std::same_as<input::text_range&>;
    { candidate.caret_changed } -> std::same_as<bool&>;
    { candidate.had_selection_before } -> std::same_as<bool&>;
    { candidate.has_selection_after } -> std::same_as<bool&>;
    { candidate.selection_before } -> std::same_as<input::text_range&>;
    { candidate.selection_after } -> std::same_as<input::text_range&>;
    { candidate.selection_changed } -> std::same_as<bool&>;
    { candidate.duration_ms } -> std::same_as<std::int64_t&>;
    { candidate.start_x } -> std::same_as<float&>;
    { candidate.start_y } -> std::same_as<float&>;
    { candidate.x } -> std::same_as<float&>;
    { candidate.y } -> std::same_as<float&>;
    { candidate.delta_x } -> std::same_as<float&>;
    { candidate.delta_y } -> std::same_as<float&>;
    { candidate.pixel_delta_x } -> std::same_as<float&>;
    { candidate.pixel_delta_y } -> std::same_as<float&>;
    { candidate.line_delta_x } -> std::same_as<float&>;
    { candidate.line_delta_y } -> std::same_as<float&>;
};

template <typename T>
concept InputActionCandidateBatchPlanInterface = requires(T batch_plan) {
    { batch_plan.label } -> std::same_as<std::string&>;
    { batch_plan.candidates } -> std::same_as<std::vector<input::input_action_candidate>&>;
    { batch_plan.counts } -> std::same_as<input::input_action_candidate_counts&>;
};

template <typename T>
concept InputActionCandidatePlanInterface = requires(T plan) {
    { plan.batches } -> std::same_as<std::vector<input::input_action_candidate_batch_plan>&>;
    { plan.candidates } -> std::same_as<std::vector<input::input_action_candidate>&>;
    { plan.counts } -> std::same_as<input::input_action_candidate_counts&>;
    { plan.final_state } -> std::same_as<input::normalized_input_replay_end_state&>;
};

template <typename T>
concept InputActionCandidatePlanFunctions = requires(
    input::input_action_candidate_counts& counts,
    input::input_action_candidate_kind candidate_kind,
    input::normalized_input_replay_ime_timeline_phase ime_phase,
    input::normalized_input_replay_pointer_timeline_kind pointer_kind,
    const input::input_event& event,
    input::input_action_candidate_plan& plan,
    input::input_action_candidate_batch_plan& batch_plan,
    input::input_action_candidate candidate,
    const input::normalized_input_replay_batch& batch,
    const input::normalized_input_replay_focus_timeline_entry& focus_entry,
    const input::normalized_input_replay_ime_timeline_entry& ime_entry,
    const input::normalized_input_replay_pointer_timeline_entry& pointer_entry,
    std::span<const input::normalized_input_replay_batch> batches,
    const input::normalized_input_replay_recording& recording) {
    { input::count_input_action_candidate_kind(counts, candidate_kind) } -> std::same_as<void>;
    { input::input_action_candidate_kind_for_ime_phase(ime_phase) }
        -> std::same_as<input::input_action_candidate_kind>;
    { input::input_action_candidate_pointer_kind_is_capture(pointer_kind) } -> std::same_as<bool>;
    { input::input_action_candidate_pointer_kind_is_wheel(pointer_kind) } -> std::same_as<bool>;
    { input::input_action_candidate_pointer_kind_is_gesture(pointer_kind) } -> std::same_as<bool>;
    { input::input_action_candidate_timestamp_for_event(event) } -> std::same_as<std::int64_t>;
    { input::input_action_candidate_timestamp_for_batch(batch) } -> std::same_as<std::int64_t>;
    { input::input_action_candidate_batch_has_text_edit(batch) } -> std::same_as<bool>;
    { input::append_input_action_candidate(plan, batch_plan, std::move(candidate)) } -> std::same_as<void>;
    { input::make_text_edit_input_action_candidate(batch, std::size_t{}) }
        -> std::same_as<input::input_action_candidate>;
    { input::make_focus_move_input_action_candidate(batch, focus_entry, std::size_t{}, std::size_t{}) }
        -> std::same_as<input::input_action_candidate>;
    { input::make_ime_input_action_candidate(batch, ime_entry, std::size_t{}, std::size_t{}) }
        -> std::same_as<input::input_action_candidate>;
    { input::make_pointer_input_action_candidate(batch, pointer_entry, candidate_kind, std::size_t{}, std::size_t{}) }
        -> std::same_as<input::input_action_candidate>;
    { input::append_input_action_candidates_for_batch(plan, batch_plan, batch, std::size_t{}) }
        -> std::same_as<void>;
    { input::plan_input_action_candidates_for_batch(batch, std::size_t{}) }
        -> std::same_as<input::input_action_candidate_batch_plan>;
    { input::plan_input_action_candidates(batches) } -> std::same_as<input::input_action_candidate_plan>;
    { input::plan_input_action_candidates(recording) } -> std::same_as<input::input_action_candidate_plan>;
};

template <typename T>
concept InputActionCandidateResultCountsInterface = requires(T counts) {
    { counts.selected } -> std::same_as<input::input_action_candidate_counts&>;
    { counts.supporting_evidence } -> std::same_as<input::input_action_candidate_counts&>;
    { counts.diagnostic_only } -> std::same_as<input::input_action_candidate_counts&>;
    { counts.rejected } -> std::same_as<input::input_action_candidate_counts&>;
    { counts.total } -> std::same_as<std::size_t&>;
};

template <typename T>
concept InputActionCandidateResultInterface = requires(T result) {
    { result.candidate } -> std::same_as<input::input_action_candidate&>;
    { result.status } -> std::same_as<input::input_action_candidate_result_status&>;
    { result.reason } -> std::same_as<input::input_action_candidate_result_reason&>;
    { result.candidate_index } -> std::same_as<std::size_t&>;
    { result.selected_candidate_index } -> std::same_as<std::size_t&>;
    { result.priority } -> std::same_as<int&>;
    { result.selected } -> std::same_as<bool&>;
    { result.supports_selected_candidate } -> std::same_as<bool&>;
    { result.emits_input_event } -> std::same_as<bool&>;
    { result.has_text_delta } -> std::same_as<bool&>;
    { result.has_focus_transition } -> std::same_as<bool&>;
    { result.has_pointer_capture_transition } -> std::same_as<bool&>;
    { result.has_gesture_event } -> std::same_as<bool&>;
    { result.has_wheel_delta } -> std::same_as<bool&>;
    { result.has_ime_payload } -> std::same_as<bool&>;
    { result.evidence_clean } -> std::same_as<bool&>;
};

template <typename T>
concept InputActionCandidateBatchResolutionInterface = requires(T batch_resolution) {
    { batch_resolution.label } -> std::same_as<std::string&>;
    { batch_resolution.results } -> std::same_as<std::vector<input::input_action_candidate_result>&>;
    { batch_resolution.counts } -> std::same_as<input::input_action_candidate_result_counts&>;
    { batch_resolution.selected_candidate_index } -> std::same_as<std::size_t&>;
    { batch_resolution.has_selected_candidate } -> std::same_as<bool&>;
};

template <typename T>
concept InputActionCandidateResolutionInterface = requires(T resolution) {
    { resolution.batches } -> std::same_as<std::vector<input::input_action_candidate_batch_resolution>&>;
    { resolution.results } -> std::same_as<std::vector<input::input_action_candidate_result>&>;
    { resolution.counts } -> std::same_as<input::input_action_candidate_result_counts&>;
    { resolution.final_state } -> std::same_as<input::normalized_input_replay_end_state&>;
};

template <typename T>
concept InputActionCandidateResolutionFunctions = requires(
    input::input_action_candidate_counts& candidate_counts,
    const input::input_action_candidate_counts& candidate_count_source,
    input::input_action_candidate_result_counts& result_counts,
    const input::input_action_candidate_result_counts& result_count_source,
    const input::input_action_candidate_result& result,
    input::input_action_candidate candidate,
    const input::pointer_capture_snapshot& capture_snapshot,
    input::gesture_policy_decision gesture_decision,
    input::input_action_candidate_batch_resolution& batch_resolution,
    input::input_action_candidate_resolution& resolution,
    input::input_action_candidate_batch_resolution batch_resolution_value,
    const input::input_action_candidate_batch_plan& batch_plan,
    const input::input_action_candidate_plan& plan,
    std::span<const input::normalized_input_replay_batch> batches,
    const input::normalized_input_replay_recording& recording) {
    { input::count_input_action_candidate_result(result_counts, result) } -> std::same_as<void>;
    { input::accumulate_input_action_candidate_counts(candidate_counts, candidate_count_source) }
        -> std::same_as<void>;
    { input::accumulate_input_action_candidate_result_counts(result_counts, result_count_source) }
        -> std::same_as<void>;
    { input::input_action_candidate_text_has_delta(candidate) } -> std::same_as<bool>;
    { input::input_action_candidate_focus_has_transition(candidate) } -> std::same_as<bool>;
    { input::input_action_candidate_capture_snapshot_changed(capture_snapshot, capture_snapshot) }
        -> std::same_as<bool>;
    { input::input_action_candidate_pointer_has_capture_transition(candidate) } -> std::same_as<bool>;
    { input::input_action_candidate_wheel_has_delta(candidate) } -> std::same_as<bool>;
    { input::input_action_candidate_gesture_emits_event(candidate) } -> std::same_as<bool>;
    { input::input_action_candidate_gesture_policy_rejected(gesture_decision) } -> std::same_as<bool>;
    { input::input_action_candidate_gesture_policy_suppressed(gesture_decision) } -> std::same_as<bool>;
    { input::input_action_candidate_ime_has_payload(candidate) } -> std::same_as<bool>;
    { input::input_action_candidate_ime_evidence_clean(candidate) } -> std::same_as<bool>;
    { input::input_action_candidate_result_priority(candidate.kind) } -> std::same_as<int>;
    { input::resolve_input_action_candidate(candidate, std::size_t{}) }
        -> std::same_as<input::input_action_candidate_result>;
    { input::input_action_candidate_result_is_primary_candidate(result) } -> std::same_as<bool>;
    { input::input_action_candidate_result_precedes(result, result) } -> std::same_as<bool>;
    { input::finalize_input_action_candidate_batch_resolution(batch_resolution) } -> std::same_as<void>;
    { input::resolve_input_action_candidate_batch(batch_plan) }
        -> std::same_as<input::input_action_candidate_batch_resolution>;
    { input::append_input_action_candidate_batch_resolution(resolution, std::move(batch_resolution_value)) }
        -> std::same_as<void>;
    { input::resolve_input_action_candidate_plan(plan) }
        -> std::same_as<input::input_action_candidate_resolution>;
    { input::resolve_input_action_candidates(batches) }
        -> std::same_as<input::input_action_candidate_resolution>;
    { input::resolve_input_action_candidates(recording) }
        -> std::same_as<input::input_action_candidate_resolution>;
};

template <typename T>
concept InputActionResolutionResultSummaryInterface = requires(T summary) {
    { summary.kind } -> std::same_as<input::input_action_candidate_kind&>;
    { summary.status } -> std::same_as<input::input_action_candidate_result_status&>;
    { summary.reason } -> std::same_as<input::input_action_candidate_result_reason&>;
    { summary.batch_label } -> std::same_as<std::string&>;
    { summary.batch_index } -> std::same_as<std::size_t&>;
    { summary.candidate_index } -> std::same_as<std::size_t&>;
    { summary.selected_candidate_index } -> std::same_as<std::size_t&>;
    { summary.event_index } -> std::same_as<std::size_t&>;
    { summary.priority } -> std::same_as<int&>;
    { summary.timestamp_ms } -> std::same_as<std::int64_t&>;
    { summary.selected } -> std::same_as<bool&>;
    { summary.supports_selected_candidate } -> std::same_as<bool&>;
    { summary.emits_input_event } -> std::same_as<bool&>;
    { summary.evidence_clean } -> std::same_as<bool&>;
    { summary.has_text_delta } -> std::same_as<bool&>;
    { summary.has_focus_transition } -> std::same_as<bool&>;
    { summary.has_pointer_capture_transition } -> std::same_as<bool&>;
    { summary.has_gesture_event } -> std::same_as<bool&>;
    { summary.has_wheel_delta } -> std::same_as<bool&>;
    { summary.has_ime_payload } -> std::same_as<bool&>;
    { summary.target_id } -> std::same_as<std::string&>;
    { summary.target_id_before } -> std::same_as<std::string&>;
    { summary.target_id_after } -> std::same_as<std::string&>;
    { summary.target_changed } -> std::same_as<bool&>;
    { summary.had_focus_before } -> std::same_as<bool&>;
    { summary.has_focus_after } -> std::same_as<bool&>;
    { summary.focus_kind } -> std::same_as<input::normalized_input_replay_focus_timeline_kind&>;
    { summary.keyboard_intent } -> std::same_as<input::keyboard_shortcut_intent&>;
    { summary.text_byte_count } -> std::same_as<std::size_t&>;
    { summary.text_byte_count_before } -> std::same_as<std::size_t&>;
    { summary.text_byte_count_after } -> std::same_as<std::size_t&>;
    { summary.text_byte_delta } -> std::same_as<std::int64_t&>;
    { summary.caret_before } -> std::same_as<input::text_range&>;
    { summary.caret_after } -> std::same_as<input::text_range&>;
    { summary.caret_changed } -> std::same_as<bool&>;
    { summary.had_selection_before } -> std::same_as<bool&>;
    { summary.has_selection_after } -> std::same_as<bool&>;
    { summary.selection_before } -> std::same_as<input::text_range&>;
    { summary.selection_after } -> std::same_as<input::text_range&>;
    { summary.selection_changed } -> std::same_as<bool&>;
    { summary.ime_phase } -> std::same_as<input::normalized_input_replay_ime_timeline_phase&>;
    { summary.utf8_text_bytes } -> std::same_as<std::size_t&>;
    { summary.committed_text_bytes } -> std::same_as<std::size_t&>;
    { summary.preedit_text_valid } -> std::same_as<bool&>;
    { summary.preedit_range_valid } -> std::same_as<bool&>;
    { summary.stale_preedit_cleared_after } -> std::same_as<bool&>;
    { summary.ime_composition_active } -> std::same_as<bool&>;
    { summary.pointer_kind } -> std::same_as<input::normalized_input_replay_pointer_timeline_kind&>;
    { summary.pointer_id } -> std::same_as<std::int32_t&>;
    { summary.event_phase } -> std::same_as<input::pointer_phase&>;
    { summary.pointer_contact } -> std::same_as<input::pointer_contact_kind&>;
    { summary.pointer_decision } -> std::same_as<input::pointer_arbitration_decision&>;
    { summary.capture_before_lifecycle } -> std::same_as<input::pointer_capture_lifecycle&>;
    { summary.capture_after_lifecycle } -> std::same_as<input::pointer_capture_lifecycle&>;
    { summary.capture_before_active } -> std::same_as<bool&>;
    { summary.capture_after_active } -> std::same_as<bool&>;
    { summary.capture_changed } -> std::same_as<bool&>;
    { summary.capture_ended_cleanly_after } -> std::same_as<bool&>;
    { summary.tracked_pointer_count_before } -> std::same_as<std::size_t&>;
    { summary.tracked_pointer_count_after } -> std::same_as<std::size_t&>;
    { summary.gesture_decision } -> std::same_as<input::gesture_policy_decision&>;
    { summary.gesture_direction_hint } -> std::same_as<input::gesture_direction&>;
    { summary.normalized_event_kind } -> std::same_as<input::input_event_summary_kind&>;
    { summary.duration_ms } -> std::same_as<std::int64_t&>;
    { summary.start_x } -> std::same_as<float&>;
    { summary.start_y } -> std::same_as<float&>;
    { summary.x } -> std::same_as<float&>;
    { summary.y } -> std::same_as<float&>;
    { summary.delta_x } -> std::same_as<float&>;
    { summary.delta_y } -> std::same_as<float&>;
    { summary.pixel_delta_x } -> std::same_as<float&>;
    { summary.pixel_delta_y } -> std::same_as<float&>;
    { summary.line_delta_x } -> std::same_as<float&>;
    { summary.line_delta_y } -> std::same_as<float&>;
};

template <typename T>
concept InputActionResolutionBatchSummaryInterface = requires(T summary) {
    { summary.label } -> std::same_as<std::string&>;
    { summary.selected } -> std::same_as<std::vector<input::input_action_resolution_result_summary>&>;
    { summary.supporting_evidence }
        -> std::same_as<std::vector<input::input_action_resolution_result_summary>&>;
    { summary.rejected } -> std::same_as<std::vector<input::input_action_resolution_result_summary>&>;
    { summary.diagnostic_only }
        -> std::same_as<std::vector<input::input_action_resolution_result_summary>&>;
    { summary.counts } -> std::same_as<input::input_action_candidate_result_counts&>;
    { summary.selected_candidate_index } -> std::same_as<std::size_t&>;
    { summary.has_selected_candidate } -> std::same_as<bool&>;
};

template <typename T>
concept InputActionResolutionReplaySummaryInterface = requires(T summary) {
    { summary.batches } -> std::same_as<std::vector<input::input_action_resolution_batch_summary>&>;
    { summary.selected } -> std::same_as<std::vector<input::input_action_resolution_result_summary>&>;
    { summary.supporting_evidence }
        -> std::same_as<std::vector<input::input_action_resolution_result_summary>&>;
    { summary.rejected } -> std::same_as<std::vector<input::input_action_resolution_result_summary>&>;
    { summary.diagnostic_only }
        -> std::same_as<std::vector<input::input_action_resolution_result_summary>&>;
    { summary.counts } -> std::same_as<input::input_action_candidate_result_counts&>;
    { summary.final_state } -> std::same_as<input::normalized_input_replay_end_state&>;
};

template <typename T>
concept InputActionResolutionSummaryFunctions = requires(
    std::size_t before_count,
    std::size_t after_count,
    const input::input_action_candidate_result& result,
    input::input_action_resolution_batch_summary& batch_summary,
    input::input_action_resolution_result_summary result_summary,
    const input::input_action_candidate_batch_resolution& batch_resolution,
    input::input_action_resolution_replay_summary& replay_summary,
    input::input_action_resolution_batch_summary batch_summary_value,
    const input::input_action_candidate_resolution& resolution,
    std::span<const input::normalized_input_replay_batch> batches,
    const input::normalized_input_replay_recording& recording) {
    { input::input_action_resolution_byte_delta(before_count, after_count) }
        -> std::same_as<std::int64_t>;
    { input::summarize_input_action_candidate_result(result) }
        -> std::same_as<input::input_action_resolution_result_summary>;
    { input::append_input_action_resolution_result_summary(batch_summary, std::move(result_summary)) }
        -> std::same_as<void>;
    { input::summarize_input_action_candidate_batch_resolution(batch_resolution) }
        -> std::same_as<input::input_action_resolution_batch_summary>;
    { input::append_input_action_resolution_batch_summary(replay_summary, std::move(batch_summary_value)) }
        -> std::same_as<void>;
    { input::summarize_input_action_candidate_resolution(resolution) }
        -> std::same_as<input::input_action_resolution_replay_summary>;
    { input::summarize_input_action_resolution_replay(batches) }
        -> std::same_as<input::input_action_resolution_replay_summary>;
    { input::summarize_input_action_resolution_replay(recording) }
        -> std::same_as<input::input_action_resolution_replay_summary>;
};

template <typename T>
concept InputActionCandidateCountDeltasInterface = requires(T deltas) {
    { deltas.text_edit } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.focus_move } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.pointer_capture } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.gesture_candidate } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.wheel_scroll } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.ime_composition_start } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.ime_preedit } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.ime_commit } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.ime_cancel } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.total } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.changed } -> std::same_as<bool&>;
};

template <typename T>
concept InputActionCandidateResultCountDeltasInterface = requires(T deltas) {
    { deltas.selected } -> std::same_as<input::input_action_candidate_count_deltas&>;
    { deltas.supporting_evidence } -> std::same_as<input::input_action_candidate_count_deltas&>;
    { deltas.diagnostic_only } -> std::same_as<input::input_action_candidate_count_deltas&>;
    { deltas.rejected } -> std::same_as<input::input_action_candidate_count_deltas&>;
    { deltas.total } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.changed } -> std::same_as<bool&>;
};

template <typename T>
concept InputActionResolutionReasonCountsInterface = requires(T counts) {
    { counts.text_edit_diff } -> std::same_as<std::size_t&>;
    { counts.focus_transition } -> std::same_as<std::size_t&>;
    { counts.pointer_capture_transition } -> std::same_as<std::size_t&>;
    { counts.pointer_capture_supports_selected } -> std::same_as<std::size_t&>;
    { counts.wheel_delta } -> std::same_as<std::size_t&>;
    { counts.gesture_emitted } -> std::same_as<std::size_t&>;
    { counts.gesture_policy_rejected } -> std::same_as<std::size_t&>;
    { counts.gesture_policy_suppressed } -> std::same_as<std::size_t&>;
    { counts.ime_composition_started } -> std::same_as<std::size_t&>;
    { counts.ime_preedit_valid } -> std::same_as<std::size_t&>;
    { counts.ime_preedit_invalid } -> std::same_as<std::size_t&>;
    { counts.ime_committed } -> std::same_as<std::size_t&>;
    { counts.ime_commit_invalid } -> std::same_as<std::size_t&>;
    { counts.ime_canceled } -> std::same_as<std::size_t&>;
    { counts.ime_cancel_invalid } -> std::same_as<std::size_t&>;
    { counts.coalesced_by_selected_candidate } -> std::same_as<std::size_t&>;
    { counts.no_observable_delta } -> std::same_as<std::size_t&>;
    { counts.total } -> std::same_as<std::size_t&>;
};

template <typename T>
concept InputActionResolutionReasonCountDeltasInterface = requires(T deltas) {
    { deltas.text_edit_diff } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.focus_transition } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.pointer_capture_transition } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.pointer_capture_supports_selected }
        -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.wheel_delta } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.gesture_emitted } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.gesture_policy_rejected } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.gesture_policy_suppressed } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.ime_composition_started } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.ime_preedit_valid } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.ime_preedit_invalid } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.ime_committed } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.ime_commit_invalid } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.ime_canceled } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.ime_cancel_invalid } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.coalesced_by_selected_candidate }
        -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.no_observable_delta } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.total } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { deltas.changed } -> std::same_as<bool&>;
};

template <typename T>
concept InputActionResolutionTargetSnapshotInterface = requires(T snapshot) {
    { snapshot.present } -> std::same_as<bool&>;
    { snapshot.target_id } -> std::same_as<std::string&>;
};

template <typename T>
concept InputActionResolutionTargetDeltaInterface = requires(T delta) {
    { delta.present } -> std::same_as<input::normalized_input_replay_bool_delta&>;
    { delta.target_id } -> std::same_as<input::normalized_input_replay_string_delta&>;
    { delta.changed } -> std::same_as<bool&>;
};

template <typename T>
concept InputActionResolutionReplayClassificationInterface = requires(T classification) {
    { classification.change_class }
        -> std::same_as<input::input_action_resolution_replay_change_class&>;
    { classification.focus_target_churn } -> std::same_as<bool&>;
    { classification.text_target_churn } -> std::same_as<bool&>;
    { classification.selected_action_lost } -> std::same_as<bool&>;
    { classification.selected_action_gained } -> std::same_as<bool&>;
    { classification.supporting_evidence_lost } -> std::same_as<bool&>;
    { classification.supporting_evidence_gained } -> std::same_as<bool&>;
    { classification.rejected_count_changed } -> std::same_as<bool&>;
    { classification.rejected_count_lost } -> std::same_as<bool&>;
    { classification.rejected_count_gained } -> std::same_as<bool&>;
    { classification.no_observable_delta_changed } -> std::same_as<bool&>;
    { classification.no_observable_delta_lost } -> std::same_as<bool&>;
    { classification.no_observable_delta_gained } -> std::same_as<bool&>;
    { classification.has_churn } -> std::same_as<bool&>;
    { classification.has_regression } -> std::same_as<bool&>;
    { classification.has_improvement } -> std::same_as<bool&>;
    { classification.changed } -> std::same_as<bool&>;
    { classification.churn_count } -> std::same_as<std::size_t&>;
    { classification.regression_count } -> std::same_as<std::size_t&>;
    { classification.improvement_count } -> std::same_as<std::size_t&>;
    { classification.changed_category_count } -> std::same_as<std::size_t&>;
};

template <typename T>
concept InputActionResolutionReplaySummaryDiffInterface = requires(T diff) {
    { diff.batch_count } -> std::same_as<input::normalized_input_replay_count_delta&>;
    { diff.result_counts } -> std::same_as<input::input_action_candidate_result_count_deltas&>;
    { diff.action_kinds } -> std::same_as<input::input_action_candidate_count_deltas&>;
    { diff.reasons } -> std::same_as<input::input_action_resolution_reason_count_deltas&>;
    { diff.focus_target } -> std::same_as<input::input_action_resolution_target_delta&>;
    { diff.text_target } -> std::same_as<input::input_action_resolution_target_delta&>;
    { diff.classification }
        -> std::same_as<input::input_action_resolution_replay_classification&>;
    { diff.changed } -> std::same_as<bool&>;
    { diff.changed_category_count } -> std::same_as<std::size_t&>;
};

template <typename T>
concept InputActionResolutionDiffFunctions = requires(
    input::input_action_resolution_reason_counts& reason_counts,
    input::input_action_candidate_result_reason reason,
    const input::normalized_input_replay_count_delta& count_delta,
    const input::input_action_candidate_count_deltas& candidate_deltas,
    const input::input_action_candidate_counts& candidate_counts,
    const input::input_action_candidate_result_count_deltas& result_deltas,
    const input::input_action_candidate_result_counts& result_counts,
    const input::input_action_resolution_replay_summary& replay_summary,
    const input::input_action_resolution_replay_summary_diff& replay_summary_diff,
    const std::vector<input::input_action_resolution_result_summary>& result_summaries,
    const input::input_action_resolution_reason_counts& reason_count_source,
    const input::input_action_resolution_reason_count_deltas& reason_deltas,
    const input::input_action_resolution_result_summary& result_summary,
    input::input_action_resolution_target_snapshot& target_snapshot,
    const input::input_action_resolution_target_snapshot& target_snapshot_source,
    std::size_t& changed_category_count,
    bool changed) {
    { input::count_input_action_resolution_reason(reason_counts, reason) } -> std::same_as<void>;
    { input::input_action_candidate_count_deltas_changed(candidate_deltas) } -> std::same_as<bool>;
    { input::diff_input_action_candidate_counts(candidate_counts, candidate_counts) }
        -> std::same_as<input::input_action_candidate_count_deltas>;
    { input::input_action_resolution_all_action_counts(result_counts) }
        -> std::same_as<input::input_action_candidate_counts>;
    { input::input_action_candidate_result_count_deltas_changed(result_deltas) }
        -> std::same_as<bool>;
    { input::diff_input_action_candidate_result_counts(result_counts, result_counts) }
        -> std::same_as<input::input_action_candidate_result_count_deltas>;
    { input::count_input_action_resolution_reasons(reason_counts, result_summaries) }
        -> std::same_as<void>;
    { input::summarize_input_action_resolution_reasons(replay_summary) }
        -> std::same_as<input::input_action_resolution_reason_counts>;
    { input::input_action_resolution_reason_count_deltas_changed(reason_deltas) }
        -> std::same_as<bool>;
    { input::diff_input_action_resolution_reason_counts(reason_count_source, reason_count_source) }
        -> std::same_as<input::input_action_resolution_reason_count_deltas>;
    { input::input_action_resolution_summary_is_focus_target_evidence(result_summary) }
        -> std::same_as<bool>;
    { input::input_action_resolution_summary_is_text_target_evidence(result_summary) }
        -> std::same_as<bool>;
    { input::update_input_action_resolution_focus_target_snapshot(target_snapshot, result_summaries) }
        -> std::same_as<void>;
    { input::update_input_action_resolution_text_target_snapshot(target_snapshot, result_summaries) }
        -> std::same_as<void>;
    { input::input_action_resolution_focus_target_snapshot(replay_summary) }
        -> std::same_as<input::input_action_resolution_target_snapshot>;
    { input::input_action_resolution_text_target_snapshot(replay_summary) }
        -> std::same_as<input::input_action_resolution_target_snapshot>;
    { input::diff_input_action_resolution_target_snapshot(target_snapshot_source, target_snapshot_source) }
        -> std::same_as<input::input_action_resolution_target_delta>;
    { input::count_input_action_resolution_diff_category(changed, changed_category_count) }
        -> std::same_as<void>;
    { input::input_action_resolution_count_delta_gained(count_delta) } -> std::same_as<bool>;
    { input::input_action_resolution_count_delta_lost(count_delta) } -> std::same_as<bool>;
    { input::input_action_candidate_count_deltas_gained(candidate_deltas) } -> std::same_as<bool>;
    { input::input_action_candidate_count_deltas_lost(candidate_deltas) } -> std::same_as<bool>;
    { input::input_action_resolution_replay_change_class_for(changed, changed, changed) }
        -> std::same_as<input::input_action_resolution_replay_change_class>;
    { input::classify_input_action_resolution_replay_diff(replay_summary_diff) }
        -> std::same_as<input::input_action_resolution_replay_classification>;
    { input::diff_input_action_resolution_replay_summaries(replay_summary, replay_summary) }
        -> std::same_as<input::input_action_resolution_replay_summary_diff>;
};

template <typename T>
concept PointerCaptureSnapshotInterface = requires(T snapshot) {
    { snapshot.lifecycle } -> std::same_as<input::pointer_capture_lifecycle&>;
    { snapshot.active } -> std::same_as<bool&>;
    { snapshot.pointer_id } -> std::same_as<std::int32_t&>;
    { snapshot.tracked_pointer_count } -> std::same_as<std::size_t&>;
};

template <typename T>
concept GesturePolicySnapshotInterface = requires(T snapshot) {
    { snapshot.decision } -> std::same_as<input::gesture_policy_decision&>;
    { snapshot.direction } -> std::same_as<input::gesture_direction&>;
    { snapshot.phase } -> std::same_as<input::pointer_phase&>;
    { snapshot.timestamp_ms } -> std::same_as<std::int64_t&>;
    { snapshot.duration_ms } -> std::same_as<std::int64_t&>;
    { snapshot.pointer_id } -> std::same_as<std::int32_t&>;
    { snapshot.start_x } -> std::same_as<float&>;
    { snapshot.start_y } -> std::same_as<float&>;
    { snapshot.x } -> std::same_as<float&>;
    { snapshot.y } -> std::same_as<float&>;
    { snapshot.delta_x } -> std::same_as<float&>;
    { snapshot.delta_y } -> std::same_as<float&>;
    { snapshot.distance } -> std::same_as<float&>;
    { snapshot.swipe_min_dx } -> std::same_as<float&>;
    { snapshot.swipe_max_dy } -> std::same_as<float&>;
    { snapshot.swipe_max_duration_ms } -> std::same_as<std::int64_t&>;
    { snapshot.long_press_min_duration_ms } -> std::same_as<std::int64_t&>;
    { snapshot.tap_slop } -> std::same_as<float&>;
    { snapshot.drag_start_slop } -> std::same_as<float&>;
    { snapshot.emitted_input_event } -> std::same_as<bool&>;
    { snapshot.emitted_kind } -> std::same_as<input::gesture_kind&>;
};

template <typename T>
concept NormalizedInputEventSummaryInterface = requires(T summary) {
    { summary.kind } -> std::same_as<input::input_event_summary_kind&>;
    { summary.timestamp_ms } -> std::same_as<std::int64_t&>;
    { summary.duration_ms } -> std::same_as<std::int64_t&>;
    { summary.pointer_id } -> std::same_as<std::int32_t&>;
    { summary.start_x } -> std::same_as<float&>;
    { summary.start_y } -> std::same_as<float&>;
    { summary.x } -> std::same_as<float&>;
    { summary.y } -> std::same_as<float&>;
    { summary.delta_x } -> std::same_as<float&>;
    { summary.delta_y } -> std::same_as<float&>;
    { summary.pixel_delta_x } -> std::same_as<float&>;
    { summary.pixel_delta_y } -> std::same_as<float&>;
    { summary.line_delta_x } -> std::same_as<float&>;
    { summary.line_delta_y } -> std::same_as<float&>;
};

template <typename T>
concept InputRoutingDiagnosticsInterface = requires(T diagnostics) {
    { diagnostics.normalized_events } -> std::same_as<std::vector<input::normalized_input_event_summary>&>;
    { diagnostics.action_routes } -> std::same_as<std::vector<input::action_route_policy_diagnostic>&>;
    { diagnostics.pointer_capture } -> std::same_as<input::pointer_capture_snapshot&>;
    { diagnostics.summary } -> std::same_as<input::input_diagnostic_summary&>;
};

template <typename T>
concept NormalizedInputEventKindCountsInterface = requires(T counts) {
    { counts.tap } -> std::same_as<std::size_t&>;
    { counts.long_press } -> std::same_as<std::size_t&>;
    { counts.swipe_left } -> std::same_as<std::size_t&>;
    { counts.swipe_right } -> std::same_as<std::size_t&>;
    { counts.drag_start } -> std::same_as<std::size_t&>;
    { counts.drag_update } -> std::same_as<std::size_t&>;
    { counts.drag_end } -> std::same_as<std::size_t&>;
    { counts.drag_cancel } -> std::same_as<std::size_t&>;
    { counts.wheel } -> std::same_as<std::size_t&>;
};

template <typename T>
concept InputRouteKindCountsInterface = requires(T counts) {
    { counts.pointer } -> std::same_as<std::size_t&>;
    { counts.text } -> std::same_as<std::size_t&>;
    { counts.ime } -> std::same_as<std::size_t&>;
    { counts.focus } -> std::same_as<std::size_t&>;
    { counts.wheel } -> std::same_as<std::size_t&>;
    { counts.total } -> std::same_as<std::size_t&>;
};

template <typename T>
concept InputDiagnosticSummaryInterface = requires(T summary) {
    { summary.normalized_events } -> std::same_as<input::normalized_input_event_kind_counts&>;
    { summary.routes } -> std::same_as<input::input_route_kind_counts&>;
    { summary.normalized_event_count } -> std::same_as<std::size_t&>;
    { summary.pointer_capture_ended_cleanly } -> std::same_as<bool&>;
    { summary.focus_ended_cleanly } -> std::same_as<bool&>;
    { summary.preedit_ended_cleanly } -> std::same_as<bool&>;
};

template <typename T>
concept InputRoutingCountDeltaInterface = requires(T delta) {
    { delta.before_count } -> std::same_as<std::size_t&>;
    { delta.after_count } -> std::same_as<std::size_t&>;
    { delta.delta } -> std::same_as<std::int64_t&>;
    { delta.changed } -> std::same_as<bool&>;
};

template <typename T>
concept InputRoutingBoolDeltaInterface = requires(T delta) {
    { delta.before_value } -> std::same_as<bool&>;
    { delta.after_value } -> std::same_as<bool&>;
    { delta.changed } -> std::same_as<bool&>;
};

template <typename T>
concept InputRoutingFloatDeltaInterface = requires(T delta) {
    { delta.before_value } -> std::same_as<float&>;
    { delta.after_value } -> std::same_as<float&>;
    { delta.delta } -> std::same_as<float&>;
    { delta.changed } -> std::same_as<bool&>;
    { delta.tightened } -> std::same_as<bool&>;
    { delta.loosened } -> std::same_as<bool&>;
};

template <typename T>
concept InputRoutingInt64DeltaInterface = requires(T delta) {
    { delta.before_value } -> std::same_as<std::int64_t&>;
    { delta.after_value } -> std::same_as<std::int64_t&>;
    { delta.delta } -> std::same_as<std::int64_t&>;
    { delta.changed } -> std::same_as<bool&>;
    { delta.tightened } -> std::same_as<bool&>;
    { delta.loosened } -> std::same_as<bool&>;
};

template <typename T>
concept InputRoutingPointerCaptureDeltaInterface = requires(T delta) {
    { delta.before_capture } -> std::same_as<input::pointer_capture_snapshot&>;
    { delta.after_capture } -> std::same_as<input::pointer_capture_snapshot&>;
    { delta.before_has_state } -> std::same_as<bool&>;
    { delta.after_has_state } -> std::same_as<bool&>;
    { delta.active } -> std::same_as<input::input_routing_bool_delta&>;
    { delta.tracked_pointer_count } -> std::same_as<input::input_routing_count_delta&>;
    { delta.lifecycle_changed } -> std::same_as<bool&>;
    { delta.pointer_id_changed } -> std::same_as<bool&>;
    { delta.changed } -> std::same_as<bool&>;
};

template <typename T>
concept InputRoutingGesturePolicyThresholdDeltasInterface = requires(T deltas) {
    { deltas.swipe_min_dx } -> std::same_as<input::input_routing_float_delta&>;
    { deltas.swipe_max_dy } -> std::same_as<input::input_routing_float_delta&>;
    { deltas.swipe_max_duration_ms } -> std::same_as<input::input_routing_int64_delta&>;
    { deltas.long_press_min_duration_ms } -> std::same_as<input::input_routing_int64_delta&>;
    { deltas.tap_slop } -> std::same_as<input::input_routing_float_delta&>;
    { deltas.drag_start_slop } -> std::same_as<input::input_routing_float_delta&>;
    { deltas.swipe_threshold_changed } -> std::same_as<bool&>;
    { deltas.long_press_threshold_changed } -> std::same_as<bool&>;
    { deltas.tap_threshold_changed } -> std::same_as<bool&>;
    { deltas.drag_threshold_changed } -> std::same_as<bool&>;
    { deltas.tightened } -> std::same_as<bool&>;
    { deltas.loosened } -> std::same_as<bool&>;
    { deltas.changed } -> std::same_as<bool&>;
};

template <typename T>
concept InputRoutingGesturePolicyRouteDiffInterface = requires(T diff) {
    { diff.before_route_index } -> std::same_as<std::size_t&>;
    { diff.after_route_index } -> std::same_as<std::size_t&>;
    { diff.before_route_kind } -> std::same_as<input::action_route_policy_kind&>;
    { diff.after_route_kind } -> std::same_as<input::action_route_policy_kind&>;
    { diff.before_policy } -> std::same_as<input::gesture_policy_snapshot&>;
    { diff.after_policy } -> std::same_as<input::gesture_policy_snapshot&>;
    { diff.before_contact } -> std::same_as<input::pointer_contact_kind&>;
    { diff.after_contact } -> std::same_as<input::pointer_contact_kind&>;
    { diff.before_phase } -> std::same_as<input::pointer_phase&>;
    { diff.after_phase } -> std::same_as<input::pointer_phase&>;
    { diff.before_pointer_id } -> std::same_as<std::int32_t&>;
    { diff.after_pointer_id } -> std::same_as<std::int32_t&>;
    { diff.thresholds } -> std::same_as<input::input_routing_gesture_policy_threshold_deltas&>;
    { diff.decision_changed } -> std::same_as<bool&>;
    { diff.emitted_kind_changed } -> std::same_as<bool&>;
    { diff.direction_changed } -> std::same_as<bool&>;
    { diff.emitted_input_event_changed } -> std::same_as<bool&>;
    { diff.accepted_to_suppressed } -> std::same_as<bool&>;
    { diff.suppressed_to_accepted } -> std::same_as<bool&>;
    { diff.pointer_mismatch } -> std::same_as<bool&>;
    { diff.contact_mismatch } -> std::same_as<bool&>;
    { diff.phase_mismatch } -> std::same_as<bool&>;
    { diff.changed } -> std::same_as<bool&>;
};

template <typename T>
concept InputRoutingGesturePolicyDiffInterface = requires(T diff) {
    { diff.routes } -> std::same_as<std::vector<input::input_routing_gesture_policy_route_diff>&>;
    { diff.route_count } -> std::same_as<input::input_routing_count_delta&>;
    { diff.compared_route_count } -> std::same_as<std::size_t&>;
    { diff.unpaired_before_route_count } -> std::same_as<std::size_t&>;
    { diff.unpaired_after_route_count } -> std::same_as<std::size_t&>;
    { diff.threshold_change_count } -> std::same_as<std::size_t&>;
    { diff.decision_change_count } -> std::same_as<std::size_t&>;
    { diff.emitted_kind_change_count } -> std::same_as<std::size_t&>;
    { diff.direction_change_count } -> std::same_as<std::size_t&>;
    { diff.accepted_to_suppressed_regression_count } -> std::same_as<std::size_t&>;
    { diff.suppressed_to_accepted_recovery_count } -> std::same_as<std::size_t&>;
    { diff.swipe_threshold_tightening_count } -> std::same_as<std::size_t&>;
    { diff.swipe_threshold_loosening_count } -> std::same_as<std::size_t&>;
    { diff.long_press_threshold_tightening_count } -> std::same_as<std::size_t&>;
    { diff.long_press_threshold_loosening_count } -> std::same_as<std::size_t&>;
    { diff.tap_threshold_tightening_count } -> std::same_as<std::size_t&>;
    { diff.tap_threshold_loosening_count } -> std::same_as<std::size_t&>;
    { diff.drag_threshold_tightening_count } -> std::same_as<std::size_t&>;
    { diff.drag_threshold_loosening_count } -> std::same_as<std::size_t&>;
    { diff.pointer_mismatch_count } -> std::same_as<std::size_t&>;
    { diff.contact_mismatch_count } -> std::same_as<std::size_t&>;
    { diff.phase_mismatch_count } -> std::same_as<std::size_t&>;
    { diff.changed } -> std::same_as<bool&>;
};

template <typename T>
concept NormalizedInputEventKindCountDeltasInterface = requires(T deltas) {
    { deltas.tap } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.long_press } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.swipe_left } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.swipe_right } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.drag_start } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.drag_update } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.drag_end } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.drag_cancel } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.wheel } -> std::same_as<input::input_routing_count_delta&>;
};

template <typename T>
concept InputRouteKindCountDeltasInterface = requires(T deltas) {
    { deltas.pointer } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.text } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.ime } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.focus } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.wheel } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.total } -> std::same_as<input::input_routing_count_delta&>;
};

template <typename T>
concept ActionRoutePolicyKindCountsInterface = requires(T counts) {
    { counts.pointer_capture_reset } -> std::same_as<std::size_t&>;
    { counts.pointer_capture_arbitration } -> std::same_as<std::size_t&>;
    { counts.wheel_summary } -> std::same_as<std::size_t&>;
    { counts.gesture_route_snapshot } -> std::same_as<std::size_t&>;
    { counts.text_commit_boundary } -> std::same_as<std::size_t&>;
    { counts.text_backspace_boundary } -> std::same_as<std::size_t&>;
    { counts.text_delete_forward_boundary } -> std::same_as<std::size_t&>;
    { counts.caret_moved } -> std::same_as<std::size_t&>;
    { counts.selection_changed } -> std::same_as<std::size_t&>;
    { counts.focus_traversal_next } -> std::same_as<std::size_t&>;
    { counts.focus_traversal_previous } -> std::same_as<std::size_t&>;
    { counts.text_submit_boundary } -> std::same_as<std::size_t&>;
    { counts.keyboard_cancel_intent } -> std::same_as<std::size_t&>;
    { counts.focus_loss } -> std::same_as<std::size_t&>;
    { counts.ime_preedit } -> std::same_as<std::size_t&>;
    { counts.ime_commit } -> std::same_as<std::size_t&>;
    { counts.ime_cancel } -> std::same_as<std::size_t&>;
    { counts.ime_composition_start } -> std::same_as<std::size_t&>;
};

template <typename T>
concept ActionRoutePolicyKindCountDeltasInterface = requires(T deltas) {
    { deltas.pointer_capture_reset } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.pointer_capture_arbitration } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.wheel_summary } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.gesture_route_snapshot } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.text_commit_boundary } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.text_backspace_boundary } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.text_delete_forward_boundary } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.caret_moved } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.selection_changed } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.focus_traversal_next } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.focus_traversal_previous } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.text_submit_boundary } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.keyboard_cancel_intent } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.focus_loss } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.ime_preedit } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.ime_commit } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.ime_cancel } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.ime_composition_start } -> std::same_as<input::input_routing_count_delta&>;
};

template <typename T>
concept ActionRoutePolicyDiagnosticInterface = requires(T diagnostic) {
    { diagnostic.kind } -> std::same_as<input::action_route_policy_kind&>;
    { diagnostic.timestamp_ms } -> std::same_as<std::int64_t&>;
    { diagnostic.emits_input_event } -> std::same_as<bool&>;
    { diagnostic.event_index } -> std::same_as<std::size_t&>;
    { diagnostic.target_id } -> std::same_as<std::string&>;
    { diagnostic.text_byte_count } -> std::same_as<std::size_t&>;
    { diagnostic.text_byte_count_before } -> std::same_as<std::size_t&>;
    { diagnostic.text_byte_count_after } -> std::same_as<std::size_t&>;
    { diagnostic.caret_before } -> std::same_as<input::text_range&>;
    { diagnostic.caret_after } -> std::same_as<input::text_range&>;
    { diagnostic.had_selection_before } -> std::same_as<bool&>;
    { diagnostic.has_selection_after } -> std::same_as<bool&>;
    { diagnostic.selection_before } -> std::same_as<input::text_range&>;
    { diagnostic.selection_after } -> std::same_as<input::text_range&>;
    { diagnostic.normalized_event } -> std::same_as<input::normalized_input_event_summary&>;
    { diagnostic.composition } -> std::same_as<input::ime_composition_state&>;
    { diagnostic.gesture_policy } -> std::same_as<input::gesture_policy_snapshot&>;
    { diagnostic.keyboard } -> std::same_as<input::keyboard_chord_diagnostic&>;
    { diagnostic.pointer_capture_before } -> std::same_as<input::pointer_capture_snapshot&>;
    { diagnostic.pointer_capture_after } -> std::same_as<input::pointer_capture_snapshot&>;
    { diagnostic.pointer_decision } -> std::same_as<input::pointer_arbitration_decision&>;
    { diagnostic.pointer_event_phase } -> std::same_as<input::pointer_phase&>;
    { diagnostic.pointer_contact } -> std::same_as<input::pointer_contact_kind&>;
    { diagnostic.pointer_id } -> std::same_as<std::int32_t&>;
    { diagnostic.tracked_pointer_count_before } -> std::same_as<std::size_t&>;
    { diagnostic.tracked_pointer_count_after } -> std::same_as<std::size_t&>;
};

template <typename T>
concept KeyboardModifierStateInterface = requires(T state) {
    { state.alt } -> std::same_as<bool&>;
    { state.ctrl } -> std::same_as<bool&>;
    { state.shift } -> std::same_as<bool&>;
    { state.meta } -> std::same_as<bool&>;
};

template <typename T>
concept KeyboardChordDiagnosticInterface = requires(T chord) {
    { chord.logical_key } -> std::same_as<std::string&>;
    { chord.key_code } -> std::same_as<std::int32_t&>;
    { chord.phase } -> std::same_as<raw_platform_key_phase&>;
    { chord.modifiers } -> std::same_as<input::keyboard_modifier_state&>;
    { chord.repeat } -> std::same_as<bool&>;
    { chord.repeat_policy } -> std::same_as<input::keyboard_repeat_policy&>;
    { chord.intent } -> std::same_as<input::keyboard_shortcut_intent&>;
};

template <typename T>
concept InputRoutingKeyboardIntentCountsInterface = requires(T counts) {
    { counts.none } -> std::same_as<std::size_t&>;
    { counts.focus_traversal_next } -> std::same_as<std::size_t&>;
    { counts.focus_traversal_previous } -> std::same_as<std::size_t&>;
    { counts.submit } -> std::same_as<std::size_t&>;
    { counts.cancel } -> std::same_as<std::size_t&>;
    { counts.caret_previous } -> std::same_as<std::size_t&>;
    { counts.caret_next } -> std::same_as<std::size_t&>;
    { counts.caret_home } -> std::same_as<std::size_t&>;
    { counts.caret_end } -> std::same_as<std::size_t&>;
    { counts.selection_previous } -> std::same_as<std::size_t&>;
    { counts.selection_next } -> std::same_as<std::size_t&>;
    { counts.select_all } -> std::same_as<std::size_t&>;
    { counts.delete_backward } -> std::same_as<std::size_t&>;
    { counts.delete_forward } -> std::same_as<std::size_t&>;
};

template <typename T>
concept InputRoutingKeyboardRepeatPolicyCountsInterface = requires(T counts) {
    { counts.not_repeat } -> std::same_as<std::size_t&>;
    { counts.allowed } -> std::same_as<std::size_t&>;
    { counts.ignored } -> std::same_as<std::size_t&>;
};

template <typename T>
concept InputRoutingKeyboardRouteCountsInterface = requires(T counts) {
    { counts.intents } -> std::same_as<input::input_routing_keyboard_intent_counts&>;
    { counts.repeat_policies } -> std::same_as<input::input_routing_keyboard_repeat_policy_counts&>;
    { counts.total } -> std::same_as<std::size_t&>;
    { counts.emitted_input_event_routes } -> std::same_as<std::size_t&>;
    { counts.diagnostic_only_routes } -> std::same_as<std::size_t&>;
};

template <typename T>
concept InputRoutingKeyboardIntentCountDeltasInterface = requires(T deltas) {
    { deltas.none } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.focus_traversal_next } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.focus_traversal_previous } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.submit } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.cancel } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.caret_previous } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.caret_next } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.caret_home } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.caret_end } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.selection_previous } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.selection_next } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.select_all } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.delete_backward } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.delete_forward } -> std::same_as<input::input_routing_count_delta&>;
};

template <typename T>
concept InputRoutingKeyboardRepeatPolicyCountDeltasInterface = requires(T deltas) {
    { deltas.not_repeat } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.allowed } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.ignored } -> std::same_as<input::input_routing_count_delta&>;
};

template <typename T>
concept InputRoutingKeyboardRouteCountDeltasInterface = requires(T deltas) {
    { deltas.intents } -> std::same_as<input::input_routing_keyboard_intent_count_deltas&>;
    { deltas.repeat_policies } -> std::same_as<input::input_routing_keyboard_repeat_policy_count_deltas&>;
    { deltas.total } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.emitted_input_event_routes } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.diagnostic_only_routes } -> std::same_as<input::input_routing_count_delta&>;
    { deltas.changed } -> std::same_as<bool&>;
};

template <typename T>
concept InputRoutingDiagnosticsDiffInterface = requires(T diff) {
    { diff.normalized_event_count } -> std::same_as<input::input_routing_count_delta&>;
    { diff.normalized_event_summary_count } -> std::same_as<input::input_routing_count_delta&>;
    { diff.action_route_count } -> std::same_as<input::input_routing_count_delta&>;
    { diff.normalized_events } -> std::same_as<input::normalized_input_event_kind_count_deltas&>;
    { diff.routes } -> std::same_as<input::input_route_kind_count_deltas&>;
    { diff.action_routes } -> std::same_as<input::action_route_policy_kind_count_deltas&>;
    { diff.keyboard_routes } -> std::same_as<input::input_routing_keyboard_route_count_deltas&>;
    { diff.gesture_policies } -> std::same_as<input::input_routing_gesture_policy_diff&>;
    { diff.pointer_capture } -> std::same_as<input::input_routing_pointer_capture_delta&>;
    { diff.pointer_capture_ended_cleanly } -> std::same_as<input::input_routing_bool_delta&>;
    { diff.focus_ended_cleanly } -> std::same_as<input::input_routing_bool_delta&>;
    { diff.preedit_ended_cleanly } -> std::same_as<input::input_routing_bool_delta&>;
    { diff.normalized_events_changed } -> std::same_as<bool&>;
    { diff.action_routes_changed } -> std::same_as<bool&>;
    { diff.keyboard_routes_changed } -> std::same_as<bool&>;
    { diff.gesture_policy_changed } -> std::same_as<bool&>;
    { diff.pointer_capture_changed } -> std::same_as<bool&>;
    { diff.clean_state_changed } -> std::same_as<bool&>;
    { diff.changed } -> std::same_as<bool&>;
};

template <typename T>
concept InputRoutingDiagnosticFunctions = requires(
    input::input_diagnostic_summary& target,
    const input::input_diagnostic_summary& source,
    input::input_event_summary_kind event_kind,
    input::action_route_policy_kind route_kind,
    input::action_route_policy_kind_counts& action_route_counts,
    input::input_routing_keyboard_intent_counts& keyboard_intents,
    input::input_routing_keyboard_repeat_policy_counts& keyboard_repeat_policies,
    const input::pointer_capture_snapshot& capture,
    const input::input_routing_diagnostics& diagnostics,
    const input::action_route_policy_diagnostic& route,
    const input::gesture_policy_snapshot& gesture_policy,
    const input::keyboard_chord_diagnostic& keyboard,
    const input::text_input_model& text) {
    { input::count_input_diagnostic_normalized_event(target, event_kind) } -> std::same_as<void>;
    { input::count_input_diagnostic_route(target, route_kind) } -> std::same_as<void>;
    { input::input_diagnostic_pointer_capture_has_state(capture) } -> std::same_as<bool>;
    { input::summarize_input_routing_diagnostics(diagnostics, text) }
        -> std::same_as<input::input_diagnostic_summary>;
    { input::accumulate_input_diagnostic_summary(target, source) } -> std::same_as<void>;
    { input::input_routing_size_delta(std::size_t{}, std::size_t{}) } -> std::same_as<std::int64_t>;
    { input::diff_input_routing_count(std::size_t{}, std::size_t{}) }
        -> std::same_as<input::input_routing_count_delta>;
    { input::diff_input_routing_bool(false, true) } -> std::same_as<input::input_routing_bool_delta>;
    { input::diff_input_routing_float_threshold(float{}, float{}, true) }
        -> std::same_as<input::input_routing_float_delta>;
    { input::diff_input_routing_int64_threshold(std::int64_t{}, std::int64_t{}, true) }
        -> std::same_as<input::input_routing_int64_delta>;
    { input::input_routing_keyboard_chord_present(keyboard) } -> std::same_as<bool>;
    { input::count_input_routing_action_route_policy_kind(action_route_counts, route_kind) }
        -> std::same_as<void>;
    { input::summarize_input_routing_action_route_policy_kinds(diagnostics) }
        -> std::same_as<input::action_route_policy_kind_counts>;
    { input::count_input_routing_keyboard_intent(keyboard_intents, input::keyboard_shortcut_intent{}) }
        -> std::same_as<void>;
    { input::count_input_routing_keyboard_repeat_policy(
        keyboard_repeat_policies,
        input::keyboard_repeat_policy{}) } -> std::same_as<void>;
    { input::summarize_input_routing_keyboard_routes(diagnostics) }
        -> std::same_as<input::input_routing_keyboard_route_counts>;
    { input::diff_input_routing_pointer_capture(capture, capture) }
        -> std::same_as<input::input_routing_pointer_capture_delta>;
    { input::diff_normalized_input_event_kind_counts(
        input::normalized_input_event_kind_counts{},
        input::normalized_input_event_kind_counts{}) } -> std::same_as<input::normalized_input_event_kind_count_deltas>;
    { input::diff_input_route_kind_counts(input::input_route_kind_counts{}, input::input_route_kind_counts{}) }
        -> std::same_as<input::input_route_kind_count_deltas>;
    { input::diff_action_route_policy_kind_counts(
        input::action_route_policy_kind_counts{},
        input::action_route_policy_kind_counts{}) } -> std::same_as<input::action_route_policy_kind_count_deltas>;
    { input::diff_input_routing_keyboard_route_counts(
        input::input_routing_keyboard_route_counts{},
        input::input_routing_keyboard_route_counts{}) }
        -> std::same_as<input::input_routing_keyboard_route_count_deltas>;
    { input::input_routing_route_has_gesture_policy(route) } -> std::same_as<bool>;
    { input::input_routing_gesture_policy_accepted(gesture_policy) } -> std::same_as<bool>;
    { input::input_routing_gesture_policy_route_indices(diagnostics) }
        -> std::same_as<std::vector<std::size_t>>;
    { input::diff_input_routing_gesture_policy_thresholds(gesture_policy, gesture_policy) }
        -> std::same_as<input::input_routing_gesture_policy_threshold_deltas>;
    { input::diff_input_routing_gesture_policy_route(route, std::size_t{}, route, std::size_t{}) }
        -> std::same_as<input::input_routing_gesture_policy_route_diff>;
    { input::diff_input_routing_gesture_policies(diagnostics, diagnostics) }
        -> std::same_as<input::input_routing_gesture_policy_diff>;
    { input::diff_input_routing_diagnostics(diagnostics, diagnostics) }
        -> std::same_as<input::input_routing_diagnostics_diff>;
};

template <typename T>
concept ImeCompositionStateInterface = requires(T state) {
    { state.active } -> std::same_as<bool&>;
    { state.preedit_text } -> std::same_as<std::string&>;
    { state.replacement_range } -> std::same_as<input::text_range&>;
    { state.preedit_range } -> std::same_as<input::text_range&>;
    { state.caret_range } -> std::same_as<input::text_range&>;
};

template <typename T>
concept TextInputModelInterface = requires(
    T& model,
    std::string target,
    std::string_view text,
    input::text_range range) {
    { model.focus(target) } -> std::same_as<void>;
    { model.clear_focus() } -> std::same_as<void>;
    { model.has_focus() } -> std::same_as<bool>;
    { model.focus_id() } -> std::same_as<const std::string&>;
    { model.text() } -> std::same_as<const std::string&>;
    { model.preedit_text() } -> std::same_as<const std::string&>;
    { model.display_text() } -> std::same_as<std::string>;
    { model.caret_byte_offset() } -> std::same_as<std::size_t>;
    { model.caret_range() } -> std::same_as<input::text_range>;
    { model.preedit_range() } -> std::same_as<std::optional<input::text_range>>;
    { model.selection_range() } -> std::same_as<std::optional<input::text_range>>;
    { model.ime_composition() } -> std::same_as<input::ime_composition_state>;
    { model.last_transaction_diagnostics() }
        -> std::same_as<const input::text_edit_transaction_diagnostics&>;
    { model.move_caret_to_start() } -> std::same_as<bool>;
    { model.move_caret_to_end() } -> std::same_as<bool>;
    { model.move_caret_left() } -> std::same_as<bool>;
    { model.move_caret_right() } -> std::same_as<bool>;
    { model.extend_selection_left() } -> std::same_as<bool>;
    { model.extend_selection_right() } -> std::same_as<bool>;
    { model.select_all() } -> std::same_as<bool>;
    { model.clear_selection() } -> std::same_as<bool>;
    { model.set_selection(range) } -> std::same_as<bool>;
    { model.commit_utf8(text) } -> std::same_as<bool>;
    { model.backspace() } -> std::same_as<bool>;
    { model.delete_forward() } -> std::same_as<bool>;
    { model.set_preedit(text) } -> std::same_as<bool>;
    { model.commit_ime(text) } -> std::same_as<bool>;
    { model.cancel_ime() } -> std::same_as<bool>;
    { model.submit() } -> std::same_as<bool>;
    { model.has_submit_text() } -> std::same_as<bool>;
    { model.consume_submit_text() } -> std::same_as<std::optional<std::string>>;
};

template <typename T>
concept TextEditTransactionSnapshotInterface = requires(T snapshot) {
    { snapshot.has_focus } -> std::same_as<bool&>;
    { snapshot.target_id } -> std::same_as<std::string&>;
    { snapshot.text } -> std::same_as<std::string&>;
    { snapshot.display_text } -> std::same_as<std::string&>;
    { snapshot.caret_byte_offset } -> std::same_as<std::size_t&>;
    { snapshot.caret_range } -> std::same_as<input::text_range&>;
    { snapshot.has_selection } -> std::same_as<bool&>;
    { snapshot.selection } -> std::same_as<input::text_range&>;
    { snapshot.has_preedit } -> std::same_as<bool&>;
    { snapshot.preedit_text } -> std::same_as<std::string&>;
    { snapshot.preedit_range } -> std::same_as<input::text_range&>;
    { snapshot.preedit_anchor_byte_offset } -> std::same_as<std::size_t&>;
    { snapshot.composition } -> std::same_as<input::ime_composition_state&>;
};

template <typename T>
concept TextEditTransactionByteDiagnosticsInterface = requires(T bytes) {
    { bytes.committed_text_bytes_before } -> std::same_as<std::size_t&>;
    { bytes.committed_text_bytes_after } -> std::same_as<std::size_t&>;
    { bytes.display_text_bytes_before } -> std::same_as<std::size_t&>;
    { bytes.display_text_bytes_after } -> std::same_as<std::size_t&>;
    { bytes.committed_byte_delta } -> std::same_as<std::int64_t&>;
    { bytes.display_byte_delta } -> std::same_as<std::int64_t&>;
    { bytes.inserted_byte_count } -> std::same_as<std::size_t&>;
    { bytes.deleted_byte_count } -> std::same_as<std::size_t&>;
    { bytes.replaced_byte_count } -> std::same_as<std::size_t&>;
};

template <typename T>
concept TextEditTransactionDiagnosticsInterface = requires(T diagnostics) {
    { diagnostics.operation } -> std::same_as<input::text_edit_transaction_operation&>;
    { diagnostics.accepted } -> std::same_as<bool&>;
    { diagnostics.rejected } -> std::same_as<bool&>;
    { diagnostics.rejection_reason } -> std::same_as<input::text_edit_transaction_rejection_reason&>;
    { diagnostics.before } -> std::same_as<input::text_edit_transaction_snapshot&>;
    { diagnostics.after } -> std::same_as<input::text_edit_transaction_snapshot&>;
    { diagnostics.bytes } -> std::same_as<input::text_edit_transaction_byte_diagnostics&>;
    { diagnostics.text_changed } -> std::same_as<bool&>;
    { diagnostics.display_text_changed } -> std::same_as<bool&>;
    { diagnostics.caret_changed } -> std::same_as<bool&>;
    { diagnostics.selection_changed } -> std::same_as<bool&>;
    { diagnostics.preedit_changed } -> std::same_as<bool&>;
    { diagnostics.before_utf8_boundary_safe } -> std::same_as<bool&>;
    { diagnostics.after_utf8_boundary_safe } -> std::same_as<bool&>;
    { diagnostics.utf8_boundary_safe } -> std::same_as<bool&>;
    { diagnostics.selection_was_active } -> std::same_as<bool&>;
    { diagnostics.selection_replaced_committed_text } -> std::same_as<bool&>;
    { diagnostics.selection_replaced_display_text } -> std::same_as<bool&>;
    { diagnostics.selection_deleted } -> std::same_as<bool&>;
    { diagnostics.selection_cleared } -> std::same_as<bool&>;
    { diagnostics.ime_preedit_started } -> std::same_as<bool&>;
    { diagnostics.ime_preedit_updated } -> std::same_as<bool&>;
    { diagnostics.ime_preedit_cleared } -> std::same_as<bool&>;
    { diagnostics.ime_committed } -> std::same_as<bool&>;
    { diagnostics.ime_canceled } -> std::same_as<bool&>;
    { diagnostics.invalid_edit_rejected } -> std::same_as<bool&>;
    { diagnostics.changed } -> std::same_as<bool&>;
};

template <typename T>
concept TextInputPresentationByteCountsInterface = requires(T counts) {
    { counts.committed_text_bytes } -> std::same_as<std::size_t&>;
    { counts.display_text_bytes } -> std::same_as<std::size_t&>;
    { counts.preedit_text_bytes } -> std::same_as<std::size_t&>;
    { counts.selected_text_bytes } -> std::same_as<std::size_t&>;
    { counts.preedit_range_bytes } -> std::same_as<std::size_t&>;
    { counts.caret_byte_offset } -> std::same_as<std::size_t&>;
    { counts.preedit_anchor_byte_offset } -> std::same_as<std::size_t&>;
};

template <typename T>
concept TextInputPresentationRouteByteDiagnosticsInterface = requires(T diagnostics) {
    { diagnostics.available } -> std::same_as<bool&>;
    { diagnostics.text_byte_count } -> std::same_as<std::size_t&>;
    { diagnostics.text_byte_count_before } -> std::same_as<std::size_t&>;
    { diagnostics.text_byte_count_after } -> std::same_as<std::size_t&>;
    { diagnostics.text_byte_delta } -> std::same_as<std::int64_t&>;
};

template <typename T>
concept TextInputPresentationSnapshotInterface = requires(T snapshot) {
    { snapshot.has_focus } -> std::same_as<bool&>;
    { snapshot.target_id } -> std::same_as<std::string&>;
    { snapshot.committed_text } -> std::same_as<std::string&>;
    { snapshot.display_text } -> std::same_as<std::string&>;
    { snapshot.caret_byte_offset } -> std::same_as<std::size_t&>;
    { snapshot.caret_range } -> std::same_as<input::text_range&>;
    { snapshot.has_selection } -> std::same_as<bool&>;
    { snapshot.selection_range } -> std::same_as<input::text_range&>;
    { snapshot.has_preedit } -> std::same_as<bool&>;
    { snapshot.preedit_text } -> std::same_as<std::string&>;
    { snapshot.preedit_range } -> std::same_as<input::text_range&>;
    { snapshot.preedit_anchor_byte_offset } -> std::same_as<std::size_t&>;
    { snapshot.has_submit_text } -> std::same_as<bool&>;
    { snapshot.focus_clean } -> std::same_as<bool&>;
    { snapshot.preedit_clean } -> std::same_as<bool&>;
    { snapshot.byte_counts } -> std::same_as<input::text_input_presentation_byte_counts&>;
    { snapshot.route_byte_diagnostics }
        -> std::same_as<input::text_input_presentation_route_byte_diagnostics&>;
};

template <typename T>
concept TextInputPresentationCountDeltaInterface = requires(T delta) {
    { delta.before_count } -> std::same_as<std::size_t&>;
    { delta.after_count } -> std::same_as<std::size_t&>;
    { delta.delta } -> std::same_as<std::int64_t&>;
    { delta.changed } -> std::same_as<bool&>;
};

template <typename T>
concept TextInputPresentationBoolDeltaInterface = requires(T delta) {
    { delta.before_value } -> std::same_as<bool&>;
    { delta.after_value } -> std::same_as<bool&>;
    { delta.changed } -> std::same_as<bool&>;
};

template <typename T>
concept TextInputPresentationIntDeltaInterface = requires(T delta) {
    { delta.before_value } -> std::same_as<std::int64_t&>;
    { delta.after_value } -> std::same_as<std::int64_t&>;
    { delta.delta } -> std::same_as<std::int64_t&>;
    { delta.changed } -> std::same_as<bool&>;
};

template <typename T>
concept TextInputPresentationRangeDeltaInterface = requires(T delta) {
    { delta.before_range } -> std::same_as<input::text_range&>;
    { delta.after_range } -> std::same_as<input::text_range&>;
    { delta.changed } -> std::same_as<bool&>;
};

template <typename T>
concept TextInputPresentationStringDeltaInterface = requires(T delta) {
    { delta.before_value } -> std::same_as<std::string&>;
    { delta.after_value } -> std::same_as<std::string&>;
    { delta.byte_delta } -> std::same_as<std::int64_t&>;
    { delta.changed } -> std::same_as<bool&>;
};

template <typename T>
concept TextInputPresentationByteCountDeltasInterface = requires(T deltas) {
    { deltas.committed_text_bytes } -> std::same_as<input::text_input_presentation_count_delta&>;
    { deltas.display_text_bytes } -> std::same_as<input::text_input_presentation_count_delta&>;
    { deltas.preedit_text_bytes } -> std::same_as<input::text_input_presentation_count_delta&>;
    { deltas.selected_text_bytes } -> std::same_as<input::text_input_presentation_count_delta&>;
    { deltas.preedit_range_bytes } -> std::same_as<input::text_input_presentation_count_delta&>;
    { deltas.caret_byte_offset } -> std::same_as<input::text_input_presentation_count_delta&>;
    { deltas.preedit_anchor_byte_offset } -> std::same_as<input::text_input_presentation_count_delta&>;
    { deltas.changed } -> std::same_as<bool&>;
};

template <typename T>
concept TextInputPresentationRouteByteDiagnosticsDiffInterface = requires(T diff) {
    { diff.available } -> std::same_as<input::text_input_presentation_bool_delta&>;
    { diff.text_byte_count } -> std::same_as<input::text_input_presentation_count_delta&>;
    { diff.text_byte_count_before } -> std::same_as<input::text_input_presentation_count_delta&>;
    { diff.text_byte_count_after } -> std::same_as<input::text_input_presentation_count_delta&>;
    { diff.text_byte_delta } -> std::same_as<input::text_input_presentation_int_delta&>;
    { diff.changed } -> std::same_as<bool&>;
};

template <typename T>
concept TextInputPresentationDiffInterface = requires(T diff) {
    { diff.has_focus } -> std::same_as<input::text_input_presentation_bool_delta&>;
    { diff.target_id } -> std::same_as<input::text_input_presentation_string_delta&>;
    { diff.committed_text } -> std::same_as<input::text_input_presentation_string_delta&>;
    { diff.display_text } -> std::same_as<input::text_input_presentation_string_delta&>;
    { diff.caret_byte_offset } -> std::same_as<input::text_input_presentation_count_delta&>;
    { diff.caret_range } -> std::same_as<input::text_input_presentation_range_delta&>;
    { diff.has_selection } -> std::same_as<input::text_input_presentation_bool_delta&>;
    { diff.selection_range } -> std::same_as<input::text_input_presentation_range_delta&>;
    { diff.has_preedit } -> std::same_as<input::text_input_presentation_bool_delta&>;
    { diff.preedit_text } -> std::same_as<input::text_input_presentation_string_delta&>;
    { diff.preedit_range } -> std::same_as<input::text_input_presentation_range_delta&>;
    { diff.preedit_anchor_byte_offset } -> std::same_as<input::text_input_presentation_count_delta&>;
    { diff.has_submit_text } -> std::same_as<input::text_input_presentation_bool_delta&>;
    { diff.focus_clean } -> std::same_as<input::text_input_presentation_bool_delta&>;
    { diff.preedit_clean } -> std::same_as<input::text_input_presentation_bool_delta&>;
    { diff.byte_counts } -> std::same_as<input::text_input_presentation_byte_count_deltas&>;
    { diff.route_byte_diagnostics }
        -> std::same_as<input::text_input_presentation_route_byte_diagnostics_diff&>;
    { diff.focus_changed } -> std::same_as<bool&>;
    { diff.target_changed } -> std::same_as<bool&>;
    { diff.committed_text_changed } -> std::same_as<bool&>;
    { diff.display_text_changed } -> std::same_as<bool&>;
    { diff.caret_changed } -> std::same_as<bool&>;
    { diff.selection_changed } -> std::same_as<bool&>;
    { diff.preedit_changed } -> std::same_as<bool&>;
    { diff.submit_changed } -> std::same_as<bool&>;
    { diff.clean_flags_changed } -> std::same_as<bool&>;
    { diff.byte_counts_changed } -> std::same_as<bool&>;
    { diff.route_byte_diagnostics_changed } -> std::same_as<bool&>;
    { diff.changed } -> std::same_as<bool&>;
};

template <typename T>
concept TextInputPresentationFunctions = requires(
    const input::text_input_model& model,
    const input::input_routing_diagnostics& diagnostics,
    const input::action_route_policy_diagnostic& route,
    input::action_route_policy_kind route_kind,
    input::text_range range,
    const input::text_input_presentation_byte_counts& byte_counts,
    const input::text_input_presentation_route_byte_diagnostics& route_byte_diagnostics,
    const input::text_input_presentation_snapshot& snapshot) {
    { input::text_input_presentation_range_byte_count(range) } -> std::same_as<std::size_t>;
    { input::text_input_presentation_size_delta(std::size_t{}, std::size_t{}) }
        -> std::same_as<std::int64_t>;
    { input::diff_text_input_presentation_count(std::size_t{}, std::size_t{}) }
        -> std::same_as<input::text_input_presentation_count_delta>;
    { input::diff_text_input_presentation_bool(bool{}, bool{}) }
        -> std::same_as<input::text_input_presentation_bool_delta>;
    { input::diff_text_input_presentation_int(std::int64_t{}, std::int64_t{}) }
        -> std::same_as<input::text_input_presentation_int_delta>;
    { input::text_input_presentation_ranges_equal(range, range) } -> std::same_as<bool>;
    { input::diff_text_input_presentation_range(range, range) }
        -> std::same_as<input::text_input_presentation_range_delta>;
    { input::diff_text_input_presentation_string(std::string{}, std::string{}) }
        -> std::same_as<input::text_input_presentation_string_delta>;
    { input::diff_text_input_presentation_byte_counts(byte_counts, byte_counts) }
        -> std::same_as<input::text_input_presentation_byte_count_deltas>;
    { input::diff_text_input_presentation_route_byte_diagnostics(
        route_byte_diagnostics,
        route_byte_diagnostics) }
        -> std::same_as<input::text_input_presentation_route_byte_diagnostics_diff>;
    { input::text_input_presentation_route_has_text_byte_diagnostics(route_kind) }
        -> std::same_as<bool>;
    { input::make_text_input_presentation_route_byte_diagnostics(route) }
        -> std::same_as<input::text_input_presentation_route_byte_diagnostics>;
    { input::find_text_input_presentation_route_byte_diagnostics(diagnostics) }
        -> std::same_as<input::text_input_presentation_route_byte_diagnostics>;
    { input::make_text_input_presentation_snapshot(model) }
        -> std::same_as<input::text_input_presentation_snapshot>;
    { input::make_text_input_presentation_snapshot(model, diagnostics) }
        -> std::same_as<input::text_input_presentation_snapshot>;
    { input::diff_text_input_presentation_snapshots(snapshot, snapshot) }
        -> std::same_as<input::text_input_presentation_diff>;
};

template <typename T>
concept TextEditTransactionReplayStepInterface = requires(T step) {
    { step.label } -> std::same_as<std::string&>;
    { step.operation } -> std::same_as<input::text_edit_transaction_operation&>;
    { step.text } -> std::same_as<std::string&>;
    { step.range } -> std::same_as<input::text_range&>;
};

template <typename T>
concept TextEditTransactionReplayOperationCountsInterface = requires(T counts) {
    { counts.none } -> std::same_as<std::size_t&>;
    { counts.focus } -> std::same_as<std::size_t&>;
    { counts.clear_focus } -> std::same_as<std::size_t&>;
    { counts.move_caret_to_start } -> std::same_as<std::size_t&>;
    { counts.move_caret_to_end } -> std::same_as<std::size_t&>;
    { counts.move_caret_left } -> std::same_as<std::size_t&>;
    { counts.move_caret_right } -> std::same_as<std::size_t&>;
    { counts.extend_selection_left } -> std::same_as<std::size_t&>;
    { counts.extend_selection_right } -> std::same_as<std::size_t&>;
    { counts.select_all } -> std::same_as<std::size_t&>;
    { counts.clear_selection } -> std::same_as<std::size_t&>;
    { counts.set_selection } -> std::same_as<std::size_t&>;
    { counts.commit_utf8 } -> std::same_as<std::size_t&>;
    { counts.backspace } -> std::same_as<std::size_t&>;
    { counts.delete_forward } -> std::same_as<std::size_t&>;
    { counts.set_preedit } -> std::same_as<std::size_t&>;
    { counts.commit_ime } -> std::same_as<std::size_t&>;
    { counts.cancel_ime } -> std::same_as<std::size_t&>;
    { counts.submit } -> std::same_as<std::size_t&>;
};

template <typename T>
concept TextEditTransactionReplayStepDiagnosticsInterface = requires(T diagnostics) {
    { diagnostics.label } -> std::same_as<std::string&>;
    { diagnostics.operation } -> std::same_as<input::text_edit_transaction_operation&>;
    { diagnostics.transaction } -> std::same_as<input::text_edit_transaction_diagnostics&>;
    { diagnostics.before_presentation } -> std::same_as<input::text_input_presentation_snapshot&>;
    { diagnostics.after_presentation } -> std::same_as<input::text_input_presentation_snapshot&>;
    { diagnostics.presentation_diff } -> std::same_as<input::text_input_presentation_diff&>;
    { diagnostics.committed_text_bytes_before } -> std::same_as<std::size_t&>;
    { diagnostics.committed_text_bytes_after } -> std::same_as<std::size_t&>;
    { diagnostics.display_text_bytes_before } -> std::same_as<std::size_t&>;
    { diagnostics.display_text_bytes_after } -> std::same_as<std::size_t&>;
    { diagnostics.caret_before } -> std::same_as<input::text_range&>;
    { diagnostics.caret_after } -> std::same_as<input::text_range&>;
    { diagnostics.had_selection_before } -> std::same_as<bool&>;
    { diagnostics.has_selection_after } -> std::same_as<bool&>;
    { diagnostics.had_preedit_before } -> std::same_as<bool&>;
    { diagnostics.has_preedit_after } -> std::same_as<bool&>;
    { diagnostics.had_submit_before } -> std::same_as<bool&>;
    { diagnostics.has_submit_after } -> std::same_as<bool&>;
    { diagnostics.accepted } -> std::same_as<bool&>;
    { diagnostics.rejected } -> std::same_as<bool&>;
    { diagnostics.invalid_edit_rejected } -> std::same_as<bool&>;
    { diagnostics.utf8_boundary_safe } -> std::same_as<bool&>;
    { diagnostics.selection_replaced_committed_text } -> std::same_as<bool&>;
    { diagnostics.selection_replaced_display_text } -> std::same_as<bool&>;
    { diagnostics.selection_deleted } -> std::same_as<bool&>;
    { diagnostics.selection_cleared } -> std::same_as<bool&>;
    { diagnostics.ime_preedit_started } -> std::same_as<bool&>;
    { diagnostics.ime_preedit_updated } -> std::same_as<bool&>;
    { diagnostics.ime_preedit_cleared } -> std::same_as<bool&>;
    { diagnostics.ime_committed } -> std::same_as<bool&>;
    { diagnostics.ime_canceled } -> std::same_as<bool&>;
};

template <typename T>
concept TextEditTransactionReplayAggregateCountsInterface = requires(T counts) {
    { counts.operations } -> std::same_as<input::text_edit_transaction_replay_operation_counts&>;
    { counts.step_count } -> std::same_as<std::size_t&>;
    { counts.accepted_count } -> std::same_as<std::size_t&>;
    { counts.rejected_count } -> std::same_as<std::size_t&>;
    { counts.invalid_edit_rejected_count } -> std::same_as<std::size_t&>;
    { counts.utf8_boundary_safe_count } -> std::same_as<std::size_t&>;
    { counts.utf8_boundary_unsafe_count } -> std::same_as<std::size_t&>;
    { counts.text_changed_count } -> std::same_as<std::size_t&>;
    { counts.display_text_changed_count } -> std::same_as<std::size_t&>;
    { counts.caret_changed_count } -> std::same_as<std::size_t&>;
    { counts.selection_changed_count } -> std::same_as<std::size_t&>;
    { counts.preedit_changed_count } -> std::same_as<std::size_t&>;
    { counts.submit_available_after_count } -> std::same_as<std::size_t&>;
    { counts.selection_replaced_committed_text_count } -> std::same_as<std::size_t&>;
    { counts.selection_replaced_display_text_count } -> std::same_as<std::size_t&>;
    { counts.selection_deleted_count } -> std::same_as<std::size_t&>;
    { counts.selection_cleared_count } -> std::same_as<std::size_t&>;
    { counts.ime_preedit_started_count } -> std::same_as<std::size_t&>;
    { counts.ime_preedit_updated_count } -> std::same_as<std::size_t&>;
    { counts.ime_preedit_cleared_count } -> std::same_as<std::size_t&>;
    { counts.ime_committed_count } -> std::same_as<std::size_t&>;
    { counts.ime_canceled_count } -> std::same_as<std::size_t&>;
    { counts.inserted_byte_count } -> std::same_as<std::size_t&>;
    { counts.deleted_byte_count } -> std::same_as<std::size_t&>;
    { counts.replaced_byte_count } -> std::same_as<std::size_t&>;
    { counts.committed_byte_delta } -> std::same_as<std::int64_t&>;
    { counts.display_byte_delta } -> std::same_as<std::int64_t&>;
};

template <typename T>
concept TextEditTransactionReplaySummaryInterface = requires(T summary) {
    { summary.steps } -> std::same_as<std::vector<input::text_edit_transaction_replay_step_diagnostics>&>;
    { summary.counts } -> std::same_as<input::text_edit_transaction_replay_aggregate_counts&>;
    { summary.final_presentation } -> std::same_as<input::text_input_presentation_snapshot&>;
    { summary.final_transaction } -> std::same_as<input::text_edit_transaction_diagnostics&>;
    { summary.final_submit_available } -> std::same_as<bool&>;
    { summary.final_utf8_boundary_safe } -> std::same_as<bool&>;
};

template <typename T>
concept TextEditTransactionReplayCountDeltaInterface = requires(T delta) {
    { delta.before_count } -> std::same_as<std::size_t&>;
    { delta.after_count } -> std::same_as<std::size_t&>;
    { delta.delta } -> std::same_as<std::int64_t&>;
    { delta.changed } -> std::same_as<bool&>;
};

template <typename T>
concept TextEditTransactionReplayIntDeltaInterface = requires(T delta) {
    { delta.before_value } -> std::same_as<std::int64_t&>;
    { delta.after_value } -> std::same_as<std::int64_t&>;
    { delta.delta } -> std::same_as<std::int64_t&>;
    { delta.changed } -> std::same_as<bool&>;
};

template <typename T>
concept TextEditTransactionReplayOperationCountDeltasInterface = requires(T deltas) {
    { deltas.none } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.focus } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.clear_focus } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.move_caret_to_start } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.move_caret_to_end } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.move_caret_left } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.move_caret_right } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.extend_selection_left } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.extend_selection_right } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.select_all } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.clear_selection } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.set_selection } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.commit_utf8 } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.backspace } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.delete_forward } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.set_preedit } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.commit_ime } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.cancel_ime } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.submit } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.changed } -> std::same_as<bool&>;
};

template <typename T>
concept TextEditTransactionReplayAggregateCountDeltasInterface = requires(T deltas) {
    { deltas.operations } -> std::same_as<input::text_edit_transaction_replay_operation_count_deltas&>;
    { deltas.step_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.accepted_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.rejected_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.invalid_edit_rejected_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.utf8_boundary_safe_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.utf8_boundary_unsafe_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.text_changed_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.display_text_changed_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.caret_changed_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.selection_changed_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.preedit_changed_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.submit_available_after_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.selection_replaced_committed_text_count }
        -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.selection_replaced_display_text_count }
        -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.selection_deleted_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.selection_cleared_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.ime_preedit_started_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.ime_preedit_updated_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.ime_preedit_cleared_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.ime_committed_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.ime_canceled_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.inserted_byte_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.deleted_byte_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.replaced_byte_count } -> std::same_as<input::text_edit_transaction_replay_count_delta&>;
    { deltas.committed_byte_delta } -> std::same_as<input::text_edit_transaction_replay_int_delta&>;
    { deltas.display_byte_delta } -> std::same_as<input::text_edit_transaction_replay_int_delta&>;
    { deltas.changed } -> std::same_as<bool&>;
};

template <typename T>
concept TextEditTransactionReplayDiffInterface = requires(T diff) {
    { diff.counts } -> std::same_as<input::text_edit_transaction_replay_aggregate_count_deltas&>;
    { diff.final_presentation } -> std::same_as<input::text_input_presentation_diff&>;
    { diff.final_presentation_changed } -> std::same_as<bool&>;
    { diff.invalid_edit_changed } -> std::same_as<bool&>;
    { diff.utf8_boundary_changed } -> std::same_as<bool&>;
    { diff.replacement_changed } -> std::same_as<bool&>;
    { diff.ime_transition_changed } -> std::same_as<bool&>;
    { diff.submit_changed } -> std::same_as<bool&>;
    { diff.changed_category_count } -> std::same_as<std::size_t&>;
    { diff.changed } -> std::same_as<bool&>;
};

template <typename T>
concept TextEditTransactionReplayFunctions = requires(
    input::text_input_model& model,
    input::text_edit_transaction_replay_operation_counts& operation_counts,
    input::text_edit_transaction_replay_aggregate_counts& aggregate_counts,
    input::text_edit_transaction_operation operation,
    const input::text_edit_transaction_replay_step& step,
    const input::text_edit_transaction_replay_step_diagnostics& step_diagnostics,
    const input::text_input_presentation_snapshot& presentation,
    const input::text_edit_transaction_replay_operation_counts& before_operations,
    const input::text_edit_transaction_replay_operation_counts& after_operations,
    const input::text_edit_transaction_replay_aggregate_counts& before_counts,
    const input::text_edit_transaction_replay_aggregate_counts& after_counts,
    const input::text_edit_transaction_replay_operation_count_deltas& operation_deltas,
    const input::text_edit_transaction_replay_summary& summary,
    std::span<const input::text_edit_transaction_replay_step> steps) {
    { input::count_text_edit_transaction_replay_operation(operation_counts, operation) }
        -> std::same_as<void>;
    { input::make_text_edit_transaction_replay_snapshot(presentation) }
        -> std::same_as<input::text_edit_transaction_snapshot>;
    { input::make_text_edit_transaction_replay_noop_transaction(presentation) }
        -> std::same_as<input::text_edit_transaction_diagnostics>;
    { input::apply_text_edit_transaction_replay_step(model, step) } -> std::same_as<void>;
    { input::record_text_edit_transaction_replay_step(model, step) }
        -> std::same_as<input::text_edit_transaction_replay_step_diagnostics>;
    { input::accumulate_text_edit_transaction_replay_counts(aggregate_counts, step_diagnostics) }
        -> std::same_as<void>;
    { input::replay_text_edit_transactions(model, steps) }
        -> std::same_as<input::text_edit_transaction_replay_summary>;
    { input::replay_text_edit_transactions(steps) }
        -> std::same_as<input::text_edit_transaction_replay_summary>;
    { input::text_edit_transaction_replay_size_delta(std::size_t{}, std::size_t{}) }
        -> std::same_as<std::int64_t>;
    { input::diff_text_edit_transaction_replay_count(std::size_t{}, std::size_t{}) }
        -> std::same_as<input::text_edit_transaction_replay_count_delta>;
    { input::diff_text_edit_transaction_replay_int(std::int64_t{}, std::int64_t{}) }
        -> std::same_as<input::text_edit_transaction_replay_int_delta>;
    { input::text_edit_transaction_replay_operation_deltas_changed(operation_deltas) }
        -> std::same_as<bool>;
    { input::diff_text_edit_transaction_replay_operation_counts(before_operations, after_operations) }
        -> std::same_as<input::text_edit_transaction_replay_operation_count_deltas>;
    { input::diff_text_edit_transaction_replay_aggregate_counts(before_counts, after_counts) }
        -> std::same_as<input::text_edit_transaction_replay_aggregate_count_deltas>;
    { input::diff_text_edit_transaction_replay_summaries(summary, summary) }
        -> std::same_as<input::text_edit_transaction_replay_diff>;
};

static_assert(ImeCompositionStateInterface<input::ime_composition_state>);
static_assert(std::is_default_constructible_v<input::ime_composition_state>);
static_assert(PointerCaptureSnapshotInterface<input::pointer_capture_snapshot>);
static_assert(GesturePolicySnapshotInterface<input::gesture_policy_snapshot>);
static_assert(NormalizedInputEventSummaryInterface<input::normalized_input_event_summary>);
static_assert(ActionRoutePolicyDiagnosticInterface<input::action_route_policy_diagnostic>);
static_assert(KeyboardModifierStateInterface<input::keyboard_modifier_state>);
static_assert(KeyboardChordDiagnosticInterface<input::keyboard_chord_diagnostic>);
static_assert(NormalizedInputEventKindCountsInterface<input::normalized_input_event_kind_counts>);
static_assert(InputRouteKindCountsInterface<input::input_route_kind_counts>);
static_assert(InputDiagnosticSummaryInterface<input::input_diagnostic_summary>);
static_assert(InputRoutingDiagnosticsInterface<input::input_routing_diagnostics>);
static_assert(InputRoutingCountDeltaInterface<input::input_routing_count_delta>);
static_assert(InputRoutingBoolDeltaInterface<input::input_routing_bool_delta>);
static_assert(InputRoutingFloatDeltaInterface<input::input_routing_float_delta>);
static_assert(InputRoutingInt64DeltaInterface<input::input_routing_int64_delta>);
static_assert(InputRoutingPointerCaptureDeltaInterface<input::input_routing_pointer_capture_delta>);
static_assert(InputRoutingGesturePolicyThresholdDeltasInterface<
    input::input_routing_gesture_policy_threshold_deltas>);
static_assert(InputRoutingGesturePolicyRouteDiffInterface<input::input_routing_gesture_policy_route_diff>);
static_assert(InputRoutingGesturePolicyDiffInterface<input::input_routing_gesture_policy_diff>);
static_assert(NormalizedInputEventKindCountDeltasInterface<input::normalized_input_event_kind_count_deltas>);
static_assert(InputRouteKindCountDeltasInterface<input::input_route_kind_count_deltas>);
static_assert(ActionRoutePolicyKindCountsInterface<input::action_route_policy_kind_counts>);
static_assert(ActionRoutePolicyKindCountDeltasInterface<input::action_route_policy_kind_count_deltas>);
static_assert(InputRoutingKeyboardIntentCountsInterface<input::input_routing_keyboard_intent_counts>);
static_assert(InputRoutingKeyboardRepeatPolicyCountsInterface<
    input::input_routing_keyboard_repeat_policy_counts>);
static_assert(InputRoutingKeyboardRouteCountsInterface<input::input_routing_keyboard_route_counts>);
static_assert(InputRoutingKeyboardIntentCountDeltasInterface<input::input_routing_keyboard_intent_count_deltas>);
static_assert(InputRoutingKeyboardRepeatPolicyCountDeltasInterface<
    input::input_routing_keyboard_repeat_policy_count_deltas>);
static_assert(InputRoutingKeyboardRouteCountDeltasInterface<
    input::input_routing_keyboard_route_count_deltas>);
static_assert(InputRoutingDiagnosticsDiffInterface<input::input_routing_diagnostics_diff>);
static_assert(InputRoutingDiagnosticFunctions<void>);
static_assert(GestureRecognizerInterface<input::gesture_recognizer>);
static_assert(PlatformInputTranslatorInterface<input::platform_input_translator>);
static_assert(PlatformMouseSampleInterface<input::platform_mouse_sample>);
static_assert(PlatformTouchSampleInterface<input::platform_touch_sample>);
static_assert(PlatformWheelSampleInterface<input::platform_wheel_sample>);
static_assert(PlatformKeySampleInterface<input::platform_key_sample>);
static_assert(PlatformCharacterSampleInterface<input::platform_character_sample>);
static_assert(PlatformImeCompositionSampleInterface<input::platform_ime_composition_sample>);
static_assert(PlatformInputTranslationDiagnosticInterface<input::platform_input_translation_diagnostic>);
static_assert(PlatformInputTranslationResultInterface<input::platform_input_translation_result>);
static_assert(PlatformInputDispatchResultInterface<input::platform_input_dispatch_result>);
static_assert(PlatformInputDispatchBatchResultInterface<input::platform_input_dispatch_batch_result>);
static_assert(PlatformInputEngineAdapterFunctions<void>);
static_assert(PlatformInputReplayStepInterface<input::platform_input_replay_step>);
static_assert(PlatformInputReplayOptionsInterface<input::platform_input_replay_options>);
static_assert(PlatformInputReplayEventCountsInterface<input::platform_input_replay_event_counts>);
static_assert(PlatformInputReplayRejectionCountsInterface<input::platform_input_replay_rejection_counts>);
static_assert(PlatformInputReplayTranslationCountsInterface<
    input::platform_input_replay_translation_counts>);
static_assert(PlatformInputReplayStepDiagnosticsInterface<
    input::platform_input_replay_step_diagnostics>);
static_assert(PlatformInputReplaySummaryInterface<input::platform_input_replay_summary>);
static_assert(PlatformInputReplayEventCountDeltasInterface<
    input::platform_input_replay_event_count_deltas>);
static_assert(PlatformInputReplayRejectionCountDeltasInterface<
    input::platform_input_replay_rejection_count_deltas>);
static_assert(PlatformInputReplayTranslationCountDeltasInterface<
    input::platform_input_replay_translation_count_deltas>);
static_assert(PlatformInputReplayDiffInterface<input::platform_input_replay_diff>);
static_assert(PlatformInputReplayFunctions<void>);
static_assert(NormalizedInputReplayEndStateInterface<input::normalized_input_replay_end_state>);
static_assert(NormalizedInputReplayBatchInterface<input::normalized_input_replay_batch>);
static_assert(NormalizedInputReplayRecordingInterface<input::normalized_input_replay_recording>);
static_assert(NormalizedInputReplayStepInterface<input::normalized_input_replay_step>);
static_assert(NormalizedInputReplayKeyboardIntentCountsInterface<
    input::normalized_input_replay_keyboard_intent_counts>);
static_assert(NormalizedInputReplayKeyboardModifierCountsInterface<
    input::normalized_input_replay_keyboard_modifier_counts>);
static_assert(NormalizedInputReplayKeyboardRepeatPolicyCountsInterface<
    input::normalized_input_replay_keyboard_repeat_policy_counts>);
static_assert(NormalizedInputReplayKeyboardSummaryInterface<input::normalized_input_replay_keyboard_summary>);
static_assert(NormalizedInputReplayGesturePolicySummaryInterface<
    input::normalized_input_replay_gesture_policy_summary>);
static_assert(NormalizedInputReplayImePhaseCountsInterface<input::normalized_input_replay_ime_phase_counts>);
static_assert(NormalizedInputReplayImeTimelineEntryInterface<input::normalized_input_replay_ime_timeline_entry>);
static_assert(NormalizedInputReplayImeSummaryInterface<input::normalized_input_replay_ime_summary>);
static_assert(NormalizedInputReplayPointerTimelineCountsInterface<
    input::normalized_input_replay_pointer_timeline_counts>);
static_assert(NormalizedInputReplayPointerContactCountsInterface<
    input::normalized_input_replay_pointer_contact_counts>);
static_assert(NormalizedInputReplayPointerCaptureLifecycleCountsInterface<
    input::normalized_input_replay_pointer_capture_lifecycle_counts>);
static_assert(NormalizedInputReplayPointerDecisionCountsInterface<
    input::normalized_input_replay_pointer_decision_counts>);
static_assert(NormalizedInputReplayPointerTimelineEntryInterface<
    input::normalized_input_replay_pointer_timeline_entry>);
static_assert(NormalizedInputReplayPointerSummaryInterface<input::normalized_input_replay_pointer_summary>);
static_assert(NormalizedInputReplayFocusTimelineCountsInterface<
    input::normalized_input_replay_focus_timeline_counts>);
static_assert(NormalizedInputReplayFocusTimelineEntryInterface<
    input::normalized_input_replay_focus_timeline_entry>);
static_assert(NormalizedInputReplayFocusSummaryInterface<input::normalized_input_replay_focus_summary>);
static_assert(NormalizedInputReplayCountDeltaInterface<input::normalized_input_replay_count_delta>);
static_assert(NormalizedInputReplayBoolDeltaInterface<input::normalized_input_replay_bool_delta>);
static_assert(NormalizedInputReplayRangeDeltaInterface<input::normalized_input_replay_range_delta>);
static_assert(NormalizedInputReplayStringDeltaInterface<input::normalized_input_replay_string_delta>);
static_assert(NormalizedInputReplayPointerCaptureDeltaInterface<
    input::normalized_input_replay_pointer_capture_delta>);
static_assert(NormalizedInputReplayFinalStateDiffInterface<
    input::normalized_input_replay_final_state_diff>);
static_assert(NormalizedInputReplayKeyboardDiffInterface<input::normalized_input_replay_keyboard_diff>);
static_assert(NormalizedInputReplayPointerDiffInterface<input::normalized_input_replay_pointer_diff>);
static_assert(NormalizedInputReplayImeDiffInterface<input::normalized_input_replay_ime_diff>);
static_assert(NormalizedInputReplayFocusDiffInterface<input::normalized_input_replay_focus_diff>);
static_assert(NormalizedInputReplayRegressionSummaryInterface<
    input::normalized_input_replay_regression_summary>);
static_assert(NormalizedInputReplayDiffInterface<input::normalized_input_replay_diff>);
static_assert(NormalizedInputReplayFunctions<void>);
static_assert(InputActionCandidateCountsInterface<input::input_action_candidate_counts>);
static_assert(InputActionCandidateInterface<input::input_action_candidate>);
static_assert(InputActionCandidateBatchPlanInterface<input::input_action_candidate_batch_plan>);
static_assert(InputActionCandidatePlanInterface<input::input_action_candidate_plan>);
static_assert(InputActionCandidatePlanFunctions<void>);
static_assert(InputActionCandidateResultCountsInterface<input::input_action_candidate_result_counts>);
static_assert(InputActionCandidateResultInterface<input::input_action_candidate_result>);
static_assert(InputActionCandidateBatchResolutionInterface<
    input::input_action_candidate_batch_resolution>);
static_assert(InputActionCandidateResolutionInterface<input::input_action_candidate_resolution>);
static_assert(InputActionCandidateResolutionFunctions<void>);
static_assert(InputActionResolutionResultSummaryInterface<
    input::input_action_resolution_result_summary>);
static_assert(InputActionResolutionBatchSummaryInterface<
    input::input_action_resolution_batch_summary>);
static_assert(InputActionResolutionReplaySummaryInterface<
    input::input_action_resolution_replay_summary>);
static_assert(InputActionResolutionSummaryFunctions<void>);
static_assert(InputActionCandidateCountDeltasInterface<input::input_action_candidate_count_deltas>);
static_assert(InputActionCandidateResultCountDeltasInterface<
    input::input_action_candidate_result_count_deltas>);
static_assert(InputActionResolutionReasonCountsInterface<input::input_action_resolution_reason_counts>);
static_assert(InputActionResolutionReasonCountDeltasInterface<
    input::input_action_resolution_reason_count_deltas>);
static_assert(InputActionResolutionTargetSnapshotInterface<
    input::input_action_resolution_target_snapshot>);
static_assert(InputActionResolutionTargetDeltaInterface<input::input_action_resolution_target_delta>);
static_assert(InputActionResolutionReplayClassificationInterface<
    input::input_action_resolution_replay_classification>);
static_assert(InputActionResolutionReplaySummaryDiffInterface<
    input::input_action_resolution_replay_summary_diff>);
static_assert(InputActionResolutionDiffFunctions<void>);
static_assert(std::is_default_constructible_v<input::platform_input_translation_request>);
static_assert(std::is_default_constructible_v<input::platform_input_translation_result>);
static_assert(std::is_default_constructible_v<input::platform_input_dispatch_result>);
static_assert(std::is_default_constructible_v<input::platform_input_dispatch_batch_result>);
static_assert(std::is_default_constructible_v<input::platform_input_replay_step>);
static_assert(std::is_default_constructible_v<input::platform_input_replay_options>);
static_assert(std::is_default_constructible_v<input::platform_input_replay_event_counts>);
static_assert(std::is_default_constructible_v<input::platform_input_replay_rejection_counts>);
static_assert(std::is_default_constructible_v<input::platform_input_replay_translation_counts>);
static_assert(std::is_default_constructible_v<input::platform_input_replay_step_diagnostics>);
static_assert(std::is_default_constructible_v<input::platform_input_replay_summary>);
static_assert(std::is_default_constructible_v<input::platform_input_replay_event_count_deltas>);
static_assert(std::is_default_constructible_v<input::platform_input_replay_rejection_count_deltas>);
static_assert(std::is_default_constructible_v<input::platform_input_replay_translation_count_deltas>);
static_assert(std::is_default_constructible_v<input::platform_input_replay_diff>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_time_update>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_options>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_end_state>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_batch>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_recording>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_keyboard_intent_counts>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_keyboard_modifier_counts>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_keyboard_repeat_policy_counts>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_keyboard_summary>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_gesture_policy_summary>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_ime_phase_counts>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_ime_timeline_entry>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_ime_summary>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_pointer_timeline_counts>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_pointer_contact_counts>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_pointer_capture_lifecycle_counts>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_pointer_decision_counts>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_pointer_timeline_entry>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_pointer_summary>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_focus_timeline_counts>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_focus_timeline_entry>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_focus_summary>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_count_delta>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_bool_delta>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_range_delta>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_string_delta>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_pointer_capture_delta>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_final_state_diff>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_keyboard_intent_count_deltas>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_keyboard_modifier_count_deltas>);
static_assert(std::is_default_constructible_v<
    input::normalized_input_replay_keyboard_repeat_policy_count_deltas>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_keyboard_diff>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_pointer_timeline_count_deltas>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_pointer_diff>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_ime_phase_count_deltas>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_ime_diff>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_focus_timeline_count_deltas>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_focus_diff>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_regression_summary>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_diff>);
static_assert(std::is_default_constructible_v<input::input_action_candidate_counts>);
static_assert(std::is_default_constructible_v<input::input_action_candidate>);
static_assert(std::is_default_constructible_v<input::input_action_candidate_batch_plan>);
static_assert(std::is_default_constructible_v<input::input_action_candidate_plan>);
static_assert(std::is_default_constructible_v<input::input_action_candidate_result_counts>);
static_assert(std::is_default_constructible_v<input::input_action_candidate_result>);
static_assert(std::is_default_constructible_v<input::input_action_candidate_batch_resolution>);
static_assert(std::is_default_constructible_v<input::input_action_candidate_resolution>);
static_assert(std::is_default_constructible_v<input::input_action_resolution_result_summary>);
static_assert(std::is_default_constructible_v<input::input_action_resolution_batch_summary>);
static_assert(std::is_default_constructible_v<input::input_action_resolution_replay_summary>);
static_assert(std::is_default_constructible_v<input::input_action_candidate_count_deltas>);
static_assert(std::is_default_constructible_v<input::input_action_candidate_result_count_deltas>);
static_assert(std::is_default_constructible_v<input::input_action_resolution_reason_counts>);
static_assert(std::is_default_constructible_v<input::input_action_resolution_reason_count_deltas>);
static_assert(std::is_default_constructible_v<input::input_action_resolution_target_snapshot>);
static_assert(std::is_default_constructible_v<input::input_action_resolution_target_delta>);
static_assert(std::is_default_constructible_v<input::input_action_resolution_replay_classification>);
static_assert(std::is_default_constructible_v<input::input_action_resolution_replay_summary_diff>);
static_assert(std::is_default_constructible_v<input::input_routing_count_delta>);
static_assert(std::is_default_constructible_v<input::input_routing_bool_delta>);
static_assert(std::is_default_constructible_v<input::input_routing_float_delta>);
static_assert(std::is_default_constructible_v<input::input_routing_int64_delta>);
static_assert(std::is_default_constructible_v<input::input_routing_pointer_capture_delta>);
static_assert(std::is_default_constructible_v<input::input_routing_gesture_policy_threshold_deltas>);
static_assert(std::is_default_constructible_v<input::input_routing_gesture_policy_route_diff>);
static_assert(std::is_default_constructible_v<input::input_routing_gesture_policy_diff>);
static_assert(std::is_default_constructible_v<input::normalized_input_event_kind_count_deltas>);
static_assert(std::is_default_constructible_v<input::input_route_kind_count_deltas>);
static_assert(std::is_default_constructible_v<input::action_route_policy_kind_counts>);
static_assert(std::is_default_constructible_v<input::action_route_policy_kind_count_deltas>);
static_assert(std::is_default_constructible_v<input::input_routing_keyboard_intent_counts>);
static_assert(std::is_default_constructible_v<input::input_routing_keyboard_repeat_policy_counts>);
static_assert(std::is_default_constructible_v<input::input_routing_keyboard_route_counts>);
static_assert(std::is_default_constructible_v<input::input_routing_keyboard_intent_count_deltas>);
static_assert(std::is_default_constructible_v<input::input_routing_keyboard_repeat_policy_count_deltas>);
static_assert(std::is_default_constructible_v<input::input_routing_keyboard_route_count_deltas>);
static_assert(std::is_default_constructible_v<input::input_routing_diagnostics_diff>);
static_assert(std::is_default_constructible_v<input::text_edit_transaction_snapshot>);
static_assert(std::is_default_constructible_v<input::text_edit_transaction_byte_diagnostics>);
static_assert(std::is_default_constructible_v<input::text_edit_transaction_diagnostics>);
static_assert(std::is_default_constructible_v<input::text_input_presentation_byte_counts>);
static_assert(std::is_default_constructible_v<input::text_input_presentation_route_byte_diagnostics>);
static_assert(std::is_default_constructible_v<input::text_input_presentation_snapshot>);
static_assert(std::is_default_constructible_v<input::text_input_presentation_count_delta>);
static_assert(std::is_default_constructible_v<input::text_input_presentation_bool_delta>);
static_assert(std::is_default_constructible_v<input::text_input_presentation_int_delta>);
static_assert(std::is_default_constructible_v<input::text_input_presentation_range_delta>);
static_assert(std::is_default_constructible_v<input::text_input_presentation_string_delta>);
static_assert(std::is_default_constructible_v<input::text_input_presentation_byte_count_deltas>);
static_assert(std::is_default_constructible_v<input::text_input_presentation_route_byte_diagnostics_diff>);
static_assert(std::is_default_constructible_v<input::text_input_presentation_diff>);
static_assert(std::is_default_constructible_v<input::text_edit_transaction_replay_step>);
static_assert(std::is_default_constructible_v<input::text_edit_transaction_replay_operation_counts>);
static_assert(std::is_default_constructible_v<input::text_edit_transaction_replay_step_diagnostics>);
static_assert(std::is_default_constructible_v<input::text_edit_transaction_replay_aggregate_counts>);
static_assert(std::is_default_constructible_v<input::text_edit_transaction_replay_summary>);
static_assert(std::is_default_constructible_v<input::text_edit_transaction_replay_count_delta>);
static_assert(std::is_default_constructible_v<input::text_edit_transaction_replay_int_delta>);
static_assert(std::is_default_constructible_v<input::text_edit_transaction_replay_operation_count_deltas>);
static_assert(std::is_default_constructible_v<input::text_edit_transaction_replay_aggregate_count_deltas>);
static_assert(std::is_default_constructible_v<input::text_edit_transaction_replay_diff>);
static_assert(std::is_default_constructible_v<input::keyboard_modifier_state>);
static_assert(std::is_default_constructible_v<input::keyboard_chord_diagnostic>);
static_assert(!std::is_polymorphic_v<input::platform_input_translation_request>);
static_assert(!std::is_polymorphic_v<input::platform_input_translation_result>);
static_assert(!std::is_polymorphic_v<input::platform_input_dispatch_result>);
static_assert(!std::is_polymorphic_v<input::platform_input_dispatch_batch_result>);
static_assert(!std::is_polymorphic_v<input::platform_input_replay_step>);
static_assert(!std::is_polymorphic_v<input::platform_input_replay_step_diagnostics>);
static_assert(!std::is_polymorphic_v<input::platform_input_replay_summary>);
static_assert(!std::is_polymorphic_v<input::platform_input_replay_diff>);
static_assert(!std::is_polymorphic_v<input::normalized_input_replay_step>);
static_assert(!std::is_polymorphic_v<input::normalized_input_replay_batch>);
static_assert(!std::is_polymorphic_v<input::normalized_input_replay_recording>);
static_assert(!std::is_polymorphic_v<input::normalized_input_replay_keyboard_summary>);
static_assert(!std::is_polymorphic_v<input::normalized_input_replay_gesture_policy_summary>);
static_assert(!std::is_polymorphic_v<input::normalized_input_replay_ime_summary>);
static_assert(!std::is_polymorphic_v<input::normalized_input_replay_pointer_summary>);
static_assert(!std::is_polymorphic_v<input::normalized_input_replay_focus_summary>);
static_assert(!std::is_polymorphic_v<input::normalized_input_replay_diff>);
static_assert(!std::is_polymorphic_v<input::input_action_candidate>);
static_assert(!std::is_polymorphic_v<input::input_action_candidate_batch_plan>);
static_assert(!std::is_polymorphic_v<input::input_action_candidate_plan>);
static_assert(!std::is_polymorphic_v<input::input_action_candidate_result>);
static_assert(!std::is_polymorphic_v<input::input_action_candidate_batch_resolution>);
static_assert(!std::is_polymorphic_v<input::input_action_candidate_resolution>);
static_assert(!std::is_polymorphic_v<input::input_action_resolution_result_summary>);
static_assert(!std::is_polymorphic_v<input::input_action_resolution_batch_summary>);
static_assert(!std::is_polymorphic_v<input::input_action_resolution_replay_summary>);
static_assert(!std::is_polymorphic_v<input::input_action_candidate_count_deltas>);
static_assert(!std::is_polymorphic_v<input::input_action_candidate_result_count_deltas>);
static_assert(!std::is_polymorphic_v<input::input_action_resolution_reason_counts>);
static_assert(!std::is_polymorphic_v<input::input_action_resolution_reason_count_deltas>);
static_assert(!std::is_polymorphic_v<input::input_action_resolution_target_snapshot>);
static_assert(!std::is_polymorphic_v<input::input_action_resolution_target_delta>);
static_assert(!std::is_polymorphic_v<input::input_action_resolution_replay_classification>);
static_assert(!std::is_polymorphic_v<input::input_action_resolution_replay_summary_diff>);
static_assert(!std::is_polymorphic_v<input::input_routing_diagnostics_diff>);
static_assert(!std::is_polymorphic_v<input::input_routing_gesture_policy_diff>);
static_assert(!std::is_polymorphic_v<input::text_edit_transaction_snapshot>);
static_assert(!std::is_polymorphic_v<input::text_edit_transaction_byte_diagnostics>);
static_assert(!std::is_polymorphic_v<input::text_edit_transaction_diagnostics>);
static_assert(!std::is_polymorphic_v<input::text_input_presentation_snapshot>);
static_assert(!std::is_polymorphic_v<input::text_input_presentation_diff>);
static_assert(!std::is_polymorphic_v<input::text_edit_transaction_replay_step>);
static_assert(!std::is_polymorphic_v<input::text_edit_transaction_replay_summary>);
static_assert(!std::is_polymorphic_v<input::text_edit_transaction_replay_diff>);
static_assert(std::is_same_v<
    decltype(input::platform_input_translation_result{}.event),
    std::optional<raw_platform_input_event>>);
static_assert(std::is_same_v<
    decltype(input::platform_input_translation_result{}.diagnostic),
    input::platform_input_translation_diagnostic>);
static_assert(std::is_same_v<
    decltype(input::platform_input_dispatch_result{}.translation),
    input::platform_input_translation_result>);
static_assert(std::is_same_v<
    decltype(input::platform_input_dispatch_result{}.input_events),
    std::vector<input::input_event>>);
static_assert(std::is_same_v<
    decltype(input::platform_input_dispatch_result{}.routing_diagnostics),
    input::input_routing_diagnostics>);
static_assert(std::is_same_v<
    decltype(input::platform_input_dispatch_batch_result{}.items),
    std::vector<input::platform_input_dispatch_result>>);
static_assert(std::is_same_v<
    decltype(input::platform_input_dispatch_batch_result{}.summary),
    input::input_diagnostic_summary>);
static_assert(std::is_same_v<
    decltype(input::normalized_input_replay_batch{}.input_events),
    std::vector<input::input_event>>);
static_assert(std::is_same_v<
    decltype(input::normalized_input_replay_batch{}.normalized_events),
    std::vector<input::normalized_input_event_summary>>);
static_assert(std::is_same_v<
    decltype(input::normalized_input_replay_batch{}.summary),
    input::input_diagnostic_summary>);
static_assert(std::is_same_v<
    decltype(input::normalized_input_replay_batch{}.keyboard),
    input::normalized_input_replay_keyboard_summary>);
static_assert(std::is_same_v<
    decltype(input::normalized_input_replay_recording{}.keyboard),
    input::normalized_input_replay_keyboard_summary>);
static_assert(std::is_same_v<
    decltype(input::normalized_input_replay_batch{}.ime),
    input::normalized_input_replay_ime_summary>);
static_assert(std::is_same_v<
    decltype(input::normalized_input_replay_recording{}.ime),
    input::normalized_input_replay_ime_summary>);
static_assert(std::is_same_v<
    decltype(input::normalized_input_replay_batch{}.pointer),
    input::normalized_input_replay_pointer_summary>);
static_assert(std::is_same_v<
    decltype(input::normalized_input_replay_recording{}.pointer),
    input::normalized_input_replay_pointer_summary>);
static_assert(std::is_same_v<
    decltype(input::normalized_input_replay_batch{}.gesture_policies),
    input::normalized_input_replay_gesture_policy_summary>);
static_assert(std::is_same_v<
    decltype(input::normalized_input_replay_recording{}.gesture_policies),
    input::normalized_input_replay_gesture_policy_summary>);
static_assert(std::is_same_v<
    decltype(input::normalized_input_replay_batch{}.focus),
    input::normalized_input_replay_focus_summary>);
static_assert(std::is_same_v<
    decltype(input::normalized_input_replay_recording{}.focus),
    input::normalized_input_replay_focus_summary>);
static_assert(std::is_same_v<
    decltype(input::normalized_input_replay_batch{}.end_state.text_presentation),
    input::text_input_presentation_snapshot>);
static_assert(std::is_same_v<
    decltype(input::normalized_input_replay_batch{}.text_presentation_diff),
    input::text_input_presentation_diff>);
static_assert(TextInputModelInterface<input::text_input_model>);
static_assert(TextEditTransactionSnapshotInterface<input::text_edit_transaction_snapshot>);
static_assert(TextEditTransactionByteDiagnosticsInterface<input::text_edit_transaction_byte_diagnostics>);
static_assert(TextEditTransactionDiagnosticsInterface<input::text_edit_transaction_diagnostics>);
static_assert(TextInputPresentationByteCountsInterface<input::text_input_presentation_byte_counts>);
static_assert(TextInputPresentationRouteByteDiagnosticsInterface<
    input::text_input_presentation_route_byte_diagnostics>);
static_assert(TextInputPresentationSnapshotInterface<input::text_input_presentation_snapshot>);
static_assert(TextInputPresentationCountDeltaInterface<input::text_input_presentation_count_delta>);
static_assert(TextInputPresentationBoolDeltaInterface<input::text_input_presentation_bool_delta>);
static_assert(TextInputPresentationIntDeltaInterface<input::text_input_presentation_int_delta>);
static_assert(TextInputPresentationRangeDeltaInterface<input::text_input_presentation_range_delta>);
static_assert(TextInputPresentationStringDeltaInterface<input::text_input_presentation_string_delta>);
static_assert(TextInputPresentationByteCountDeltasInterface<input::text_input_presentation_byte_count_deltas>);
static_assert(TextInputPresentationRouteByteDiagnosticsDiffInterface<
    input::text_input_presentation_route_byte_diagnostics_diff>);
static_assert(TextInputPresentationDiffInterface<input::text_input_presentation_diff>);
static_assert(TextInputPresentationFunctions<void>);
static_assert(TextEditTransactionReplayStepInterface<input::text_edit_transaction_replay_step>);
static_assert(TextEditTransactionReplayOperationCountsInterface<
    input::text_edit_transaction_replay_operation_counts>);
static_assert(TextEditTransactionReplayStepDiagnosticsInterface<
    input::text_edit_transaction_replay_step_diagnostics>);
static_assert(TextEditTransactionReplayAggregateCountsInterface<
    input::text_edit_transaction_replay_aggregate_counts>);
static_assert(TextEditTransactionReplaySummaryInterface<input::text_edit_transaction_replay_summary>);
static_assert(TextEditTransactionReplayCountDeltaInterface<input::text_edit_transaction_replay_count_delta>);
static_assert(TextEditTransactionReplayIntDeltaInterface<input::text_edit_transaction_replay_int_delta>);
static_assert(TextEditTransactionReplayOperationCountDeltasInterface<
    input::text_edit_transaction_replay_operation_count_deltas>);
static_assert(TextEditTransactionReplayAggregateCountDeltasInterface<
    input::text_edit_transaction_replay_aggregate_count_deltas>);
static_assert(TextEditTransactionReplayDiffInterface<input::text_edit_transaction_replay_diff>);
static_assert(TextEditTransactionReplayFunctions<void>);

constexpr input::text_range text_range_contract{
    .start_byte = 3,
    .end_byte = 7,
};
static_assert(text_range_contract.start_byte == 3);
static_assert(text_range_contract.end_byte == 7);

constexpr input::text_event_kind caret_moved_contract_kind = input::text_event_kind::caret_moved;
static_assert(caret_moved_contract_kind == input::text_event_kind::caret_moved);

constexpr input::text_event_kind selection_changed_contract_kind = input::text_event_kind::selection_changed;
static_assert(selection_changed_contract_kind == input::text_event_kind::selection_changed);

constexpr input::text_event_kind delete_forward_contract_kind = input::text_event_kind::delete_forward;
static_assert(delete_forward_contract_kind == input::text_event_kind::delete_forward);

constexpr input::text_event_kind cancel_contract_kind = input::text_event_kind::cancel;
static_assert(cancel_contract_kind == input::text_event_kind::cancel);

constexpr input::keyboard_modifier_state keyboard_modifiers_contract{
    .alt = true,
    .ctrl = true,
    .shift = false,
    .meta = true,
};
static_assert(keyboard_modifiers_contract.alt);
static_assert(keyboard_modifiers_contract.ctrl);
static_assert(!keyboard_modifiers_contract.shift);
static_assert(keyboard_modifiers_contract.meta);

constexpr input::keyboard_chord_diagnostic keyboard_chord_contract{
    .logical_key = "Tab",
    .key_code = 9,
    .phase = raw_platform_key_phase::down,
    .modifiers = input::keyboard_modifier_state{
        .shift = true,
    },
    .repeat = true,
    .repeat_policy = input::keyboard_repeat_policy::ignored,
    .intent = input::keyboard_shortcut_intent::focus_traversal_previous,
};
static_assert(keyboard_chord_contract.key_code == 9);
static_assert(keyboard_chord_contract.modifiers.shift);
static_assert(keyboard_chord_contract.repeat);
static_assert(keyboard_chord_contract.repeat_policy == input::keyboard_repeat_policy::ignored);
static_assert(keyboard_chord_contract.intent == input::keyboard_shortcut_intent::focus_traversal_previous);

constexpr input::normalized_input_replay_keyboard_intent_counts replay_keyboard_intents_contract{
    .focus_traversal_next = 1,
    .submit = 2,
    .delete_forward = 3,
};
static_assert(replay_keyboard_intents_contract.focus_traversal_next == 1);
static_assert(replay_keyboard_intents_contract.submit == 2);
static_assert(replay_keyboard_intents_contract.delete_forward == 3);

constexpr input::normalized_input_replay_keyboard_modifier_counts replay_keyboard_modifiers_contract{
    .unmodified = 4,
    .alt = 1,
    .ctrl = 2,
    .shift = 3,
};
static_assert(replay_keyboard_modifiers_contract.unmodified == 4);
static_assert(replay_keyboard_modifiers_contract.ctrl == 2);
static_assert(replay_keyboard_modifiers_contract.shift == 3);

constexpr input::normalized_input_replay_keyboard_repeat_policy_counts replay_keyboard_repeat_contract{
    .not_repeat = 5,
    .allowed = 6,
    .ignored = 7,
};
static_assert(replay_keyboard_repeat_contract.not_repeat == 5);
static_assert(replay_keyboard_repeat_contract.allowed == 6);
static_assert(replay_keyboard_repeat_contract.ignored == 7);

constexpr input::normalized_input_replay_ime_phase_counts replay_ime_phases_contract{
    .composition_start = 1,
    .preedit_update = 2,
    .commit = 3,
    .cancel = 4,
};
static_assert(replay_ime_phases_contract.composition_start == 1);
static_assert(replay_ime_phases_contract.preedit_update == 2);
static_assert(replay_ime_phases_contract.commit == 3);
static_assert(replay_ime_phases_contract.cancel == 4);

constexpr input::normalized_input_replay_ime_timeline_entry replay_ime_entry_contract{
    .phase = input::normalized_input_replay_ime_timeline_phase::commit,
    .timestamp_ms = 80,
    .emits_input_event = true,
    .event_index = 1,
    .target_id = "answer",
    .utf8_text = "a",
    .committed_text = "a",
    .composition = input::ime_composition_state{},
    .text_byte_count = 1,
    .text_byte_count_before = 0,
    .text_byte_count_after = 1,
    .caret_before = input::text_range{},
    .caret_after = input::text_range{.start_byte = 1, .end_byte = 1},
    .had_selection_before = false,
    .has_selection_after = false,
    .selection_before = input::text_range{},
    .selection_after = input::text_range{},
    .preedit_text_valid = true,
    .preedit_range_valid = true,
    .stale_preedit_cleared_after = true,
    .committed_text_after = "a",
    .display_text_after = "a",
    .preedit_text_after = "",
};
static_assert(replay_ime_entry_contract.phase == input::normalized_input_replay_ime_timeline_phase::commit);
static_assert(replay_ime_entry_contract.timestamp_ms == 80);
static_assert(replay_ime_entry_contract.emits_input_event);
static_assert(replay_ime_entry_contract.text_byte_count_after == 1);
static_assert(replay_ime_entry_contract.preedit_text_valid);
static_assert(replay_ime_entry_contract.stale_preedit_cleared_after);

constexpr input::normalized_input_replay_pointer_timeline_counts replay_pointer_kinds_contract{
    .pointer_capture_arbitration = 1,
    .drag_start = 2,
    .wheel = 3,
};
static_assert(replay_pointer_kinds_contract.pointer_capture_arbitration == 1);
static_assert(replay_pointer_kinds_contract.drag_start == 2);
static_assert(replay_pointer_kinds_contract.wheel == 3);

constexpr input::normalized_input_replay_pointer_contact_counts replay_pointer_contacts_contract{
    .unknown = 1,
    .mouse_like = 2,
    .touch_like = 3,
};
static_assert(replay_pointer_contacts_contract.unknown == 1);
static_assert(replay_pointer_contacts_contract.mouse_like == 2);
static_assert(replay_pointer_contacts_contract.touch_like == 3);

constexpr input::normalized_input_replay_pointer_capture_lifecycle_counts replay_pointer_lifecycles_contract{
    .idle = 1,
    .tracking = 2,
    .captured = 3,
};
static_assert(replay_pointer_lifecycles_contract.idle == 1);
static_assert(replay_pointer_lifecycles_contract.tracking == 2);
static_assert(replay_pointer_lifecycles_contract.captured == 3);

constexpr input::normalized_input_replay_pointer_decision_counts replay_pointer_decisions_contract{
    .tracked = 1,
    .captured = 2,
    .released = 3,
};
static_assert(replay_pointer_decisions_contract.tracked == 1);
static_assert(replay_pointer_decisions_contract.captured == 2);
static_assert(replay_pointer_decisions_contract.released == 3);

constexpr input::normalized_input_replay_pointer_timeline_entry replay_pointer_entry_contract{
    .kind = input::normalized_input_replay_pointer_timeline_kind::drag_start,
    .timestamp_ms = 90,
    .emits_input_event = true,
    .event_index = 0,
    .pointer_id = 7,
    .event_phase = input::pointer_phase::move,
    .contact = input::pointer_contact_kind::touch_like,
    .decision = input::pointer_arbitration_decision::captured,
    .capture_before = input::pointer_capture_snapshot{
        .lifecycle = input::pointer_capture_lifecycle::tracking,
        .active = false,
        .pointer_id = 7,
        .tracked_pointer_count = 1,
    },
    .capture_after = input::pointer_capture_snapshot{
        .lifecycle = input::pointer_capture_lifecycle::captured,
        .active = true,
        .pointer_id = 7,
        .tracked_pointer_count = 1,
    },
    .tracked_pointer_count_before = 1,
    .tracked_pointer_count_after = 1,
    .normalized_event = input::normalized_input_event_summary{
        .kind = input::input_event_summary_kind::drag_start,
        .timestamp_ms = 90,
        .pointer_id = 7,
        .delta_x = 9.0f,
    },
    .gesture_policy = input::gesture_policy_snapshot{
        .decision = input::gesture_policy_decision::drag_started,
        .phase = input::pointer_phase::move,
        .timestamp_ms = 90,
        .pointer_id = 7,
        .delta_x = 9.0f,
        .emitted_input_event = true,
        .emitted_kind = input::gesture_kind::drag_start,
    },
    .capture_changed = true,
    .capture_ended_cleanly_after = false,
    .duration_ms = 20,
    .delta_x = 9.0f,
};
static_assert(replay_pointer_entry_contract.kind == input::normalized_input_replay_pointer_timeline_kind::drag_start);
static_assert(replay_pointer_entry_contract.pointer_id == 7);
static_assert(replay_pointer_entry_contract.contact == input::pointer_contact_kind::touch_like);
static_assert(replay_pointer_entry_contract.capture_after.lifecycle == input::pointer_capture_lifecycle::captured);
static_assert(replay_pointer_entry_contract.capture_changed);
static_assert(replay_pointer_entry_contract.delta_x == 9.0f);

constexpr input::normalized_input_replay_focus_timeline_counts replay_focus_counts_contract{
    .focus_gain = 1,
    .focus_loss = 2,
    .focus_traversal_next = 3,
    .caret_moved = 4,
    .selection_changed = 5,
};
static_assert(replay_focus_counts_contract.focus_gain == 1);
static_assert(replay_focus_counts_contract.focus_traversal_next == 3);
static_assert(replay_focus_counts_contract.caret_moved == 4);
static_assert(replay_focus_counts_contract.selection_changed == 5);

constexpr input::normalized_input_replay_focus_timeline_entry replay_focus_entry_contract{
    .kind = input::normalized_input_replay_focus_timeline_kind::selection_changed,
    .timestamp_ms = 100,
    .emits_input_event = true,
    .event_index = 0,
    .target_id = "answer",
    .target_id_before = "answer",
    .target_id_after = "answer",
    .had_focus_before = true,
    .has_focus_after = true,
    .target_changed = false,
    .text_byte_count_before = 5,
    .text_byte_count_after = 5,
    .caret_before = input::text_range{.start_byte = 1, .end_byte = 1},
    .caret_after = input::text_range{.start_byte = 4, .end_byte = 4},
    .caret_changed = true,
    .had_selection_before = false,
    .has_selection_after = true,
    .selection_before = input::text_range{},
    .selection_after = input::text_range{.start_byte = 1, .end_byte = 4},
    .selection_changed = true,
    .keyboard = input::keyboard_chord_diagnostic{
        .logical_key = "ArrowRight",
        .modifiers = input::keyboard_modifier_state{
            .shift = true,
        },
        .intent = input::keyboard_shortcut_intent::selection_next,
    },
    .focus_clean_after = true,
};
static_assert(replay_focus_entry_contract.kind
    == input::normalized_input_replay_focus_timeline_kind::selection_changed);
static_assert(replay_focus_entry_contract.target_id == "answer");
static_assert(replay_focus_entry_contract.caret_changed);
static_assert(replay_focus_entry_contract.selection_changed);
static_assert(replay_focus_entry_contract.keyboard.modifiers.shift);

static_assert(std::is_default_constructible_v<input::ime_event>);
static_assert(std::is_same_v<decltype(input::ime_event{}.composition), input::ime_composition_state>);

constexpr input::pointer_capture_snapshot captured_pointer_contract{
    .lifecycle = input::pointer_capture_lifecycle::captured,
    .active = true,
    .pointer_id = 3,
    .tracked_pointer_count = 1,
};
static_assert(captured_pointer_contract.lifecycle == input::pointer_capture_lifecycle::captured);
static_assert(captured_pointer_contract.active);
static_assert(captured_pointer_contract.pointer_id == 3);

constexpr input::normalized_input_event_summary wheel_summary_contract{
    .kind = input::input_event_summary_kind::wheel,
    .timestamp_ms = 30,
    .x = 4.0f,
    .y = 5.0f,
    .pixel_delta_y = -120.0f,
};
static_assert(wheel_summary_contract.kind == input::input_event_summary_kind::wheel);
static_assert(wheel_summary_contract.pixel_delta_y == -120.0f);

constexpr input::input_diagnostic_summary input_summary_contract{
    .normalized_events = input::normalized_input_event_kind_counts{
        .tap = 1,
        .wheel = 2,
    },
    .routes = input::input_route_kind_counts{
        .pointer = 1,
        .text = 2,
        .ime = 3,
        .focus = 4,
        .wheel = 5,
        .total = 15,
    },
    .normalized_event_count = 3,
    .pointer_capture_ended_cleanly = true,
    .focus_ended_cleanly = true,
    .preedit_ended_cleanly = true,
};
static_assert(input_summary_contract.normalized_events.tap == 1);
static_assert(input_summary_contract.normalized_events.wheel == 2);
static_assert(input_summary_contract.routes.total == 15);
static_assert(input_summary_contract.pointer_capture_ended_cleanly);

constexpr input::gesture_policy_snapshot swipe_policy_contract{
    .decision = input::gesture_policy_decision::swipe_accepted,
    .direction = input::gesture_direction::right,
    .phase = input::pointer_phase::up,
    .timestamp_ms = 50,
    .duration_ms = 40,
    .pointer_id = 2,
    .start_x = 0.0f,
    .start_y = 0.0f,
    .x = 70.0f,
    .y = 0.0f,
    .delta_x = 70.0f,
    .distance = 70.0f,
    .swipe_min_dx = 60.0f,
    .swipe_max_dy = 40.0f,
    .swipe_max_duration_ms = 800,
    .tap_slop = 8.0f,
    .drag_start_slop = 8.0f,
    .emitted_input_event = true,
    .emitted_kind = input::gesture_kind::swipe_right,
};
static_assert(swipe_policy_contract.decision == input::gesture_policy_decision::swipe_accepted);
static_assert(swipe_policy_contract.direction == input::gesture_direction::right);
static_assert(swipe_policy_contract.emitted_kind == input::gesture_kind::swipe_right);

constexpr input::action_route_policy_diagnostic submit_policy_contract{
    .kind = input::action_route_policy_kind::text_submit_boundary,
    .timestamp_ms = 40,
    .emits_input_event = true,
    .event_index = 2,
    .target_id = "answer",
    .text_byte_count = 6,
    .text_byte_count_before = 6,
    .text_byte_count_after = 0,
    .caret_before = input::text_range{.start_byte = 6, .end_byte = 6},
    .caret_after = input::text_range{.start_byte = 0, .end_byte = 0},
    .had_selection_before = false,
    .has_selection_after = false,
    .selection_before = input::text_range{},
    .selection_after = input::text_range{},
    .normalized_event = input::normalized_input_event_summary{},
    .composition = input::ime_composition_state{},
    .gesture_policy = input::gesture_policy_snapshot{},
    .pointer_capture_before = input::pointer_capture_snapshot{},
    .pointer_capture_after = input::pointer_capture_snapshot{},
    .pointer_decision = input::pointer_arbitration_decision::none,
    .pointer_event_phase = input::pointer_phase::down,
    .pointer_contact = input::pointer_contact_kind::touch_like,
    .pointer_id = 0,
    .tracked_pointer_count_before = 2,
    .tracked_pointer_count_after = 1,
};
static_assert(submit_policy_contract.kind == input::action_route_policy_kind::text_submit_boundary);
static_assert(submit_policy_contract.emits_input_event);
static_assert(submit_policy_contract.event_index == 2);
static_assert(submit_policy_contract.text_byte_count_before == 6);
static_assert(submit_policy_contract.caret_after.start_byte == 0);
static_assert(submit_policy_contract.pointer_decision == input::pointer_arbitration_decision::none);
static_assert(submit_policy_contract.pointer_contact == input::pointer_contact_kind::touch_like);
static_assert(submit_policy_contract.tracked_pointer_count_before == 2);
static_assert(submit_policy_contract.tracked_pointer_count_after == 1);

constexpr input::pointer_arbitration_decision ignored_pointer_contract =
    input::pointer_arbitration_decision::ignored_by_capture;
static_assert(ignored_pointer_contract == input::pointer_arbitration_decision::ignored_by_capture);
constexpr input::pointer_contact_kind touch_like_pointer_contract = input::pointer_contact_kind::touch_like;
static_assert(touch_like_pointer_contract == input::pointer_contact_kind::touch_like);

constexpr input::action_route_policy_kind text_commit_policy_kind =
    input::action_route_policy_kind::text_commit_boundary;
static_assert(text_commit_policy_kind == input::action_route_policy_kind::text_commit_boundary);
constexpr input::action_route_policy_kind text_delete_forward_policy_kind =
    input::action_route_policy_kind::text_delete_forward_boundary;
static_assert(text_delete_forward_policy_kind == input::action_route_policy_kind::text_delete_forward_boundary);
constexpr input::action_route_policy_kind keyboard_cancel_policy_kind =
    input::action_route_policy_kind::keyboard_cancel_intent;
static_assert(keyboard_cancel_policy_kind == input::action_route_policy_kind::keyboard_cancel_intent);
constexpr input::action_route_policy_kind pointer_arbitration_policy_kind =
    input::action_route_policy_kind::pointer_capture_arbitration;
static_assert(pointer_arbitration_policy_kind == input::action_route_policy_kind::pointer_capture_arbitration);
constexpr input::action_route_policy_kind ime_preedit_policy_kind =
    input::action_route_policy_kind::ime_preedit;
static_assert(ime_preedit_policy_kind == input::action_route_policy_kind::ime_preedit);
constexpr input::action_route_policy_kind ime_composition_start_policy_kind =
    input::action_route_policy_kind::ime_composition_start;
static_assert(ime_composition_start_policy_kind == input::action_route_policy_kind::ime_composition_start);
constexpr input::action_route_policy_kind gesture_snapshot_policy_kind =
    input::action_route_policy_kind::gesture_route_snapshot;
static_assert(gesture_snapshot_policy_kind == input::action_route_policy_kind::gesture_route_snapshot);
constexpr input::action_route_policy_kind focus_traversal_next_policy_kind =
    input::action_route_policy_kind::focus_traversal_next;
static_assert(focus_traversal_next_policy_kind == input::action_route_policy_kind::focus_traversal_next);
constexpr input::action_route_policy_kind focus_traversal_previous_policy_kind =
    input::action_route_policy_kind::focus_traversal_previous;
static_assert(focus_traversal_previous_policy_kind == input::action_route_policy_kind::focus_traversal_previous);

constexpr input::gesture_event drag_contract_event{
    .kind = input::gesture_kind::drag_update,
    .timestamp_ms = 20,
    .duration_ms = 10,
    .pointer_id = 4,
    .start_x = 1.0f,
    .start_y = 2.0f,
    .x = 5.0f,
    .y = 7.0f,
    .delta_x = 3.0f,
    .delta_y = 4.0f,
};
static_assert(drag_contract_event.kind == input::gesture_kind::drag_update);
static_assert(drag_contract_event.delta_x == 3.0f);
static_assert(drag_contract_event.delta_y == 4.0f);

constexpr input::gesture_thresholds drag_threshold_contract{
    .swipe_min_dx = 60.0f,
    .swipe_max_dy = 40.0f,
    .swipe_max_duration_ms = 800,
    .long_press_min_duration_ms = 600,
    .tap_slop = 8.0f,
    .drag_start_slop = 12.0f,
};
static_assert(drag_threshold_contract.drag_start_slop == 12.0f);

constexpr input::platform_mouse_sample platform_mouse_contract{
    .timestamp_ms = 20,
    .pointer_id = 7,
    .phase = input::platform_pointer_sample_phase::down,
    .button = input::platform_mouse_button::primary,
    .x = 3.0f,
    .y = 4.0f,
};
static_assert(platform_mouse_contract.pointer_id == 7);
static_assert(platform_mouse_contract.button == input::platform_mouse_button::primary);

constexpr input::platform_wheel_sample platform_wheel_contract{
    .timestamp_ms = 22,
    .x = 10.0f,
    .y = 12.0f,
    .delta_x = 1.0f,
    .delta_y = -2.0f,
    .unit = input::platform_scroll_delta_unit::lines,
};
static_assert(platform_wheel_contract.unit == input::platform_scroll_delta_unit::lines);
static_assert(platform_wheel_contract.delta_y == -2.0f);

constexpr input::platform_input_translation_diagnostic platform_translation_diagnostic_contract{
    .source = input::platform_input_source_kind::character,
    .status = input::platform_input_translation_status::rejected,
    .rejection_reason = input::platform_input_rejection_reason::invalid_utf8,
    .timestamp_ms = 30,
    .emitted_event = false,
};
static_assert(platform_translation_diagnostic_contract.source == input::platform_input_source_kind::character);
static_assert(platform_translation_diagnostic_contract.rejection_reason
    == input::platform_input_rejection_reason::invalid_utf8);

static_assert(std::variant_size_v<input::platform_input_sample> == 6);
static_assert(std::is_same_v<
    std::variant_alternative_t<0, input::platform_input_sample>,
    input::platform_mouse_sample>);
static_assert(std::is_same_v<
    std::variant_alternative_t<5, input::platform_input_sample>,
    input::platform_ime_composition_sample>);

constexpr input::raw_scroll_event raw_scroll_contract{
    .timestamp_ms = 10,
    .x = 20.0f,
    .y = 30.0f,
    .delta_x = 1.0f,
    .delta_y = -2.0f,
    .unit = input::scroll_delta_unit::lines,
};
static_assert(raw_scroll_contract.unit == input::scroll_delta_unit::lines);

constexpr raw_platform_scroll_event raw_platform_scroll_contract{
    .timestamp_ms = 12,
    .x = 20.0f,
    .y = 30.0f,
    .delta_x = 4.0f,
    .delta_y = -8.0f,
    .unit = raw_platform_scroll_delta_unit::pixels,
};
static_assert(raw_platform_scroll_contract.unit == raw_platform_scroll_delta_unit::pixels);

constexpr input::scroll_event scroll_contract{
    .timestamp_ms = 10,
    .x = 20.0f,
    .y = 30.0f,
    .pixel_delta_x = 4.0f,
    .pixel_delta_y = -8.0f,
    .line_delta_x = 1.0f,
    .line_delta_y = -2.0f,
};
static_assert(scroll_contract.pixel_delta_y == -8.0f);
static_assert(scroll_contract.line_delta_y == -2.0f);
static_assert(std::variant_size_v<input::input_event> == 4);
static_assert(std::variant_size_v<input::normalized_input_replay_action> == 3);
static_assert(std::is_same_v<
    std::variant_alternative_t<0, input::normalized_input_replay_action>,
    raw_platform_input_event>);
static_assert(std::is_same_v<
    std::variant_alternative_t<1, input::normalized_input_replay_action>,
    input::raw_scroll_event>);
static_assert(std::is_same_v<
    std::variant_alternative_t<2, input::normalized_input_replay_action>,
    input::normalized_input_replay_time_update>);

constexpr input::normalized_input_replay_time_update replay_time_contract{
    .timestamp_ms = 44,
};
static_assert(replay_time_contract.timestamp_ms == 44);

template <typename T>
concept InputEngineInterface = requires(
    T& engine,
    std::string target,
    const raw_platform_input_event& event,
    const input::raw_scroll_event& scroll,
    std::int64_t timestamp_ms) {
    { engine.focus_text_target(target) } -> std::same_as<void>;
    { engine.clear_text_focus() } -> std::same_as<void>;
    { engine.has_text_focus() } -> std::same_as<bool>;
    { engine.text_focus_id() } -> std::same_as<const std::string&>;
    { engine.text_model() } -> std::same_as<const input::text_input_model&>;
    { engine.text_presentation_snapshot() } -> std::same_as<input::text_input_presentation_snapshot>;
    { engine.routing_diagnostics() } -> std::same_as<const input::input_routing_diagnostics&>;
    { engine.process_raw_event(event) } -> std::same_as<std::vector<input::input_event>>;
    { engine.process_scroll_event(scroll) } -> std::same_as<std::vector<input::input_event>>;
    { engine.update_time(timestamp_ms) } -> std::same_as<std::vector<input::input_event>>;
    { engine.reset() } -> std::same_as<void>;
};

static_assert(std::is_constructible_v<input::input_engine>);
static_assert(InputEngineInterface<input::input_engine>);

static_assert(std::variant_size_v<raw_platform_input_event> == 6);
static_assert(std::is_same_v<
    std::variant_alternative_t<0, raw_platform_input_event>,
    raw_platform_pointer_event>);
static_assert(std::is_same_v<
    std::variant_alternative_t<1, raw_platform_input_event>,
    raw_platform_text_event>);
static_assert(std::is_same_v<
    std::variant_alternative_t<2, raw_platform_input_event>,
    raw_platform_ime_event>);
static_assert(std::is_same_v<
    std::variant_alternative_t<3, raw_platform_input_event>,
    raw_platform_key_event>);
static_assert(std::is_same_v<
    std::variant_alternative_t<4, raw_platform_input_event>,
    raw_platform_focus_event>);
static_assert(std::is_same_v<
    std::variant_alternative_t<5, raw_platform_input_event>,
    raw_platform_scroll_event>);

} // namespace
