#include "EpollServer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <cstdio>
#include <mutex>

EpollServer::EpollServer(int port, int thread_num) 
    : m_port(port), m_pool(thread_num), m_running(true)
{
    initDatabase();
    startDataCollection();

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

    std::cout << "Server started on http://127.0.0.1:" << m_port << std::endl;
}

EpollServer::~EpollServer() {
    m_running = false;
    close(m_listen_fd);
    close(m_epoll_fd);
    sqlite3_close(m_db);
}

void EpollServer::initDatabase() {
    if (sqlite3_open("device_data.db", &m_db) != SQLITE_OK) {
        std::cerr << "无法打开数据库!" << std::endl;
        exit(1);
    }
    const char* sql = "CREATE TABLE IF NOT EXISTS temp_log (id INTEGER PRIMARY KEY AUTOINCREMENT, time TEXT, value REAL);";
    char* errMsg = nullptr;
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "建表失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
}

void EpollServer::startDataCollection() {
    std::thread([this]() {
        while (m_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            auto now = std::chrono::system_clock::now();
            std::time_t now_time = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&now_time), "%H:%M:%S");
            double temp = 25.0 + (rand() % 50) / 10.0;

            std::lock_guard<std::mutex> lock(m_mutex);
            std::string sql = "INSERT INTO temp_log (time, value) VALUES ('" + ss.str() + "', " + std::to_string(temp) + ");";
            sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, nullptr);
        }
    }).detach();
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
    m_pool.enqueue([this, fd]() {
        char buf[4096];
        int n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            handleHttpRequest(fd, std::string(buf));
        }
        close(fd);
    });
}

void EpollServer::handleHttpRequest(int fd, const std::string& request) {
    std::string path = "/index.html";
    size_t start = request.find("GET ");
    if (start != std::string::npos) {
        size_t end = request.find(" HTTP", start);
        if (end != std::string::npos) {
            path = request.substr(start + 4, end - start - 4);
        }
    }
    if (path == "/") path = "/index.html";

    if (path == "/api/history") {
        std::string json = "[";
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            sqlite3_stmt* stmt;
            const char* sql = "SELECT time, value FROM temp_log ORDER BY id DESC LIMIT 20;";
            if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                bool first = true;
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    if (!first) json += ",";
                    std::string time = (const char*)sqlite3_column_text(stmt, 0);
                    double value = sqlite3_column_double(stmt, 1);
                    json += "{\"time\": \"" + time + "\", \"value\": " + std::to_string(value) + "}";
                    first = false;
                }
                sqlite3_finalize(stmt);
            }
        }
        json += "]";

        std::string header = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
        write(fd, (header + json).c_str(), (header + json).size());
        return;
    }

    std::string filepath = "./www" + path;
    FILE* file = fopen(filepath.c_str(), "rb");
    if (file) {
        std::string header = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n";
        write(fd, header.c_str(), header.size());
        char file_buf[4096];
        size_t bytes_read;
        while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), file)) > 0) {
            write(fd, file_buf, bytes_read);
        }
        fclose(file);
    } else {
        std::string not_found = "HTTP/1.1 404 Not Found\r\n\r\n<h1>404 Not Found</h1>";
        write(fd, not_found.c_str(), not_found.size());
    }
}
