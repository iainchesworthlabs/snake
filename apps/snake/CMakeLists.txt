# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 6.8 REQUIRED COMPONENTS Quick LinguistTools)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(snake_app src/main.cpp)
set_target_properties(snake_app PROPERTIES OUTPUT_NAME snake)

# QML is compiled ahead of time by qmlcachegen; no QML file is parsed from source at run time.
# The URI must differ from the executable name: the module gets a directory of that name in the build tree,
# and on a case-insensitive file system (macOS, Windows) "Snake" and "snake" are the same path.
qt_add_qml_module(snake_app
    URI SnakeApp
    VERSION 1.0
    QML_FILES qml/Main.qml
    SOURCES
        src/viewmodel/game_view_model.cpp
        src/viewmodel/game_view_model.hpp)
# qmltyperegistrar includes each header by its file name, so the directory of the view-models is on the path too.
target_include_directories(snake_app PRIVATE src src/viewmodel)

# Translations: the source language and Simplified Chinese. `cmake --build <dir> --target update_translations`
# refreshes the .ts files from the sources. docs/i18n.md records where each translation came from.
qt_add_translations(snake_app
    TS_FILES translations/snake_en.ts translations/snake_zh_CN.ts
    LUPDATE_OPTIONS -locations none -no-obsolete)

target_link_libraries(snake_app
    PRIVATE snake::core Qt6::Quick icl::warnings icl::hardening icl::coverage)

# qmlcachegen, rcc and moc write C++ files that the strict warning set rejects with some compilers.
icl_relax_generated_warnings(snake_app)

install(TARGETS snake_app
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin)

# Copies the Qt libraries, plugins and QML modules the application needs next to it, so the installed
# tree runs on a machine without Qt. This is what the release packages hold.
qt_generate_deploy_qml_app_script(
    TARGET snake_app
    OUTPUT_SCRIPT snake_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    NO_TRANSLATIONS)
install(SCRIPT "${snake_deploy_script}")

install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION share/doc/snake)

if(SNAKE_BUILD_TESTS)
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
