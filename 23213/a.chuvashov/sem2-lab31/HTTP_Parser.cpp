#include "HTTP_Parser.h"

string HTTP_Parser::trim(const string& str) {
    size_t start = str.find_first_not_of(" \t");
    if (start == string::npos) 
        return "";

    size_t end = str.find_last_not_of(" \t");
        return str.substr(start, end - start + 1);
}

vector<string> HTTP_Parser::split(const string& s, const string& delimiter) {
    vector<string> tokens;
    size_t pos_start = 0;
    size_t pos_end;

    while ((pos_end = s.find(delimiter, pos_start)) != string::npos) {
        tokens.push_back(s.substr(pos_start, pos_end - pos_start));
        pos_start = pos_end + delimiter.length();
        if (pos_end == string::npos) break;
    }
    tokens.push_back(s.substr(pos_start));
    return tokens;
}

bool HTTP_Parser::parse_HTTP_Request(const string& request, HTTP_Request& out) {
    size_t headers_end = request.find("\r\n\r\n");
    if (headers_end == string::npos) return false;

    string headers_part = request.substr(0, headers_end);
    vector<string> header_lines = split(headers_part, "\r\n");

    if (header_lines.empty()) return false;


    string request_line = header_lines[0];
    size_t method_end = request_line.find(' ');
    if (method_end == string::npos) return false;

    size_t path_end = request_line.find(' ', method_end + 1);
    if (path_end == string::npos) return false;

    out.method = trim(request_line.substr(0, method_end));
    out.path = trim(request_line.substr(method_end + 1, path_end - method_end - 1));
    out.version = trim(request_line.substr(path_end + 1));
    out.host = parse_HTTP_Path(out.path);

    for (size_t i = 1; i < header_lines.size(); ++i) {
        const string& line = header_lines[i];
        if (line.empty()) break;

        size_t colon_pos = line.find(':');
        if (colon_pos == string::npos) continue;

        string key = trim(line.substr(0, colon_pos));
        string value = trim(line.substr(colon_pos + 1));

        out.headers[key] = value;
    }
    return true;
}

string HTTP_Parser::parse_HTTP_Path(string& request) {
    size_t path_start = request.find("http://");
    if (path_start != string::npos) {
        path_start += 7;    
    } else {
        path_start = 0;
    }
    
    size_t path_end = request.find('/', path_start);
    if (path_end == string::npos) {
        return request.substr(path_start);
    }

    return request.substr(path_start, path_end - path_start);
}
