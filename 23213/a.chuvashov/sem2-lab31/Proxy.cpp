#include "Proxy.h"
#include "HTTP_Parser.h"

using namespace std;

void Proxy::close_connection(int fd_index) {
    int fd = fds[fd_index].fd;
    int otherfd;
    if (conns.count(fd)) {
        otherfd = conns[fd];
        conns.erase(fd);
        rev_conns.erase(otherfd);
    } else if (rev_conns.count(fd)) {
        otherfd = rev_conns[fd];
        rev_conns.erase(fd);
        conns.erase(otherfd);
    }

    fds.erase(fds.begin() + fd_index);
    close(fd);

    for(size_t i = 1; i < fds.size(); i++) {
        if (fds[i].fd == otherfd)
        {
            fds.erase(fds.begin() + i);
            close(otherfd);
            break;
        }
    }

}

void Proxy::handle_connection(int index) {
    char buffer[BUFSIZ];
    ssize_t dataRead;

    int from = fds[index].fd;
    int to = conns.count(from) ? conns[from] : rev_conns[from];

    if ((dataRead = read(from, buffer, BUFSIZ)) > 0) {
        write(to, buffer, dataRead);
    } else if (dataRead == 0){
        close_connection(index);
    } else if (dataRead == -1) {
        perror("read");
        close_connection(index);
    }
}

string Proxy::accumulate_response(int fd) {
    string response;
    char buffer[BUFSIZ];
    size_t total_size = 0;
    const size_t MAX_RESPONSE_SIZE = 100 * 1024 * 1024;

    while (response.find("\r\n\r\n") == string::npos) {
        ssize_t bytes_read = read(fd, buffer, BUFSIZ);
        if (bytes_read <= 0) break;

        total_size += bytes_read;
        if (total_size > MAX_RESPONSE_SIZE) {
            cerr << "Response too large to handle\n";
            return "";
        }

        response.append(buffer, bytes_read);
    }
    return response;
}

int Proxy::remote_connect(int clientfd) {
    char buffer[BUFSIZ];
    string accumulated_request;

    while (true) {
        ssize_t bytes_read = read(clientfd, buffer, BUFSIZ);
        if (bytes_read < 0) {
            perror("Read from client");
            return -1;
        }
        if (bytes_read == 0) {
            cerr << "Client closed connection before complete request" << endl;
            return -1;
        }

        accumulated_request.append(buffer, bytes_read);

        if (accumulated_request.find("\r\n\r\n") != string::npos) {
            break;
        }
    }

    HTTP_Request request;
    if (!HTTP_Parser::parse_HTTP_Request(accumulated_request, request)) {
        cerr << "Failed to parse HTTP request" << endl;
        return -1;
    }

    if (request.version != "HTTP/1.0") {
        string responce = "HTTP/1.0 505 Version Not Supported\r\n\r\n";
        cerr << request.path << ": " << request.version << " Version not supported\n";
        write(clientfd, responce.c_str(), responce.length());
        close(clientfd);
        return -1;
    }

    if (request.method != "GET" && request.method != "POST" 
        && request.method != "HEAD" && request.method != "PUT" 
        && request.method != "POST") {
        string responce = "HTTP/1.0 405 Not Implemented\r\n\r\n";
        cerr << request.path << ": " << request.method << " Method not supported\n";
        write(clientfd, responce.c_str(), responce.length());
        close(clientfd);
        return -1;
    }

    // if (request.method == "GET") {
    //     string cached_response;
    //     string cache_key = request.host + request.path;

    //     if (request.headers.count("Content-Length")) {
    //         size_t content_length = stoll(request.headers["Content-Length"]);
    //         if (content_length > MAX_CACHEABLE_SIZE) {
    //             cerr << "File too large to cache: " << content_length << " bytes\n";
    //             string response = "HTTP/1.0 413 Request Entity Too Large\r\n\r\n";
    //             write(clientfd, response.c_str(), response.length());
    //             return -1;
    //         }
    //     }

    //     if (cache.get(cache_key, cached_response)) {
    //         write(clientfd, cached_response.c_str(), cached_response.length());
    //         return -1;
    //     }
    // }

    string remote_host;
    int remote_port;

    if (request.host.empty()) {
        cerr << "Host header is missing\n";
        return -1;
    }
    size_t colon_pos = request.host.find(':');
    if (colon_pos != string::npos) {
        remote_host = request.host.substr(0, colon_pos);
        remote_port = stoi(request.host.substr(colon_pos + 1));
    } else {
        remote_host = request.host;
        remote_port = 80;
    }

    string modified_request = request.method + " " + request.path + " " + request.version + "\r\n";
    for (const auto& header : request.headers) {
        modified_request += header.first + ": " + header.second + "\r\n";
    }
    modified_request += "\r\n";

    struct hostent* host = gethostbyname(remote_host.data());
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
        return -1;
    }

    if (write(remotefd, modified_request.c_str(), modified_request.length()) < 0) {
        perror("write to remote server");
        close(remotefd);
        return -1;
    }

    // if (request.method == "GET" || request.method == "HEAD") {
    //     string response = accumulate_response(remotefd);
    //     // string cache_key = request.host + request.path;
    //     // cache.put(cache_key, response);
    //     write(clientfd, response.c_str(), response.length());
    // }

    return remotefd;
}

Proxy::Proxy(int listen_port) {
    this->listen_port = listen_port;
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

    if (listen(listenfd, MAX_CONNS) == -1) {
        perror("Listen failure");
        exit(EXIT_FAILURE);
    }
}

void Proxy::run() {
    fds.push_back({listenfd, POLLIN, 0});

    while (working_flag) {
        cache.cleanup();
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

            int remotefd = remote_connect(clientfd);
            if (remotefd == -1) {
                close(clientfd);
                continue;
            }

            if (fds.size() >= MAX_CONNS * 2 + 1) {
                fprintf(stderr, "Too many connections\n");
                close(remotefd);
                close(clientfd);
                continue;
            }

            conns[clientfd] = remotefd;
            rev_conns[remotefd] = clientfd;
            fds.push_back({clientfd, POLLIN, 0});
            fds.push_back({remotefd, POLLIN, 0});
        }

        for(size_t i = 1; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN)
                handle_connection(i);
        }
    }
}
