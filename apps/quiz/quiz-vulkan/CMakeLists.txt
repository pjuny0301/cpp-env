cmake_minimum_required(VERSION 3.25)

project(
    quiz_vulkan
    VERSION 0.1.0
    LANGUAGES CXX
)

add_library(quiz_vulkan_platform_shell STATIC)
target_sources(
    quiz_vulkan_platform_shell
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/platform/platform_shell.h
)
target_compile_features(quiz_vulkan_platform_shell PUBLIC cxx_std_20)
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
        src/core/domain/learning_state.cpp
        src/core/domain/quiz_model.cpp
        src/core/domain/quiz_session.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/core/domain/app_action.hpp
            src/core/domain/app_snapshot.hpp
            src/core/domain/learning_state.hpp
            src/core/domain/quiz_model.hpp
            src/core/domain/quiz_session.hpp
)
target_compile_features(quiz_vulkan_domain PUBLIC cxx_std_20)
target_include_directories(quiz_vulkan_domain PUBLIC src)

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
            src/core/layout/layout_placer.h
)
target_compile_features(quiz_vulkan_scene INTERFACE cxx_std_20)
target_include_directories(quiz_vulkan_scene INTERFACE src)

add_library(quiz_vulkan_ui STATIC)
target_sources(
    quiz_vulkan_ui
    PRIVATE
        src/core/ui/ui_renderer.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/core/ui/quiz_screens.h
            src/core/ui/quiz_screens.hpp
            src/core/ui/ui_renderer.h
)
target_compile_features(quiz_vulkan_ui PUBLIC cxx_std_20)
target_include_directories(quiz_vulkan_ui PUBLIC src)
target_link_libraries(quiz_vulkan_ui PUBLIC quiz_vulkan_domain quiz_vulkan_scene)

add_library(quiz_vulkan_renderer STATIC)
target_sources(
    quiz_vulkan_renderer
    PRIVATE
        src/render/vulkan/vulkan_renderer.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/render/vulkan/vulkan_renderer.h
)
target_compile_features(quiz_vulkan_renderer PUBLIC cxx_std_20)
target_include_directories(quiz_vulkan_renderer PUBLIC src)
target_link_libraries(quiz_vulkan_renderer PUBLIC quiz_vulkan_ui)

add_executable(quiz_vulkan)
target_sources(
    quiz_vulkan
    PRIVATE
        src/app/app.cpp
        src/app/app_state.cpp
        src/app/main.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/app/app.h
            src/app/app_state.h
)
target_compile_features(quiz_vulkan PRIVATE cxx_std_20)
target_link_libraries(quiz_vulkan PRIVATE quiz_vulkan_platform_shell quiz_vulkan_renderer quiz_vulkan_domain)

if(MSVC)
    target_compile_options(quiz_vulkan_platform_shell PRIVATE /W4 /permissive-)
    target_compile_options(quiz_vulkan_domain PRIVATE /W4 /permissive-)
    target_compile_options(quiz_vulkan_ui PRIVATE /W4 /permissive-)
    target_compile_options(quiz_vulkan_renderer PRIVATE /W4 /permissive-)
    target_compile_options(quiz_vulkan PRIVATE /W4 /permissive-)
else()
    target_compile_options(quiz_vulkan_platform_shell PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quiz_vulkan_domain PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quiz_vulkan_ui PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quiz_vulkan_renderer PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quiz_vulkan PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(quiz_vulkan_domain_smoke_test tests/domain/domain_smoke_test.cpp)
    target_link_libraries(quiz_vulkan_domain_smoke_test PRIVATE quiz_vulkan_domain)
    add_test(NAME quiz_vulkan_domain_smoke_test COMMAND quiz_vulkan_domain_smoke_test)

    add_executable(quiz_vulkan_app_state_tests tests/app/app_state_tests.cpp src/app/app_state.cpp)
    target_link_libraries(quiz_vulkan_app_state_tests PRIVATE quiz_vulkan_domain)
    add_test(NAME quiz_vulkan_app_state_tests COMMAND quiz_vulkan_app_state_tests)

    add_executable(quiz_vulkan_scene_layout_data_tests tests/scene/scene_layout_data_tests.cpp)
    target_link_libraries(quiz_vulkan_scene_layout_data_tests PRIVATE quiz_vulkan_scene)
    add_test(NAME quiz_vulkan_scene_layout_data_tests COMMAND quiz_vulkan_scene_layout_data_tests)

    add_executable(quiz_vulkan_layout_placer_tests tests/layout/layout_placer_tests.cpp)
    target_link_libraries(quiz_vulkan_layout_placer_tests PRIVATE quiz_vulkan_scene)
    add_test(NAME quiz_vulkan_layout_placer_tests COMMAND quiz_vulkan_layout_placer_tests)

    add_executable(quiz_vulkan_quiz_screens_tests tests/ui/quiz_screens_tests.cpp)
    target_link_libraries(quiz_vulkan_quiz_screens_tests PRIVATE quiz_vulkan_ui)
    add_test(NAME quiz_vulkan_quiz_screens_tests COMMAND quiz_vulkan_quiz_screens_tests)

    add_executable(quiz_vulkan_renderer_tests tests/render/vulkan_renderer_tests.cpp)
    target_link_libraries(quiz_vulkan_renderer_tests PRIVATE quiz_vulkan_renderer)
    add_test(NAME quiz_vulkan_renderer_tests COMMAND quiz_vulkan_renderer_tests)
endif()
