#pragma once

#include <cstdint>
#include <string>
#include <android/hardware_buffer.h>

class SocketIPCClient {
public:
    static SocketIPCClient *GetInstance();

    void Init(AHardwareBuffer *hwBuffer, int dataSocket);
    void SetImageGeometry(uint32_t w,uint32_t h,uint32_t ch);
    void Draw(const uint8_t* data);
    void BeginDraw(const uint8_t* data);
    void EndDraw();

    void Destroy();

    void Draw();

private:
    SocketIPCClient() = default;

    static SocketIPCClient s_Renderer;

    uint32_t m_ImgWidth = 800;
    uint32_t m_ImgHeight = 600;
    uint32_t m_ImgChannel = 4;


    AHardwareBuffer *buffer;

    int cur = 0;
};
