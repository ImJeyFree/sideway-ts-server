#include "RtspStreamer.h"
#include "RtpStreamer.h"
#include "QualityConfig.h"
#include "WireProtocol.h"
#include <future>
#include <iostream>
#include <fstream>
#include <stdexcept>
using namespace std::chrono_literals;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Client {
    SOCKET socket=INVALID_SOCKET;std::string pending;
    Client(int port){socket=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);DWORD timeout=3000;setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<char*>(&timeout),sizeof(timeout));sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(port);inet_pton(AF_INET,"127.0.0.1",&a.sin_addr);Check(connect(socket,reinterpret_cast<sockaddr*>(&a),sizeof(a))==0,"RTSP connect");}
    ~Client(){if(socket!=INVALID_SOCKET)closesocket(socket);}
    void Send(const std::string& s){size_t at=0;while(at<s.size()){int n=send(socket,s.data()+at,int(s.size()-at),0);Check(n>0,"send");at+=n;}}
    std::string Response(){
        for(;;){
            if(!pending.empty()&&pending[0]=='$'){
                if(pending.size()>=4){size_t n=4+(static_cast<unsigned char>(pending[2])<<8)+static_cast<unsigned char>(pending[3]);if(pending.size()>=n){pending.erase(0,n);continue;}}
            }else {auto end=pending.find("\r\n\r\n");if(end!=std::string::npos){auto result=pending.substr(0,end+4);pending.erase(0,end+4);return result;}}
            char b[4096];int n=recv(socket,b,sizeof(b),0);Check(n>0,"response closed or timed out");pending.append(b,n);
        }
    }
    std::string Request(const std::string& method,const std::string& headers=""){Send(method+" rtsp://127.0.0.1/live RTSP/1.0\r\nCSeq: 1\r\n"+headers+"\r\n");return Response();}
};
static std::string Session(const std::string& response){auto at=response.find("Session: ");Check(at!=std::string::npos,"session missing");at+=9;return response.substr(at,response.find(';',at)-at);}
int main(int argc,char**) {try{
    {using namespace WireProtocol;Message m;
     std::string b="POST /x HTTP/1.1\r\nContent-Length: 4\r\n\r\nab";Check(Extract(b,m)==Result::NeedMore,"partial body");b+="cd";Check(Extract(b,m)==Result::Request&&m.body=="abcd","body boundary");
     b="POST / HTTP/1.1\r\nContent-Length: 999999999999999\r\n\r\n";Check(Extract(b,m)==Result::TooLarge,"body limit");
     b="GET / HTTP/1.1\r\nContent-Length: 0\r\ncontent-length: 1\r\n\r\n";Check(Extract(b,m)==Result::Invalid,"duplicate header");
     b=std::string(8193,'x');Check(Extract(b,m)==Result::TooLarge,"header limit");}
    auto temp=std::filesystem::temp_directory_path()/("sideway-quality-test-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(temp);auto file=temp/"quality.json";
    {QualityStore store(file);Check(!store.UpdateFromJson("[]"),"nonobject");Check(!store.UpdateFromJson("{\"networkCachingMs\":-1}"),"negative cache");Check(!store.UpdateFromJson("{\"avcodecThreads\":1.5}"),"fractional threads");Check(!store.UpdateFromJson("{\"rtspTransport\":\"bad\"}"),"transport enum");
     Check(store.UpdateFromJson("{\"networkCachingMs\":1500}"),"save valid");QualityStore loaded(file);Check(loaded.Get().networkCachingMs==1500,"reload");
     auto revision=nlohmann::json::parse(store.ToJsonString()).at("revision");Check(store.Acknowledge({{"clientId","test"},{"revision",revision}}),"ack current");Check(!store.Acknowledge({{"clientId","test"},{"revision",0}}),"ack stale");
     // 대상 파일을 열어 Windows 교체 실패를 만들고 메모리/원본 보존을 확인한다.
     HANDLE locked=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);Check(locked!=INVALID_HANDLE_VALUE,"lock fixture");
     Check(!store.UpdateFromJson("{\"networkCachingMs\":2000}"),"save failure reported");Check(store.Get().networkCachingMs==1500,"rollback memory");CloseHandle(locked);QualityStore again(file);Check(again.Get().networkCachingMs==1500,"original preserved");}
    std::filesystem::remove(file);std::filesystem::remove(temp);
    WSADATA w{};Check(WSAStartup(MAKEWORD(2,2),&w)==0,"winsock");
    {TsBroadcaster hub;RtspStreamer server(hub,0);Check(server.Start(),"RTSP start");server.SetEnabled(true);
     {Client client(server.GetPort());auto setup=client.Request("SETUP","Transport: RTP/AVP/TCP;unicast;interleaved=2-3\r\n");Check(setup.find("200 OK")!=std::string::npos&&setup.find("timeout=60")!=std::string::npos,"TCP setup");auto id=Session(setup);auto header="Session: "+id+"\r\n";
      for(int i=0;i<4;++i){Check(client.Request("PLAY",header).find("200 OK")!=std::string::npos,"PLAY");std::vector<uint8_t> ts(188*7,0xff);for(size_t at=0;at<ts.size();at+=188)ts[at]=0x47;hub.PushData(ts.data(),ts.size());Check(client.Request("PAUSE",header).find("200 OK")!=std::string::npos,"PAUSE");}
      // RTCP + 본문 있는 keep-alive + 다음 요청이 같은 TCP 데이터에 붙어도 구분한다.
      std::string rtcp("$\x03\x00\x08\x80\xc9\x00\x01\x00\x00\x00\x01",12);
      client.Send(rtcp+"GET_PARAMETER rtsp://127.0.0.1/live RTSP/1.0\r\nCSeq: 2\r\n"+header+"Content-Length: 4\r\n\r\npingOPTIONS rtsp://127.0.0.1/live RTSP/1.0\r\nCSeq: 3\r\n\r\n");Check(client.Response().find("200 OK")!=std::string::npos,"RTCP demux");Check(client.Response().find("200 OK")!=std::string::npos,"body boundary next request");
      if(argc>1){std::this_thread::sleep_for(31s);Check(client.Request("GET_PARAMETER",header).find("200 OK")!=std::string::npos,"advertised timeout retained after 30s");}
      Check(client.Request("TEARDOWN",header).find("200 OK")!=std::string::npos,"teardown");}
     {SOCKET receiver=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);Check(bind(receiver,reinterpret_cast<sockaddr*>(&a),sizeof(a))==0,"UDP receiver");int n=sizeof(a);getsockname(receiver,reinterpret_cast<sockaddr*>(&a),&n);int port=ntohs(a.sin_port);DWORD timeout=2000;setsockopt(receiver,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<char*>(&timeout),sizeof(timeout));
      Client client(server.GetPort());auto setup=client.Request("SETUP","Transport: RTP/AVP;unicast;client_port="+std::to_string(port)+"-"+std::to_string(port==65535?port-1:port+1)+"\r\n");Check(setup.find("200 OK")!=std::string::npos,"UDP setup");auto id=Session(setup);int sourcePort=std::stoi(setup.substr(setup.find("server_port=")+12));
      Check(client.Request("PLAY","Session: "+id+"\r\n").find("200 OK")!=std::string::npos,"UDP play");std::this_thread::sleep_for(30ms);std::vector<uint8_t> bytes(188*7,0x47);hub.PushData(bytes.data(),bytes.size());char data[2048];sockaddr_in from{};n=sizeof(from);Check(recvfrom(receiver,data,sizeof(data),0,reinterpret_cast<sockaddr*>(&from),&n)==1328,"RTP payload");Check(ntohs(from.sin_port)==sourcePort,"advertised source port");closesocket(receiver);}
     {Client client(server.GetPort());auto stopped=std::async(std::launch::async,[&]{server.Stop();});Check(stopped.wait_for(3s)==std::future_status::ready,"active client stop");stopped.get();}
    }
    {TsBroadcaster hub;SOCKET receiver=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
     sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);Check(bind(receiver,reinterpret_cast<sockaddr*>(&a),sizeof(a))==0,"RTP receiver");int length=sizeof(a);getsockname(receiver,reinterpret_cast<sockaddr*>(&a),&length);
     DWORD timeout=3000;setsockopt(receiver,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<char*>(&timeout),sizeof(timeout));
     RtpStreamer rtp(hub,"127.0.0.1",ntohs(a.sin_port));Check(rtp.Start(),"RTP start");rtp.SetEnabled(true);
     auto timestamp=[&]{std::vector<uint8_t> ts(188*7,0x47);hub.PushData(ts.data(),ts.size());uint8_t packet[2048];Check(recv(receiver,reinterpret_cast<char*>(packet),sizeof(packet),0)==1328,"RTP read");return (uint32_t(packet[4])<<24)|(uint32_t(packet[5])<<16)|(uint32_t(packet[6])<<8)|packet[7];};
     auto first=timestamp();std::this_thread::sleep_for(120ms);auto second=timestamp();Check(uint32_t(second-first)>5000,"RTP timestamp follows elapsed time, not fixed 49");rtp.Stop();closesocket(receiver);}
    WSACleanup();std::cout<<"PASS: bounded parsing, quality transaction, RTSP resume/RTCP/UDP/stop\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
