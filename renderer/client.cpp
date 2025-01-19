#include <sys/timerfd.h>
#include <sys/epoll.h>
#include "client.h"
#include "SocketIPCClient.h"
#include "InputServer.h"
#include "LogUtil.h"

#define MAX_EVENTS 10
static bool isRunning = false;
SocketIPCClient *clientRenderer = nullptr;
static AHardwareBuffer *hwBuffer = nullptr;
static int dataSocket = -1;

static InputServer *inputServer;

#define SOCKET_NAME     "shard_texture_socket"

void ClientSetup() {
    char socketName[108];
    struct sockaddr_un serverAddr;

    dataSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (dataSocket < 0) {
        LOG_E("socket: %s", strerror(errno));
        printf("socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    memcpy(&socketName[0], "\0", 1);
    strcpy(&socketName[1], SOCKET_NAME);

    memset(&serverAddr, 0, sizeof(struct sockaddr_un));
    serverAddr.sun_family = AF_UNIX;
    strncpy(serverAddr.sun_path, socketName, sizeof(serverAddr.sun_path) - 1);

    // connect
    int ret = connect(dataSocket, reinterpret_cast<const sockaddr *>(&serverAddr),
                      sizeof(struct sockaddr_un));
    if (ret < 0) {
        LOG_E("connect: %s", strerror(errno));
        printf("connect: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    LOG_I("Client ClientSetup complete.");
    printf("%s","Client ClientSetup complete.");
}

void DisplayClientInit(uint32_t width, uint32_t height, uint32_t channel) {
    LOG_D("    CLIENT_APP_CMD_INIT_WINDOW");
    printf("%s","    CLIENT_APP_CMD_INIT_WINDOW");
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
            LOG_E("Failed to allocate hardware buffer.");
            exit(EXIT_FAILURE);
        }
    }

    clientRenderer = SocketIPCClient::GetInstance();
    clientRenderer->Init(hwBuffer, dataSocket);
    clientRenderer->SetImageGeometry(width, height, channel);
}

void DisplayClientStart() {
    LOG_D("----------------------------------------------------------------");
    LOG_D("    DisplayClientStart()");
    isRunning = true;

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timer_fd == -1) {
        LOG_E("timerfd_create failed:%s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct itimerspec timer_spec;
    timer_spec.it_interval.tv_sec = 1; // 1-second interval
    timer_spec.it_interval.tv_nsec = 30000000;
    timer_spec.it_value.tv_sec = 0;
    timer_spec.it_value.tv_nsec = 30000000;//初始延迟秒触发

    if (timerfd_settime(timer_fd, 0, &timer_spec, NULL) == -1) {
        LOG_E("timerfd_settime failed:%s", strerror(errno));
        close(timer_fd);
        exit(EXIT_FAILURE);
    }

    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        LOG_E("epoll_create failed:%s", strerror(errno));
        close(timer_fd);
        exit(EXIT_FAILURE);
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = timer_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &event) == -1) {
        LOG_E("epoll_ctl failed:%s", strerror(errno));
        close(epoll_fd);
        close(timer_fd);
        exit(EXIT_FAILURE);
    }

    struct epoll_event events[MAX_EVENTS];

    while (true) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            LOG_E("epoll_wait failed:%s", strerror(errno));
            close(epoll_fd);
            close(timer_fd);
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < num_events; ++i) {
            if (events[i].data.fd == timer_fd) {
                uint64_t expirations;
                ssize_t s = read(timer_fd, &expirations, sizeof(expirations));
                if (s != sizeof(expirations)) {
                    LOG_E("read failed:%s", strerror(errno));
                    close(epoll_fd);
                    close(timer_fd);
                    exit(EXIT_FAILURE);
                }
                // LOG_I("Client Timer expired!");
                // Add your code to handle timer expiration asynchronously
                if (!isRunning) {
                    break;
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
void DisplayDraw(const uint8_t* data){
    if (clientRenderer){
        clientRenderer->Draw(data);
    }
}
void InputInit(InputHandler handler){
    inputServer = new InputServer;
    inputServer->Init();
    inputServer->SetInputHandler(handler);
};