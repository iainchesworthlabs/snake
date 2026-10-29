# SPDX-License-Identifier: GPL-3.0-or-later

# The translation catalogues are read as text, so this binary needs no Qt.
add_executable(snake_translation_tests test_translations.cpp)
target_link_libraries(snake_translation_tests
    PRIVATE Catch2::Catch2WithMain icl::warnings icl::coverage)
catch_discover_tests(snake_translation_tests
    PROPERTIES
        LABELS unit
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../translations")

# The view-model is built a second time here so that the tests link it without the QML module.
add_executable(snake_viewmodel_tests
    test_main.cpp
    test_game_view_model.cpp
    test_profile_view_model.cpp
    test_qt_keys.cpp
    test_effects_model.cpp
    test_gamepad_controller.cpp
    test_haptics.cpp
    test_language_choice.cpp
    test_input_controller.cpp
    test_orientation_director.cpp
    test_save_file.cpp
    test_sound_director.cpp
    ../src/input/qt_keys.cpp
    ../src/input/qt_keys.hpp
    ../src/platform/motion_preference.cpp
    ../src/platform/motion_preference.hpp
    ../src/platform/save_file.cpp
    ../src/platform/save_file.hpp
    ../src/platform/sound_sink.cpp
    ../src/platform/sound_sink.hpp
    ../src/platform/touch_screen.cpp
    ../src/platform/touch_screen.hpp
    ../src/board/effects.cpp
    ../src/board/effects.hpp
    ../src/device/accelerometer.cpp
    ../src/device/gamepad.cpp
    ../src/language/language_choice.cpp
    ../src/language/language_choice.hpp
    ../src/device/haptics.cpp
    ../src/device/haptics.hpp
    ../src/viewmodel/effects_model.cpp
    ../src/viewmodel/effects_model.hpp
    ../src/viewmodel/game_view_model.cpp
    ../src/viewmodel/game_view_model.hpp
    ../src/viewmodel/gamepad_controller.cpp
    ../src/viewmodel/gamepad_controller.hpp
    ../src/viewmodel/haptics_director.cpp
    ../src/viewmodel/haptics_director.hpp
    ../src/viewmodel/input_controller.cpp
    ../src/viewmodel/input_controller.hpp
    ../src/viewmodel/leaderboard_model.cpp
    ../src/viewmodel/leaderboard_model.hpp
    ../src/viewmodel/mode_list_model.cpp
    ../src/viewmodel/mode_list_model.hpp
    ../src/viewmodel/orientation_director.cpp
    ../src/viewmodel/orientation_director.hpp
    ../src/viewmodel/profile_view_model.cpp
    ../src/viewmodel/profile_view_model.hpp
    ../src/viewmodel/sound_director.cpp
    ../src/viewmodel/sound_director.hpp
    ../src/viewmodel/ui_events.cpp
    ../src/viewmodel/ui_events.hpp)
set_target_properties(snake_viewmodel_tests PROPERTIES AUTOMOC ON)
# The device's own accelerometer and motor, for the operating system this is built for.
icl_platform_sources(snake_viewmodel_tests DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src/device")
# A computer's gamepads come from SDL, which the test of them drives with a virtual one.
find_package(SDL3 CONFIG REQUIRED)
target_sources(snake_viewmodel_tests PRIVATE test_sdl_gamepad.cpp ../src/device/sdl_gamepad.cpp ../src/device/sdl_gamepad.hpp)
target_include_directories(snake_viewmodel_tests PRIVATE ../src)
target_link_libraries(snake_viewmodel_tests
    PRIVATE snake::audio snake::input snake::profile snake::sim SDL3::SDL3 Qt6::Gui Qt6::Qml Qt6::Test Catch2::Catch2 icl::warnings icl::coverage)
icl_relax_generated_warnings(snake_viewmodel_tests)
catch_discover_tests(snake_viewmodel_tests
    PROPERTIES
        LABELS unit
        ENVIRONMENT "${snake_ui_test_environment}")

# The board item is drawn into an off-screen window and the pixels are read back. It needs the QML-free view-model too.
add_executable(snake_board_tests
    test_board_item.cpp
    test_board_main.cpp
    ../src/board/board_item.cpp
    ../src/board/board_item.hpp
    ../src/board/board_scene.cpp
    ../src/board/board_scene.hpp
    ../src/board/effects.cpp
    ../src/board/effects.hpp
    ../src/input/qt_keys.cpp
    ../src/input/qt_keys.hpp
    ../src/viewmodel/effects_model.cpp
    ../src/viewmodel/effects_model.hpp
    ../src/viewmodel/game_view_model.cpp
    ../src/viewmodel/game_view_model.hpp)
set_target_properties(snake_board_tests PROPERTIES AUTOMOC ON)
target_include_directories(snake_board_tests PRIVATE ../src)
target_link_libraries(snake_board_tests
    PRIVATE snake::input snake::sim Qt6::Quick Catch2::Catch2 icl::warnings icl::coverage)
icl_relax_generated_warnings(snake_board_tests)
catch_discover_tests(snake_board_tests
    PROPERTIES
        LABELS ui
        TIMEOUT 60
        ENVIRONMENT "${snake_ui_test_environment}")

# The theme: the token values against docs/design/tokens.json, and the bundled fonts loading.
add_executable(snake_theme_tests
    test_theme.cpp
    test_board_main.cpp
    ../src/theme/theme.cpp
    ../src/theme/theme.hpp
    ../src/theme/tokens.hpp)
set_target_properties(snake_theme_tests PROPERTIES AUTOMOC ON)
snake_add_font_resources(snake_theme_tests)
target_include_directories(snake_theme_tests PRIVATE ../src)
target_compile_definitions(snake_theme_tests PRIVATE
    SNAKE_TOKENS_JSON="${PROJECT_SOURCE_DIR}/docs/design/tokens.json"
    SNAKE_TRANSLATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../translations")
target_link_libraries(snake_theme_tests
    PRIVATE Qt6::Quick Catch2::Catch2 icl::warnings icl::coverage)
icl_relax_generated_warnings(snake_theme_tests)
catch_discover_tests(snake_theme_tests
    PROPERTIES
        LABELS unit
        TIMEOUT 60
        ENVIRONMENT "${snake_ui_test_environment}")

# The shapes of the board art, checked without a window: the layout, the stripes, the head, the food and the frame.
add_executable(snake_board_scene_tests
    test_board_scene.cpp
    test_effects.cpp
    ../src/board/board_scene.cpp
    ../src/board/board_scene.hpp
    ../src/board/effects.cpp
    ../src/board/effects.hpp)
target_include_directories(snake_board_scene_tests PRIVATE ../src)
target_link_libraries(snake_board_scene_tests
    PRIVATE snake::sim Qt6::Gui Catch2::Catch2WithMain icl::warnings icl::coverage)
icl_relax_generated_warnings(snake_board_scene_tests)
catch_discover_tests(snake_board_scene_tests PROPERTIES LABELS unit)

# The audio output: the synthesiser behind its lock, and a device that plays nothing but is asked for samples like a
# real one. It needs no Qt widgets or window, so it has its own program.
add_executable(snake_audio_output_tests
    test_audio_output.cpp
    ../src/platform/audio_output.cpp
    ../src/platform/audio_output.hpp
    ../src/platform/sound_sink.hpp)
set_target_properties(snake_audio_output_tests PROPERTIES AUTOMOC OFF)
target_include_directories(snake_audio_output_tests PRIVATE ../src)
target_link_libraries(snake_audio_output_tests
    PRIVATE snake::audio snake::miniaudio Qt6::Core Catch2::Catch2WithMain icl::warnings icl::coverage)
catch_discover_tests(snake_audio_output_tests
    PROPERTIES
        LABELS unit
        TIMEOUT 60)
