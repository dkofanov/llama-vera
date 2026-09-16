cmake_minimum_required(VERSION 3.14)
project(parser3 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Grammar headers are build artifacts: they are generated from grammars/*.grammar
# into the build tree under the parser3-generated/ include prefix and are never
# installed.  The hand-written core headers below are the library.
set(PARSER3_GENERATED_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/include")
set(PARSER3_GENERATED_DIR "${PARSER3_GENERATED_INCLUDE}/parser3-generated")

add_library(parser3 INTERFACE)
add_library(parser3::parser3 ALIAS parser3)
target_include_directories(parser3 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PARSER3_GENERATED_INCLUDE}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# When embedded with add_subdirectory the library builds on its own; tests and
# benchmarks default on only in a top-level build.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(PARSER3_TOP_LEVEL ON)
else()
    set(PARSER3_TOP_LEVEL OFF)
endif()

option(PARSER3_BUILD_TESTS "Build parser3 tests" ${PARSER3_TOP_LEVEL})
option(PARSER3_BUILD_BENCHMARKS "Build parser3 benchmarks" ${PARSER3_TOP_LEVEL})
set(PARSER3_BENCH_UNITS "64" CACHE STRING "Root unit count for parser3-bench")

# Release is built at -O2: -O3 aggressively inlines the template-instantiated
# Feed code and is consistently 2-3x slower here. Debug info and frame pointers
# let perf profile the release build directly (no separate configuration).
# These are directory-scoped, so embedding parser3 does not touch the parent's
# flags.
if(PARSER3_BUILD_TESTS OR PARSER3_BUILD_BENCHMARKS)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
    add_compile_options($<$<CONFIG:Release>:-g> $<$<CONFIG:Release>:-fno-omit-frame-pointer>)
endif()

# Generate a grammar header into the build tree.  A grammar may %include
# bindings that the generator inlines, so those are extra dependencies.
function(parser3_generate_grammar source output namespace)
    set(out "${PARSER3_GENERATED_DIR}/${output}")
    set(deps "")
    foreach(dep IN LISTS ARGN)
        list(APPEND deps "${CMAKE_CURRENT_SOURCE_DIR}/${dep}")
    endforeach()
    add_custom_command(
        OUTPUT "${out}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PARSER3_GENERATED_DIR}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/grammar_gen.py"
                "${source}" -n "${namespace}" -o "${out}"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/grammar_gen.py"
            ${deps}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Generating ${output}"
        VERBATIM)
    set(PARSER3_GENERATED_OUTPUTS "${PARSER3_GENERATED_OUTPUTS};${out}" PARENT_SCOPE)
endfunction()

set(PARSER3_GENERATED_OUTPUTS "")
if(PARSER3_BUILD_TESTS OR PARSER3_BUILD_BENCHMARKS)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    parser3_generate_grammar(grammars/full.grammar full_grammar.h vera::parser3::full)
    parser3_generate_grammar(grammars/full_sem.grammar full_sem_grammar.h vera::parser3::full_sem
        grammars/full_semantics.h)
    parser3_generate_grammar(tests/signal.grammar signal_grammar.h vera::parser3::signal_demo)
endif()
add_custom_target(parser3-generated DEPENDS ${PARSER3_GENERATED_OUTPUTS})

if(PARSER3_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(PARSER3_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

install(TARGETS parser3 EXPORT parser3Targets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT parser3Targets
    FILE parser3Targets.cmake
    NAMESPACE parser3::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/parser3)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/parser3Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/parser3Config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/parser3)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/parser3ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/parser3Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/parser3ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/parser3)

find_program(CLANG_FORMAT_EXECUTABLE
    NAMES clang-format clang-format-18 clang-format-17 clang-format-16 clang-format-15 clang-format-14)
if(CLANG_FORMAT_EXECUTABLE)
    file(GLOB_RECURSE PARSER3_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/parser3/*.h")
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${PARSER3_HEADERS}
        VERBATIM)
else()
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format was not found"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM)
endif()
