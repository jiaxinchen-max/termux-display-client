
#include <android/hardware_buffer.h>
#include "SocketIPCClient.h"

#define STB_IMAGE_IMPLEMENTATION

#include "stb_image/stb_image.h"

int SocketIPCClient::init(AHardwareBuffer *hwBuffer, int dataSocket) {
    buffer = hwBuffer;
    return AHardwareBuffer_sendHandleToUnixSocket(hwBuffer, dataSocket);
}

int SocketIPCClient::draw() {
    int ret;
    void *shared_buffer;
    ret = AHardwareBuffer_lock(buffer,
                               AHARDWAREBUFFER_USAGE_CPU_READ_RARELY |
                               AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY,
                               -1, // no fence in demo
                               NULL,
                               &shared_buffer);
    if (ret != 0) {
        printf("%s\n", "Failed to AHardwareBuffer_lock");
        return ret;
    }

    //Produces a gradient pattern, uses shift to set as red, blue, or green
    uint8_t *chs;
    int width, height, channel;
    {
        int id = cur % 3;
        switch (id) {
            case 0: {
                chs = stbi_load("/storage/emulated/0/Download/textures/awesomeface-0.png", &width,
                                &height, &channel,
                                STBI_rgb_alpha);
                break;
            }
            case 1: {
                chs = stbi_load("/storage/emulated/0/Download/textures/awesomeface-1.png", &width,
                                &height, &channel,
                                STBI_rgb_alpha);
                break;
            }
            case 2: {
                chs = stbi_load("/storage/emulated/0/Download/textures/awesomeface-2.png", &width,
                                &height, &channel,
                                STBI_rgb_alpha);
                break;
            }
        }
        cur++;
    }
    // assuming format was set to 4 bytes per pixel and not 565 mode
    memcpy(shared_buffer, chs, width * height * sizeof(uint32_t));
    ret = AHardwareBuffer_unlock(buffer, NULL);
    stbi_image_free(chs);
    return ret;
}

SocketIPCClient SocketIPCClient::s_Renderer{};

SocketIPCClient *SocketIPCClient::getInstance() {
    return &s_Renderer;
}
