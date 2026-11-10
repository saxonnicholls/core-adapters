# core-adapters - the third-party adapters for snicholls-core.
#
# snicholls-core is standard-library-only and stays that way: it takes no
# third-party dependency. Everything that joins core to a third-party
# library lives here instead, and pins core like any other member of the
# family (core CLAUDE.md "One core for the whole family"):
#
#   snicholls::core_nanobind   nanobind helpers (GIL, errors, ndarray, bytes,
#                              optional / result): a header library for any
#                              snicholls Python extension.
#   snicholls::core_json       nlohmann/json hooks for core's value types.
#   snicholls_core (Python)    nanobind bindings of core's game/ and
#                              stats/random, built by scikit-build-core.
#
# Driven three ways: as the top-level project (tests, demos, benchmarks, docs),
# as a subdirectory of a consumer (the targets only), and by scikit-build-core
# from pyproject.toml (the Python modules and their stubs only).

cmake_minimum_required(VERSION 3.20)

project(snicholls_core_adapters
    VERSION 0.1.0
    DESCRIPTION "core-adapters - third-party adapters (nanobind, nlohmann/json) for snicholls-core"
    LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CORE_ADAPTERS_TOP_LEVEL ON)
else()
    set(CORE_ADAPTERS_TOP_LEVEL OFF)
endif()

if(SKBUILD)
    set(_core_adapters_tests_default OFF)
else()
    set(_core_adapters_tests_default ${CORE_ADAPTERS_TOP_LEVEL})
endif()

option(CORE_ADAPTERS_BUILD_TESTS "Build the core-adapters tests and demos" ${_core_adapters_tests_default})
option(CORE_ADAPTERS_WITH_NANOBIND "Find Python 3.12+ and the pinned nanobind, for snicholls::core_nanobind consumers" ON)
option(CORE_ADAPTERS_BUILD_PYTHON "Build the snicholls_core Python modules (needs CORE_ADAPTERS_WITH_NANOBIND)"
       ${CORE_ADAPTERS_TOP_LEVEL})
option(CORE_ADAPTERS_WITH_NLOHMANN "Define snicholls::core_json (nlohmann/json)" ON)
option(CORE_ADAPTERS_BUILD_BENCHMARKS "Build the benchmarks (Google Benchmark)" OFF)
option(CORE_ADAPTERS_BUILD_DOCS "Build the Doxygen documentation; warnings are errors" OFF)
option(CORE_ADAPTERS_WERROR "Treat warnings as errors in everything this project compiles" OFF)
set(CORE_ADAPTERS_SANITIZE "" CACHE STRING "Sanitizers for the C++ tests and demos, e.g. address,undefined")

if(CORE_ADAPTERS_TOP_LEVEL AND NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

# ---------------------------------------------------------------------------
# snicholls-core, pinned to a release tag (third_party/core). The diamond rules
# (core CLAUDE.md "Versioning and the diamond"): when a consumer already added
# core, core's own CMakeLists records this copy and skips it.

set(CORE_ADAPTERS_CORE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/core)
if(NOT EXISTS "${CORE_ADAPTERS_CORE_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "Submodule 'third_party/core' is not checked out.\n"
                        "Run: git submodule update --init --recursive")
endif()
add_subdirectory(${CORE_ADAPTERS_CORE_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/third_party/core EXCLUDE_FROM_ALL)
snicholls_core_require(0.4.0)

function(core_adapters_compile_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(CORE_ADAPTERS_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(CORE_ADAPTERS_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(core_adapters_sanitize target)
    if(CORE_ADAPTERS_SANITIZE AND NOT MSVC)
        target_compile_options(${target} PRIVATE -fsanitize=${CORE_ADAPTERS_SANITIZE} -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=${CORE_ADAPTERS_SANITIZE})
    endif()
endfunction()

# ---------------------------------------------------------------------------
# snicholls::core_nanobind - the header library. It carries our include root
# and core; nanobind itself arrives with the consumer's nanobind_add_module().

add_library(snicholls_core_nanobind INTERFACE)
add_library(snicholls::core_nanobind ALIAS snicholls_core_nanobind)
target_include_directories(snicholls_core_nanobind INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(snicholls_core_nanobind INTERFACE snicholls::core)
target_compile_features(snicholls_core_nanobind INTERFACE cxx_std_20)

# The version header alone, for anything that only needs to name the library
add_library(snicholls_core_adapters INTERFACE)
add_library(snicholls::core_adapters ALIAS snicholls_core_adapters)
target_include_directories(snicholls_core_adapters INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(snicholls_core_adapters INTERFACE snicholls::core)

# ---------------------------------------------------------------------------
# snicholls::core_json - nlohmann/json v3.12.0 (third_party/nlohmann_json),
# implicit conversions off, includes not SYSTEM. A consumer that already
# defines nlohmann_json::nlohmann_json keeps its own.

if(CORE_ADAPTERS_WITH_NLOHMANN)
    if(NOT TARGET nlohmann_json::nlohmann_json)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann_json/CMakeLists.txt")
            message(FATAL_ERROR "Submodule 'third_party/nlohmann_json' is not checked out.\n"
                                "Run: git submodule update --init --recursive")
        endif()
        set(JSON_BuildTests OFF CACHE INTERNAL "")
        set(JSON_Install OFF CACHE INTERNAL "")
        set(JSON_ImplicitConversions OFF CACHE INTERNAL "")
        set(JSON_SystemInclude OFF CACHE INTERNAL "")
        add_subdirectory(third_party/nlohmann_json ${CMAKE_CURRENT_BINARY_DIR}/third_party/nlohmann_json EXCLUDE_FROM_ALL)
    endif()
    add_library(snicholls_core_json INTERFACE)
    add_library(snicholls::core_json ALIAS snicholls_core_json)
    target_include_directories(snicholls_core_json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(snicholls_core_json INTERFACE snicholls::core nlohmann_json::nlohmann_json)
endif()

# ---------------------------------------------------------------------------
# Python: nanobind (third_party/nanobind, v3.1.0) and the snicholls_core
# package. Modules are written to <build>/python/snicholls_core next to the
# package's pure-Python files, so the build tree is an importable package.

if(CORE_ADAPTERS_BUILD_PYTHON)
    set(CORE_ADAPTERS_WITH_NANOBIND ON)
endif()
if(CORE_ADAPTERS_WITH_NANOBIND)
    # Python and nanobind_add_module(), unless the consumer already found them. A consumer that builds its own
    # extension modules with snicholls::core_nanobind uses this nanobind: one pin, one nanobind ABI for every module
    # that passes the other's objects (a snicholls_core.stats.Wyrand, say).
    if(NOT TARGET Python::Module)
        find_package(Python 3.12 REQUIRED COMPONENTS Interpreter Development.Module
                     OPTIONAL_COMPONENTS Development.SABIModule)
    endif()
    if(NOT COMMAND nanobind_add_module)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanobind/cmake/nanobind-config.cmake")
            message(FATAL_ERROR "Submodule 'third_party/nanobind' is not checked out.\n"
                                "Run: git submodule update --init --recursive")
        endif()
        find_package(nanobind CONFIG REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanobind/cmake
                     NO_DEFAULT_PATH)
    endif()
endif()
if(CORE_ADAPTERS_BUILD_PYTHON)
    set(CORE_ADAPTERS_PYTHON_DIR ${CMAKE_CURRENT_BINARY_DIR}/python)
    add_subdirectory(python)
endif()

# ---------------------------------------------------------------------------
# Tests, demos, benchmarks, docs

if(CORE_ADAPTERS_BUILD_TESTS)
    enable_testing()
    if(NOT TARGET GTest::gtest_main)
        # GoogleTest: core's pinned copy (third_party/core/third_party/googletest, v1.18.0)
        set(_gtest_root ${CORE_ADAPTERS_CORE_ROOT}/third_party/googletest)
        if(NOT EXISTS ${_gtest_root}/CMakeLists.txt)
            message(FATAL_ERROR "${_gtest_root} is empty - run: git submodule update --init --recursive")
        endif()
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        add_subdirectory(${_gtest_root} ${CMAKE_CURRENT_BINARY_DIR}/third_party/googletest EXCLUDE_FROM_ALL)
        # Plain -I, never SYSTEM: a Homebrew googletest in /usr/local/include would shadow it otherwise
        set_property(TARGET gtest gtest_main gmock gmock_main PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "")
    endif()

    # Every header compiles on its own, twice (a broken include guard is caught too). The
    # nanobind headers are compiled inside a module by their tests instead: they need Python.
    file(GLOB_RECURSE _ca_headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/include CONFIGURE_DEPENDS
         ${CMAKE_CURRENT_SOURCE_DIR}/include/snicholls/*.hpp)
    set(_ca_check_sources)
    foreach(_header IN LISTS _ca_headers)
        if(_header MATCHES "/nanobind" OR (_header MATCHES "/nlohmann/" AND NOT CORE_ADAPTERS_WITH_NLOHMANN))
            continue()
        endif()
        string(MAKE_C_IDENTIFIER "${_header}" _id)
        set(HEADER "${_header}")
        configure_file(cmake/header_check.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/header_check/${_id}.cpp @ONLY)
        list(APPEND _ca_check_sources ${CMAKE_CURRENT_BINARY_DIR}/header_check/${_id}.cpp)
    endforeach()
    add_library(core_adapters_header_check OBJECT ${_ca_check_sources})
    target_link_libraries(core_adapters_header_check PRIVATE snicholls::core_adapters)
    if(CORE_ADAPTERS_WITH_NLOHMANN)
        target_link_libraries(core_adapters_header_check PRIVATE snicholls::core_json)
    endif()
    core_adapters_compile_options(core_adapters_header_check)

    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME check.version COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_version.py)
    add_test(NAME check.mirror COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_mirror.py)
    set_tests_properties(check.version check.mirror PROPERTIES LABELS boundary)

    add_subdirectory(tests)
    add_subdirectory(demos)
endif()

if(CORE_ADAPTERS_BUILD_BENCHMARKS)
    if(NOT TARGET benchmark::benchmark)
        set(_bench_root ${CORE_ADAPTERS_CORE_ROOT}/third_party/benchmark)
        if(NOT EXISTS ${_bench_root}/CMakeLists.txt)
            message(FATAL_ERROR "${_bench_root} is empty - run: git submodule update --init --recursive")
        endif()
        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
        add_subdirectory(${_bench_root} ${CMAKE_CURRENT_BINARY_DIR}/third_party/benchmark EXCLUDE_FROM_ALL)
    endif()
    add_subdirectory(benchmarks)
endif()

if(CORE_ADAPTERS_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)
    configure_file(docs/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Doxygen: every public entity documented, warnings are errors"
        VERBATIM)
    if(CORE_ADAPTERS_BUILD_TESTS)
        add_test(NAME docs.build COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        set_tests_properties(docs.build PROPERTIES LABELS docs)
    endif()
endif()

# snicholls-core diamond rule 3: exactly one version of core in this build. Only the
# top-level project asserts it; as a subdirectory, the consumer's last line does.
if(CORE_ADAPTERS_TOP_LEVEL)
    snicholls_core_assert_single_version()
endif()
