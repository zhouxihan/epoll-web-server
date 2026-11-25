#ifndef EPOLLSERVER_H
#define EPOLLSERVER_H

#include <sys/epoll.h>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <sqlite3.h>
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
    std::mutex m_mutex; 
    
    sqlite3* m_db;
    std::atomic<bool> m_running;

    void setNonBlocking(int fd);
    void handleNewConnection();
    void handleRead(int fd);
    void handleHttpRequest(int fd, const std::string& request);

    void initDatabase();
    void startDataCollection();
};

#endif
