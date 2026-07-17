find_package(FFMPEG REQUIRED COMPONENTS avcodec avutil swscale)
find_package(nlohmann_json REQUIRED)
add_executable(controller src/main.cpp src/controller.cpp)
target_include_directories(controller PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/libui/include
    ${CMAKE_SOURCE_DIR}/libs/libnetwork/include
    ${CMAKE_SOURCE_DIR}/libs/libclipboard/include
    ${CMAKE_SOURCE_DIR}/libs/libfiletransfer/include
    ${CMAKE_SOURCE_DIR}/libs/libproto/include
    ${CMAKE_SOURCE_DIR}/libs/libinput/include
)
target_link_libraries(controller PRIVATE
    libui libnetwork libclipboard libfiletransfer libproto libinput
    FFMPEG::avcodec FFMPEG::avutil FFMPEG::swscale
    nlohmann_json::nlohmann_json sodium
)
