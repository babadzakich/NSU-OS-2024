#include "Proxy.h"

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

void Proxy::handle_cacheable_connection(int index) {
    HTTP_Request request = clients_requests[fds[index].fd];
    CacheEntry entry;
    Cache::get(request.path, entry);

    size_t size = entry.has_size ? entry.body_size : BUFSIZ;
    char buffer[size];
    ssize_t dataRead;

    int from = fds[index].fd;
    int to = conns.count(from) ? conns[from] : rev_conns[from];

    if ((dataRead = read(from, buffer, size)) > 0) {
        Cache::append(request.path, string(buffer, dataRead));

        if (entry.body.size() > Cache::MAX_CACHEABLE_SIZE) {
            clients_requests.erase(from);
            cerr << "Erasing\n";
            if (request.method == "GET") {
                write(to, entry.body.c_str(), entry.body.size());
            }
            Cache::delete_entry(request.path);
        }
    } else if (dataRead == 0) {
        cerr << "End of reading from serv\n" << endl;
        Cache::set_uploaded(request.path, true);
        string responce;
        responce = entry.head + entry.body;
        cerr << "GET Cache hit: " + request.path << endl;
        
        write(to, responce.c_str(), responce.size());
        for (auto& pair : awaiting_requests[request.path]) {
            if (pair.first == "GET") {
                write(pair.second, responce.c_str(), responce.size());
            } else if (pair.first == "HEAD") {
                write(pair.second, entry.head.c_str(), entry.head.size());
            }
            close(pair.second);
        }
        awaiting_requests.erase(request.path);
        close_connection(index);
    } else if (dataRead == -1) {
        perror("Cacheable read");
        cerr << entry.head << endl;
        Cache::delete_entry(request.path);
        close_connection(index);
    }
}


void Proxy::handle_uncacheable_connection(int index) {
    char buffer[BUFSIZ];
    ssize_t dataRead;

    int from = fds[index].fd;
    int to = conns.count(from) ? conns[from] : rev_conns[from];

    if ((dataRead = read(from, buffer, BUFSIZ)) > 0) {
        write(to, buffer, dataRead);
    } else if (dataRead == 0){
        close_connection(index);
    } else if (dataRead == -1) {
        perror("Uncacheable read");
        close_connection(index);
    }
}

int Proxy::remote_connect(int clientfd) {
    char buffer[BUFSIZ] = {0};
    string accumulated_request;

    while (accumulated_request.find("\r\n") == string::npos) {
        ssize_t bytes_read = read(clientfd, buffer, BUFSIZ);
        if (bytes_read < 0) {
            perror("Read from client");
            return -1;
        }
        if (bytes_read == 0) {
            cerr << "Client closed connection before complete request\n";
            return -1;
        }

        accumulated_request.append(buffer, bytes_read);
    }

    HTTP_Request request;
    if (!HTTP_Parser::parse_HTTP_Request_Header(accumulated_request, request)) {
        cerr << "Failed to parse HTTP request header\n";
        return -1;
    }

    if (request.version != "HTTP/1.0") {
        string responce = "HTTP/1.0 505 Version Not Supported\r\n\r\n";
        cerr << request.path << ": " << request.version << " Version not supported\n";
        write(clientfd, responce.c_str(), responce.length());
        return -1;
    }

    if (request.method != "GET" && request.method != "POST"
        && request.method != "HEAD" && request.method != "PUT"
        && request.method != "POST") {
        string responce = "HTTP/1.0 405 Not Implemented\r\n\r\n";
        cerr << request.path << ": " << request.method << " Method not supported\n";
        write(clientfd, responce.c_str(), responce.length());
        return -1;
    }

    if (request.path.find("https://") != string::npos) {
        string responce = "HTTPS/1.0 400 Bad Request\r\n\r\n";
        write(clientfd, responce.c_str(), responce.length());
        return -1;
    }


    while (accumulated_request.find("\r\n\r\n") == string::npos) {
        ssize_t bytes_read = read(clientfd, buffer, BUFSIZ);
        if (bytes_read < 0) {
            perror("Read from client");
            return -1;
        }
        if (bytes_read == 0) {
            cerr << "Client closed connection before complete request\n";
            return -1;
        }

        accumulated_request.append(buffer, bytes_read);
    }

    if (!HTTP_Parser::parse_HTTP_Request(accumulated_request, request)) {
        cerr << "Failed to parse HTTP request\n";
        return -1;
    }

    CacheEntry response;

    if (request.method == "GET" || request.method == "HEAD") {
        if (Cache::get(request.path, response)) {
            cerr << "Cache record found " << request.method + " " + request.path << endl;
            if (request.method == "HEAD") {
                string head = response.head;
                cerr << "HEAD Cache hit " + request.path << endl;
                write(clientfd, head.c_str(), head.length());
                return -1;
            } else { 
                if (Cache::is_uploaded(request.path)) {
                    cerr << "GET Cache hit: " + request.path << endl;
                    write(clientfd, (response.head + response.body).c_str(), response.head.size() + response.body.size());
                    return -1;
                } else {
                    awaiting_requests[request.path].push_back({request.method, clientfd});
                    return -2;
                }
            }
        }
    }

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

    timeval timeout{.tv_sec = 5, .tv_usec = 0};
    if (setsockopt(remotefd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        perror("setsockopt RCV_TIMEO");
        close(remotefd);
        return -1;
    }

    if (setsockopt(remotefd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
        perror("setsockopt SND_TIMEO");
        close(remotefd);
        return -1;
    }

    if (write(remotefd, accumulated_request.c_str(), accumulated_request.length()) < 0) {
        perror("write to remote server");
        close(remotefd);
        return -1;
    }

    memset(buffer, 0, BUFSIZ);
    string accumulated_responce;

    while (accumulated_responce.find("\r\n") == string::npos) {
        ssize_t bytes_read = read(remotefd, buffer, BUFSIZ);
        if (bytes_read < 0) {
            perror("Read from server");
            write(clientfd, error_page.c_str(), error_page.length());
            close(remotefd);
            return -1;
        }

        if (bytes_read == 0) {
            cerr << "Server closed connection before complete request\n";
            return -1;
        }

        accumulated_responce.append(buffer, bytes_read);
    }

    HTTP_Responce server_responce;
    if (!HTTP_Parser::parse_HTTP_Responce_Header(accumulated_responce, server_responce)) {
        cerr << "Failed to parse HTTP response header\n";
        close(remotefd);
        return -1;
    }

    if (!(server_responce.status_code == 200) || !(server_responce.status_message == "OK")) {
        write(clientfd, accumulated_responce.c_str(), accumulated_responce.length());
        cerr << "Server response: " << server_responce.status_code << " " << server_responce.status_message << endl;
        close(remotefd);
        return -1;
    }

    while (accumulated_responce.find("\r\n\r\n") == string::npos) {
        ssize_t bytes_read = read(remotefd, buffer, BUFSIZ);
        if (bytes_read < 0) {
            perror("Read from server");
            return -1;
        }

        if (bytes_read == 0) {
            cerr << "Server closed connection before complete request\n";
            return -1;
        }

        accumulated_responce.append(buffer, bytes_read);
    }

    if (!HTTP_Parser::parse_HTTP_Responce(accumulated_responce, server_responce)) {
        cerr << "Failed to parse HTTP response\n";
        close(remotefd);
        return -1;
    }

    if (request.method == "GET" || request.method == "HEAD") {
        size_t content_length = 0;
        bool has_size = false;
        if (!server_responce.headers["Content-Length"].empty()) {
            cerr << "Content-Length header found\n";
            has_size = true;
            content_length = stoi(server_responce.headers["Content-Length"]);
            if (content_length > Cache::MAX_CACHEABLE_SIZE) {
                cerr << "Content-Length is too big\n";
                write(clientfd, accumulated_responce.c_str(), accumulated_responce.length());
                return remotefd;
            }
        }
        string head = accumulated_responce.substr(0, accumulated_responce.find("\r\n\r\n") + 4);
        
        string body;
        if (accumulated_responce.find("\r\n\r\n") + 4 < accumulated_responce.size()) {
            body = accumulated_responce.substr(accumulated_responce.find("\r\n\r\n") + 4);
        } else {
            body = "";
        }
        Cache::put(request.path, head, body, content_length, has_size);
        if (request.method == "HEAD") {
            write(clientfd, head.c_str(), head.length());
            return -1;
        }
    } else {
        write(clientfd, accumulated_responce.c_str(), accumulated_responce.length());
        return remotefd;
    }

    clients_requests[remotefd] = request;
    return remotefd;
}

Proxy::Proxy(int listen_port) {
    fstream errorpage("errorpage.html");

    string line;
    while (getline(errorpage, line)) {
        error_page += line + "\n";
    }
    errorpage.close();

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
        Cache::cleanup();
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

            timeval client_timeout{.tv_sec = 10, .tv_usec = 0};
            if (setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, &client_timeout, sizeof(client_timeout)) == -1) {
                perror("Setsockopt client RCVTIMEO");
                close(clientfd);
                continue;
            }

            if (setsockopt(clientfd, SOL_SOCKET, SO_SNDTIMEO, &client_timeout, sizeof(client_timeout)) == -1) {
                perror("Setsockopt client SNDTIMEO");
                close(clientfd);
                continue;
            }

            int remotefd = remote_connect(clientfd);
            if (remotefd == -1) {
                close(clientfd);
                continue;
            } else if (remotefd == -2) {
                continue;
            }

            if (fds.size() >= MAX_CONNS * 2 + 1) {
                cerr << "Too many connections\n";
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
            if (fds[i].revents & POLLIN) {
                if (clients_requests.count(fds[i].fd)) {
                    HTTP_Request request = clients_requests[fds[i].fd];
                    if (request.method == "GET" || request.method == "HEAD") {
                        handle_cacheable_connection(i);
                    } else {
                        handle_uncacheable_connection(i);
                    }
                } else {
                    handle_uncacheable_connection(i);
                }
            }
        }
    }
}
