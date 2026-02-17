#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include "include/wayland.h"
#include "include/tlog.h"

#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"

volatile sig_atomic_t keep_running = 1;
extern LorieBuffer* lorieBuffer;
extern struct lorie_shared_server_state *serverState;
static char path[256];
static int frame_number = 0;

static int animate(){
    tlog(LOG_INFO,"Start rend a picture");
    int ret;
    void *shared_buffer;
    lorie_mutex_lock(&serverState->lock, &serverState->lockingPid);
    ret = LorieBuffer_lock(lorieBuffer,&shared_buffer);
    if (ret != 0) {
        tlog(LOG_ERR, "Failed to AHardwareBuffer_lock");
        return ret;
    }
    uint8_t *chs;
    int width, height, channel;
    {
        int id = frame_number % 9;
        sprintf(path, "/data/data/com.termux/files/home/.wayland/assets/animation_light_grey/frame_%03d.png", id);
        chs = stbi_load(path, &width, &height, &channel, STBI_rgb_alpha);
        frame_number++;
    }
    // assuming format was set to 4 bytes per pixel and not 565 mode
    memcpy(shared_buffer, chs, width * height * sizeof(uint32_t));
    serverState->waitForNextFrame = false;
    serverState->drawRequested = 1;
    lorie_mutex_unlock(&serverState->lock, &serverState->lockingPid);
    pthread_cond_signal(&serverState->cond);
    ret = LorieBuffer_unlock(lorieBuffer);
    stbi_image_free(chs);
    tlog(LOG_INFO,"End rend a picture");
    return ret;
}

void timer_handler(int signum) {
    animate();
}

void int_handler(int signum) {
    (void)signum;
    keep_running = 0;
    tlog(LOG_INFO, "Received Ctrl+C, exiting...\n");
}
int main(int count,char** argv){
    tlog(LOG_INFO, "========== MAIN START ==========");
    fflush(NULL);

    tlog(LOG_INFO, "Calling connectToRender...");
    fflush(NULL);

    int ret = connectToRender();

    tlog(LOG_INFO, "========== connectToRender RETURNED: %d ==========", ret);
    fflush(NULL);

    tlog(LOG_INFO,"Termux render initialization succeed");
    fflush(NULL);

    tlog(LOG_INFO, "Setting up signal handlers...");
    struct sigaction sa_timer, sa_int;
    struct itimerval timer;

    sa_timer.sa_handler = timer_handler;
    sa_timer.sa_flags = SA_RESTART;
    sigemptyset(&sa_timer.sa_mask);
    if (sigaction(SIGALRM, &sa_timer, NULL) == -1) {
        tlog(LOG_ERR, "sigaction timer");
        return 1;
    }

    sa_int.sa_handler = int_handler;
    sa_int.sa_flags = 0;
    sigemptyset(&sa_int.sa_mask);
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        tlog(LOG_ERR,"sigaction int");
        return 1;
    }

    timer.it_value.tv_sec = 1;
    timer.it_value.tv_usec = 0;       // 100ms
    timer.it_interval.tv_sec = 1;
    timer.it_interval.tv_usec = 0;    // 100ms

    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        tlog(LOG_ERR,"setitimer");
        return 1;
    }

    while (keep_running) {
        pause();
    }
}
