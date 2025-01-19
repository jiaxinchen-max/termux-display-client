
#include <android/hardware_buffer.h>
#include "SocketIPCClient.h"
#include "LogUtil.h"

#define STB_IMAGE_IMPLEMENTATION

#include "stb_image/stb_image.h"

void SocketIPCClient::Init(AHardwareBuffer *hwBuffer, int dataSocket) {
    buffer = hwBuffer;
    AHardwareBuffer_sendHandleToUnixSocket(hwBuffer, dataSocket);
}

void SocketIPCClient::Draw() {
    int ret;
    void *shared_buffer;
    ret = AHardwareBuffer_lock(buffer,
                               AHARDWAREBUFFER_USAGE_CPU_WRITE_MASK,
                               -1, // no fence in demo
                               NULL,
                               &shared_buffer);
    if (ret != 0) {
        LOG_E("Failed to AHardwareBuffer_lock");
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
    if (ret != 0) {
        LOG_E("Failed to AHardwareBuffer_unlock");
    }
}

SocketIPCClient SocketIPCClient::s_Renderer{};

SocketIPCClient *SocketIPCClient::GetInstance() {
    return &s_Renderer;
}

void SocketIPCClient::Destroy() {
    DEBUG_LOG();

    LOG_E("Destroy success.");
}

void SocketIPCClient::SetImageGeometry(uint32_t w, uint32_t h, uint32_t ch) {
    m_ImgWidth = w;
    m_ImgHeight = h;
    m_ImgChannel = ch;
}

void SocketIPCClient::Draw(const uint8_t *data) {
    if (m_ImgWidth < 1 ||
        m_ImgHeight < 1) {
        LOG_E("Display Geometry Size Not Set");
        return;
    }
    int ret;
    void *shared_buffer;
    ret = AHardwareBuffer_lock(buffer,
                               AHARDWAREBUFFER_USAGE_CPU_WRITE_MASK,
                               -1, // no fence in demo
                               NULL,
                               &shared_buffer);
    if (ret != 0) {
        LOG_E("Failed to AHardwareBuffer_lock");
    }

    memcpy(shared_buffer, data, m_ImgWidth * m_ImgWidth * sizeof(uint32_t));
    ret = AHardwareBuffer_unlock(buffer, NULL);
    if (ret != 0) {
        LOG_E("Failed to AHardwareBuffer_unlock");
    }
}
