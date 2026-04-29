#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace quiz_vulkan::render {

constexpr std::uint32_t utf8_replacement_codepoint = 0xfffd;

struct utf8_text_codepoint {
    std::uint32_t code_point = utf8_replacement_codepoint;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 1;
    bool valid = false;
    bool cluster_start = true;
};

struct utf8_text_cluster {
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::size_t codepoint_offset = 0;
    std::size_t codepoint_count = 0;
    bool valid = true;
};

inline bool is_utf8_continuation_byte(const unsigned char byte)
{
    return (byte & 0xc0U) == 0x80U;
}

inline bool is_utf8_combining_mark(const std::uint32_t code_point)
{
    return (code_point >= 0x0300U && code_point <= 0x036fU)
        || (code_point >= 0x1ab0U && code_point <= 0x1affU)
        || (code_point >= 0x1dc0U && code_point <= 0x1dffU)
        || (code_point >= 0x20d0U && code_point <= 0x20ffU)
        || (code_point >= 0xfe20U && code_point <= 0xfe2fU);
}

inline bool is_utf8_hangul_syllable(const std::uint32_t code_point)
{
    return code_point >= 0xac00U && code_point <= 0xd7a3U;
}

inline utf8_text_codepoint decode_utf8_text_codepoint(
    const std::string_view text,
    const std::size_t byte_offset)
{
    if (byte_offset >= text.size()) {
        return utf8_text_codepoint{
            .byte_offset = text.size(),
            .byte_count = 0,
            .valid = false,
            .cluster_start = true,
        };
    }

    const auto byte = static_cast<unsigned char>(text[byte_offset]);
    if (byte < 0x80U) {
        return utf8_text_codepoint{
            .code_point = byte,
            .byte_offset = byte_offset,
            .byte_count = 1,
            .valid = true,
            .cluster_start = true,
        };
    }

    const std::size_t remaining = text.size() - byte_offset;
    if (byte >= 0xc2U && byte <= 0xdfU && remaining >= 2) {
        const auto second = static_cast<unsigned char>(text[byte_offset + 1]);
        if (is_utf8_continuation_byte(second)) {
            return utf8_text_codepoint{
                .code_point = static_cast<std::uint32_t>(((byte & 0x1fU) << 6U) | (second & 0x3fU)),
                .byte_offset = byte_offset,
                .byte_count = 2,
                .valid = true,
                .cluster_start = true,
            };
        }
    }

    if (byte >= 0xe0U && byte <= 0xefU && remaining >= 3) {
        const auto second = static_cast<unsigned char>(text[byte_offset + 1]);
        const auto third = static_cast<unsigned char>(text[byte_offset + 2]);
        const bool valid_second =
            (byte != 0xe0U || second >= 0xa0U)
            && (byte != 0xedU || second <= 0x9fU)
            && is_utf8_continuation_byte(second);
        if (valid_second && is_utf8_continuation_byte(third)) {
            return utf8_text_codepoint{
                .code_point = static_cast<std::uint32_t>(
                    ((byte & 0x0fU) << 12U) | ((second & 0x3fU) << 6U) | (third & 0x3fU)),
                .byte_offset = byte_offset,
                .byte_count = 3,
                .valid = true,
                .cluster_start = true,
            };
        }
    }

    if (byte >= 0xf0U && byte <= 0xf4U && remaining >= 4) {
        const auto second = static_cast<unsigned char>(text[byte_offset + 1]);
        const auto third = static_cast<unsigned char>(text[byte_offset + 2]);
        const auto fourth = static_cast<unsigned char>(text[byte_offset + 3]);
        const bool valid_second =
            (byte != 0xf0U || second >= 0x90U)
            && (byte != 0xf4U || second <= 0x8fU)
            && is_utf8_continuation_byte(second);
        if (valid_second && is_utf8_continuation_byte(third) && is_utf8_continuation_byte(fourth)) {
            return utf8_text_codepoint{
                .code_point = static_cast<std::uint32_t>(
                    ((byte & 0x07U) << 18U) | ((second & 0x3fU) << 12U)
                    | ((third & 0x3fU) << 6U) | (fourth & 0x3fU)),
                .byte_offset = byte_offset,
                .byte_count = 4,
                .valid = true,
                .cluster_start = true,
            };
        }
    }

    return utf8_text_codepoint{
        .byte_offset = byte_offset,
        .byte_count = 1,
        .valid = false,
        .cluster_start = true,
    };
}

inline bool starts_new_utf8_text_cluster(
    const std::vector<utf8_text_codepoint>& prior_codepoints,
    const utf8_text_codepoint& codepoint)
{
    if (prior_codepoints.empty()) {
        return true;
    }
    if (!codepoint.valid) {
        return true;
    }
    if (!prior_codepoints.back().valid) {
        return true;
    }
    if (is_utf8_combining_mark(codepoint.code_point)) {
        return false;
    }
    if (is_utf8_hangul_syllable(codepoint.code_point)) {
        return true;
    }
    return true;
}

inline std::vector<utf8_text_codepoint> iterate_utf8_text_run(const std::string_view text)
{
    std::vector<utf8_text_codepoint> codepoints;
    for (std::size_t byte_offset = 0; byte_offset < text.size();) {
        utf8_text_codepoint codepoint = decode_utf8_text_codepoint(text, byte_offset);
        codepoint.cluster_start = starts_new_utf8_text_cluster(codepoints, codepoint);
        codepoints.push_back(codepoint);
        byte_offset += codepoint.byte_count == 0 ? 1 : codepoint.byte_count;
    }
    return codepoints;
}

inline std::vector<utf8_text_cluster> cluster_utf8_text_run(
    const std::vector<utf8_text_codepoint>& codepoints)
{
    std::vector<utf8_text_cluster> clusters;
    for (std::size_t index = 0; index < codepoints.size(); ++index) {
        const utf8_text_codepoint& codepoint = codepoints[index];
        if (codepoint.cluster_start || clusters.empty()) {
            clusters.push_back(utf8_text_cluster{
                .byte_offset = codepoint.byte_offset,
                .byte_count = codepoint.byte_count,
                .codepoint_offset = index,
                .codepoint_count = 1,
                .valid = codepoint.valid,
            });
            continue;
        }

        utf8_text_cluster& cluster = clusters.back();
        cluster.byte_count = (codepoint.byte_offset + codepoint.byte_count) - cluster.byte_offset;
        ++cluster.codepoint_count;
        cluster.valid = cluster.valid && codepoint.valid;
    }
    return clusters;
}

inline std::vector<utf8_text_cluster> cluster_utf8_text_run(const std::string_view text)
{
    return cluster_utf8_text_run(iterate_utf8_text_run(text));
}

} // namespace quiz_vulkan::render
