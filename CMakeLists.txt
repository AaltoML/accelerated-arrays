cmake_minimum_required(VERSION 3.0)
set(LIBNAME accelerated-arrays)
project(${LIBNAME})

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wextra -Wall -pedantic")
find_package(Threads REQUIRED)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebSymbols)
endif()

add_library(${LIBNAME} src/image.cpp src/function.cpp src/cpu_image.cpp src/cpu_ops.cpp src/future.cpp)
set_target_properties(${LIBNAME} PROPERTIES PUBLIC_HEADER "src/image.hpp;src/cpu_image.hpp")
target_link_libraries(${LIBNAME} Threads::Threads)

install(TARGETS ${LIBNAME}
  ARCHIVE DESTINATION lib
  PUBLIC_HEADER DESTINATION include/${LIBNAME})

add_subdirectory(test)
