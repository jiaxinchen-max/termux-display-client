cmake_minimum_required(VERSION 3.22.1)

project("termux-wayland")
add_library(termux-buffer
        SHARED
        buffer.c
)
target_link_libraries(termux-buffer
        android
        log
        m
        z
        EGL
        GLESv2
)
add_library(${CMAKE_PROJECT_NAME} SHARED
        # List C/C++ source files with relative paths to this CMakeLists.txt.
        wayland.c)

target_link_libraries(${CMAKE_PROJECT_NAME}
        # List libraries link to the target library
        PRIVATE termux-buffer
)
install(TARGETS ${CMAKE_PROJECT_NAME} LIBRARY)
install(TARGETS termux-buffer LIBRARY)
install(DIRECTORY include/ DESTINATION include/termux/render)

add_executable(termux-render-daemon main.c)
target_link_libraries(termux-render-daemon
        ${CMAKE_PROJECT_NAME}
        )
install(TARGETS termux-render-daemon RUNTIME)
