#include <stdio.h>
#include "InputEvent.h"
#include "client.h"

void inputCallback(termuxdc_event ev) {
    printf("%d\n", ev.type);
}
int main(int argc,const char** argv){
    DisplayClientInit(800, 600, 4);
    InputInit(inputCallback);
    DisplayClientStart();
    return 0;
}
