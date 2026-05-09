#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_swapchain.h"

#include <cassert>
#include <cstdio>
#include <string_view>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_acquire_request
make_ready_request()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_swapchain_image_acquire_request{
        .requested = true,
        .lifecycle_ready = true,
        .swapchain_ready = true,
        .swapchain = vulkan_backend::vulkan_swapchain_handle{.value = 10},
        .image_count = 3,
        .timeout_nanoseconds = 0,
        .image_available_semaphore_ready = true,
        .fence_ready = true,
        .allow_suboptimal = true,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_result
make_acquire_result(
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status status)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const bool image_ready =
        status == vulkan_backend::vulkan_swapchain_acquire_status::acquired
        || status == vulkan_backend::vulkan_swapchain_acquire_status::suboptimal;
    return vulkan_backend::vulkan_swapchain_acquire_result{
        .status = status,
        .image = vulkan_backend::vulkan_swapchain_image_state{
            .id = image_ready ? vulkan_backend::vulkan_swapchain_image_id{.value = 2}
                              : vulkan_backend::vulkan_swapchain_image_id{},
            .available = image_ready,
            .acquired = image_ready,
            .presented = false,
        },
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_acquire_plan_result
make_acquire_plan(
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status status)
{
    return quiz_vulkan::render::vulkan_backend::build_vulkan_swapchain_image_acquire_plan(
        make_ready_request(),
        make_acquire_result(status));
}

void test_swapchain_recreate_policy_status_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::swapchain_present_status_name(
            vulkan_backend::vulkan_swapchain_present_status::out_of_date)
            == std::string_view{"out_of_date"},
        "swapchain present out-of-date status name is stable");
    require(
        vulkan_backend::swapchain_present_status_name(
            vulkan_backend::vulkan_swapchain_present_status::suboptimal)
            == std::string_view{"suboptimal"},
        "swapchain present suboptimal status name is stable");
    require(
        vulkan_backend::swapchain_present_status_name(
            vulkan_backend::vulkan_swapchain_present_status::error)
            == std::string_view{"error"},
        "swapchain present error status name is stable");
    require(
        vulkan_backend::swapchain_recreate_policy_action_name(
            vulkan_backend::vulkan_swapchain_recreate_policy_action::keep_rendering)
            == std::string_view{"keep_rendering"},
        "swapchain recreate policy keep-rendering action name is stable");
    require(
        vulkan_backend::swapchain_recreate_policy_action_name(
            vulkan_backend::vulkan_swapchain_recreate_policy_action::recreate_immediately)
            == std::string_view{"recreate_immediately"},
        "swapchain recreate policy immediate recreate action name is stable");
    require(
        vulkan_backend::swapchain_recreate_policy_action_name(
            vulkan_backend::vulkan_swapchain_recreate_policy_action::fatal_error)
            == std::string_view{"fatal_error"},
        "swapchain recreate policy fatal action name is stable");
}

void test_swapchain_recreate_policy_keeps_rendering_for_normal_frame()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_swapchain_recreate_policy_result policy =
        vulkan_backend::evaluate_vulkan_swapchain_recreate_policy(
            make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::acquired),
            vulkan_backend::vulkan_swapchain_present_result{
                .status = vulkan_backend::vulkan_swapchain_present_status::presented,
                .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 2},
            });

    require(policy.checked, "normal swapchain recreate policy is checked");
    require(policy.keep_rendering(), "normal swapchain frame keeps rendering");
    require(policy.allows_frame_progress(), "normal swapchain frame allows progress");
    require(!policy.should_recreate_immediately, "normal swapchain frame does not recreate immediately");
    require(!policy.should_recreate_after_frame, "normal swapchain frame does not recreate after frame");
    require(!policy.should_skip_submit, "normal swapchain frame does not skip submit");
    require(!policy.fatal, "normal swapchain frame has no fatal error");
}

void test_swapchain_recreate_policy_recreates_immediately_for_out_of_date_acquire()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_swapchain_recreate_policy_result policy =
        vulkan_backend::evaluate_vulkan_swapchain_recreate_policy(
            make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::out_of_date));

    require(policy.checked, "out-of-date acquire policy is checked");
    require(policy.recreate_immediately(), "out-of-date acquire recreates immediately");
    require(policy.should_skip_submit, "out-of-date acquire skips submit");
    require(!policy.allows_frame_progress(), "out-of-date acquire blocks frame progress");
    require(policy.acquire_out_of_date, "out-of-date acquire flag is set");
}

void test_swapchain_recreate_policy_recreates_after_suboptimal_frame()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_swapchain_recreate_policy_result acquire_policy =
        vulkan_backend::evaluate_vulkan_swapchain_recreate_policy(
            make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::suboptimal));
    require(acquire_policy.recreate_after_frame(), "suboptimal acquire recreates after frame");
    require(acquire_policy.allows_frame_progress(), "suboptimal acquire keeps frame progress");
    require(acquire_policy.acquire_suboptimal, "suboptimal acquire flag is set");

    const vulkan_backend::vulkan_swapchain_recreate_policy_result present_policy =
        vulkan_backend::evaluate_vulkan_swapchain_recreate_policy(
            make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::acquired),
            vulkan_backend::vulkan_swapchain_present_result{
                .status = vulkan_backend::vulkan_swapchain_present_status::suboptimal,
                .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 2},
            });
    require(present_policy.recreate_after_frame(), "suboptimal present recreates after frame");
    require(present_policy.present_suboptimal, "suboptimal present flag is set");
    require(present_policy.allows_frame_progress(), "suboptimal present keeps frame progress");
}

void test_swapchain_recreate_policy_skips_submit_for_backpressure()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_swapchain_recreate_policy_result timeout_policy =
        vulkan_backend::evaluate_vulkan_swapchain_recreate_policy(
            make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::timeout));
    require(timeout_policy.skip_submit(), "timeout acquire skips submit");
    require(timeout_policy.acquire_timed_out, "timeout acquire flag is set");
    require(!timeout_policy.should_recreate_immediately, "timeout acquire does not recreate immediately");
    require(!timeout_policy.fatal, "timeout acquire is not fatal");

    const vulkan_backend::vulkan_swapchain_recreate_policy_result backpressure_policy =
        vulkan_backend::evaluate_vulkan_swapchain_recreate_policy(
            make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::backpressured));
    require(backpressure_policy.skip_submit(), "backpressured acquire skips submit");
}

void test_swapchain_recreate_policy_reports_present_out_of_date_and_fatal_errors()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_swapchain_recreate_policy_result out_of_date_present =
        vulkan_backend::evaluate_vulkan_swapchain_recreate_policy(
            make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::acquired),
            vulkan_backend::vulkan_swapchain_present_result{
                .status = vulkan_backend::vulkan_swapchain_present_status::out_of_date,
                .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 2},
            });
    require(out_of_date_present.recreate_after_frame(), "out-of-date present recreates after frame");
    require(out_of_date_present.present_out_of_date, "out-of-date present flag is set");

    const vulkan_backend::vulkan_swapchain_recreate_policy_result acquire_error =
        vulkan_backend::evaluate_vulkan_swapchain_recreate_policy(
            make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::error));
    require(acquire_error.fatal_error(), "acquire error surfaces fatal policy");
    require(acquire_error.acquire_error, "acquire fatal flag is set");

    const vulkan_backend::vulkan_swapchain_recreate_policy_result present_error =
        vulkan_backend::evaluate_vulkan_swapchain_recreate_policy(
            make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::acquired),
            vulkan_backend::vulkan_swapchain_present_result{
                .status = vulkan_backend::vulkan_swapchain_present_status::error,
                .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 2},
            });
    require(present_error.fatal_error(), "present error surfaces fatal policy");
    require(present_error.present_error, "present fatal flag is set");
}

} // namespace

int main()
{
    test_swapchain_recreate_policy_status_names_are_stable();
    test_swapchain_recreate_policy_keeps_rendering_for_normal_frame();
    test_swapchain_recreate_policy_recreates_immediately_for_out_of_date_acquire();
    test_swapchain_recreate_policy_recreates_after_suboptimal_frame();
    test_swapchain_recreate_policy_skips_submit_for_backpressure();
    test_swapchain_recreate_policy_reports_present_out_of_date_and_fatal_errors();
    return 0;
}
