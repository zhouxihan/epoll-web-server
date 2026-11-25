# 基于 epoll 的轻量级边缘设备 Web 监控终端

## 项目简介

基于 C++17 开发的轻量级嵌入式监控后台。采用 Reactor 架构，底层使用 epoll 边缘触发 (ET) 模式与线程池实现高并发网络通信；集成 SQLite 嵌入式数据库实现数据持久化；配合前端 ECharts 实现设备状态（如温度）的实时无刷新动态监控。

> 💡 **应用场景**：适用于工控机、树莓派、边缘网关等资源受限设备。现场工程师无需安装任何客户端，只需通过手机或电脑浏览器即可直接访问局域网 IP，实现跨平台的设备运行状态监控。

## 技术栈

- **语言**：C++17
- **网络**：Linux Socket, epoll (ET模式), 非阻塞 I/O
- **并发**：C++11 线程池 (`std::thread`, `std::mutex`, `std::condition_variable`)
- **协议**：HTTP 1.1
- **数据库**：SQLite3 (嵌入式关系型数据库)
- **前端**：HTML5, JavaScript, ECharts (数据可视化)
- **构建**：CMake

## 核心特性

- **高并发网络架构**：采用 epoll ET 模式，主线程分发事件，工作线程池处理业务，彻底解耦网络 I/O 与计算，单机支持万级并发连接。
- **数据持久化**：后台独立采集线程每秒模拟采集一次设备温度，通过 C++ 封装 SQLite API，实现历史数据的本地持久化存储。
- **动态 API 接口**：实现 HTTP GET 路由解析，提供 `/api/history` 接口。服务器执行 SQL 查询后，动态拼装 JSON 格式响应下发至前端。
- **前端实时监控**：前端利用 Ajax 轮询请求接口，配合 ECharts 绘制带渐变发光效果的动态折线图，实现数据的无刷新呈现。
- **线程安全**：针对多线程并发访问连接池与数据库，引入 `std::mutex` 与 `std::lock_guard` 进行临界区保护，杜绝内存踩踏（Double Free）。

## 快速开始

### 1. 环境依赖
需要 Linux 环境，并安装以下依赖库：
```bash
sudo apt-get update
sudo apt-get install libsqlite3-dev cmake g++
