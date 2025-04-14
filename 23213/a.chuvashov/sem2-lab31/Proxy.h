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
#include "HTTP_Parser.h"

#define MAX_CONNS 510

using namespace std;

class Proxy {
private:
    vector<struct pollfd> fds;
    unordered_map<int, int> conns, rev_conns;
    unordered_map<int, HTTP_Request> clients_requests;

    const string error_page = "HTTP/1.1 504 Gateway Timeout\r\n"
"Content-Type: text/html\r\n"
"Connection: close\r\n"
"Content-Length: 1512\r\n"
"\r\n"
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"    <meta charset=\"utf-8\">\n"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
"    <title>504 - Gateway Timeout</title>\n"
"    <style>\n"
"        body {\n"
"            font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", Roboto, Helvetica, Arial, sans-serif;\n"
"            line-height: 1.6;\n"
"            color: #333;\n"
"            margin: 0;\n"
"            padding: 20px;\n"
"            text-align: center;\n"
"        }\n"
"        \n"
"        .container {\n"
"            max-width: 800px;\n"
"            margin: 50px auto;\n"
"        }\n"
"        \n"
"        h1 {\n"
"            color: #dc3545;\n"
"            font-size: 2.5em;\n"
"            margin-bottom: 20px;\n"
"        }\n"
"        \n"
"        .error-code {\n"
"            font-size: 1.2em;\n"
"            color: #666;\n"
"            margin-bottom: 30px;\n"
"        }\n"
"        \n"
"        .icon {\n"
"            font-size: 4em;\n"
"            color: #ffc107;\n"
"            margin-bottom: 20px;\n"
"        }\n"
"        \n"
"        .contact {\n"
"            margin-top: 40px;\n"
"            padding-top: 20px;\n"
"            border-top: 1px solid #eee;\n"
"            color: #666;\n"
"        }\n"
"        \n"
"        a {\n"
"            color: #007bff;\n"
"            text-decoration: none;\n"
"        }\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"    <div class=\"container\">\n"
"        <div class=\"icon\">⚠️</div>\n"
"        <h1>Connection Failed</h1>\n"
"        \n"
"        <div class=\"error-code\">\n"
"            Error 504 - Gateway Timeout\n"
"        </div>\n"
"\n"
"        <p>We're unable to establish a connection to the upstream server.</p>\n"
"        <p>This could be due to:\n"
"            <ul style=\"list-style: none; padding: 0;\">\n"
"                <li>• Temporary network issues</li>\n"
"                <li>• Server maintenance</li>\n"
"                <li>• High traffic load</li>\n"
"            </ul>\n"
"        </p>\n"
"\n"
"        <p>Please try:\n"
"            <ul style=\"list-style: none; padding: 0;\">\n"
"                <li>• Refreshing the page</li>\n"
"                <li>• Checking your network connection</li>\n"
"                <li>• Coming back later</li>\n"
"            </ul>\n"
"        </p>\n"
"\n"
"        <div class=\"contact\">\n"
"            Need immediate assistance? <a href=\"mailto:support@example.com\">Contact support</a>\n"
"            or return to <a href=\"/\">home page</a>.\n"
"        </div>\n"
"    </div>\n"
"</body>\n"
"</html>\n";
;

    int listenfd;
    int listen_port;
    bool working_flag;

    void close_connection(int fd_index);

    void handle_uncacheable_connection(int index);

    void handle_cacheable_connection(int index);
    
    int remote_connect(int clientfd);
    
public:
    Proxy(int listen_port);    

    void run();

    void setFlag(bool flag) {
        working_flag = flag;
    };
};

#endif