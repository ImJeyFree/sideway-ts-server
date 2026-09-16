#pragma once

#include "Common.h"
#include "TsBroadcaster.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <thread>
#include <string>
#include <memory>

/**
 * @brief RFC 2250 / RFC 3550 기반 RTP 멀티캐스트 스트리머
 *
 * MPEG-2 TS 패킷(188바이트) 7개를 12바이트 RTP 고정 헤더와 결합하여
 * 1328바이트 데이터그램 단위로 멀티캐스트 전송합니다.
 * 패킷 손실 감지(Sequence Number), 재생 지터 보정(90kHz Timestamp)을 제공합니다.
 * 기존 UdpStreamer와 완전히 독립된 수명주기 및 링버퍼 구독 큐를 가집니다.
 */
class RtpStreamer {
public:
    RtpStreamer(TsBroadcaster& broadcaster,
                const std::string& multicastIp = DEFAULT_UDP_MULTICAST_ADDR,
                int port = DEFAULT_RTP_PORT);
    ~RtpStreamer();

    // 스트리머 스레드 및 소켓 시작/종료
    bool Start();
    void Stop();

    // 송출 활성화/비활성화 제어 (동적 On/Off 토글)
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return m_enabled.load(); }

    // 송출 대상 주소 문자열 반환 (예: "239.255.0.1:5004")
    std::string GetTargetAddress() const;

private:
    // RTP 패킷 조립 및 UDP 멀티캐스트 송출 메인 루프
    void StreamLoop();

    TsBroadcaster& m_broadcaster;
    std::shared_ptr<TsSubscriber> m_subscriber;

    std::string m_multicastIp;
    int m_port;

    SOCKET m_socket = INVALID_SOCKET;
    std::string m_interfaceAddress;
    sockaddr_in m_destAddr{};

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_enabled{false}; // 기본값: 비활성화 (서버 시작 시 정지 상태)
    std::thread m_workerThread;

    // RFC 3550 RTP 헤더 필드
    uint16_t m_sequenceNumber{0};       // 16비트 패킷 시퀀스 번호 (단조 증가)
    uint32_t m_timestamp{0};            // 32비트 90kHz MPEG 타임스탬프
    uint32_t m_ssrc{0x12345678};        // 32비트 동기화 소스 식별자 (SSRC)
};
