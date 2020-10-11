enable_testing()

set(TEST_FILES main.cpp convolution.cpp fixed_point.cpp thread_pool.cpp)
if (WITH_OPENGL)
  list(APPEND TEST_FILES opengl.cpp)
  add_definitions("-DTEST_WITH_OPENGL")
endif()

if (WITH_OPENGL_ES)
  add_definitions("-DACCELERATED_ARRAYS_USE_OPENGL_ES")
endif()

add_executable(run-tests ${TEST_FILES})
target_include_directories(run-tests PRIVATE
  Catch2/single_include
  ../src/)
target_link_libraries(run-tests accelerated-arrays)
add_test(NAME run-tests COMMAND run-tests)
