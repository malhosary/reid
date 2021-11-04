# create by Ricardo Lu in 11/04/2021

cmake_minimum_required(VERSION 3.10)

project(ReID-test)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAS} -Wall -O3")

include_directories(
    .
    /opt/thundersoft/common
    ${OpenCV_INCLUDE_DIRS}
    ${GST_INCLUDE_DIRS}
    ${GSTAPP_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${JSON_INCLUDE_DIRS}
    ${GFLAGS_INCLUDE_DIRS}
    ${DeepStream_INCLUDE_DIRS}
    ${UUID_INCLUDE_DIRS}
)

link_directories(
    ${GST_LIBRARY_DIRS}
    ${GSTAPP_LIBRARY_DIRS}
    ${GLIB_LIBRARY_DIRS}
    ${JSON_LIBRARY_DIRS}
    ${GFLAGS_LIBRARY_DIRS}
    ${DeepStream_LIBRARY_DIRS}
    ${UUID_LIBRARY_DIRS}
)

add_executable(${PROJECT_NAME}
    VideoAnalyzer.cpp
    VideoPipeline.cpp
    ../AlgReID.cpp
    main.cpp
)

target_link_libraries(${PROJECT_NAME}
    ${GST_LIBRARIES}
    ${GSTAPP_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${JSON_LIBRARIES}
    ${GFLAGS_LIBRARIES}
    pthread
    nvdsgst_meta
    nvds_meta
    nvds_utils
    TSObjectReID
    dl
)