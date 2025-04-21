#ifndef PROXY_HEADER
#define PROXY_HEADER

#include <iostream>
#include <fstream>
#include <algorithm>
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
#include <sys/time.h>
#include <fcntl.h>

#include "Cache.h"
#include "HTTP_Parser.h"

#define MAX_CONNS 510

using namespace std;

class Proxy {
private:
    vector<struct pollfd> fds;
    unordered_map<int, int> conns, rev_conns;
    unordered_map<int, HTTP_Request> clients_requests;
    unordered_map<int, pair<string, bool>> unprocessed_servers_responses;
    unordered_map<string, vector<pair<string, int>>> awaiting_requests;
    unordered_map<int, pair<string, bool>> unprocessed_requests;

    string error_page;
    int listenfd;
    int listen_port;
    bool working_flag;

    void close_connection(int fd_index);

    void get_uncacheable_body(int index);

    void get_cacheable_body(int index);

    void remote_connect(int index);

    int connect_to_remote_server(const string& host, int port, int clientfd);

    int request_first_line(int index);

    int request_all_lines(int index);

    void get_server_response(int index);

public:
    Proxy(int listen_port);

    void run();

    void setFlag(bool flag) {
        working_flag = flag;
    };
};

#endif