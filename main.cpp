#include "EpollServer.h"

int main() {
    EpollServer server(8888, 4);
    server.start();
    return 0;
}
