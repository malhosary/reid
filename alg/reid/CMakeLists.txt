# create by Ricardo Lu in 11/04/2021

cmake_minimum_required(VERSION 3.10)

project(AlgReID)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O3")

include(FindPkgConfig)
pkg_check_modules(GST    REQUIRED gstreamer-1.0)
pkg_check_modules(OpenCV REQUIRED opencv4)
pkg_check_modules(GSTAPP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GLIB   REQUIRED glib-2.0)
pkg_check_modules(JSON   REQUIRED json-glib-1.0)
pkg_check_modules(UUID   REQUIRED uuid)
pkg_check_modules(GFLAGS REQUIRED gflags)

set(DeepStream_ROOT "/opt/nvidia/deepstream/deepstream-6.0")
set(DeepStream_INCLUDE_DIRS "${DeepStream_ROOT}/sources/includes")
set(DeepStream_LIBRARY_DIRS "${DeepStream_ROOT}/lib")

message(STATUS "GST:   ${GST_INCLUDE_DIRS},${GST_LIBRARY_DIRS},${GST_LIBRARIES}")
message(STATUS "GSTAPP:${GSTAPP_INCLUDE_DIRS},${GSTAPP_LIBRARY_DIRS},${GSTAPP_LIBRARIES}")
message(STATUS "GLIB:  ${GLIB_INCLUDE_DIRS},${GLIB_LIBRARY_DIRS},${GLIB_LIBRARIES}")
message(STATUS "JSON:  ${JSON_INCLUDE_DIRS},${JSON_LIBRARY_DIRS},${JSON_LIBRARIES}")
message(STATUS "UUID:  ${UUID_INCLUDE_DIRS},${UUID_LIBRARY_DIRS},${UUID_LIBRARIES}")
message(STATUS "GFLAGS:${GFLAGS_INCLUDE_DIRS},${GFLAGS_LIBRARY_DIRS},${GFLAGS_LIBRARIES}")
message(STATUS "OpenCV:${OpenCV_INCLUDE_DIRS},${OpenCV_LIBRARY_DIRS},${OpenCV_LIBRARIES}")
message(STATUS "DeepStream: ${DeepStream_INCLUDE_DIRS}, ${DeepStream_LIBRARY_DIRS}")

include_directories(
    .
    /opt/thundersoft/algs/include
    ${OpenCV_INCLUDE_DIRS}
    ${GST_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${JSON_INCLUDE_DIRS}
)

link_directories(
    /opt/thundersoft/algs/lib
    ${GST_LIBRARY_DIRS}
    ${GLIB_LIBRARY_DIRS}
    ${JSON_LIBRARY_DIRS}
)

add_library(${PROJECT_NAME}
    SHARED
    AlgReID.cpp
)

target_link_libraries(${PROJECT_NAME}
    ${GLIB_LIBRARIES}
    ${JSON_LIBRARIES}
    ${OpenCV_LIBRARIES}
)

install(
    TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION /opt/thundersoft/algs/lib
)

install(
    FILES reid.json
    DESTINATION /opt/thundersoft/algs/configs
)

install(
    FILES common.h
    DESTINATION /opt/thundersoft/common
)

# test program
add_subdirectory(test)