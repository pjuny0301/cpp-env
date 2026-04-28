// Aggregate compile-time interface lock. Individual contract fragments live near
// the owning subsystem; this file is the single target humans and agents build.

#include "../audio/audio_engine_interface_contract_tests.cpp"
#include "../app/app_render_pipeline_interface_contract_tests.cpp"
#include "../input/input_engine_interface_contract_tests.cpp"
#include "../persistence/learning_state_store_interface_contract_tests.cpp"
#include "../render/image/image_engine_interface_contract_tests.cpp"
#include "../render/text/text_engine_interface_contract_tests.cpp"
#include "../scene/scene_modifier_interface_contract_tests.cpp"
#include "../ui/ui_renderer_interface_contract_tests.cpp"
