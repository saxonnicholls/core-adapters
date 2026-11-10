# Benchmarks (Google Benchmark, core's pinned copy). Only Release numbers mean anything; scripts/run_benchmarks.sh
# builds them in Release and rewrites BENCHMARKS.md. A short smoke run of each is a CTest (label benchmark).

add_executable(tabular_shapley_benchmark tabular_shapley_benchmark.cpp)
target_link_libraries(tabular_shapley_benchmark PRIVATE snicholls::core benchmark::benchmark)
core_adapters_compile_options(tabular_shapley_benchmark)

if(CORE_ADAPTERS_BUILD_TESTS AND CORE_ADAPTERS_BUILD_PYTHON)
    add_test(NAME benchmark.tabular_shapley
             COMMAND Python::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tabular_shapley_benchmark.py
                     --cpp $<TARGET_FILE:tabular_shapley_benchmark> --smoke)
    set_tests_properties(benchmark.tabular_shapley PROPERTIES LABELS benchmark
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1;PYTHONPATH=${CORE_ADAPTERS_PYTHON_DIR}")
endif()
