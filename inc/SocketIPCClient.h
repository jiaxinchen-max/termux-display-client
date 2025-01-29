#pragma once

#include <cstdint>
#include <string>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2platform.h>
#include <android/hardware_buffer.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

class SocketIPCClient {
public:
    static SocketIPCClient *GetInstance();

    void Init(AHardwareBuffer *hwBuffer, int dataSocket);
    void SetImageGeometry(uint32_t w,uint32_t h,uint32_t ch);
    void Draw(const uint8_t* data);

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