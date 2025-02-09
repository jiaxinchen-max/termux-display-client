#pragma once

#include "termuxdc_event.h"
class termuxdc_event_callback{
public:
    virtual void callback(termuxdc_event ev);
};
