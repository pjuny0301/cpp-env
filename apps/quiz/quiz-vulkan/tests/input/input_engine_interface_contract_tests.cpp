#include "core/input/gesture_recognizer.h"
#include "core/input/input_engine.h"
#include "core/input/normalized_input_replay.h"
#include "core/input/platform_input_engine_adapter.h"
#include "core/input/platform_input_translator.h"
#include "core/input/text_input_model.h"
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
};

template <typename T>
concept NormalizedInputReplayBatchInterface = requires(T batch) {
    { batch.label } -> std::same_as<std::string&>;
    { batch.input_events } -> std::same_as<std::vector<input::input_event>&>;
    { batch.normalized_events } -> std::same_as<std::vector<input::normalized_input_event_summary>&>;
    { batch.summary } -> std::same_as<input::input_diagnostic_summary&>;
    { batch.keyboard } -> std::same_as<input::normalized_input_replay_keyboard_summary&>;
    { batch.ime } -> std::same_as<input::normalized_input_replay_ime_summary&>;
    { batch.end_state } -> std::same_as<input::normalized_input_replay_end_state&>;
};

template <typename T>
concept NormalizedInputReplayRecordingInterface = requires(T recording) {
    { recording.batches } -> std::same_as<std::vector<input::normalized_input_replay_batch>&>;
    { recording.summary } -> std::same_as<input::input_diagnostic_summary&>;
    { recording.keyboard } -> std::same_as<input::normalized_input_replay_keyboard_summary&>;
    { recording.ime } -> std::same_as<input::normalized_input_replay_ime_summary&>;
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
concept NormalizedInputReplayFunctions = requires(
    input::input_engine& engine,
    const input::normalized_input_replay_action& action,
    std::span<const input::normalized_input_replay_step> steps,
    std::span<const input::action_route_policy_diagnostic> routes,
    std::span<const input::input_event> events,
    const input::keyboard_chord_diagnostic& keyboard,
    const input::ime_composition_state& composition,
    const input::normalized_input_replay_end_state& end_state,
    input::normalized_input_replay_keyboard_summary& keyboard_target,
    const input::normalized_input_replay_keyboard_summary& keyboard_source,
    input::normalized_input_replay_ime_summary& ime_target,
    const input::normalized_input_replay_ime_summary& ime_source,
    const input::normalized_input_replay_options& options) {
    { input::pointer_capture_snapshot_clean(input::pointer_capture_snapshot{}) } -> std::same_as<bool>;
    { input::keyboard_chord_present(keyboard) } -> std::same_as<bool>;
    { input::normalized_input_replay_preedit_text_valid(composition) } -> std::same_as<bool>;
    { input::normalized_input_replay_composition_range_valid(composition) } -> std::same_as<bool>;
    { input::summarize_normalized_input_replay_ime_routes(routes, events, end_state) }
        -> std::same_as<input::normalized_input_replay_ime_summary>;
    { input::accumulate_normalized_input_replay_ime_summary(ime_target, ime_source) }
        -> std::same_as<void>;
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
static_assert(NormalizedInputReplayImePhaseCountsInterface<input::normalized_input_replay_ime_phase_counts>);
static_assert(NormalizedInputReplayImeTimelineEntryInterface<input::normalized_input_replay_ime_timeline_entry>);
static_assert(NormalizedInputReplayImeSummaryInterface<input::normalized_input_replay_ime_summary>);
static_assert(NormalizedInputReplayFunctions<void>);
static_assert(std::is_default_constructible_v<input::platform_input_translation_request>);
static_assert(std::is_default_constructible_v<input::platform_input_translation_result>);
static_assert(std::is_default_constructible_v<input::platform_input_dispatch_result>);
static_assert(std::is_default_constructible_v<input::platform_input_dispatch_batch_result>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_time_update>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_options>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_end_state>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_batch>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_recording>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_keyboard_intent_counts>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_keyboard_modifier_counts>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_keyboard_repeat_policy_counts>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_keyboard_summary>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_ime_phase_counts>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_ime_timeline_entry>);
static_assert(std::is_default_constructible_v<input::normalized_input_replay_ime_summary>);
static_assert(std::is_default_constructible_v<input::keyboard_modifier_state>);
static_assert(std::is_default_constructible_v<input::keyboard_chord_diagnostic>);
static_assert(!std::is_polymorphic_v<input::platform_input_translation_request>);
static_assert(!std::is_polymorphic_v<input::platform_input_translation_result>);
static_assert(!std::is_polymorphic_v<input::platform_input_dispatch_result>);
static_assert(!std::is_polymorphic_v<input::platform_input_dispatch_batch_result>);
static_assert(!std::is_polymorphic_v<input::normalized_input_replay_step>);
static_assert(!std::is_polymorphic_v<input::normalized_input_replay_batch>);
static_assert(!std::is_polymorphic_v<input::normalized_input_replay_recording>);
static_assert(!std::is_polymorphic_v<input::normalized_input_replay_keyboard_summary>);
static_assert(!std::is_polymorphic_v<input::normalized_input_replay_ime_summary>);
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
static_assert(TextInputModelInterface<input::text_input_model>);

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
