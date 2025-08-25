cmake_minimum_required(VERSION 3.10.1)
project(termux-display-client)

find_package(x11 REQUIRED)
find_package(xserver REQUIRED)

include_directories(inc)
include_directories(include)
include_directories(${X11_INCLUDE_DIR})
include_directories(${XSERVER_INCLUDE_DIR})

add_library(termux-display-lorie
        SHARED
        renderer/buffer.c
)
target_link_libraries(termux-display-lorie
        ${XSERVER_LIBS}
        ${X11_LIBS}
        log
        android
)

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
