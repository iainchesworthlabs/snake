# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(snake_app src/main.cpp)
set_target_properties(snake_app PROPERTIES OUTPUT_NAME snake)

# QML is compiled ahead of time by qmlcachegen; no QML file is parsed from source at run time.
qt_add_qml_module(snake_app
    URI Snake
    VERSION 1.0
    QML_FILES qml/Main.qml)

target_link_libraries(snake_app
    PRIVATE snake::core Qt6::Quick icl::warnings icl::hardening icl::coverage)

# qmlcachegen, rcc and moc write C++ files that the strict warning set rejects with some compilers.
# The files Qt adds to the target are found after Qt finalises it, at the end of this directory.
function(snake_relax_generated_warnings target)
    get_target_property(target_sources ${target} SOURCES)
    foreach(source IN LISTS target_sources)
        if(source MATCHES "^${CMAKE_CURRENT_BINARY_DIR}/.*\\.(cpp|cxx|cc)$")
            set_source_files_properties("${source}" PROPERTIES COMPILE_OPTIONS "${ICL_WARNINGS_OFF_FLAG}")
        endif()
    endforeach()
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/${target}_autogen/mocs_compilation.cpp"
        PROPERTIES COMPILE_OPTIONS "${ICL_WARNINGS_OFF_FLAG}" GENERATED ON)
endfunction()
cmake_language(DEFER CALL snake_relax_generated_warnings snake_app)

install(TARGETS snake_app
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin)

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
endif()
