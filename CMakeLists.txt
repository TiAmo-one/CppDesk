cmake_minimum_required(VERSION 3.20)
add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
project(remote-desktop VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain")
endif()

option(BUILD_TESTING "Build tests" ON)
include(CTest)

add_subdirectory(libs/libproto)
add_subdirectory(libs/libcapture)
#add_subdirectory(libs/libencode)  # requires x264
add_subdirectory(libs/libnetwork)
add_subdirectory(libs/libinput)
add_subdirectory(libs/libclipboard)
add_subdirectory(libs/libfiletransfer)
add_subdirectory(libs/libui)
add_subdirectory(apps/agent)
add_subdirectory(apps/controller)
add_subdirectory(apps/signal-server)
