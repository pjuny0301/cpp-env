#include "render/vulkan/vulkan_renderer.h"
#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_frame_plan.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

class fake_vulkan_backend_device final : public quiz_vulkan::render::vulkan_backend::vulkan_backend_device_interface {
public:
    explicit fake_vulkan_backend_device(quiz_vulkan::render::vulkan_backend::vulkan_surface_extent surface)
        : surface_(surface)
    {
    }

    quiz_vulkan::render::vulkan_backend::vulkan_backend_lifecycle_readiness current_lifecycle_readiness() const override
    {
        return lifecycle;
    }

    quiz_vulkan::render::vulkan_backend::vulkan_surface_extent current_surface_extent() const override
    {
        return surface_;
    }

    bool begin_frame(quiz_vulkan::render::vulkan_backend::vulkan_surface_extent surface) override
    {
        calls.push_back("begin");
        begun_surface = surface;
        return begin_succeeds;
    }

    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_result acquire_next_image(
        quiz_vulkan::render::vulkan_backend::vulkan_surface_extent surface) override
    {
        calls.push_back("acquire");
        acquired_surface = surface;
        return quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_result{
            .status = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status::acquired,
            .image = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_state{
                .id = next_image_id,
                .available = true,
                .acquired = true,
                .presented = false,
            },
        };
    }

    bool record_frame_commands(const quiz_vulkan::render::vulkan_backend::vulkan_frame_plan& plan) override
    {
        calls.push_back("record");
        recorded_plan = plan;
        return record_succeeds;
    }

    bool submit_frame() override
    {
        calls.push_back("submit");
        return submit_succeeds;
    }

    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_result present_image(
        quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_id image_id) override
    {
        calls.push_back("present_image");
        presented_image_id = image_id;
        return quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_result{
            .status = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_status::presented,
            .image_id = image_id,
        };
    }

    bool present_frame() override
    {
        calls.push_back("present");
        return present_succeeds;
    }

    bool begin_succeeds = true;
    bool record_succeeds = true;
    bool submit_succeeds = true;
    bool present_succeeds = true;
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_id next_image_id{.value = 7};
    quiz_vulkan::render::vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle{
        .instance_ready = true,
        .device_ready = true,
        .swapchain_ready = true,
        .pipeline_ready = true,
        .command_recorder_ready = true,
    };
    quiz_vulkan::render::vulkan_backend::vulkan_surface_extent begun_surface;
    quiz_vulkan::render::vulkan_backend::vulkan_surface_extent acquired_surface;
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_id presented_image_id;
    quiz_vulkan::render::vulkan_backend::vulkan_frame_plan recorded_plan;
    std::vector<std::string> calls;

private:
    quiz_vulkan::render::vulkan_backend::vulkan_surface_extent surface_;
};

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

quiz_vulkan::render::render_draw_command make_quad_command(
    std::string node_id,
    quiz_vulkan::render::render_rect bounds,
    quiz_vulkan::render::render_color color)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::quad,
        .node_id = std::move(node_id),
        .parent_node_id = {},
        .depth = 0,
        .bounds = bounds,
        .content_bounds = bounds,
        .paint = render_paint{.source = "test", .color = color},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
}

quiz_vulkan::render::render_draw_command make_image_command(
    std::string node_id,
    quiz_vulkan::render::render_rect bounds,
    std::string uri)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::image,
        .node_id = std::move(node_id),
        .parent_node_id = {},
        .depth = 0,
        .bounds = bounds,
        .content_bounds = bounds,
        .paint = render_paint{.source = "image", .color = render_color{1.0f, 1.0f, 1.0f, 1.0f}},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = render_image_ref{.uri = std::move(uri), .alt_text = "fixture", .aspect_ratio = 1.0f},
    };
}

void require_frame_fallback_summary(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_fallback_summary& summary,
    quiz_vulkan::render::vulkan_backend::vulkan_backend_fallback_reason reason,
    quiz_vulkan::render::vulkan_backend::vulkan_frame_lifecycle_failure_classification classification,
    quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_stage reached_stage)
{
    using namespace quiz_vulkan::render;

    require(summary.checked, "frame fallback summary is checked");
    require(summary.required, "frame fallback summary marks fallback required");
    require(summary.reason == reason, "frame fallback summary records fallback reason");
    require(summary.reached_stage == reached_stage, "frame fallback summary records reached stage");
    require(
        summary.recoverable
            == (classification == vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable),
        "frame fallback summary records recoverable classification");
    require(
        summary.fatal == (classification == vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal),
        "frame fallback summary records fatal classification");
    require(summary.reason_count == 1, "frame fallback summary records one fallback reason");
    require(!summary.completed(), "frame fallback summary does not complete when fallback is required");
}

void require_completed_swapchain_policy(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_swapchain_policy_state& state,
    std::size_t width,
    std::size_t height)
{
    using namespace quiz_vulkan::render;

    require(state.checked, "swapchain policy is checked");
    require(state.completed(), "swapchain policy completes");
    require(state.extent.checked, "swapchain extent policy is checked");
    require(state.extent.completed(), "swapchain extent policy completes");
    require(state.extent.requested_extent.width == width, "swapchain extent policy records requested width");
    require(state.extent.requested_extent.height == height, "swapchain extent policy records requested height");
    require(state.extent.selected_extent.width == width, "swapchain extent policy records selected width");
    require(state.extent.selected_extent.height == height, "swapchain extent policy records selected height");
    require(state.extent.min_extent.width == 1, "swapchain extent policy records minimum width");
    require(state.extent.min_extent.height == 1, "swapchain extent policy records minimum height");
    require(state.extent.max_extent.width == 4096, "swapchain extent policy records maximum width");
    require(state.extent.max_extent.height == 4096, "swapchain extent policy records maximum height");
    require(state.extent.extent_supported, "swapchain extent policy marks extent supported");
    require(!state.extent.extent_clamped, "swapchain extent policy does not clamp supported fixture extent");
    require(state.present_mode.checked, "swapchain present mode policy is checked");
    require(state.present_mode.completed(), "swapchain present mode policy completes");
    require(
        state.present_mode.requested_mode == vulkan_backend::vulkan_swapchain_present_mode::fifo,
        "swapchain present mode policy requests fifo");
    require(
        state.present_mode.selected_mode == vulkan_backend::vulkan_swapchain_present_mode::fifo,
        "swapchain present mode policy selects fifo");
    require(state.present_mode.requested_mode_supported, "swapchain present mode policy supports fifo");
    require(!state.present_mode.fallback_to_fifo, "swapchain present mode policy does not fallback from fifo");
}

const quiz_vulkan::render::vulkan_backend::vulkan_frame_lifecycle_step_snapshot* find_lifecycle_snapshot(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_lifecycle_policy_state& state,
    quiz_vulkan::render::vulkan_backend::vulkan_frame_lifecycle_step step)
{
    for (const quiz_vulkan::render::vulkan_backend::vulkan_frame_lifecycle_step_snapshot& snapshot :
         state.snapshots) {
        if (snapshot.step == step) {
            return &snapshot;
        }
    }
    return nullptr;
}

void require_stable_frame_lifecycle_snapshot_order(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_lifecycle_policy_state& state)
{
    using namespace quiz_vulkan::render;

    require(state.snapshots.size() == 5, "frame lifecycle policy stores five stable snapshots");
    require(
        state.snapshots[0].step == vulkan_backend::vulkan_frame_lifecycle_step::acquire,
        "frame lifecycle first snapshot is acquire");
    require(state.snapshots[0].expected_order == 0, "frame lifecycle acquire expected order is stable");
    require(
        state.snapshots[1].step == vulkan_backend::vulkan_frame_lifecycle_step::begin,
        "frame lifecycle second snapshot is begin");
    require(state.snapshots[1].expected_order == 1, "frame lifecycle begin expected order is stable");
    require(
        state.snapshots[2].step == vulkan_backend::vulkan_frame_lifecycle_step::render,
        "frame lifecycle third snapshot is render");
    require(state.snapshots[2].expected_order == 2, "frame lifecycle render expected order is stable");
    require(
        state.snapshots[3].step == vulkan_backend::vulkan_frame_lifecycle_step::submit,
        "frame lifecycle fourth snapshot is submit");
    require(state.snapshots[3].expected_order == 3, "frame lifecycle submit expected order is stable");
    require(
        state.snapshots[4].step == vulkan_backend::vulkan_frame_lifecycle_step::present,
        "frame lifecycle fifth snapshot is present");
    require(state.snapshots[4].expected_order == 4, "frame lifecycle present expected order is stable");
}

void require_failed_frame_lifecycle_policy(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_lifecycle_policy_state& state,
    quiz_vulkan::render::vulkan_backend::vulkan_frame_lifecycle_step failed_step,
    quiz_vulkan::render::vulkan_backend::vulkan_frame_lifecycle_failure_classification classification,
    quiz_vulkan::render::vulkan_backend::vulkan_backend_fallback_reason reason,
    std::size_t attempted_step_count,
    std::size_t completed_step_count)
{
    using namespace quiz_vulkan::render;

    require(state.checked, "failed frame lifecycle policy is checked");
    require(!state.completed(), "failed frame lifecycle policy does not complete");
    require(state.ordering_valid, "failed frame lifecycle policy keeps ordering valid");
    require(
        state.recoverable_failure
            == (classification == vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable),
        "failed frame lifecycle policy records recoverable classification");
    require(
        state.fatal_failure
            == (classification == vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal),
        "failed frame lifecycle policy records fatal classification");
    require(state.failed_step == failed_step, "failed frame lifecycle policy records failed step");
    require(state.fallback_reason == reason, "failed frame lifecycle policy records fallback reason");
    require(state.attempted_step_count == attempted_step_count, "failed frame lifecycle attempted count is stable");
    require(state.completed_step_count == completed_step_count, "failed frame lifecycle completed count is stable");
    require_stable_frame_lifecycle_snapshot_order(state);

    const vulkan_backend::vulkan_frame_lifecycle_step_snapshot* failed_snapshot =
        find_lifecycle_snapshot(state, failed_step);
    require(failed_snapshot != nullptr, "failed frame lifecycle snapshot exists");
    require(failed_snapshot->attempted, "failed frame lifecycle snapshot is attempted");
    require(!failed_snapshot->completed, "failed frame lifecycle snapshot is not completed");
    require(
        failed_snapshot->status == vulkan_backend::vulkan_frame_lifecycle_order_status::failed,
        "failed frame lifecycle snapshot reports failed status");
    require(failed_snapshot->failure_classification == classification, "failed snapshot records classification");
    require(failed_snapshot->fallback_reason == reason, "failed snapshot records fallback reason");
    require(failed_snapshot->failed(), "failed snapshot exposes failure helper");
    require(
        failed_snapshot->observed_order == failed_snapshot->expected_order,
        "failed snapshot keeps deterministic observed order");

    for (const vulkan_backend::vulkan_frame_lifecycle_step_snapshot& snapshot : state.snapshots) {
        if (snapshot.expected_order < failed_snapshot->expected_order) {
            require(snapshot.attempted, "pre-failure lifecycle snapshot is attempted");
            require(snapshot.completed, "pre-failure lifecycle snapshot is completed");
            require(
                snapshot.status == vulkan_backend::vulkan_frame_lifecycle_order_status::completed,
                "pre-failure lifecycle snapshot completed before failure");
        }
        if (snapshot.expected_order > failed_snapshot->expected_order) {
            require(!snapshot.attempted, "post-failure lifecycle snapshot is not attempted");
            require(!snapshot.completed, "post-failure lifecycle snapshot is not completed");
            require(
                snapshot.status == vulkan_backend::vulkan_frame_lifecycle_order_status::skipped,
                "post-failure lifecycle snapshot is skipped");
        }
    }
}

void require_descriptor_blocked_command_recorder_gate(
    const quiz_vulkan::render::vulkan_backend::vulkan_command_recorder_gate_state& gate,
    quiz_vulkan::render::vulkan_backend::vulkan_batch_kind batch_kind,
    std::size_t command_index,
    quiz_vulkan::render::vulkan_backend::vulkan_resource_binding_kind binding_kind,
    std::size_t binding,
    std::string_view resource_id,
    std::size_t planned_batch_count,
    std::size_t descriptor_set_count,
    std::size_t missing_resource_count)
{
    using namespace quiz_vulkan::render;

    require(gate.checked, "blocked command recorder gate is checked");
    require(!gate.completed(), "blocked command recorder gate does not complete");
    require(gate.blocked(), "command recorder gate reports a block");
    require(!gate.recording_allowed, "blocked command recorder gate disallows recording");
    require(
        gate.status == vulkan_backend::vulkan_command_recorder_gate_status::blocked_by_descriptor_validation,
        "command recorder gate blocks on descriptor validation");
    require(
        gate.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::resource_binding_unavailable,
        "blocked command recorder gate records resource fallback reason");
    require(gate.resource_bindings_checked, "blocked gate checks resource bindings");
    require(!gate.resource_bindings_completed, "blocked gate records incomplete resource bindings");
    require(gate.descriptor_validation_checked, "blocked gate checks descriptor validation");
    require(!gate.descriptor_validation_completed, "blocked gate records incomplete descriptor validation");
    require(gate.resource_registry_checked, "blocked gate checks resource registry");
    require(!gate.resource_registry_completed, "blocked gate records incomplete resource registry");
    require(gate.missing_required_resource, "blocked gate records missing required resource");
    require(!gate.duplicate_binding, "blocked gate has no duplicate binding");
    require(!gate.invalid_layout, "blocked gate has no invalid descriptor layout");
    require(
        gate.descriptor_status
            == vulkan_backend::vulkan_descriptor_validation_status::missing_required_resource,
        "blocked gate descriptor status records missing resource");
    require(gate.blocked_batch_kind == batch_kind, "blocked gate records failed batch kind");
    require(gate.blocked_command_index == command_index, "blocked gate records failed command index");
    require(gate.blocked_binding_kind == binding_kind, "blocked gate records failed binding kind");
    require(gate.blocked_binding == binding, "blocked gate records failed binding slot");
    require(gate.blocked_resource_id == resource_id, "blocked gate records stable missing resource id");
    require(gate.planned_batch_count == planned_batch_count, "blocked gate tracks planned batch count");
    require(gate.descriptor_set_count == descriptor_set_count, "blocked gate tracks descriptor set count");
    require(
        gate.invalid_descriptor_set_count == descriptor_set_count,
        "blocked gate records invalid descriptor set count");
    require(gate.missing_resource_count == missing_resource_count, "blocked gate tracks missing registry count");
}

void require_failed_command_buffer_recording_state(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_command_buffer_submit_state& state,
    quiz_vulkan::render::vulkan_backend::vulkan_command_recorder_failure_stage failure_stage,
    std::size_t failure_recording_index,
    std::size_t planned_batch_count,
    std::size_t recorded_batch_count,
    bool finish_requested)
{
    using namespace quiz_vulkan::render;

    require(state.checked, "failed command buffer diagnostics are checked");
    require(!state.completed(), "failed command buffer diagnostics do not complete");
    require(state.recording.command_buffer.valid(), "failed command buffer id is valid");
    require(state.recording.begin_requested, "failed command buffer recording was begun");
    require(state.recording.finish_requested == finish_requested, "failed command buffer finish flag is stable");
    require(
        state.recording.status == vulkan_backend::vulkan_command_buffer_recording_status::failed,
        "failed command buffer recording status is failed");
    require(state.recording.failed(), "failed command buffer recording helper reports failure");
    require(state.recording.failure_stage == failure_stage, "failed command buffer records failure stage");
    require(
        state.recording.failure_recording_index == failure_recording_index,
        "failed command buffer records failure index");
    require(state.recording.planned_batch_count == planned_batch_count, "failed command buffer planned count is stable");
    require(state.recording.recorded_batch_count == recorded_batch_count, "failed command buffer recorded count is stable");
    require(!state.submit.submit_requested, "failed command buffer is not submitted");
    require(
        state.submit.status == vulkan_backend::vulkan_frame_submit_status::not_requested,
        "failed command buffer submit status is not requested");
}

void require_unsubmitted_recorded_command_buffer_state(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_command_buffer_submit_state& state,
    std::size_t planned_batch_count)
{
    using namespace quiz_vulkan::render;

    require(state.checked, "unsubmitted command buffer diagnostics are checked");
    require(!state.completed(), "unsubmitted command buffer diagnostics do not complete");
    require(state.recording.completed(), "unsubmitted command buffer recording completed");
    require(state.recording.planned_batch_count == planned_batch_count, "unsubmitted command buffer planned count is stable");
    require(state.recording.recorded_batch_count == planned_batch_count, "unsubmitted command buffer recorded count is stable");
    require(!state.submit.submit_requested, "unsubmitted command buffer has no submit request");
    require(
        state.submit.status == vulkan_backend::vulkan_frame_submit_status::not_requested,
        "unsubmitted command buffer submit status is not requested");
}

void test_vulkan_command_recorder_failure_stage_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::command_recorder_failure_stage_name(
            vulkan_backend::vulkan_command_recorder_failure_stage::none)
            == std::string_view{"none"},
        "command recorder failure stage name for none is stable");
    require(
        vulkan_backend::command_recorder_failure_stage_name(
            vulkan_backend::vulkan_command_recorder_failure_stage::begin_recording)
            == std::string_view{"begin_recording"},
        "command recorder failure stage name for begin is stable");
    require(
        vulkan_backend::command_recorder_failure_stage_name(
            vulkan_backend::vulkan_command_recorder_failure_stage::record_draw_batch)
            == std::string_view{"record_draw_batch"},
        "command recorder failure stage name for record is stable");
    require(
        vulkan_backend::command_recorder_failure_stage_name(
            vulkan_backend::vulkan_command_recorder_failure_stage::finish_recording)
            == std::string_view{"finish_recording"},
        "command recorder failure stage name for finish is stable");
}

void test_vulkan_command_recorder_gate_status_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::command_recorder_gate_status_name(
            vulkan_backend::vulkan_command_recorder_gate_status::not_checked)
            == std::string_view{"not_checked"},
        "command recorder gate status name for not checked is stable");
    require(
        vulkan_backend::command_recorder_gate_status_name(
            vulkan_backend::vulkan_command_recorder_gate_status::allowed)
            == std::string_view{"allowed"},
        "command recorder gate status name for allowed is stable");
    require(
        vulkan_backend::command_recorder_gate_status_name(
            vulkan_backend::vulkan_command_recorder_gate_status::blocked_by_descriptor_validation)
            == std::string_view{"blocked_by_descriptor_validation"},
        "command recorder gate status name for descriptor validation block is stable");
    require(
        vulkan_backend::command_recorder_gate_status_name(
            vulkan_backend::vulkan_command_recorder_gate_status::blocked_by_resource_binding)
            == std::string_view{"blocked_by_resource_binding"},
        "command recorder gate status name for resource binding block is stable");
    require(
        vulkan_backend::command_recorder_gate_status_name(
            vulkan_backend::vulkan_command_recorder_gate_status::blocked_by_resource_registry)
            == std::string_view{"blocked_by_resource_registry"},
        "command recorder gate status name for resource registry block is stable");
}

void test_vulkan_command_buffer_recording_status_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::command_buffer_recording_status_name(
            vulkan_backend::vulkan_command_buffer_recording_status::not_started)
            == std::string_view{"not_started"},
        "command buffer recording status name for not started is stable");
    require(
        vulkan_backend::command_buffer_recording_status_name(
            vulkan_backend::vulkan_command_buffer_recording_status::recording)
            == std::string_view{"recording"},
        "command buffer recording status name for recording is stable");
    require(
        vulkan_backend::command_buffer_recording_status_name(
            vulkan_backend::vulkan_command_buffer_recording_status::recorded)
            == std::string_view{"recorded"},
        "command buffer recording status name for recorded is stable");
    require(
        vulkan_backend::command_buffer_recording_status_name(
            vulkan_backend::vulkan_command_buffer_recording_status::failed)
            == std::string_view{"failed"},
        "command buffer recording status name for failed is stable");
}

void test_vulkan_diagnostic_command_recorder_records_planned_batches()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{10.0f, 20.0f, 30.0f, 40.0f},
        render_color{0.8f, 0.7f, 0.6f, 1.0f}));
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{50.0f, 60.0f, 20.0f, 20.0f},
        "fixture://renderer/image.png"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_command_recorder recorder;
    require(
        recorder.begin_recording(
            vulkan_backend::vulkan_surface_extent{.width = 10, .height = 10},
            plan.batches.size()),
        "diagnostic recorder begins command buffer recording");
    for (const vulkan_backend::vulkan_draw_batch& batch : plan.batches) {
        require(recorder.record_draw_batch(batch), "diagnostic recorder records each planned batch");
    }
    require(recorder.finish_recording(), "diagnostic recorder finishes command buffer recording");

    const vulkan_backend::vulkan_backend_command_recorder_state& state = recorder.recorder_state();
    require(state.completed(), "diagnostic recorder reports completed command buffer state");
    require(!state.failed(), "diagnostic recorder reports no failure after successful recording");
    require(
        state.failure_stage == vulkan_backend::vulkan_command_recorder_failure_stage::none,
        "diagnostic recorder keeps none failure stage after successful recording");
    require(state.planned_batch_count == 2, "diagnostic recorder stores planned batch count");
    require(state.recorded_batch_count == 2, "diagnostic recorder stores recorded batch count");
    require(state.recorded_batches.size() == 2, "diagnostic recorder stores recorded batches");

    const vulkan_backend::vulkan_recorded_draw_batch& first = state.recorded_batches[0];
    require(first.kind == vulkan_backend::vulkan_batch_kind::quad, "diagnostic recorder stores first batch kind");
    require(first.command_index == 0, "diagnostic recorder stores first source command index");
    require(first.recording_index == 0, "diagnostic recorder stores first recording order");
    require(first.clipped_bounds.x == 10.0f, "diagnostic recorder stores first clipped x");
    require(first.scissor.x == 1, "diagnostic recorder stores first scissor x");
    require(first.scissor.y == 2, "diagnostic recorder stores first scissor y");
    require(first.scissor.width == 3, "diagnostic recorder stores first scissor width");
    require(first.scissor.height == 4, "diagnostic recorder stores first scissor height");

    const vulkan_backend::vulkan_recorded_draw_batch& second = state.recorded_batches[1];
    require(second.kind == vulkan_backend::vulkan_batch_kind::image, "diagnostic recorder stores second batch kind");
    require(second.command_index == 1, "diagnostic recorder stores second source command index");
    require(second.recording_index == 1, "diagnostic recorder stores second recording order");
    require(second.scissor.x == 5, "diagnostic recorder stores second scissor x");
    require(second.scissor.y == 6, "diagnostic recorder stores second scissor y");
    require(second.scissor.width == 2, "diagnostic recorder stores second scissor width");
    require(second.scissor.height == 2, "diagnostic recorder stores second scissor height");
}

void test_vulkan_diagnostic_command_recorder_reports_begin_failure()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::diagnostic_vulkan_command_recorder recorder(
        vulkan_backend::diagnostic_vulkan_command_recorder_options{
            .ready = true,
            .fail_at = vulkan_backend::vulkan_command_recorder_failure_stage::begin_recording,
        });

    require(
        !recorder.begin_recording(
            vulkan_backend::vulkan_surface_extent{.width = 10, .height = 10},
            1),
        "diagnostic recorder can deterministically fail begin");

    const vulkan_backend::vulkan_backend_command_recorder_state& state = recorder.recorder_state();
    require(state.ready, "begin failure recorder remains logically ready");
    require(!state.frame_open, "begin failure does not open recorder frame");
    require(!state.command_buffer_recorded, "begin failure does not record command buffer");
    require(state.failed(), "begin failure marks recorder state failed");
    require(
        state.failure_stage == vulkan_backend::vulkan_command_recorder_failure_stage::begin_recording,
        "begin failure records begin failure stage");
    require(state.failure_recording_index == 0, "begin failure records deterministic failure index");
    require(state.planned_batch_count == 1, "begin failure preserves planned batch count");
    require(state.recorded_batch_count == 0, "begin failure records no batches");
    require(state.recorded_batches.empty(), "begin failure stores no recorded batches");
}

void test_vulkan_diagnostic_command_recorder_reports_record_failure()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "first",
        render_rect{0.0f, 0.0f, 40.0f, 40.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_quad_command(
        "second",
        render_rect{50.0f, 50.0f, 20.0f, 20.0f},
        render_color{0.0f, 1.0f, 0.0f, 1.0f}));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_command_recorder recorder(
        vulkan_backend::diagnostic_vulkan_command_recorder_options{
            .ready = true,
            .fail_at = vulkan_backend::vulkan_command_recorder_failure_stage::record_draw_batch,
            .fail_recording_index = 1,
        });

    require(
        recorder.begin_recording(
            vulkan_backend::vulkan_surface_extent{.width = 10, .height = 10},
            plan.batches.size()),
        "record failure recorder begins successfully");
    require(recorder.record_draw_batch(plan.batches[0]), "record failure recorder records first batch");
    require(!recorder.record_draw_batch(plan.batches[1]), "record failure recorder fails second batch");
    require(!recorder.finish_recording(), "record failure recorder does not finish after failed batch");

    const vulkan_backend::vulkan_backend_command_recorder_state& state = recorder.recorder_state();
    require(state.ready, "record failure recorder remains ready");
    require(state.frame_open, "record failure keeps frame open");
    require(!state.command_buffer_recorded, "record failure does not finish command buffer");
    require(state.failed(), "record failure marks recorder state failed");
    require(
        state.failure_stage == vulkan_backend::vulkan_command_recorder_failure_stage::record_draw_batch,
        "record failure records record failure stage");
    require(state.failure_recording_index == 1, "record failure records failed batch index");
    require(state.planned_batch_count == 2, "record failure preserves planned batch count");
    require(state.recorded_batch_count == 1, "record failure preserves successfully recorded count");
    require(state.recorded_batches.size() == 1, "record failure stores only successful batches");
    require(state.recorded_batches.front().recording_index == 0, "record failure preserves first recording index");
}

void test_vulkan_diagnostic_command_recorder_reports_finish_failure()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_command_recorder recorder(
        vulkan_backend::diagnostic_vulkan_command_recorder_options{
            .ready = true,
            .fail_at = vulkan_backend::vulkan_command_recorder_failure_stage::finish_recording,
        });

    require(
        recorder.begin_recording(
            vulkan_backend::vulkan_surface_extent{.width = 10, .height = 10},
            plan.batches.size()),
        "finish failure recorder begins successfully");
    require(recorder.record_draw_batch(plan.batches.front()), "finish failure recorder records planned batch");
    require(!recorder.finish_recording(), "finish failure recorder fails finish");

    const vulkan_backend::vulkan_backend_command_recorder_state& state = recorder.recorder_state();
    require(state.ready, "finish failure recorder remains ready");
    require(state.frame_open, "finish failure keeps frame open");
    require(!state.command_buffer_recorded, "finish failure does not record command buffer");
    require(state.failed(), "finish failure marks recorder state failed");
    require(
        state.failure_stage == vulkan_backend::vulkan_command_recorder_failure_stage::finish_recording,
        "finish failure records finish failure stage");
    require(state.failure_recording_index == 1, "finish failure records failure after one recorded batch");
    require(state.planned_batch_count == 1, "finish failure preserves planned batch count");
    require(state.recorded_batch_count == 1, "finish failure preserves recorded batch count");
    require(state.recorded_batches.size() == 1, "finish failure preserves recorded batches");
}

void test_vulkan_backend_adapter_falls_back_when_command_recorder_is_unready()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    device.lifecycle.command_recorder_ready = false;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete without a command recorder");
    require(result.attempted, "backend records attempted frame with unavailable command recorder");
    require(result.fallback_required, "backend requires fallback without command recorder readiness");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::command_recorder_unavailable,
        "backend reports command recorder readiness fallback reason");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::backend_attempted,
        "backend stops at attempted stage without command recorder readiness");
    require_frame_fallback_summary(
        result.fallback_summary,
        vulkan_backend::vulkan_backend_fallback_reason::command_recorder_unavailable,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal,
        vulkan_backend::vulkan_backend_frame_stage::backend_attempted);
    require(!result.swapchain_policy.checked, "backend does not check swapchain policy before lifecycle readiness");
    require(result.lifecycle.instance_ready, "backend records ready instance");
    require(result.lifecycle.device_ready, "backend records ready device");
    require(result.lifecycle.swapchain_ready, "backend records ready swapchain");
    require(result.lifecycle.pipeline_ready, "backend records ready pipeline");
    require(!result.lifecycle.command_recorder_ready, "backend records unavailable command recorder");
    require(!result.lifecycle.ready_for_frame(), "backend lifecycle is not frame-ready without command recorder");
    require(!result.command_recorder.ready, "backend command recorder state is not ready");
    require(!result.command_recorder.frame_open, "backend command recorder does not open frame when unready");
    require(!result.command_recorder.command_buffer_recorded, "backend command recorder does not record when unready");
    require(!result.command_recorder.failed(), "backend command recorder state does not fail before lifecycle readiness");
    require(result.command_recorder.empty(), "backend command recorder state remains empty when unready");
    require(!result.lifecycle_ready, "backend aggregate lifecycle readiness fails");
    require(!result.surface.valid(), "backend does not query a surface before lifecycle readiness passes");
    require(!result.surface_ready, "backend surface is not ready when lifecycle readiness fails");
    require(!result.frame_begun, "backend does not begin frame without command recorder readiness");
    require(!result.commands_recorded, "backend does not record without command recorder readiness");
    require(!result.frame_submitted, "backend does not submit without command recorder readiness");
    require(!result.frame_presented, "backend does not present without command recorder readiness");
    require(result.planned_batch_count == 0, "backend does not plan batches without command recorder readiness");
    require(result.recorded_batch_count == 0, "backend does not record batches without command recorder readiness");
    require(device.calls.empty(), "backend does not call frame lifecycle without command recorder readiness");
}

void test_vulkan_backend_adapter_falls_back_when_injected_recorder_rejects_frame()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    vulkan_backend::diagnostic_vulkan_command_recorder recorder(false);
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        recorder,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when injected recorder rejects frame");
    require(result.lifecycle_ready, "backend lifecycle is ready before injected recorder rejects frame");
    require(result.surface_ready, "backend surface is ready before injected recorder rejects frame");
    require(result.frame_begun, "backend begins frame before injected recorder rejects frame");
    require(!result.commands_recorded, "backend does not mark commands recorded after recorder rejection");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "backend reports record fallback when injected recorder rejects frame");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_begun,
        "backend stops at frame begun stage when injected recorder rejects frame");
    require_failed_frame_lifecycle_policy(
        result.lifecycle_policy,
        vulkan_backend::vulkan_frame_lifecycle_step::render,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal,
        vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        3,
        2);
    require(!result.command_recorder.ready, "backend records rejected command recorder readiness");
    require(!result.command_recorder.frame_open, "backend does not open rejected command recorder frame");
    require(!result.command_recorder.command_buffer_recorded, "backend does not record rejected command buffer");
    require(result.command_recorder.failed(), "backend records rejected recorder failure");
    require(
        result.command_recorder.failure_stage == vulkan_backend::vulkan_command_recorder_failure_stage::begin_recording,
        "backend records begin failure stage from rejected recorder");
    require(result.command_recorder.failure_recording_index == 0, "backend records begin failure index");
    require(result.command_recorder.planned_batch_count == 1, "backend preserves planned count from rejected recorder");
    require(result.command_recorder.recorded_batches.empty(), "backend has no recorded batches after recorder rejection");
    require_failed_command_buffer_recording_state(
        result.command_buffer_submit,
        vulkan_backend::vulkan_command_recorder_failure_stage::begin_recording,
        0,
        1,
        0,
        false);
    require(result.swapchain.acquired(), "backend acquires image before injected recorder rejects frame");
    require(!result.swapchain.present_requested, "backend does not present image after recorder rejection");
    require(device.calls.size() == 2, "backend stops device lifecycle after acquire when recorder rejects frame");
    require(device.calls[0] == "acquire", "backend acquires image before injected recorder rejects frame");
    require(device.calls[1] == "begin", "backend begins before injected recorder rejects frame");
    require(!recorder.recorder_state().ready, "injected recorder exposes rejected state");
}

void test_vulkan_backend_adapter_falls_back_when_image_resource_is_missing()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        ""));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    vulkan_backend::diagnostic_vulkan_command_recorder recorder;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        recorder,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when image texture resource is missing");
    require(result.attempted, "backend records attempted frame with missing image resource");
    require(result.fallback_required, "backend requires fallback when image resource is missing");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::resource_binding_unavailable,
        "backend reports resource binding fallback for missing image resource");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready,
        "backend stops after frame plan when image resource binding is missing");
    require_completed_swapchain_policy(result.swapchain_policy, 16, 16);
    require_frame_fallback_summary(
        result.fallback_summary,
        vulkan_backend::vulkan_backend_fallback_reason::resource_binding_unavailable,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal,
        vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready);
    require(result.pipeline.completed(), "backend validates pipeline before resource binding failure");
    require(result.resource_bindings.checked, "backend checks resource bindings before missing image fallback");
    require(!result.resource_bindings.completed(), "backend records incomplete resource binding state");
    require(result.resource_bindings.missing_resource, "backend records missing resource binding");
    require(
        result.resource_bindings.missing_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "backend records missing image batch binding");
    require(result.resource_bindings.missing_command_index == 0, "backend records missing image command index");
    require(
        result.resource_bindings.missing_binding_kind == vulkan_backend::vulkan_resource_binding_kind::image_texture,
        "backend records missing image texture binding kind");
    require(
        result.resource_bindings.missing_resource_id == "missing_image_texture:image",
        "backend records stable missing image resource id");
    require(result.resource_bindings.planned_batch_count == 1, "backend records planned image batch count");
    require(result.resource_bindings.binding_count == 3, "backend records attempted image binding count");
    require(result.resource_bindings.batch_snapshots.size() == 1, "backend records image binding snapshot");
    require(
        result.resource_bindings.batch_snapshots.front().bindings[1].kind
            == vulkan_backend::vulkan_resource_binding_kind::image_texture,
        "backend records failed texture binding slot");
    require(result.resource_registry.checked, "backend checks resource registry before missing image fallback");
    require(!result.resource_registry.completed(), "backend records incomplete registry with missing image");
    require(result.resource_registry.missing_resource_count == 1, "backend registry records one missing image resource");
    require(
        result.resource_registry.missing_resources.front().resource_id == "missing_image_texture:image",
        "backend registry records stable missing image resource id");
    require(
        result.resource_registry.missing_resources.front().kind
            == vulkan_backend::vulkan_resource_binding_kind::image_texture,
        "backend registry records missing image texture kind");
    require(result.resource_registry.registered_resource_count == 2, "backend registry records available image bindings");
    require_descriptor_blocked_command_recorder_gate(
        result.command_recorder.gate,
        vulkan_backend::vulkan_batch_kind::image,
        0,
        vulkan_backend::vulkan_resource_binding_kind::image_texture,
        1,
        "missing_image_texture:image",
        1,
        1,
        1);
    require(result.command_recorder.ready, "backend keeps command recorder ready when image resource gates recording");
    require(!result.command_recorder.frame_open, "backend does not open recorder after image gate failure");
    require(!result.command_recorder.command_buffer_recorded, "backend records no command buffer after image gate failure");
    require(result.command_recorder.planned_batch_count == 1, "backend command recorder preserves gated image batch count");
    require(result.command_recorder.recorded_batch_count == 0, "backend command recorder records no gated image batches");
    require(result.command_buffer_submit.checked == false, "backend does not allocate command buffer after image gate failure");
    require(!result.frame_begun, "backend does not begin frame when image resource is missing");
    require(!result.swapchain.acquire_requested, "backend does not acquire image when resource binding is missing");
    require(!result.commands_recorded, "backend does not record commands when image resource is missing");
    require(device.calls.empty(), "backend does not call device lifecycle when image resource is missing");
    require(!recorder.recorder_state().frame_open, "backend does not open recorder when image resource is missing");
}

void test_vulkan_backend_adapter_falls_back_when_text_resource_is_missing()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(render_draw_command{
        .type = render_draw_command_type::text,
        .node_id = "text",
        .parent_node_id = {},
        .depth = 0,
        .bounds = render_rect{0.0f, 0.0f, 100.0f, 24.0f},
        .content_bounds = render_rect{0.0f, 0.0f, 100.0f, 24.0f},
        .paint = render_paint{.source = "white", .color = render_color{1.0f, 1.0f, 1.0f, 1.0f}},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    });

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when text resource is missing");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::resource_binding_unavailable,
        "backend reports resource binding fallback for missing text resource");
    require(result.resource_bindings.missing_resource, "backend records missing text resource");
    require(
        result.resource_bindings.missing_batch_kind == vulkan_backend::vulkan_batch_kind::text,
        "backend records missing text batch kind");
    require(
        result.resource_bindings.missing_binding_kind == vulkan_backend::vulkan_resource_binding_kind::text_run_buffer,
        "backend records missing text run binding kind");
    require(
        result.resource_bindings.missing_resource_id == "missing_text_run_buffer:text",
        "backend records stable missing text resource id");
    require(result.resource_bindings.binding_count == 3, "backend records attempted text binding count");
    require(result.resource_registry.checked, "backend checks resource registry before missing text fallback");
    require(result.resource_registry.missing_resource_count == 2, "backend registry records missing text resources");
    require(
        result.resource_registry.missing_resources[0].resource_id == "missing_text_run_buffer:text",
        "backend registry records stable missing text run id");
    require(
        result.resource_registry.missing_resources[1].resource_id == "missing_text_glyph_atlas:text",
        "backend registry records stable missing glyph atlas id");
    require_descriptor_blocked_command_recorder_gate(
        result.command_recorder.gate,
        vulkan_backend::vulkan_batch_kind::text,
        0,
        vulkan_backend::vulkan_resource_binding_kind::text_run_buffer,
        1,
        "missing_text_run_buffer:text",
        1,
        1,
        2);
    require(result.command_recorder.ready, "backend keeps command recorder ready when text resource gates recording");
    require(!result.command_recorder.frame_open, "backend does not open recorder after text gate failure");
    require(!result.command_recorder.command_buffer_recorded, "backend records no command buffer after text gate failure");
    require(result.command_recorder.planned_batch_count == 1, "backend command recorder preserves gated text batch count");
    require(result.command_recorder.recorded_batch_count == 0, "backend command recorder records no gated text batches");
    require(result.command_buffer_submit.checked == false, "backend does not allocate command buffer after text gate failure");
    require(device.calls.empty(), "backend does not call device lifecycle when text resource is missing");
}

void test_vulkan_backend_adapter_falls_back_when_injected_recorder_fails_record_batch()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "first",
        render_rect{0.0f, 0.0f, 40.0f, 40.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_quad_command(
        "second",
        render_rect{50.0f, 50.0f, 20.0f, 20.0f},
        render_color{0.0f, 1.0f, 0.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    vulkan_backend::diagnostic_vulkan_command_recorder recorder(
        vulkan_backend::diagnostic_vulkan_command_recorder_options{
            .ready = true,
            .fail_at = vulkan_backend::vulkan_command_recorder_failure_stage::record_draw_batch,
            .fail_recording_index = 1,
        });
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        recorder,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when injected recorder fails a batch");
    require(result.frame_begun, "backend begins frame before injected record failure");
    require(!result.commands_recorded, "backend does not call device recording after injected record failure");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "backend reports record fallback when injected recorder fails a batch");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_begun,
        "backend stops at frame begun stage when injected recorder fails a batch");
    require_failed_frame_lifecycle_policy(
        result.lifecycle_policy,
        vulkan_backend::vulkan_frame_lifecycle_step::render,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal,
        vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        3,
        2);
    require(result.command_recorder.ready, "backend records ready injected recorder on record failure");
    require(result.command_recorder.frame_open, "backend records open frame on record failure");
    require(!result.command_recorder.command_buffer_recorded, "backend records unfinished buffer on record failure");
    require(result.command_recorder.failed(), "backend records injected recorder record failure");
    require(
        result.command_recorder.failure_stage
            == vulkan_backend::vulkan_command_recorder_failure_stage::record_draw_batch,
        "backend records record failure stage from injected recorder");
    require(result.command_recorder.failure_recording_index == 1, "backend records failed batch index");
    require(result.command_recorder.planned_batch_count == 2, "backend preserves planned count on record failure");
    require(result.command_recorder.recorded_batch_count == 1, "backend preserves recorded count before failure");
    require(result.command_recorder.recorded_batches.size() == 1, "backend preserves successful batches before failure");
    require_failed_command_buffer_recording_state(
        result.command_buffer_submit,
        vulkan_backend::vulkan_command_recorder_failure_stage::record_draw_batch,
        1,
        2,
        1,
        false);
    require(result.swapchain.acquired(), "backend acquires image before injected record failure");
    require(!result.swapchain.present_requested, "backend does not present image after injected record failure");
    require(device.calls.size() == 2, "backend stops device lifecycle after acquire on injected record failure");
    require(device.calls[0] == "acquire", "backend acquires image before injected recorder fails a batch");
    require(device.calls[1] == "begin", "backend begins before injected recorder fails a batch");
}

void test_vulkan_backend_adapter_falls_back_when_injected_recorder_fails_finish()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    vulkan_backend::diagnostic_vulkan_command_recorder recorder(
        vulkan_backend::diagnostic_vulkan_command_recorder_options{
            .ready = true,
            .fail_at = vulkan_backend::vulkan_command_recorder_failure_stage::finish_recording,
        });
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        recorder,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when injected recorder fails finish");
    require(result.frame_begun, "backend begins frame before injected finish failure");
    require(!result.commands_recorded, "backend does not call device recording after injected finish failure");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "backend reports record fallback when injected recorder fails finish");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_begun,
        "backend stops at frame begun stage when injected recorder fails finish");
    require_failed_frame_lifecycle_policy(
        result.lifecycle_policy,
        vulkan_backend::vulkan_frame_lifecycle_step::render,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal,
        vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        3,
        2);
    require(result.command_recorder.ready, "backend records ready injected recorder on finish failure");
    require(result.command_recorder.frame_open, "backend records open frame on finish failure");
    require(!result.command_recorder.command_buffer_recorded, "backend records unfinished buffer on finish failure");
    require(result.command_recorder.failed(), "backend records injected recorder finish failure");
    require(
        result.command_recorder.failure_stage
            == vulkan_backend::vulkan_command_recorder_failure_stage::finish_recording,
        "backend records finish failure stage from injected recorder");
    require(result.command_recorder.failure_recording_index == 1, "backend records finish failure index");
    require(result.command_recorder.planned_batch_count == 1, "backend preserves planned count on finish failure");
    require(result.command_recorder.recorded_batch_count == 1, "backend preserves recorded count before finish failure");
    require(result.command_recorder.recorded_batches.size() == 1, "backend preserves batches before finish failure");
    require_failed_command_buffer_recording_state(
        result.command_buffer_submit,
        vulkan_backend::vulkan_command_recorder_failure_stage::finish_recording,
        1,
        1,
        1,
        true);
    require(result.swapchain.acquired(), "backend acquires image before injected finish failure");
    require(!result.swapchain.present_requested, "backend does not present image after injected finish failure");
    require(device.calls.size() == 2, "backend stops device lifecycle after acquire on injected finish failure");
    require(device.calls[0] == "acquire", "backend acquires image before injected recorder fails finish");
    require(device.calls[1] == "begin", "backend begins before injected recorder fails finish");
}

void test_vulkan_backend_adapter_falls_back_when_recording_fails()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    device.record_succeeds = false;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when command recording fails");
    require(result.attempted, "backend records attempted frame when command recording fails");
    require(result.fallback_required, "backend requires fallback when command recording fails");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "backend reports command recording fallback reason");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_begun,
        "backend reaches frame begun stage before recording failure");
    require(result.surface_ready, "backend had a surface before recording failed");
    require(result.frame_begun, "backend began frame before recording failed");
    require(result.swapchain.acquired(), "backend acquired image before device recording failed");
    require(!result.swapchain.present_requested, "backend does not present image after device recording failure");
    require(result.frame_sync.acquire_completed(), "backend completes acquire sync before device recording failure");
    require(!result.frame_sync.submit_wait_image_available_semaphore.requested, "backend does not submit-wait after recording failure");
    require(!result.frame_sync.present_wait_render_finished_semaphore.requested, "backend does not present-wait after recording failure");
    require_failed_frame_lifecycle_policy(
        result.lifecycle_policy,
        vulkan_backend::vulkan_frame_lifecycle_step::render,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal,
        vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        3,
        2);
    require(!result.commands_recorded, "backend reports failed command recording");
    require(!result.frame_submitted, "backend does not submit after failed recording");
    require(!result.frame_presented, "backend does not present after failed recording");
    require(result.planned_batch_count == 1, "backend still reports planned batch count before failure");
    require(result.recorded_batch_count == 0, "backend does not count batches after failed recording");
    require(result.command_recorder.ready, "command recorder was ready before recording failure");
    require(result.command_recorder.frame_open, "command recorder frame was open before device recording failure");
    require(result.command_recorder.command_buffer_recorded, "command recorder records buffer before device recording failure");
    require(result.command_recorder.planned_batch_count == 1, "command recorder tracks planned count before device failure");
    require(result.command_recorder.recorded_batch_count == 1, "command recorder tracks recorded count before device failure");
    require(result.command_recorder.recorded_batches.size() == 1, "command recorder stores batch before device failure");
    require_unsubmitted_recorded_command_buffer_state(result.command_buffer_submit, 1);
    require(device.calls.size() == 3, "backend stops lifecycle after failed recording");
    require(device.calls[0] == "acquire", "backend acquires image before failed recording");
    require(device.calls[1] == "begin", "backend begins before failed recording");
    require(device.calls[2] == "record", "backend records before stopping");
}

} // namespace

int main()
{
    test_vulkan_command_recorder_failure_stage_names_are_stable();
    test_vulkan_command_recorder_gate_status_names_are_stable();
    test_vulkan_command_buffer_recording_status_names_are_stable();
    test_vulkan_diagnostic_command_recorder_records_planned_batches();
    test_vulkan_diagnostic_command_recorder_reports_begin_failure();
    test_vulkan_diagnostic_command_recorder_reports_record_failure();
    test_vulkan_diagnostic_command_recorder_reports_finish_failure();
    test_vulkan_backend_adapter_falls_back_when_command_recorder_is_unready();
    test_vulkan_backend_adapter_falls_back_when_injected_recorder_rejects_frame();
    test_vulkan_backend_adapter_falls_back_when_image_resource_is_missing();
    test_vulkan_backend_adapter_falls_back_when_text_resource_is_missing();
    test_vulkan_backend_adapter_falls_back_when_injected_recorder_fails_record_batch();
    test_vulkan_backend_adapter_falls_back_when_injected_recorder_fails_finish();
    test_vulkan_backend_adapter_falls_back_when_recording_fails();
    return 0;
}
