cmake_minimum_required(VERSION 3.4.1)

include_directories(inc)
include_directories(include)
add_library(termux-display-client
        SHARED
        renderer/client.cpp
        renderer/SocketIPCClient.cpp
        renderer/InputServer.cpp
        renderer/InputEventCallback.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

target_link_libraries(termux-display-client
        log
        android
)
install(TARGETS termux-display-client LIBRARY)
install(DIRECTORY include/ DESTINATION include/termux/display/client)

add_executable(termux-display-client-entity renderer/main.cpp)
target_link_libraries(termux-display-client-entity
        termux-display-client
        log
        android)
install(TARGETS termux-display-client-entity BIN)