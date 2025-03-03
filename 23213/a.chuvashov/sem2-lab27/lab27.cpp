#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <bits/stdc++.h>

#define BACKLOG 510
using namespace std;
class ProxyServer
{
private:
    vector<struct pollfd> fds;
    unordered_map<int, int> conns, rev_conns;
    int listenfd;
    string remote_host;
    int remote_port;
    struct hostent * host;

    void handle_connection(int index) {
        char buffer[BUFSIZ];
        ssize_t dataRead;

        int client_fd = fds[index].fd;
        int remote_fd = conns.count(fds[index].fd) ? conns[fds[index].fd] : rev_conns[fds[index].fd];
        
        dataRead = recv(client_fd, buffer, BUFSIZ, 0);
        if (dataRead > 0) {
            send(remote_fd, buffer, dataRead, 0);
        } else if (dataRead == 0 || (fds[index].revents & (POLLHUP | POLLERR))){
            close_connection(fds[index].fd);
        }
    }

    void close_connection(int fd) {
        if (conns.count(fd)) {
            int server_fd = conns[fd];
            close(server_fd);
            conns.erase(fd);
            rev_conns.erase(server_fd);
        } else if (rev_conns.count(fd)) {
            int client_fd = rev_conns[fd];
            close(client_fd);
            rev_conns.erase(fd);
            conns.erase(client_fd);
        }

        for (auto i = fds.begin(); i != fds.end(); i++) {
            if (i->fd == fd) {
                close(fd);
                fds.erase(i);
                break;
            }
        }
    }

    int remote_connect() {
        host = gethostbyname(remote_host.data());
        if (host == NULL) {
            herror("gethostbyname");
            return -1;
        }

        int remotefd;
        if ((remotefd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Server socket failure");
            return -1;
        }

        struct sockaddr_in remote_in;
        memset(&remote_in, 0, sizeof(remote_in));
        remote_in.sin_family = AF_INET;
        remote_in.sin_port = htons(remote_port);
        remote_in.sin_addr = *(struct in_addr*)host->h_addr_list[0];

        if (connect(remotefd, (struct sockaddr*)&remote_in, sizeof(remote_in)) < 0) {
            perror("connect");
            close(remotefd);
            return -1;;
        }
        return remotefd;
    }


public:
    ProxyServer(int listen_port, const string remote_host, int remote_port) {
        this->remote_host = remote_host;
        this->remote_port = remote_port;
        
        if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Can`t create socket");
            exit(EXIT_FAILURE);
        }

        int opt = 1;
        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            perror("setsockopt error");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in address_in;
        memset(&address_in, 0, sizeof(struct sockaddr_in));
        address_in.sin_family = AF_INET;
        address_in.sin_addr.s_addr = INADDR_ANY;
        address_in.sin_port = htons(listen_port);

        if (bind(listenfd, (struct sockaddr *)&address_in, sizeof(address_in)) < 0) {
            perror("Bind failure");
            exit(EXIT_FAILURE);
        }

        if (listen(listenfd, BACKLOG) == -1) {
            perror("Listen failure");
            exit(EXIT_FAILURE);
        }
        fds.push_back({listenfd, POLLIN, 0});
    }
    
    void run() {
        while (true) {
            int ret = poll(fds.data(), fds.size(), -1);
            if (ret == -1) {
                perror("Poll");
                continue;
            }

            if (fds[0].revents & POLLIN) {
                int clientfd;
                if ((clientfd = accept(listenfd, nullptr, nullptr)) == -1) {
                    perror("accept failure");
                    continue;
                }
                
                int remotefd = remote_connect();
                if (remotefd == -1) {
                    close(clientfd);
                    continue;
                } 

                if (fds.size() >= BACKLOG * 2 + 1) {
                    fprintf(stderr, "Too many connections\n");
                    close(remotefd);
                    close(clientfd);
                    continue;
                }

                conns[clientfd] = remotefd; 
                rev_conns[remotefd] = clientfd;
                fds.push_back({clientfd, POLLIN | POLLERR | POLLHUP, 0});
                fds.push_back({remotefd, POLLIN | POLLERR | POLLHUP, 0});
            }

            for(size_t i = 1; i < fds.size(); i++) {
                if (fds[i].revents == 0) continue; 
                handle_connection(i);
            }   
        }
    }
    ~ProxyServer() {
        close(listenfd);
        for (auto& p : conns) {
            close(p.first);
            close(p.second);
        }
    }
};

int main(int argc, char** argv) {
    if (argc != 4) {
        cerr << "There should be 3 arguments: " << argv[0] << " <listen_port> <remote_host> <remote_port>\n";
        return EXIT_FAILURE;
    }

    int listen_port;
    if ((listen_port = stoi(argv[1])) < 1) {
        cerr << "Failed to parse listen port\n";
        return EXIT_FAILURE;
    }
    
    char *remote_host = argv[2];

    int remote_port;
    if ((remote_port = stoi(argv[3])) < 1) {
        cerr << "Failed to parse remote port\n";
        return EXIT_FAILURE;
    }

    ProxyServer server(listen_port, remote_host, remote_port);
    server.run();
    
    return EXIT_SUCCESS;
}
