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
    test_qt_keys.cpp
    ../src/input/qt_keys.cpp
    ../src/input/qt_keys.hpp
    ../src/viewmodel/game_view_model.cpp
    ../src/viewmodel/game_view_model.hpp)
set_target_properties(snake_viewmodel_tests PROPERTIES AUTOMOC ON)
target_include_directories(snake_viewmodel_tests PRIVATE ../src)
target_link_libraries(snake_viewmodel_tests
    PRIVATE snake::input snake::sim Qt6::Qml Catch2::Catch2 icl::warnings icl::coverage)
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
    ../src/input/qt_keys.cpp
    ../src/input/qt_keys.hpp
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
