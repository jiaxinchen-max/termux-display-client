#ifndef RENDER_H
#define RENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <android/hardware_buffer.h>
#include <android/native_window_jni.h>
#include <android/choreographer.h>
#include <android/log.h>
#include <stdbool.h>
#include <jni.h>
#include <errno.h>
#include <sys/socket.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "linux/input-event-codes.h"
#include "buffer.h"
#include "tlog.h"

#define PORT 7892
#define MAGIC "0xDEADBEEF"

struct lorie_shared_server_state;
bool waylandConnectionAlive();
static inline __always_inline void lorie_mutex_lock(pthread_mutex_t* mutex, pid_t* lockingPid) {
    // Unfortunately there is no robust mutexes in bionic.
    // Posix does not define any valid way to unlock stuck non-robust mutex
    // so in the case if renderer or X server process unexpectedly die with locked mutex
    // we will simply reinitialize it.
    struct timespec ts = {0};
    while(true) {
        clock_gettime(CLOCK_MONOTONIC, &ts);

        // 33 msec is enough to complete any drawing operation on both X server and renderer side
        // In the case if mutex is locked most likely other thread died with the mutex locked
        ts.tv_nsec += 33UL * 1000000UL;
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec  += ts.tv_nsec / 1000000000L;
            ts.tv_nsec  = ts.tv_nsec % 1000000000L;
        }

        int ret = pthread_mutex_timedlock(mutex, &ts);
        if (ret == ETIMEDOUT) {
            tlog(LOG_DEBUG,"lorie_mutex_lock timeout");
            if (*lockingPid == getpid() || waylandConnectionAlive())
                continue;

            pthread_mutexattr_t attr;
            pthread_mutex_t initializer = PTHREAD_MUTEX_INITIALIZER;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
            memcpy(mutex, &initializer, sizeof(initializer));
            pthread_mutex_init(mutex, &attr);
            // Mutex will be locked fine on the next iteration
        } else {
            *lockingPid = getpid();
            return;
        }
    }
}

static inline __always_inline void lorie_mutex_unlock(pthread_mutex_t* mutex, pid_t* lockingPid) {
    *lockingPid = 0;
    pthread_mutex_unlock(mutex);
}

typedef enum {
    EVENT_UNKNOWN __unused = 0,
    EVENT_SHARED_SERVER_STATE,
    EVENT_ADD_BUFFER,
    EVENT_REMOVE_BUFFER,
    EVENT_SCREEN_SIZE,
    EVENT_TOUCH,
    EVENT_MOUSE,
    EVENT_KEY,
    EVENT_STYLUS,
    EVENT_STYLUS_ENABLE,
    EVENT_UNICODE,
    EVENT_CLIPBOARD_ENABLE,
    EVENT_CLIPBOARD_ANNOUNCE,
    EVENT_CLIPBOARD_REQUEST,
    EVENT_CLIPBOARD_SEND,
    EVENT_WINDOW_FOCUS_CHANGED,
    EVENT_APPLY_SERVER_STATE,
    EVENT_APPLY_BUFFER,
    EVENT_APPLY_EVENT_FD,
    EVENT_SHARED_EVENT_FD,
    EVENT_SERVER_VERIFY_SUCCEED,
    EVENT_CLIENT_VERIFY_SUCCEED,
    EVENT_STOP_RENDER,
} eventType;

typedef union {
    uint8_t type;
    struct {
        uint8_t t;
        uint16_t width, height, framerate;
        size_t name_size;
        char *name;
        uint8_t format;
        uint8_t type;
    } screenSize;
    struct {
        uint8_t t;
        unsigned long id;
    } removeBuffer;
    struct {
        uint8_t t;
        uint16_t type, id, x, y;
    } touch;
    struct {
        uint8_t t;
        float x, y;
        uint8_t detail, down, relative;
    } mouse;
    struct {
        uint8_t t;
        uint16_t key;
        uint8_t state;
    } key;
    struct {
        uint8_t t;
        float x, y;
        uint16_t pressure;
        int8_t tilt_x, tilt_y;
        int16_t orientation;
        uint8_t buttons, eraser, mouse;
    } stylus;
    struct {
        uint8_t t, enable;
    } stylusEnable;
    struct {
        uint8_t t;
        uint32_t code;
    } unicode;
    struct {
        uint8_t t;
        uint8_t enable;
    } clipboardEnable;
    struct {
        uint8_t t;
        uint32_t count;
    } clipboardSend;
} lorieEvent;

struct lorie_shared_server_state {
    /*
     * Renderer and X server are separated into 2 different processes.
     * Root window and cursor content and properties are shared across these 2 processes.
     * Reading/drawing root window in renderer the same time X server writes it can cause
     * tearing, texture garbling and other visual artifacts so we should block X server while we are drawing.
     */
    pthread_mutex_t lock; // initialized at X server side.
    pid_t lockingPid;

    /*
     * Renderer thread sleeps when it is idle so we must explicitly wake it up.
     */
    pthread_cond_t cond; // initialized at X server side.

    /* ID of root window texture to be drawn. */
    uint64_t rootWindowTextureID;

    /* A signal to renderer to update root window texture content from shared fragment if needed */
    volatile uint8_t drawRequested;

    /* We should avoid triggering renderer if there is no output surface */
    volatile uint8_t surfaceAvailable;

    /*
     * We do not want to block the X server for an extended period; ideally, we would avoid blocking it at all.
     * However, if we don’t block the X server, it will overwrite root window memory fragment, causing tearing or frame distortion.
     * On some devices, there is no way to make EGL/GLES2 render a frame without calling eglSwapBuffers;
     * calls like glFinish, eglWaitGL, and eglWaitClient have no effect.
     * The only way to force EGL to render a frame and flush the command queue is by invoking eglSwapBuffers.
     * But eglSwapBuffers will not return until Android actually displays the frame.
     * Since we want to proceed as quickly as possible, waiting for the frame to be shown is not acceptable.
     *
     * Therefore, we set eglSwapInterval(dpy, 1), so that eglSwapBuffers does not block until the frame is displayed.
     * Even then, we do not want to waste GPU resources rendering more than one full-screen quad per vsync,
     * because that would spend GPU time on a frame that will never be shown.
     * To handle this, we use a waitForNextFrame flag, which we set after a successful render and clear from the AChoreographer’s frame callback.
     */
    volatile uint8_t waitForNextFrame;

    /* Needed to show FPS counter in logcat */
    volatile int renderedFrames;

    struct {
        // We should not allow updating cursor content the same time renderer draws it.
        // locking the mutex protecting the root window can cause waiting for the frame to be drawn which is unacceptable
        pthread_mutex_t lock; // initialized at X server side.
        pid_t lockingPid;
        uint32_t x, y, xhot, yhot, width, height;
        uint32_t bits[512*512]; // 1 megabyte should be enough for any cursor up to 512x512
        // Signals to renderer to update cursor's texture or its coordinates
        volatile uint8_t updated, moved;
    } cursor;
};

extern int android_to_linux_keycode[304];
void setScreenConfig(int, int, int);
int connectToRender();
void setExitCallback(void (*callback)(void));
void stopEventLoop();
/** Returns the fd for input events (mouse, touch, etc.), or -1 if not connected. */
int get_connFd();
LorieBuffer *get_lorieBuffer();
struct lorie_shared_server_state *get_serverState();

#ifdef __cplusplus
}
#endif

#endif /* RENDER_H */
