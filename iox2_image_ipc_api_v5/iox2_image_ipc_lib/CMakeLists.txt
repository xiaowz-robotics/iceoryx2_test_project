cmake_minimum_required(VERSION 3.16)
project(iox2_image_ipc_lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(IOX2_INSTALL "$ENV{HOME}/IPC/iceoryx2/build/install" CACHE PATH "iceoryx2 install prefix")
list(APPEND CMAKE_PREFIX_PATH "${IOX2_INSTALL}")

find_package(iceoryx2-cxx REQUIRED)

add_library(iox2_image_ipc SHARED
    src/iox2_image_ipc.cpp
)

target_include_directories(iox2_image_ipc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(iox2_image_ipc
    PRIVATE
        iceoryx2-cxx::shared-lib-cxx
)

set_target_properties(iox2_image_ipc PROPERTIES
    OUTPUT_NAME iox2_image_ipc
    VERSION 1.0.0
    SOVERSION 1
    INSTALL_RPATH "${IOX2_INSTALL}/lib"
    BUILD_RPATH "${IOX2_INSTALL}/lib"
)

install(TARGETS iox2_image_ipc
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
)
