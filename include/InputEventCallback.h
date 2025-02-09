#pragma once

#include "InputEvent.h"
class InputEventCallback{
public:
    virtual void callback(termuxdc_event ev);
};
