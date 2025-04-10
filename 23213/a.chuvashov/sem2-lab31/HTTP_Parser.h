#ifndef HTTP_PARSER
#define HTTP_PARSER

#include <string>
#include <map>
#include <vector>
#include <iostream>

using namespace std;

struct HTTP_Request {
    string method;
    string version;
    string path;
    string host;
    map<string, string> headers;
};

class HTTP_Parser {
    static string trim(const string& str);
    static vector<string> split(const string& line, const string& delimeters);
    static string parse_HTTP_Path(string& request);
public:
    HTTP_Parser();
    static bool parse_HTTP_Request(const string& request, HTTP_Request& out);
};

#endif