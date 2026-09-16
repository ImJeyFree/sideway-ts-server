#include "RtspStreamer.h"
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
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

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

    m_running = true;
    m_acceptThread = std::thread(&RtspStreamer::AcceptLoop, this);

    std::cout << "[RtspStreamer] RTSP 스트리밍 서버 가동 -> rtsp://0.0.0.0:" << m_port << "/live" << std::endl;
    return true;
}

void RtspStreamer::Stop() {
    if (!m_running) return;
    m_running = false;

    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }

    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }

    // 모든 활성 세션 종료
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        for (auto& session : m_sessions) {
            session->sessionRunning = false;
            if (session->tcpSocket != INVALID_SOCKET) {
                closesocket(session->tcpSocket);
                session->tcpSocket = INVALID_SOCKET;
            }
            if (session->udpSocket != INVALID_SOCKET) {
                closesocket(session->udpSocket);
                session->udpSocket = INVALID_SOCKET;
            }
            if (session->streamThread.joinable()) {
                session->streamThread.join();
            }
            if (session->subscriber) {
                m_broadcaster.RemoveSubscriber(session->subscriber);
                session->subscriber.reset();
            }
        }
        m_sessions.clear();
    }

    std::cout << "[RtspStreamer] RTSP 스트리밍 서버 정지" << std::endl;
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
    m_enabled = enabled;
    std::cout << "[RtspStreamer] RTSP 송출 상태 변경: " << (enabled ? "ON" : "OFF") << std::endl;
    if (!enabled) {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        for (auto& session : m_sessions) {
            session->isPlaying = false;
            session->sessionRunning = false;
            if (session->tcpSocket != INVALID_SOCKET) {
                shutdown(session->tcpSocket, SD_BOTH);
            }
        }
    }
}

void RtspStreamer::AcceptLoop() {
    while (m_running) {
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(m_listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
        if (clientSocket == INVALID_SOCKET) {
            if (!m_running) break;
            continue;
        }

        char ipBuf[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipBuf, sizeof(ipBuf));
        std::string clientIp = ipBuf;

        // 클라이언트별 독립 세션 스레드 실행
        std::thread([this, clientSocket, clientIp]() {
            HandleClient(clientSocket, clientIp);
        }).detach();
    }
}

void RtspStreamer::HandleClient(SOCKET clientSocket, const std::string& clientIp) {
    auto session = std::make_shared<RtspClientSession>();
    session->tcpSocket = clientSocket;
    session->clientIp = clientIp;
    session->sessionId = std::to_string(++m_sessionCounter);

    // 수신 타임아웃 30초 설정
    DWORD timeout = 30000;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    // Nagle 알고리즘 비활성화 (지연 최소화) 및 송신 버퍼 2MB 확장
    int nodelay = 1;
    setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
    int sndbuf = 1024 * 1024 * 2;
    setsockopt(clientSocket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&sndbuf), sizeof(sndbuf));

    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        m_sessions.push_back(session);
    }

    std::cout << "[RtspStreamer] 클라이언트 접속 (" << clientIp << ", 세션 " << session->sessionId << ")" << std::endl;

    std::string receiveBuffer;
    char tempBuf[4096];
    bool shouldClose = false;

    while (m_running && !shouldClose) {
        int bytesRead = recv(clientSocket, tempBuf, sizeof(tempBuf) - 1, 0);
        if (bytesRead <= 0) {
            break;
        }
        tempBuf[bytesRead] = '\0';
        receiveBuffer.append(tempBuf, bytesRead);

        // RTSP 헤더 구분자 "\r\n\r\n" 감지
        size_t headerEnd = receiveBuffer.find("\r\n\r\n");
        while (headerEnd != std::string::npos) {
            std::string request = receiveBuffer.substr(0, headerEnd + 4);
            receiveBuffer.erase(0, headerEnd + 4);

            bool startStream = false;
            std::string response = ProcessRtspRequest(request, session, shouldClose, startStream);
            if (!response.empty()) {
                std::lock_guard<std::mutex> lock(session->sendMutex);
                int sent = send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
                if (sent <= 0) {
                    shouldClose = true;
                    break;
                }
            }

            // PLAY 200 OK 응답이 온전히 전송된 이후에 스트리밍 스레드 가동
            if (startStream && !session->isPlaying) {
                session->isPlaying = true;
                session->subscriber = m_broadcaster.CreateSubscriber(1024 * 1024 * 4); // 4MB 링버퍼
                session->sessionRunning = true;
                session->streamThread = std::thread(&RtspStreamer::StreamLoop, this, session);
            }

            if (shouldClose) break;
            headerEnd = receiveBuffer.find("\r\n\r\n");
        }
    }

    // 세션 종료 및 정리
    session->sessionRunning = false;
    if (session->udpSocket != INVALID_SOCKET) {
        closesocket(session->udpSocket);
        session->udpSocket = INVALID_SOCKET;
    }
    if (session->tcpSocket != INVALID_SOCKET) {
        closesocket(session->tcpSocket);
        session->tcpSocket = INVALID_SOCKET;
    }
    if (session->streamThread.joinable()) {
        session->streamThread.join();
    }
    if (session->subscriber) {
        m_broadcaster.RemoveSubscriber(session->subscriber);
        session->subscriber.reset();
    }

    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        auto it = std::find(m_sessions.begin(), m_sessions.end(), session);
        if (it != m_sessions.end()) {
            m_sessions.erase(it);
        }
    }

    std::cout << "[RtspStreamer] 클라이언트 접속 해제 (" << clientIp << ", 세션 " << session->sessionId << ")" << std::endl;
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
        std::string responseTransport;

        // TCP Interleaved 모드 감지 (예: RTP/AVP/TCP;unicast;interleaved=0-1)
        if (transport.find("TCP") != std::string::npos || transport.find("interleaved") != std::string::npos) {
            session->transportMode = TransportMode::TCP_INTERLEAVED;
            session->rtpChannel = 0;
            session->rtcpChannel = 1;
            responseTransport = "RTP/AVP/TCP;unicast;interleaved=0-1;ssrc=87654321";
        }
        else {
            // UDP Unicast 모드 (예: RTP/AVP;unicast;client_port=50000-50001)
            session->transportMode = TransportMode::UDP_UNICAST;
            int clientRtp = 0, clientRtcp = 0;
            size_t cpPos = transport.find("client_port=");
            if (cpPos != std::string::npos) {
                sscanf_s(transport.c_str() + cpPos + 12, "%d-%d", &clientRtp, &clientRtcp);
            }
            if (clientRtp == 0) clientRtp = 50000;
            if (clientRtcp == 0) clientRtcp = clientRtp + 1;

            session->clientRtpPort = clientRtp;
            session->clientRtcpPort = clientRtcp;
            session->serverRtpPort = 60000;

            // 클라이언트 대상 UDP 소켓 준비
            if (session->udpSocket != INVALID_SOCKET) {
                closesocket(session->udpSocket);
            }
            session->udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            int sndbuf = 1024 * 512;
            setsockopt(session->udpSocket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&sndbuf), sizeof(sndbuf));

            std::memset(&session->clientRtpAddr, 0, sizeof(session->clientRtpAddr));
            session->clientRtpAddr.sin_family = AF_INET;
            session->clientRtpAddr.sin_port = htons(static_cast<u_short>(session->clientRtpPort));
            inet_pton(AF_INET, session->clientIp.c_str(), &session->clientRtpAddr.sin_addr);

            std::ostringstream ts;
            ts << "RTP/AVP;unicast;client_port=" << clientRtp << "-" << clientRtcp
               << ";server_port=60000-60001;ssrc=87654321";
            responseTransport = ts.str();
        }

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
                 << "RTP-Info: url=" << url << "/track0;seq=0;rtptime=0\r\n\r\n";
    }
    else if (method == "PAUSE") {
        session->isPlaying = false;
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

    auto startTime = std::chrono::steady_clock::now();

    while (m_running && m_enabled.load() && session->sessionRunning && session->isPlaying) {
        if (!session->subscriber || !session->subscriber->PopData(tsPayload.data(), UDP_DATAGRAM_SIZE)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // 1. RFC 3550 RTP 헤더 구성
        rtpPacket[0] = 0x80; // V=2, P=0, X=0, CC=0
        rtpPacket[1] = RTP_PAYLOAD_TYPE_MP2T & 0x7F; // PT=33

        uint16_t seq = htons(session->seqNumber++);
        std::memcpy(&rtpPacket[2], &seq, sizeof(seq));

        // 실시간 90kHz MPEG 클럭 타임스탬프 계산 (지터 및 누적 오차 원천 차단)
        auto now = std::chrono::steady_clock::now();
        auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count();
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
}
