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

void Proxy::get_cacheable_body(int index) {
    HTTP_Request request = clients_requests[fds[index].fd];
    CacheEntry entry;
    Cache::get(request.path, entry);

    size_t size = entry.has_size ? entry.body_size : BUFSIZ;
    char buffer[size];
    ssize_t dataRead;

    int from = fds[index].fd;
    int to = conns.count(from) ? conns[from] : rev_conns[from];

    if ((dataRead = read(from, buffer, size)) > 0) {
        Cache::append_body(request.path, string(buffer, dataRead));
        write(to, buffer, dataRead);
        for (auto& pair : awaiting_requests[request.path]) {
            if (pair.first == "GET") {
                write(pair.second, buffer, dataRead);
            } else if (pair.first == "HEAD") {
                write(pair.second, entry.head.c_str(), entry.head.size());
            }
            close(pair.second);
        }

        if (entry.body.size() > Cache::MAX_CACHEABLE_SIZE) {
            clients_requests.erase(from);
            cerr << "Erasing\n";
            Cache::delete_entry(request.path);
        }
    } else if (dataRead == 0) {
        cerr << "End of reading from serv\n" << endl;
        Cache::set_uploaded(request.path, true);
        string responce;
        responce = entry.head + entry.body;
        cerr << "GET Cache hit: " + request.path << endl;
        
        // write(to, responce.c_str(), responce.size());
        for (auto& pair : awaiting_requests[request.path]) {
            write(pair.second, responce.c_str(), responce.size());
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

void Proxy::get_uncacheable_body(int index) {
    char buffer[BUFSIZ];
    ssize_t dataRead;

    int from = fds[index].fd;
    int to = conns.find(from) != conns.end() ? conns[from] : rev_conns[from];

    if ((dataRead = read(from, buffer, BUFSIZ)) > 0) {
        write(to, buffer, dataRead);
    } else if (dataRead == 0){
        close_connection(index);
    } else if (dataRead == -1) {
        perror("Uncacheable read");
        close_connection(index);
    }
}

int Proxy::connect_to_remote_server(const string& remote_host, int remote_port, int clientfd) {
    struct hostent* host = gethostbyname(remote_host.c_str());
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

    return remotefd;
}

int Proxy::request_first_line(int index) {
    int clientfd = fds[index].fd;
    char buffer[BUFSIZ] = {0};
    string& accumulated_request = unprocessed_requests[clientfd].first;
    ssize_t bytes_read;
    while ((bytes_read = read(clientfd, buffer, BUFSIZ)) > 0) {
        accumulated_request.append(buffer, bytes_read);
        if (accumulated_request.find("\r\n") != string::npos) {
            HTTP_Request request;
            cerr << "Request: " << accumulated_request << endl;
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

            if (request.method == "GET" || request.method == "HEAD") {
                CacheEntry entry;
                cerr << "Cacheable request: " + request.path << endl;
                if (Cache::get(request.path, entry)) {
                    cerr << "Cache hit: " + request.path << endl;
                    write(clientfd, entry.head.c_str(), entry.head.size());
                    if (entry.body.size() > 0 && request.method == "HEAD") return -1;
                    if (entry.body.size() > 0 && request.method == "GET") {
                        write(clientfd, entry.body.c_str(), entry.body.size());
                        if (entry.uploaded) return -1;
                    }
                    fds.erase(fds.begin() + index);
                    unprocessed_requests.erase(clientfd);
                    clients_requests.erase(clientfd);
                    awaiting_requests[request.path].push_back({request.method, clientfd});
                    return 0;
                }
            } else {
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
                int remotefd = connect_to_remote_server(remote_host, remote_port, clientfd);
                conns[clientfd] = remotefd;
                rev_conns[remotefd] = clientfd;
                fds.push_back({remotefd, POLLIN, 0});
                unprocessed_requests.erase(clientfd);
                if (write(remotefd, accumulated_request.c_str(), accumulated_request.length()) < 0) {
                    perror("write to remote server");
                    close(remotefd);
                    return -1;
                }
                return 0;
            }
            unprocessed_requests[clientfd].second = true;
            if (request.method == "GET" || request.method == "HEAD")
                Cache::make_entry(request.path);
            return 1;
        }
    }
    if (bytes_read < 0 && errno != EAGAIN) {
        perror("Read from client");
        return -1;
    }
    if (bytes_read == 0) {
        cerr << "Client closed connection before complete request\n";
        return -1;
    }
    return 0;
}

int Proxy::request_all_lines(int index) {
    int clientfd = fds[index].fd;
    char buffer[BUFSIZ] = {0};
    HTTP_Request request;
    string& accumulated_request = unprocessed_requests[clientfd].first;
    ssize_t bytes_read;
    CacheEntry entry;

    if (accumulated_request.find("\r\n\r\n") != string::npos) {
        cerr << "All request lines received\n";
        if (!HTTP_Parser::parse_HTTP_Request_Header(accumulated_request, request)) {
            cerr << "Failed to parse HTTP request header\n";
            return -1;
        }

        if (!HTTP_Parser::parse_HTTP_Request(accumulated_request, request)) {
            cerr << "Failed to parse HTTP request\n";
            return -1;
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

        int remotefd = connect_to_remote_server(remote_host, remote_port, clientfd);

        clients_requests[remotefd] = request;

        if (write(remotefd, accumulated_request.c_str(), accumulated_request.length()) < 0) {
            perror("write to remote server");
            close(remotefd);
            return -1;
        }

        conns[clientfd] = remotefd;
        rev_conns[remotefd] = clientfd;
        fds.push_back({remotefd, POLLIN, 0});
        unprocessed_requests.erase(clientfd);
    }

    while ((bytes_read = read(clientfd, buffer, BUFSIZ)) > 0) {
        accumulated_request.append(buffer, bytes_read);
    }

    if (bytes_read < 0 && errno != EAGAIN) {
        perror("Read from client");
        return -1;
    }

    if (bytes_read == 0) {
        cerr << "Client closed connection before complete request\n";
        return -1;
    }
    return 0;
}

void Proxy::remote_connect(int index) {
    int clientfd = fds[index].fd;
    if (!unprocessed_requests[clientfd].second) {
        int ret = request_first_line(index);
        if (ret == -1) {
            close(clientfd);
            fds.erase(fds.begin() + index);
            unprocessed_requests.erase(clientfd);
            clients_requests.erase(clientfd);
            return;
        } 
        if (ret == 0)
            return;
    }

    int ret = request_all_lines(index);
    if (ret == -1) {
        close(clientfd);
        fds.erase(fds.begin() + index);
        unprocessed_requests.erase(clientfd);
        clients_requests.erase(clientfd);
        return;
    } else if (ret == 0) {
        return;
    }
}

void Proxy::run() {
    fds.push_back({listenfd, POLLIN, 0});

    while (working_flag) {
        Cache::cleanup();
        int ret = poll(fds.data(), fds.size(), 5000);
        if (ret == -1) {
            perror("Poll");
            continue;
        } else if (ret == 0) {
            continue;
        }

        if (fds[0].revents & POLLIN) {
            int clientfd;
            if ((clientfd = accept(listenfd, nullptr, nullptr)) == -1) {
                perror("accept failure");
                continue;
            }
        
            if (fcntl(clientfd, F_SETFL, O_NONBLOCK) == -1) {
                perror("fcntl");
                close(clientfd);    
                continue;
            }
        
            fds.push_back({clientfd, POLLIN, 0});
            unprocessed_requests[clientfd] = {"", false};
        }

        for(size_t i = 1; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                if (unprocessed_requests.find(fds[i].fd) != unprocessed_requests.end()) {
                    remote_connect(i);
                } else {
                    HTTP_Request request = clients_requests[fds[i].fd];
                    if (request.method == "GET" || request.method == "HEAD") {
                        if (unprocessed_servers_responses.find(fds[i].fd) != unprocessed_servers_responses.end()) {
                            get_server_response(i);
                        } else {
                            get_cacheable_body(i);
                        }
                    } else {
                        get_uncacheable_body(i);
                    }
                }
            }
        }
    }
}

void Proxy::get_server_response(int index) {
    int serverfd = fds[index].fd;
    int clientfd = rev_conns[serverfd];
    char buffer[BUFSIZ];
    ssize_t bytes_read;
    string& accumulated_response = unprocessed_servers_responses[serverfd].first;

    while ((bytes_read = read(serverfd, buffer, BUFSIZ)) > 0) {
        accumulated_response.append(buffer, bytes_read);
        write(clientfd, buffer, bytes_read);
        if (accumulated_response.find("\r\n\r\n") != string::npos) {
            HTTP_Responce server_responce;
            if (!HTTP_Parser::parse_HTTP_Responce(accumulated_response, server_responce)) {
                cerr << "Failed to parse HTTP response header\n";
                unprocessed_servers_responses.erase(serverfd);
                close_connection(index);
                return;
            }

            if (!(server_responce.status_code == 200) || !(server_responce.status_message == "OK")) {
                write(clientfd, accumulated_response.c_str(), accumulated_response.length());
                cerr << "Server response: " << server_responce.status_code << " " << server_responce.status_message << endl;
                unprocessed_servers_responses.erase(serverfd);
                close_connection(index);
                return;
            }

            unprocessed_servers_responses[serverfd].second = true;
            CacheEntry entry;
            Cache::get(clients_requests[serverfd].path, entry);
            string resp = string(buffer, bytes_read);
            Cache::append_head(clients_requests[serverfd].path, resp.substr(0, resp.find("\r\n\r\n") + 4));
            if (resp.find("\r\n\r\n") + 4 <= resp.size()) {
                Cache::append_body(clients_requests[serverfd].path, resp.substr(resp.find("\r\n\r\n") + 4));
            }
            if (server_responce.headers.find("Content-Length") != server_responce.headers.end()) {
                size_t content_length = stoi(server_responce.headers["Content-Length"]);
                if (content_length > Cache::MAX_CACHEABLE_SIZE) {
                    cerr << "Erasing\n";
                    Cache::delete_entry(clients_requests[serverfd].path);
                    unprocessed_servers_responses.erase(serverfd);
                    clients_requests.erase(serverfd);
                    return;
                }
                Cache::set_size(clients_requests[serverfd].path, stoi(server_responce.headers["Content-Length"]));  
            }          
        }
        Cache::append_head(clients_requests[serverfd].path, string(buffer, bytes_read));
    }

    if (bytes_read < 0) {
        perror("Read from server");
        close_connection(index);
    } else if (bytes_read == 0) {
        close_connection(index);
    }
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
