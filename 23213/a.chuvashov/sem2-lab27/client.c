#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char** argv) {
    char* remote_host = argv[1];
    int remote_port;
    if ((remote_port = atoi(argv[2])) < 1) {
        fprintf(stderr, "Failed to parse remote port\n");
        return EXIT_FAILURE;
    }

    struct hostent * host = gethostbyname(remote_host);
    if (host == NULL) {
        herror("gethostbyname");
        return -1;
    }

    int descriptor;
    if ((descriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket failure");
        exit(-1);
    }

    char buffer[BUFSIZ];
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(remote_port);
    addr.sin_addr = *(struct in_addr *)host->h_addr_list[0];
    if ((connect(descriptor, (struct sockaddr* )&addr, sizeof(addr))) == -1) {
        perror("Connect failure");
        exit(-1);
    }

    size_t bytes;
    while((bytes = read(0, buffer, BUFSIZ)) > 0) {
        if (write(descriptor, buffer, bytes) == -1) {
            perror("Write failure");
            exit(-1);
        } 
    }

    if (bytes == -1) {
        perror("Read failure");
        exit(-1);
    }

    exit(0);
}
