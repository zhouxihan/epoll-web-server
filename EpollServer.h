#ifndef EPOLLSERVER_H
#define EPOLLSERVER_H

#include <sys/epoll.h>
#include <string>
#include <mutex>
#include "ThreadPool.h"

class EpollServer {
public:
    EpollServer(int port, int thread_num);
    ~EpollServer();
    void start();

private:
    int m_port;
    int m_epoll_fd;
    int m_listen_fd;
    ThreadPool m_pool;

    void setNonBlocking(int fd);
    void handleNewConnection();
    void handleRead(int fd);
};

#endif
