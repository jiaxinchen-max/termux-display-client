#pragma once

#include "InputEvent.h"
class InputEventCallback{
public:
    virtual void callback(InputEvent ev);
};
