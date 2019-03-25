#ifndef Cache_h
#define Cache_h

#include <mutex>

using namespace std;

class parsed_message {
public:
    unordered_map<string, string> mps;
    string content;
    parsed_message() {
        mps.clear();
        content = "";
    }
    parsed_message(string s, unordered_map<string, string> m)
    : content(s), mps(m) {}
    parsed_message operator=(const parsed_message &b) {
        parsed_message p;
        p.content = b.content;
        p.mps = b.mps;
        return p;
    }
};

bool can_cache(std::unordered_map<std::string, std::string> &parser_request,
               std::unordered_map<std::string, std::string> &parser_response) {
    // This is even to add to the cache
    // We can allow not to cache incomplete requests, should we do so?
    std::string temp_str("GET");
    if (parser_request["http_method"].compare(temp_str) != 0) {
        return false;
    }
    unordered_map<string, string>::const_iterator partial_check =
    parser_response.find("status_code");
    string temp_partial("206");
    if (partial_check->second.compare(temp_partial) == 0) {
        return false; // 206, do not save in cache
    }
    string temp_OK("200");
    if (partial_check->second.compare(temp_OK) != 0) {
        return false; // 206, do not save in cache
    }
    
    std::string request_directives;
    std::unordered_map<std::string, std::string>::const_iterator
    request_iterator = parser_request.find("cache-control");
    if (request_iterator != parser_request.end()) {
        request_directives = request_iterator->second;
    }
    std::string response_directives;
    std::unordered_map<std::string, std::string>::const_iterator
    response_iterator = parser_response.find("cache-control");
    if (response_iterator != parser_response.end()) {
        response_directives = response_iterator->second;
    } // eof
    if (request_directives.find("no-") != std::string::npos &&
        response_directives.find("no-") != std::string::npos) {
        return false;
    }
    if (response_directives.find("private") != string::npos) {
        return false;
    }
    if (parser_request.find("authorization") != parser_request.end()) {
        // This means, there is an authorization
        if (response_directives.find("must-revalidate") != std::string::npos ||
            response_directives.find("public") != std::string::npos ||
            response_directives.find("s-maxage") != std::string::npos) {
            std::cout << "Cannot cache sorry" << std::endl;
            return false;
        }
    }
    
    if (parser_response.find("expires") != parser_response.end()) {
        return true; // If it can pass all that, and expires is satisfied. can cache
    }
    // Now, check cache control directives
    if (response_directives.find("max-age") != string::npos) {
        return true;
    }
    if (response_directives.find("s-maxage") != string::npos) {
        return true;
    }
    if (response_directives.find("public") != string::npos) {
        return true;
    }
    std::cout << "Cannot cache" << std::endl;
    return false;
}

class HttpCache {
private:
    mutex cache_mutex;
    unordered_map<string, parsed_message> contents;
    
public:
    HttpCache() { contents.clear(); }
    bool URI_in_cache(string URI) {
        lock_guard<mutex> lock(cache_mutex);
        if (contents.find(URI) != contents.end()) {
            return true;
        } else {
            return false;
        }
    }
    
    parsed_message return_message(string URI) {
        return contents.find(URI)->second;
    }
    double calculate_epoch(string timestamp) {
        struct tm calculated_time = {0};
        struct tm genesis = {0};
        vector<string> days;
        vector<string> months;
        stringstream ss_days;
        stringstream ss_months;
        ss_days << "sun mon tue wed thu fri sat"; // last one will be empty string
        ss_months
        << "jan feb mar apr may jun jul aug sep oct nov dec"; // if ss.good()
        days.resize(7);
        months.resize(12);
        while (ss_days.good()) {
            string temp;
            ss_days >> temp;
            days.push_back(temp);
        }
        days.erase(days.begin(), days.begin() + 7);
        while (ss_months.good()) {
            string temp;
            ss_months >> temp;
            months.push_back(temp);
        }
        months.erase(months.begin(), months.begin() + 12);
        // Ask wenqian about this, he has more exp with sstreams
        size_t comma_index;
        if ((comma_index = timestamp.find_first_of(",")) == string::npos) {
            return -1; // Invalid string
        }
        string dayname_str =
        timestamp.substr(comma_index - 3, 3); // 3 min comma, 3 len
        timestamp.erase(timestamp.begin(), timestamp.begin() + comma_index + 2);
        int day_week = -1;
        for (size_t i = 0; i < days.size(); i++) {
            if (days[i].compare(dayname_str) == 0) {
                day_week = i;
                break;
            }
        }
        if (day_week == -1) {
            return -1;
        }
        // This way, delete all up to the first val
        string day_str = timestamp.substr(0, 2); // Start at 0, 2 chars
        int day_month = atoi(day_str.c_str());
        // Now, months
        timestamp.erase(timestamp.begin(),
                        timestamp.begin() + 3);  //'01 'delete 4 elems
        string mon_str = timestamp.substr(0, 3); // Again, start at 0, 3 chars
        
        int month_year = -1;
        for (size_t i = 0; i < months.size(); i++) {
            if (months[i].compare(mon_str) == 0) {
                month_year = i;
                break;
            }
        }
        if (month_year == -1) {
            return -1;
        }
        timestamp.erase(timestamp.begin(),
                        timestamp.begin() + 4); //'nov '//delete 4
        string year_str = timestamp.substr(0, 4);
        int year_val = atoi(year_str.c_str()) - 1900; // years since 1900
        timestamp.erase(timestamp.begin(), timestamp.begin() + 5); //'1994 ', del 5
        string hour_str = timestamp.substr(0, 2);
        int hour_val = atoi(hour_str.c_str());
        timestamp.erase(timestamp.begin(), timestamp.begin() + 3); //'08:', del3
        string min_str = timestamp.substr(0, 2);
        int min_val = atoi(min_str.c_str());
        timestamp.erase(timestamp.begin(), timestamp.begin() + 3); //'08:', del3
        string sec_str = timestamp.substr(0, 2);
        int sec_val = atoi(sec_str.c_str());
        calculated_time.tm_wday = day_week;
        calculated_time.tm_mday = day_month;
        calculated_time.tm_mon = month_year;
        calculated_time.tm_year = year_val;
        calculated_time.tm_hour = hour_val;
        calculated_time.tm_min = min_val;
        calculated_time.tm_sec = sec_val;
        double seconds = difftime(mktime(&calculated_time), mktime(&genesis));
        // Now, all can be tried.
        return seconds;
    }
    
    double get_time_now(void) {
        time_t rawtime;
        struct tm *ptm;
        struct tm genesis = {0};
        time(&rawtime);
        ptm = gmtime(&rawtime);
        double seconds = difftime(mktime(ptm), mktime(&genesis));
        return seconds;
    }
    double expires_freshness(parsed_message &response_headers) {
        // Do we really need to calculate by string? Better way?
        unordered_map<string, string>::const_iterator it =
        response_headers.mps.find("expires");
        if (it == response_headers.mps.end()) {
            // Does not have expires, so unacceptable
            return -1;
        } else {
            
            unordered_map<string, string>::const_iterator it2 =
            response_headers.mps.find("date");
            string directives_expires = it->second;
            string directives_date = it2->second;
            // Expires - date, equals to the value it has
            return calculate_epoch(directives_expires) -
            calculate_epoch(directives_date);
        }
    }
    double calculate_freshness(parsed_message &response_headers) {
        string directives = response_headers.mps.find("cache-control")->second;
        size_t index = -1;
        if (directives.find("max-age") != string::npos) {
            index = directives.find("max-age");
        } else if (directives.find("s-maxage") != string::npos) {
            index = directives.find("s-maxage");
        } else {
            // Expires exists, that means to calculate separately.
            return expires_freshness(response_headers);
        }
        // Either case, find substr from first num to \r, that will be int.
        // stoi/atoi
        // delete until index;
        char *pEnd;
        index = directives.find_first_of("0123456789", index);
        directives.erase(0, index);
        long int return_val = strtol(directives.c_str(), &pEnd, 10);
        return return_val;
        // Now, convert to int
    }
    
    string revalidation(string request_uri) {
        parsed_message response_headers = contents[request_uri];
        string etag;
        string last_modified;
        if (response_headers.mps.find("etag") != response_headers.mps.end()) {
            // There is an etag
            etag = response_headers.mps.find("etag")->second;
        }
        if (response_headers.mps.find("last_modified") !=
            response_headers.mps.end()) {
            // There is an etag
            last_modified = response_headers.mps.find("last_modified")->second;
        }
        stringstream request;
        request << "GET " << request_uri<<" "<<response_headers.mps["protocol"]+"\r\n";
        if (!etag.empty()){
            request <<"If-None-Match:"+etag+"\r\n";
        }
        if (!last_modified.empty()){
            request <<"If-modified-since:"+last_modified+"\r\n";
        }
        request << "\r\n";
        return request.str();
        // Now, start generating the new request to be sent. Stringstream best
        // choice here
    }
    // Whether return response or not
    std::string respond_cache(string request_uri) {
        
        std::string false_str("false");
        if (URI_in_cache(request_uri)) {
            // Now, check if GET.
            parsed_message response_headers = contents[request_uri];
            std::string get_str("GET");
            if (response_headers.mps.find("method")->second.compare(get_str)) {
                string directives = response_headers.mps.find("cache-control")->second;
                if (directives.find("must-revalidate") != string::npos ||
                    directives.find("proxy-revalidate") != string::npos) {
                    // This calls for revalidation
                }
                long int freshness = calculate_freshness(response_headers);
                if (freshness <= 0 && directives.find("max-stale") == string::npos) {
                    return false_str;
                }
                string date_created = response_headers.mps.find("date")->second;
                // Either fresh, or client accepts stale
                if (get_time_now() - calculate_epoch(date_created) < freshness ||
                    directives.find("max-stale") != string::npos) {
                    std::cout << "Ok, here you go. Cached result!" << std::endl;
                    // return the s.
                    return response_headers.content;
                }
                // still valid upto this point, call another func to check timing now
            }
        }
        return false_str;
    }
    
    bool overwrite(std::string address, parsed_message page) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        bool result = true;
        if (contents.find(address) != contents.end())
            result = false;
        contents[address] = page;
        return result;
    }
    bool remove(std::string address) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (contents.find(address) == contents.end())
            return false;
        contents.erase(address);
        return true;
    }
};
#endif /* Cache_h */
