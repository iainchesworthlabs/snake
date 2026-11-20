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

# The Game Center achievement images (tools/game-center/make_images.py): each file is the size, colour type and resolution
# App Store Connect asks for, the script gives the same bytes twice, and the committed images are the ones it draws.
if(Python3_Interpreter_FOUND)
    add_test(NAME tools.game_center_images
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s tools/tests -p test_game_center_images.py
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    set_tests_properties(tools.game_center_images PROPERTIES LABELS unit)
endif()

# The script that keeps Game Center in App Store Connect the same as components.json
# (tools/game-center/sync_app_store_connect.py), run against a stand-in for App Store Connect: what a dry run says, what
# an apply makes, that a second run changes nothing, and that nothing is deleted. It uses no key and reaches nobody.
if(Python3_Interpreter_FOUND)
    add_test(NAME tools.game_center_sync
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s tools/tests -p test_game_center_sync.py
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    set_tests_properties(tools.game_center_sync PROPERTIES LABELS unit)
endif()
