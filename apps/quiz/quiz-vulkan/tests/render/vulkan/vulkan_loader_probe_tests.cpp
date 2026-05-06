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

} // namespace

int main()
{
    test_fake_vulkan_loader_reports_available_when_required_symbol_exists();
    test_fake_vulkan_loader_reports_required_symbol_missing();
    test_system_vulkan_loader_reports_missing_library_for_explicit_missing_candidate();
    test_vulkan_loader_probe_status_names_are_stable();
    return 0;
}
