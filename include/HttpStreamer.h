#pragma once

#include "Common.h"
#include "TsBroadcaster.h"
#include "TunerClient.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <set>
#include <functional>

// 전송 함수 주입으로 부분 전송/연결 종료를 회귀 검증한다.
namespace HttpWire {
std::string JsonEscape(const std::string& value);
bool SendAll(const char* data, size_t size, const std::function<int(const char*, int)>& sender);
}

class UdpStreamer;
class RtpStreamer;
class ScanClient;

class HttpStreamer {
public:
    HttpStreamer(TsBroadcaster& broadcaster, TunerClient& tuner, UdpStreamer* pUdpStreamer = nullptr, int port = DEFAULT_SERVER_PORT);
    ~HttpStreamer();

    bool Start();
    void Stop();
    void SetScanner(ScanClient* scanner) { m_scanner=scanner; }
    // RTP 멀티캐스트 스트리머 주입 (웹 대시보드 상태 노출 및 On/Off 토글용)
    void SetRtpStreamer(RtpStreamer* pRtpStreamer) { m_pRtpStreamer = pRtpStreamer; }

    int GetPort() const { return m_port; }
    size_t GetActiveClients() const { return m_activeClients; }
    double GetCurrentBitrateMbps() const;
    uint64_t GetUptimeSeconds() const;

private:
    void AcceptLoop();
    void HandleClient(SOCKET clientSocket);

    void SendHttpResponse(SOCKET clientSocket, const std::string& contentType, const std::string& body);
    void HandleStatusApi(SOCKET clientSocket);
    void HandleTuneApi(SOCKET clientSocket, const std::string& request);
    void HandleUdpToggleApi(SOCKET clientSocket, const std::string& request);
    void HandleRtpToggleApi(SOCKET clientSocket, const std::string& request); // GET /api/rtp/toggle 핸들러
    void StreamTsToClient(SOCKET clientSocket, int channel, bool isClearQam = false);

    TsBroadcaster& m_broadcaster;
    TunerClient& m_tuner;
    UdpStreamer* m_pUdpStreamer = nullptr;
    RtpStreamer* m_pRtpStreamer = nullptr;
    ScanClient* m_scanner = nullptr;
    int m_port;

    SOCKET m_listenSocket = INVALID_SOCKET;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_isBroadcasting{false};
    std::atomic<size_t> m_activeClients{0};
    std::atomic<uint64_t> m_totalBytesSent{0};

    std::chrono::steady_clock::time_point m_startTime;
    std::thread m_acceptThread;
    std::mutex m_clientsMutex;
    std::condition_variable m_clientsCv;
    std::set<SOCKET> m_clientSockets;
};
