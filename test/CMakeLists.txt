enable_testing()
add_executable(run-tests main.cpp)
target_include_directories(run-tests PRIVATE
  Catch2/single_include
  ../src/)
target_link_libraries(run-tests accelerated-arrays)
add_test(NAME run-tests COMMAND run-tests)
