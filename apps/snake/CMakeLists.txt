# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 6.8 REQUIRED COMPONENTS Quick LinguistTools)
qt_standard_project_setup(REQUIRES 6.8)

# The bundled font, from the design package. The files are compiled into the executable as resources.
set(SNAKE_FONT_DIR "${PROJECT_SOURCE_DIR}/docs/design/fonts")
set(SNAKE_FONT_FILES
    pixelify-sans-latin-400-normal.woff2
    pixelify-sans-latin-500-normal.woff2
    pixelify-sans-latin-600-normal.woff2
    pixelify-sans-latin-700-normal.woff2)
function(snake_add_font_resources target)
    list(TRANSFORM SNAKE_FONT_FILES PREPEND "${SNAKE_FONT_DIR}/" OUTPUT_VARIABLE font_paths)
    qt_add_resources(${target} fonts PREFIX /snake/fonts BASE "${SNAKE_FONT_DIR}" FILES ${font_paths})
endfunction()

# The QML module of the application: the screens and the C++ types they use. The executable and the UI tests each build
# one, so the tests run the same screens against the same types.
set(SNAKE_QML_FILES
    qml/DashedFrame.qml
    qml/GameOverScreen.qml
    qml/GameScreen.qml
    qml/Main.qml
    qml/ModeCard.qml
    qml/PauseScreen.qml
    qml/PhonePage.qml
    qml/ShadowText.qml
    qml/TitleScreen.qml
    qml/ToyButton.qml)
# The QML files sit at the root of the module (SnakeApp/Main.qml), as the tools expect: qmllint finds the types of the
# other files in the directory only then.
foreach(qml_file IN LISTS SNAKE_QML_FILES)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()
set(SNAKE_UI_SOURCES
    src/board/board_item.cpp
    src/board/board_item.hpp
    src/board/board_scene.cpp
    src/board/board_scene.hpp
    src/input/qt_keys.cpp
    src/input/qt_keys.hpp
    src/platform/save_file.cpp
    src/platform/save_file.hpp
    src/theme/theme.cpp
    src/theme/theme.hpp
    src/theme/tokens.hpp
    src/viewmodel/game_view_model.cpp
    src/viewmodel/game_view_model.hpp
    src/viewmodel/mode_list_model.cpp
    src/viewmodel/mode_list_model.hpp
    src/viewmodel/profile_view_model.cpp
    src/viewmodel/profile_view_model.hpp)
# QML is compiled ahead of time by qmlcachegen; no QML file is parsed from source at run time.
# The URI must differ from the executable name: the module gets a directory of that name in the build tree,
# and on a case-insensitive file system (macOS, Windows) "Snake" and "snake" are the same path.
# A second module of the same URI needs an output directory of its own: pass OUTPUT_DIRECTORY.
function(snake_add_ui_module target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "OUTPUT_DIRECTORY" "")
    set(output_directory)
    if(arg_OUTPUT_DIRECTORY)
        set(output_directory OUTPUT_DIRECTORY "${arg_OUTPUT_DIRECTORY}")
    endif()
    qt_add_qml_module(${target}
        URI SnakeApp
        VERSION 1.0
        QML_FILES ${SNAKE_QML_FILES}
        SOURCES ${SNAKE_UI_SOURCES}
        ${output_directory})
    # qmltyperegistrar includes each header by its file name, so the directories of the QML types are on the path too.
    target_include_directories(${target} PRIVATE src src/board src/theme src/viewmodel)
    snake_add_font_resources(${target})
    target_link_libraries(${target}
        PRIVATE snake::input snake::profile snake::sim Qt6::Quick icl::warnings icl::hardening icl::coverage)
    # qmlcachegen, rcc and moc write C++ files that the strict warning set rejects with some compilers.
    icl_relax_generated_warnings(${target})
endfunction()

qt_add_executable(snake_app src/main.cpp)
# The executable is named snake while the target is snake_app, so the QML module and the target stay distinct.
set_target_properties(snake_app PROPERTIES OUTPUT_NAME snake)
snake_add_ui_module(snake_app)

# Translations: the source language and Simplified Chinese. `cmake --build <dir> --target update_translations`
# refreshes the .ts files from the sources. docs/i18n.md records where each translation came from.
qt_add_translations(snake_app
    TS_FILES translations/snake_en.ts translations/snake_zh_CN.ts
    LUPDATE_OPTIONS -locations none -no-obsolete)

target_link_libraries(snake_app PRIVATE snake::core)

# Installs the program, copies the Qt libraries, plugins and QML modules next to it so the installed tree runs on a
# machine without Qt, and adds the licence, notices and source information. The macOS bundle identifier follows the
# reverse-domain form of the project's GitHub address; change it here if the project later gets its own domain, because
# macOS keys saved settings and permissions by it. catch2 is used by the tests only and is not shipped.
icl_install_qt_app(snake_app
    BUNDLE_IDENTIFIER io.github.iainchesworthlabs.snake
    DISPLAY_NAME Snake
    NOTICE_EXCLUDE catch2)
# The font's licence asks for its copyright notice and text to travel with the font.
if(APPLE)
    install(FILES "${SNAKE_FONT_DIR}/LICENSE-pixelify-sans.txt" DESTINATION "snake.app/Contents/Resources/doc")
else()
    install(FILES "${SNAKE_FONT_DIR}/LICENSE-pixelify-sans.txt" DESTINATION "share/doc/snake")
endif()

if(SNAKE_BUILD_TESTS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    set(snake_ui_test_environment
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QSG_RENDER_LOOP=basic)
    if(ICL_SANITIZERS MATCHES "thread")
        # The prebuilt Qt libraries are not instrumented; see the file for the reason.
        list(APPEND snake_ui_test_environment
            "TSAN_OPTIONS=suppressions=${PROJECT_SOURCE_DIR}/tools/sanitizers/tsan.supp")
    endif()

    # The screens, run for real: the application's QML module is built into a second executable with the tests, which
    # load Main.qml in an off-screen window and press the keys and buttons.
    qt_add_executable(snake_ui_tests tests/test_screens.cpp tests/test_screens_main.cpp)
    snake_add_ui_module(snake_ui_tests OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ui_tests/SnakeApp")
    target_link_libraries(snake_ui_tests PRIVATE Qt6::Test Catch2::Catch2)
    catch_discover_tests(snake_ui_tests
        PROPERTIES
            LABELS ui
            TIMEOUT 120
            ENVIRONMENT "${snake_ui_test_environment}")

    # Starts the real application without a display, waits for the first frame and exits.
    # The save file is in the build tree, so the run neither reads nor writes the user's own.
    add_test(NAME app.smoke
        COMMAND snake_app --smoke-test --save-file "${CMAKE_CURRENT_BINARY_DIR}/smoke-save.snk")
    set_tests_properties(app.smoke PROPERTIES
        LABELS ui
        TIMEOUT 60
        ENVIRONMENT "${snake_ui_test_environment}")

    # qmllint over the QML files; any warning fails (see qml/.qmllint.ini).
    add_test(NAME app.qmllint
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target snake_app_qmllint)
    set_tests_properties(app.qmllint PROPERTIES LABELS ui TIMEOUT 120)

    add_subdirectory(tests)
endif()
