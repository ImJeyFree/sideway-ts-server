#pragma once

#include "Common.h"
#include "TsBroadcaster.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <string>
#include <memory>
#include <vector>
#include <map>

/**
 * @brief RFC 2326 / RFC 3550 / RFC 2250 기반 RTSP 1.0 라이브 스트리밍 서버
 *
 * TCP 포트 8554에서 RTSP 세션을 수신하고, 클라이언트(Android Sideway Player, VLC 등)의
 * OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN 요청을 처리합니다.
 * 미디어는 MPEG-2 TS (RFC 2250 Payload Type 33) 단일 트랙으로 SDP를 제공하며,
 * 클라이언트의 요청에 따라 UDP 유니캐스트 또는 TCP 인터리빙(Interleaved) 방식으로 실시간 스트리밍합니다.
 */
class RtspStreamer {
public:
    RtspStreamer(TsBroadcaster& broadcaster, int port = DEFAULT_RTSP_PORT);
    ~RtspStreamer();

    bool Start();
    void Stop();

    bool IsRunning() const { return m_running.load(); }
    int GetPort() const { return m_port; }
    size_t GetClientCount() const;

    void SetEnabled(bool enabled);
    bool IsEnabled() const { return m_enabled.load(); }

private:
    enum class TransportMode {
        UDP_UNICAST,
        TCP_INTERLEAVED
    };

    struct RtspClientSession {
        SOCKET tcpSocket = INVALID_SOCKET;
        std::string clientIp;
        int clientRtpPort = 0;
        int clientRtcpPort = 0;
        int serverRtpPort = 0;
        SOCKET udpSocket = INVALID_SOCKET;
        sockaddr_in clientRtpAddr{};
        TransportMode transportMode = TransportMode::UDP_UNICAST;
        uint8_t rtpChannel = 0;
        uint8_t rtcpChannel = 1;
        std::string sessionId;
        bool isPlaying = false;
        uint16_t seqNumber = 0;
        uint32_t timestamp = 0;
        uint32_t ssrc = 0x87654321;
        std::shared_ptr<TsSubscriber> subscriber;
        std::thread streamThread;
        std::atomic<bool> sessionRunning{false};
        std::mutex sendMutex;
    };

    void AcceptLoop();
    void HandleClient(SOCKET clientSocket, const std::string& clientIp);
    void StreamLoop(std::shared_ptr<RtspClientSession> session);

    // RTSP 프로토콜 파싱 및 응답 생성
    std::string ProcessRtspRequest(const std::string& request,
                                  std::shared_ptr<RtspClientSession>& session,
                                  bool& shouldClose,
                                  bool& startStream);

    TsBroadcaster& m_broadcaster;
    int m_port;
    SOCKET m_listenSocket = INVALID_SOCKET;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_enabled{false};
    std::thread m_acceptThread;

    mutable std::mutex m_sessionsMutex;
    std::vector<std::shared_ptr<RtspClientSession>> m_sessions;
    std::atomic<uint64_t> m_sessionCounter{1000};
};
