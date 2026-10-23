# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(snake_cli src/main.cpp)
set_target_properties(snake_cli PROPERTIES OUTPUT_NAME snake)
target_link_libraries(snake_cli
    PRIVATE snake::core icl::warnings icl::hardening icl::coverage)

install(TARGETS snake_cli RUNTIME DESTINATION bin)

if(SNAKE_BUILD_TESTS)
    # Integration tests run the real executable.
    add_test(NAME cli.version COMMAND snake_cli --version)
    set_tests_properties(cli.version PROPERTIES
        LABELS integration
        PASS_REGULAR_EXPRESSION "^snake ${PROJECT_VERSION}")
    add_test(NAME cli.greets COMMAND snake_cli Ada)
    set_tests_properties(cli.greets PROPERTIES
        LABELS integration
        PASS_REGULAR_EXPRESSION "Hello, Ada, from ")
    add_test(NAME cli.rejects_extra_arguments COMMAND snake_cli one two)
    set_tests_properties(cli.rejects_extra_arguments PROPERTIES LABELS integration WILL_FAIL TRUE)
endif()
