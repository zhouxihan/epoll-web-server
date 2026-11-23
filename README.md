# 基于 Linux epoll 与线程池的高并发 Web 服务器

## 项目简介
基于 C++17 开发的轻量级静态 Web 服务器。采用 Reactor 架构，底层使用 epoll 边缘触发 (ET) 模式与线程池，支持解析 HTTP GET 请求并返回静态资源。

## 技术栈
- **语言**：C++17
- **网络**：Linux Socket, epoll (ET模式), 非阻塞 I/O
- **并发**：C++11 线程池 (`std::thread`, `std::mutex`, `std::condition_variable`)
- **协议**：HTTP 1.1
- **构建**：CMake

## 核心特性
- 采用 epoll ET 模式，单机支持万级并发连接。
- 主从 Reactor 模型：主线程分发事件，工作线程池处理业务，彻底解耦 I/O 与计算。
- 状态机解析 HTTP 报文，支持返回 HTML、CSS 等静态资源及 404 响应。
- 加锁保护多线程共享状态，杜绝数据竞争与内存踩踏。

