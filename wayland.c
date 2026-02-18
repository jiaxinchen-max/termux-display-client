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
#include "include/wayland.h"
#include "include/tlog.h"

static int conn_fd = -1, epfd = -1, stateFd = -1;
static struct epoll_event ev, events[5];
static int connect_retry = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static volatile int buffer_ready = 0;  // State: 0=not ready, 1=ready, -1=error
static volatile int event_loop_running = 0;  // Control flag for event loop
static pthread_t event_thread_id = 0;  // Event loop thread handle

#define MAX_RETRY_TIMES 5

#define SOCKET_PATH "/data/data/com.termux/files/home/.wayland/unix_socket"

LorieBuffer *lorieBuffer;
struct lorie_shared_server_state *serverState;

JNIEXPORT jstring JNICALL
Java_com_termux_wayland_NativeLib_stringFromJNI(
        JNIEnv *env,
        jobject this) {
    return "hello";
}

bool waylandConnectionAlive(void) {
    return lorieBuffer;
}

static void waylandApplyBuffer() {
    LorieBuffer_recvHandleFromUnixSocket(conn_fd, &lorieBuffer);
    const LorieBuffer_Desc *desc = LorieBuffer_description(lorieBuffer);
    tlog(LOG_INFO, "Receive shared buffer width %d stride %d height %d format %d type %d id %llu",
           desc->width, desc->stride, desc->height, desc->format, desc->type, desc->id);
    lorieEvent e = {.type = EVENT_CLIENT_VERIFY_SUCCEED};
    write(conn_fd, &e, sizeof(e));

    // Signal initialization complete
    pthread_mutex_lock(&mutex);
    buffer_ready = 1;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}

static void waylandDestroyBuffer() {
    unsigned long id;
    if (!lorieBuffer)
        return;  // Not exist or not registered so no need to unregister
    id = LorieBuffer_description(lorieBuffer)->id;
    if (conn_fd != -1) {
        lorieEvent e = {.removeBuffer = {.t = EVENT_DESTROY_BUFFER, .id = id}};
        write(conn_fd, &e, sizeof(e));
    }
}

static void waylandApplySharedServerState() {
    stateFd = ancil_recv_fd(conn_fd);

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
    lorieEvent e = {.type = EVENT_APPLY_BUFFER};
    write(conn_fd, &e, sizeof(e));
    lorieEvent e2 = {.screenSize = {.t = EVENT_SCREEN_SIZE, .width = 1080, .height = 720, .framerate = 30, .format=AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM, .type=LORIEBUFFER_AHARDWAREBUFFER}};
    write(conn_fd, &e2, sizeof(e2));
}

static void waylandDestroySharedServerState() {
    if (conn_fd != -1) {
        lorieEvent e = {.type = EVENT_DESTROY_SERVER_STATE};
        write(conn_fd, &e, sizeof(e));
    }
}

static void *eventLoopThread(void *arg) {
    tlog(LOG_INFO, "Event loop thread started");
    event_loop_running = 1;

    while (event_loop_running) {
        int nfds = epoll_wait(epfd, events, 5, 1000);  // 1 second timeout to check running flag
        if (nfds == -1) {
            if (errno == EINTR) continue; // Interrupted by signal, retry
            tlog(LOG_ERR, "epoll_wait error: %s", strerror(errno));
            break;
        }

        if (nfds == 0) continue;  // Timeout, check running flag again

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == conn_fd) {
                if (events[i].events & EPOLLIN) {
                    lorieEvent e = {0};
                    if (read(conn_fd, &e, sizeof(e)) == sizeof(e)) {
                        switch (e.type) {
                            case EVENT_SERVER_VERIFY_SUCCEED: {
                                tlog(LOG_INFO, "Verification succeeded");
                                lorieEvent req = {.type = EVENT_APPLY_SERVER_STATE};
                                if (write(conn_fd, &req, sizeof(req)) != sizeof(req)) {
                                    tlog(LOG_ERR, "Failed to send APPLY_SERVER_STATE");
                                    goto cleanup;
                                }
                                break;
                            }
                            case EVENT_SHARED_SERVER_STATE: {
                                waylandApplySharedServerState();
                                tlog(LOG_INFO, "Server state initialized");
                                break;
                            }
                            case EVENT_ADD_BUFFER: {
                                waylandApplyBuffer();
                                tlog(LOG_INFO, "Buffer initialized");
                                // Continue running to handle future events
                                break;
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
        buffer_ready = -1;  // Mark as error if not initialized yet
        pthread_cond_signal(&cond);
    }
    pthread_mutex_unlock(&mutex);

    if (epfd != -1) {
        close(epfd);
        epfd = -1;
    }
    if (conn_fd != -1) {
        close(conn_fd);
        conn_fd = -1;
    }

    event_loop_running = 0;
    tlog(LOG_INFO, "Event loop thread exited");
    return NULL;
}

static int waitForInitialization(void) {
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 10;  // 10 seconds timeout

    pthread_mutex_lock(&mutex);
    // Use while loop to prevent spurious wakeups and lost signals
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

    conn_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (conn_fd < 0) {
        tlog(LOG_ERR, "socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    strncpy(serverAddr.sun_path, SOCKET_PATH, sizeof(serverAddr.sun_path) - 1);

    int ret = connect(conn_fd, (const struct sockaddr *) &serverAddr, sizeof(serverAddr));
    if (ret < 0) {
        tlog(LOG_INFO, "connect: %s, retry %d", strerror(errno), connect_retry + 1);
        if (connect_retry >= MAX_RETRY_TIMES - 1) {
            tlog(LOG_ERR, "connect: %s, failed after %d times", strerror(errno),
                   connect_retry + 1);
            close(conn_fd);
            exit(EXIT_FAILURE);
        }
        connect_retry++;
        sleep(5);
    } else {
        tlog(LOG_INFO, "Render connection established");

        // Create epoll instance
        epfd = epoll_create1(0);
        if (epfd == -1) {
            tlog(LOG_ERR, "epoll_create1 failed: %s", strerror(errno));
            close(conn_fd);
            return EXIT_FAILURE;
        }

        // Add connection fd to epoll
        ev.events = EPOLLIN;
        ev.data.fd = conn_fd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &ev) == -1) {
            tlog(LOG_ERR, "epoll_ctl failed: %s", strerror(errno));
            close(conn_fd);
            close(epfd);
            return EXIT_FAILURE;
        }

        // Start event loop thread
        if (pthread_create(&event_thread_id, NULL, eventLoopThread, NULL) != 0) {
            tlog(LOG_ERR, "Failed to create event loop thread");
            close(conn_fd);
            close(epfd);
            return EXIT_FAILURE;
        }

        // Send MAGIC handshake to trigger initialization
        char hello[] = MAGIC;
        if (write(conn_fd, hello, sizeof(hello)) != sizeof(hello)) {
            tlog(LOG_ERR, "Failed to send handshake");
            close(conn_fd);
            close(epfd);
            return EXIT_FAILURE;
        }
        tlog(LOG_INFO, "Handshake sent to server");

        // Wait for initialization to complete
        if (waitForInitialization() != 0) {
            tlog(LOG_ERR, "Resource initialization failed");
            return EXIT_FAILURE;
        }

        tlog(LOG_INFO, "All resources initialized successfully");
        fflush(NULL);  // Flush all output buffers
    }

    tlog(LOG_INFO, "connectToRender() returning 0");
    fflush(NULL);
    return 0;
}

void stopEventLoop(void) {
    if (!event_loop_running) {
        tlog(LOG_WARNING, "Event loop is not running");
        return;
    }

    tlog(LOG_INFO, "Stopping event loop thread...");

    // Signal the thread to exit
    event_loop_running = 0;

    // Wait for thread to finish
    if (event_thread_id != 0) {
        // Give thread some time to exit gracefully (max 5 seconds)
        int waited = 0;
        while (event_loop_running && waited < 50) {
            usleep(100000);  // 100ms
            waited++;
        }

        // Thread should have set event_loop_running to 0 when exiting
        // Now try to join (will block until thread exits)
        int ret = pthread_join(event_thread_id, NULL);
        if (ret == 0) {
            tlog(LOG_INFO, "Event loop thread stopped successfully");
        } else {
            tlog(LOG_ERR, "pthread_join failed: %s", strerror(ret));
        }

        event_thread_id = 0;
    }

    // Cleanup resources (if thread didn't cleanup already)
    if (conn_fd != -1) {
        close(conn_fd);
        conn_fd = -1;
    }
    if (epfd != -1) {
        close(epfd);
        epfd = -1;
    }

    // Reset state
    pthread_mutex_lock(&mutex);
    buffer_ready = 0;
    pthread_mutex_unlock(&mutex);

    // Cleanup buffer resources
    waylandDestroyBuffer();
    waylandDestroySharedServerState();

    tlog(LOG_INFO, "Event loop stopped and resources cleaned up");
}
