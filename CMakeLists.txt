cmake_minimum_required(VERSION 3.10)
project(EpollServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(server 
    main.cpp 
    EpollServer.cpp 
    ThreadPool.cpp
)

# ★ 加上 sqlite3
target_link_libraries(server PRIVATE Threads::Threads sqlite3)
