#pragma once

#include "render/text/text_engine.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

using font_face_id = std::uint32_t;

struct font_codepoint_range {
    std::uint32_t first = 0;
    std::uint32_t last = 0;

    bool contains(std::uint32_t codepoint) const
    {
        return first <= codepoint && codepoint <= last;
    }
};

struct font_face_descriptor {
    font_face_id id = 0;
    std::string family;
    std::string source_uri;
    std::string version;
    std::string license;
    std::vector<font_codepoint_range> coverage;
    int weight = 400;
    bool italic = false;
    bool fallback = false;

    bool supports_codepoint(std::uint32_t codepoint) const
    {
        if (coverage.empty()) {
            return true;
        }

        return std::any_of(
            coverage.begin(),
            coverage.end(),
            [&](const font_codepoint_range& range) { return range.contains(codepoint); });
    }
};

struct font_face_resolution {
    const font_face_descriptor* requested_face = nullptr;
    const font_face_descriptor* resolved_face = nullptr;
    bool used_fallback = false;
    bool glyph_supported = false;
};

class font_face_catalog {
public:
    const font_face_descriptor& add_face(font_face_descriptor descriptor)
    {
        if (descriptor.id == 0) {
            descriptor.id = next_id_++;
        } else {
            next_id_ = std::max(next_id_, descriptor.id + 1);
        }

        faces_.push_back(std::move(descriptor));
        return faces_.back();
    }

    const font_face_descriptor* find_exact(
        const std::string& family,
        int weight,
        bool italic) const
    {
        const auto match = std::find_if(
            faces_.begin(),
            faces_.end(),
            [&](const font_face_descriptor& face) {
                return face.family == family && face.weight == weight && face.italic == italic;
            });
        return match == faces_.end() ? nullptr : &*match;
    }

    const font_face_descriptor* find_by_id(font_face_id id) const
    {
        const auto match = std::find_if(
            faces_.begin(),
            faces_.end(),
            [&](const font_face_descriptor& face) { return face.id == id; });
        return match == faces_.end() ? nullptr : &*match;
    }

    const font_face_descriptor* fallback_face() const
    {
        const auto explicit_fallback = std::find_if(
            faces_.begin(),
            faces_.end(),
            [](const font_face_descriptor& face) { return face.fallback; });
        if (explicit_fallback != faces_.end()) {
            return &*explicit_fallback;
        }
        return faces_.empty() ? nullptr : &faces_.front();
    }

    const font_face_descriptor* find_covering_fallback(std::uint32_t codepoint) const
    {
        const auto known_coverage_match = std::find_if(
            faces_.begin(),
            faces_.end(),
            [&](const font_face_descriptor& face) {
                return face.fallback && !face.coverage.empty() && face.supports_codepoint(codepoint);
            });
        if (known_coverage_match != faces_.end()) {
            return &*known_coverage_match;
        }

        const auto match = std::find_if(
            faces_.begin(),
            faces_.end(),
            [&](const font_face_descriptor& face) {
                return face.fallback && face.supports_codepoint(codepoint);
            });
        return match == faces_.end() ? nullptr : &*match;
    }

    const font_face_descriptor* find_covering_face(std::uint32_t codepoint) const
    {
        const auto known_coverage_match = std::find_if(
            faces_.begin(),
            faces_.end(),
            [&](const font_face_descriptor& face) {
                return !face.coverage.empty() && face.supports_codepoint(codepoint);
            });
        if (known_coverage_match != faces_.end()) {
            return &*known_coverage_match;
        }

        const auto match = std::find_if(
            faces_.begin(),
            faces_.end(),
            [&](const font_face_descriptor& face) { return face.supports_codepoint(codepoint); });
        return match == faces_.end() ? nullptr : &*match;
    }

    const font_face_descriptor* resolve(const render_text_style& style) const
    {
        const font_face_descriptor* exact = find_exact(style.font_family, style.font_weight, style.italic);
        return exact == nullptr ? fallback_face() : exact;
    }

    font_face_resolution resolve_for_codepoint(
        const render_text_style& style,
        std::uint32_t codepoint) const
    {
        const font_face_descriptor* requested = find_exact(style.font_family, style.font_weight, style.italic);
        if (requested != nullptr && requested->supports_codepoint(codepoint)) {
            return font_face_resolution{
                .requested_face = requested,
                .resolved_face = requested,
                .used_fallback = false,
                .glyph_supported = true,
            };
        }

        if (const font_face_descriptor* fallback = find_covering_fallback(codepoint);
            fallback != nullptr) {
            return font_face_resolution{
                .requested_face = requested,
                .resolved_face = fallback,
                .used_fallback = true,
                .glyph_supported = true,
            };
        }

        if (const font_face_descriptor* covering = find_covering_face(codepoint);
            covering != nullptr) {
            return font_face_resolution{
                .requested_face = requested,
                .resolved_face = covering,
                .used_fallback = requested == nullptr || covering->id != requested->id,
                .glyph_supported = true,
            };
        }

        const font_face_descriptor* unresolved = requested == nullptr ? fallback_face() : requested;
        return font_face_resolution{
            .requested_face = requested,
            .resolved_face = unresolved,
            .used_fallback = requested == nullptr && unresolved != nullptr,
            .glyph_supported = false,
        };
    }

    font_face_resolution resolve_for_face_and_codepoint(
        font_face_id requested_face_id,
        std::uint32_t codepoint) const
    {
        const font_face_descriptor* requested = find_by_id(requested_face_id);
        if (requested != nullptr && requested->supports_codepoint(codepoint)) {
            return font_face_resolution{
                .requested_face = requested,
                .resolved_face = requested,
                .used_fallback = false,
                .glyph_supported = true,
            };
        }

        if (const font_face_descriptor* fallback = find_covering_fallback(codepoint);
            fallback != nullptr) {
            return font_face_resolution{
                .requested_face = requested,
                .resolved_face = fallback,
                .used_fallback = requested == nullptr || fallback->id != requested->id,
                .glyph_supported = true,
            };
        }

        if (const font_face_descriptor* covering = find_covering_face(codepoint);
            covering != nullptr) {
            return font_face_resolution{
                .requested_face = requested,
                .resolved_face = covering,
                .used_fallback = requested == nullptr || covering->id != requested->id,
                .glyph_supported = true,
            };
        }

        const font_face_descriptor* unresolved = requested == nullptr ? fallback_face() : requested;
        return font_face_resolution{
            .requested_face = requested,
            .resolved_face = unresolved,
            .used_fallback = requested == nullptr && unresolved != nullptr,
            .glyph_supported = false,
        };
    }

    const std::vector<font_face_descriptor>& faces() const
    {
        return faces_;
    }

private:
    std::vector<font_face_descriptor> faces_;
    font_face_id next_id_ = 1;
};

struct glyph_atlas_key {
    font_face_id face_id = 0;
    std::uint32_t glyph_id = 0;
    std::uint32_t pixel_size = 0;

    friend bool operator==(const glyph_atlas_key&, const glyph_atlas_key&) = default;
};

struct glyph_atlas_page_config {
    std::size_t width = 256;
    std::size_t height = 256;
    std::size_t padding = 1;
};

struct glyph_atlas_slot {
    glyph_atlas_key key;
    render_text_atlas_page page;
    render_rect atlas_bounds;
    bool newly_allocated = false;
};

class glyph_atlas_cache {
public:
    explicit glyph_atlas_cache(glyph_atlas_page_config config = {})
        : config_(config)
    {
    }

    std::optional<glyph_atlas_slot> find(const glyph_atlas_key& key) const
    {
        const allocation_record* allocation = find_allocation(key);
        if (allocation == nullptr) {
            return std::nullopt;
        }

        return glyph_atlas_slot{
            .key = allocation->key,
            .page = pages_[allocation->page_index].page,
            .atlas_bounds = allocation->atlas_bounds,
            .newly_allocated = false,
        };
    }

    std::optional<glyph_atlas_slot> cache_glyph(
        glyph_atlas_key key,
        std::size_t width,
        std::size_t height)
    {
        if (std::optional<glyph_atlas_slot> existing = find(key); existing.has_value()) {
            return existing;
        }
        if (!fits_page(width, height)) {
            return std::nullopt;
        }

        for (std::size_t page_index = 0; page_index < pages_.size(); ++page_index) {
            if (std::optional<render_rect> bounds = allocate_on_page(pages_[page_index], width, height);
                bounds.has_value()) {
                return record_allocation(std::move(key), page_index, *bounds);
            }
        }

        page_state page;
        page.page = render_text_atlas_page{
            .id = static_cast<render_text_atlas_page_id>(pages_.size() + 1),
            .revision = 0,
            .width = config_.width,
            .height = config_.height,
        };
        pages_.push_back(page);

        std::optional<render_rect> bounds = allocate_on_page(pages_.back(), width, height);
        if (!bounds.has_value()) {
            pages_.pop_back();
            return std::nullopt;
        }
        return record_allocation(std::move(key), pages_.size() - 1, *bounds);
    }

    std::vector<render_text_atlas_page> pages() const
    {
        std::vector<render_text_atlas_page> output;
        output.reserve(pages_.size());
        for (const page_state& page : pages_) {
            output.push_back(page.page);
        }
        return output;
    }

    std::vector<render_text_atlas_update> consume_dirty_page_updates()
    {
        std::vector<render_text_atlas_update> updates;
        updates.reserve(pages_.size());
        for (page_state& page : pages_) {
            if (!page.dirty) {
                continue;
            }

            updates.push_back(render_text_atlas_update{
                .page = page.page,
                .updated_bounds = page.dirty_bounds,
                .rgba = make_deterministic_update_rgba(page.page, page.dirty_bounds),
            });
            page.dirty = false;
            page.dirty_bounds = {};
        }
        return updates;
    }

private:
    struct page_state {
        render_text_atlas_page page;
        std::size_t cursor_x = 0;
        std::size_t cursor_y = 0;
        std::size_t row_height = 0;
        bool dirty = false;
        render_rect dirty_bounds;
    };

    struct allocation_record {
        glyph_atlas_key key;
        std::size_t page_index = 0;
        render_rect atlas_bounds;
    };

    bool fits_page(std::size_t width, std::size_t height) const
    {
        return width + (config_.padding * 2) <= config_.width
            && height + (config_.padding * 2) <= config_.height;
    }

    const allocation_record* find_allocation(const glyph_atlas_key& key) const
    {
        const auto match = std::find_if(
            allocations_.begin(),
            allocations_.end(),
            [&](const allocation_record& allocation) { return allocation.key == key; });
        return match == allocations_.end() ? nullptr : &*match;
    }

    std::optional<render_rect> allocate_on_page(page_state& page, std::size_t width, std::size_t height) const
    {
        const std::size_t padded_width = width + (config_.padding * 2);
        const std::size_t padded_height = height + (config_.padding * 2);

        if (page.cursor_x + padded_width > page.page.width) {
            page.cursor_x = 0;
            page.cursor_y += page.row_height;
            page.row_height = 0;
        }
        if (page.cursor_y + padded_height > page.page.height) {
            return std::nullopt;
        }

        const render_rect bounds{
            static_cast<float>(page.cursor_x + config_.padding),
            static_cast<float>(page.cursor_y + config_.padding),
            static_cast<float>(width),
            static_cast<float>(height),
        };
        page.cursor_x += padded_width;
        page.row_height = std::max(page.row_height, padded_height);
        ++page.page.revision;
        mark_dirty(page, bounds);
        return bounds;
    }

    static void mark_dirty(page_state& page, render_rect bounds)
    {
        if (!page.dirty) {
            page.dirty = true;
            page.dirty_bounds = bounds;
            return;
        }

        const float left = std::min(page.dirty_bounds.x, bounds.x);
        const float top = std::min(page.dirty_bounds.y, bounds.y);
        const float right = std::max(page.dirty_bounds.x + page.dirty_bounds.width, bounds.x + bounds.width);
        const float bottom = std::max(page.dirty_bounds.y + page.dirty_bounds.height, bounds.y + bounds.height);
        page.dirty_bounds = render_rect{
            left,
            top,
            right - left,
            bottom - top,
        };
    }

    static std::vector<unsigned char> make_deterministic_update_rgba(
        const render_text_atlas_page& page,
        const render_rect& bounds)
    {
        const std::size_t width = static_cast<std::size_t>(bounds.width);
        const std::size_t height = static_cast<std::size_t>(bounds.height);
        const std::uint32_t origin_x = static_cast<std::uint32_t>(bounds.x);
        const std::uint32_t origin_y = static_cast<std::uint32_t>(bounds.y);
        const std::uint32_t revision = static_cast<std::uint32_t>(page.revision & 0xffU);

        std::vector<unsigned char> rgba(width * height * 4U);
        for (std::size_t y = 0; y < height; ++y) {
            for (std::size_t x = 0; x < width; ++x) {
                const std::uint32_t atlas_x = origin_x + static_cast<std::uint32_t>(x);
                const std::uint32_t atlas_y = origin_y + static_cast<std::uint32_t>(y);
                const std::uint32_t seed = (page.id * 17U) + (revision * 31U);
                const std::size_t offset = ((y * width) + x) * 4U;
                rgba[offset] = static_cast<unsigned char>((seed + atlas_x + (atlas_y * 3U)) & 0xffU);
                rgba[offset + 1U] = static_cast<unsigned char>((revision + atlas_x) & 0xffU);
                rgba[offset + 2U] = static_cast<unsigned char>((page.id + atlas_y) & 0xffU);
                rgba[offset + 3U] = 255U;
            }
        }
        return rgba;
    }

    glyph_atlas_slot record_allocation(glyph_atlas_key key, std::size_t page_index, render_rect bounds)
    {
        allocations_.push_back(allocation_record{
            .key = key,
            .page_index = page_index,
            .atlas_bounds = bounds,
        });
        return glyph_atlas_slot{
            .key = key,
            .page = pages_[page_index].page,
            .atlas_bounds = bounds,
            .newly_allocated = true,
        };
    }

    glyph_atlas_page_config config_;
    std::vector<page_state> pages_;
    std::vector<allocation_record> allocations_;
};

} // namespace quiz_vulkan::render
