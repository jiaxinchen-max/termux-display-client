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

void termuxdc_server::init() {
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

void termuxdc_server::initDefault() {
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

void termuxdc_server::destroy() {
    reset();
}

void termuxdc_server::reset() {
    running = false;
    close(dataSocket);
}

int termuxdc_server::getDataSocket() {
    return dataSocket;
}

termuxdc_event_callback *termuxdc_server::getCallback() {
    return inputEventCallback;
}

void termuxdc_server::setCallback(termuxdc_event_callback *ca) {
    inputEventCallback = ca;
}

termuxdc_server::~termuxdc_server() {
    reset();
    delete inputEventCallback;
}

InputHandler termuxdc_server::getInputHandler() {
    return inputHandler;
}

void termuxdc_server::setInputHandler(InputHandler handler) {
    inputHandler = handler;
}

bool termuxdc_server::isRunning() {
    return running;
}

void termuxdc_server::setRunning(bool run) {
    running = run;
}
ssize_t termuxdc_server::recvEvent(void *buffer, size_t length) {
    size_t total_received = 0;
    ssize_t bytes_received;
    char *buf = (char *)buffer;
    while (total_received < length) {
        bytes_received = recv(dataSocket, buf + total_received, length - total_received, MSG_WAITALL);
        if (bytes_received <= 0) {
            // Error or connection closed
            return bytes_received;
        }
        total_received += bytes_received;
    }
    return total_received;
}
int termuxdc_server::waitEvent(termuxdc_event *event) {
//    printf("<======waitEvent========>\n");
    if (dataSocket > 0) {
        ssize_t result = recvEvent(event, sizeof(termuxdc_event));
        if (result < 0) {
//            perror("recv");
            return result;
        } else if (result == 0) {
//            printf("Connection closed by peer\n");
            return -1;
        } else {
//            printf("Received %zd bytes of data\n", result);
            return result-sizeof(termuxdc_event);
        }
    } else {
        return -1;
    }
}
int termuxdc_server::sendEvent(termuxdc_event *event){
    send(dataSocket, event, sizeof(termuxdc_event), MSG_DONTWAIT);
}

void *work(void *args) {
    auto *server = static_cast<termuxdc_server *>(args);
    server->setRunning(true);
    while (server->isRunning()) {
        termuxdc_event buf;
        server->waitEvent(&buf);
        if (buf.type==EVENT_FRAME_COMPLETE){
            termuxdc_event ev = {.type=EVENT_DRAW_FRAME};
            server->sendEvent(&ev);
        }
        if (server->getInputHandler()) {
            server->getInputHandler()(buf);
        } else if (server->getCallback()) {
            server->getCallback()->callback(buf);
        } else {
            server->setRunning(false);
        }
    }
    return nullptr;
}
