#include "core/input/normalized_input_replay_diff_policy.h"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    std::cerr << "normalized_input_replay_diff_policy_tests failed: " << message << '\n';
    std::exit(1);
}

void require_range(
    quiz_vulkan::input::text_range range,
    std::size_t start_byte,
    std::size_t end_byte,
    const char* message)
{
    require(range.start_byte == start_byte, message);
    require(range.end_byte == end_byte, message);
}

void test_count_bool_range_and_string_deltas_are_stable()
{
    using namespace quiz_vulkan::input;

    const normalized_input_replay_count_delta growth =
        normalized_input_replay_diff_count(2, 5);
    require(growth.before_count == 2, "count delta records before count");
    require(growth.after_count == 5, "count delta records after count");
    require(growth.delta == 3, "count delta records positive delta");
    require(growth.changed, "count delta marks positive change");

    const normalized_input_replay_count_delta shrink =
        normalized_input_replay_diff_count(7, 4);
    require(shrink.delta == -3, "count delta records negative delta");
    require(shrink.changed, "count delta marks negative change");

    const normalized_input_replay_count_delta stable =
        normalized_input_replay_diff_count(6, 6);
    require(stable.delta == 0, "count delta records zero delta");
    require(!stable.changed, "count delta marks stable counts unchanged");

    const normalized_input_replay_bool_delta bool_delta =
        normalized_input_replay_diff_bool(false, true);
    require(!bool_delta.before_value, "bool delta records before value");
    require(bool_delta.after_value, "bool delta records after value");
    require(bool_delta.changed, "bool delta marks changed values");

    const text_range before_range{
        .start_byte = 1,
        .end_byte = 3,
    };
    const text_range same_range{
        .start_byte = 1,
        .end_byte = 3,
    };
    const text_range after_range{
        .start_byte = 2,
        .end_byte = 4,
    };
    require(normalized_input_replay_same_text_range(before_range, same_range),
        "range helper treats equal byte ranges as same");
    require(!normalized_input_replay_same_text_range(before_range, after_range),
        "range helper treats shifted byte ranges as changed");

    const normalized_input_replay_range_delta range_delta =
        normalized_input_replay_diff_range(before_range, after_range);
    require_range(range_delta.before_range, 1, 3, "range delta records before range");
    require_range(range_delta.after_range, 2, 4, "range delta records after range");
    require(range_delta.changed, "range delta marks changed range");

    normalized_input_replay_string_delta string_delta =
        normalized_input_replay_diff_string(std::string{"abc"}, std::string{"abcdef"});
    require(string_delta.before_value == "abc", "string delta records before string");
    require(string_delta.after_value == "abcdef", "string delta records after string");
    require(string_delta.byte_delta == 3, "string delta records byte growth");
    require(string_delta.changed, "string delta marks changed strings");

    string_delta = normalized_input_replay_diff_string(std::string{"same"}, std::string{"same"});
    require(string_delta.byte_delta == 0, "string delta records stable byte size");
    require(!string_delta.changed, "string delta marks equal strings unchanged");
}

void test_pointer_capture_delta_tracks_lifecycle_and_cleanliness()
{
    using namespace quiz_vulkan::input;

    const pointer_capture_snapshot idle_capture;
    const pointer_capture_snapshot captured_pointer{
        .lifecycle = pointer_capture_lifecycle::captured,
        .active = true,
        .pointer_id = 42,
        .tracked_pointer_count = 1,
    };
    const pointer_capture_snapshot tracking_pointer{
        .lifecycle = pointer_capture_lifecycle::tracking,
        .active = false,
        .pointer_id = 42,
        .tracked_pointer_count = 1,
    };

    require(pointer_capture_snapshot_clean(idle_capture),
        "pointer capture helper treats default idle capture as clean");
    require(!pointer_capture_snapshot_clean(captured_pointer),
        "pointer capture helper treats active capture as unclean");
    require(normalized_input_replay_pointer_capture_changed(idle_capture, captured_pointer),
        "pointer capture helper marks changed capture fields");
    require(normalized_input_replay_pointer_capture_changed(captured_pointer, tracking_pointer),
        "pointer capture helper marks lifecycle changes");

    const normalized_input_replay_pointer_capture_delta capture_delta =
        normalized_input_replay_diff_pointer_capture(idle_capture, captured_pointer);
    require(capture_delta.before_capture.lifecycle == pointer_capture_lifecycle::idle,
        "pointer capture delta records before lifecycle");
    require(capture_delta.after_capture.lifecycle == pointer_capture_lifecycle::captured,
        "pointer capture delta records after lifecycle");
    require(capture_delta.after_capture.pointer_id == 42,
        "pointer capture delta records after pointer id");
    require(capture_delta.before_clean, "pointer capture delta records clean before state");
    require(!capture_delta.after_clean, "pointer capture delta records unclean after state");
    require(capture_delta.changed, "pointer capture delta marks changed capture");
    require(capture_delta.clean_changed, "pointer capture delta marks cleanliness change");

    const normalized_input_replay_pointer_capture_delta stable_delta =
        normalized_input_replay_diff_pointer_capture(idle_capture, idle_capture);
    require(!stable_delta.changed, "pointer capture delta marks stable capture unchanged");
    require(!stable_delta.clean_changed, "pointer capture delta marks stable clean state unchanged");
}

} // namespace

int main()
{
    test_count_bool_range_and_string_deltas_are_stable();
    test_pointer_capture_delta_tracks_lifecycle_and_cleanliness();

    std::cout << "normalized_input_replay_diff_policy_tests passed\n";
    return 0;
}
