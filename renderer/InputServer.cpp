#include "InputServer.h"
#include "LogUtil.h"
#include "InputEvent.h"

#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#ifndef SOCKET_NAME
#define SOCKET_NAME     "shard_texture_socket"
#endif

void InputServer::Init() {
    char socketName[108];
    struct sockaddr_un serverAddr;

    dataSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (dataSocket < 0) {
        LOG_E("socket: %s", strerror(errno));
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
        exit(EXIT_FAILURE);
    }

    LOG_I("Input Server Setup complete.");
    pthread_create(&t, nullptr, work, this);
}

void InputServer::Destroy() {
    reset();
}

void InputServer::reset() {
    close(dataSocket);
}

int InputServer::GetDataSocket() {
    return dataSocket;
}

InputEventCallback *InputServer::GetCallback() {
    return inputEventCallback;
}

void InputServer::SetCallback(InputEventCallback *ca) {
    inputEventCallback = ca;
}

InputServer::~InputServer() {
    reset();
    delete inputEventCallback;
}

InputHandler InputServer::GetInputHandler() {
    return inputHandler;
}

void InputServer::SetInputHandler(InputHandler handler) {
    inputHandler = handler;
}

void *work(void *args) {
    auto *server = static_cast<InputServer *>(args);
    bool running = true;
    while (running) {
        InputEvent buf;
        recv(server->GetDataSocket(), &buf, sizeof(buf), MSG_WAITALL);
        if (server->GetInputHandler()) {
            server->GetInputHandler()(buf);
        } else if (server->GetCallback()) {
            server->GetCallback()->callback(buf);
        } else {
            running = false;
        }
    }
    return nullptr;
}
