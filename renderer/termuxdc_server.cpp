#include "termuxdc_server.h"
#include "termuxdc_event.h"
#include "SocketIPCClient.h"

#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#ifndef SOCKET_NAME
#define SOCKET_NAME     "shard_texture_socket"
#endif

void termuxdc_server::Init() {
    char socketName[108];
    struct sockaddr_un serverAddr;

    dataSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (dataSocket < 0) {
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
        printf("connect: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("%s\n", "Input Server Setup complete.");
    pthread_create(&t, nullptr, work, this);
}

void termuxdc_server::InitDefault() {
    char socketName[108];
    struct sockaddr_un serverAddr;

    dataSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (dataSocket < 0) {
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
        printf("connect: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("%s\n", "Input Server Setup complete.");
}

void termuxdc_server::Destroy() {
    reset();
}

void termuxdc_server::reset() {
    running = false;
    close(dataSocket);
}

int termuxdc_server::GetDataSocket() {
    return dataSocket;
}

termuxdc_event_callback *termuxdc_server::GetCallback() {
    return inputEventCallback;
}

void termuxdc_server::SetCallback(termuxdc_event_callback *ca) {
    inputEventCallback = ca;
}

termuxdc_server::~termuxdc_server() {
    reset();
    delete inputEventCallback;
}

InputHandler termuxdc_server::GetInputHandler() {
    return inputHandler;
}

void termuxdc_server::SetInputHandler(InputHandler handler) {
    inputHandler = handler;
}

bool termuxdc_server::isRunning() {
    return running;
}

void termuxdc_server::setRunning(bool run) {
    running = run;
}

int termuxdc_server::waitEvent(termuxdc_event *event) {
    if (dataSocket > 0) {
        recv(dataSocket, event, sizeof(*event), MSG_WAITALL);
    } else {
        return -1;
    }
    return 0;
}

void *work(void *args) {
    auto *server = static_cast<termuxdc_server *>(args);
    server->setRunning(true);
    while (server->isRunning()) {
        termuxdc_event buf;
        recv(server->GetDataSocket(), &buf, sizeof(buf), MSG_WAITALL);
        if (server->GetInputHandler()) {
            server->GetInputHandler()(buf);
        } else if (server->GetCallback()) {
            server->GetCallback()->callback(buf);
        } else {
            server->setRunning(false);
        }
    }
    return nullptr;
}
