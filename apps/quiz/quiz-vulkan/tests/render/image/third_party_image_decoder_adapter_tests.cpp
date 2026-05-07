#include "render/image/third_party_image_decoder_adapter.h"

#include <cassert>
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
    test_optional_chain_decodes_with_adapter_before_standard_candidates();
    test_adapter_failure_falls_back_to_standard_decoder_chain();
    test_unavailable_adapter_preserves_standard_unsupported_failure();
    test_adapter_unsupported_format_is_placeholder_safe();
    test_third_party_adapter_header_stays_image_owned();
    return 0;
}
