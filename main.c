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
    // Get buffer dimensions first
    const LorieBuffer_Desc *desc = LorieBuffer_description(lorieBuffer);
    int bufferWidth = desc->width;
    int bufferHeight = desc->height;
    int stride = desc->stride;  // Stride in pixels

    uint8_t *chs;
    int width, height, channel;
    {
        int id = frame_number % 9;
        sprintf(path, "/data/data/com.termux/files/home/.wayland/assets/animation_light_grey/frame_%03d.png", id);
        chs = stbi_load(path, &width, &height, &channel, STBI_rgb_alpha);
        frame_number++;
    }

    if (!chs) {
        tlog(LOG_ERR, "Failed to load image: %s", path);
        lorie_mutex_unlock(&serverState->lock, &serverState->lockingPid);
        return -1;
    }

    tlog(LOG_INFO, "Loaded image: %dx%d, Buffer: %dx%d, stride: %d",
         width, height, bufferWidth, bufferHeight, stride);

    // Prepare final image buffer matching buffer dimensions
    uint8_t *finalImage = NULL;

    if (width == bufferWidth && height == bufferHeight) {
        // Image matches buffer size, use directly
        finalImage = chs;
    } else {
        // Need to resize/crop/pad image to match buffer
        finalImage = (uint8_t *)calloc(bufferWidth * bufferHeight, 4);
        if (!finalImage) {
            tlog(LOG_ERR, "Failed to allocate memory for image processing");
            stbi_image_free(chs);
            lorie_mutex_unlock(&serverState->lock, &serverState->lockingPid);
            return -1;
        }

        // Center the image (crop or pad with black)
        int copyWidth = (width < bufferWidth) ? width : bufferWidth;
        int copyHeight = (height < bufferHeight) ? height : bufferHeight;
        int xOffset = (bufferWidth - copyWidth) / 2;
        int yOffset = (bufferHeight - copyHeight) / 2;

        for (int y = 0; y < copyHeight; y++) {
            memcpy(finalImage + ((yOffset + y) * bufferWidth + xOffset) * 4,
                   chs + y * width * 4,
                   copyWidth * 4);
        }
    }

    // Now copy to shared buffer (one memcpy if stride matches width)
    if (stride == bufferWidth) {
        // No padding, one memcpy
        memcpy(shared_buffer, finalImage, bufferWidth * bufferHeight * 4);
        tlog(LOG_INFO, "Using fast copy (no stride padding)");
    } else {
        // Has padding, copy line by line
        int strideBytes = stride * 4;
        for (int y = 0; y < bufferHeight; y++) {
            memcpy((uint8_t *)shared_buffer + y * strideBytes,
                   finalImage + y * bufferWidth * 4,
                   bufferWidth * 4);
        }
        tlog(LOG_INFO, "Using line-by-line copy (stride=%d, width=%d)", stride, bufferWidth);
    }

    // Cleanup
    if (finalImage != chs) {
        free(finalImage);
    }
    serverState->waitForNextFrame = false;
    serverState->drawRequested = 1;
    pthread_cond_signal(&serverState->cond);
    lorie_mutex_unlock(&serverState->lock, &serverState->lockingPid);
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
