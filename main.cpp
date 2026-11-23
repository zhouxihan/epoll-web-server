#include "EpollServer.h"

int main() {
    // 只有 2 个参数：端口号 和 线程数
    EpollServer server(8888, 4);
    server.start();
    return 0;
}
