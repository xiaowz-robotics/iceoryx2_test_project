cmake_minimum_required(VERSION 3.16)
project(sender_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IPC_LIB_INSTALL "${CMAKE_CURRENT_SOURCE_DIR}/../iox2_image_ipc_lib/install" CACHE PATH "iox2_image_ipc install prefix")

add_executable(sender_app src/sender_app.cpp)

target_include_directories(sender_app PRIVATE
    "${IPC_LIB_INSTALL}/include"
)

target_link_directories(sender_app PRIVATE
    "${IPC_LIB_INSTALL}/lib"
)

target_link_libraries(sender_app PRIVATE iox2_image_ipc)

set_target_properties(sender_app PROPERTIES
    BUILD_RPATH "${IPC_LIB_INSTALL}/lib"
    INSTALL_RPATH "${IPC_LIB_INSTALL}/lib"
)
