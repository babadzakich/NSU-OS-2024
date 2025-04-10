#ifndef PROXY_HEADER
#define PROXY_HEADER

#include <iostream>
#include <stdio.h>
#include <vector>
#include <unordered_set>
#include <string>
#include <cstring>
#include <unordered_map>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include "Cache.h"

#define MAX_CONNS 510
#define MAX_CACHEABLE_SIZE (50 * 1024 * 1024) // 50MB max cacheable file size

using namespace std;

class Proxy {
private:
    vector<struct pollfd> fds;
    unordered_map<int, int> conns, rev_conns;
    int listenfd;
    int listen_port;
    bool working_flag;
    Cache cache;

    void close_connection(int fd_index);

    void handle_connection(int index);
    
    int remote_connect(int clientfd);
    
    string accumulate_response(int fd);
    
public:
    Proxy(int listen_port);    

    void run();

    void setFlag(bool flag) {
        working_flag = flag;
    };
};

#endif