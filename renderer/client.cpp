#include <sys/timerfd.h>
#include <sys/epoll.h>
#include "client.h"
#include "SocketIPCClient.h"
#include "InputServer.h"

#define MAX_EVENTS 10
static bool isRunning = false;
SocketIPCClient *clientRenderer = nullptr;
static AHardwareBuffer *hwBuffer = nullptr;
static int dataSocket = -1;
static int connect_retry = 0;
static  int timer_fd =-1;
static int epoll_fd =-1;
#define MAX_RETRY_TIMES 12

#define SIGTERM_MSG "KILL | SIGTERM received.\n"
#define SOCKET_NAME     "shard_texture_socket"
static InputServer *inputServer;

void SigTermHandler(int signum, siginfo_t *info, void *ptr) {
    write(STDERR_FILENO, SIGTERM_MSG, sizeof(SIGTERM_MSG));
    InputEvent ev = {.type=EVENT_CLIENT_EXIT};
    send(inputServer->GetDataSocket(), &ev, sizeof(ev), MSG_DONTWAIT);
    isRunning = false;
    close(epoll_fd);
    close(timer_fd);
    inputServer->Destroy();
    clientRenderer->Destroy();
    AHardwareBuffer_release(hwBuffer);
    close(dataSocket);
}

void CatchSigterm() {
    static struct sigaction sigact;

    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_sigaction = SigTermHandler;
    sigact.sa_flags = SA_SIGINFO;

//    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGINT, &sigact, NULL);
}

void ClientSetup() {
    char socketName[108];
    struct sockaddr_un serverAddr;

    dataSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (dataSocket < 0) {
        printf("socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    memcpy(&socketName[0], "\0", 1);
    strcpy(&socketName[1], SOCKET_NAME);

    memset(&serverAddr, 0, sizeof(struct sockaddr_un));
    serverAddr.sun_family = AF_UNIX;
    strncpy(serverAddr.sun_path, socketName, sizeof(serverAddr.sun_path) - 1);

    // connect
    while (connect_retry < MAX_RETRY_TIMES) {
        int ret = connect(dataSocket, reinterpret_cast<const sockaddr *>(&serverAddr),
                          sizeof(struct sockaddr_un));
        if (ret < 0) {
            printf("connect: %s, retry %d\n", strerror(errno), connect_retry + 1);
            if (connect_retry >= MAX_RETRY_TIMES) {
                printf("connect: %s, failed after %d times\n", strerror(errno), connect_retry + 1);
                exit(EXIT_FAILURE);
            }
            connect_retry++;
            sleep(5);
        } else {
            break;
        }
    }
    CatchSigterm();
    printf("%s\n", "Client ClientSetup complete.");
}

void DisplayClientInit(uint32_t width, uint32_t height, uint32_t channel) {
    printf("%s\n", "    CLIENT_APP_CMD_INIT");
    sleep(1);
    if (dataSocket < 0) {
        ClientSetup();
        AHardwareBuffer_Desc hwDesc;
        hwDesc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
        hwDesc.width = width;
        hwDesc.height = height;
        hwDesc.layers = 1;
        hwDesc.rfu0 = 0;
        hwDesc.rfu1 = 0;
        hwDesc.stride = 0;
        hwDesc.usage =
                AHARDWAREBUFFER_USAGE_CPU_READ_NEVER | AHARDWAREBUFFER_USAGE_CPU_WRITE_NEVER
                | AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT |
                AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
        int rtCode = AHardwareBuffer_allocate(&hwDesc, &hwBuffer);
        if (rtCode != 0 || !hwBuffer) {
            printf("%s\n", "Failed to allocate hardware buffer.");
            exit(EXIT_FAILURE);
        }
    }

    clientRenderer = SocketIPCClient::GetInstance();
    clientRenderer->Init(hwBuffer, dataSocket);
    clientRenderer->SetImageGeometry(width, height, channel);
}

void DisplayClientStart() {
    printf("%s\n","----------------------------------------------------------------");
    printf("%s\n","    DisplayClientStart()");
    isRunning = true;

    timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timer_fd == -1) {
        printf("timerfd_create failed:%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct itimerspec timer_spec;
    timer_spec.it_interval.tv_sec = 1; // 1-second interval
    timer_spec.it_interval.tv_nsec = 30000000;
    timer_spec.it_value.tv_sec = 0;
    timer_spec.it_value.tv_nsec = 30000000;//初始延迟秒触发

    if (timerfd_settime(timer_fd, 0, &timer_spec, NULL) == -1) {
        printf("timerfd_settime failed:%s\n", strerror(errno));
        close(timer_fd);
        exit(EXIT_FAILURE);
    }

    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        printf("epoll_create failed:%s\n", strerror(errno));
        close(timer_fd);
        exit(EXIT_FAILURE);
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = timer_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &event) == -1) {
        printf("epoll_ctl failed:%s\n", strerror(errno));
        close(epoll_fd);
        close(timer_fd);
        exit(EXIT_FAILURE);
    }

    struct epoll_event events[MAX_EVENTS];

    while (true) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            printf("epoll_wait failed:%s\n", strerror(errno));
            close(epoll_fd);
            close(timer_fd);
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < num_events; ++i) {
            if (events[i].data.fd == timer_fd) {
                uint64_t expirations;
                ssize_t s = read(timer_fd, &expirations, sizeof(expirations));
                if (s != sizeof(expirations)) {
                    printf("read failed:%s\n", strerror(errno));
                    close(epoll_fd);
                    close(timer_fd);
                    exit(EXIT_FAILURE);
                }
                // LOG_I("Client Timer expired!");
                // Add your code to handle timer expiration asynchronously
                if (!isRunning) {
                    printf("receive kill signal\n");
                    close(epoll_fd);
                    close(timer_fd);
                    AHardwareBuffer_release(hwBuffer);
                    return;
                }
                if (clientRenderer) {
                    clientRenderer->Draw();
                }
            }
        }
    }
    close(epoll_fd);
    close(timer_fd);
}

void DisplayDraw(const uint8_t *data) {
    if (clientRenderer) {
        clientRenderer->Draw(data);
    }
}

void InputInit(InputHandler handler) {
    inputServer = new InputServer;
    inputServer->Init();
    inputServer->SetInputHandler(handler);
};
