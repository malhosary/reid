# create by Ricardo Lu in 10/25/2021

cmake_minimum_required(VERSION 3.10)

project(spl-test)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O3")

include_directories(
    .
)

add_executable(${PROJECT_NAME}
    main.cpp
)

target_link_libraries(${PROJECT_NAME}
    SplReID
    ${OpenCV_LIBRARIES}
    ${GST_LIBRARIES}
    ${GSTAPP_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${JSON_LIBRARIES}
    ${GFLAGS_LIBRARIES}
    ${UUID_LIBRARIES}
    nvdsgst_meta
    nvds_meta
    nvds_utils
    pthread
    osdyuv
    nvbufsurface
)