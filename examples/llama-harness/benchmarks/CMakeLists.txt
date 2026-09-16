add_executable(parser3-full-bench parser3_full_bench.cpp)
target_link_libraries(parser3-full-bench PRIVATE parser3-vera)
target_compile_definitions(parser3-full-bench PRIVATE PARSER3_BUILD_TYPE="$<CONFIG>")
add_dependencies(parser3-full-bench llama-harness-generated)

add_executable(parser3-fork-bench parser3_fork_bench.cpp)
target_link_libraries(parser3-fork-bench PRIVATE parser3-vera)
target_compile_definitions(parser3-fork-bench PRIVATE PARSER3_BUILD_TYPE="$<CONFIG>")
add_dependencies(parser3-fork-bench llama-harness-generated)
