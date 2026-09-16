add_executable(parser3-bench parser3_bench.cpp)
target_link_libraries(parser3-bench PRIVATE parser3)
target_compile_definitions(parser3-bench PRIVATE
    PARSER3_BENCH_UNITS=${PARSER3_BENCH_UNITS}
    PARSER3_BUILD_TYPE="$<CONFIG>")

add_custom_target(run-benchmarks
    COMMAND ${CMAKE_COMMAND} -E env
        "PARSER3_CURRENT_CONFIG=$<CONFIG>"
        "PARSER3_CURRENT_BIN=$<TARGET_FILE:parser3-bench>"
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks.sh
    DEPENDS parser3-bench
    USES_TERMINAL
    VERBATIM)
