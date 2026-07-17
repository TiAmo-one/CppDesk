find_package(websocketpp REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(signal-server src/main.cpp src/server.cpp)
target_link_libraries(signal-server PRIVATE websocketpp::websocketpp nlohmann_json::nlohmann_json)
