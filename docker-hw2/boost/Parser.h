#ifndef Parser_h
#define Parser_h

#include <unordered_map>
#include <sstream>
using namespace std;

void string_trim(string& s){
    size_t p = s.find_first_not_of(" ");
    s.erase(0, p);
    p = s.find_last_not_of(" \r");
    if (p != string::npos)
        s.erase(p+1);
    return;
}

string convertLower(string s){
    string res = "";
    for (int i=0; i< s.length(); i++){
        char temp = s[i];
        if (temp <= 'Z' && temp >= 'A')
            res.push_back(temp-('A'-'a'));
        else
            res.push_back(temp);
    }
    return res;
}

string remove_connection(string s){
    string res = "";
    istringstream ss(s);
    int i=0;
    while (ss.good()){
        string line;
        getline(ss, line, '\n');
        size_t idx = line.find(":");
        if (idx == string::npos || i==0 ){
            // empty line
            i ++;
            if (ss.good()) res += (line+"\n");
            continue;
        }
        string field_name = convertLower(line.substr(0, idx));
        string field_content = line.substr(idx+1, line.length());
        string_trim(field_content);
        string_trim(field_name);
        if (field_name == "connection") field_content = "close\r";
        if (field_name == "proxy-connection") field_content = "close\r";
        res += field_name + ": " + field_content + "\n";
        i ++;
    }
    //cout<<res;
    return res;
}

unordered_map<string, string> parse_header (string request){
    unordered_map<string, string> result;
    
    string req_line;
    size_t idx = request.find("\r\n");
    istringstream ss_temp(request.substr(0, idx));
    string method, abs_path, protocol_version;
    ss_temp >> method;
    ss_temp >> abs_path;
    ss_temp >> protocol_version;
    //protocol_version.pop_back();
    result["method"] = method;
    result["uri"] = abs_path;
    result["protocol"] = protocol_version;
    
    istringstream ss(request.substr(idx+2, request.length()));
    while (ss.good()){
        string line;
        getline(ss, line, '\n');
        size_t idxx = line.find(":");
        if (idxx == string::npos){
            // a error line(discard), or the last line (empty)
            continue;
        }
        string field_name = convertLower(line.substr(0, idxx));
        string field_content = line.substr(idxx+1, line.length());
        string_trim(field_content);
        result[field_name] = field_content;
    }
    return result;
}

unordered_map<string, string> parse_header2 (string request){
    unordered_map<string, string> result;
    
    string req_line;
    size_t idx = request.find("\r\n");
    istringstream ss_temp(request.substr(0, idx));
    string protocol, status_code, respond_head_message;
    ss_temp >> protocol;
    ss_temp >> status_code;
    ss_temp >> respond_head_message;
    //protocol_version.pop_back();
    result["protocol"] = protocol;
    result["status_code"] = status_code;
    result["respond_head_message"] = respond_head_message;
    
    istringstream ss(request.substr(idx+2, request.length()));
    while (ss.good()){
        string line;
        getline(ss, line, '\n');
        size_t idxx = line.find(":");
        if (idxx == string::npos){
            // a error line(discard), or the last line (empty)
            continue;
        }
        string field_name = convertLower(line.substr(0, idxx));
        string field_content = line.substr(idxx+1, line.length());
        string_trim(field_content);
        result[field_name] = field_content;
    }
    return result;
}


#endif /* Parser_h */
