/**
 * @file HttpStreamer.h
 * @brief HTTP 기반 MPEG-TS 스트리밍, M3U 플레이리스트 및 REST API 웹 서버
 * @author Sideway Team
 * @date 2026-09-16
 */

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

/**
 * @namespace HttpWire
 * @brief HTTP 네트워크 전송 헬퍼 함수
 */
namespace HttpWire {
    /** @brief JSON 문자열 이스케이프 처리 */
    std::string JsonEscape(const std::string& value);

    /** @brief 데이터를 소켓으로 완전히 전송될 때까지 루프 송출 */
    bool SendAll(const char* data, size_t size, const std::function<int(const char*, int)>& sender);
}

class UdpStreamer;
class RtpStreamer;
class RtspStreamer;
class ScanClient;
class QualityStore;

/**
 * @class HttpStreamer
 * @brief HTTP 실시간 MPEG-TS 방송 송출, M3U 플레이리스트 생성 및 웹 관제 API 서버
 */
class HttpStreamer {
public:
    /**
     * @brief HttpStreamer 생성자
     * @param broadcaster TS 브로드캐스터 참조
     * @param tuner 튜너 클라이언트 참조
     * @param pUdpStreamer UDP 멀티캐스트 스트리머 포인터
     * @param port HTTP 서버 포트 (기본: 8080)
     */
    HttpStreamer(TsBroadcaster& broadcaster, TunerClient& tuner, UdpStreamer* pUdpStreamer = nullptr, int port = DEFAULT_SERVER_PORT);

    /**
     * @brief HttpStreamer 소멸자 (서버 중지 및 소켓 정리)
     */
    ~HttpStreamer();

    /**
     * @brief HTTP 리슨 소켓 바인딩 및 Accept 스레드 시작
     * @return 성공 시 true, 실패 시 false
     */
    bool Start();

    /**
     * @brief HTTP 서버 정지 및 모든 연결 클라이언트 소켓 닫기
     */
    void Stop();

    /** @brief 스캐너 클라이언트 등록 */
    void SetScanner(ScanClient* scanner) { m_scanner = scanner; }

    /** @brief RTP 멀티캐스트 스트리머 등록 (웹 대시보드 제어용) */
    void SetRtpStreamer(RtpStreamer* pRtpStreamer) { m_pRtpStreamer = pRtpStreamer; }

    /** @brief RTSP 스트리머 등록 (웹 대시보드 제어용) */
    void SetRtspStreamer(RtspStreamer* pRtspStreamer) { m_pRtspStreamer = pRtspStreamer; }

    /** @brief 스트리밍 및 디코딩 품질 설정 저장소 등록 */
    void SetQualityStore(QualityStore* pQualityStore) { m_pQualityStore = pQualityStore; }

    /** @brief 서버 포트 번호 반환 */
    int GetPort() const { return m_port; }

    /** @brief 현재 연결된 동시 HTTP 스트리밍 클라이언트 수 반환 */
    size_t GetActiveClients() const { return m_activeClients; }

    /** @brief 실시간 평균 전송 비트레이트 (Mbps) 반환 */
    double GetCurrentBitrateMbps() const;

    /** @brief 서버 가동 시간 (초) 반환 */
    uint64_t GetUptimeSeconds() const;

private:
    /** @brief 신규 HTTP 클라이언트 연결 수락(Accept) 스레드 루프 */
    void AcceptLoop();

    /** @brief 개별 클라이언트 소켓의 HTTP 요청 수신 및 라우팅 처리 스레드 */
    void HandleClient(SOCKET clientSocket);

    /** @brief HTTP 헤더 및 응답 본문 전송 유틸리티 */
    void SendHttpResponse(SOCKET clientSocket, const std::string& contentType, const std::string& body);

    /** @brief GET /api/status (서버 상태 및 튜너 신호 정보 JSON) API 핸들러 */
    void HandleStatusApi(SOCKET clientSocket);

    /** @brief POST /api/tune (채널 변경 요청) API 핸들러 */
    void HandleTuneApi(SOCKET clientSocket, const std::string& request);

    /** @brief POST /api/udp/toggle (UDP 송출 On/Off) API 핸들러 */
    void HandleUdpToggleApi(SOCKET clientSocket, const std::string& request);

    /** @brief POST /api/rtp/toggle (RTP 송출 On/Off) API 핸들러 */
    void HandleRtpToggleApi(SOCKET clientSocket, const std::string& request);

    /** @brief POST /api/rtsp/toggle (RTSP 송출 On/Off) API 핸들러 */
    void HandleRtspToggleApi(SOCKET clientSocket, const std::string& request);

    /** @brief GET /api/config/quality (품질 설정 조회) API 핸들러 */
    void HandleGetQualityApi(SOCKET clientSocket);

    /** @brief POST /api/config/quality (품질 설정 저장) API 핸들러 */
    void HandlePostQualityApi(SOCKET clientSocket, const std::string& body);

    /** @brief GET /stream (MPEG-TS 실시간 바이너리 스트리밍 송출 루프) */
    void StreamTsToClient(SOCKET clientSocket, int channel, bool isClearQam = false);

    TsBroadcaster& m_broadcaster;               ///< TS 브로드캐스터 참조
    TunerClient& m_tuner;                       ///< 튜너 클라이언트 참조
    UdpStreamer* m_pUdpStreamer = nullptr;      ///< UDP 스트리머 포인터
    RtpStreamer* m_pRtpStreamer = nullptr;      ///< RTP 스트리머 포인터
    RtspStreamer* m_pRtspStreamer = nullptr;    ///< RTSP 스트리머 포인터
    ScanClient* m_scanner = nullptr;            ///< 스캐너 클라이언트 포인터
    QualityStore* m_pQualityStore = nullptr;    ///< 품질 설정 저장소 포인터
    int m_port;                                 ///< HTTP 바인딩 포트

    SOCKET m_listenSocket = INVALID_SOCKET;     ///< HTTP 리슨 소켓 핸들
    std::atomic<bool> m_running{false};          ///< 서버 가동 상태 플래그
    std::atomic<bool> m_isBroadcasting{false};   ///< 스트리밍 활성화 플래그
    std::atomic<size_t> m_activeClients{0};      ///< 동시 접속 클라이언트 수
    std::atomic<uint64_t> m_totalBytesSent{0};   ///< 누적 전송 바이트 수

    std::chrono::steady_clock::time_point m_startTime; ///< 서버 시작 시각
    std::thread m_acceptThread;                  ///< Accept 전용 스레드
    std::mutex m_clientsMutex;                  ///< 클라이언트 동기화 뮤텍스
    std::condition_variable m_clientsCv;         ///< 동기화 조건 변수
    std::set<SOCKET> m_clientSockets;           ///< 활성 클라이언트 소켓 집합
};
