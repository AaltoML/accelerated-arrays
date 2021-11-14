cmake_minimum_required(VERSION 3.0)
set(LIBNAME accelerated-arrays)
project(${LIBNAME})
set(PROJECT_VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 14)

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pedantic")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wextra -Wall -pedantic")
endif()
find_package(Threads REQUIRED)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(WITH_OPENGL "Compile with OpenGL support" ON)
option(WITH_OPENGL_ES "Use OpenGL ES" OFF)
option(VERBOSE_LOGGING "Verbose logging (LOG_TRACE)" OFF)

set(SRC_FILES
    src/cpu/image.cpp
    src/cpu/operations.cpp
    src/future.cpp
    src/function.cpp
    src/image.cpp
    src/log_and_assert.cpp
    src/queue.cpp
    src/standard_ops.cpp
)

# a bit tedious to list all these manually
install(FILES
  src/fixed_point.hpp
  src/function.hpp
  src/future.hpp
  src/image.hpp
  src/standard_ops.hpp
  src/assert.hpp
  src/opencv_adapter.hpp # note: optional, no hard depdendency to OpenCV
  DESTINATION include/${LIBNAME}
  COMPONENT Headers)

install(FILES
  src/cpu/image.hpp
  src/cpu/operations.hpp
  DESTINATION include/${LIBNAME}/cpu
  COMPONENT Headers)

set(LIBRARY_DEPS Threads::Threads)
if (ANDROID)
  list(APPEND LIBRARY_DEPS log)
endif()

if (WITH_OPENGL)
  # TODO: iOS
  if (NOT ANDROID)
    set(OpenGL_GL_PREFERENCE LEGACY)
    find_package(OpenGL REQUIRED)
  endif()

  if(APPLE)
    message("Silencing Apple OpenGL API deprecation warnings")
    add_definitions(-DGL_SILENCE_DEPRECATION)
    list(APPEND LIBRARY_DEPS ${OPENGL_LIBRARIES})
    find_package(glfw3 REQUIRED)
  endif()

  list(APPEND SRC_FILES
    src/opengl/adapters.cpp
    src/opengl/image.cpp
    src/opengl/operations.cpp
    src/opengl/read_adapters.cpp
    src/opengl/texture_formats.cpp
  )

  # Note: omitting some internals on purpose
  install(FILES
    src/opengl/image.hpp
    src/opengl/operations.hpp
    DESTINATION include/${LIBNAME}/opengl
    COMPONENT Headers)

  if (NOT ANDROID)
    list(APPEND SRC_FILES src/opengl/glfw.cpp)
    list(APPEND LIBRARY_DEPS glfw)
  endif()

  if (WITH_OPENGL_ES)
    if (ANDROID)
      list(APPEND LIBRARY_DEPS GLESv3)
    else()
      list(APPEND LIBRARY_DEPS EGL) # TODO check
      #list(APPEND LIBRARY_DEPS GL)
      list(APPEND LIBRARY_DEPS GLESv2)
    endif()
  else()
    if (NOT APPLE)
      if(ANDROID)
        list(APPEND LIBRARY_DEPS GL)
      else()
        list(APPEND LIBRARY_DEPS ${OPENGL_LIBRARIES})
      endif()
    endif()
  endif()
endif()

add_library(${LIBNAME} ${SRC_FILES})
# TODO: public header part
target_link_libraries(${LIBNAME} ${LIBRARY_DEPS})

if (WITH_OPENGL_ES)
  target_compile_definitions(${LIBNAME} PRIVATE "-DACCELERATED_ARRAYS_USE_OPENGL_ES")
endif()

if (ANDROID)
  target_compile_definitions(${LIBNAME} PRIVATE "-DACCELERATED_ARRAYS_MAX_COMPATIBILITY_READS")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  target_compile_definitions(${LIBNAME} PRIVATE "-DACCELERATED_ARRAYS_SYNC_GLFW")
endif()

if (VERBOSE_LOGGING)
  target_compile_definitions(${LIBNAME} PRIVATE "-DACCELERATED_ARRAYS_LOG_TRACE")
endif()

# Note, also consider using these flags
# ACCELERATED_ARRAYS_DODGY_READS
# ACCELERATED_ARRAYS_MAX_COMPATIBILITY_READS

install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${LIBNAME}/${LIBNAME}ConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

export(EXPORT ${PROJECT_NAME}Targets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/${LIBNAME}/${PROJECT_NAME}Targets.cmake"
  NAMESPACE ${LIBNAME}::
)
configure_file(cmake/${PROJECT_NAME}Config.cmake
  "${CMAKE_CURRENT_BINARY_DIR}/${LIBNAME}/${PROJECT_NAME}Config.cmake"
  COPYONLY
)

set(ConfigPackageLocation lib/cmake/${LIBNAME})
install(EXPORT ${PROJECT_NAME}Targets
  FILE
    ${PROJECT_NAME}Targets.cmake
  NAMESPACE
    ${LIBNAME}::
  DESTINATION
    ${ConfigPackageLocation}
)
install(
  FILES
    cmake/${PROJECT_NAME}Config.cmake
    "${CMAKE_CURRENT_BINARY_DIR}/${LIBNAME}/${PROJECT_NAME}ConfigVersion.cmake"
  DESTINATION
    ${ConfigPackageLocation}
  COMPONENT
    Devel
)

add_subdirectory(test)
