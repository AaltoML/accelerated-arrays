cmake_minimum_required(VERSION 3.0)
set(LIBNAME accelerated-arrays)
project(${LIBNAME})

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wextra -Wall -pedantic")
find_package(Threads REQUIRED)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebSymbols)
endif()

option(WITH_OPENGL "Compile with OpenGL support" ON)
option(WITH_OPENGL_ES "Use OpenGL ES" OFF)

set(SRC_FILES
    src/cpu/image.cpp
    src/cpu/operations.cpp
    src/future.cpp
    src/function.cpp
    src/image.cpp
    src/queue.cpp
)

set(LIBRARY_DEPS Threads::Threads)

if (WITH_OPENGL)
  find_package(OpenGL REQUIRED)
  list(APPEND SRC_FILES
    src/opengl/adapters.cpp
    src/opengl/image.cpp
    src/opengl/operations.cpp
    src/opengl/read_adapters.cpp
    src/opengl/glfw.cpp # TODO: not on mobile
    src/opengl/texture_formats.cpp
  )

  list(APPEND LIBRARY_DEPS glfw) # TODO: not on mobile

  if (WITH_OPENGL_ES)
    list(APPEND LIBRARY_DEPS EGL) # TODO check
    #list(APPEND LIBRARY_DEPS GL)
    list(APPEND LIBRARY_DEPS GLESv2)
    #list(APPEND LIBRARY_DEPS GLESv3) # TODO
  else()
    list(APPEND LIBRARY_DEPS GL)
  endif()
endif()

add_library(${LIBNAME} ${SRC_FILES})
# TODO: public header part
target_link_libraries(${LIBNAME} ${LIBRARY_DEPS})

if (WITH_OPENGL_ES)
  target_compile_definitions(${LIBNAME} PRIVATE "-DACCELERATED_ARRAYS_USE_OPENGL_ES")
endif()

# Note, also consider using these flags
# ACCELERATED_ARRAYS_DODGY_READS
# ACCELERATED_ARRAYS_MAX_COMPATIBILITY_READS

install(TARGETS ${LIBNAME}
  ARCHIVE DESTINATION lib
  PUBLIC_HEADER DESTINATION include/${LIBNAME})

add_subdirectory(test)
