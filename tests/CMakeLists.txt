# Tests. The layout mirrors include/snicholls/core_adapters/ (scripts/check_mirror.py enforces it):
#
#   <sub>/<name>.hpp           -> <sub>/<name>_test.cpp, a GoogleTest file in core_adapters_tests
#   nanobind/<name>.hpp        -> nanobind/<name>_test.cpp, a test module, and nanobind/<name>_test.py, its pytest file
#   the snicholls_core package -> python/<module>_test.py, run against the build tree here and against the installed
#                                 package by scripts/gate.sh
#
# support/ holds what is shared or checks core itself: the JSON kinds, core's generator against the independent
# fixture, and the C++ reference program the Python determinism tests compare with.

set(_fixtures ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)

add_executable(core_adapters_tests
    version_test.cpp
)
target_link_libraries(core_adapters_tests PRIVATE snicholls::core_adapters GTest::gtest_main)
if(CORE_ADAPTERS_WITH_NLOHMANN)
    target_sources(core_adapters_tests PRIVATE
        nlohmann/decimal_json_test.cpp
        nlohmann/json_test.cpp
        nlohmann/secret_json_test.cpp
        nlohmann/time_json_test.cpp
        nlohmann/uuid_json_test.cpp
        support/core_random_fixture.cpp
    )
    target_link_libraries(core_adapters_tests PRIVATE snicholls::core_json)
    target_compile_definitions(core_adapters_tests PRIVATE CORE_ADAPTERS_FIXTURES="${_fixtures}")
endif()

# What core computes, for the Python determinism tests to compare with
add_executable(core_adapters_reference_values support/reference_values.cpp)
target_link_libraries(core_adapters_reference_values PRIVATE snicholls::core)
core_adapters_compile_options(core_adapters_reference_values)
core_adapters_compile_options(core_adapters_tests)
core_adapters_sanitize(core_adapters_tests)
include(GoogleTest)
gtest_discover_tests(core_adapters_tests PROPERTIES LABELS unit DISCOVERY_TIMEOUT 60)

add_test(NAME generated.random_fixture_up_to_date
         COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/scripts/generate_random_fixture.py --check)
set_tests_properties(generated.random_fixture_up_to_date PROPERTIES LABELS boundary)

if(CORE_ADAPTERS_BUILD_PYTHON)
    set(_modules ${CMAKE_CURRENT_BINARY_DIR}/modules)
    set(_pytest Python::Interpreter -m pytest -q -p no:cacheprovider)
    set(_env "PYTHONDONTWRITEBYTECODE=1")

    # nanobind/<name>_test.cpp as the module <name>_module (not <name>_test: pytest imports the .py under that name),
    # and nanobind/<name>_test.py run against it
    set(_module_tests
        nanobind/bytes_test.cpp
        nanobind/errors_test.cpp
        nanobind/gil_test.cpp
        nanobind/ndarray_test.cpp
        nanobind/optional_test.cpp
    )
    foreach(_src IN LISTS _module_tests)
        get_filename_component(_name ${_src} NAME_WE)
        string(REGEX REPLACE "_test$" "" _name ${_name})
        nanobind_add_module(${_name}_module STABLE_ABI NB_STATIC ${_src})
        target_link_libraries(${_name}_module PRIVATE snicholls::core_nanobind)
        set_target_properties(${_name}_module PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${_modules})
        core_adapters_compile_options(${_name}_module)
        add_test(NAME pytest.nanobind.${_name}
                 COMMAND ${_pytest} ${CMAKE_CURRENT_SOURCE_DIR}/nanobind/${_name}_test.py)
        set_tests_properties(pytest.nanobind.${_name} PROPERTIES LABELS unit
            ENVIRONMENT "${_env};PYTHONPATH=${_modules}:${CORE_ADAPTERS_PYTHON_DIR}")
    endforeach()

    # The snicholls_core package, from the build tree
    add_test(NAME pytest.snicholls_core COMMAND ${_pytest} ${CMAKE_CURRENT_SOURCE_DIR}/python)
    set_tests_properties(pytest.snicholls_core PROPERTIES LABELS unit
        ENVIRONMENT "${_env};PYTHONPATH=${CORE_ADAPTERS_PYTHON_DIR};CORE_ADAPTERS_REFERENCE_DIR=${PROJECT_BINARY_DIR}")
endif()
