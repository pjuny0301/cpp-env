#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_loader.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

quiz_vulkan::render::vulkan_backend::vulkan_loader_probe_request make_probe_request(
    std::string library_name)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_loader_probe_request{
        .candidate_library_names = {std::move(library_name)},
        .required_symbol_name = std::string{vulkan_backend::vulkan_loader_required_symbol_name()},
        .use_default_library_names = false,
    };
}

void test_fake_vulkan_loader_reports_available_when_required_symbol_exists()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_loader loader(
        vulkan_backend::fake_vulkan_loader_options{
            .library_name = "fake-vulkan-loader",
            .library_available = true,
            .required_symbol_available = true,
        });

    const vulkan_backend::vulkan_loader_probe_result result =
        vulkan_backend::probe_vulkan_loader(loader, make_probe_request("fake-vulkan-loader"));

    require(result.checked, "available probe records that loader availability was checked");
    require(
        result.status == vulkan_backend::vulkan_loader_probe_status::available,
        "available probe reports available status");
    require(result.available(), "available probe result reports available");
    require(result.library_found, "available probe records a discovered loader library");
    require(result.required_symbol_found, "available probe records vkGetInstanceProcAddr");
    require(
        result.loaded_library_name == "fake-vulkan-loader",
        "available probe records loaded library name");
    require(
        result.required_symbol_name == vulkan_backend::vulkan_loader_required_symbol_name(),
        "available probe records required Vulkan loader symbol name");
    require(result.attempted_library_count == 1, "available probe records one attempted library");
    require(
        result.attempted_library_names.size() == 1,
        "available probe records attempted library names");
    require(
        result.attempted_library_names.front() == "fake-vulkan-loader",
        "available probe records the attempted fake loader name");
    require(
        result.candidate_diagnostics.size() == 1,
        "available probe records one candidate diagnostic");
    require(
        result.candidate_diagnostics.front().library_name == "fake-vulkan-loader",
        "available probe candidate records the fake loader name");
    require(
        result.candidate_diagnostics.front().status
            == vulkan_backend::vulkan_loader_candidate_status::usable,
        "available probe candidate is classified as usable");
    require(
        result.candidate_diagnostics.front().library_found,
        "available probe candidate records a found library");
    require(
        result.candidate_diagnostics.front().required_symbol_found,
        "available probe candidate records a found required symbol");
    require(
        result.candidate_diagnostics.front().usable(),
        "available probe candidate passes the usable gate");
}

void test_fake_vulkan_loader_reports_required_symbol_missing()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_loader loader(
        vulkan_backend::fake_vulkan_loader_options{
            .library_name = "fake-vulkan-loader",
            .library_available = true,
            .required_symbol_available = false,
        });

    const vulkan_backend::vulkan_loader_probe_result result =
        vulkan_backend::probe_vulkan_loader(loader, make_probe_request("fake-vulkan-loader"));

    require(result.checked, "missing-symbol probe records that loader availability was checked");
    require(
        result.status == vulkan_backend::vulkan_loader_probe_status::required_symbol_missing,
        "missing-symbol probe reports required symbol missing status");
    require(!result.available(), "missing-symbol probe result is not available");
    require(result.library_found, "missing-symbol probe records a discovered loader library");
    require(
        !result.required_symbol_found,
        "missing-symbol probe records that vkGetInstanceProcAddr is unavailable");
    require(
        result.loaded_library_name == "fake-vulkan-loader",
        "missing-symbol probe records loaded library name");
    require(
        result.attempted_library_count == 1,
        "missing-symbol probe records one attempted library");
    require(
        result.candidate_diagnostics.size() == 1,
        "missing-symbol probe records one candidate diagnostic");
    require(
        result.candidate_diagnostics.front().status
            == vulkan_backend::vulkan_loader_candidate_status::required_symbol_missing,
        "missing-symbol probe candidate is classified by missing vkGetInstanceProcAddr");
    require(
        result.candidate_diagnostics.front().library_found,
        "missing-symbol probe candidate records the found loader library");
    require(
        !result.candidate_diagnostics.front().required_symbol_found,
        "missing-symbol probe candidate records the missing required symbol");
    require(
        !result.candidate_diagnostics.front().usable(),
        "missing-symbol probe candidate is not usable");
}

void test_fake_vulkan_loader_classifies_candidates_until_usable_loader()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_loader loader(
        vulkan_backend::fake_vulkan_loader_options{
            .library_name = "fake-vulkan-loader",
            .library_available = true,
            .required_symbol_available = true,
        });
    const vulkan_backend::vulkan_loader_probe_result result =
        vulkan_backend::probe_vulkan_loader(
            loader,
            vulkan_backend::vulkan_loader_probe_request{
                .candidate_library_names = {
                    "missing-vulkan-loader",
                    "fake-vulkan-loader",
                    "unused-vulkan-loader",
                },
                .required_symbol_name =
                    std::string{vulkan_backend::vulkan_loader_required_symbol_name()},
                .use_default_library_names = false,
            });

    require(result.available(), "candidate classification finds a usable loader");
    require(
        result.attempted_library_count == 2,
        "candidate classification stops after the first usable loader");
    require(
        result.candidate_diagnostics.size() == 2,
        "candidate classification records attempted candidates only");
    require(
        result.candidate_diagnostics[0].library_name == "missing-vulkan-loader",
        "candidate classification records the missing candidate name");
    require(
        result.candidate_diagnostics[0].status
            == vulkan_backend::vulkan_loader_candidate_status::library_missing,
        "candidate classification marks the missing library candidate");
    require(
        !result.candidate_diagnostics[0].library_found,
        "candidate classification records that the first library was missing");
    require(
        result.candidate_diagnostics[1].library_name == "fake-vulkan-loader",
        "candidate classification records the usable candidate name");
    require(
        result.candidate_diagnostics[1].status
            == vulkan_backend::vulkan_loader_candidate_status::usable,
        "candidate classification marks the found loader candidate usable");
    require(
        result.candidate_diagnostics[1].usable(),
        "candidate classification usable helper follows the candidate state");
}

void test_system_vulkan_loader_reports_missing_library_for_explicit_missing_candidate()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::system_vulkan_loader loader;
    const vulkan_backend::vulkan_loader_probe_result result =
        vulkan_backend::probe_vulkan_loader(
            loader,
            make_probe_request("quiz_vulkan_missing_vulkan_loader_probe_20260507_7f9b0b7e.dll"));

    require(result.checked, "missing-library probe records that loader availability was checked");
    require(
        result.status == vulkan_backend::vulkan_loader_probe_status::library_missing,
        "missing-library probe reports library missing status");
    require(!result.available(), "missing-library probe result is not available");
    require(!result.library_found, "missing-library probe records no discovered loader library");
    require(
        !result.required_symbol_found,
        "missing-library probe records that vkGetInstanceProcAddr is unavailable");
    require(result.loaded_library_name.empty(), "missing-library probe records no loaded library");
    require(
        result.attempted_library_count == 1,
        "missing-library probe records one attempted library");
    require(
        result.attempted_library_names.front()
            == "quiz_vulkan_missing_vulkan_loader_probe_20260507_7f9b0b7e.dll",
        "missing-library probe records the explicit missing candidate");
    require(
        result.candidate_diagnostics.size() == 1,
        "missing-library probe records one candidate diagnostic");
    require(
        result.candidate_diagnostics.front().status
            == vulkan_backend::vulkan_loader_candidate_status::library_missing,
        "missing-library probe candidate is classified as library missing");
    require(
        !result.candidate_diagnostics.front().library_found,
        "missing-library probe candidate records no library");
    require(
        !result.candidate_diagnostics.front().required_symbol_found,
        "missing-library probe candidate records no required symbol");
}

void test_vulkan_loader_probe_status_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::vulkan_loader_required_symbol_name()
            == std::string_view{"vkGetInstanceProcAddr"},
        "Vulkan loader required symbol name is stable");
    require(
        vulkan_backend::loader_probe_status_name(
            vulkan_backend::vulkan_loader_probe_status::not_checked)
            == std::string_view{"not_checked"},
        "loader probe status name for not checked is stable");
    require(
        vulkan_backend::loader_probe_status_name(
            vulkan_backend::vulkan_loader_probe_status::available)
            == std::string_view{"available"},
        "loader probe status name for available is stable");
    require(
        vulkan_backend::loader_probe_status_name(
            vulkan_backend::vulkan_loader_probe_status::library_missing)
            == std::string_view{"library_missing"},
        "loader probe status name for library missing is stable");
    require(
        vulkan_backend::loader_probe_status_name(
            vulkan_backend::vulkan_loader_probe_status::required_symbol_missing)
            == std::string_view{"required_symbol_missing"},
        "loader probe status name for required symbol missing is stable");
}

void test_vulkan_loader_readiness_status_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::loader_readiness_status_name(
            vulkan_backend::vulkan_loader_readiness_status::not_checked)
            == std::string_view{"not_checked"},
        "loader readiness status name for not checked is stable");
    require(
        vulkan_backend::loader_readiness_status_name(
            vulkan_backend::vulkan_loader_readiness_status::ready)
            == std::string_view{"ready"},
        "loader readiness status name for ready is stable");
    require(
        vulkan_backend::loader_readiness_status_name(
            vulkan_backend::vulkan_loader_readiness_status::library_missing)
            == std::string_view{"library_missing"},
        "loader readiness status name for library missing is stable");
    require(
        vulkan_backend::loader_readiness_status_name(
            vulkan_backend::vulkan_loader_readiness_status::required_symbol_missing)
            == std::string_view{"required_symbol_missing"},
        "loader readiness status name for required symbol missing is stable");
}

void test_vulkan_loader_candidate_status_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::loader_candidate_status_name(
            vulkan_backend::vulkan_loader_candidate_status::not_checked)
            == std::string_view{"not_checked"},
        "loader candidate status name for not checked is stable");
    require(
        vulkan_backend::loader_candidate_status_name(
            vulkan_backend::vulkan_loader_candidate_status::library_missing)
            == std::string_view{"library_missing"},
        "loader candidate status name for library missing is stable");
    require(
        vulkan_backend::loader_candidate_status_name(
            vulkan_backend::vulkan_loader_candidate_status::required_symbol_missing)
            == std::string_view{"required_symbol_missing"},
        "loader candidate status name for required symbol missing is stable");
    require(
        vulkan_backend::loader_candidate_status_name(
            vulkan_backend::vulkan_loader_candidate_status::usable)
            == std::string_view{"usable"},
        "loader candidate status name for usable is stable");
}

void test_loader_probe_result_builds_instance_readiness_state()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_loader loader;
    const vulkan_backend::vulkan_loader_probe_result probe_result =
        vulkan_backend::probe_vulkan_loader(loader, make_probe_request("fake-vulkan-loader"));
    const vulkan_backend::vulkan_loader_readiness_state readiness =
        vulkan_backend::make_vulkan_loader_readiness_state(probe_result);

    require(readiness.checked, "loader readiness records checked probe");
    require(
        readiness.status == vulkan_backend::vulkan_loader_readiness_status::ready,
        "loader readiness maps available probe to ready");
    require(
        readiness.probe_status == vulkan_backend::vulkan_loader_probe_status::available,
        "loader readiness preserves source probe status");
    require(
        readiness.loader_library_available,
        "loader readiness records available loader library");
    require(
        readiness.instance_proc_address_available,
        "loader readiness records available vkGetInstanceProcAddr");
    require(readiness.instance_ready, "loader readiness drives instance readiness");
    require(readiness.ready_for_instance(), "loader readiness allows instance creation gate");
    require(
        readiness.loaded_library_name == "fake-vulkan-loader",
        "loader readiness carries loaded library name");
    require(
        readiness.required_symbol_name == vulkan_backend::vulkan_loader_required_symbol_name(),
        "loader readiness carries required symbol name");
    require(readiness.attempted_library_count == 1, "loader readiness carries attempt count");
}

void test_null_backend_loader_available_reaches_device_gate()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_loader loader;
    vulkan_backend::null_vulkan_backend_device device(
        vulkan_backend::probe_vulkan_loader(loader, make_probe_request("fake-vulkan-loader")));

    const vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle =
        device.current_lifecycle_readiness();
    require(lifecycle.loader.checked, "null backend records checked loader readiness");
    require(lifecycle.loader.ready_for_instance(), "null backend loader readiness is instance-ready");
    require(lifecycle.instance_ready, "null backend maps loader readiness to instance readiness");
    require(
        lifecycle.effective_instance_ready(),
        "null backend effective instance readiness follows loader readiness");
    require(!lifecycle.ready_for_frame(), "null backend is not frame-ready without device resources");

    const quiz_vulkan::render::render_draw_list draw_list;
    const vulkan_backend::vulkan_backend_frame_result result =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            draw_list,
            quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    require(result.lifecycle.instance_ready, "frame result records loader-driven instance readiness");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::device_unavailable,
        "loader-ready null backend falls through to device readiness gate");
}

void test_null_backend_missing_loader_stops_at_instance_gate()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_loader loader(
        vulkan_backend::fake_vulkan_loader_options{
            .library_name = "fake-vulkan-loader",
            .library_available = false,
            .required_symbol_available = true,
        });
    vulkan_backend::null_vulkan_backend_device device(
        vulkan_backend::probe_vulkan_loader(loader, make_probe_request("fake-vulkan-loader")));

    const quiz_vulkan::render::render_draw_list draw_list;
    const vulkan_backend::vulkan_backend_frame_result result =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            draw_list,
            quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    require(result.lifecycle.loader.checked, "missing-loader result records checked loader readiness");
    require(
        result.lifecycle.loader.status
            == vulkan_backend::vulkan_loader_readiness_status::library_missing,
        "missing-loader result maps probe to library missing readiness");
    require(!result.lifecycle.instance_ready, "missing-loader result keeps instance unavailable");
    require(
        !result.lifecycle.effective_instance_ready(),
        "missing-loader result blocks effective instance readiness");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::instance_unavailable,
        "missing-loader result stops at instance readiness gate");
}

void test_null_backend_missing_required_symbol_stops_at_instance_gate()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_loader loader(
        vulkan_backend::fake_vulkan_loader_options{
            .library_name = "fake-vulkan-loader",
            .library_available = true,
            .required_symbol_available = false,
        });
    vulkan_backend::null_vulkan_backend_device device(
        vulkan_backend::probe_vulkan_loader(loader, make_probe_request("fake-vulkan-loader")));

    const quiz_vulkan::render::render_draw_list draw_list;
    const vulkan_backend::vulkan_backend_frame_result result =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            draw_list,
            quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    require(
        result.lifecycle.loader.status
            == vulkan_backend::vulkan_loader_readiness_status::required_symbol_missing,
        "missing-symbol result maps probe to required symbol missing readiness");
    require(
        result.lifecycle.loader.loader_library_available,
        "missing-symbol result records that the loader library was found");
    require(
        !result.lifecycle.loader.instance_proc_address_available,
        "missing-symbol result records unavailable vkGetInstanceProcAddr");
    require(!result.lifecycle.instance_ready, "missing-symbol result keeps instance unavailable");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::instance_unavailable,
        "missing-symbol result stops at instance readiness gate");
}

} // namespace

int main()
{
    test_fake_vulkan_loader_reports_available_when_required_symbol_exists();
    test_fake_vulkan_loader_reports_required_symbol_missing();
    test_fake_vulkan_loader_classifies_candidates_until_usable_loader();
    test_system_vulkan_loader_reports_missing_library_for_explicit_missing_candidate();
    test_vulkan_loader_probe_status_names_are_stable();
    test_vulkan_loader_readiness_status_names_are_stable();
    test_vulkan_loader_candidate_status_names_are_stable();
    test_loader_probe_result_builds_instance_readiness_state();
    test_null_backend_loader_available_reaches_device_gate();
    test_null_backend_missing_loader_stops_at_instance_gate();
    test_null_backend_missing_required_symbol_stops_at_instance_gate();
    return 0;
}
