#include "RtspStreamer.h"
#include "WireProtocol.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <chrono>

RtspStreamer::RtspStreamer(TsBroadcaster& broadcaster, int port)
    : m_broadcaster(broadcaster), m_port(port) {
}

RtspStreamer::~RtspStreamer() {
    Stop();
}

bool RtspStreamer::Start() {
    if (m_running) return true;

    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET) {
        std::cerr << "[RtspStreamer] TCP 소켓 생성 실패: " << WSAGetLastError() << std::endl;
        return false;
    }

    BOOL reuse = TRUE;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(static_cast<u_short>(m_port));

    if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[RtspStreamer] 바인드 실패 (포트 " << m_port << "): " << WSAGetLastError() << std::endl;
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }

    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[RtspStreamer] listen 실패: " << WSAGetLastError() << std::endl;
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }

    {sockaddr_in bound{};int n=sizeof(bound);if(getsockname(m_listenSocket,reinterpret_cast<sockaddr*>(&bound),&n)==0)m_port=ntohs(bound.sin_port);}
    m_running = true;
    m_acceptThread = std::thread(&RtspStreamer::AcceptLoop, this);

    std::cout << "[RtspStreamer] RTSP 스트리밍 서버 가동 -> rtsp://0.0.0.0:" << m_port << "/live" << std::endl;
    return true;
}

// 소켓 정리 소유자는 세션 스레드다. 서버 종료는 중단 요청 후 해당 스레드를 회수한다.
void RtspStreamer::Interrupt(const std::shared_ptr<RtspClientSession>& session) {
    session->sessionRunning=false;
    std::lock_guard lock(session->socketMutex);
    if(session->tcpSocket!=INVALID_SOCKET)shutdown(session->tcpSocket,SD_BOTH);
}
void RtspStreamer::Stop() {
    if(!m_running.exchange(false))return;
    closesocket(m_listenSocket);
    if(m_acceptThread.joinable())m_acceptThread.join();
    m_listenSocket=INVALID_SOCKET;
    std::vector<std::shared_ptr<RtspClientSession>> sessions;
    {std::lock_guard lock(m_sessionsMutex);sessions.swap(m_sessions);}
    for(auto& session:sessions)Interrupt(session);
    for(auto& session:sessions)if(session->handlerThread.joinable())session->handlerThread.join();
}

size_t RtspStreamer::GetClientCount() const {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    size_t count = 0;
    for (const auto& s : m_sessions) {
        if (s->isPlaying) ++count;
    }
    return count;
}

void RtspStreamer::SetEnabled(bool enabled) {
    m_enabled=enabled;
    if(!enabled){std::lock_guard lock(m_sessionsMutex);for(auto& s:m_sessions)Interrupt(s);}
}
void RtspStreamer::AcceptLoop() {
    while(m_running) {
        sockaddr_in addr{};int length=sizeof(addr);
        SOCKET socket=accept(m_listenSocket,reinterpret_cast<sockaddr*>(&addr),&length);
        if(socket==INVALID_SOCKET)break;
        char ip[INET_ADDRSTRLEN]{};inet_ntop(AF_INET,&addr.sin_addr,ip,sizeof(ip));
        std::lock_guard lock(m_sessionsMutex);
        // 끝난 스레드를 계속 쌓지 않는다. finished 이후에는 서버 상태에 접근하지 않는다.
        for(auto it=m_sessions.begin();it!=m_sessions.end();) {
            if((*it)->finished){(*it)->handlerThread.join();it=m_sessions.erase(it);}else ++it;
        }
        if(!m_running || m_sessions.size()>=32){closesocket(socket);continue;}
        auto session=std::make_shared<RtspClientSession>();
        session->tcpSocket=socket;session->clientIp=ip;session->sessionId=std::to_string(++m_sessionCounter);
        session->sessionRunning=true;m_sessions.push_back(session);
        try {session->handlerThread=std::thread(&RtspStreamer::HandleClient,this,session);}
        catch(...){m_sessions.pop_back();closesocket(socket);}
    }
}
void RtspStreamer::StopStream(const std::shared_ptr<RtspClientSession>& s) {
    s->isPlaying=false;
    if(s->subscriber)s->subscriber->Stop();
    if(s->streamThread.joinable())s->streamThread.join();
    if(s->subscriber){m_broadcaster.RemoveSubscriber(s->subscriber);s->subscriber.reset();}
}
void RtspStreamer::HandleClient(std::shared_ptr<RtspClientSession> session) {
    const SOCKET socket=session->tcpSocket;
    DWORD sendTimeout=2000;
    setsockopt(socket,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&sendTimeout),sizeof(sendTimeout));
    int yes=1;setsockopt(socket,IPPROTO_TCP,TCP_NODELAY,reinterpret_cast<const char*>(&yes),sizeof(yes));
    std::string buffer;char bytes[4096];bool close=false;
    using Clock=std::chrono::steady_clock;
    auto deadline=Clock::now()+std::chrono::seconds(60);
    try {
        while(m_running && session->sessionRunning && !close) {
            WireProtocol::Message message;auto result=WireProtocol::Extract(buffer,message,true);
            if(result==WireProtocol::Result::NeedMore) {
                auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-Clock::now()).count();
                if(remaining<=0)break;
                // 막 접속한 유휴 세션에서도 shutdown/recv 경합 때문에 종료가 60초 지연되지 않게 한다.
                // 소켓 닫기만으로 대기를 깨우는 데 의존하지 않고 제한된 간격으로 종료 플래그를 확인한다.
                fd_set readable;FD_ZERO(&readable);FD_SET(socket,&readable);
                timeval interval{0,250000};int ready=select(0,&readable,nullptr,nullptr,&interval);
                if(!m_running || !session->sessionRunning || ready<0)break;
                if(!ready)continue;
                int n=recv(socket,bytes,sizeof(bytes),0);if(n<=0)break;
                if(buffer.empty())deadline=(std::min)(deadline,Clock::now()+std::chrono::seconds(5));
                buffer.append(bytes,n);continue;
            }
            if(result==WireProtocol::Result::Invalid || result==WireProtocol::Result::TooLarge)break;
            if(result==WireProtocol::Result::Interleaved) {
                // 길이로 분리한 RTCP만 소비한다. 바이너리를 다음 RTSP 요청에 섞지 않는다.
                if(!session->setup || message.channel!=session->rtcpChannel || message.body.size()<4 ||
                   (static_cast<unsigned char>(message.body[0])>>6)!=2)break;
            } else {
                if(message.version!="RTSP/1.0")break;
                std::ostringstream request;request<<message.method<<" "<<message.target<<" "<<message.version<<"\r\n";
                for(auto& [key,value]:message.headers)request<<key<<": "<<value<<"\r\n";
                request<<"\r\n";
                bool play=false;auto response=ProcessRtspRequest(request.str(),session,close,play);
                {std::lock_guard lock(session->sendMutex);size_t at=0;
                 while(at<response.size()){int n=send(socket,response.data()+at,int(response.size()-at),0);if(n<=0){close=true;break;}at+=n;}}
                if(play && !close && !session->isPlaying) {
                    StopStream(session);
                    session->subscriber=m_broadcaster.CreateSubscriber();session->isPlaying=true;
                    session->streamThread=std::thread(&RtspStreamer::StreamLoop,this,session);
                }
            }
            deadline=Clock::now()+std::chrono::seconds(buffer.empty()?60:5);
        }
    }catch(...){/* 세션 오류를 서버 전체 종료로 전파하지 않는다. */}
    Interrupt(session);StopStream(session);
    {std::lock_guard lock(session->socketMutex);closesocket(session->tcpSocket);session->tcpSocket=INVALID_SOCKET;}
    if(session->udpSocket!=INVALID_SOCKET)closesocket(session->udpSocket);
    if(session->rtcpSocket!=INVALID_SOCKET)closesocket(session->rtcpSocket);
    session->finished=true;
}

std::string RtspStreamer::ProcessRtspRequest(const std::string& request,
                                            std::shared_ptr<RtspClientSession>& session,
                                            bool& shouldClose,
                                            bool& startStream) {
    std::istringstream stream(request);
    std::string line;
    if (!std::getline(stream, line)) return "";

    std::cout << "[RtspStreamer] >>> " << line << std::endl;

    // 공백 및 CR 제거
    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream lineStream(line);
    std::string method, url, version;
    lineStream >> method >> url >> version;

    std::string cseq = "1";
    std::string transport;
    std::string sessionHeader;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string headerName = line.substr(0, colon);
            std::string headerValue = line.substr(colon + 1);
            // 앞뒤 공백 제거
            while (!headerValue.empty() && (headerValue.front() == ' ' || headerValue.front() == '\t')) headerValue.erase(0, 1);
            while (!headerValue.empty() && (headerValue.back() == ' ' || headerValue.back() == '\t')) headerValue.pop_back();

            std::string lowerName = headerName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            if (lowerName == "cseq") {
                cseq = headerValue;
            } else if (lowerName == "transport") {
                transport = headerValue;
            } else if (lowerName == "session") {
                sessionHeader = headerValue;
            }
        }
    }

    std::ostringstream response;
    auto error=[&](int code,const char* text){return "RTSP/1.0 "+std::to_string(code)+" "+text+"\r\nCSeq: "+cseq+"\r\n\r\n";};
    if((method=="PLAY"||method=="PAUSE"||method=="TEARDOWN"||method=="GET_PARAMETER"||method=="SET_PARAMETER") &&
       (!session->setup || sessionHeader.substr(0,sessionHeader.find(';'))!=session->sessionId))return error(454,"Session Not Found");
    if(method=="SETUP" && session->isPlaying)return error(455,"Method Not Valid in This State");

    if (!m_enabled.load()) {
        response << "RTSP/1.0 503 Service Unavailable\r\n"
                 << "CSeq: " << cseq << "\r\n"
                 << "Connection: close\r\n\r\n";
        shouldClose = true;
        return response.str();
    }

    if (method == "OPTIONS") {
        response << "RTSP/1.0 200 OK\r\n"
                 << "CSeq: " << cseq << "\r\n"
                 << "Public: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN, GET_PARAMETER\r\n\r\n";
    }
    else if (method == "DESCRIBE") {
        // RFC 2250 MPEG-2 TS SDP 생성 (Payload Type 33)
        std::ostringstream sdp;
        sdp << "v=0\r\n"
            << "o=- " << session->sessionId << " 1 IN IP4 0.0.0.0\r\n"
            << "s=Sideway TS Live Broadcast\r\n"
            << "c=IN IP4 0.0.0.0\r\n"
            << "t=0 0\r\n"
            << "m=video 0 RTP/AVP " << static_cast<int>(RTP_PAYLOAD_TYPE_MP2T) << "\r\n"
            << "a=control:track0\r\n";

        std::string sdpBody = sdp.str();
        response << "RTSP/1.0 200 OK\r\n"
                 << "CSeq: " << cseq << "\r\n"
                 << "Content-Type: application/sdp\r\n"
                 << "Content-Base: " << url << "/\r\n"
                 << "Content-Length: " << sdpBody.length() << "\r\n\r\n"
                 << sdpBody;
    }
    else if (method == "SETUP") {
        StopStream(session);session->setup=false;
        if(session->udpSocket!=INVALID_SOCKET){closesocket(session->udpSocket);session->udpSocket=INVALID_SOCKET;}
        if(session->rtcpSocket!=INVALID_SOCKET){closesocket(session->rtcpSocket);session->rtcpSocket=INVALID_SOCKET;}
        std::string responseTransport;

        // TCP Interleaved 모드 감지 (예: RTP/AVP/TCP;unicast;interleaved=0-1)
        if (transport.find("TCP") != std::string::npos || transport.find("interleaved") != std::string::npos) {
            session->transportMode = TransportMode::TCP_INTERLEAVED;
            unsigned rtp=0,rtcp=1;auto at=transport.find("interleaved=");
            if(at!=std::string::npos && sscanf_s(transport.c_str()+at+12,"%u-%u",&rtp,&rtcp)!=2)return error(461,"Unsupported Transport");
            if(rtp>255||rtcp>255||rtp==rtcp)return error(461,"Unsupported Transport");
            session->rtpChannel=uint8_t(rtp);session->rtcpChannel=uint8_t(rtcp);
            responseTransport="RTP/AVP/TCP;unicast;interleaved="+std::to_string(rtp)+"-"+std::to_string(rtcp)+";ssrc=87654321";
        }
        else {
            // UDP Unicast 모드 (예: RTP/AVP;unicast;client_port=50000-50001)
            session->transportMode = TransportMode::UDP_UNICAST;
            int clientRtp = 0, clientRtcp = 0;
            size_t cpPos = transport.find("client_port=");
            if (cpPos != std::string::npos) {
                sscanf_s(transport.c_str() + cpPos + 12, "%d-%d", &clientRtp, &clientRtcp);
            }
            if(clientRtp<1||clientRtp>65535||clientRtcp<1||clientRtcp>65535||clientRtp==clientRtcp || transport.find("multicast")!=std::string::npos)return error(461,"Unsupported Transport");

            session->clientRtpPort = clientRtp;
            session->clientRtcpPort = clientRtcp;
            // 실제 사용 가능한 연속 RTP/RTCP 포트를 확보한 뒤 응답한다.
            for(int attempt=0;attempt<64;++attempt){
                SOCKET rtp=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP),rtcp=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
                sockaddr_in local{};local.sin_family=AF_INET;local.sin_addr.s_addr=htonl(INADDR_ANY);
                int len=sizeof(local);bool ok=rtp!=INVALID_SOCKET&&rtcp!=INVALID_SOCKET;
                if(ok)ok=bind(rtp,reinterpret_cast<sockaddr*>(&local),sizeof(local))==0 && getsockname(rtp,reinterpret_cast<sockaddr*>(&local),&len)==0;
                int port=ntohs(local.sin_port);
                if(ok && port<65535 && !(port&1)){local.sin_port=htons(static_cast<u_short>(port+1));ok=bind(rtcp,reinterpret_cast<sockaddr*>(&local),sizeof(local))==0;}else ok=false;
                if(ok){session->udpSocket=rtp;session->rtcpSocket=rtcp;session->serverRtpPort=port;session->serverRtcpPort=port+1;break;}
                if(rtp!=INVALID_SOCKET)closesocket(rtp);if(rtcp!=INVALID_SOCKET)closesocket(rtcp);
            }
            if(session->udpSocket==INVALID_SOCKET)return error(500,"UDP Bind Failed");
            u_long nonblocking=1;ioctlsocket(session->rtcpSocket,FIONBIO,&nonblocking);

            std::memset(&session->clientRtpAddr, 0, sizeof(session->clientRtpAddr));
            session->clientRtpAddr.sin_family = AF_INET;
            session->clientRtpAddr.sin_port = htons(static_cast<u_short>(session->clientRtpPort));
            inet_pton(AF_INET, session->clientIp.c_str(), &session->clientRtpAddr.sin_addr);

            std::ostringstream ts;
            ts << "RTP/AVP;unicast;client_port=" << clientRtp << "-" << clientRtcp
               << ";server_port=" << session->serverRtpPort << "-" << session->serverRtcpPort << ";ssrc=87654321";
            responseTransport = ts.str();
        }

        session->setup=true;
        response << "RTSP/1.0 200 OK\r\n"
                 << "CSeq: " << cseq << "\r\n"
                 << "Transport: " << responseTransport << "\r\n"
                 << "Session: " << session->sessionId << ";timeout=60\r\n\r\n";
    }
    else if (method == "PLAY") {
        startStream = true;

        response << "RTSP/1.0 200 OK\r\n"
                 << "CSeq: " << cseq << "\r\n"
                 << "Session: " << session->sessionId << "\r\n"
                 << "Range: npt=0.000-\r\n"
                 << "\r\n";
    }
    else if (method == "PAUSE") {
        StopStream(session);
        response << "RTSP/1.0 200 OK\r\n"
                 << "CSeq: " << cseq << "\r\n"
                 << "Session: " << session->sessionId << "\r\n\r\n";
    }
    else if (method == "TEARDOWN") {
        shouldClose = true;
        response << "RTSP/1.0 200 OK\r\n"
                 << "CSeq: " << cseq << "\r\n"
                 << "Session: " << session->sessionId << "\r\n\r\n";
    }
    else if (method == "GET_PARAMETER" || method == "SET_PARAMETER") {
        // Keep-Alive 핑
        response << "RTSP/1.0 200 OK\r\n"
                 << "CSeq: " << cseq << "\r\n"
                 << "Session: " << session->sessionId << "\r\n\r\n";
    }
    else {
        response << "RTSP/1.0 501 Not Implemented\r\n"
                 << "CSeq: " << cseq << "\r\n\r\n";
    }

    return response.str();
}

void RtspStreamer::StreamLoop(std::shared_ptr<RtspClientSession> session) {
    // 12바이트 RFC 3550 RTP 고정 헤더 + 1316바이트 TS 페이로드 = 1328 바이트
    std::vector<uint8_t> rtpPacket(RTP_DATAGRAM_SIZE);
    std::vector<uint8_t> tsPayload(UDP_DATAGRAM_SIZE);
    std::vector<uint8_t> tcpFrame(4 + RTP_DATAGRAM_SIZE);



    while (m_running && m_enabled.load() && session->sessionRunning && session->isPlaying) {
        if (!session->subscriber || !session->subscriber->PopData(tsPayload.data(), UDP_DATAGRAM_SIZE,100)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        if(!session->isPlaying || !session->sessionRunning)break;
        // RTCP 수신 버퍼를 제한된 횟수로 소비한다. 송신자 보고서는 별도 확장 대상이다.
        if(session->rtcpSocket!=INVALID_SOCKET){char report[2048];for(int i=0;i<8;++i)if(recv(session->rtcpSocket,report,sizeof(report),0)<=0)break;}
        // 1. RFC 3550 RTP 헤더 구성
        rtpPacket[0] = 0x80; // V=2, P=0, X=0, CC=0
        rtpPacket[1] = RTP_PAYLOAD_TYPE_MP2T & 0x7F; // PT=33

        uint16_t seq = htons(session->seqNumber++);
        std::memcpy(&rtpPacket[2], &seq, sizeof(seq));

        // 실시간 90kHz MPEG 클럭 타임스탬프 계산 (지터 및 누적 오차 원천 차단)
        auto now = std::chrono::steady_clock::now();
        auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        uint32_t currentTs = static_cast<uint32_t>((elapsedUs * 90) / 1000);
        uint32_t ts = htonl(currentTs);
        std::memcpy(&rtpPacket[4], &ts, sizeof(ts));

        uint32_t ssrc = htonl(session->ssrc);
        std::memcpy(&rtpPacket[8], &ssrc, sizeof(ssrc));

        // 2. TS 페이로드 복사
        std::memcpy(&rtpPacket[RTP_HEADER_SIZE], tsPayload.data(), UDP_DATAGRAM_SIZE);

        // 3. 전송 모드별 송출
        if (session->transportMode == TransportMode::UDP_UNICAST) {
            int sent = sendto(session->udpSocket,
                              reinterpret_cast<const char*>(rtpPacket.data()),
                              static_cast<int>(RTP_DATAGRAM_SIZE),
                              0,
                              reinterpret_cast<sockaddr*>(&session->clientRtpAddr),
                              sizeof(session->clientRtpAddr));
            if (sent == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err != WSAEWOULDBLOCK) {
                    break;
                }
            }
        }
        else if (session->transportMode == TransportMode::TCP_INTERLEAVED) {
            // RFC 2326 Interleaved 프레임: '$' (0x24) + 채널(1B) + 길이(2B Big-Endian) + RTP 패킷
            tcpFrame[0] = '$';
            tcpFrame[1] = session->rtpChannel;
            uint16_t pktLen = htons(static_cast<uint16_t>(RTP_DATAGRAM_SIZE));
            std::memcpy(&tcpFrame[2], &pktLen, sizeof(pktLen));
            std::memcpy(&tcpFrame[4], rtpPacket.data(), RTP_DATAGRAM_SIZE);

            std::lock_guard<std::mutex> lock(session->sendMutex);
            const char* sendPtr = reinterpret_cast<const char*>(tcpFrame.data());
            int remaining = static_cast<int>(tcpFrame.size());
            while (remaining > 0 && m_running && session->sessionRunning) {
                int sent = send(session->tcpSocket, sendPtr, remaining, 0);
                if (sent <= 0) {
                    session->sessionRunning = false;
                    break;
                }
                sendPtr += sent;
                remaining -= sent;
            }
        }
    }
    session->isPlaying=false;
}
