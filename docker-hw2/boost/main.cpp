#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <iostream>
#include <stdlib.h>
#include <vector>
#include <thread>
#include <sstream>
#include <ctime>
#include <time.h>
#include <fstream>
#include "Parser.h"
#include "Cache.h"
#include "Exception.h"


using namespace std;
using boost::asio::ip::tcp;

string https_test =
string("GET www.google.com HTTP/1.1\r\n")+
string("cache-control: no-cache\r\n")+
string("accept: */*\r\n")+
string("host: www.google.com:443\r\n")+
string("connection: close\r\n")+
string("\r\n");

string connect_establish =
string("HTTP/1.1 200 Connection Established\r\n")+
//string("Connection: close\r\n")+
string("\r\n");

string http_bad_request =
string("HTTP/1.1 400 Bad Request\r\n")+
string("\r\n");

string http_server_error =
string("HTTP/1.1 502 Bad Gateway\r\n")+
string("\r\n");

string log_location = "/var/log/erss/proxy.log";

class server
{
public:
    
    string convertStreamBuf2String(boost::asio::streambuf& buf) {
        boost::asio::streambuf::const_buffers_type b = buf.data();
        return string(boost::asio::buffers_begin(b), boost::asio::buffers_end(b));
    }
    
    void convertString2StreamBuf(string str,  boost::asio::streambuf&bf){
        ostream out_stream(&bf);
        out_stream <<str;
        return;
    }
    
    void logging(int logid, string content){
        ofstream outfile;
        std::unique_lock<mutex> lck(log_mutex);
        outfile.open(log_location, ios::out | ios::app );
        outfile << to_string(logid)+" "+content;
        outfile.close();
        lck.unlock();
    }
    
    string current_time(){
        time_t rawtime;
        struct tm * ptm;
        time(& rawtime);
        ptm = gmtime(&rawtime);
        return asctime(ptm);
    }
    
    parsed_message read_request(shared_ptr<tcp::socket> s){
        boost::system::error_code err;
        boost::asio::streambuf request;
        try {
            size_t size = boost::asio::read_until(*s, request, "\r\n\r\n", err);
            string request_header = convertStreamBuf2String(request);
            size_t idx = request_header.find("\r\n\r\n");
            if (idx == string::npos) {
                cout<<"read_headr_error ";
                throw BadRequestException();
            }
            
            string out_request_header = request_header.substr(0, idx + 2);
            string tail = request_header.substr(idx+4, request_header.length());
            
            //cout<<out_request_header;
            cout<<"--------------------------------"<<endl;
                unordered_map<string, string> hd = parse_header(out_request_header);
            
                string xxx = remove_connection(out_request_header);
            
                if (hd.find("content-length")!= hd.end()){
                    int content_size = atoi(hd["content-length"].c_str());
                    boost::asio::streambuf ctbf;
                    if (content_size > tail.length()){
                        size_t n =
                        boost::asio::read(*s, ctbf,boost::asio::transfer_exactly(content_size - tail.length()), err);
                    }
                    if (err){
                        cout<<"content-length seems wrong"<<err.message()<<endl;
                        throw ContentLengthException();
                    };
                    
                    string after = convertStreamBuf2String(ctbf);
                    tail += after;
                    xxx += "\r\n" + tail;
                    //hd["message_body"] = tail;
                    //request_header += after;
                }
                else{
                    xxx += "\r\n";
                }
                parsed_message p(xxx, hd);
                return p;
        }
        catch (exception &e){
            boost::asio::streambuf req_back;
            convertString2StreamBuf(http_bad_request, req_back);
            boost::asio::write(*s, req_back);
            cout<<e.what()<<endl;
            throw e;
        }
    }
    
    parsed_message read_response(shared_ptr<tcp::socket> s){
        boost::system::error_code err;
        boost::asio::streambuf respond;
        
        size_t size = boost::asio::read_until(*s, respond, "\r\n\r\n", err);
        string respond_header = convertStreamBuf2String(respond);
        size_t idx = respond_header.find("\r\n\r\n");
        if (idx == string::npos) {
            cout<<"read_headr_error";
            throw InvalidHeaderException();
        };
        
        string out_respond_header = respond_header.substr(0, idx + 2);
        string tail = respond_header.substr(idx+4, respond_header.length());
        
        //cout<<out_request_header;
        cout<<"--------------------------------"<<endl;
        unordered_map<string, string> hd = parse_header2(out_respond_header);
        string xxx = out_respond_header;
        
        if (hd.find("content-length")!= hd.end()){
            int content_size = atoi(hd["content-length"].c_str());
            boost::asio::streambuf ctbf;
            if (content_size > tail.length()){
                size_t n =
                boost::asio::read(*s, ctbf,boost::asio::transfer_exactly(content_size - tail.length()), err);
            }
            if (err){
                cout<<"content-length seems wrong"<<err.message()<<endl;
                throw ContentLengthException();
            }
            
            string after = convertStreamBuf2String(ctbf);
            tail += after;
            xxx += "\r\n" + tail;
            //hd["message_body"] = tail;
            //request_header += after;
        }
        else{
            boost::system::error_code error;
            boost::asio::streambuf response_x;
            ostringstream print_res;
            
            while (boost::asio::read(*s, response_x, boost::asio::transfer_at_least(1), error))
                print_res << &response_x;
            if (error != boost::asio::error::eof){
                cout<<"NOT EOF ERROR "<<error.message()<<endl;
                throw boost::system::system_error(error);
            }
            xxx = respond_header + print_res.str();
        }
        cout<<"----- Parsing a response END -----"<<endl;
        parsed_message p(xxx, hd);
        return p;
    }
    
    
    void forward_request(shared_ptr<tcp::socket> src, shared_ptr<tcp::socket> dst, int tid){
        try{
            while (1){
                if(src->is_open()) {
                    //cout << "socket is open in tid " << tid << endl;
                } else {
                    cout << "socket is NOT open in tid " << tid << endl;
                    return;
                }
                boost::system::error_code err;
                char data[512];
                size_t length = src->read_some(boost::asio::buffer(data), err);
                if (err == boost::asio::error::eof){
                    cout <<"HTTPS closed due to eof "<<err.message()<<" tid is "<<tid<<" length "<<length<<endl;
                        return;
                }
                else if (err){
                    cout <<"err is "<<err.message()<<" tid is "<<tid<<" length "<<length<<endl;
                    throw boost::system::system_error(err);
                    return;
                }
                    boost::asio::write(*dst, boost::asio::buffer(data, length));
            } // while 1
        }
        catch (exception& e) {
            cout << "Exception: " << e.what() << "\n";
        }
        return;
    }
    
    void connect_handler(shared_ptr<tcp::socket> s, parsed_message pm, int this_log_id){

        cout<<"https connection func"<<endl;
        string target_server = pm.mps["host"];
        string port = "443";
        //cout<<"target server: "<<target_server<<" port: "<<port<<"----"<<endl;
        size_t idx = target_server.find(":");
        //cout<<"idx ="<<idx<<"---"<<endl;
        string unchange = target_server;
        if (idx != string::npos){
            target_server = unchange.substr(0, idx);
            port = unchange.substr(idx+1, unchange.length());
        }
        //cout<<"target server: "<<target_server<<" port: "<<port<<endl;
        boost::asio::io_service io_service_2;
        tcp::resolver resolver(io_service_2);
        shared_ptr<tcp::socket> skt(new tcp::socket(io_service_2));
        
        boost::system::error_code error_k2 = boost::asio::error::host_not_found;
        tcp::resolver::query query(target_server, port);
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query, error_k2);
        if (error_k2){
            boost::asio::streambuf req_backk;
            convertString2StreamBuf(http_server_error, req_backk);
            boost::asio::write(*s, req_backk);
            throw HttpsQueryException();
        }
        tcp::resolver::iterator end;
        boost::system::error_code error = boost::asio::error::host_not_found;
        
        while (error && endpoint_iterator != end)
        {
            skt->close();
            skt->connect(*endpoint_iterator++, error);
        }
        if (error){
            cout<<"error in connction"<<endl;
            boost::asio::streambuf req_backk;
            convertString2StreamBuf(http_server_error, req_backk);
            boost::asio::write(*s, req_backk);
            throw HttpsQueryException();
        }
        //cout<<"----- Successfully Connect to Https host -----"<<endl;
        
        boost::asio::streambuf req_back;
        convertString2StreamBuf(connect_establish, req_back);
        boost::asio::write(*s, req_back);
        
        std::vector<std::thread> threadList;
        threadList.push_back(thread(&server::forward_request, this, s, skt, 0));
        threadList.push_back(thread(&server::forward_request, this, skt, s, 1));

        threadList[0].join();
        threadList[1].join();
        logging(this_log_id, "Tunnel closed");
        cout << "---- One Https connetion ends ----" << endl;
    }
    
    parsed_message revalidate(shared_ptr<tcp::socket> skt, string ask){
        boost::asio::streambuf request;
        convertString2StreamBuf(ask, request);
        boost::asio::write(*skt, request);
        return read_response(skt);
    }
    
    void handle_request(shared_ptr<tcp::socket> s) {
        std::unique_lock<mutex> lck(log_id_mutex);
        int this_log_id = log_id++;
        lck.unlock();
        
        try{
            parsed_message p_msg = read_request(s);
            string head_line = p_msg.mps["method"]+" "+p_msg.mps["uri"]+" "+p_msg.mps["protocol"]+" from "+s->remote_endpoint().address().to_string()+" @ "+current_time();
            logging(this_log_id, head_line);
            if (p_msg.mps["method"] == "CONNECT") {
                try{
                    connect_handler(s, p_msg, this_log_id);
                }
                catch (exception& e){
                    cout<<"e is"<<e.what();
                }
                    return;
            }
// *************** CHECK CACHE *******************
            string to_validate = "";
            if (cache_memory.URI_in_cache(p_msg.mps["uri"])){
                string respd = cache_memory.respond_cache(p_msg.mps["uri"]);
                if (respd == "false"){
                    cache_memory.remove(p_msg.mps["uri"]);
                }
                else {
                    cout << "RETURN A MESSAGE IN CACHE!!!"<<endl;
                    // directly return the cache!
                    if(respd == "revalidate"){
                        to_validate = cache_memory.revalidation(p_msg.mps["uri"]);
                        logging(this_log_id, "in cache, requires validation");
                    }
                    else{
                        logging(this_log_id, "in cache, valid");
                        boost::asio::streambuf req_back_c;
                        convertString2StreamBuf(respd, req_back_c);
                        boost::asio::write(*s, req_back_c);
                        s->close();
                        return;
                    }
                }
            }
            else{
                logging(this_log_id, "not in cache");
            }
// *************** CHECK ADD CACHE **************
            string target_server = p_msg.mps["uri"];
            size_t idx = target_server.find("http://");
            //cout<<p_msg.content;
            if (idx == string::npos){
                cout<<"The http request line seems invalid"<<endl;
                throw InvalidHeaderException();
                }
            target_server = target_server.substr(idx+7, target_server.length());
            idx = target_server.find("/");
            if (idx != string::npos){
                target_server = target_server.substr(0, idx);
            }
            string port = "80";
            
            logging(this_log_id, "Requesting "+head_line+" from "+target_server);
            
            boost::asio::io_service io_service_2;
            tcp::resolver resolver(io_service_2);
            tcp::resolver::query query(target_server, port);
            
            boost::system::error_code resolve_error;
            tcp::resolver::iterator endpoint_iterator = resolver.resolve(query, resolve_error);
            if (resolve_error){
                cout<<"error in connction"<<endl;
                boost::asio::streambuf request_bk;
                convertString2StreamBuf(http_server_error, request_bk);
                boost::asio::write(*s, request_bk);
                throw HttpsQueryException();
            }
                
            tcp::resolver::iterator end;
            boost::system::error_code error = boost::asio::error::host_not_found;
        
            shared_ptr<tcp::socket> skt(new tcp::socket(io_service_2));
            while (error && endpoint_iterator != end)
                {
                    skt->close();
                    skt->connect(*endpoint_iterator++, error);
                }
                if (error){
                    cout<<"error in connction"<<endl;
                    boost::asio::streambuf request_bk;
                    convertString2StreamBuf(http_server_error, request_bk);
                    boost::asio::write(*s, request_bk);
                    throw HttpsQueryException();
                }
            
            cout<<"----- Successfully Connect to Http host -----"<<endl;
            // *************** HANDLE VALIDATE **************
            if (to_validate != ""){
                parsed_message rvinfo = revalidate(skt, to_validate);
                boost::asio::streambuf req_back6;
                
                if (rvinfo.mps["status_code"] == "304"){
                    convertString2StreamBuf(cache_memory.respond_cache(p_msg.mps["uri"]), req_back6);
                    boost::asio::write(*s, req_back6);
                }
                else {
                    convertString2StreamBuf(rvinfo.content, req_back6);
                    boost::asio::write(*s, req_back6);
                    cache_memory.overwrite(p_msg.mps["uri"], rvinfo);
                    logging(this_log_id, "but requries re-validation");
                }
                skt->close();
                s->close();
                return;
            }
           // *************** HANDLE VALIDATE **************
            boost::asio::streambuf request;
            convertString2StreamBuf(p_msg.content, request);
            boost::asio::write(*skt, request);
        
            parsed_message respond_infos = read_response(skt);
            //cout<<respond_infos.content<<endl;
            string respond_head = respond_infos.mps["protocol"]+" "+
                                  respond_infos.mps["status_code"]+" "+
                                  respond_infos.mps["respond_head_message"];
            logging(this_log_id, "Received "+respond_head+" from "+target_server);
            
 // ************************ CACHE **************************
            if(can_cache(p_msg.mps, respond_infos.mps)){
                cout<< "this response can be cached!"<<endl;
                cache_memory.overwrite(p_msg.mps["uri"], respond_infos);
                cout << "PUT A MESSAGE IN CACHE!!!"<<endl;
            }
            else{
                logging(this_log_id, "not cacheable because no-cache");
            }
// ********************* END CACHE **************************
            boost::asio::streambuf req_back;
            convertString2StreamBuf(respond_infos.content, req_back);
            boost::asio::write(*s, req_back);
            
            logging(this_log_id, "Responding "+head_line);
            
            skt->close();
            s->close();
            cout<<"----- Successfully handle one Http host -----"<<endl;
        }
        catch (exception &e){
            cout << "Exception in hanlde_req: " << e.what() << "\n";
            return;
        }
        return;
    }
    
    void start() {
        boost::system::error_code err;
        while (1){
        shared_ptr<tcp::socket> s (new tcp::socket(io_service_));
        acceptor_.accept(*s, err);
        thread(&server::handle_request, this, s).detach();
        }
    }
    
    server(boost::asio::io_service& io_service, short port)
    : io_service_(io_service),
    acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
    log_id(0){}
    
private:
    boost::asio::io_service& io_service_;
    tcp::acceptor acceptor_;
    HttpCache cache_memory;
    mutex log_id_mutex;
    mutex log_mutex;
    int log_id;
};

int main(int argc, char* argv[]) {
   try {
        short port = 12345;
        boost::asio::io_service io_service;
        server s(io_service, port);
        io_service.run();
        s.start();
    } catch (exception& e) {
        cout << "Exception: " << e.what() << "\n";
    }
    
    return 0;
}
