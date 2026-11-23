#include "EpollServer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <sstream>

EpollServer::EpollServer(int port, int thread_num) 
    : m_port(port), m_pool(thread_num) 
{
    m_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    setNonBlocking(m_listen_fd);
    int opt = 1;
    setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);
    bind(m_listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(m_listen_fd, 128);

    m_epoll_fd = epoll_create1(0);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = m_listen_fd;
    epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_listen_fd, &ev);

    std::cout << "Web Server started on http://127.0.0.1:" << m_port << std::endl;
}

EpollServer::~EpollServer() {
    close(m_listen_fd);
    close(m_epoll_fd);
}

void EpollServer::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void EpollServer::start() {
    struct epoll_event events[1024];
    while (true) {
        int num = epoll_wait(m_epoll_fd, events, 1024, -1);
        for (int i = 0; i < num; ++i) {
            int fd = events[i].data.fd;
            if (fd == m_listen_fd) {
                handleNewConnection();
            } else if (events[i].events & EPOLLIN) {
                handleRead(fd);
            }
        }
    }
}

void EpollServer::handleNewConnection() {
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int client_fd = accept(m_listen_fd, (struct sockaddr*)&client_addr, &len);
    if (client_fd < 0) return;

    setNonBlocking(client_fd);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET; 
    ev.data.fd = client_fd;
    epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
}

void EpollServer::handleRead(int fd) {
    // 丢进线程池去处理 HTTP 请求
    m_pool.enqueue([this, fd]() {
        char buf[4096];
        int n = read(fd, buf, sizeof(buf) - 1);
        
        if (n > 0) {
            buf[n] = '\0';
            std::string request(buf);
            
            // 极其简陋的 HTTP 解析（只找请求路径）
            std::string path = "/index.html"; // 默认首页
            size_t start = request.find("GET ");
            if (start != std::string::npos) {
                size_t end = request.find(" HTTP", start);
                path = request.substr(start + 4, end - start - 4);
            }
            if (path == "/") path = "/index.html";

            // 去 www 文件夹下找文件
            std::string filepath = "./www" + path;
            FILE* file = fopen(filepath.c_str(), "rb");

            if (file) {
                // 找到了文件，拼接 200 OK 响应头
                std::string header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
                write(fd, header.c_str(), header.size());
                
                // 将文件内容发回去
                char file_buf[1024];
                size_t bytes_read;
                while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), file)) > 0) {
                    write(fd, file_buf, bytes_read);
                }
                fclose(file);
                std::cout << "服务了网页请求: " << path << std::endl;
            } else {
                // 没找到文件，返回 404
                std::string not_found = "HTTP/1.1 404 Not Found\r\n\r\n<h1>404 Not Found</h1>";
                write(fd, not_found.c_str(), not_found.size());
                std::cout << "404 找不到: " << path << std::endl;
            }
        }
        
        // HTTP 是无状态的短连接，回复完就关闭（简化版）
        close(fd);
    });
}
