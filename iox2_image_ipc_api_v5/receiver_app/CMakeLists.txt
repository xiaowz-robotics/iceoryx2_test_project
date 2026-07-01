cmake_minimum_required(VERSION 3.16)
project(receiver_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IPC_LIB_INSTALL "${CMAKE_CURRENT_SOURCE_DIR}/../iox2_image_ipc_lib/install" CACHE PATH "iox2_image_ipc install prefix")

add_executable(receiver_app src/receiver_app.cpp)

target_include_directories(receiver_app PRIVATE
    "${IPC_LIB_INSTALL}/include"
)

target_link_directories(receiver_app PRIVATE
    "${IPC_LIB_INSTALL}/lib"
)

target_link_libraries(receiver_app PRIVATE iox2_image_ipc)

set_target_properties(receiver_app PROPERTIES
    BUILD_RPATH "${IPC_LIB_INSTALL}/lib"
    INSTALL_RPATH "${IPC_LIB_INSTALL}/lib"
)
