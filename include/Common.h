#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

// MPEG-2 Transport Stream 규격 상수
constexpr size_t TS_PACKET_SIZE = 188;              // 표준 TS 패킷 크기 (188 바이트)
constexpr uint8_t TS_SYNC_BYTE = 0x47;              // TS 동기화 바이트 ('G')
constexpr size_t CHUNK_PACKETS = 348;               // 네트워크 전송 단위 (약 65KB, 188 * 348)
constexpr size_t CHUNK_SIZE = TS_PACKET_SIZE * CHUNK_PACKETS;

// 기본 네트워크 서버 설정
constexpr int DEFAULT_SERVER_PORT = 8080;
constexpr size_t RING_BUFFER_CAPACITY = 1024 * 1024 * 8; // 8MB 브로드캐스트 링버퍼

// UDP 스트리밍 규격 상수 (MTU 1500 내 188바이트 TS 패킷 7개 = 1316바이트)
constexpr size_t UDP_TS_PACKETS = 7;
constexpr size_t UDP_DATAGRAM_SIZE = TS_PACKET_SIZE * UDP_TS_PACKETS; // 1316 바이트
constexpr int DEFAULT_UDP_PORT = 1234;
inline const char* DEFAULT_UDP_MULTICAST_ADDR = "239.255.0.1";

// 서버 및 튜너 상태 구조체
struct ServerStats {
    bool tunerLocked = false;
    int currentPhysicalChannel = 15;
    std::string currentChannelName = "KBS 1 (CH 15)";
    std::string modulation = "8VSB (ATSC)";
    size_t activeClients = 0;
    uint64_t totalBytesSent = 0;
    double currentBitrateMbps = 0.0;
    uint64_t uptimeSeconds = 0;
    // 튜너 하드웨어 상세 정보
    bool hasHardwareTuner = false;
    std::string tunerDeviceName = "장치 미감지 (가상 모드)";
    std::string tunerHardwareId = "N/A";
    std::string tunerDriverStatus = "가상 생성기 동작 중";
    std::string supportedStandards = "ATSC (8VSB), Clear QAM (256QAM)";

    // UDP 듀얼 스트리밍 상태
    bool isUdpEnabled = true;
    std::string udpTarget = "239.255.0.1:1234";
};
