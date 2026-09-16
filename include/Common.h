/**
 * @file Common.h
 * @brief Sideway TS 방송 서버 전역 상수, 네트워크 데이터그램 규격 및 상태 구조체 정의
 * @author Sideway Team
 * @date 2026-09-16
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

/**
 * @brief MPEG-2 Transport Stream (ISO/IEC 13818-1) 기본 규격 상수
 */
constexpr size_t TS_PACKET_SIZE = 188;              ///< 표준 TS 패킷 크기 (188 바이트)
constexpr uint8_t TS_SYNC_BYTE = 0x47;              ///< TS 동기화 바이트 ('G')
constexpr size_t CHUNK_PACKETS = 348;               ///< 네트워크 전송 청크 단위 (348개 패킷 = 약 65KB)
constexpr size_t CHUNK_SIZE = TS_PACKET_SIZE * CHUNK_PACKETS; ///< 1회 전송 청크 바이너리 크기 (65,424 바이트)

/**
 * @brief HTTP 웹 관제 및 스트리밍 서버 기본 설정
 */
constexpr int DEFAULT_SERVER_PORT = 8080;                  ///< HTTP 스트리밍 및 웹 관제 기본 포트
constexpr size_t RING_BUFFER_CAPACITY = 1024 * 1024 * 8;  ///< 브로드캐스트 링버퍼 용량 (8MB)

/**
 * @brief UDP 멀티캐스트/유니캐스트 스트리밍 규격 상수 (MTU 1500 이내 전송)
 */
constexpr size_t UDP_TS_PACKETS = 7;                        ///< 1개 UDP 데이터그램 당 수록되는 TS 패킷 수 (7개)
constexpr size_t UDP_DATAGRAM_SIZE = TS_PACKET_SIZE * UDP_TS_PACKETS; ///< UDP 페이로드 바이너리 크기 (1,316 바이트)
constexpr int DEFAULT_UDP_PORT = 1234;                      ///< UDP 스트리밍 기본 포트
inline const char* DEFAULT_UDP_MULTICAST_ADDR = "239.255.0.1"; ///< UDP 멀티캐스트 기본 IP 주소

/**
 * @brief RTP 스트리밍 규격 상수 (RFC 3550 / RFC 2250)
 * - RTP 고정 헤더: 12 바이트 (V=2, PT=33 MP2T, Sequence, Timestamp, SSRC)
 * - 페이로드: 188바이트 TS 패킷 7개 (1316 바이트)
 * - 전체 데이터그램 크기: 1328 바이트 (이더넷 MTU 1500 이내로 IP 단편화 방지)
 */
constexpr size_t RTP_HEADER_SIZE = 12;                                 ///< RTP 고정 헤더 크기 (12 바이트)
constexpr size_t RTP_DATAGRAM_SIZE = RTP_HEADER_SIZE + UDP_DATAGRAM_SIZE; ///< RTP 전체 데이터그램 크기 (1,328 바이트)
constexpr int DEFAULT_RTP_PORT = 5004;                                 ///< 표준 RTP 기본 포트
constexpr uint8_t RTP_PAYLOAD_TYPE_MP2T = 33;                          ///< RFC 3551 Payload Type 33 (MPEG-2 TS)

/**
 * @struct ServerStats
 * @brief 전체 방송 서버 및 BDA TV 튜너 하드웨어의 실시간 상태 정보 구조체
 */
struct ServerStats {
    bool tunerLocked = false;                          ///< 튜너 주파수 신호 고정(Lock) 성공 여부
    int currentPhysicalChannel = 15;                   ///< 현재 수신 중인 물리 채널 번호 (예: 15)
    std::string currentChannelName = "KBS 1 (CH 15)";  ///< 현재 수신 채널 서비스 명칭
    std::string modulation = "8VSB (ATSC)";            ///< 변조 방식 (8VSB, 256QAM 등)
    size_t activeClients = 0;                          ///< 현재 연결된 동시 HTTP/UDP 클라이언트 수
    uint64_t totalBytesSent = 0;                       ///< 누적 전송 데이터량 (바이트)
    double currentBitrateMbps = 0.0;                   ///< 실시간 방송 송출 비트레이트 (Mbps)
    uint64_t uptimeSeconds = 0;                        ///< 서버 가동 시간 (초)

    // 튜너 하드웨어 상세 정보
    bool hasHardwareTuner = false;                     ///< 실물 BDA TV 튜너 하드웨어 감지 여부
    std::string tunerDeviceName = "장치 미감지 (가상 모드)"; ///< 튜너 장치 명칭
    std::string tunerHardwareId = "N/A";                ///< 튜너 장치 하드웨어 ID
    std::string tunerDriverStatus = "가상 생성기 동작 중"; ///< 드라이버 가동 상태
    std::string supportedStandards = "ATSC (8VSB), Clear QAM (256QAM)"; ///< 지원 방송 표준

    // UDP 듀얼 스트리밍 상태
    bool isUdpEnabled = true;                          ///< UDP 방송 동시 송출 활성화 여부
    std::string udpTarget = "239.255.0.1:1234";        ///< UDP 송출 타겟 (주소:포트)
};
