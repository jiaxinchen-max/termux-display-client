#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "include/wayland.h"
#include "include/tlog.h"

#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"

volatile sig_atomic_t keep_running = 1;
extern LorieBuffer* lorieBuffer;
extern struct lorie_shared_server_state *serverState;
static char path[256];
static int frame_number = 0;
static const char *animation_folder = "animation_light_grey";

#define NUM_FRAMES 9
typedef struct {
    uint8_t *data;
    int width;
    int height;
    bool loaded;
} CachedFrame;

static CachedFrame frame_cache[NUM_FRAMES] = {0};

static uint8_t* loadAndPreprocessFrame(int frameId, int bufferWidth, int bufferHeight) {
    sprintf(path, "/data/data/com.termux/files/home/.wayland/assets/%s/frame_%03d.png", animation_folder, frameId);

    int width, height, channel;
    uint8_t *chs = stbi_load(path, &width, &height, &channel, STBI_rgb_alpha);

    if (!chs) {
        tlog(LOG_ERR, "Failed to load image: %s", path);
        return NULL;
    }

    uint8_t *processedImage = (uint8_t *)calloc(bufferWidth * bufferHeight, 4);
    if (!processedImage) {
        tlog(LOG_ERR, "Failed to allocate memory for frame %d", frameId);
        stbi_image_free(chs);
        return NULL;
    }

    int copyWidth = (width < bufferWidth) ? width : bufferWidth;
    int copyHeight = (height < bufferHeight) ? height : bufferHeight;
    int xOffset = (bufferWidth - copyWidth) / 2;
    int yOffset = (bufferHeight - copyHeight) / 2;

    for (int y = 0; y < copyHeight; y++) {
        memcpy(processedImage + ((yOffset + y) * bufferWidth + xOffset) * 4,
               chs + y * width * 4,
               copyWidth * 4);
    }

    stbi_image_free(chs);
    return processedImage;
}

static int animate(){
    int ret;
    void *shared_buffer;
    lorie_mutex_lock(&serverState->lock, &serverState->lockingPid);
    ret = LorieBuffer_lock(lorieBuffer,&shared_buffer);
    if (ret != 0) {
        tlog(LOG_ERR, "Failed to AHardwareBuffer_lock");
        lorie_mutex_unlock(&serverState->lock, &serverState->lockingPid);
        return ret;
    }

    const LorieBuffer_Desc *desc = LorieBuffer_description(lorieBuffer);
    int bufferWidth = desc->width;
    int bufferHeight = desc->height;
    int stride = desc->stride;

    int frameId = frame_number % NUM_FRAMES;
    frame_number++;

    if (!frame_cache[frameId].loaded) {
        frame_cache[frameId].data = loadAndPreprocessFrame(frameId, bufferWidth, bufferHeight);
        if (!frame_cache[frameId].data) {
            LorieBuffer_unlock(lorieBuffer);
            lorie_mutex_unlock(&serverState->lock, &serverState->lockingPid);
            return -1;
        }
        frame_cache[frameId].width = bufferWidth;
        frame_cache[frameId].height = bufferHeight;
        frame_cache[frameId].loaded = true;
    }

    uint8_t *frameData = frame_cache[frameId].data;

    if (stride == bufferWidth) {
        memcpy(shared_buffer, frameData, bufferWidth * bufferHeight * 4);
    } else {
        int strideBytes = stride * 4;
        for (int y = 0; y < bufferHeight; y++) {
            memcpy((uint8_t *)shared_buffer + y * strideBytes,
                   frameData + y * bufferWidth * 4,
                   bufferWidth * 4);
        }
    }

    serverState->waitForNextFrame = false;
    serverState->drawRequested = 1;
    pthread_cond_signal(&serverState->cond);
    lorie_mutex_unlock(&serverState->lock, &serverState->lockingPid);
    ret = LorieBuffer_unlock(lorieBuffer);
    return ret;
}

void cleanupFrameCache() {
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (frame_cache[i].loaded && frame_cache[i].data) {
            free(frame_cache[i].data);
            frame_cache[i].data = NULL;
            frame_cache[i].loaded = false;
        }
    }
}

void timer_handler(int signum) {
    animate();
}

void int_handler(int signum) {
    (void)signum;
    keep_running = 0;
}

void exit_handler(void) {
    cleanupFrameCache();
    stopEventLoop();
    exit(0);
}

int main(int count,char** argv){
    int fps = 10;
    if (count > 1) {
        fps = atoi(argv[1]);
        if (fps <= 0 || fps > 240) {
            tlog(LOG_ERR, "Invalid fps value: %d. Using default 30 fps", fps);
            fps = 30;
        }
    }

    if (count > 2) {
        animation_folder = argv[2];
    }

    int interval_usec = 1000000 / fps;

    if (connectToRender() != 0) {
        return 1;
    }

    setExitCallback(exit_handler);

    struct sigaction sa_timer, sa_int;
    struct itimerval timer;

    sa_timer.sa_handler = timer_handler;
    sa_timer.sa_flags = SA_RESTART;
    sigemptyset(&sa_timer.sa_mask);
    if (sigaction(SIGALRM, &sa_timer, NULL) == -1) {
        tlog(LOG_ERR, "sigaction timer failed");
        return 1;
    }

    sa_int.sa_handler = int_handler;
    sa_int.sa_flags = 0;
    sigemptyset(&sa_int.sa_mask);
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        tlog(LOG_ERR, "sigaction int failed");
        return 1;
    }

    timer.it_value.tv_sec = interval_usec / 1000000;
    timer.it_value.tv_usec = interval_usec % 1000000;
    timer.it_interval.tv_sec = interval_usec / 1000000;
    timer.it_interval.tv_usec = interval_usec % 1000000;

    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        tlog(LOG_ERR, "setitimer failed");
        return 1;
    }

    while (keep_running) {
        pause();
    }

    cleanupFrameCache();
    stopEventLoop();

    return 0;
}
