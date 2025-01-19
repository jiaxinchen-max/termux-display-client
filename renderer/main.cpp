#include "termux/display/client/InputEvent.h"
#include "termux/display/client/client.h"

void inputCallback(InputEvent ev) {
    printf("%d", ev.type);
}
int main(int argc,const char** argv){
    DisplayClientInit(800, 600, 4);
    InputInit(inputCallback);
    pthread_t t;
    DisplayClientStart();
    return 0;
}