#pragma once
#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <string>

// HTTP/RTSP 제어 메시지는 크기를 제한하고 완전한 본문까지 수신한 뒤 처리한다.
namespace WireProtocol {
constexpr size_t MaxHeader = 8192, MaxBody = 16384, MaxBuffered = 73728;
enum class Result { NeedMore, Request, Interleaved, Invalid, TooLarge };
struct Message {
    std::string method, target, version, body;
    std::map<std::string,std::string> headers;
    unsigned channel = 0;
};
inline std::string Lower(std::string s) {
    for(auto& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
inline std::string Trim(std::string s) {
    auto begin=s.find_first_not_of(" \t"),end=s.find_last_not_of(" \t");
    return begin==std::string::npos?std::string{}:s.substr(begin,end-begin+1);
}
inline Result Extract(std::string& buffer, Message& out, bool interleaved=false) {
    out={};
    if(buffer.size()>MaxBuffered)return Result::TooLarge;
    if(buffer.empty())return Result::NeedMore;
    if(interleaved && buffer[0]=='$') {
        if(buffer.size()<4)return Result::NeedMore;
        size_t n=(static_cast<unsigned char>(buffer[2])<<8)|static_cast<unsigned char>(buffer[3]);
        if(buffer.size()<n+4)return Result::NeedMore;
        out.channel=static_cast<unsigned char>(buffer[1]);out.body=buffer.substr(4,n);
        buffer.erase(0,n+4);return Result::Interleaved;
    }
    auto end=buffer.find("\r\n\r\n");
    if(end==std::string::npos)return buffer.size()>MaxHeader?Result::TooLarge:Result::NeedMore;
    if(end+4>MaxHeader)return Result::TooLarge;
    std::istringstream input(buffer.substr(0,end));std::string line,extra;
    if(!std::getline(input,line))return Result::Invalid;
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    std::istringstream first(line);
    if(!(first>>out.method>>out.target>>out.version)||(first>>extra))return Result::Invalid;
    while(std::getline(input,line)) {
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        auto colon=line.find(':');if(colon==0||colon==std::string::npos)return Result::Invalid;
        auto key=Lower(line.substr(0,colon));
        if(key.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-_")!=std::string::npos)return Result::Invalid;
        if(!out.headers.emplace(key,Trim(line.substr(colon+1))).second)return Result::Invalid;
    }
    if(out.headers.count("transfer-encoding"))return Result::Invalid;
    size_t n=0;
    if(auto it=out.headers.find("content-length");it!=out.headers.end()) {
        if(it->second.empty())return Result::Invalid;
        for(char c:it->second){if(c<'0'||c>'9')return Result::Invalid;n=n*10+(c-'0');if(n>MaxBody)return Result::TooLarge;}
    }
    if(buffer.size()<end+4+n)return Result::NeedMore;
    out.body=buffer.substr(end+4,n);buffer.erase(0,end+4+n);return Result::Request;
}
}
