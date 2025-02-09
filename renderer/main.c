#include <stdio.h>
#include "termuxdc_event.h"
#include "client.h"

void inputCallback(termuxdc_event ev) {
    printf("%d\n", ev.type);
}
int main(int argc,const char** argv){
    display_client_init(800, 600, 4);
    event_socket_init(inputCallback);
    display_client_start();
    return 0;
}
