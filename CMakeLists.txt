cmake_minimum_required(VERSION 3.14)
project(llama-vera LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

# parser3 is the library this repository publishes; it builds standalone.  In
# this top-level build its tests and benchmarks are on by default; a project
# that embeds parser3 via add_subdirectory keeps them off.
option(PARSER3_BUILD_TESTS "Build parser3 tests" ON)
option(PARSER3_BUILD_BENCHMARKS "Build parser3 benchmarks" ON)
add_subdirectory(parser3)

# The llama harness is an example consumer of parser3.  It needs llama.cpp, so
# it is skipped (with a warning) when llama is not found, leaving parser3 usable
# on its own.
option(LLAMA_VERA_BUILD_EXAMPLE "Build the llama harness example" ON)
if(LLAMA_VERA_BUILD_EXAMPLE)
    find_package(llama CONFIG QUIET)
    if(llama_FOUND)
        add_subdirectory(examples/llama-vera)
    else()
        message(WARNING
            "llama.cpp was not found (find_package(llama CONFIG)); skipping examples/llama-vera. "
            "Point CMAKE_PREFIX_PATH at a llama.cpp install, or configure with -DLLAMA_VERA_BUILD_EXAMPLE=OFF.")
    endif()
endif()
