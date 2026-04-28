cmake_minimum_required(VERSION 3.25)

project(
    quiz_vulkan
    VERSION 0.1.0
    LANGUAGES CXX
)

function(quiz_vulkan_copy_mingw_runtime_dlls target_name)
    if(NOT MINGW)
        return()
    endif()

    get_filename_component(mingw_runtime_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(runtime_dll_paths)
    foreach(runtime_dll IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        set(runtime_dll_path "${mingw_runtime_dir}/${runtime_dll}")
        if(EXISTS "${runtime_dll_path}")
            list(APPEND runtime_dll_paths "${runtime_dll_path}")
        endif()
    endforeach()
    if(runtime_dll_paths)
        file(COPY ${runtime_dll_paths} DESTINATION "${CMAKE_BINARY_DIR}")
    endif()
endfunction()

add_library(quiz_vulkan_platform_shell STATIC)
target_sources(
    quiz_vulkan_platform_shell
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/platform/platform_input_event.h
            src/platform/platform_shell.h
)
target_compile_features(quiz_vulkan_platform_shell PUBLIC cxx_std_23)
target_include_directories(quiz_vulkan_platform_shell PUBLIC src)

if(WIN32)
    target_sources(quiz_vulkan_platform_shell PRIVATE src/platform/windows_platform_shell.cpp)
    target_compile_definitions(quiz_vulkan_platform_shell PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(quiz_vulkan_platform_shell PRIVATE user32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(quiz_vulkan_platform_shell PRIVATE src/platform/linux_platform_shell.cpp)
else()
    target_sources(quiz_vulkan_platform_shell PRIVATE src/platform/null_platform_shell.cpp)
endif()

add_library(quiz_vulkan_domain STATIC)
target_sources(
    quiz_vulkan_domain
    PRIVATE
        src/core/domain/app_action.cpp
        src/core/domain/app_snapshot.cpp
        src/core/domain/deck_artifact_loader.cpp
        src/core/domain/learning_state.cpp
        src/core/domain/quiz_model.cpp
        src/core/domain/quiz_session.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/core/domain/app_action.hpp
            src/core/domain/app_snapshot.hpp
            src/core/domain/deck_artifact_loader.hpp
            src/core/domain/learning_state.hpp
            src/core/domain/quiz_model.hpp
            src/core/domain/quiz_session.hpp
)
target_compile_features(quiz_vulkan_domain PUBLIC cxx_std_23)
target_include_directories(quiz_vulkan_domain PUBLIC src)

add_library(quiz_vulkan_asset_contract INTERFACE)
target_sources(
    quiz_vulkan_asset_contract
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/assets/asset_resolver.h
)
target_compile_features(quiz_vulkan_asset_contract INTERFACE cxx_std_23)
target_include_directories(quiz_vulkan_asset_contract INTERFACE src)

add_library(quiz_vulkan_scene INTERFACE)
target_sources(
    quiz_vulkan_scene
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/core/scene/modifier_interface.h
            src/core/scene/scene_layout_data.h
            src/core/scene/scene_layout_edit_data.h
            src/core/scene/scene_layout_patch.h
            src/core/scene/scene_modifier.h
            src/core/layout/input_hit_test.h
            src/core/layout/layout_placer.h
)
target_compile_features(quiz_vulkan_scene INTERFACE cxx_std_23)
target_include_directories(quiz_vulkan_scene INTERFACE src)

add_library(quiz_vulkan_render_contract INTERFACE)
target_sources(
    quiz_vulkan_render_contract
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/render/image/image_decoder.h
            src/render/image/image_resolver.h
            src/render/image/image_texture_cache.h
            src/render/image/image_types.h
            src/render/render_draw_list.h
            src/render/text/text_engine.h
)
target_compile_features(quiz_vulkan_render_contract INTERFACE cxx_std_23)
target_include_directories(quiz_vulkan_render_contract INTERFACE src)

add_library(quiz_vulkan_audio_contract INTERFACE)
target_sources(
    quiz_vulkan_audio_contract
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/audio/audio_engine.h
            src/audio/sound_definition.h
            src/audio/sound_event.h
)
target_compile_features(quiz_vulkan_audio_contract INTERFACE cxx_std_23)
target_include_directories(quiz_vulkan_audio_contract INTERFACE src)

add_library(quiz_vulkan_input OBJECT)
target_sources(
    quiz_vulkan_input
    PRIVATE
        src/core/input/gesture_recognizer.cpp
        src/core/input/input_engine.cpp
        src/core/input/text_input_model.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/core/input/gesture_recognizer.h
            src/core/input/input_engine.h
            src/core/input/input_event.h
            src/core/input/text_input_model.h
)
target_compile_features(quiz_vulkan_input PUBLIC cxx_std_23)
target_include_directories(quiz_vulkan_input PUBLIC src)

add_library(quiz_vulkan_persistence_contract INTERFACE)
target_sources(
    quiz_vulkan_persistence_contract
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/core/persistence/learning_state_store.h
)
target_compile_features(quiz_vulkan_persistence_contract INTERFACE cxx_std_23)
target_include_directories(quiz_vulkan_persistence_contract INTERFACE src)
target_link_libraries(quiz_vulkan_persistence_contract INTERFACE quiz_vulkan_domain)

add_library(quiz_vulkan_external_worker_contract INTERFACE)
target_sources(
    quiz_vulkan_external_worker_contract
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/core/workers/external_worker_artifact.h
)
target_compile_features(quiz_vulkan_external_worker_contract INTERFACE cxx_std_23)
target_include_directories(quiz_vulkan_external_worker_contract INTERFACE src)

add_library(quiz_vulkan_text_scene_bridge INTERFACE)
target_sources(
    quiz_vulkan_text_scene_bridge
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/render/text/scene_text_metrics_adapter.h
)
target_compile_features(quiz_vulkan_text_scene_bridge INTERFACE cxx_std_23)
target_include_directories(quiz_vulkan_text_scene_bridge INTERFACE src)

add_library(quiz_vulkan_text_engine STATIC)
target_sources(
    quiz_vulkan_text_engine
    PRIVATE
        src/render/text/fake_text_engine.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/render/text/fake_text_engine.h
)
target_compile_features(quiz_vulkan_text_engine PUBLIC cxx_std_23)
target_include_directories(quiz_vulkan_text_engine PUBLIC src)
target_link_libraries(quiz_vulkan_text_engine PUBLIC quiz_vulkan_render_contract)
target_link_libraries(quiz_vulkan_text_scene_bridge INTERFACE quiz_vulkan_scene quiz_vulkan_render_contract)

add_library(quiz_vulkan_ui_renderer STATIC)
target_sources(
    quiz_vulkan_ui_renderer
    PRIVATE
        src/core/ui/ui_renderer.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/core/ui/ui_renderer.h
)
target_compile_features(quiz_vulkan_ui_renderer PUBLIC cxx_std_23)
target_include_directories(quiz_vulkan_ui_renderer PUBLIC src)
target_link_libraries(quiz_vulkan_ui_renderer PUBLIC quiz_vulkan_scene quiz_vulkan_render_contract)

add_library(quiz_vulkan_quiz_screens INTERFACE)
target_sources(
    quiz_vulkan_quiz_screens
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/core/ui/quiz_screens.h
            src/core/ui/quiz_screens.hpp
)
target_compile_features(quiz_vulkan_quiz_screens INTERFACE cxx_std_23)
target_include_directories(quiz_vulkan_quiz_screens INTERFACE src)
target_link_libraries(quiz_vulkan_quiz_screens INTERFACE quiz_vulkan_domain quiz_vulkan_scene)

add_library(quiz_vulkan_ui INTERFACE)
target_link_libraries(quiz_vulkan_ui INTERFACE quiz_vulkan_ui_renderer quiz_vulkan_quiz_screens)

add_library(quiz_vulkan_renderer STATIC)
target_sources(
    quiz_vulkan_renderer
    PRIVATE
        src/render/vulkan/vulkan_frame_plan.cpp
        src/render/vulkan/vulkan_frame_plan.h
        src/render/vulkan/vulkan_renderer.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/render/vulkan/vulkan_renderer.h
)
target_compile_features(quiz_vulkan_renderer PUBLIC cxx_std_23)
target_include_directories(quiz_vulkan_renderer PUBLIC src)
target_link_libraries(quiz_vulkan_renderer PUBLIC quiz_vulkan_render_contract)

add_executable(quiz_vulkan)
target_sources(
    quiz_vulkan
    PRIVATE
        src/app/app_action_router.cpp
        src/app/app.cpp
        src/app/app_demo.cpp
        src/app/app_state.cpp
        src/app/main.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/app/app_action_router.h
            src/app/app.h
            src/app/app_demo.h
            src/app/app_render_pipeline.h
            src/app/app_state.h
)
target_compile_features(quiz_vulkan PRIVATE cxx_std_23)
target_link_libraries(quiz_vulkan PRIVATE quiz_vulkan_platform_shell quiz_vulkan_renderer quiz_vulkan_ui quiz_vulkan_domain)
quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan)

if(MSVC)
    target_compile_options(quiz_vulkan_platform_shell PRIVATE /W4 /permissive-)
    target_compile_options(quiz_vulkan_domain PRIVATE /W4 /permissive-)
    target_compile_options(quiz_vulkan_input PRIVATE /W4 /permissive-)
    target_compile_options(quiz_vulkan_ui_renderer PRIVATE /W4 /permissive-)
    target_compile_options(quiz_vulkan_renderer PRIVATE /W4 /permissive-)
    target_compile_options(quiz_vulkan PRIVATE /W4 /permissive-)
else()
    target_compile_options(quiz_vulkan_platform_shell PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quiz_vulkan_domain PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quiz_vulkan_input PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quiz_vulkan_ui_renderer PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quiz_vulkan_renderer PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quiz_vulkan PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(quiz_vulkan_domain_smoke_test tests/domain/domain_smoke_test.cpp)
    target_link_libraries(quiz_vulkan_domain_smoke_test PRIVATE quiz_vulkan_domain)
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_domain_smoke_test)
    add_test(NAME quiz_vulkan_domain_smoke_test COMMAND quiz_vulkan_domain_smoke_test)

    add_executable(quiz_vulkan_deck_artifact_loader_tests tests/domain/deck_artifact_loader_tests.cpp)
    target_link_libraries(quiz_vulkan_deck_artifact_loader_tests PRIVATE quiz_vulkan_domain)
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_deck_artifact_loader_tests)
    add_test(NAME quiz_vulkan_deck_artifact_loader_tests COMMAND quiz_vulkan_deck_artifact_loader_tests)

    add_executable(quiz_vulkan_app_state_tests tests/app/app_state_tests.cpp src/app/app_state.cpp)
    target_link_libraries(quiz_vulkan_app_state_tests PRIVATE quiz_vulkan_domain)
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_app_state_tests)
    add_test(NAME quiz_vulkan_app_state_tests COMMAND quiz_vulkan_app_state_tests)

    add_executable(quiz_vulkan_app_action_router_tests tests/app/app_action_router_tests.cpp src/app/app_action_router.cpp)
    target_link_libraries(quiz_vulkan_app_action_router_tests PRIVATE quiz_vulkan_domain quiz_vulkan_scene)
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_app_action_router_tests)
    add_test(NAME quiz_vulkan_app_action_router_tests COMMAND quiz_vulkan_app_action_router_tests)

    add_executable(quiz_vulkan_app_demo_tests tests/app/app_demo_tests.cpp src/app/app_demo.cpp src/app/app_state.cpp)
    target_link_libraries(quiz_vulkan_app_demo_tests PRIVATE quiz_vulkan_renderer quiz_vulkan_ui quiz_vulkan_domain)
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_app_demo_tests)
    add_test(NAME quiz_vulkan_app_demo_tests COMMAND quiz_vulkan_app_demo_tests)

    add_executable(quiz_vulkan_scene_layout_data_tests tests/scene/scene_layout_data_tests.cpp)
    target_link_libraries(quiz_vulkan_scene_layout_data_tests PRIVATE quiz_vulkan_scene)
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_scene_layout_data_tests)
    add_test(NAME quiz_vulkan_scene_layout_data_tests COMMAND quiz_vulkan_scene_layout_data_tests)

    add_executable(quiz_vulkan_layout_placer_tests tests/layout/layout_placer_tests.cpp)
    target_link_libraries(quiz_vulkan_layout_placer_tests PRIVATE quiz_vulkan_scene)
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_layout_placer_tests)
    add_test(NAME quiz_vulkan_layout_placer_tests COMMAND quiz_vulkan_layout_placer_tests)

    add_executable(quiz_vulkan_input_hit_test_tests tests/layout/input_hit_test_tests.cpp)
    target_link_libraries(quiz_vulkan_input_hit_test_tests PRIVATE quiz_vulkan_scene)
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_input_hit_test_tests)
    add_test(NAME quiz_vulkan_input_hit_test_tests COMMAND quiz_vulkan_input_hit_test_tests)

    add_executable(quiz_vulkan_gesture_recognizer_tests tests/input/gesture_recognizer_tests.cpp)
    target_link_libraries(quiz_vulkan_gesture_recognizer_tests PRIVATE quiz_vulkan_input)
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_gesture_recognizer_tests)
    add_test(NAME quiz_vulkan_gesture_recognizer_tests COMMAND quiz_vulkan_gesture_recognizer_tests)

    add_executable(quiz_vulkan_text_input_model_tests tests/input/text_input_model_tests.cpp)
    target_link_libraries(quiz_vulkan_text_input_model_tests PRIVATE quiz_vulkan_input)
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_text_input_model_tests)
    add_test(NAME quiz_vulkan_text_input_model_tests COMMAND quiz_vulkan_text_input_model_tests)

    add_executable(quiz_vulkan_input_engine_tests tests/input/input_engine_tests.cpp)
    target_link_libraries(quiz_vulkan_input_engine_tests PRIVATE quiz_vulkan_input)
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_input_engine_tests)
    add_test(NAME quiz_vulkan_input_engine_tests COMMAND quiz_vulkan_input_engine_tests)

    add_executable(quiz_vulkan_quiz_screens_tests tests/ui/quiz_screens_tests.cpp)
    target_link_libraries(quiz_vulkan_quiz_screens_tests PRIVATE quiz_vulkan_ui)
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_quiz_screens_tests)
    add_test(NAME quiz_vulkan_quiz_screens_tests COMMAND quiz_vulkan_quiz_screens_tests)

    add_executable(quiz_vulkan_renderer_tests tests/render/vulkan_renderer_tests.cpp)
    target_link_libraries(quiz_vulkan_renderer_tests PRIVATE quiz_vulkan_renderer)
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_renderer_tests)
    add_test(NAME quiz_vulkan_renderer_tests COMMAND quiz_vulkan_renderer_tests)

    add_executable(quiz_vulkan_fake_text_engine_tests tests/render/fake_text_engine_tests.cpp)
    target_link_libraries(
        quiz_vulkan_fake_text_engine_tests
        PRIVATE
            quiz_vulkan_render_contract
            quiz_vulkan_scene
            quiz_vulkan_text_engine
            quiz_vulkan_text_scene_bridge
    )
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_fake_text_engine_tests)
    add_test(NAME quiz_vulkan_fake_text_engine_tests COMMAND quiz_vulkan_fake_text_engine_tests)

    add_executable(quiz_vulkan_fake_image_texture_cache_tests tests/render/fake_image_texture_cache_tests.cpp)
    target_link_libraries(
        quiz_vulkan_fake_image_texture_cache_tests
        PRIVATE
            quiz_vulkan_render_contract
    )
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_fake_image_texture_cache_tests)
    add_test(NAME quiz_vulkan_fake_image_texture_cache_tests COMMAND quiz_vulkan_fake_image_texture_cache_tests)

    add_executable(quiz_vulkan_fake_audio_engine_tests tests/audio/fake_audio_engine_tests.cpp)
    target_link_libraries(
        quiz_vulkan_fake_audio_engine_tests
        PRIVATE
            quiz_vulkan_audio_contract
    )
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_fake_audio_engine_tests)
    add_test(NAME quiz_vulkan_fake_audio_engine_tests COMMAND quiz_vulkan_fake_audio_engine_tests)

    add_executable(quiz_vulkan_asset_resolver_tests tests/assets/asset_resolver_tests.cpp)
    target_link_libraries(
        quiz_vulkan_asset_resolver_tests
        PRIVATE
            quiz_vulkan_asset_contract
    )
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_asset_resolver_tests)
    add_test(NAME quiz_vulkan_asset_resolver_tests COMMAND quiz_vulkan_asset_resolver_tests)

    add_library(
        quiz_vulkan_interface_contract_compile_tests
        OBJECT
        EXCLUDE_FROM_ALL
        tests/contracts/interface_contract_compile_tests.cpp
    )
    target_include_directories(quiz_vulkan_interface_contract_compile_tests PRIVATE src)
    target_compile_features(quiz_vulkan_interface_contract_compile_tests PRIVATE cxx_std_23)
endif()
