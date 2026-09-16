# Example harness: drives a llama.cpp completion with parser3 constraints.
if(NOT TARGET llama)
    find_package(llama CONFIG REQUIRED)
endif()

if(DEFINED LLAMA_LIB_DIR)
    set(CMAKE_BUILD_RPATH "${LLAMA_LIB_DIR}")
endif()

# Core: a parser3 instance for VERA, with no llama.cpp dependency.
add_library(parser3-vera src/parser3_vera.cpp)
target_include_directories(parser3-vera PUBLIC src)
target_link_libraries(parser3-vera PUBLIC parser3)

# Adapter: the llama.cpp sampler that wraps the core.
add_library(llama-vera-sampler src/llama_vera_sampler.cpp)
target_link_libraries(llama-vera-sampler PUBLIC llama parser3-vera)

add_executable(llama-vera src/main.cpp)
target_link_libraries(llama-vera PRIVATE llama-vera-sampler)
if(UNIX AND NOT APPLE AND DEFINED LLAMA_LIB_DIR)
    target_link_options(llama-vera PRIVATE "-Wl,--disable-new-dtags")
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(parser3-vera-test tests/test_parser3_vera.cpp)
    target_link_libraries(parser3-vera-test PRIVATE parser3-vera)
    add_test(NAME parser3-vera COMMAND parser3-vera-test)

    set(LLAMA_VERA_TEST_MODEL "" CACHE FILEPATH "GGUF model used by the sampler integration test")
    if(LLAMA_VERA_TEST_MODEL)
        add_executable(llama-vera-sampler-test tests/test_llama_vera_sampler.cpp)
        target_link_libraries(llama-vera-sampler-test PRIVATE llama-vera-sampler)
        if(UNIX AND NOT APPLE AND DEFINED LLAMA_LIB_DIR)
            target_link_options(llama-vera-sampler-test PRIVATE "-Wl,--disable-new-dtags")
        endif()
        add_test(NAME llama-vera-sampler COMMAND llama-vera-sampler-test ${LLAMA_VERA_TEST_MODEL})
    endif()
endif()
