enable_testing()

# The headless tests probably don't work without an actual monitor / GPU
# so it might be easier to disable this on CI environments
option(TEST_OPENGL_OPERATIONS "Run headless OpenGL tests" ON)
option(TEST_OPENGL_WITH_VISIBLE_WINDOW "Test creating a window and drawing to it" OFF)
set(TEST_FILES main.cpp fixed_point.cpp operations.cpp thread_pool.cpp)
# The tests use GLFW, which is not relevant on Android
if (WITH_OPENGL AND TEST_OPENGL_OPERATIONS)
  list(APPEND TEST_FILES opengl.cpp)
  add_definitions("-DTEST_WITH_OPENGL")
  if (TEST_OPENGL_WITH_VISIBLE_WINDOW)
    add_definitions("-DTEST_OPENGL_WITH_VISIBLE_WINDOW")
  endif()
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
