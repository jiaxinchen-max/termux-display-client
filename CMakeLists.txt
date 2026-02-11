cmake_minimum_required(VERSION 3.22.1)

project("wayland")
add_library(termux-display-lorie
        SHARED
        buffer.c
)
target_link_libraries(termux-display-lorie
        android
        log
        m
        z
        EGL
        GLESv2
)
add_library(${CMAKE_PROJECT_NAME} SHARED
        # List C/C++ source files with relative paths to this CMakeLists.txt.
        buffer.c
        wayland.c)

target_link_libraries(${CMAKE_PROJECT_NAME}
        # List libraries link to the target library
        PRIVATE termux-display-lorie
)
install(TARGETS termux-display-client LIBRARY)
install(TARGETS termux-display-lorie LIBRARY)
install(DIRECTORY include/ DESTINATION include/termux/display/client)

add_executable(termux-display-client-entity renderer/main.c)
target_link_libraries(termux-display-client-entity
        termux-display-client
        log
        dl
        android)
install(TARGETS termux-display-client-entity RUNTIME)
