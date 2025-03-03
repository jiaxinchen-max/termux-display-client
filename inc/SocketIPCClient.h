#pragma once

#include <cstdint>
#include <string>
#include <android/hardware_buffer.h>

class SocketIPCClient {
public:
    static SocketIPCClient *GetInstance();

    int Init(AHardwareBuffer *hwBuffer, int dataSocket);
    void SetImageGeometry(uint32_t w,uint32_t h,uint32_t ch);
    int Draw(const uint8_t* data);
    int BeginDraw(const uint8_t* data);
    int BeginDraw(void* data_ptr);
    int EndDraw();

    void Destroy();

    int Draw();

private:
    SocketIPCClient() = default;

    static SocketIPCClient s_Renderer;

    uint32_t m_ImgWidth = 800;
    uint32_t m_ImgHeight = 600;
    uint32_t m_ImgChannel = 4;


    AHardwareBuffer *buffer;

    int cur = 0;
};
