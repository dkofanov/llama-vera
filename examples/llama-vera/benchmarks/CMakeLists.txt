add_executable(llama-vera-full-bench full_bench.cpp)
target_link_libraries(llama-vera-full-bench PRIVATE parser3-vera)
target_compile_definitions(llama-vera-full-bench PRIVATE PARSER3_BUILD_TYPE="$<CONFIG>")
add_dependencies(llama-vera-full-bench llama-vera-generated)

add_executable(llama-vera-fork-bench fork_bench.cpp)
target_link_libraries(llama-vera-fork-bench PRIVATE parser3-vera)
target_compile_definitions(llama-vera-fork-bench PRIVATE PARSER3_BUILD_TYPE="$<CONFIG>")
add_dependencies(llama-vera-fork-bench llama-vera-generated)
