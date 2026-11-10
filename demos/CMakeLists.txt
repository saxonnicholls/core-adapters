# Demos. The layout mirrors include/snicholls/core_adapters/ (scripts/check_mirror.py enforces it):
#
#   <sub>/<name>.hpp           -> <sub>/<name>_demo.cpp, an executable
#   nanobind/<name>.hpp        -> nanobind/<name>_demo.cpp, a demo module <name>_demo_module, and
#                                 nanobind/<name>_demo.py, the script that drives it
#   the snicholls_core package -> python/<module>_demo.py
#
# Each prints what it does and exits non-zero if the result is not what it documents, so every demo is also a CTest
# (label demo). scripts/demos/<name>_demo.sh (python_<module>_demo.sh for the package) starts one.

function(core_adapters_add_demo name source)
    add_executable(core_adapters_${name} ${source})
    target_link_libraries(core_adapters_${name} PRIVATE snicholls::core_adapters ${ARGN})
    core_adapters_compile_options(core_adapters_${name})
    core_adapters_sanitize(core_adapters_${name})
    add_test(NAME demo.${name} COMMAND core_adapters_${name})
    set_tests_properties(demo.${name} PROPERTIES LABELS demo)
endfunction()

core_adapters_add_demo(version_demo version_demo.cpp)

if(CORE_ADAPTERS_WITH_NLOHMANN)
    core_adapters_add_demo(decimal_json_demo nlohmann/decimal_json_demo.cpp snicholls::core_json)
    core_adapters_add_demo(json_demo nlohmann/json_demo.cpp snicholls::core_json)
    core_adapters_add_demo(secret_json_demo nlohmann/secret_json_demo.cpp snicholls::core_json)
    core_adapters_add_demo(time_json_demo nlohmann/time_json_demo.cpp snicholls::core_json)
    core_adapters_add_demo(uuid_json_demo nlohmann/uuid_json_demo.cpp snicholls::core_json)
endif()

if(CORE_ADAPTERS_BUILD_PYTHON)
    set(_demo_modules ${CMAKE_CURRENT_BINARY_DIR}/modules)
    set(_demo_env "PYTHONDONTWRITEBYTECODE=1;PYTHONPATH=${_demo_modules}:${CORE_ADAPTERS_PYTHON_DIR}")

    # core_adapters_add_module_demo(<name> <source>): the module <name>_module and the CTest running <name>.py
    function(core_adapters_add_module_demo name source)
        nanobind_add_module(${name}_module STABLE_ABI NB_STATIC ${source})
        target_link_libraries(${name}_module PRIVATE snicholls::core_nanobind)
        set_target_properties(${name}_module PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${_demo_modules})
        core_adapters_compile_options(${name}_module)
        get_filename_component(_dir ${source} DIRECTORY)
        add_custom_target(core_adapters_${name} DEPENDS ${name}_module snicholls_core_stats snicholls_core_game)
        add_test(NAME demo.${name} COMMAND Python::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/${_dir}/${name}.py)
        set_tests_properties(demo.${name} PROPERTIES LABELS demo ENVIRONMENT "${_demo_env}")
    endfunction()

    core_adapters_add_module_demo(bytes_demo nanobind/bytes_demo.cpp)
    core_adapters_add_module_demo(errors_demo nanobind/errors_demo.cpp)
    core_adapters_add_module_demo(gil_demo nanobind/gil_demo.cpp)
    core_adapters_add_module_demo(ndarray_demo nanobind/ndarray_demo.cpp)
    core_adapters_add_module_demo(optional_demo nanobind/optional_demo.cpp)

    # The package's demos
    foreach(_m IN ITEMS errors game stats)
        add_custom_target(core_adapters_python_${_m}_demo DEPENDS snicholls_core_stats snicholls_core_game)
        add_test(NAME demo.python.${_m} COMMAND Python::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/python/${_m}_demo.py)
        set_tests_properties(demo.python.${_m} PROPERTIES LABELS demo ENVIRONMENT "${_demo_env}")
    endforeach()
endif()
