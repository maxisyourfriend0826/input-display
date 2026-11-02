cmake_minimum_required(VERSION 3.24)
project(InputDisplay VERSION 5.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(IDISP_BUILD_APP "Build the overlay app (needs SDL3)" ON)
option(IDISP_BUILD_TESTS "Build the core unit tests" OFF)

include(cmake/Dependencies.cmake)

# ---- core: input math, rules, settings, layouts. No SDL. -------------------
add_library(idisp_core STATIC
    core/src/rules.cpp
    core/src/settings.cpp
    core/src/layout.cpp
    core/src/game.cpp
)
target_include_directories(idisp_core PUBLIC core/include)
target_link_libraries(idisp_core PRIVATE nlohmann_json::nlohmann_json)
if(MSVC)
    target_compile_options(idisp_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(idisp_core PRIVATE -Wall -Wextra)
endif()

# ---- tests -----------------------------------------------------------------
if(IDISP_BUILD_TESTS)
    enable_testing()
    add_executable(idisp_tests tests/test_core.cpp tests/test_equivalence.cpp tests/test_game.cpp)
    target_link_libraries(idisp_tests PRIVATE idisp_core doctest_header)
    target_compile_definitions(idisp_tests PRIVATE IDISP_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_test(NAME idisp_tests COMMAND idisp_tests)
endif()

# ---- app -------------------------------------------------------------------
if(IDISP_BUILD_APP)
    add_executable(InputDisplay
        app/icon.rc
        app/main.cpp
        app/draw.cpp
        app/overlay.cpp
        app/text.cpp
        app/image.cpp
        app/settings_window.cpp
        app/ui_theme.cpp
        app/layout_editor.cpp
        app/game_memory.cpp
    )
    target_link_libraries(InputDisplay PRIVATE idisp_core SDL3::SDL3-static imgui stb_header nlohmann_json::nlohmann_json)
    set_target_properties(InputDisplay PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE FALSE)
    if(MSVC)
        target_compile_options(InputDisplay PRIVATE /W4 /permissive-)
    endif()
    # Ship assets next to the executable.
    add_custom_command(TARGET InputDisplay POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:InputDisplay>/assets
        # Remove this obsolete directory from an incremental build output too.
        COMMAND ${CMAKE_COMMAND} -E remove_directory $<TARGET_FILE_DIR:InputDisplay>/assets/toons
        COMMAND ${CMAKE_COMMAND} -E rm -f $<TARGET_FILE_DIR:InputDisplay>/assets/ARLRDBD.TTF
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/THIRD-PARTY-NOTICES.txt $<TARGET_FILE_DIR:InputDisplay>)
endif()
