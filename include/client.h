#pragma once

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "termuxdc_event.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif
int display_client_init(uint32_t width, uint32_t height, uint32_t channel);

int display_client_start();

void display_destroy();

void event_socket_init(InputHandler handler);

void event_socket_init_default();

void event_socket_destroy();

int get_input_socket();

int event_wait(termuxdc_event *event);

struct termuxdc_buffer *get_termuxdc_buffer();

const termuxdc_native_handle_t *get_native_handler();

#ifdef __cplusplus
}
#endif
