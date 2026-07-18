find_package(FFMPEG REQUIRED)
find_package(nlohmann_json REQUIRED)
add_executable(controller WIN32 src/main.cpp src/controller.cpp)
target_include_directories(controller PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/libui/include
    ${CMAKE_SOURCE_DIR}/libs/libnetwork/include
    ${CMAKE_SOURCE_DIR}/libs/libclipboard/include
    ${CMAKE_SOURCE_DIR}/libs/libfiletransfer/include
    ${CMAKE_SOURCE_DIR}/libs/libproto/include
    ${CMAKE_SOURCE_DIR}/libs/libinput/include
    ${FFMPEG_INCLUDE_DIRS}
    C:/vcpkg-build/installed/x64-windows/include
)
target_link_directories(controller PRIVATE C:/vcpkg-build/installed/x64-windows/lib C:/vcpkg-build/packages/libsodium_x64-windows/lib)
target_link_libraries(controller PRIVATE
    libui libnetwork libclipboard libfiletransfer libproto libinput
    ${FFMPEG_LIBRARIES}
    nlohmann_json::nlohmann_json libsodium
)
# Auto-copy runtime DLLs to output
add_custom_command(TARGET controller POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/vcpkg-build/installed/x64-windows/bin/avcodec-62.dll"
        $<TARGET_FILE_DIR:controller>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/vcpkg-build/installed/x64-windows/bin/avdevice-62.dll"
        $<TARGET_FILE_DIR:controller>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/vcpkg-build/installed/x64-windows/bin/avfilter-11.dll"
        $<TARGET_FILE_DIR:controller>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/vcpkg-build/installed/x64-windows/bin/avformat-62.dll"
        $<TARGET_FILE_DIR:controller>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/vcpkg-build/installed/x64-windows/bin/avutil-60.dll"
        $<TARGET_FILE_DIR:controller>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/vcpkg-build/installed/x64-windows/bin/swresample-6.dll"
        $<TARGET_FILE_DIR:controller>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/vcpkg-build/installed/x64-windows/bin/swscale-9.dll"
        $<TARGET_FILE_DIR:controller>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/vcpkg-build/installed/x64-windows/bin/libsodium.dll"
        $<TARGET_FILE_DIR:controller>
    COMMENT "Copying runtime DLLs to controller output"
)