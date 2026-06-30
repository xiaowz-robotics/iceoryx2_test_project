cmake_minimum_required(VERSION 3.16)
project(iceoryx2_stress_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IOX2_INSTALL "$ENV{HOME}/IPC/iceoryx2/build/install" CACHE PATH "iceoryx2 install prefix")
list(APPEND CMAKE_PREFIX_PATH "${IOX2_INSTALL}")

find_package(iceoryx2-cxx REQUIRED)

set(IOX2_INCLUDE_DIR "${IOX2_INSTALL}/include/iceoryx2/v0.9.0")

add_executable(image_sender sender.cpp)
target_include_directories(image_sender PRIVATE ${IOX2_INCLUDE_DIR})
target_link_libraries(image_sender PRIVATE iceoryx2-cxx::static-lib-cxx)

add_executable(image_receiver receiver.cpp)
target_include_directories(image_receiver PRIVATE ${IOX2_INCLUDE_DIR})
target_link_libraries(image_receiver PRIVATE iceoryx2-cxx::static-lib-cxx)
