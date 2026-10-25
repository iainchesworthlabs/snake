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

qt_add_executable(snake_app src/main.cpp)
# The executable is named snake while the target is snake_app, so the QML module and the target stay distinct.
set_target_properties(snake_app PROPERTIES OUTPUT_NAME snake)

# QML is compiled ahead of time by qmlcachegen; no QML file is parsed from source at run time.
# The URI must differ from the executable name: the module gets a directory of that name in the build tree,
# and on a case-insensitive file system (macOS, Windows) "Snake" and "snake" are the same path.
qt_add_qml_module(snake_app
    URI SnakeApp
    VERSION 1.0
    QML_FILES qml/Main.qml
    SOURCES
        src/board/board_item.cpp
        src/board/board_item.hpp
        src/board/board_scene.cpp
        src/board/board_scene.hpp
        src/input/qt_keys.cpp
        src/input/qt_keys.hpp
        src/theme/theme.cpp
        src/theme/theme.hpp
        src/theme/tokens.hpp
        src/viewmodel/game_view_model.cpp
        src/viewmodel/game_view_model.hpp
        src/viewmodel/mode_list_model.cpp
        src/viewmodel/mode_list_model.hpp
        src/viewmodel/profile_view_model.cpp
        src/viewmodel/profile_view_model.hpp)
# qmltyperegistrar includes each header by its file name, so the directories of the QML types are on the path too.
target_include_directories(snake_app PRIVATE src src/board src/theme src/viewmodel)
snake_add_font_resources(snake_app)

# Translations: the source language and Simplified Chinese. `cmake --build <dir> --target update_translations`
# refreshes the .ts files from the sources. docs/i18n.md records where each translation came from.
qt_add_translations(snake_app
    TS_FILES translations/snake_en.ts translations/snake_zh_CN.ts
    LUPDATE_OPTIONS -locations none -no-obsolete)

target_link_libraries(snake_app
    PRIVATE snake::core snake::input snake::profile snake::sim Qt6::Quick icl::warnings icl::hardening icl::coverage)

# qmlcachegen, rcc and moc write C++ files that the strict warning set rejects with some compilers.
icl_relax_generated_warnings(snake_app)

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

    # Starts the real application without a display, waits for the first frame and exits.
    add_test(NAME app.smoke COMMAND snake_app --smoke-test)
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
