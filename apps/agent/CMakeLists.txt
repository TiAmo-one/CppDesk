find_package(nlohmann_json REQUIRED)
add_executable(agent src/main.cpp src/agent.cpp)
target_include_directories(agent PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/libcapture/include
    ${CMAKE_SOURCE_DIR}/libs/libencode/include
    ${CMAKE_SOURCE_DIR}/libs/libnetwork/include
    ${CMAKE_SOURCE_DIR}/libs/libinput/include
    ${CMAKE_SOURCE_DIR}/libs/libclipboard/include
    ${CMAKE_SOURCE_DIR}/libs/libfiletransfer/include
    ${CMAKE_SOURCE_DIR}/libs/libproto/include
)
target_link_libraries(agent PRIVATE
    libcapture libencode libnetwork libinput libclipboard libfiletransfer libproto
    nlohmann_json::nlohmann_json sodium
)
