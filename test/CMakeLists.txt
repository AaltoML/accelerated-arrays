enable_testing()

set(TEST_FILES main.cpp thread_pool.cpp)
if (WITH_OPENGL)
  list(APPEND TEST_FILES opengl.cpp)
endif()

add_executable(run-tests ${TEST_FILES})
target_include_directories(run-tests PRIVATE
  Catch2/single_include
  ../src/)
target_link_libraries(run-tests accelerated-arrays)
add_test(NAME run-tests COMMAND run-tests)
