#pragma once

#include "SocketIPCClient.h"
#include "InputEventCallback.h"
#include "types.h"


class InputServer {
public:

    void Init();

    void Destroy();

    int GetDataSocket();

    InputEventCallback *GetCallback();

    void SetCallback(InputEventCallback *ca);

    void SetInputHandler(InputHandler handler);

    InputHandler GetInputHandler();

private:
    int dataSocket;

    pthread_t t;

    InputEventCallback *inputEventCallback;

    InputHandler inputHandler;

    void reset();

    ~InputServer();
};

void *work(void *args);