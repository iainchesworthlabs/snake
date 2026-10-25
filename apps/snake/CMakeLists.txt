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
# The fonts for the scripts Pixelify Sans does not have, cut down to the characters of the translations by
# tools/fonts/subset_fonts.py.
set(SNAKE_NOTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fonts")
set(SNAKE_NOTO_FILES
    noto-sans-jp-400.woff2
    noto-sans-jp-700.woff2
    noto-sans-sc-400.woff2
    noto-sans-sc-700.woff2
    noto-sans-hebrew-400.woff2
    noto-sans-hebrew-700.woff2)
function(snake_add_font_resources target)
    list(TRANSFORM SNAKE_FONT_FILES PREPEND "${SNAKE_FONT_DIR}/" OUTPUT_VARIABLE font_paths)
    qt_add_resources(${target} fonts PREFIX /snake/fonts BASE "${SNAKE_FONT_DIR}" FILES ${font_paths})
    list(TRANSFORM SNAKE_NOTO_FILES PREPEND "${SNAKE_NOTO_DIR}/" OUTPUT_VARIABLE noto_paths)
    qt_add_resources(${target} noto_fonts PREFIX /snake/fonts BASE "${SNAKE_NOTO_DIR}" FILES ${noto_paths})
endfunction()

# What the About screen shows. The revision is the exact tag or commit this build was configured from: `git describe`,
# else the commit of the CI run, else "unknown". It is read when CMake configures, so a configure is needed to follow a
# new commit; every CI and release build configures afresh.
find_program(SNAKE_GIT git)
set(SNAKE_SOURCE_REVISION "")
if(SNAKE_GIT)
    execute_process(COMMAND "${SNAKE_GIT}" -C "${PROJECT_SOURCE_DIR}" describe --tags --always --dirty
        OUTPUT_VARIABLE SNAKE_SOURCE_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(NOT SNAKE_SOURCE_REVISION AND DEFINED ENV{GITHUB_SHA})
    set(SNAKE_SOURCE_REVISION "$ENV{GITHUB_SHA}")
endif()
if(NOT SNAKE_SOURCE_REVISION)
    set(SNAKE_SOURCE_REVISION "unknown")
endif()
configure_file(src/about/source_info.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/snake/source_info.hpp" @ONLY)

# The licence texts the About screen shows are compiled into the executable: the licence of the program, the notices
# that icl_install_qt_app() generates when CMake configures (one entry per runtime dependency, with its licence text),
# and the licence of the bundled font.
set(SNAKE_NOTICES_FILE "${CMAKE_CURRENT_BINARY_DIR}/snake-THIRD-PARTY-NOTICES.txt")
set_source_files_properties("${PROJECT_SOURCE_DIR}/LICENSE" PROPERTIES QT_RESOURCE_ALIAS licence.txt)
set_source_files_properties("${SNAKE_NOTICES_FILE}" PROPERTIES QT_RESOURCE_ALIAS notices.txt)
set_source_files_properties("${SNAKE_FONT_DIR}/LICENSE-pixelify-sans.txt" PROPERTIES QT_RESOURCE_ALIAS font-licence.txt)
set_source_files_properties("${SNAKE_NOTO_DIR}/LICENSE-noto-sans.txt" PROPERTIES QT_RESOURCE_ALIAS font-licence-noto.txt)
function(snake_add_about_resources target)
    qt_add_resources(${target} about PREFIX /snake/about
        FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${SNAKE_NOTICES_FILE}" "${SNAKE_FONT_DIR}/LICENSE-pixelify-sans.txt"
             "${SNAKE_NOTO_DIR}/LICENSE-noto-sans.txt")
endfunction()

# The QML module of the application: the screens and the C++ types they use. The executable and the UI tests each build
# one, so the tests run the same screens against the same types.
set(SNAKE_QML_FILES
    qml/AboutScreen.qml
    qml/ConfirmDialog.qml
    qml/DashedFrame.qml
    qml/GameOverScreen.qml
    qml/GameScreen.qml
    qml/LeaderboardScreen.qml
    qml/LetterKey.qml
    qml/Main.qml
    qml/ModeCard.qml
    qml/NameEntryScreen.qml
    qml/NoticeScreen.qml
    qml/PauseScreen.qml
    qml/PhonePage.qml
    qml/SettingsScreen.qml
    qml/ShadowText.qml
    qml/TitleScreen.qml
    qml/ToggleSwitch.qml
    qml/ToyButton.qml)
# The QML files sit at the root of the module (SnakeApp/Main.qml), as the tools expect: qmllint finds the types of the
# other files in the directory only then.
foreach(qml_file IN LISTS SNAKE_QML_FILES)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()
# miniaudio is a single-file C library from vcpkg. Its code is compiled once, here, with its own settings: the
# project's strict warnings and coverage are for the project's code. The audio thread and the runtime loading of the
# system's audio libraries need the thread library and, on Linux, libdl.
find_package(Threads REQUIRED)
find_path(SNAKE_MINIAUDIO_INCLUDE_DIR miniaudio.h REQUIRED)
add_library(snake_miniaudio STATIC src/platform/miniaudio_impl.cpp)
target_include_directories(snake_miniaudio SYSTEM PUBLIC "${SNAKE_MINIAUDIO_INCLUDE_DIR}")
set_source_files_properties(src/platform/miniaudio_impl.cpp PROPERTIES COMPILE_DEFINITIONS MINIAUDIO_IMPLEMENTATION)
target_link_libraries(snake_miniaudio PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
add_library(snake::miniaudio ALIAS snake_miniaudio)

set(SNAKE_UI_SOURCES
    src/platform/audio_output.cpp
    src/platform/audio_output.hpp
    src/platform/sound_sink.cpp
    src/platform/sound_sink.hpp
    src/about/about_info.cpp
    src/about/about_info.hpp
    src/board/board_item.cpp
    src/board/board_item.hpp
    src/board/board_scene.cpp
    src/board/board_scene.hpp
    src/board/effects.cpp
    src/board/effects.hpp
    src/input/qt_keys.cpp
    src/input/qt_keys.hpp
    src/platform/motion_preference.cpp
    src/platform/motion_preference.hpp
    src/platform/save_file.cpp
    src/platform/save_file.hpp
    src/platform/touch_screen.cpp
    src/platform/touch_screen.hpp
    src/theme/theme.cpp
    src/theme/theme.hpp
    src/theme/tokens.hpp
    src/viewmodel/effects_model.cpp
    src/viewmodel/effects_model.hpp
    src/viewmodel/game_view_model.cpp
    src/viewmodel/game_view_model.hpp
    src/viewmodel/leaderboard_model.cpp
    src/viewmodel/leaderboard_model.hpp
    src/viewmodel/mode_list_model.cpp
    src/viewmodel/mode_list_model.hpp
    src/viewmodel/profile_view_model.cpp
    src/viewmodel/profile_view_model.hpp
    src/viewmodel/sound_director.cpp
    src/viewmodel/sound_director.hpp
    src/viewmodel/ui_events.cpp
    src/viewmodel/ui_events.hpp)
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
    target_include_directories(${target} PRIVATE src src/about src/board src/theme src/viewmodel
        "${CMAKE_CURRENT_BINARY_DIR}/generated")
    snake_add_font_resources(${target})
    snake_add_about_resources(${target})
    target_link_libraries(${target}
        PRIVATE snake::audio snake::core snake::input snake::miniaudio snake::profile snake::sim Qt6::Quick icl::warnings icl::hardening icl::coverage)
    # qmlcachegen, rcc and moc write C++ files that the strict warning set rejects with some compilers.
    icl_relax_generated_warnings(${target})
endfunction()

qt_add_executable(snake_app src/main.cpp)
# The executable is named snake while the target is snake_app, so the QML module and the target stay distinct.
set_target_properties(snake_app PROPERTIES OUTPUT_NAME snake)
snake_add_ui_module(snake_app)

# Translations: the source language and the others of docs/i18n.md. `cmake --build <dir> --target
# update_translations` refreshes the .ts files from the sources. docs/i18n.md records where each translation came from.
qt_add_translations(snake_app
    TS_FILES translations/snake_en.ts translations/snake_zh_CN.ts translations/snake_fr.ts
             translations/snake_es.ts translations/snake_de.ts translations/snake_ja.ts
             translations/snake_he.ts translations/snake_yi.ts
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
    install(FILES "${SNAKE_FONT_DIR}/LICENSE-pixelify-sans.txt" "${SNAKE_NOTO_DIR}/LICENSE-noto-sans.txt"
        DESTINATION "snake.app/Contents/Resources/doc")
else()
    install(FILES "${SNAKE_FONT_DIR}/LICENSE-pixelify-sans.txt" "${SNAKE_NOTO_DIR}/LICENSE-noto-sans.txt"
        DESTINATION "share/doc/snake")
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
    # The language tests load the compiled catalogues that the application is built with.
    target_compile_definitions(snake_ui_tests PRIVATE SNAKE_QM_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    add_dependencies(snake_ui_tests release_translations)
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

    # The version and the help are answered without a window system: the platform plugin is one that does not exist.
    add_test(NAME app.version COMMAND snake_app --version)
    set_tests_properties(app.version PROPERTIES
        LABELS ui
        TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=there-is-no-such-plugin"
        PASS_REGULAR_EXPRESSION "^snake [0-9]+\\.[0-9]+\\.[0-9]+")
    add_test(NAME app.help COMMAND snake_app --help)
    set_tests_properties(app.help PROPERTIES
        LABELS ui
        TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=there-is-no-such-plugin"
        PASS_REGULAR_EXPRESSION "--language")

    # qmllint over the QML files; any warning fails (see qml/.qmllint.ini).
    add_test(NAME app.qmllint
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target snake_app_qmllint)
    set_tests_properties(app.qmllint PROPERTIES LABELS ui TIMEOUT 120)

    add_subdirectory(tests)
endif()
