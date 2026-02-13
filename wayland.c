#include <jni.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <pthread.h>
#include "include/buffer.h"
#include "include/wayland.h"
#include "include/syslog.h"

static int conn_fd = -1, epfd = -1, stateFd = -1;
static struct epoll_event ev, events[5];
static int connect_retry = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

#define MAX_RETRY_TIMES 5

#define SIGTERM_MSG "\nKILL | SIGTERM received."
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

static void OsVendorInit(void) {
    pthread_mutexattr_t mutex_attr;
    pthread_condattr_t cond_attr;


    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&serverState->lock, &mutex_attr);
    pthread_mutex_init(&serverState->cursor.lock, &mutex_attr);

    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&serverState->cond, &cond_attr);
}

static void waylandApplyBuffer() {
    LorieBuffer_recvHandleFromUnixSocket(conn_fd, &lorieBuffer);
    const LorieBuffer_Desc *desc = LorieBuffer_description(lorieBuffer);
    syslog(LOG_INFO, "Receive shared buffer width %d stride %d height %d format %d type %d id %llu",
           desc->width, desc->stride, desc->height, desc->format, desc->type, desc->id);
    OsVendorInit();
    pthread_mutex_lock(&mutex);
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
        syslog(LOG_ERR, "Failed to parse server state: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    serverState = mmap(NULL, sizeof(*serverState), PROT_READ | PROT_WRITE, MAP_SHARED, stateFd, 0);
    if (!serverState || serverState == MAP_FAILED) {
        syslog(LOG_ERR, "Failed to map server state: %s", strerror(errno));
        serverState = NULL;
        exit(EXIT_FAILURE);
    }
    close(stateFd);
    lorieEvent e = {.type = EVENT_APPLY_BUFFER};
    write(conn_fd, &e, sizeof(e));
    lorieEvent e2 = {.screenSize = { .t = EVENT_SCREEN_SIZE, .width = 1080, .height = 720, .framerate = 30, } };
    write(conn_fd, &e2, sizeof(e2));
}

static void waylandDestroySharedServerState() {
    if (conn_fd != -1) {
        lorieEvent e = {.type = EVENT_DESTROY_SERVER_STATE};
        write(conn_fd, &e, sizeof(e));
    }
}

static void sigTermHandler(int signum, siginfo_t *info, void *ptr) {
    waylandDestroySharedServerState();
    waylandDestroyBuffer();
    write(STDERR_FILENO, SIGTERM_MSG, sizeof(SIGTERM_MSG));
}

static void catchSigTerm() {
    static struct sigaction sigact;

    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_sigaction = sigTermHandler;
    sigact.sa_flags = SA_SIGINFO;
    sigaction(SIGINT, &sigact, NULL);
}

static int callback() {
    while (1) {
        int nfds = epoll_wait(epfd, events, 5, -1);
        if (nfds == -1) {
            if (errno == EINTR) continue; // 被信号中断，重试
            syslog(LOG_INFO, "epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == conn_fd) {
                if (events[i].events & EPOLLIN) {
                    lorieEvent e = {0};
                    if (read(conn_fd, &e, sizeof(e)) == sizeof(e)) {
                        switch (e.type) {
                            case EVENT_VERIFY_SUCCEED: {
                                lorieEvent e = {.type = EVENT_APPLY_SERVER_STATE};
                                ssize_t written_n = write(conn_fd, &e, sizeof(e));
                                if (written_n != sizeof(e)) {
                                    syslog(LOG_ERR, "write failed");
                                    close(conn_fd);
                                    close(epfd);
                                    return EXIT_FAILURE;
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
                        }
                    } else {
                        syslog(LOG_ERR, "Incomplete package detected.");
                        goto cleanup;
                    }
                } else if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    syslog(LOG_ERR, "EPOLL error/hangup detected.");
                    goto cleanup;
                }
            }
        }
    }

    cleanup:
    close(conn_fd);
    close(epfd);
    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    return EXIT_SUCCESS;
}

static int waylandRenderConnected(void) {
    pthread_t t;
    pthread_create(&t, NULL, (void *(*)(void *)) callback, NULL);
    pthread_mutex_lock(&mutex);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
    pthread_join(t, NULL);
    return 0;
}

int connectToRender() {
    struct sockaddr_un serverAddr;

    conn_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (conn_fd < 0) {
        syslog(LOG_ERR, "socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    strncpy(serverAddr.sun_path, SOCKET_PATH, sizeof(serverAddr.sun_path) - 1);

    int ret = connect(conn_fd, (const struct sockaddr *) &serverAddr, sizeof(serverAddr));
    if (ret < 0) {
        syslog(LOG_INFO, "connect: %s, retry %d", strerror(errno), connect_retry + 1);
        if (connect_retry >= MAX_RETRY_TIMES - 1) {
            syslog(LOG_ERR, "connect: %s, failed after %d times", strerror(errno),
                   connect_retry + 1);
            close(conn_fd);
            exit(EXIT_FAILURE);
        }
        connect_retry++;
        sleep(5);
    } else {
        catchSigTerm();
        syslog(LOG_INFO, "render connect succeed.");
        epfd = epoll_create1(0);
        if (epfd == -1) {
            syslog(LOG_ERR, "epoll_create1 failed");
            close(conn_fd);
            return EXIT_FAILURE;
        }
        ev.events = EPOLLIN;
        ev.data.fd = conn_fd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &ev) == -1) {
            syslog(LOG_ERR, "epoll_ctl failed");
            close(conn_fd);
            close(epfd);
            return EXIT_FAILURE;
        }
        char hello[] = MAGIC;
        ssize_t written_n = write(conn_fd, hello, sizeof(hello));
        if (written_n != sizeof(hello)) {
            syslog(LOG_ERR, "write magic code failed");
            close(conn_fd);
            close(epfd);
            return EXIT_FAILURE;
        }
        syslog(LOG_INFO, "Sent event to server: type=%s", hello);
        waylandRenderConnected();
    }

    return 0;
}
