#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::scene {

using scene_node_id = std::string;
using scene_screen_id = std::string;
using scene_route_id = std::string;
using scene_style_id = std::string;

struct scene_size {
    float width = 0.0f;
    float height = 0.0f;
};

struct scene_point {
    float x = 0.0f;
    float y = 0.0f;
};

struct scene_edges {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    float horizontal() const
    {
        return left + right;
    }

    float vertical() const
    {
        return top + bottom;
    }
};

struct scene_rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    float right() const
    {
        return x + width;
    }

    float bottom() const
    {
        return y + height;
    }

    bool contains(scene_point point) const
    {
        return point.x >= x && point.x <= right() && point.y >= y && point.y <= bottom();
    }

    scene_rect inset(scene_edges edges) const
    {
        const float inset_width = std::max(0.0f, width - edges.horizontal());
        const float inset_height = std::max(0.0f, height - edges.vertical());
        return scene_rect{x + edges.left, y + edges.top, inset_width, inset_height};
    }

    scene_rect clipped_to(scene_rect clip) const
    {
        const float clipped_x = std::max(x, clip.x);
        const float clipped_y = std::max(y, clip.y);
        const float clipped_right = std::min(right(), clip.right());
        const float clipped_bottom = std::min(bottom(), clip.bottom());
        return scene_rect{
            clipped_x,
            clipped_y,
            std::max(0.0f, clipped_right - clipped_x),
            std::max(0.0f, clipped_bottom - clipped_y)};
    }
};

struct scene_keyboard_state {
    bool visible = false;
    float bottom_inset = 0.0f;
    scene_node_id focused_node_id;
};

struct scene_layout_environment {
    scene_rect viewport;
    scene_edges safe_area;
    scene_keyboard_state keyboard;

    scene_rect safe_bounds() const
    {
        return viewport.inset(safe_area);
    }

    scene_rect keyboard_safe_bounds() const
    {
        scene_rect bounds = safe_bounds();
        if (keyboard.visible && keyboard.bottom_inset > 0.0f) {
            const float keyboard_top = viewport.bottom() - keyboard.bottom_inset;
            if (bounds.bottom() > keyboard_top) {
                bounds.height = std::max(0.0f, keyboard_top - bounds.y);
            }
        }
        return bounds;
    }
};

enum class scene_node_kind {
    container,
    text,
    image,
    input,
    spacer,
};

enum class scene_layout_mode {
    overlay,
    vertical,
    horizontal,
};

enum class scene_alignment {
    start,
    center,
    end,
    stretch,
};

struct scene_layout_rule {
    scene_layout_mode mode = scene_layout_mode::overlay;
    bool has_x = false;
    bool has_y = false;
    bool has_width = false;
    bool has_height = false;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    scene_edges margin;
    scene_edges padding;
    float gap = 0.0f;
    float flex_grow = 0.0f;
    scene_alignment horizontal_alignment = scene_alignment::stretch;
    scene_alignment vertical_alignment = scene_alignment::start;
    bool clip_children = false;
    bool respect_safe_area = false;
    bool avoid_keyboard = false;
    bool anchor_to_keyboard = false;
};

struct scene_style {
    scene_style_id token;
    std::string background_color;
    std::string foreground_color;
    float opacity = 1.0f;
    float border_radius = 0.0f;
};

struct scene_text_run {
    std::string text;
    scene_style_id style_token;
};

struct scene_image_ref {
    std::string uri;
    std::string alt_text;
    float aspect_ratio = 0.0f;
};

enum class scene_action_trigger {
    press,
    focus,
    change,
};

struct scene_action_binding {
    scene_action_trigger trigger = scene_action_trigger::press;
    std::string action_type;
    std::string payload;

    bool empty() const
    {
        return action_type.empty() && payload.empty();
    }
};

enum class scene_node_role {
    generic,
    app_shell,
    quiz_question_stage,
    quiz_question_header,
    quiz_question_prompt,
    quiz_question_body,
    quiz_question_image,
    quiz_option_group,
    quiz_option,
    quiz_feedback,
    quiz_answer_input,
    quiz_answer_dock,
    quiz_controls,
};

enum class scene_quiz_stage {
    none,
    question,
    feedback,
    completed,
};

enum class scene_quiz_feedback_state {
    none,
    correct,
    incorrect,
    skipped,
    marked_unknown,
};

enum class scene_quiz_option_state {
    idle,
    selected,
    correct,
    incorrect,
    disabled,
};

enum class scene_question_length_class {
    unspecified,
    short_question,
    long_question,
};

struct scene_quiz_semantics {
    static constexpr std::size_t no_option_index = std::numeric_limits<std::size_t>::max();

    scene_quiz_stage stage = scene_quiz_stage::none;
    scene_quiz_feedback_state feedback = scene_quiz_feedback_state::none;
    scene_quiz_option_state option_state = scene_quiz_option_state::idle;
    scene_question_length_class question_length = scene_question_length_class::unspecified;
    std::size_t option_index = no_option_index;
    bool reveal_correctness = false;
    bool accepts_keyboard_input = false;
};

struct scene_node_semantics {
    scene_node_role role = scene_node_role::generic;
    std::string label;
    scene_quiz_semantics quiz;
};

inline const char* to_string(scene_node_role role)
{
    switch (role) {
        case scene_node_role::generic:
            return "generic";
        case scene_node_role::app_shell:
            return "app_shell";
        case scene_node_role::quiz_question_stage:
            return "quiz_question_stage";
        case scene_node_role::quiz_question_header:
            return "quiz_question_header";
        case scene_node_role::quiz_question_prompt:
            return "quiz_question_prompt";
        case scene_node_role::quiz_question_body:
            return "quiz_question_body";
        case scene_node_role::quiz_question_image:
            return "quiz_question_image";
        case scene_node_role::quiz_option_group:
            return "quiz_option_group";
        case scene_node_role::quiz_option:
            return "quiz_option";
        case scene_node_role::quiz_feedback:
            return "quiz_feedback";
        case scene_node_role::quiz_answer_input:
            return "quiz_answer_input";
        case scene_node_role::quiz_answer_dock:
            return "quiz_answer_dock";
        case scene_node_role::quiz_controls:
            return "quiz_controls";
    }

    return "generic";
}

inline const char* to_string(scene_quiz_stage stage)
{
    switch (stage) {
        case scene_quiz_stage::none:
            return "none";
        case scene_quiz_stage::question:
            return "question";
        case scene_quiz_stage::feedback:
            return "feedback";
        case scene_quiz_stage::completed:
            return "completed";
    }

    return "none";
}

inline const char* to_string(scene_quiz_feedback_state feedback)
{
    switch (feedback) {
        case scene_quiz_feedback_state::none:
            return "none";
        case scene_quiz_feedback_state::correct:
            return "correct";
        case scene_quiz_feedback_state::incorrect:
            return "incorrect";
        case scene_quiz_feedback_state::skipped:
            return "skipped";
        case scene_quiz_feedback_state::marked_unknown:
            return "marked_unknown";
    }

    return "none";
}

inline const char* to_string(scene_quiz_option_state state)
{
    switch (state) {
        case scene_quiz_option_state::idle:
            return "idle";
        case scene_quiz_option_state::selected:
            return "selected";
        case scene_quiz_option_state::correct:
            return "correct";
        case scene_quiz_option_state::incorrect:
            return "incorrect";
        case scene_quiz_option_state::disabled:
            return "disabled";
    }

    return "idle";
}

inline const char* to_string(scene_question_length_class length_class)
{
    switch (length_class) {
        case scene_question_length_class::unspecified:
            return "unspecified";
        case scene_question_length_class::short_question:
            return "short";
        case scene_question_length_class::long_question:
            return "long";
    }

    return "unspecified";
}

inline scene_question_length_class classify_question_length(
    const std::string& prompt,
    const std::string& body = std::string(),
    std::size_t long_threshold = 120)
{
    return prompt.size() + body.size() >= long_threshold
        ? scene_question_length_class::long_question
        : scene_question_length_class::short_question;
}

struct scene_input_region {
    scene_node_id node_id;
    scene_rect bounds;
    scene_action_binding action;
    bool enabled = true;
    scene_node_semantics semantics;
};

struct scene_animation_state {
    bool active = false;
    std::string name;
    float elapsed_seconds = 0.0f;
    float duration_seconds = 0.0f;
};

struct scene_route_state {
    scene_route_id route_id;
    scene_screen_id screen_id;
    std::map<std::string, std::string> metadata;
};

struct scene_node_data {
    scene_node_id id;
    scene_node_id parent_id;
    scene_node_kind kind = scene_node_kind::container;
    std::string debug_name;
    scene_layout_rule layout_rule;
    scene_style style;
    std::vector<scene_text_run> text_runs;
    scene_image_ref image;
    bool has_image = false;
    scene_action_binding action_binding;
    bool has_action_binding = false;
    scene_node_semantics semantics;
    std::vector<scene_node_id> children;
    bool visible = true;
    bool input_enabled = true;
};

class scene_layout_data {
public:
    static constexpr std::size_t append_to_end = std::numeric_limits<std::size_t>::max();

    explicit scene_layout_data(scene_screen_id root_screen_id = "root")
        : root_screen_id_(std::move(root_screen_id))
    {
        scene_node_data root;
        root.id = root_node_id_;
        root.kind = scene_node_kind::container;
        root.debug_name = "root";
        nodes_.emplace(root_node_id_, std::move(root));

        route_state_.route_id = root_screen_id_;
        route_state_.screen_id = root_screen_id_;
    }

    const scene_node_id& root_node_id() const
    {
        return root_node_id_;
    }

    const scene_screen_id& root_screen_id() const
    {
        return root_screen_id_;
    }

    const scene_route_state& route_state() const
    {
        return route_state_;
    }

    const scene_animation_state& animation_state() const
    {
        return animation_state_;
    }

    bool has_focus() const
    {
        return has_focus_;
    }

    const scene_node_id& focus_id() const
    {
        return focus_id_;
    }

    const std::map<scene_node_id, scene_node_data>& nodes() const
    {
        return nodes_;
    }

    const std::map<scene_style_id, scene_style>& style_tokens() const
    {
        return style_tokens_;
    }

    bool contains_node(const scene_node_id& id) const
    {
        return nodes_.find(id) != nodes_.end();
    }

    const scene_node_data* find_node(const scene_node_id& id) const
    {
        const auto found = nodes_.find(id);
        return found == nodes_.end() ? nullptr : &found->second;
    }

    scene_node_data* find_node(const scene_node_id& id)
    {
        const auto found = nodes_.find(id);
        return found == nodes_.end() ? nullptr : &found->second;
    }

    const scene_node_data& root_node() const
    {
        return nodes_.at(root_node_id_);
    }

    scene_node_data& root_node()
    {
        return nodes_.at(root_node_id_);
    }

    const std::vector<scene_node_id>& root_children() const
    {
        return root_node().children;
    }

    bool append_node(
        const scene_node_id& parent_id,
        scene_node_data node,
        std::string* error = nullptr,
        std::size_t child_index = append_to_end)
    {
        const scene_node_id resolved_parent_id = parent_id.empty() ? root_node_id_ : parent_id;
        auto parent = nodes_.find(resolved_parent_id);
        if (parent == nodes_.end()) {
            set_error(error, "parent node does not exist: " + resolved_parent_id);
            return false;
        }

        if (node.id.empty()) {
            set_error(error, "node id is required");
            return false;
        }

        if (node.id == root_node_id_) {
            set_error(error, "cannot append a second root node");
            return false;
        }

        if (contains_node(node.id)) {
            set_error(error, "node already exists: " + node.id);
            return false;
        }

        node.parent_id = resolved_parent_id;
        node.children.clear();
        const scene_node_id node_id = node.id;
        nodes_.emplace(node_id, std::move(node));

        auto& children = parent->second.children;
        if (child_index == append_to_end || child_index >= children.size()) {
            children.push_back(node_id);
        } else {
            children.insert(children.begin() + static_cast<std::ptrdiff_t>(child_index), node_id);
        }

        return true;
    }

    bool remove_node(const scene_node_id& id, std::string* error = nullptr)
    {
        if (id == root_node_id_) {
            set_error(error, "cannot remove root node");
            return false;
        }

        const auto found = nodes_.find(id);
        if (found == nodes_.end()) {
            set_error(error, "node does not exist: " + id);
            return false;
        }

        auto parent = nodes_.find(found->second.parent_id);
        if (parent != nodes_.end()) {
            auto& siblings = parent->second.children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), id), siblings.end());
        }

        std::vector<scene_node_id> subtree_ids;
        collect_subtree_ids(id, subtree_ids);
        for (const auto& subtree_id : subtree_ids) {
            nodes_.erase(subtree_id);
        }

        if (has_focus_ && !contains_node(focus_id_)) {
            focus_id_.clear();
            has_focus_ = false;
        }

        return true;
    }

    bool set_text(const scene_node_id& id, std::vector<scene_text_run> text_runs, std::string* error = nullptr)
    {
        auto* node = find_node(id);
        if (node == nullptr) {
            set_error(error, "node does not exist: " + id);
            return false;
        }

        node->text_runs = std::move(text_runs);
        return true;
    }

    bool set_style(const scene_node_id& id, scene_style style, std::string* error = nullptr)
    {
        auto* node = find_node(id);
        if (node == nullptr) {
            set_error(error, "node does not exist: " + id);
            return false;
        }

        node->style = std::move(style);
        if (!node->style.token.empty()) {
            style_tokens_[node->style.token] = node->style;
        }
        return true;
    }

    bool set_bounds_rule(const scene_node_id& id, scene_layout_rule bounds_rule, std::string* error = nullptr)
    {
        auto* node = find_node(id);
        if (node == nullptr) {
            set_error(error, "node does not exist: " + id);
            return false;
        }

        node->layout_rule = bounds_rule;
        return true;
    }

    bool set_image(const scene_node_id& id, scene_image_ref image, std::string* error = nullptr)
    {
        auto* node = find_node(id);
        if (node == nullptr) {
            set_error(error, "node does not exist: " + id);
            return false;
        }

        node->image = std::move(image);
        node->has_image = true;
        return true;
    }

    bool bind_action(const scene_node_id& id, scene_action_binding action, std::string* error = nullptr)
    {
        auto* node = find_node(id);
        if (node == nullptr) {
            set_error(error, "node does not exist: " + id);
            return false;
        }

        node->action_binding = std::move(action);
        node->has_action_binding = !node->action_binding.empty();
        return true;
    }

    bool set_semantics(const scene_node_id& id, scene_node_semantics semantics, std::string* error = nullptr)
    {
        auto* node = find_node(id);
        if (node == nullptr) {
            set_error(error, "node does not exist: " + id);
            return false;
        }

        node->semantics = std::move(semantics);
        return true;
    }

    bool set_focus(scene_node_id id, std::string* error = nullptr)
    {
        if (id.empty()) {
            focus_id_.clear();
            has_focus_ = false;
            return true;
        }

        if (!contains_node(id)) {
            set_error(error, "focus node does not exist: " + id);
            return false;
        }

        focus_id_ = std::move(id);
        has_focus_ = true;
        return true;
    }

    bool set_route(scene_route_state route_state)
    {
        route_state_ = std::move(route_state);
        if (!route_state_.screen_id.empty()) {
            root_screen_id_ = route_state_.screen_id;
        }
        return true;
    }

    bool start_transition(scene_animation_state animation_state)
    {
        animation_state_ = std::move(animation_state);
        animation_state_.active = true;
        return true;
    }

    void set_style_token(scene_style_id id, scene_style style)
    {
        style.token = id;
        style_tokens_[std::move(id)] = std::move(style);
    }

private:
    static void set_error(std::string* error, std::string message)
    {
        if (error != nullptr) {
            *error = std::move(message);
        }
    }

    void collect_subtree_ids(const scene_node_id& id, std::vector<scene_node_id>& subtree_ids) const
    {
        const auto found = nodes_.find(id);
        if (found == nodes_.end()) {
            return;
        }

        subtree_ids.push_back(id);
        for (const auto& child_id : found->second.children) {
            collect_subtree_ids(child_id, subtree_ids);
        }
    }

    scene_screen_id root_screen_id_;
    scene_node_id root_node_id_ = "root";
    std::map<scene_node_id, scene_node_data> nodes_;
    std::map<scene_style_id, scene_style> style_tokens_;
    scene_node_id focus_id_;
    bool has_focus_ = false;
    scene_animation_state animation_state_;
    scene_route_state route_state_;
};

} // namespace quiz_vulkan::scene
