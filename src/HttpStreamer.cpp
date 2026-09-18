#include "HttpStreamer.h"
#include "UdpStreamer.h"
#include "RtpStreamer.h"
#include "RtspStreamer.h"
#include "ScanClient.h"
#include "QualityConfig.h"
#include "WebDashboard.h"
#include "WireProtocol.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <climits>
#include <algorithm>
#include <cstdlib>

namespace HttpWire {
std::string JsonEscape(const std::string& value) {
    std::ostringstream out;
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << static_cast<char>(c);
        else if (c < 0x20) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c);
        else out << static_cast<char>(c);
    }
    return out.str();
}
// TCP send는 일부만 전송할 수 있으므로 남은 바이트를 반복 전송한다. 실패하면 연결 처리를 끝낸다.
bool SendAll(const char* data, size_t size, const std::function<int(const char*, int)>& sender) {
    size_t offset = 0;
    while (offset < size) {
        int requested = static_cast<int>((std::min)(size - offset, static_cast<size_t>(INT_MAX)));
        int sent = sender(data + offset, requested);
        if (sent <= 0 || sent > requested) return false;
        offset += sent;
    }
    return true;
}
}
static bool SendSocket(SOCKET socket, const std::string& data) {
    return HttpWire::SendAll(data.data(), data.size(),
        [socket](const char* p, int n) { return send(socket, p, n, 0); });
}
static std::string LanAddress(int port) {
    SOCKET probe = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (probe == INVALID_SOCKET) return "";
    sockaddr_in remote{}, local{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(DEFAULT_UDP_PORT);
    inet_pton(AF_INET, DEFAULT_UDP_MULTICAST_ADDR, &remote.sin_addr);
    std::string address;
    if (const char* configured = std::getenv("TS_MULTICAST_INTERFACE")) {
        address = configured;
    } else if (connect(probe, reinterpret_cast<sockaddr*>(&remote), sizeof(remote)) == 0) {
        int size = sizeof(local);
        char text[INET_ADDRSTRLEN]{};
        if (getsockname(probe, reinterpret_cast<sockaddr*>(&local), &size) == 0 &&
            inet_ntop(AF_INET, &local.sin_addr, text, sizeof(text))) address = text;
    }
    closesocket(probe);
    return address.empty() ? "" : address + ":" + std::to_string(port);
}
static void SendError(SOCKET socket, int status, const std::string& message) {
    const std::string body = "{\"success\":false,\"error\":\"" + HttpWire::JsonEscape(message) + "\"}";
    SendSocket(socket, "HTTP/1.1 " + std::to_string(status) +
        (status == 400 ? " Bad Request\r\n" : status == 405 ? " Method Not Allowed\r\n" : status == 413 ? " Payload Too Large\r\n" : status == 408 ? " Request Timeout\r\n" : status == 409 ? " Conflict\r\n" : status == 500 ? " Internal Server Error\r\n" : " Service Unavailable\r\n") +
        "Content-Type: application/json; charset=UTF-8\r\nConnection: close\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\n\r\n" + body);
}
static bool ParseChannel(const std::string& request, int& channel) {
    auto pos = request.find("ch=");
    if (pos == std::string::npos) return true;
    auto end = request.find_first_of("& ", pos);
    auto value = request.substr(pos + 3, end - pos - 3);
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) return false;
    try { channel = std::stoi(value); } catch (...) { return false; }
    return channel >= 2 && channel <= 158;
}

HttpStreamer::HttpStreamer(TsBroadcaster& broadcaster, TunerClient& tuner, UdpStreamer* pUdpStreamer, int port)
    : m_broadcaster(broadcaster), m_tuner(tuner), m_pUdpStreamer(pUdpStreamer), m_port(port) {
    m_startTime = std::chrono::steady_clock::now();
}

HttpStreamer::~HttpStreamer() {
    Stop();
}

bool HttpStreamer::Start() {
    if (m_running) return true;

    // 1. IPv6 듀얼 스택 소켓 시도 (IPv4 및 IPv6 localhost ::1 동시 지원)
    m_listenSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket != INVALID_SOCKET) {
        DWORD v6Only = 0; // Dual-Stack 활성화 (IPv4 매핑 허용)
        setsockopt(m_listenSocket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&v6Only), sizeof(v6Only));

        BOOL opt = TRUE;
        setsockopt(m_listenSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&opt), sizeof(opt));

        sockaddr_in6 serverAddr6{};
        serverAddr6.sin6_family = AF_INET6;
        serverAddr6.sin6_addr = in6addr_any;
        serverAddr6.sin6_port = htons(static_cast<u_short>(m_port));

        if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&serverAddr6), sizeof(serverAddr6)) == SOCKET_ERROR) {
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
        }
    }

    // 2. IPv6 실패 시 IPv4 전용 소켓으로 대체
    if (m_listenSocket == INVALID_SOCKET) {
        m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listenSocket == INVALID_SOCKET) {
            std::cerr << "[HttpStreamer] 소켓 생성 실패: " << WSAGetLastError() << std::endl;
            return false;
        }

        BOOL opt = TRUE;
        setsockopt(m_listenSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&opt), sizeof(opt));

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(static_cast<u_short>(m_port));

        if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "[HttpStreamer] 포트 바인딩 실패 (" << m_port << "): " << WSAGetLastError() << std::endl;
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
            return false;
        }
    }

    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[HttpStreamer] 리슨 실패: " << WSAGetLastError() << std::endl;
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }

    m_running = true;
    m_startTime = std::chrono::steady_clock::now();
    m_acceptThread = std::thread(&HttpStreamer::AcceptLoop, this);

    std::cout << "[HttpStreamer] HTTP 서버 가동 시작 (IPv4/IPv6 Dual-Stack 포트: " << m_port << ")" << std::endl;
    std::cout << "  👉 웹 대시보드 주소: http://localhost:" << m_port << "/" << std::endl;
    std::cout << "  👉 HTTP TS 스트림: http://localhost:" << m_port << "/stream" << std::endl;

    return true;
}

void HttpStreamer::Stop() {
    if (!m_running) return;
    m_running = false;

    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
    }

    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }

    m_listenSocket = INVALID_SOCKET;
    std::unique_lock lock(m_clientsMutex);
    for (SOCKET socket : m_clientSockets) shutdown(socket, SD_BOTH);
    // 분리된 클라이언트 스레드도 this를 사용한다. 마지막 스레드가 소켓을 반납할 때까지 객체를 유지한다.
    m_clientsCv.wait(lock, [this] { return m_clientSockets.empty(); });
    std::cout << "[HttpStreamer] HTTP 서버 종료" << std::endl;
}

void HttpStreamer::AcceptLoop() {
    while (m_running) {
        sockaddr_storage clientAddr{};
        int clientLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(m_listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);

        if (clientSocket == INVALID_SOCKET) {
            if (m_running) {
                // 일시적 오류거나 소켓 닫힘
            }
            break;
        }

        DWORD timeout = 5000;
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        { std::lock_guard lock(m_clientsMutex); if(!m_running || m_clientSockets.size()>=64){closesocket(clientSocket);continue;} m_clientSockets.insert(clientSocket); }
        try { std::thread([this, clientSocket] {
            try { HandleClient(clientSocket); } catch(...) { /* 연결 오류를 서버 전체로 전파하지 않는다. */ }
            std::lock_guard lock(m_clientsMutex);
            closesocket(clientSocket);
            m_clientSockets.erase(clientSocket);
            m_clientsCv.notify_all();
        }).detach(); } catch(...) {std::lock_guard lock(m_clientsMutex);closesocket(clientSocket);m_clientSockets.erase(clientSocket);m_clientsCv.notify_all();}
    }
}

void HttpStreamer::HandleClient(SOCKET clientSocket) {
    std::string request;WireProtocol::Message parsed;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    for(;;) {
        auto result=WireProtocol::Extract(request,parsed);
        if(result==WireProtocol::Result::Request)break;
        if(result!=WireProtocol::Result::NeedMore){SendError(clientSocket,result==WireProtocol::Result::TooLarge?413:400,"요청 형식 또는 크기 오류");return;}
        auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
        if(remaining<=0){SendError(clientSocket,408,"요청 수신 시간 초과");return;}
        DWORD timeout=static_cast<DWORD>(remaining);setsockopt(clientSocket,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
        char bytes[2048];int n=recv(clientSocket,bytes,sizeof(bytes),0);if(n<=0)return;request.append(bytes,n);
    }
    if((parsed.version!="HTTP/1.1"&&parsed.version!="HTTP/1.0") || parsed.target.empty() || parsed.target.front()!='/') {SendError(clientSocket,400,"요청 행 오류");return;}
    const std::string firstLine=parsed.method+" "+parsed.target+" "+parsed.version;
    const auto routePath=parsed.target.substr(0,parsed.target.find('?'));
    const bool action=parsed.headers.count("x-ts-action") && parsed.headers.at("x-ts-action")=="1";
    if(parsed.method=="POST" && routePath!="/api/epg" && routePath!="/api/version" && !action){SendError(clientSocket,400,"X-TS-Action: 1 헤더가 필요합니다");return;}
    const bool control=routePath=="/api/tune" || routePath=="/api/udp/toggle" || routePath=="/api/rtp/toggle" || routePath=="/api/rtsp/toggle" || routePath=="/api/udp/enabled" || routePath=="/api/rtp/enabled" || routePath=="/api/rtsp/enabled";
    if(control) {
        if(parsed.method!="POST"){SendError(clientSocket,405,"제어 요청은 POST를 사용하세요");return;}
        if(routePath=="/api/tune"){HandleTuneApi(clientSocket,firstLine);return;}
        try {
            auto j=nlohmann::json::parse(parsed.body);
            if(!j.is_object() || !j.at("enabled").is_boolean())throw std::runtime_error("enabled 필요");
            const bool enabled=j.at("enabled").get<bool>();nlohmann::json result={{"success",true}};
            if(routePath.find("/udp/")!=std::string::npos){if(!m_pUdpStreamer)throw std::runtime_error("UDP 없음");m_pUdpStreamer->SetEnabled(enabled);result["isUdpEnabled"]=m_pUdpStreamer->IsEnabled();}
            else if(routePath.find("/rtp/")!=std::string::npos){if(!m_pRtpStreamer)throw std::runtime_error("RTP 없음");m_pRtpStreamer->SetEnabled(enabled);result["isRtpEnabled"]=m_pRtpStreamer->IsEnabled();}
            else {if(!m_pRtspStreamer)throw std::runtime_error("RTSP 없음");m_pRtspStreamer->SetEnabled(enabled);result["isRtspEnabled"]=m_pRtspStreamer->IsEnabled();}
            SendHttpResponse(clientSocket,"application/json; charset=UTF-8",result.dump());
        }catch(...){SendError(clientSocket,400,"enabled는 true/false여야 하며 해당 송출기가 필요합니다");}
        return;
    }
    if(routePath=="/api/version") {
        if(parsed.method!="GET"){SendError(clientSocket,405,"GET을 사용하세요");return;}
        nlohmann::json core=nullptr;try{core=m_tuner.Query("capabilities");}catch(...){}
        SendHttpResponse(clientSocket,"application/json; charset=UTF-8",nlohmann::json{{"serverVersion","1.3.0"},{"serverBuild",__DATE__ " " __TIME__},
            {"features",{"epg","explicitStreamControl","qualityAcknowledgement","boundedRequests"}},{"core",core}}.dump());return;
    }
    if(routePath=="/api/config/quality/status" || routePath=="/api/config/quality/applied") {
        if(!m_pQualityStore){SendError(clientSocket,503,"설정 저장소 없음");return;}
        if(routePath=="/api/config/quality/status" && parsed.method=="GET") {
            SendHttpResponse(clientSocket,"application/json; charset=UTF-8",m_pQualityStore->ApplicationStatus().dump());return;
        }
        if(routePath=="/api/config/quality/applied" && parsed.method=="POST") {
            try{if(m_pQualityStore->Acknowledge(nlohmann::json::parse(parsed.body))){SendHttpResponse(clientSocket,"application/json","{\"success\":true}");return;}}catch(...){}
            SendError(clientSocket,409,"최신 revision과 유효한 clientId를 보고하세요");return;
        }
        SendError(clientSocket,405,"지원하지 않는 메서드");return;
    }
    if(routePath=="/api/config/quality") {
        if(parsed.method=="GET")HandleGetQualityApi(clientSocket);
        else if(parsed.method=="POST")HandlePostQualityApi(clientSocket,parsed.body);
        else SendError(clientSocket,405,"GET 또는 POST를 사용하세요");
        return;
    }
    // EPG 조회는 채널을 변경하지 않는다. 선택 직후에는 collecting 상태를 반환한다.
    const auto epgSpace=firstLine.find(' '), epgLastSpace=firstLine.rfind(' ');
    const auto epgTarget=firstLine.substr(epgSpace+1,epgLastSpace-epgSpace-1);
    if(epgTarget.substr(0,epgTarget.find('?'))=="/api/epg") {
        if(firstLine.rfind("GET ",0)!=0) {
            const std::string body="{\"error\":\"GET 요청만 지원합니다\"}";
            SendSocket(clientSocket,"HTTP/1.1 405 Method Not Allowed\r\nAllow: GET\r\nContent-Type: application/json; charset=UTF-8\r\nConnection: close\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body);
            return;
        }
        try {
            const auto body=m_tuner.Query("epg").dump();
            SendSocket(clientSocket,"HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=UTF-8\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body);
        } catch(const std::exception&) {
            SendError(clientSocket,503,"EPG 조회 실패: EPG를 지원하는 튜너 DLL과 수신 상태를 확인하세요");
        }
        return;
    }
    if(m_scanner) {
        const auto firstSpace=firstLine.find(' '),lastSpace=firstLine.rfind(' ');
        const auto target=firstLine.substr(firstSpace+1,lastSpace-firstSpace-1);
        const auto queryAt=target.find('?');const auto route=target.substr(0,queryAt);
        if(route=="/api/scan" || route=="/api/channels" || route=="/api/scan/start" || route=="/api/scan/cancel" || route=="/api/channels/select" || route=="/api/channels/stop") {
            const bool readOnly=route=="/api/scan" || route=="/api/channels";
            const bool hasActionHeader = action;
            if((readOnly && firstLine.rfind("GET ",0)!=0) || (!readOnly && (firstLine.rfind("POST ",0)!=0 || !hasActionHeader))) {
                SendError(clientSocket,400,"작업 요청 형식 오류");return;
            }
            std::map<std::string,std::string> params;
            if(queryAt!=std::string::npos){std::istringstream qs(target.substr(queryAt+1));std::string pair;while(std::getline(qs,pair,'&')){auto at=pair.find('=');if(at!=std::string::npos)params[pair.substr(0,at)]=pair.substr(at+1);}}
            try {
                auto integer=[&](const char* key,int fallback){auto it=params.find(key);if(it==params.end())return fallback;size_t n=0;int x=std::stoi(it->second,&n);if(n!=it->second.size())throw std::runtime_error("숫자 입력 오류");return x;};
                if(route=="/api/scan/start") {
                    ScanOptions options;
                    if(params.count("input")){if(params["input"]!="cable" && params["input"]!="antenna")throw std::runtime_error("입력 방식 오류");options.cable=params["input"]=="cable";}
                    if(params.count("modulation")){if(params["modulation"]!="8VSB" && params["modulation"]!="QAM256")throw std::runtime_error("변조 방식 오류");options.qam=params["modulation"]=="QAM256";}
                    options.first=integer("first",2);options.last=integer("last",options.cable?135:69);options.waitMs=integer("waitMs",1800);
                    if(!m_scanner->Start(options)){SendError(clientSocket,409,"이미 스캔 중이거나 검색 범위가 올바르지 않습니다");return;}
                }else if(route=="/api/scan/cancel")m_scanner->Cancel();
                else if(route=="/api/channels/select") {
                    if(!m_scanner->Select(params["id"])){SendError(clientSocket,409,"스캔 중이거나 방송 선택에 실패했습니다");return;}
                    m_isBroadcasting = true;
                }else if(route=="/api/channels/stop") {
                    m_isBroadcasting = false;
                    m_scanner->Stop();
                }
                SendHttpResponse(clientSocket,"application/json; charset=UTF-8",m_scanner->Status().dump());
            }catch(const std::exception& e){SendError(clientSocket,400,e.what());}
            return;
        }
        if(m_scanner->IsScanning() && (target.rfind("/api/tune",0)==0 || target.rfind("/stream",0)==0 || target.rfind("/api/udp/toggle",0)==0)) {
            SendError(clientSocket,409,"채널 스캔 중입니다. 완료 후 다시 연결하세요");return;
        }
    }

    // 파비콘 요청 빠른 응답
    if (firstLine.find("GET /favicon.ico") != std::string::npos) {
        std::string noContent = "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n";
        SendSocket(clientSocket, noContent);
        return;
    }

    // 1. 웹 대시보드 홈 (GET / 또는 GET /index.html 등)
    if (firstLine.find("GET / ") != std::string::npos ||
        firstLine.find("GET /?") != std::string::npos ||
        firstLine.find("GET /index.html") != std::string::npos) {
        SendHttpResponse(clientSocket, "text/html; charset=UTF-8", WebDashboard::GetDashboardHtml());
        return;
    }

    // 2. 서버 상태 조회 API (GET /api/status)
    if (parsed.method=="GET" && routePath=="/api/status") {
        HandleStatusApi(clientSocket);
        return;
    }

    // 3. 지상파 ATSC 실시간 폐쇄자막(CC) 스냅샷 API (GET /api/caption)
    if (parsed.method=="GET" && routePath=="/api/caption") {
        HandleCaptionApi(clientSocket);
        return;
    }

    // 5. TS 실시간 스트리밍 (GET /stream?ch=XX)
    if (parsed.method=="GET" && routePath=="/stream") {
        int ch = m_tuner.GetCurrentChannel();
        bool isClearQam = m_tuner.IsClearQam();
        if (!ParseChannel(firstLine, ch)) {
            SendError(clientSocket, 400, "채널 번호가 올바르지 않습니다");
            return;
        }
        if (firstLine.find("qam=1") != std::string::npos || firstLine.find("mode=cable") != std::string::npos) {
            isClearQam = true;
        } else if (firstLine.find("mode=air") != std::string::npos || firstLine.find("qam=0") != std::string::npos) {
            isClearQam = false;
        }
        StreamTsToClient(clientSocket, ch, isClearQam);
        return;
    }

    // 6. 404 Not Found
    std::string notFound = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\nConnection: close\r\n\r\nNot Found";
    SendSocket(clientSocket, notFound);
}

void HttpStreamer::SendHttpResponse(SOCKET clientSocket, const std::string& contentType, const std::string& body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: " << contentType << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Cache-Control: no-store\r\n"
        << "Connection: close\r\n\r\n"
        << body;
    std::string response = oss.str();
    SendSocket(clientSocket, response);
}

void HttpStreamer::HandleStatusApi(SOCKET clientSocket) {
    // DLL 호출 한 번의 스냅샷으로 값 간 시간차를 줄인다.
    auto status=m_tuner.Query("status");
    status["serverAddress"]=LanAddress(m_port);
    status["isBroadcasting"]=status.value("receiving",false);status["isVirtual"]=false;
    status["activeClients"]=m_activeClients.load();status["rtspClients"]=m_pRtspStreamer?m_pRtspStreamer->GetClientCount():0;
    status["uptimeSeconds"]=GetUptimeSeconds();
    status["isUdpEnabled"]=m_pUdpStreamer&&m_pUdpStreamer->IsEnabled();
    status["udpTarget"]=m_pUdpStreamer?m_pUdpStreamer->GetTargetAddress():"";
    status["isRtpEnabled"]=m_pRtpStreamer&&m_pRtpStreamer->IsEnabled();
    status["rtpTarget"]=m_pRtpStreamer?m_pRtpStreamer->GetTargetAddress():"";
    status["isRtspEnabled"]=m_pRtspStreamer&&m_pRtspStreamer->IsEnabled();
    status["rtspPort"]=m_pRtspStreamer?m_pRtspStreamer->GetPort():DEFAULT_RTSP_PORT;
    SendHttpResponse(clientSocket,"application/json; charset=UTF-8",status.dump());
}

void HttpStreamer::HandleCaptionApi(SOCKET clientSocket) {
    try {
        const auto captionJson = m_tuner.QueryCaption();
        if (!captionJson.is_null() && !captionJson.empty()) {
            SendHttpResponse(clientSocket, "application/json; charset=UTF-8", captionJson.dump());
            return;
        }
    } catch (...) {}
    SendHttpResponse(clientSocket, "application/json; charset=UTF-8", "{\"visible\":false,\"text\":\"\",\"lines\":[],\"windows\":[],\"events\":[]}");
}

void HttpStreamer::HandleTuneApi(SOCKET clientSocket, const std::string& request) {
    int targetCh = m_tuner.GetCurrentChannel();
    bool isClearQam = m_tuner.IsClearQam();

    if (request.find("qam=1") != std::string::npos || request.find("mode=cable") != std::string::npos) {
        isClearQam = true;
    } else if (request.find("qam=0") != std::string::npos || request.find("mode=air") != std::string::npos) {
        isClearQam = false;
    }

    if (!ParseChannel(request, targetCh) || !m_tuner.IsChannelSupported(targetCh, isClearQam)) {
        SendError(clientSocket, 400, "채널 번호가 올바르지 않습니다");
        return;
    }
    if (m_scanner ? !m_scanner->TuneLegacy(targetCh,isClearQam) : (!m_tuner.Tune(targetCh, isClearQam) || !m_tuner.Start())) {
        SendError(clientSocket, 503, m_tuner.GetDriverStatus());
        return;
    }
    m_isBroadcasting = true;

    std::ostringstream json;
    json << "{\"success\":true,\"tunedChannel\":" << targetCh 
         << ",\"isClearQam\":" << (isClearQam ? "true" : "false")
         << ",\"modulation\":\"" << (isClearQam ? "Clear QAM 256" : "ATSC 8VSB") << "\"}";
    SendHttpResponse(clientSocket, "application/json; charset=UTF-8", json.str());
}

// 스트리밍/디코딩 품질 설정 조회 API (GET /api/config/quality)
void HttpStreamer::HandleGetQualityApi(SOCKET clientSocket) {
    std::string jsonStr = m_pQualityStore ? m_pQualityStore->ToJsonString() : "{}";
    SendHttpResponse(clientSocket, "application/json; charset=UTF-8", jsonStr);
}

// 스트리밍/디코딩 품질 설정 저장 API (POST /api/config/quality)
void HttpStreamer::HandlePostQualityApi(SOCKET clientSocket, const std::string& body) {
    if (!m_pQualityStore) {
        SendError(clientSocket, 500, "QualityStore 미설정");
        return;
    }
    bool success = m_pQualityStore->UpdateFromJson(body);
    if (success) {
        std::ostringstream json;
        json << "{\"success\":true,\"message\":\"품질 설정이 성공적으로 저장되었습니다\",\"config\":"
             << m_pQualityStore->ToJsonString() << "}";
        SendHttpResponse(clientSocket, "application/json; charset=UTF-8", json.str());
    } else {
        SendError(clientSocket, 400, "잘못된 JSON 형식 또는 설정 저장 실패");
    }
}

void HttpStreamer::StreamTsToClient(SOCKET clientSocket, int channel, bool isClearQam) {
    if (!m_tuner.IsChannelSupported(channel, isClearQam)) {
        SendError(clientSocket, 400, "채널 번호가 올바르지 않습니다");
        return;
    }
    if (m_scanner ? !m_scanner->TuneLegacy(channel,isClearQam) : (!m_tuner.Tune(channel, isClearQam) || !m_tuner.Start())) {
        SendError(clientSocket, 503, m_tuner.GetDriverStatus());
        return;
    }
    m_isBroadcasting = true;
    auto subscriber = m_broadcaster.CreateSubscriber(1024 * 1024 * 4);
    std::vector<uint8_t> sendBuffer(CHUNK_SIZE);
    // 최초 실제 TS가 없으면 200으로 빈 스트림을 가장하지 않는다.
    if (!subscriber->PopData(sendBuffer.data(), CHUNK_SIZE, 5000)) {
        m_broadcaster.RemoveSubscriber(subscriber);
        SendError(clientSocket, 503, "방송 TS 미수신: 케이블 채널과 신호를 확인하세요");
        return;
    }

    // Wi-Fi 환경 전송 버퍼 1MB 확장
    int sndbuf = 1024 * 1024;
    setsockopt(clientSocket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&sndbuf), sizeof(sndbuf));

    // 송신 타임아웃 5초 설정 (클라이언트 접속 끊김 자동 감지)
    DWORD timeout = 5000;
    setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    // HTTP 200 Live Streaming 응답 헤더 전송
    std::string httpHeader =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: video/mp2t\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Cache-Control: no-cache, no-store\r\n\r\n";

    if (!SendSocket(clientSocket, httpHeader)) {
        m_broadcaster.RemoveSubscriber(subscriber);
        return;
    }
    ++m_activeClients;
    do {
        if (!m_running || (m_scanner && m_scanner->IsScanning())) break;
        const bool sent = HttpWire::SendAll(reinterpret_cast<const char*>(sendBuffer.data()), sendBuffer.size(),
            [this, clientSocket](const char* p, int n) {
                int count = send(clientSocket, p, n, 0);
                if (count > 0) m_totalBytesSent += count;
                return count;
            });
        if (!sent) break;
    } while (subscriber->PopData(sendBuffer.data(), CHUNK_SIZE, 2000));

    // 접속 종료 시 브로드캐스터에서 구독자 해제
    m_broadcaster.RemoveSubscriber(subscriber);

    m_activeClients--;
    std::cout << "[HttpStreamer] 플레이어 연결 해제 (현재 동시 접속자: " << m_activeClients << "명)" << std::endl;
}

double HttpStreamer::GetCurrentBitrateMbps() const {
    return m_tuner.GetBitrateMbps();
}

uint64_t HttpStreamer::GetUptimeSeconds() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime).count();
}
