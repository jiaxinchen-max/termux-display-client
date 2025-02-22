cmake_minimum_required(VERSION 3.10.1)
project(termux-display-client)
include_directories(inc)
include_directories(include)
add_library(termux-display-client
        SHARED
        renderer/client.cpp
        renderer/SocketIPCClient.cpp
        renderer/termuxdc_server.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

target_link_libraries(termux-display-client
        log
        android
)
install(TARGETS termux-display-client LIBRARY)
install(DIRECTORY include/ DESTINATION include/termux/display/client)

add_executable(termux-display-client-entity renderer/main.c)
target_link_libraries(termux-display-client-entity
        termux-display-client
        log
        dl
        android)
install(TARGETS termux-display-client-entity RUNTIME)
