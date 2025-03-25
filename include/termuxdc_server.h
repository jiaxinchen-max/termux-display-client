#pragma once

#include <bits/pthread_types.h>
#include "termuxdc_event_callback.h"
#include "types.h"


class termuxdc_server {
public:

    void Init();
    void InitDefault();

    void Destroy();

    int GetDataSocket();

    termuxdc_event_callback *GetCallback();

    void SetCallback(termuxdc_event_callback *ca);

    void SetInputHandler(InputHandler handler);

    InputHandler GetInputHandler();

    bool isRunning();
    void setRunning(bool run);

    ssize_t recv_event( void *buffer, size_t length);
    int waitEvent(termuxdc_event *event);

private:
    int dataSocket;

    pthread_t t;

    bool running;

    termuxdc_event_callback *inputEventCallback;

    InputHandler inputHandler;

    void reset();

    ~termuxdc_server();
};

void *work(void *args);
