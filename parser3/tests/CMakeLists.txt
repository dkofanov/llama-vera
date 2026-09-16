add_executable(parser3-test-matching test_matching.cpp)
target_link_libraries(parser3-test-matching PRIVATE parser3)
add_test(NAME matching COMMAND parser3-test-matching)

add_executable(parser3-test-stream test_stream.cpp)
target_link_libraries(parser3-test-stream PRIVATE parser3)
add_test(NAME stream COMMAND parser3-test-stream)

add_executable(parser3-test-optional test_optional.cpp)
target_link_libraries(parser3-test-optional PRIVATE parser3)
add_test(NAME optional COMMAND parser3-test-optional)

add_executable(parser3-test-context test_context.cpp)
target_link_libraries(parser3-test-context PRIVATE parser3)
add_test(NAME context COMMAND parser3-test-context)

add_executable(parser3-test-signal test_signal.cpp)
target_link_libraries(parser3-test-signal PRIVATE parser3)
add_test(NAME signal COMMAND parser3-test-signal)

add_executable(parser3-test-class test_class.cpp)
target_link_libraries(parser3-test-class PRIVATE parser3)
add_test(NAME class COMMAND parser3-test-class)

add_executable(parser3-verify-class verify_class.cpp)
target_link_libraries(parser3-verify-class PRIVATE parser3)
