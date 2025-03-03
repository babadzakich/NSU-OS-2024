#include <ctype.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

int working = 1;

void handle_sigint(int sig) {
    working = 0;
}


int create_socket(char* server_host, int server_port) {
    int descriptor;
    if ((descriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket failure");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(descriptor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt error");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_host, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    
    if (bind(descriptor, (struct sockaddr* )&addr, sizeof(addr)) == -1) {
        perror("Bind failure");
        exit(EXIT_FAILURE);
    }

    if (listen(descriptor, 0) == -1) {
        perror("listen failure");
        exit(EXIT_FAILURE);
    }
    return descriptor;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* server_host = argv[1];
    int server_port;
    if ((server_port = atoi(argv[2])) < 1) {
        fprintf(stderr, "Failed to parse remote port\n");
        exit(EXIT_FAILURE);
    }

    int clientDescriptor, descriptor = create_socket(server_host, server_port);

    // signal(SIGINT, handle_sigint);

    char buffer[BUFSIZ];
    while(working) {
        if ((clientDescriptor = accept(descriptor, NULL, NULL)) == -1) {
            perror("Accept failure");
            continue;
        }
        
        size_t bufLen;
        while (working && (bufLen = read(clientDescriptor, buffer, BUFSIZ)) > 0)
        {
            if (write(clientDescriptor, buffer, bufLen) != bufLen) {
                perror("write fail");
                break;
            }
        }

        if (bufLen == -1) {
            perror("Read failure");
        }
    }
    exit(EXIT_SUCCESS);
}
