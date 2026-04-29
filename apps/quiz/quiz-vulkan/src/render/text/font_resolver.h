#pragma once

#include "render/text/font_glyph_atlas.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>

namespace quiz_vulkan::render {

struct font_resolver_request {
    std::string family;
    int weight = 400;
    bool italic = false;
};

struct font_resolver_result {
    font_resolver_request requested;
    font_face_id exact_face_id = 0;
    font_face_id resolved_face_id = 0;
    std::string resolved_family;
    int resolved_weight = 400;
    bool resolved_italic = false;
    bool used_family_fallback = false;
    bool used_style_fallback = false;

    bool used_fallback() const
    {
        return used_family_fallback || used_style_fallback;
    }
};

class deterministic_fake_font_resolver {
public:
    deterministic_fake_font_resolver()
    {
        add_face(font_face_descriptor{
            .family = "Sans",
            .source_uri = "fixture://fonts/sans-regular",
            .version = "fixture-1",
            .license = "test-fixture",
            .weight = 400,
            .italic = false,
            .fallback = true,
        });
        add_face(font_face_descriptor{
            .family = "Sans",
            .source_uri = "fixture://fonts/sans-bold",
            .version = "fixture-1",
            .license = "test-fixture",
            .weight = 700,
            .italic = false,
            .fallback = false,
        });
        add_face(font_face_descriptor{
            .family = "Serif",
            .source_uri = "fixture://fonts/serif-semibold-italic",
            .version = "fixture-1",
            .license = "test-fixture",
            .weight = 600,
            .italic = true,
            .fallback = false,
        });
    }

    const font_face_descriptor& add_face(font_face_descriptor descriptor)
    {
        return catalog_.add_face(std::move(descriptor));
    }

    font_resolver_result resolve(const render_text_style& style) const
    {
        return resolve(font_resolver_request{
            .family = style.font_family,
            .weight = style.font_weight,
            .italic = style.italic,
        });
    }

    font_resolver_result resolve(const font_resolver_request& request) const
    {
        if (const font_face_descriptor* exact = catalog_.find_exact(request.family, request.weight, request.italic);
            exact != nullptr) {
            return make_result(request, exact, exact, false, false);
        }

        if (const font_face_descriptor* family_face = find_best_family_face(request);
            family_face != nullptr) {
            return make_result(request, nullptr, family_face, false, true);
        }

        const font_face_descriptor* fallback = catalog_.fallback_face();
        return make_result(request, nullptr, fallback, fallback != nullptr, false);
    }

    const font_face_catalog& catalog() const
    {
        return catalog_;
    }

private:
    static int weight_distance(const int left, const int right)
    {
        return std::abs(left - right);
    }

    const font_face_descriptor* find_best_family_face(const font_resolver_request& request) const
    {
        const font_face_descriptor* best = nullptr;
        for (const font_face_descriptor& face : catalog_.faces()) {
            if (face.family != request.family) {
                continue;
            }
            if (best == nullptr) {
                best = &face;
                continue;
            }

            const bool face_italic_matches = face.italic == request.italic;
            const bool best_italic_matches = best->italic == request.italic;
            if (face_italic_matches != best_italic_matches) {
                if (face_italic_matches) {
                    best = &face;
                }
                continue;
            }

            const int face_distance = weight_distance(face.weight, request.weight);
            const int best_distance = weight_distance(best->weight, request.weight);
            if (face_distance < best_distance || (face_distance == best_distance && face.weight > best->weight)) {
                best = &face;
            }
        }
        return best;
    }

    static font_resolver_result make_result(
        const font_resolver_request& request,
        const font_face_descriptor* exact,
        const font_face_descriptor* resolved,
        const bool used_family_fallback,
        const bool used_style_fallback)
    {
        return font_resolver_result{
            .requested = request,
            .exact_face_id = exact == nullptr ? 0 : exact->id,
            .resolved_face_id = resolved == nullptr ? 0 : resolved->id,
            .resolved_family = resolved == nullptr ? std::string{} : resolved->family,
            .resolved_weight = resolved == nullptr ? request.weight : resolved->weight,
            .resolved_italic = resolved == nullptr ? request.italic : resolved->italic,
            .used_family_fallback = used_family_fallback,
            .used_style_fallback = used_style_fallback,
        };
    }

    font_face_catalog catalog_;
};

} // namespace quiz_vulkan::render
