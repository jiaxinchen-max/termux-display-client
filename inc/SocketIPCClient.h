#pragma once

#include <cstdint>
#include <string>
#include <android/hardware_buffer.h>

class SocketIPCClient {
public:
    static SocketIPCClient *getInstance();

    int init(AHardwareBuffer *hwBuffer, int dataSocket);

    int draw();

private:
    SocketIPCClient() = default;

    static SocketIPCClient s_Renderer;

    AHardwareBuffer *buffer;

    int cur = 0;
};
