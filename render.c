#include <jni.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include "include/buffer.h"
#include "include/render.h"
#include "include/tlog.h"
#include "linux/input-event-codes.h"

// Define the keycode conversion array
int android_to_linux_keycode[304] = {
        [ 4   /* ANDROID_KEYCODE_BACK */] = KEY_ESC,
        [ 7   /* ANDROID_KEYCODE_0 */] = KEY_0,
        [ 8   /* ANDROID_KEYCODE_1 */] = KEY_1,
        [ 9   /* ANDROID_KEYCODE_2 */] = KEY_2,
        [ 10  /* ANDROID_KEYCODE_3 */] = KEY_3,
        [ 11  /* ANDROID_KEYCODE_4 */] = KEY_4,
        [ 12  /* ANDROID_KEYCODE_5 */] = KEY_5,
        [ 13  /* ANDROID_KEYCODE_6 */] = KEY_6,
        [ 14  /* ANDROID_KEYCODE_7 */] = KEY_7,
        [ 15  /* ANDROID_KEYCODE_8 */] = KEY_8,
        [ 16  /* ANDROID_KEYCODE_9 */] = KEY_9,
        [ 17  /* ANDROID_KEYCODE_STAR */] = KEY_KPASTERISK,
        [ 19  /* ANDROID_KEYCODE_DPAD_UP */] = KEY_UP,
        [ 20  /* ANDROID_KEYCODE_DPAD_DOWN */] = KEY_DOWN,
        [ 21  /* ANDROID_KEYCODE_DPAD_LEFT */] = KEY_LEFT,
        [ 22  /* ANDROID_KEYCODE_DPAD_RIGHT */] = KEY_RIGHT,
        [ 23  /* ANDROID_KEYCODE_DPAD_CENTER */] = KEY_ENTER,
        [ 24  /* ANDROID_KEYCODE_VOLUME_UP */] = KEY_VOLUMEUP, // XF86XK_AudioRaiseVolume
        [ 25  /* ANDROID_KEYCODE_VOLUME_DOWN */] = KEY_VOLUMEDOWN, // XF86XK_AudioLowerVolume
        [ 26  /* ANDROID_KEYCODE_POWER */] = KEY_POWER,
        [ 27  /* ANDROID_KEYCODE_CAMERA */] = KEY_CAMERA,
        [ 28  /* ANDROID_KEYCODE_CLEAR */] = KEY_CLEAR,
        [ 29  /* ANDROID_KEYCODE_A */] = KEY_A,
        [ 30  /* ANDROID_KEYCODE_B */] = KEY_B,
        [ 31  /* ANDROID_KEYCODE_C */] = KEY_C,
        [ 32  /* ANDROID_KEYCODE_D */] = KEY_D,
        [ 33  /* ANDROID_KEYCODE_E */] = KEY_E,
        [ 34  /* ANDROID_KEYCODE_F */] = KEY_F,
        [ 35  /* ANDROID_KEYCODE_G */] = KEY_G,
        [ 36  /* ANDROID_KEYCODE_H */] = KEY_H,
        [ 37  /* ANDROID_KEYCODE_I */] = KEY_I,
        [ 38  /* ANDROID_KEYCODE_J */] = KEY_J,
        [ 39  /* ANDROID_KEYCODE_K */] = KEY_K,
        [ 40  /* ANDROID_KEYCODE_L */] = KEY_L,
        [ 41  /* ANDROID_KEYCODE_M */] = KEY_M,
        [ 42  /* ANDROID_KEYCODE_N */] = KEY_N,
        [ 43  /* ANDROID_KEYCODE_O */] = KEY_O,
        [ 44  /* ANDROID_KEYCODE_P */] = KEY_P,
        [ 45  /* ANDROID_KEYCODE_Q */] = KEY_Q,
        [ 46  /* ANDROID_KEYCODE_R */] = KEY_R,
        [ 47  /* ANDROID_KEYCODE_S */] = KEY_S,
        [ 48  /* ANDROID_KEYCODE_T */] = KEY_T,
        [ 49  /* ANDROID_KEYCODE_U */] = KEY_U,
        [ 50  /* ANDROID_KEYCODE_V */] = KEY_V,
        [ 51  /* ANDROID_KEYCODE_W */] = KEY_W,
        [ 52  /* ANDROID_KEYCODE_X */] = KEY_X,
        [ 53  /* ANDROID_KEYCODE_Y */] = KEY_Y,
        [ 54  /* ANDROID_KEYCODE_Z */] = KEY_Z,
        [ 55  /* ANDROID_KEYCODE_COMMA */] = KEY_COMMA,
        [ 56  /* ANDROID_KEYCODE_PERIOD */] = KEY_DOT,
        [ 57  /* ANDROID_KEYCODE_ALT_LEFT */] = KEY_LEFTALT,
        [ 58  /* ANDROID_KEYCODE_ALT_RIGHT */] = KEY_RIGHTALT,
        [ 59  /* ANDROID_KEYCODE_SHIFT_LEFT */] = KEY_LEFTSHIFT,
        [ 60  /* ANDROID_KEYCODE_SHIFT_RIGHT */] = KEY_RIGHTSHIFT,
        [ 61  /* ANDROID_KEYCODE_TAB */] = KEY_TAB,
        [ 62  /* ANDROID_KEYCODE_SPACE */] = KEY_SPACE,
        [ 64  /* ANDROID_KEYCODE_EXPLORER */] = KEY_WWW,
        [ 65  /* ANDROID_KEYCODE_ENVELOPE */] = KEY_MAIL,
        [ 66  /* ANDROID_KEYCODE_ENTER */] = KEY_ENTER,
        [ 67  /* ANDROID_KEYCODE_DEL */] = KEY_BACKSPACE,
        [ 68  /* ANDROID_KEYCODE_GRAVE */] = KEY_GRAVE,
        [ 69  /* ANDROID_KEYCODE_MINUS */] = KEY_MINUS,
        [ 70  /* ANDROID_KEYCODE_EQUALS */] = KEY_EQUAL,
        [ 71  /* ANDROID_KEYCODE_LEFT_BRACKET */] = KEY_LEFTBRACE,
        [ 72  /* ANDROID_KEYCODE_RIGHT_BRACKET */] = KEY_RIGHTBRACE,
        [ 73  /* ANDROID_KEYCODE_BACKSLASH */] = KEY_BACKSLASH,
        [ 74  /* ANDROID_KEYCODE_SEMICOLON */] = KEY_SEMICOLON,
        [ 75  /* ANDROID_KEYCODE_APOSTROPHE */] = KEY_APOSTROPHE,
        [ 76  /* ANDROID_KEYCODE_SLASH */] = KEY_SLASH,
        [ 81  /* ANDROID_KEYCODE_PLUS */] = KEY_KPPLUS,
        [ 82  /* ANDROID_KEYCODE_MENU */] = KEY_CONTEXT_MENU,
        [ 84  /* ANDROID_KEYCODE_SEARCH */] = KEY_SEARCH,
        [ 85  /* ANDROID_KEYCODE_MEDIA_PLAY_PAUSE */] = KEY_PLAYPAUSE,
        [ 86  /* ANDROID_KEYCODE_MEDIA_STOP */] = KEY_STOP_RECORD,
        [ 87  /* ANDROID_KEYCODE_MEDIA_NEXT */] = KEY_NEXTSONG,
        [ 88  /* ANDROID_KEYCODE_MEDIA_PREVIOUS */] = KEY_PREVIOUSSONG,
        [ 89  /* ANDROID_KEYCODE_MEDIA_REWIND */] = KEY_REWIND,
        [ 90  /* ANDROID_KEYCODE_MEDIA_FAST_FORWARD */] = KEY_FASTFORWARD,
        [ 91  /* ANDROID_KEYCODE_MUTE */] = KEY_MUTE,
        [ 92  /* ANDROID_KEYCODE_PAGE_UP */] = KEY_PAGEUP,
        [ 93  /* ANDROID_KEYCODE_PAGE_DOWN */] = KEY_PAGEDOWN,
        [ 111  /* ANDROID_KEYCODE_ESCAPE */] = KEY_ESC,
        [ 112  /* ANDROID_KEYCODE_FORWARD_DEL */] = KEY_DELETE,
        [ 113  /* ANDROID_KEYCODE_CTRL_LEFT */] = KEY_LEFTCTRL,
        [ 114  /* ANDROID_KEYCODE_CTRL_RIGHT */] = KEY_RIGHTCTRL,
        [ 115  /* ANDROID_KEYCODE_CAPS_LOCK */] = KEY_CAPSLOCK,
        [ 116  /* ANDROID_KEYCODE_SCROLL_LOCK */] = KEY_SCROLLLOCK,
        [ 117  /* ANDROID_KEYCODE_META_LEFT */] = KEY_LEFTMETA,
        [ 118  /* ANDROID_KEYCODE_META_RIGHT */] = KEY_RIGHTMETA,
        [ 120  /* ANDROID_KEYCODE_SYSRQ */] = KEY_PRINT,
        [ 121  /* ANDROID_KEYCODE_BREAK */] = KEY_BREAK,
        [ 122  /* ANDROID_KEYCODE_MOVE_HOME */] = KEY_HOME,
        [ 123  /* ANDROID_KEYCODE_MOVE_END */] = KEY_END,
        [ 124  /* ANDROID_KEYCODE_INSERT */] = KEY_INSERT,
        [ 125  /* ANDROID_KEYCODE_FORWARD */] = KEY_FORWARD,
        [ 126  /* ANDROID_KEYCODE_MEDIA_PLAY */] = KEY_PLAYCD,
        [ 127  /* ANDROID_KEYCODE_MEDIA_PAUSE */] = KEY_PAUSECD,
        [ 128  /* ANDROID_KEYCODE_MEDIA_CLOSE */] = KEY_CLOSECD,
        [ 129  /* ANDROID_KEYCODE_MEDIA_EJECT */] = KEY_EJECTCD,
        [ 130  /* ANDROID_KEYCODE_MEDIA_RECORD */] = KEY_RECORD,
        [ 131  /* ANDROID_KEYCODE_F1 */] = KEY_F1,
        [ 132  /* ANDROID_KEYCODE_F2 */] = KEY_F2,
        [ 133  /* ANDROID_KEYCODE_F3 */] = KEY_F3,
        [ 134  /* ANDROID_KEYCODE_F4 */] = KEY_F4,
        [ 135  /* ANDROID_KEYCODE_F5 */] = KEY_F5,
        [ 136  /* ANDROID_KEYCODE_F6 */] = KEY_F6,
        [ 137  /* ANDROID_KEYCODE_F7 */] = KEY_F7,
        [ 138  /* ANDROID_KEYCODE_F8 */] = KEY_F8,
        [ 139  /* ANDROID_KEYCODE_F9 */] = KEY_F9,
        [ 140  /* ANDROID_KEYCODE_F10 */] = KEY_F10,
        [ 141  /* ANDROID_KEYCODE_F11 */] = KEY_F11,
        [ 142  /* ANDROID_KEYCODE_F12 */] = KEY_F12,
        [ 143  /* ANDROID_KEYCODE_NUM_LOCK */] = KEY_NUMLOCK,
        [ 144  /* ANDROID_KEYCODE_NUMPAD_0 */] = KEY_KP0,
        [ 145  /* ANDROID_KEYCODE_NUMPAD_1 */] = KEY_KP1,
        [ 146  /* ANDROID_KEYCODE_NUMPAD_2 */] = KEY_KP2,
        [ 147  /* ANDROID_KEYCODE_NUMPAD_3 */] = KEY_KP3,
        [ 148  /* ANDROID_KEYCODE_NUMPAD_4 */] = KEY_KP4,
        [ 149  /* ANDROID_KEYCODE_NUMPAD_5 */] = KEY_KP5,
        [ 150  /* ANDROID_KEYCODE_NUMPAD_6 */] = KEY_KP6,
        [ 151  /* ANDROID_KEYCODE_NUMPAD_7 */] = KEY_KP7,
        [ 152  /* ANDROID_KEYCODE_NUMPAD_8 */] = KEY_KP8,
        [ 153  /* ANDROID_KEYCODE_NUMPAD_9 */] = KEY_KP9,
        [ 154  /* ANDROID_KEYCODE_NUMPAD_DIVIDE */] = KEY_KPSLASH,
        [ 155  /* ANDROID_KEYCODE_NUMPAD_MULTIPLY */] = KEY_KPASTERISK,
        [ 156  /* ANDROID_KEYCODE_NUMPAD_SUBTRACT */] = KEY_KPMINUS,
        [ 157  /* ANDROID_KEYCODE_NUMPAD_ADD */] = KEY_KPPLUS,
        [ 158  /* ANDROID_KEYCODE_NUMPAD_DOT */] = KEY_KPDOT,
        [ 159  /* ANDROID_KEYCODE_NUMPAD_COMMA */] = KEY_KPCOMMA,
        [ 160  /* ANDROID_KEYCODE_NUMPAD_ENTER */] = KEY_KPENTER,
        [ 161  /* ANDROID_KEYCODE_NUMPAD_EQUALS */] = KEY_KPEQUAL,
        [ 162  /* ANDROID_KEYCODE_NUMPAD_LEFT_PAREN */] = KEY_KPLEFTPAREN,
        [ 163  /* ANDROID_KEYCODE_NUMPAD_RIGHT_PAREN */] = KEY_KPRIGHTPAREN,
        [ 164  /* ANDROID_KEYCODE_VOLUME_MUTE */] = KEY_MUTE,
        [ 165  /* ANDROID_KEYCODE_INFO */] = KEY_INFO,
        [ 166  /* ANDROID_KEYCODE_CHANNEL_UP */] = KEY_CHANNELUP,
        [ 167  /* ANDROID_KEYCODE_CHANNEL_DOWN */] = KEY_CHANNELDOWN,
        [ 168  /* ANDROID_KEYCODE_ZOOM_IN */] = KEY_ZOOMIN,
        [ 169  /* ANDROID_KEYCODE_ZOOM_OUT */] = KEY_ZOOMOUT,
        [ 170  /* ANDROID_KEYCODE_TV */] = KEY_TV,
        [ 208  /* ANDROID_KEYCODE_CALENDAR */] = KEY_CALENDAR,
        [ 210  /* ANDROID_KEYCODE_CALCULATOR */] = KEY_CALC,
};

static int event_fd = -1, conn_fd=-1, epfd = -1, stateFd = -1;
static struct epoll_event ev, events[5];
static int connect_retry = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static volatile int buffer_ready = 0;
static volatile int event_loop_running = 0;
static pthread_t event_thread_id = 0;

static void (*onExitCallback)(void) = NULL;

static int screen_width = 1080;
static int screen_height = 720;
static int screen_framerate = 10;
static int screen_format = AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
static int screen_type = LORIEBUFFER_AHARDWAREBUFFER;

#define MAX_RETRY_TIMES 5

#define SOCKET_PATH "/data/data/com.termux/files/usr/tmp/wayland-0"

LorieBuffer *lorieBuffer;
struct lorie_shared_server_state *serverState;

JNIEXPORT jstring JNICALL
Java_com_termux_wayland_NativeLib_stringFromJNI(
        JNIEnv *env,
        jobject this) {
    return "hello";
}

bool waylandConnectionAlive() {
    return lorieBuffer;
}

void setExitCallback(void (*callback)(void)) {
    onExitCallback = callback;
}

int get_connFd() {
    return conn_fd;
}

LorieBuffer *get_lorieBuffer() {
    return lorieBuffer;
}

struct lorie_shared_server_state *get_serverState() {
    return serverState;
}

void setScreenConfig(int width, int height, int framerate) {
    if (width > 0) screen_width = width;
    if (height > 0) screen_height = height;
    if (framerate > 0) screen_framerate = framerate;
}

static void waylandApplyBuffer() {
    LorieBuffer_recvHandleFromUnixSocket(event_fd, &lorieBuffer);
    lorieEvent e = {.type = EVENT_APPLY_SERVER_STATE};
    if (write(event_fd, &e, sizeof(e)) != sizeof(e)) {
        tlog(LOG_ERR, "Failed to send APPLY_SERVER_STATE");
        exit(EXIT_FAILURE);
    }
}

static void waylandDestroyBuffer() {
    if (lorieBuffer) {
        lorieBuffer = NULL;
    }
}

static void waylandApplySharedServerState() {
    stateFd = ancil_recv_fd(event_fd);

    if (stateFd < 0) {
        tlog(LOG_ERR, "Failed to parse server state: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    serverState = mmap(NULL, sizeof(*serverState), PROT_READ | PROT_WRITE, MAP_SHARED, stateFd, 0);
    if (!serverState || serverState == MAP_FAILED) {
        tlog(LOG_ERR, "Failed to map server state: %s", strerror(errno));
        serverState = NULL;
        exit(EXIT_FAILURE);
    }
    close(stateFd);
    lorieEvent e = {.type = EVENT_APPLY_EVENT_FD};
    if (write(event_fd, &e, sizeof(e)) != sizeof(e)) {
        tlog(LOG_ERR, "Failed to send APPLY_EVENT_FD");
        exit(EXIT_FAILURE);
    }
}

static void waylandDestroySharedServerState() {
    if (serverState) {
        munmap(serverState, sizeof(*serverState));
        serverState = NULL;
    }
}

static void waylandApplyEventFD(){
    conn_fd = ancil_recv_fd(event_fd);

    if (conn_fd < 0) {
        tlog(LOG_ERR, "Failed to parse server state: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    serverState->lockingPid = getpid();
    lorieEvent e = {.type = EVENT_CLIENT_VERIFY_SUCCEED};
    write(event_fd, &e, sizeof(e));

    pthread_mutex_lock(&mutex);
    buffer_ready = 1;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}

static void *eventLoopThread(void *arg) {
    event_loop_running = 1;

    while (event_loop_running) {
        int nfds = epoll_wait(epfd, events, 5, 1000);
        if (nfds == -1) {
            if (errno == EINTR) continue;
            tlog(LOG_ERR, "epoll_wait error: %s", strerror(errno));
            break;
        }

        if (nfds == 0) continue;

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == event_fd) {
                if (events[i].events & EPOLLIN) {
                    lorieEvent e = {0};
                    if (read(event_fd, &e, sizeof(e)) == sizeof(e)) {
                        switch (e.type) {
                            case EVENT_SERVER_VERIFY_SUCCEED: {
                                lorieEvent e1 = {.type = EVENT_APPLY_BUFFER};
                                if (write(event_fd, &e1, sizeof(e1)) != sizeof(e1)) {
                                    tlog(LOG_ERR, "Failed to send APPLY_BUFFER");
                                    goto cleanup;
                                }
                                lorieEvent e2 = {.screenSize = {.t = EVENT_SCREEN_SIZE, .width = screen_width, .height = screen_height, .framerate = screen_framerate, .format = screen_format, .type = screen_type}};
                                if (write(event_fd, &e2, sizeof(e2)) != sizeof(e2)) {
                                    tlog(LOG_ERR, "Failed to send BUFFER PROPERTIES");
                                    goto cleanup;
                                }
                                break;
                            }
                            case EVENT_SHARED_SERVER_STATE: {
                                waylandApplySharedServerState();
                                break;
                            }
                            case EVENT_ADD_BUFFER: {
                                waylandApplyBuffer();
                                break;
                            }
                            case EVENT_SHARED_EVENT_FD:{
                                waylandApplyEventFD();
                                break;
                            }
                            case EVENT_STOP_RENDER: {
                                event_loop_running = 0;
                                waylandDestroyBuffer();
                                waylandDestroySharedServerState();
                                if (epfd != -1) {
                                    close(epfd);
                                    epfd = -1;
                                }
                                if (event_fd != -1) {
                                    close(event_fd);
                                    event_fd = -1;
                                }
                                if (onExitCallback) {
                                    onExitCallback();
                                } else {
                                    exit(0);
                                }
                                return NULL;
                            }
                            default:
                                tlog(LOG_WARNING, "Unknown event type: %d", e.type);
                                break;
                        }
                    } else {
                        tlog(LOG_ERR, "Incomplete event received");
                        goto cleanup;
                    }
                } else if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    tlog(LOG_ERR, "Connection error/hangup detected");
                    goto cleanup;
                }
            }
        }
    }

    cleanup:
    pthread_mutex_lock(&mutex);
    if (buffer_ready == 0) {
        buffer_ready = -1;
        pthread_cond_signal(&cond);
    }
    pthread_mutex_unlock(&mutex);

    if (epfd != -1) {
        close(epfd);
        epfd = -1;
    }
    if (event_fd != -1) {
        close(event_fd);
        event_fd = -1;
    }

    event_loop_running = 0;
    return NULL;
}

static int waitForInitialization() {
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 10;

    pthread_mutex_lock(&mutex);
    while (buffer_ready == 0) {
        int ret = pthread_cond_timedwait(&cond, &mutex, &timeout);
        if (ret == ETIMEDOUT) {
            pthread_mutex_unlock(&mutex);
            tlog(LOG_ERR, "Timeout waiting for buffer initialization");
            return EXIT_FAILURE;
        }
    }
    int status = buffer_ready;
    pthread_mutex_unlock(&mutex);

    if (status < 0) {
        tlog(LOG_ERR, "Buffer initialization failed");
        return EXIT_FAILURE;
    }

    return 0;
}

int connectToRender() {
    struct sockaddr_un serverAddr;

    event_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (event_fd < 0) {
        tlog(LOG_ERR, "socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    strncpy(serverAddr.sun_path, SOCKET_PATH, sizeof(serverAddr.sun_path) - 1);

    int ret = connect(event_fd, (const struct sockaddr *) &serverAddr, sizeof(serverAddr));
    if (ret < 0) {
        if (connect_retry >= MAX_RETRY_TIMES - 1) {
            tlog(LOG_ERR, "connect failed after %d attempts: %s", connect_retry + 1,
                 strerror(errno));
            close(event_fd);
            exit(EXIT_FAILURE);
        }
        connect_retry++;
        sleep(5);
    } else {
        epfd = epoll_create1(0);
        if (epfd == -1) {
            tlog(LOG_ERR, "epoll_create1 failed: %s", strerror(errno));
            close(event_fd);
            return EXIT_FAILURE;
        }

        ev.events = EPOLLIN;
        ev.data.fd = event_fd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, event_fd, &ev) == -1) {
            tlog(LOG_ERR, "epoll_ctl failed: %s", strerror(errno));
            close(event_fd);
            close(epfd);
            return EXIT_FAILURE;
        }

        if (pthread_create(&event_thread_id, NULL, eventLoopThread, NULL) != 0) {
            tlog(LOG_ERR, "Failed to create event loop thread");
            close(event_fd);
            close(epfd);
            return EXIT_FAILURE;
        }

        char hello[] = MAGIC;
        if (write(event_fd, hello, sizeof(hello)) != sizeof(hello)) {
            tlog(LOG_ERR, "Failed to send handshake");
            close(event_fd);
            close(epfd);
            return EXIT_FAILURE;
        }

        if (waitForInitialization() != 0) {
            tlog(LOG_ERR, "Resource initialization failed");
            return EXIT_FAILURE;
        }
    }

    return 0;
}

void stopEventLoop() {
    if (!event_loop_running) {
        return;
    }

    event_loop_running = 0;

    if (event_thread_id != 0) {
        int waited = 0;
        while (event_loop_running && waited < 50) {
            usleep(100000);
            waited++;
        }

        int ret = pthread_join(event_thread_id, NULL);
        if (ret != 0) {
            tlog(LOG_ERR, "pthread_join failed: %s", strerror(ret));
        }

        event_thread_id = 0;
    }

    if (event_fd != -1) {
        lorieEvent e = {.type = EVENT_STOP_RENDER};
        write(event_fd, &e, sizeof(e));
    }

    waylandDestroyBuffer();
    waylandDestroySharedServerState();

    if (event_fd != -1) {
        close(event_fd);
        event_fd = -1;
    }
    if (epfd != -1) {
        close(epfd);
        epfd = -1;
    }
    if (conn_fd!=-1){
        close(conn_fd);
        conn_fd=-1;
    }

    pthread_mutex_lock(&mutex);
    buffer_ready = 0;
    pthread_mutex_unlock(&mutex);
}
