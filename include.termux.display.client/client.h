#pragma once

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "InputEvent.h"
#include "types.h"

void DisplayClientInit(uint32_t width, uint32_t height, uint32_t channel);

void DisplayClientStart();

void DisplayDraw(const uint8_t *data);

void InputInit(InputHandler handler);