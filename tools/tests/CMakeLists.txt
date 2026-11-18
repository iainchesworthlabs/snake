# SPDX-License-Identifier: GPL-3.0-or-later

# The unit tests of the scripts that check the documentation site (tools/ci/check_site_kit.py, check_site_pages.py and
# tools/docs/site_shell_hook.py). They need Python 3; the tests that run the company's palette checker skip themselves
# when Node is not installed. CI also runs them in the Site job of ci.yml.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME tools.site_checks
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s tools/tests
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    set_tests_properties(tools.site_checks PROPERTIES LABELS unit)
endif()
