find_package(nlohmann_json REQUIRED)
add_executable(signal-server src/main.cpp src/server.cpp)
target_link_libraries(signal-server PRIVATE nlohmann_json::nlohmann_json ws2_32)
