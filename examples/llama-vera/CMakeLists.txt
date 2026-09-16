# Example harness: drives a llama.cpp completion with parser3 constraints, and
# owns the VERA grammar (full + class semantics) that the harness is built on.
if(NOT TARGET llama)
    find_package(llama CONFIG REQUIRED)
endif()

if(DEFINED LLAMA_LIB_DIR)
    set(CMAKE_BUILD_RPATH "${LLAMA_LIB_DIR}")
endif()

# The VERA grammar headers are build artifacts, generated here with parser3's
# tool into parser3-generated/ (the same prefix the library uses for its own
# test fixtures).
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(VERA_GRAMMAR_GEN "${CMAKE_SOURCE_DIR}/parser3/tools/grammar_gen.py")
set(VERA_GENERATED_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/include")
set(VERA_GENERATED_DIR "${VERA_GENERATED_INCLUDE}/parser3-generated")

function(vera_generate_grammar source output namespace)
    set(out "${VERA_GENERATED_DIR}/${output}")
    set(deps "")
    foreach(dep IN LISTS ARGN)
        list(APPEND deps "${CMAKE_CURRENT_SOURCE_DIR}/${dep}")
    endforeach()
    add_custom_command(
        OUTPUT "${out}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${VERA_GENERATED_DIR}"
        COMMAND "${Python3_EXECUTABLE}" "${VERA_GRAMMAR_GEN}"
                "${source}" -n "${namespace}" -o "${out}"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
            "${VERA_GRAMMAR_GEN}"
            ${deps}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Generating ${output}"
        VERBATIM)
    set(VERA_GENERATED_OUTPUTS "${VERA_GENERATED_OUTPUTS};${out}" PARENT_SCOPE)
endfunction()

set(VERA_GENERATED_OUTPUTS "")
vera_generate_grammar(grammars/full.grammar full_grammar.h vera::parser3::full)
vera_generate_grammar(grammars/full_sem.grammar full_sem_grammar.h vera::parser3::full_sem
    grammars/full_semantics.h)
add_custom_target(llama-vera-generated DEPENDS ${VERA_GENERATED_OUTPUTS})

# Core: a parser3 instance for VERA, with no llama.cpp dependency.
add_library(parser3-vera src/parser3_vera.cpp)
target_include_directories(parser3-vera PUBLIC src "${VERA_GENERATED_INCLUDE}")
target_link_libraries(parser3-vera PUBLIC parser3)
add_dependencies(parser3-vera llama-vera-generated)

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

    # The VERA grammar and semantic tests live with the harness.
    add_executable(llama-vera-class-test tests/test_class.cpp)
    target_link_libraries(llama-vera-class-test PRIVATE parser3-vera)
    add_dependencies(llama-vera-class-test llama-vera-generated)
    add_test(NAME llama-vera-class COMMAND llama-vera-class-test)

    add_executable(llama-vera-verify-class tests/verify_class.cpp)
    target_link_libraries(llama-vera-verify-class PRIVATE parser3-vera)
    add_dependencies(llama-vera-verify-class llama-vera-generated)

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

option(LLAMA_VERA_BUILD_BENCHMARKS "Build the harness benchmarks" ON)
if(LLAMA_VERA_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
