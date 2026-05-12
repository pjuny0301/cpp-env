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
            src/core/domain/deck_source.h
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
            src/assets/asset_bytes_provider.h
            src/assets/asset_manifest.h
            src/assets/asset_manifest_version_policy.h
            src/assets/asset_pack_index.h
            src/assets/asset_pack_validation.h
            src/assets/asset_path_policy.h
            src/assets/asset_resolver.h
            src/assets/asset_runtime_catalog.h
            src/assets/asset_runtime_resolver_policy.h
)
target_compile_features(quiz_vulkan_asset_contract INTERFACE cxx_std_23)
target_include_directories(quiz_vulkan_asset_contract INTERFACE src)

add_library(quiz_vulkan_deck_source_asset_adapter STATIC)
target_sources(
    quiz_vulkan_deck_source_asset_adapter
    PRIVATE
        src/assets/deck_source_asset_adapter.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/assets/deck_source_asset_adapter.h
)
target_compile_features(quiz_vulkan_deck_source_asset_adapter PUBLIC cxx_std_23)
target_include_directories(quiz_vulkan_deck_source_asset_adapter PUBLIC src)
target_link_libraries(
    quiz_vulkan_deck_source_asset_adapter
    PUBLIC
        quiz_vulkan_asset_contract
        quiz_vulkan_domain
)

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
            src/render/image/bmp_image_decoder.h
            src/render/image/image_decoder.h
            src/render/image/image_manifest_texture_pipeline.h
            src/render/image/image_resolver.h
            src/render/image/image_source_bytes_loader.h
            src/render/image/image_texture_cache.h
            src/render/image/image_texture_cache_snapshot.h
            src/render/image/image_texture_diagnostics.h
            src/render/image/image_texture_frame_snapshot.h
            src/render/image/image_texture_pipeline.h
            src/render/image/image_texture_placeholder_policy.h
            src/render/image/image_texture_upload.h
            src/render/image/image_texture_upload_operation_plan.h
            src/render/image/image_texture_upload_retry_policy.h
            src/render/image/image_texture_upload_snapshot_diff.h
            src/render/image/image_types.h
            src/render/image/png_image_chunk_scanner.h
            src/render/image/png_image_decode_boundary.h
            src/render/image/png_image_decoder.h
            src/render/image/png_image_header_inspector.h
            src/render/image/png_image_unfilter_boundary.h
            src/render/image/png_image_zlib_inflater.h
            src/render/image/standard_image_decoder_chain.h
            src/render/image/third_party_image_decoder_adapter.h
            src/render/render_draw_list.h
            src/render/text/font_backend_adapter.h
            src/render/text/font_backend_capabilities.h
            src/render/text/font_backend_dependency.h
            src/render/text/font_backend_selection.h
            src/render/text/font_coverage_run_segmentation.h
            src/render/text/font_cmap_inspector.h
            src/render/text/font_glyph_atlas.h
            src/render/text/font_glyph_atlas_page_plan.h
            src/render/text/font_glyph_atlas_upload_operation_plan.h
            src/render/text/font_glyph_id_resolver.h
            src/render/text/font_rasterizer.h
            src/render/text/font_resolver.h
            src/render/text/font_shaped_atlas_update.h
            src/render/text/font_shaping_backend.h
            src/render/text/font_sfnt_inspector.h
            src/render/text/font_source_bytes_loader.h
            src/render/text/font_source_resolver.h
            src/render/text/font_unicode_coverage.h
            src/render/text/glyph_run.h
            src/render/text/text_frame_draw_plan.h
            src/render/text/text_frame_snapshot.h
            src/render/text/text_engine.h
            src/render/text/utf8_line_break.h
            src/render/text/utf8_text_run.h
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
            src/core/input/input_key_policy.h
            src/core/input/input_pointer_policy.h
            src/core/input/input_routing_diagnostics.h
            src/core/input/input_text_route_policy.h
            src/core/input/normalized_input_replay.h
            src/core/input/normalized_input_replay_diff.h
            src/core/input/platform_input_engine_adapter.h
            src/core/input/platform_input_translator.h
            src/core/input/text_edit_transaction_diagnostics.h
            src/core/input/text_edit_transaction_replay.h
            src/core/input/text_input_presentation.h
            src/core/input/text_input_model.h
            src/core/input/text_input_types.h
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

add_library(quiz_vulkan_theme_contract INTERFACE)
target_sources(
    quiz_vulkan_theme_contract
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/core/theme/theme_tokens.h
)
target_compile_features(quiz_vulkan_theme_contract INTERFACE cxx_std_23)
target_include_directories(quiz_vulkan_theme_contract INTERFACE src)

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
            src/render/text/fake_text_engine_layout.h
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

add_library(quiz_vulkan_app_presentation INTERFACE)
target_sources(
    quiz_vulkan_app_presentation
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/app/app_quiz_screens.h
            src/app/app_quiz_screens.hpp
)
target_compile_features(quiz_vulkan_app_presentation INTERFACE cxx_std_23)
target_include_directories(quiz_vulkan_app_presentation INTERFACE src)
target_link_libraries(quiz_vulkan_app_presentation INTERFACE quiz_vulkan_domain quiz_vulkan_scene)

add_library(quiz_vulkan_ui INTERFACE)
target_link_libraries(quiz_vulkan_ui INTERFACE quiz_vulkan_ui_renderer)

add_library(quiz_vulkan_renderer STATIC)
target_sources(
    quiz_vulkan_renderer
    PRIVATE
        src/render/vulkan/vulkan_backend_adapter.cpp
        src/render/vulkan/vulkan_backend_frame_lifecycle.cpp
        src/render/vulkan/vulkan_backend_pipeline_cache.cpp
        src/render/vulkan/vulkan_backend_resource_binding.cpp
        src/render/vulkan/vulkan_backend_adapter_names.cpp
        src/render/vulkan/vulkan_frame_plan.cpp
        src/render/vulkan/vulkan_renderer.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/render/vulkan/vulkan_backend_adapter.h
            src/render/vulkan/vulkan_backend_command_recording.h
            src/render/vulkan/vulkan_backend_command_submit.h
            src/render/vulkan/vulkan_backend_device.h
            src/render/vulkan/vulkan_backend_frame_lifecycle.h
            src/render/vulkan/vulkan_backend_graphics_pipeline.h
            src/render/vulkan/vulkan_backend_instance.h
            src/render/vulkan/vulkan_backend_loader.h
            src/render/vulkan/vulkan_backend_native_readiness.h
            src/render/vulkan/vulkan_backend_native_symbols.h
            src/render/vulkan/vulkan_backend_pipeline_layout.h
            src/render/vulkan/vulkan_backend_queue_submit.h
            src/render/vulkan/vulkan_backend_render_pass.h
            src/render/vulkan/vulkan_backend_sdk.h
            src/render/vulkan/vulkan_backend_shader_module.h
            src/render/vulkan/vulkan_backend_swapchain.h
            src/render/vulkan/vulkan_frame_plan.h
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
        src/app/app_deck_loader.cpp
        src/app/app_demo.cpp
        src/app/app_input_router.cpp
        src/app/app_state.cpp
        src/app/main.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/app/app_action_router.h
            src/app/app.h
            src/app/app_deck_loader.h
            src/app/app_demo.h
            src/app/app_input_router.h
            src/app/app_render_pipeline.h
            src/app/app_sound_router.h
            src/app/app_state.h
)
target_compile_features(quiz_vulkan PRIVATE cxx_std_23)
target_link_libraries(
    quiz_vulkan
    PRIVATE
        quiz_vulkan_platform_shell
        quiz_vulkan_renderer
        quiz_vulkan_ui
        quiz_vulkan_app_presentation
        quiz_vulkan_text_engine
        quiz_vulkan_text_scene_bridge
        quiz_vulkan_domain
        quiz_vulkan_deck_source_asset_adapter
        quiz_vulkan_input
        quiz_vulkan_audio_contract
)
quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan)

if(MSVC)
    target_compile_options(quiz_vulkan_platform_shell PRIVATE /W4 /permissive-)
    target_compile_options(quiz_vulkan_domain PRIVATE /W4 /permissive-)
    target_compile_options(quiz_vulkan_deck_source_asset_adapter PRIVATE /W4 /permissive-)
    target_compile_options(quiz_vulkan_input PRIVATE /W4 /permissive-)
    target_compile_options(quiz_vulkan_ui_renderer PRIVATE /W4 /permissive-)
    target_compile_options(quiz_vulkan_renderer PRIVATE /W4 /permissive-)
    target_compile_options(quiz_vulkan PRIVATE /W4 /permissive-)
else()
    target_compile_options(quiz_vulkan_platform_shell PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quiz_vulkan_domain PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quiz_vulkan_deck_source_asset_adapter PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quiz_vulkan_input PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quiz_vulkan_ui_renderer PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quiz_vulkan_renderer PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quiz_vulkan PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    function(quiz_vulkan_add_test_executable target_name)
        add_executable(${target_name} ${ARGN})
        quiz_vulkan_copy_mingw_runtime_dlls(${target_name})
        add_test(NAME ${target_name} COMMAND ${target_name})
    endfunction()

    function(quiz_vulkan_add_test_directory test_dir)
        set(options)
        set(one_value_args)
        set(multi_value_args LINK_LIBRARIES SOURCES)
        cmake_parse_arguments(
            QUIZ_TEST_DIR
            "${options}"
            "${one_value_args}"
            "${multi_value_args}"
            ${ARGN}
        )

        file(
            GLOB test_sources
            CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/${test_dir}/*_test.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/${test_dir}/*_tests.cpp"
        )
        foreach(test_source IN LISTS test_sources)
            get_filename_component(test_name "${test_source}" NAME_WE)
            if(test_name MATCHES "_interface_contract_tests$")
                continue()
            endif()

            set(target_name "quiz_vulkan_${test_name}")
            quiz_vulkan_add_test_executable(
                ${target_name}
                "${test_source}"
                ${QUIZ_TEST_DIR_SOURCES}
            )
            if(QUIZ_TEST_DIR_LINK_LIBRARIES)
                target_link_libraries(${target_name} PRIVATE ${QUIZ_TEST_DIR_LINK_LIBRARIES})
            endif()
        endforeach()
    endfunction()

    quiz_vulkan_add_test_directory(tests/domain LINK_LIBRARIES quiz_vulkan_domain)

    quiz_vulkan_add_test_executable(
        quiz_vulkan_app_state_tests
        tests/app/app_state_tests.cpp
        src/app/app_state.cpp
    )
    target_link_libraries(quiz_vulkan_app_state_tests PRIVATE quiz_vulkan_domain)

    quiz_vulkan_add_test_executable(
        quiz_vulkan_app_action_router_tests
        tests/app/app_action_router_tests.cpp
        src/app/app_action_router.cpp
    )
    target_link_libraries(quiz_vulkan_app_action_router_tests PRIVATE quiz_vulkan_domain quiz_vulkan_scene)

    quiz_vulkan_add_test_executable(quiz_vulkan_app_sound_router_tests tests/app/app_sound_router_tests.cpp)
    target_link_libraries(quiz_vulkan_app_sound_router_tests PRIVATE quiz_vulkan_domain quiz_vulkan_audio_contract)

    add_executable(
        quiz_vulkan_app_deck_loader_tests
        tests/app/app_deck_loader_tests.cpp
        src/app/app_deck_loader.cpp
    )
    target_link_libraries(
        quiz_vulkan_app_deck_loader_tests
        PRIVATE
            quiz_vulkan_domain
            quiz_vulkan_deck_source_asset_adapter
    )
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_app_deck_loader_tests)
    add_test(NAME quiz_vulkan_app_deck_loader_tests COMMAND quiz_vulkan_app_deck_loader_tests)

    add_executable(
        quiz_vulkan_app_input_router_tests
        tests/app/app_input_router_tests.cpp
        src/app/app_action_router.cpp
        src/app/app_input_router.cpp
    )
    target_link_libraries(quiz_vulkan_app_input_router_tests PRIVATE quiz_vulkan_domain quiz_vulkan_input quiz_vulkan_scene)
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_app_input_router_tests)
    add_test(NAME quiz_vulkan_app_input_router_tests COMMAND quiz_vulkan_app_input_router_tests)

    add_executable(quiz_vulkan_app_quiz_screens_tests tests/app/app_quiz_screens_tests.cpp)
    target_link_libraries(
        quiz_vulkan_app_quiz_screens_tests
        PRIVATE
            quiz_vulkan_app_presentation
            quiz_vulkan_scene
            quiz_vulkan_domain
    )
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_app_quiz_screens_tests)
    add_test(NAME quiz_vulkan_app_quiz_screens_tests COMMAND quiz_vulkan_app_quiz_screens_tests)

    add_executable(quiz_vulkan_app_demo_tests tests/app/app_demo_tests.cpp src/app/app_demo.cpp src/app/app_state.cpp)
    target_link_libraries(
        quiz_vulkan_app_demo_tests
        PRIVATE
            quiz_vulkan_renderer
            quiz_vulkan_ui
            quiz_vulkan_app_presentation
            quiz_vulkan_text_engine
            quiz_vulkan_text_scene_bridge
            quiz_vulkan_domain
    )
    quiz_vulkan_copy_mingw_runtime_dlls(quiz_vulkan_app_demo_tests)
    add_test(NAME quiz_vulkan_app_demo_tests COMMAND quiz_vulkan_app_demo_tests)

    quiz_vulkan_add_test_directory(tests/scene LINK_LIBRARIES quiz_vulkan_scene)
    quiz_vulkan_add_test_directory(tests/layout LINK_LIBRARIES quiz_vulkan_scene)
    quiz_vulkan_add_test_directory(tests/input LINK_LIBRARIES quiz_vulkan_input)
    quiz_vulkan_add_test_directory(tests/ui LINK_LIBRARIES quiz_vulkan_ui)

    quiz_vulkan_add_test_executable(
        quiz_vulkan_architecture_boundary_tests
        tests/architecture/architecture_boundary_tests.cpp
    )
    target_compile_definitions(
        quiz_vulkan_architecture_boundary_tests
        PRIVATE
            QUIZ_VULKAN_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    )

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

    quiz_vulkan_add_test_directory(tests/render/text LINK_LIBRARIES quiz_vulkan_render_contract quiz_vulkan_scene quiz_vulkan_text_engine quiz_vulkan_text_scene_bridge)
    quiz_vulkan_add_test_directory(tests/render/image LINK_LIBRARIES quiz_vulkan_render_contract)
    quiz_vulkan_add_test_directory(tests/render/vulkan LINK_LIBRARIES quiz_vulkan_renderer)
    quiz_vulkan_add_test_directory(tests/audio LINK_LIBRARIES quiz_vulkan_audio_contract)
    quiz_vulkan_add_test_directory(tests/assets LINK_LIBRARIES quiz_vulkan_asset_contract quiz_vulkan_deck_source_asset_adapter)

    add_library(
        quiz_vulkan_interface_contract_compile_tests
        OBJECT
        EXCLUDE_FROM_ALL
        tests/contracts/interface_contract_compile_tests.cpp
    )
    target_include_directories(quiz_vulkan_interface_contract_compile_tests PRIVATE src)
    target_compile_features(quiz_vulkan_interface_contract_compile_tests PRIVATE cxx_std_23)
endif()
