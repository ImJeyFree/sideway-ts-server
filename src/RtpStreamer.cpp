#include "RtpStreamer.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstring>

RtpStreamer::RtpStreamer(TsBroadcaster& broadcaster, const std::string& multicastIp, int port)
    : m_broadcaster(broadcaster), m_multicastIp(multicastIp), m_port(port) {
}

RtpStreamer::~RtpStreamer() {
    Stop();
}

bool RtpStreamer::Start() {
    if (m_running) return true;

    // UDP 소켓 생성 (IPv4, 비연결형 데이터그램)
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "[RtpStreamer] 소켓 생성 실패: " << WSAGetLastError() << std::endl;
        return false;
    }

    // 소켓 옵션 설정 헬퍼 람다
    auto setOption = [this](int level, int option, const void* value, int size) {
        if (setsockopt(m_socket, level, option, static_cast<const char*>(value), size) == 0) return true;
        std::cerr << "[RtpStreamer] 소켓 옵션 실패: " << WSAGetLastError() << std::endl;
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    };

    // 다중 네트워크 인터페이스(NIC) 환경 지원: TS_MULTICAST_INTERFACE 환경변수가 있으면 해당 IP로 바인딩
    if (const char* address = std::getenv("TS_MULTICAST_INTERFACE")) {
        in_addr local{};
        if (inet_pton(AF_INET, address, &local) != 1) {
            std::cerr << "[RtpStreamer] TS_MULTICAST_INTERFACE IPv4 주소 오류" << std::endl;
            closesocket(m_socket); m_socket = INVALID_SOCKET; return false;
        }
        if (!setOption(IPPROTO_IP, IP_MULTICAST_IF, &local, sizeof(local))) return false;
        m_interfaceAddress = address;
    }

    // 멀티캐스트 TTL 1 설정 (로컬 서브넷 라우팅 범위로 제한하여 외부 유출 방지)
    DWORD ttl = 1;
    if (!setOption(IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl))) return false;

    // 멀티캐스트 로컬 루프백 활성화 (서버가 실행 중인 PC의 로컬 플레이어/VLC에서도 수신 허용)
    BOOL loopOpt = TRUE;
    if (!setOption(IPPROTO_IP, IP_MULTICAST_LOOP, &loopOpt, sizeof(loopOpt))) return false;

    // 브로드캐스트 허용 플래그
    BOOL broadcastOpt = TRUE;
    if (!setOption(SOL_SOCKET, SO_BROADCAST, &broadcastOpt, sizeof(broadcastOpt))) return false;

    // 송신 버퍼를 512KB로 확장하여 19Mbps 방송 신호 순간 버스트 시 패킷 유실 방지
    int sndbuf = 1024 * 512;
    if (!setOption(SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf))) return false;

    // 목적지 주소 설정 (멀티캐스트 IP 및 포트 5004)
    std::memset(&m_destAddr, 0, sizeof(m_destAddr));
    m_destAddr.sin_family = AF_INET;
    m_destAddr.sin_port = htons(static_cast<u_short>(m_port));
    if (inet_pton(AF_INET, m_multicastIp.c_str(), &m_destAddr.sin_addr) != 1) {
        closesocket(m_socket); m_socket = INVALID_SOCKET; return false;
    }

    // 1:N TsBroadcaster로부터 RTP 전용 독립 구독자 큐 생성 (2MB 링버퍼)
    m_subscriber = m_broadcaster.CreateSubscriber(1024 * 1024 * 2);

    m_running = true;
    m_workerThread = std::thread(&RtpStreamer::StreamLoop, this);

    std::cout << "[RtpStreamer] RTP 스트리머 가동 -> rtp://"
              << m_multicastIp << ":" << m_port << " (RFC 2250 MP2T 단위 송출)" << std::endl;
    return true;
}

void RtpStreamer::Stop() {
    if (!m_running) return;
    m_running = false;

    // 구독자 해제 및 링버퍼 정리
    if (m_subscriber) {
        m_broadcaster.RemoveSubscriber(m_subscriber);
    }

    // 송출 워커 스레드 종료 대기
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    m_subscriber.reset();
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    std::cout << "[RtpStreamer] RTP 스트리머 정지" << std::endl;
}

void RtpStreamer::SetEnabled(bool enabled) {
    m_enabled = enabled;
    std::cout << "[RtpStreamer] RTP 송출 상태 변경: " << (enabled ? "ON" : "OFF") << std::endl;
}

std::string RtpStreamer::GetTargetAddress() const {
    return m_multicastIp + ":" + std::to_string(m_port);
}

void RtpStreamer::StreamLoop() {
    // 12바이트 RFC 3550 RTP 고정 헤더 + 1316바이트 TS 페이로드 = 총 1328바이트 (MTU 1500 이내)
    std::vector<uint8_t> rtpPacket(RTP_DATAGRAM_SIZE);
    std::vector<uint8_t> tsPayload(UDP_DATAGRAM_SIZE);

    while (m_running) {
        // 브로드캐스터 구독 큐에서 1316바이트(188B TS * 7개) 단위로 TS 데이터 수신
        if (!m_subscriber || !m_subscriber->PopData(tsPayload.data(), UDP_DATAGRAM_SIZE)) {
            continue;
        }

        // 스트리밍이 활성화(ON)된 상태일 때만 네트워크로 송출 (비활성화 시 큐 소비만 진행하여 지연 방지)
        if (m_enabled.load()) {
            // 1. RFC 3550 RTP 헤더 구성 (12 바이트, 네트워크 빅엔디안 바이트 오더)
            // Byte 0: V=2 (버전 2), P=0 (패딩 없음), X=0 (확장 헤더 없음), CC=0 (CSRC 개수 0)
            rtpPacket[0] = 0x80;
            // Byte 1: M=0 (마커 비트 0), Payload Type = 33 (RFC 3551 MP2T: MPEG-2 TS)
            rtpPacket[1] = RTP_PAYLOAD_TYPE_MP2T & 0x7F;

            // Byte 2~3: Sequence Number (16비트 순차 증가로 수신측에서 패킷 유실 감지 가능)
            uint16_t seq = htons(m_sequenceNumber++);
            std::memcpy(&rtpPacket[2], &seq, sizeof(seq));

            // Byte 4~7: Timestamp (32비트, MPEG 90kHz 클럭 기준 7 TS 패킷당 약 49 틱 증가)
            // 수신측(VLC 등)에서 지터 버퍼링 및 프레임 디코딩 동기화에 사용
            uint32_t ts = htonl(m_timestamp);
            std::memcpy(&rtpPacket[4], &ts, sizeof(ts));
            m_timestamp += 49;

            // Byte 8~11: SSRC (동기화 소스 식별자, 멀티캐스트 세션 내 고유 발신자 구분)
            uint32_t ssrc = htonl(m_ssrc);
            std::memcpy(&rtpPacket[8], &ssrc, sizeof(ssrc));

            // 2. TS 페이로드 복사 (1316 바이트)
            std::memcpy(&rtpPacket[RTP_HEADER_SIZE], tsPayload.data(), UDP_DATAGRAM_SIZE);

            // 3. UDP 멀티캐스트 전송 (총 1328 바이트)
            int sent = sendto(m_socket,
                   reinterpret_cast<const char*>(rtpPacket.data()),
                   static_cast<int>(RTP_DATAGRAM_SIZE),
                   0,
                   reinterpret_cast<sockaddr*>(&m_destAddr),
                   sizeof(m_destAddr));
            if (sent != static_cast<int>(RTP_DATAGRAM_SIZE)) {
                std::cerr << "[RtpStreamer] 송출 실패: " << WSAGetLastError() << std::endl;
                m_enabled = false;
            }
        }
    }
}
