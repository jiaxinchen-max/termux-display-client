#pragma once

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "InputEvent.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif
int DisplayClientInit(uint32_t width, uint32_t height, uint32_t channel);

int DisplayClientStart();

int DisplayDraw(const uint8_t *data);

int BeginDisplayDraw(const uint8_t *data);

void DisplayDestroy();

int EndDisplayDraw();

void InputInit(InputHandler handler);

void InputDestroy();

int GetInputSocket();


#ifdef __cplusplus
}
#endif
