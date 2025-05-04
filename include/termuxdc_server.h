#pragma once

#include <bits/pthread_types.h>
#include "termuxdc_event_callback.h"
#include "types.h"


class termuxdc_server {
public:

    void init();
    void initDefault();

    void destroy();

    int getDataSocket();

    termuxdc_event_callback *getCallback();

    void setCallback(termuxdc_event_callback *ca);

    void setInputHandler(InputHandler handler);

    InputHandler getInputHandler();

    bool isRunning();
    void setRunning(bool run);

    ssize_t recvEvent(void *buffer, size_t length);
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
