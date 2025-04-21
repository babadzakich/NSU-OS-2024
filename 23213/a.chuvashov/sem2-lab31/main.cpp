#include "Proxy.h"
#include <iostream>
#include <signal.h>

Proxy* proxy = nullptr;

void handle_sigint(int sig) {
        proxy->setFlag(false);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    int listen_port;

    if ((listen_port = atoi(argv[1])) < 1) {
        cerr << "Failed to parse listen port\n";
        return EXIT_FAILURE;
    }
    Proxy p = Proxy(listen_port);
    proxy = &p;
    proxy->setFlag(true);
    signal(SIGINT, handle_sigint);
    proxy->run();

    return EXIT_SUCCESS;
}