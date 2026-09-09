cmake_minimum_required(VERSION 3.14)
project(llama-vera CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(llama CONFIG REQUIRED)

if(DEFINED LLAMA_LIB_DIR)
    set(CMAKE_BUILD_RPATH "${LLAMA_LIB_DIR}")
endif()

add_library(vera-semantic
    src/semantic_sampler.cpp
    src/vera_analyzer.cpp
)
target_include_directories(vera-semantic PUBLIC src)
target_link_libraries(vera-semantic PUBLIC llama)

add_executable(llama-vera src/main.cpp)
target_link_libraries(llama-vera PRIVATE vera-semantic)
if(UNIX AND NOT APPLE AND DEFINED LLAMA_LIB_DIR)
    target_link_options(llama-vera PRIVATE "-Wl,--disable-new-dtags")
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(vera-analyzer-test
        tests/test_vera_analyzer.cpp
        src/vera_analyzer.cpp
    )
    target_include_directories(vera-analyzer-test PRIVATE src)
    add_test(NAME vera-analyzer COMMAND vera-analyzer-test)

    set(LLAMA_VERA_TEST_MODEL "" CACHE FILEPATH "GGUF model used by semantic sampler integration tests")
    if(LLAMA_VERA_TEST_MODEL)
        add_executable(semantic-filter-test tests/test_semantic_filter.cpp)
        target_link_libraries(semantic-filter-test PRIVATE vera-semantic)
        if(UNIX AND NOT APPLE AND DEFINED LLAMA_LIB_DIR)
            target_link_options(semantic-filter-test PRIVATE "-Wl,--disable-new-dtags")
        endif()
        add_test(NAME semantic-filter COMMAND semantic-filter-test ${LLAMA_VERA_TEST_MODEL})
    endif()
endif()
