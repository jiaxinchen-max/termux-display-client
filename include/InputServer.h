#pragma once

#include <bits/pthread_types.h>
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

    bool isRunning();
    void setRunning(bool run);

private:
    int dataSocket;

    pthread_t t;

    bool running;

    InputEventCallback *inputEventCallback;

    InputHandler inputHandler;

    void reset();

    ~InputServer();
};

void *work(void *args);
