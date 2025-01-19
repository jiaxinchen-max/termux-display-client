#pragma once

#include "InputEvent.h"
#include "SocketIPCClient.h"
class InputEventCallback{
public:
    virtual void callback(InputEvent ev);
};