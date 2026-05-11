#ifndef QUIZ_VULKAN_RENDER_TEXT_FONT_GLYPH_ATLAS_PAGE_PLAN_DECLARED
#if !defined(QUIZ_VULKAN_RENDER_TEXT_FONT_GLYPH_ATLAS_PAGE_PLAN_HAVE_MATERIALIZATION)
#include "render/text/font_shaped_atlas_update.h"
#endif
#endif

#if defined(QUIZ_VULKAN_RENDER_TEXT_FONT_GLYPH_ATLAS_PAGE_PLAN_HAVE_MATERIALIZATION) \
    && !defined(QUIZ_VULKAN_RENDER_TEXT_FONT_GLYPH_ATLAS_PAGE_PLAN_DECLARED)
#define QUIZ_VULKAN_RENDER_TEXT_FONT_GLYPH_ATLAS_PAGE_PLAN_DECLARED

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_glyph_atlas_page_plan_status {
    reused_existing_placement,
    selected_existing_page,
    allocated_new_page,
    skipped_materialization,
    overflow,
};

inline std::string render_text_glyph_atlas_page_plan_status_name(
    const render_text_glyph_atlas_page_plan_status status)
{
    switch (status) {
    case render_text_glyph_atlas_page_plan_status::reused_existing_placement:
        return "reused_existing_placement";
    case render_text_glyph_atlas_page_plan_status::selected_existing_page:
        return "selected_existing_page";
    case render_text_glyph_atlas_page_plan_status::allocated_new_page:
        return "allocated_new_page";
    case render_text_glyph_atlas_page_plan_status::skipped_materialization:
        return "skipped_materialization";
    case render_text_glyph_atlas_page_plan_status::overflow:
        return "overflow";
    }

    return "unknown";
}

struct render_text_glyph_atlas_page_plan_constraints {
    std::size_t width = 256;
    std::size_t height = 256;
    std::size_t padding = 1;
    std::size_t max_pages = 0;

    bool has_page_extent() const
    {
        return width > 0 && height > 0;
    }
};

struct render_text_glyph_atlas_page_plan_entry_snapshot {
    std::size_t materialization_index = 0;
    std::string materialization_id;
    std::size_t run_index = 0;
    std::size_t cluster_index = 0;
    std::size_t cluster_byte_offset = 0;
    std::size_t cluster_byte_count = 0;
    glyph_atlas_key cache_key;
    bool has_cache_key = false;
    render_text_glyph_atlas_materialization_status materialization_status =
        render_text_glyph_atlas_materialization_status::skipped_missing_cache_key;
    render_text_glyph_atlas_page_plan_status status =
        render_text_glyph_atlas_page_plan_status::skipped_materialization;
    render_text_atlas_page page;
    render_rect atlas_bounds;
    bool has_atlas_bounds = false;
    std::size_t page_index = 0;
    std::size_t glyph_width = 0;
    std::size_t glyph_height = 0;
    std::size_t padded_width = 0;
    std::size_t padded_height = 0;
    std::size_t placed_area = 0;
    std::size_t page_capacity = 0;
    std::size_t page_used_area_before = 0;
    std::size_t page_used_area_after = 0;
    float estimated_occupancy_before = 0.0f;
    float estimated_occupancy_after = 0.0f;
    float estimated_fragmentation_before = 0.0f;
    float estimated_fragmentation_after = 0.0f;
    std::size_t upload_rgba_bytes = 0;
    bool selected_existing_page = false;
    bool allocated_new_page = false;
    bool reused_existing_placement = false;
    bool skipped = true;
    bool overflow = false;
    bool upload_ready = false;
    bool clean_reuse = false;
    glyph_atlas_key eviction_candidate_key;
    bool has_eviction_candidate = false;
    std::string diagnostic;
};

struct render_text_glyph_atlas_page_plan_page_snapshot {
    render_text_atlas_page page;
    bool allocated_by_plan = false;
    bool referenced_by_pending_update = false;
    std::size_t materialization_count = 0;
    std::size_t upload_ready_count = 0;
    std::size_t clean_reuse_count = 0;
    std::size_t reused_placement_count = 0;
    std::size_t pending_update_count = 0;
    std::size_t page_capacity = 0;
    std::size_t used_area = 0;
    std::size_t available_area = 0;
    std::size_t upload_rgba_bytes = 0;
    std::size_t pending_update_rgba_bytes = 0;
    float estimated_occupancy = 0.0f;
    float estimated_fragmentation = 0.0f;
    bool overflow = false;
    glyph_atlas_key eviction_candidate_key;
    bool has_eviction_candidate = false;
};

struct render_text_glyph_atlas_page_plan_policy_snapshot {
    std::size_t materialization_count = 0;
    std::size_t planned_entry_count = 0;
    std::size_t skipped_count = 0;
    std::size_t overflow_count = 0;
    std::size_t allocated_new_page_count = 0;
    std::size_t selected_existing_page_count = 0;
    std::size_t reused_placement_count = 0;
    std::size_t pending_update_count = 0;
    std::size_t page_count = 0;
    std::size_t eviction_candidate_count = 0;
    std::size_t materialization_upload_rgba_bytes = 0;
    std::size_t pending_update_rgba_bytes = 0;
    std::size_t total_upload_rgba_bytes = 0;
    std::size_t total_page_capacity = 0;
    std::size_t total_used_area = 0;
    std::size_t total_available_area = 0;
    float estimated_occupancy = 0.0f;
    float estimated_fragmentation = 0.0f;
    bool has_overflow = false;
    bool has_eviction_candidates = false;
    std::string diagnostic;
};

struct render_text_glyph_atlas_page_plan_request {
    std::vector<render_text_glyph_atlas_materialization_snapshot> materializations;
    std::vector<render_text_atlas_update> pending_updates;
    render_text_glyph_atlas_page_plan_constraints constraints;
};

struct render_text_glyph_atlas_page_plan_snapshot {
    std::vector<render_text_glyph_atlas_page_plan_entry_snapshot> entries;
    std::vector<render_text_glyph_atlas_page_plan_page_snapshot> pages;
    render_text_glyph_atlas_page_plan_policy_snapshot policy;

    bool ok() const
    {
        return !policy.has_overflow;
    }

    bool has_overflow() const
    {
        return policy.has_overflow;
    }

    bool has_eviction_candidates() const
    {
        return policy.has_eviction_candidates;
    }
};

inline render_text_glyph_atlas_page_plan_constraints render_text_glyph_atlas_page_plan_constraints_for(
    const glyph_atlas_page_config& config,
    const std::size_t max_pages = 0)
{
    return render_text_glyph_atlas_page_plan_constraints{
        .width = config.width,
        .height = config.height,
        .padding = config.padding,
        .max_pages = max_pages,
    };
}

inline std::size_t render_text_glyph_atlas_page_plan_rect_dimension_for(
    const float value,
    const std::size_t fallback)
{
    if (value <= 0.0f) {
        return fallback;
    }

    const std::size_t truncated = static_cast<std::size_t>(value);
    return static_cast<float>(truncated) == value ? truncated : truncated + 1U;
}

inline std::size_t render_text_glyph_atlas_page_plan_rect_area(const render_rect& rect)
{
    const std::size_t width = render_text_glyph_atlas_page_plan_rect_dimension_for(rect.width, 0);
    const std::size_t height = render_text_glyph_atlas_page_plan_rect_dimension_for(rect.height, 0);
    return width * height;
}

inline float render_text_glyph_atlas_page_plan_ratio(
    const std::size_t numerator,
    const std::size_t denominator)
{
    if (denominator == 0U) {
        return 0.0f;
    }
    return static_cast<float>(numerator) / static_cast<float>(denominator);
}

inline std::size_t render_text_glyph_atlas_page_plan_pending_update_bytes(
    const render_text_atlas_update& update)
{
    return update.rgba.size();
}

inline bool render_text_glyph_atlas_page_plan_key_exists(
    const std::vector<glyph_atlas_key>& keys,
    const glyph_atlas_key& key)
{
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

inline std::size_t render_text_glyph_atlas_page_plan_width_for(
    const render_text_glyph_atlas_materialization_snapshot& materialization)
{
    if (materialization.has_atlas_placement && materialization.atlas_bounds.width > 0.0f) {
        return render_text_glyph_atlas_page_plan_rect_dimension_for(materialization.atlas_bounds.width, 1);
    }
    if (materialization.has_atlas_update && materialization.atlas_update_bounds.width > 0.0f) {
        return render_text_glyph_atlas_page_plan_rect_dimension_for(materialization.atlas_update_bounds.width, 1);
    }
    if (materialization.has_layout_bounds && materialization.layout_bounds.width > 0.0f) {
        return render_text_glyph_atlas_page_plan_rect_dimension_for(materialization.layout_bounds.width, 1);
    }
    return 1U;
}

inline std::size_t render_text_glyph_atlas_page_plan_height_for(
    const render_text_glyph_atlas_materialization_snapshot& materialization)
{
    if (materialization.has_atlas_placement && materialization.atlas_bounds.height > 0.0f) {
        return render_text_glyph_atlas_page_plan_rect_dimension_for(materialization.atlas_bounds.height, 1);
    }
    if (materialization.has_atlas_update && materialization.atlas_update_bounds.height > 0.0f) {
        return render_text_glyph_atlas_page_plan_rect_dimension_for(materialization.atlas_update_bounds.height, 1);
    }
    if (materialization.has_layout_bounds && materialization.layout_bounds.height > 0.0f) {
        return render_text_glyph_atlas_page_plan_rect_dimension_for(materialization.layout_bounds.height, 1);
    }
    return 1U;
}

inline render_text_glyph_atlas_page_plan_snapshot plan_render_text_glyph_atlas_pages(
    render_text_glyph_atlas_page_plan_request request)
{
    struct working_page {
        render_text_glyph_atlas_page_plan_page_snapshot snapshot;
        std::size_t cursor_x = 0;
        std::size_t cursor_y = 0;
        std::size_t row_height = 0;
        std::vector<glyph_atlas_key> keys;
    };

    auto page_capacity_for = [](const render_text_atlas_page& page) {
        return page.width * page.height;
    };
    auto finish_page_metrics = [&](render_text_glyph_atlas_page_plan_page_snapshot& page) {
        page.page_capacity = page_capacity_for(page.page);
        page.available_area = page.used_area >= page.page_capacity ? 0U : page.page_capacity - page.used_area;
        page.estimated_occupancy =
            render_text_glyph_atlas_page_plan_ratio(page.used_area, page.page_capacity);
        page.estimated_fragmentation =
            page.page_capacity == 0U ? 0.0f : 1.0f - page.estimated_occupancy;
    };

    render_text_glyph_atlas_page_plan_snapshot plan;
    plan.policy.materialization_count = request.materializations.size();
    plan.policy.pending_update_count = request.pending_updates.size();
    plan.entries.reserve(request.materializations.size());

    std::vector<working_page> pages;
    auto find_page = [&](const render_text_atlas_page_id page_id) -> working_page* {
        const auto match = std::find_if(
            pages.begin(),
            pages.end(),
            [&](const working_page& page) { return page.snapshot.page.id == page_id; });
        return match == pages.end() ? nullptr : &*match;
    };
    auto append_page = [&](render_text_atlas_page page, const bool allocated_by_plan) -> working_page& {
        if (page.id == 0U) {
            page.id = static_cast<render_text_atlas_page_id>(pages.size() + 1U);
        }
        if (page.width == 0U) {
            page.width = request.constraints.width;
        }
        if (page.height == 0U) {
            page.height = request.constraints.height;
        }
        pages.push_back(working_page{
            .snapshot = render_text_glyph_atlas_page_plan_page_snapshot{
                .page = page,
                .allocated_by_plan = allocated_by_plan,
                .page_capacity = page.width * page.height,
            },
        });
        return pages.back();
    };
    auto eviction_candidate_from_pages = [&]() {
        for (const working_page& page : pages) {
            if (!page.keys.empty()) {
                return page.keys.front();
            }
        }
        return glyph_atlas_key{};
    };
    auto has_eviction_candidate = [&]() {
        return std::any_of(
            pages.begin(),
            pages.end(),
            [](const working_page& page) { return !page.keys.empty(); });
    };
    auto mark_overflow_page = [&]() {
        if (!pages.empty()) {
            pages.front().snapshot.overflow = true;
        }
    };
    auto allocate_on_page = [&](working_page& page, const std::size_t width, const std::size_t height) {
        const std::size_t padded_width = width + (request.constraints.padding * 2U);
        const std::size_t padded_height = height + (request.constraints.padding * 2U);
        if (page.cursor_x + padded_width > page.snapshot.page.width) {
            page.cursor_x = 0;
            page.cursor_y += page.row_height;
            page.row_height = 0;
        }
        if (page.cursor_y + padded_height > page.snapshot.page.height) {
            return render_rect{};
        }

        const render_rect bounds{
            static_cast<float>(page.cursor_x + request.constraints.padding),
            static_cast<float>(page.cursor_y + request.constraints.padding),
            static_cast<float>(width),
            static_cast<float>(height),
        };
        page.cursor_x += padded_width;
        page.row_height = std::max(page.row_height, padded_height);
        return bounds;
    };

    for (const render_text_atlas_update& update : request.pending_updates) {
        plan.policy.pending_update_rgba_bytes += render_text_glyph_atlas_page_plan_pending_update_bytes(update);
        if (update.page.id == 0U) {
            continue;
        }

        working_page* page = find_page(update.page.id);
        if (page == nullptr) {
            page = &append_page(update.page, false);
        }
        page->snapshot.referenced_by_pending_update = true;
        ++page->snapshot.pending_update_count;
        page->snapshot.pending_update_rgba_bytes +=
            render_text_glyph_atlas_page_plan_pending_update_bytes(update);
    }

    for (std::size_t index = 0; index < request.materializations.size(); ++index) {
        const render_text_glyph_atlas_materialization_snapshot& materialization =
            request.materializations[index];
        render_text_glyph_atlas_page_plan_entry_snapshot entry{
            .materialization_index = index,
            .materialization_id = render_text_glyph_atlas_materialization_stable_id_for(materialization),
            .run_index = materialization.run_index,
            .cluster_index = materialization.cluster_index,
            .cluster_byte_offset = materialization.cluster_byte_offset,
            .cluster_byte_count = materialization.cluster_byte_count,
            .cache_key = materialization.cache_key,
            .has_cache_key = materialization.has_cache_key,
            .materialization_status = materialization.status,
            .glyph_width = render_text_glyph_atlas_page_plan_width_for(materialization),
            .glyph_height = render_text_glyph_atlas_page_plan_height_for(materialization),
            .upload_rgba_bytes = materialization.queued ? materialization.atlas_update_rgba_bytes : 0U,
            .upload_ready = materialization.queued,
            .clean_reuse = materialization.clean_reuse,
        };
        entry.padded_width = entry.glyph_width + (request.constraints.padding * 2U);
        entry.padded_height = entry.glyph_height + (request.constraints.padding * 2U);
        entry.placed_area = entry.glyph_width * entry.glyph_height;
        plan.policy.materialization_upload_rgba_bytes += entry.upload_rgba_bytes;

        if (render_text_glyph_atlas_materialization_is_skipped(materialization)
            || !materialization.materialized
            || !materialization.has_cache_key) {
            entry.status = render_text_glyph_atlas_page_plan_status::skipped_materialization;
            entry.skipped = true;
            entry.diagnostic = "atlas page planning skipped non-materialized glyph work";
            ++plan.policy.skipped_count;
            plan.entries.push_back(std::move(entry));
            continue;
        }

        const bool too_large_for_page =
            !request.constraints.has_page_extent()
            || entry.padded_width > request.constraints.width
            || entry.padded_height > request.constraints.height;
        if (too_large_for_page) {
            entry.status = render_text_glyph_atlas_page_plan_status::overflow;
            entry.skipped = false;
            entry.overflow = true;
            entry.has_eviction_candidate = has_eviction_candidate();
            entry.eviction_candidate_key = eviction_candidate_from_pages();
            entry.diagnostic = "atlas page planning overflowed because glyph dimensions exceed page constraints";
            mark_overflow_page();
            ++plan.policy.overflow_count;
            plan.policy.has_overflow = true;
            if (entry.has_eviction_candidate) {
                ++plan.policy.eviction_candidate_count;
                plan.policy.has_eviction_candidates = true;
            }
            plan.entries.push_back(std::move(entry));
            continue;
        }

        working_page* selected_page = nullptr;
        bool allocated_new_page = false;
        if (materialization.page.id != 0U) {
            selected_page = find_page(materialization.page.id);
            if (selected_page == nullptr) {
                if (request.constraints.max_pages == 0U || pages.size() < request.constraints.max_pages) {
                    selected_page = &append_page(materialization.page, false);
                    allocated_new_page = true;
                }
            }
        } else {
            for (working_page& page : pages) {
                const render_rect bounds = allocate_on_page(page, entry.glyph_width, entry.glyph_height);
                if (bounds.width > 0.0f && bounds.height > 0.0f) {
                    selected_page = &page;
                    entry.atlas_bounds = bounds;
                    entry.has_atlas_bounds = true;
                    break;
                }
            }
            if (selected_page == nullptr
                && (request.constraints.max_pages == 0U || pages.size() < request.constraints.max_pages)) {
                selected_page = &append_page(render_text_atlas_page{}, true);
                allocated_new_page = true;
                entry.atlas_bounds = allocate_on_page(
                    *selected_page,
                    entry.glyph_width,
                    entry.glyph_height);
                entry.has_atlas_bounds = entry.atlas_bounds.width > 0.0f && entry.atlas_bounds.height > 0.0f;
            }
        }

        if (selected_page == nullptr) {
            entry.status = render_text_glyph_atlas_page_plan_status::overflow;
            entry.skipped = false;
            entry.overflow = true;
            entry.has_eviction_candidate = has_eviction_candidate();
            entry.eviction_candidate_key = eviction_candidate_from_pages();
            entry.diagnostic = "atlas page planning overflowed because no page capacity was available";
            mark_overflow_page();
            ++plan.policy.overflow_count;
            plan.policy.has_overflow = true;
            if (entry.has_eviction_candidate) {
                ++plan.policy.eviction_candidate_count;
                plan.policy.has_eviction_candidates = true;
            }
            plan.entries.push_back(std::move(entry));
            continue;
        }

        if (!entry.has_atlas_bounds && materialization.has_atlas_placement) {
            entry.atlas_bounds = materialization.atlas_bounds;
            entry.has_atlas_bounds = true;
        }
        entry.page = selected_page->snapshot.page;
        entry.page_index = static_cast<std::size_t>(selected_page - pages.data());
        entry.page_capacity = page_capacity_for(selected_page->snapshot.page);
        entry.page_used_area_before = selected_page->snapshot.used_area;
        entry.estimated_occupancy_before = render_text_glyph_atlas_page_plan_ratio(
            entry.page_used_area_before,
            entry.page_capacity);
        entry.estimated_fragmentation_before =
            entry.page_capacity == 0U ? 0.0f : 1.0f - entry.estimated_occupancy_before;
        const bool key_already_known =
            render_text_glyph_atlas_page_plan_key_exists(selected_page->keys, materialization.cache_key);
        entry.reused_existing_placement = materialization.clean_reuse || key_already_known;
        entry.allocated_new_page = allocated_new_page;
        entry.selected_existing_page = !entry.allocated_new_page && !entry.reused_existing_placement;
        entry.skipped = false;
        if (entry.reused_existing_placement) {
            entry.status = render_text_glyph_atlas_page_plan_status::reused_existing_placement;
            ++plan.policy.reused_placement_count;
            ++selected_page->snapshot.reused_placement_count;
            entry.diagnostic = "atlas page planning reused an existing glyph placement";
        } else if (entry.allocated_new_page) {
            entry.status = render_text_glyph_atlas_page_plan_status::allocated_new_page;
            ++plan.policy.allocated_new_page_count;
            selected_page->snapshot.allocated_by_plan = true;
            entry.diagnostic = "atlas page planning selected a newly allocated page";
        } else {
            entry.status = render_text_glyph_atlas_page_plan_status::selected_existing_page;
            ++plan.policy.selected_existing_page_count;
            entry.diagnostic = "atlas page planning selected an existing page";
        }

        if (!key_already_known) {
            selected_page->snapshot.used_area += entry.placed_area;
            selected_page->keys.push_back(materialization.cache_key);
        }
        entry.page_used_area_after = selected_page->snapshot.used_area;
        entry.estimated_occupancy_after = render_text_glyph_atlas_page_plan_ratio(
            entry.page_used_area_after,
            entry.page_capacity);
        entry.estimated_fragmentation_after =
            entry.page_capacity == 0U ? 0.0f : 1.0f - entry.estimated_occupancy_after;
        ++selected_page->snapshot.materialization_count;
        if (entry.upload_ready) {
            ++selected_page->snapshot.upload_ready_count;
            selected_page->snapshot.upload_rgba_bytes += entry.upload_rgba_bytes;
        }
        if (entry.clean_reuse) {
            ++selected_page->snapshot.clean_reuse_count;
        }
        selected_page->snapshot.has_eviction_candidate = !selected_page->keys.empty();
        selected_page->snapshot.eviction_candidate_key =
            selected_page->keys.empty() ? glyph_atlas_key{} : selected_page->keys.front();
        plan.entries.push_back(std::move(entry));
    }

    plan.pages.reserve(pages.size());
    for (working_page& page : pages) {
        finish_page_metrics(page.snapshot);
        plan.policy.total_page_capacity += page.snapshot.page_capacity;
        plan.policy.total_used_area += page.snapshot.used_area;
        plan.policy.total_available_area += page.snapshot.available_area;
        plan.policy.has_eviction_candidates =
            plan.policy.has_eviction_candidates || page.snapshot.has_eviction_candidate;
        plan.pages.push_back(std::move(page.snapshot));
    }
    plan.policy.page_count = plan.pages.size();
    plan.policy.planned_entry_count = plan.entries.size();
    plan.policy.total_upload_rgba_bytes =
        plan.policy.materialization_upload_rgba_bytes + plan.policy.pending_update_rgba_bytes;
    plan.policy.estimated_occupancy = render_text_glyph_atlas_page_plan_ratio(
        plan.policy.total_used_area,
        plan.policy.total_page_capacity);
    plan.policy.estimated_fragmentation =
        plan.policy.total_page_capacity == 0U ? 0.0f : 1.0f - plan.policy.estimated_occupancy;
    plan.policy.diagnostic = "glyph atlas page plan: "
        + std::to_string(plan.policy.page_count)
        + " pages, "
        + std::to_string(plan.policy.overflow_count)
        + " overflows, "
        + std::to_string(plan.policy.total_upload_rgba_bytes)
        + " upload bytes";
    return plan;
}

inline render_text_glyph_atlas_page_plan_snapshot plan_render_text_glyph_atlas_pages(
    std::vector<render_text_glyph_atlas_materialization_snapshot> materializations,
    const glyph_atlas_page_config config = {},
    const std::size_t max_pages = 0)
{
    return plan_render_text_glyph_atlas_pages(render_text_glyph_atlas_page_plan_request{
        .materializations = std::move(materializations),
        .constraints = render_text_glyph_atlas_page_plan_constraints_for(config, max_pages),
    });
}

} // namespace quiz_vulkan::render

#endif // QUIZ_VULKAN_RENDER_TEXT_FONT_GLYPH_ATLAS_PAGE_PLAN_DECLARED
