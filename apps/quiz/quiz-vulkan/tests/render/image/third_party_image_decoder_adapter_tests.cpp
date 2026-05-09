#include "render/image/third_party_image_decoder_adapter.h"
#include "render/image/image_texture_cache.h"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

template <typename T>
concept HasFakeCacheSnapshotField = requires(T value) {
    { value.cache_snapshot } -> std::same_as<quiz_vulkan::render::fake_image_texture_cache_snapshot&>;
};

template <typename T>
concept HasFakeUploadSnapshotField = requires(T value) {
    { value.upload_snapshot } -> std::same_as<quiz_vulkan::render::fake_image_texture_upload_snapshot&>;
};

static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_decoder_capability_manifest>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_decoder_capability_candidate_snapshot>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_external_decoder_selection_snapshot>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::stb_image_decoder_dependency_manifest>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::stb_image_decoder_adapter_selection_result>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_decoder_capability_manifest>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_decoder_capability_candidate_snapshot>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_external_decoder_selection_snapshot>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::stb_image_decoder_dependency_manifest>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::stb_image_decoder_adapter_selection_result>);

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

void append_byte(std::vector<std::byte>& bytes, unsigned char value)
{
    bytes.push_back(std::byte{value});
}

std::vector<std::byte> make_bytes(std::initializer_list<unsigned char> values)
{
    std::vector<std::byte> bytes;
    bytes.reserve(values.size());
    for (const unsigned char value : values) {
        append_byte(bytes, value);
    }
    return bytes;
}

std::vector<std::byte> make_jpeg_signature_bytes()
{
    return make_bytes({0xff, 0xd8, 0xff, 0xd9});
}

std::vector<std::byte> make_png_signature_bytes()
{
    return make_bytes({0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a});
}

std::vector<std::byte> make_bmp_signature_bytes()
{
    return make_bytes({'B', 'M', 0x00, 0x00});
}

std::vector<std::byte> make_ppm_bytes()
{
    std::vector<std::byte> bytes;
    for (const char value : std::string_view{"P6\n1 1\n255\n"}) {
        append_byte(bytes, static_cast<unsigned char>(value));
    }
    append_byte(bytes, 10);
    append_byte(bytes, 20);
    append_byte(bytes, 30);
    return bytes;
}

quiz_vulkan::render::render_decoded_image make_rgba_image(
    std::initializer_list<unsigned char> pixel_bytes)
{
    std::vector<std::byte> pixels;
    pixels.reserve(pixel_bytes.size());
    for (const unsigned char value : pixel_bytes) {
        append_byte(pixels, value);
    }

    return quiz_vulkan::render::render_decoded_image{
        .width = 1,
        .height = 1,
        .pixel_format = quiz_vulkan::render::render_image_pixel_format::rgba8_srgb,
        .pixels = std::move(pixels),
    };
}

quiz_vulkan::render::render_image_decode_request make_decode_request(
    std::string normalized_uri,
    std::vector<std::byte> encoded_bytes)
{
    return quiz_vulkan::render::render_image_decode_request{
        .source = quiz_vulkan::render::render_resolved_image_source{
            .original_uri = normalized_uri,
            .normalized_uri = std::move(normalized_uri),
            .kind = quiz_vulkan::render::render_image_source_kind::local_path,
        },
        .encoded_bytes = std::move(encoded_bytes),
    };
}

void require_candidate(
    const quiz_vulkan::render::render_image_decoder_capability_manifest& manifest,
    std::size_t index,
    quiz_vulkan::render::render_image_decoder_capability_candidate_kind kind,
    quiz_vulkan::render::render_image_decoder_capability_candidate_status status,
    std::string_view decoder_id,
    bool terminal)
{
    require(index < manifest.candidates.size(), "capability manifest candidate index exists");
    const quiz_vulkan::render::render_image_decoder_capability_candidate_snapshot& candidate =
        manifest.candidates[index];
    require(candidate.candidate_index == index, "capability manifest candidate records stable index");
    require(candidate.candidate_order == index + 1, "capability manifest candidate records stable order");
    require(candidate.kind == kind, "capability manifest candidate records kind");
    require(candidate.kind_name == quiz_vulkan::render::render_image_decoder_capability_candidate_kind_name(kind),
        "capability manifest candidate records kind name");
    require(candidate.status == status, "capability manifest candidate records status");
    require(candidate.status_name == quiz_vulkan::render::render_image_decoder_capability_candidate_status_name(status),
        "capability manifest candidate records status name");
    require(candidate.decoder_id == decoder_id, "capability manifest candidate records decoder id");
    require(candidate.terminal_candidate == terminal, "capability manifest candidate records terminal flag");
}

void require_stb_selection_status(
    const quiz_vulkan::render::stb_image_decoder_adapter_selection_result& selection,
    quiz_vulkan::render::stb_image_decoder_adapter_selection_status status,
    bool ready,
    bool fallback)
{
    require(selection.status == status, "stb selection records expected status");
    require(
        selection.status_name == quiz_vulkan::render::stb_image_decoder_adapter_selection_status_name(status),
        "stb selection status name is stable");
    require(selection.ready_for_external_decode == ready, "stb selection ready flag is expected");
    require(selection.fallback_to_standard_decoder_chain == fallback, "stb selection fallback flag is expected");
    require(selection.ok() == ready, "stb selection ok helper matches ready state");
}

std::filesystem::path locate_source_file(
    std::string_view app_relative_path,
    std::string_view project_relative_path)
{
    std::filesystem::path current = std::filesystem::current_path();
    while (true) {
        const std::filesystem::path app_candidate = current / std::string(app_relative_path);
        if (std::filesystem::exists(app_candidate)) {
            return app_candidate;
        }

        const std::filesystem::path project_candidate = current / std::string(project_relative_path);
        if (std::filesystem::exists(project_candidate)) {
            return project_candidate;
        }

        if (!current.has_parent_path() || current == current.parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    return {};
}

std::string read_text_file(const std::filesystem::path& path)
{
    std::ifstream input(path);
    require(input.good(), "source file opens for dependency guard");

    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

void test_adapter_decodes_matching_format_and_sets_metadata()
{
    using namespace quiz_vulkan::render;

    fake_third_party_image_decoder_backend backend;
    backend.set_decoder_id("fake_stb_decoder");
    backend.set_supported_formats({render_image_encoded_format::jpeg});
    backend.set_decoded_image(make_rgba_image({1, 2, 3, 4}));
    const third_party_image_decoder_adapter adapter(backend);

    const render_image_decode_request request =
        make_decode_request("textures/card.jpg", make_jpeg_signature_bytes());
    const render_image_decode_result result = adapter.decode(request);

    require(adapter.supports(request), "third-party adapter supports configured JPEG source");
    require(result.ok(), "third-party adapter decodes configured source");
    require(result.metadata.decoder_id == "fake_stb_decoder", "third-party metadata records decoder id");
    require(result.metadata.encoded_byte_count == request.encoded_bytes.size(), "third-party metadata records source byte count");
    require(
        result.metadata.format_detection.detected_format == render_image_encoded_format::jpeg,
        "third-party metadata records detected source format");
    require(result.metadata.format_detection.recognized_signature, "third-party metadata records signature match");
    require(result.metadata.size_validation.valid, "third-party metadata validates decoded payload");
    require(result.image.pixels == make_bytes({1, 2, 3, 4}), "third-party adapter preserves pixels");
    require(backend.inspect_requests.size() >= 2, "fake backend records capability inspections");
    require(backend.decode_requests.size() == 1, "fake backend records decode attempt");
}

void test_stb_dependency_selection_missing_falls_back()
{
    using namespace quiz_vulkan::render;

    fake_stb_image_decoder_dependency_probe probe;
    probe.set_missing("fake_stb_image_decoder");
    const render_image_decode_request request =
        make_decode_request("textures/card.jpg", make_jpeg_signature_bytes());

    const stb_image_decoder_adapter_selection_result selection =
        select_stb_image_decoder_adapter(request, probe);
    const third_party_image_decoder_capability capability =
        make_third_party_image_decoder_capability_from_stb_selection(request, selection);

    require(probe.probe_count == 1, "fake stb probe records dependency inspection");
    require_stb_selection_status(
        selection,
        stb_image_decoder_adapter_selection_status::fallback_missing_dependency,
        false,
        true);
    require(selection.decoder_id == "fake_stb_image_decoder", "missing stb selection records decoder id");
    require(selection.detected_format == render_image_encoded_format::jpeg, "missing stb selection detects JPEG");
    require(selection.detected_format_name == "jpeg", "missing stb selection records format name");
    require(selection.dependency_status == stb_image_decoder_dependency_status::missing, "missing stb status is stable");
    require(selection.dependency_status_name == "missing", "missing stb status name is stable");
    require(!selection.dependency_available, "missing stb selection records dependency unavailable");
    require(!selection.dependency_capability_ready, "missing stb selection records capability unavailable");
    require(!selection.format_supported_by_dependency, "missing stb selection avoids format support claim");
    require(selection.supported_format_matrix.size() == 4, "missing stb selection still exposes default format matrix");
    require(
        selection.diagnostic == "stb_image dependency is missing; falling back to standard image decoders",
        "missing stb selection diagnostic is stable");
    require(capability.status == third_party_image_decoder_adapter_status::unavailable, "missing stb capability is unavailable");
    require(capability.decoder_id == "fake_stb_image_decoder", "missing stb capability preserves decoder id");
    require(!capability.supports_decode(), "missing stb capability does not support decode");
}

void test_stb_dependency_selection_ready_for_jpeg()
{
    using namespace quiz_vulkan::render;

    fake_stb_image_decoder_dependency_probe probe;
    probe.set_available("fake_stb_image_decoder");
    const render_image_decode_request request =
        make_decode_request("textures/card.jpg", make_jpeg_signature_bytes());

    const stb_image_decoder_adapter_selection_result selection =
        select_stb_image_decoder_adapter(request, probe);
    const third_party_image_decoder_capability capability =
        make_third_party_image_decoder_capability_from_stb_selection(request, selection);
    const stb_image_decoder_format_matrix_entry* jpeg_entry =
        stb_image_decoder_format_matrix_entry_for(
            selection.supported_format_matrix,
            render_image_encoded_format::jpeg);

    require_stb_selection_status(
        selection,
        stb_image_decoder_adapter_selection_status::ready,
        true,
        false);
    require(selection.dependency_status == stb_image_decoder_dependency_status::available, "ready stb selection records dependency available");
    require(selection.dependency_available, "ready stb selection records dependency available flag");
    require(selection.dependency_capability_ready, "ready stb selection records complete capabilities");
    require(selection.format_supported_by_dependency, "ready stb selection records format support");
    require(!selection.internal_decoder_available, "ready JPEG selection has no internal decoder ownership");
    require(!selection.prefer_internal_decoder, "ready JPEG selection does not prefer internal decoder");
    require(selection.external_decode_enabled, "ready JPEG selection enables external decode");
    require(
        selection.diagnostic == "stb_image adapter selected for external jpeg decode",
        "ready stb selection diagnostic is stable");
    require(jpeg_entry != nullptr, "ready stb selection exposes JPEG matrix entry");
    require(jpeg_entry->route_to_external(), "ready JPEG matrix entry routes externally");
    require(jpeg_entry->format_name == "jpeg", "ready JPEG matrix entry records format name");
    require(jpeg_entry->mime_type == "image/jpeg", "ready JPEG matrix entry records MIME type");
    require(capability.status == third_party_image_decoder_adapter_status::supported, "ready stb capability is supported");
    require(capability.supports_decode(), "ready stb capability supports decode");
}

void test_stb_dependency_selection_preserves_internal_decoders()
{
    using namespace quiz_vulkan::render;

    fake_stb_image_decoder_dependency_probe probe;
    probe.set_available("fake_stb_image_decoder");

    const stb_image_decoder_adapter_selection_result png_selection =
        select_stb_image_decoder_adapter(
            make_decode_request("textures/card.png", make_png_signature_bytes()),
            probe);
    require_stb_selection_status(
        png_selection,
        stb_image_decoder_adapter_selection_status::fallback_internal_decoder_preferred,
        false,
        true);
    require(png_selection.detected_format == render_image_encoded_format::png, "PNG selection detects PNG");
    require(png_selection.format_supported_by_dependency, "PNG selection records stb support");
    require(png_selection.internal_decoder_available, "PNG selection records internal decoder ownership");
    require(png_selection.prefer_internal_decoder, "PNG selection preserves internal decoder");
    require(!png_selection.external_decode_enabled, "PNG selection keeps external route disabled");
    require(
        png_selection.diagnostic == "stb_image adapter preserves internal png decoder",
        "PNG internal preservation diagnostic is stable");

    const stb_image_decoder_adapter_selection_result bmp_selection =
        select_stb_image_decoder_adapter(
            make_decode_request("textures/card.bmp", make_bmp_signature_bytes()),
            probe);
    require_stb_selection_status(
        bmp_selection,
        stb_image_decoder_adapter_selection_status::fallback_internal_decoder_preferred,
        false,
        true);
    require(bmp_selection.detected_format == render_image_encoded_format::bmp, "BMP selection detects BMP");
    require(bmp_selection.format_supported_by_dependency, "BMP selection records stb support");
    require(bmp_selection.prefer_internal_decoder, "BMP selection preserves internal decoder");
}

void test_stb_dependency_selection_mismatched_capability_falls_back()
{
    using namespace quiz_vulkan::render;

    fake_stb_image_decoder_dependency_probe probe;
    probe.set_mismatched("fake_stb_image_decoder");
    const render_image_decode_request request =
        make_decode_request("textures/card.jpg", make_jpeg_signature_bytes());

    const stb_image_decoder_adapter_selection_result selection =
        select_stb_image_decoder_adapter(request, probe);
    const third_party_image_decoder_capability capability =
        make_third_party_image_decoder_capability_from_stb_selection(request, selection);

    require_stb_selection_status(
        selection,
        stb_image_decoder_adapter_selection_status::fallback_mismatched_capability,
        false,
        true);
    require(selection.dependency_available, "mismatched stb selection records dependency present");
    require(!selection.dependency_capability_ready, "mismatched stb selection records incomplete capability");
    require(
        selection.dependency_status == stb_image_decoder_dependency_status::mismatched_capability,
        "mismatched stb selection records dependency status");
    require(
        selection.diagnostic == "stb_image dependency is missing required decode capabilities",
        "mismatched stb selection diagnostic is stable");
    require(capability.status == third_party_image_decoder_adapter_status::unsupported_format, "mismatched stb capability is unsupported");
    require(!capability.supports_decode(), "mismatched stb capability does not support decode");
}

void test_stb_dependency_selection_respects_supported_format_matrix()
{
    using namespace quiz_vulkan::render;

    std::vector<stb_image_decoder_format_matrix_entry> matrix = make_default_stb_image_decoder_format_matrix();
    matrix[0] = make_stb_image_decoder_format_matrix_entry(
        render_image_encoded_format::jpeg,
        false,
        false,
        false);

    fake_stb_image_decoder_dependency_probe probe;
    probe.set_available("fake_stb_image_decoder", std::move(matrix));
    const render_image_decode_request request =
        make_decode_request("textures/card.jpg", make_jpeg_signature_bytes());

    const stb_image_decoder_adapter_selection_result selection =
        select_stb_image_decoder_adapter(request, probe);
    const stb_image_decoder_format_matrix_entry* jpeg_entry =
        stb_image_decoder_format_matrix_entry_for(
            selection.supported_format_matrix,
            render_image_encoded_format::jpeg);

    require_stb_selection_status(
        selection,
        stb_image_decoder_adapter_selection_status::fallback_unsupported_format,
        false,
        true);
    require(jpeg_entry != nullptr, "unsupported matrix exposes JPEG entry");
    require(!jpeg_entry->dependency_supports, "unsupported matrix records no JPEG support");
    require(!jpeg_entry->route_to_external(), "unsupported matrix does not route externally");
    require(!selection.format_supported_by_dependency, "unsupported matrix selection records no format support");
    require(
        selection.diagnostic == "stb_image dependency does not support detected image format",
        "unsupported matrix selection diagnostic is stable");
}

void test_optional_chain_decodes_with_adapter_before_standard_candidates()
{
    using namespace quiz_vulkan::render;

    fake_third_party_image_decoder_backend backend;
    backend.set_decoder_id("fake_stb_decoder");
    backend.set_supported_formats({render_image_encoded_format::jpeg});
    backend.set_decoded_image(make_rgba_image({5, 6, 7, 8}));
    const optional_third_party_image_decoder_chain decoder(backend);

    const render_image_decode_result result =
        decoder.decode(make_decode_request("textures/card.jpg", make_jpeg_signature_bytes()));

    require(decoder.decoder_count() == 4, "optional decoder chain exposes third-party plus standard candidates");
    require(result.ok(), "optional decoder chain decodes through third-party adapter");
    require(result.metadata.decoder_id == "fake_stb_decoder", "optional decoder chain reports adapter decoder id");
    require(result.decoder_diagnostics.size() == 1, "successful third-party decode stops before fallback chain");
    require(result.decoder_diagnostics[0].decoder_id == "fake_stb_decoder", "third-party diagnostic records decoder id");
    require(result.decoder_diagnostics[0].candidate_index == 0, "third-party diagnostic is first candidate");
    require(result.decoder_diagnostics[0].supported, "third-party diagnostic records supported candidate");
    require(result.decoder_diagnostics[0].decode_attempted, "third-party diagnostic records decode attempt");
    require(result.decoder_diagnostics[0].terminal_candidate, "third-party diagnostic records terminal success");

    const render_image_decoder_capability_manifest manifest =
        make_render_image_decoder_capability_manifest(
            make_decode_request("textures/card.jpg", make_jpeg_signature_bytes()),
            result);
    require(manifest.candidates.size() == 1, "third-party success manifest records one candidate");
    require(manifest.used_third_party_adapter, "third-party success manifest records adapter use");
    require(!manifest.fallback_used, "third-party success manifest records no fallback");
    require(manifest.decoded, "third-party success manifest records decoded result");
    require_candidate(
        manifest,
        0,
        render_image_decoder_capability_candidate_kind::third_party_adapter,
        render_image_decoder_capability_candidate_status::decoded,
        "fake_stb_decoder",
        true);
    require(manifest.terminal_decoder_id == "fake_stb_decoder", "third-party success manifest terminal id is adapter");
    require(
        manifest.terminal_kind == render_image_decoder_capability_candidate_kind::third_party_adapter,
        "third-party success manifest terminal kind is adapter");
}

void test_adapter_failure_falls_back_to_standard_decoder_chain()
{
    using namespace quiz_vulkan::render;

    fake_third_party_image_decoder_backend backend;
    backend.set_decoder_id("fake_stb_decoder");
    backend.set_supported_formats({render_image_encoded_format::ppm});
    backend.set_result(render_image_decode_result{
        .status = render_image_decode_status::invalid_data,
        .image = {},
        .diagnostic = "fake third-party decode failed",
        .metadata = {},
    });
    const optional_third_party_image_decoder_chain decoder(backend);

    const render_image_decode_result result =
        decoder.decode(make_decode_request("textures/card.ppm", make_ppm_bytes()));

    require(result.ok(), "optional decoder chain falls back after adapter failure");
    require(result.metadata.decoder_id == "ppm_image_decoder", "fallback selects standard PPM decoder");
    require(result.image.pixels == make_bytes({10, 20, 30, 0xff}), "fallback preserves PPM pixels");
    require(result.decoder_diagnostics.size() == 3, "fallback records adapter, BMP, and PPM diagnostics");
    require(result.decoder_diagnostics[0].decoder_id == "fake_stb_decoder", "fallback records adapter first");
    require(result.decoder_diagnostics[0].decode_attempted, "fallback records adapter decode attempt");
    require(!result.decoder_diagnostics[0].terminal_candidate, "failed adapter is not terminal when fallback succeeds");
    require(result.decoder_diagnostics[1].decoder_id == "bmp_image_decoder", "fallback reindexes BMP candidate");
    require(result.decoder_diagnostics[1].candidate_index == 1, "fallback BMP candidate index follows adapter");
    require(result.decoder_diagnostics[2].decoder_id == "ppm_image_decoder", "fallback reindexes PPM candidate");
    require(result.decoder_diagnostics[2].candidate_index == 2, "fallback PPM candidate index follows adapter");
    require(result.decoder_diagnostics[2].terminal_candidate, "fallback PPM candidate is terminal");

    const render_image_decoder_capability_manifest manifest =
        make_render_image_decoder_capability_manifest(
            make_decode_request("textures/card.ppm", make_ppm_bytes()),
            result);
    require(manifest.candidates.size() == 3, "adapter failure manifest records adapter, BMP, and PPM");
    require(manifest.used_third_party_adapter, "adapter failure manifest records adapter use");
    require(manifest.fallback_used, "adapter failure manifest records standard fallback");
    require_candidate(
        manifest,
        0,
        render_image_decoder_capability_candidate_kind::third_party_adapter,
        render_image_decoder_capability_candidate_status::decode_failed,
        "fake_stb_decoder",
        false);
    require_candidate(
        manifest,
        1,
        render_image_decoder_capability_candidate_kind::bmp,
        render_image_decoder_capability_candidate_status::unsupported_format,
        "bmp_image_decoder",
        false);
    require_candidate(
        manifest,
        2,
        render_image_decoder_capability_candidate_kind::ppm,
        render_image_decoder_capability_candidate_status::decoded,
        "ppm_image_decoder",
        true);
    require(manifest.terminal_decoder_id == "ppm_image_decoder", "adapter failure manifest terminal id is fallback PPM");
}

void test_unsupported_adapter_falls_back_to_standard_decoder_chain()
{
    using namespace quiz_vulkan::render;

    fake_third_party_image_decoder_backend backend;
    backend.set_decoder_id("fake_stb_decoder");
    backend.set_supported_formats({render_image_encoded_format::jpeg});
    backend.set_decoded_image(make_rgba_image({1, 2, 3, 4}));
    const optional_third_party_image_decoder_chain decoder(backend);

    const render_image_decode_result result =
        decoder.decode(make_decode_request("textures/card.ppm", make_ppm_bytes()));

    require(result.ok(), "unsupported adapter capability falls back to standard decoder");
    require(result.metadata.decoder_id == "ppm_image_decoder", "unsupported adapter fallback selects PPM");
    require(result.decoder_diagnostics.size() == 3, "unsupported adapter fallback records adapter, BMP, and PPM");

    const render_image_decoder_capability_manifest manifest =
        make_render_image_decoder_capability_manifest(
            make_decode_request("textures/card.ppm", make_ppm_bytes()),
            result);
    require(manifest.used_third_party_adapter, "unsupported adapter manifest records adapter");
    require(manifest.fallback_used, "unsupported adapter manifest records fallback");
    require_candidate(
        manifest,
        0,
        render_image_decoder_capability_candidate_kind::third_party_adapter,
        render_image_decoder_capability_candidate_status::unsupported_format,
        "fake_stb_decoder",
        false);
    require_candidate(
        manifest,
        2,
        render_image_decoder_capability_candidate_kind::ppm,
        render_image_decoder_capability_candidate_status::decoded,
        "ppm_image_decoder",
        true);
}

void test_unavailable_adapter_preserves_standard_unsupported_failure()
{
    using namespace quiz_vulkan::render;

    fake_third_party_image_decoder_backend backend;
    backend.set_decoder_id("fake_stb_decoder");
    backend.set_available(false);
    backend.set_supported_formats({render_image_encoded_format::jpeg});
    const optional_third_party_image_decoder_chain decoder(backend);

    const render_image_decode_result result =
        decoder.decode(make_decode_request("textures/card.jpg", make_jpeg_signature_bytes()));

    require(!result.ok(), "unavailable adapter keeps unsupported source as a failure");
    require(
        result.status == render_image_decode_status::unsupported_format,
        "unavailable adapter preserves unsupported format status");
    require(result.image.empty(), "unavailable adapter does not create placeholder pixels");
    require(result.decoder_diagnostics.size() == 4, "unavailable adapter preserves adapter and standard diagnostics");
    require(result.decoder_diagnostics[0].decoder_id == "fake_stb_decoder", "unavailable adapter diagnostic is visible");
    require(!result.decoder_diagnostics[0].supported, "unavailable adapter diagnostic records unsupported candidate");
    require(!result.decoder_diagnostics[0].decode_attempted, "unavailable adapter diagnostic avoids decode attempt");
    require(result.decoder_diagnostics[1].decoder_id == "bmp_image_decoder", "standard BMP follows adapter diagnostic");
    require(result.decoder_diagnostics[3].terminal_candidate, "standard chain terminal diagnostic is stable");
    require(
        result.decoder_diagnostics[3].diagnostic == "decoder chain exhausted all candidates",
        "standard unsupported diagnostic remains deterministic");
    require(backend.decode_requests.empty(), "unavailable adapter does not decode");

    const render_image_decoder_capability_manifest manifest =
        make_render_image_decoder_capability_manifest(
            make_decode_request("textures/card.jpg", make_jpeg_signature_bytes()),
            result);
    require(manifest.candidates.size() == 5, "unavailable adapter manifest records adapter, standard candidates, and terminal");
    require(manifest.used_third_party_adapter, "unavailable adapter manifest records adapter");
    require(manifest.fallback_used, "unavailable adapter manifest records fallback");
    require(!manifest.decoded, "unavailable adapter manifest records failure");
    require_candidate(
        manifest,
        0,
        render_image_decoder_capability_candidate_kind::third_party_adapter,
        render_image_decoder_capability_candidate_status::unavailable,
        "fake_stb_decoder",
        false);
    require_candidate(
        manifest,
        4,
        render_image_decoder_capability_candidate_kind::unsupported_terminal,
        render_image_decoder_capability_candidate_status::unsupported_terminal,
        "unsupported_terminal",
        true);
    require(
        manifest.terminal_kind == render_image_decoder_capability_candidate_kind::unsupported_terminal,
        "unavailable adapter manifest terminal kind is explicit unsupported terminal");
}

void test_adapter_unsupported_format_is_placeholder_safe()
{
    using namespace quiz_vulkan::render;

    fake_third_party_image_decoder_backend backend;
    backend.set_decoder_id("fake_stb_decoder");
    backend.set_supported_formats({render_image_encoded_format::jpeg});
    backend.set_decoded_image(make_rgba_image({1, 2, 3, 4}));
    const third_party_image_decoder_adapter adapter(backend);

    const render_image_decode_result result =
        adapter.decode(make_decode_request("textures/card.ppm", make_ppm_bytes()));

    require(!result.ok(), "unsupported direct adapter decode fails");
    require(
        result.status == render_image_decode_status::unsupported_format,
        "unsupported direct adapter decode reports unsupported format");
    require(result.image.empty(), "unsupported direct adapter decode does not create placeholder pixels");
    require(result.metadata.decoder_id == "fake_stb_decoder", "unsupported adapter failure records decoder id");
    require(
        result.diagnostic.find("does not support") != std::string::npos,
        "unsupported adapter failure records deterministic diagnostic");
    require(backend.decode_requests.empty(), "unsupported direct adapter decode does not call backend decode");
}

void test_third_party_adapter_header_stays_image_owned()
{
    const std::filesystem::path header_path = locate_source_file(
        "apps/quiz/quiz-vulkan/src/render/image/third_party_image_decoder_adapter.h",
        "src/render/image/third_party_image_decoder_adapter.h");
    require(!header_path.empty(), "third-party image decoder adapter header is discoverable");

    const std::string header = read_text_file(header_path);
    const std::string unix_host_prefix = std::string{"/mnt"} + "/c/aa";
    const std::string windows_drive_prefix = std::string{"C:"} + "\\";
    require(header.find(unix_host_prefix) == std::string::npos, "third-party adapter header has no host path");
    require(header.find(windows_drive_prefix) == std::string::npos, "third-party adapter header has no Windows host path");
    require(header.find("#include <stb") == std::string::npos, "third-party adapter header has no external include");

    const std::vector<std::string_view> forbidden_includes = {
        "#include \"app/",
        "#include \"asset/",
        "#include \"assets/",
        "#include \"audio/",
        "#include \"domain/",
        "#include \"input/",
        "#include \"platform/",
        "#include \"render/text/",
        "#include \"render/vulkan/",
        "#include \"renderer/",
        "#include \"scene/",
        "#include \"text/",
        "#include \"ui/",
    };

    for (const std::string_view forbidden_include : forbidden_includes) {
        require(
            header.find(forbidden_include) == std::string::npos,
            "third-party adapter header has no upper-layer include");
    }
}

} // namespace

int main()
{
    test_adapter_decodes_matching_format_and_sets_metadata();
    test_stb_dependency_selection_missing_falls_back();
    test_stb_dependency_selection_ready_for_jpeg();
    test_stb_dependency_selection_preserves_internal_decoders();
    test_stb_dependency_selection_mismatched_capability_falls_back();
    test_stb_dependency_selection_respects_supported_format_matrix();
    test_optional_chain_decodes_with_adapter_before_standard_candidates();
    test_adapter_failure_falls_back_to_standard_decoder_chain();
    test_unsupported_adapter_falls_back_to_standard_decoder_chain();
    test_unavailable_adapter_preserves_standard_unsupported_failure();
    test_adapter_unsupported_format_is_placeholder_safe();
    test_third_party_adapter_header_stays_image_owned();
    return 0;
}
