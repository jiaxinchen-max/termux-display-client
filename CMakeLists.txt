cmake_minimum_required(VERSION 3.10.1)
project(termux-display-client)

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBPIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)


include_directories(inc)
include_directories(include)
include_directories(xserver/include)

add_library(termux-display-client
        SHARED
        renderer/buffer.c
        renderer/client.cpp
        renderer/SocketIPCClient.cpp
        renderer/termuxdc_server.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

target_link_libraries(termux-display-client
        PRIVATE PkgConfig::X11
        PRIVATE PkgConfig::LIBPIXMAN
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
