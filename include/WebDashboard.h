#pragma once

#include <string>

namespace WebDashboard {

// 임베디드 웹 대시보드 HTML/CSS/JS (Dual Streaming 지원)
inline const char* GetDashboardHtml() {
    return R"rawhtml(<!DOCTYPE html>
<html lang="ko">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Sideway TS Streaming Server</title>
    <link href="https://fonts.googleapis.com/css2?family=Pretendard:wght@400;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-color: #0f172a;
            --card-bg: rgba(30, 41, 59, 0.7);
            --card-border: rgba(255, 255, 255, 0.08);
            --accent-primary: #38bdf8;
            --accent-green: #22c55e;
            --accent-purple: #c084fc;
            --accent-red: #ef4444;
            --text-primary: #f8fafc;
            --text-secondary: #94a3b8;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: 'Pretendard', -apple-system, BlinkMacSystemFont, sans-serif;
        }

        body {
            background: radial-gradient(circle at top right, #1e293b, var(--bg-color));
            color: var(--text-primary);
            min-height: 100vh;
            padding: 2rem;
            display: flex;
            flex-direction: column;
            align-items: center;
        }

        .container {
            width: 100%;
            max-width: 960px;
        }

        header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 2rem;
            padding-bottom: 1rem;
            border-bottom: 1px solid var(--card-border);
        }

        .logo-area {
            display: flex;
            align-items: center;
            gap: 0.75rem;
        }

        .logo-badge {
            background: linear-gradient(135deg, #0284c7, #38bdf8);
            padding: 0.5rem 0.75rem;
            border-radius: 12px;
            font-size: 1.25rem;
            box-shadow: 0 4px 14px rgba(56, 189, 248, 0.3);
        }

        h1 {
            font-size: 1.5rem;
            font-weight: 700;
            background: linear-gradient(to right, #fff, #94a3b8);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }

        .badge-online {
            display: inline-flex;
            align-items: center;
            gap: 0.4rem;
            background: rgba(34, 197, 94, 0.15);
            color: var(--accent-green);
            padding: 0.35rem 0.75rem;
            border-radius: 9999px;
            font-size: 0.85rem;
            font-weight: 600;
            border: 1px solid rgba(34, 197, 94, 0.3);
        }

        .dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: var(--accent-green);
            animation: pulse 2s infinite;
        }

        @keyframes pulse {
            0% { transform: scale(0.95); box-shadow: 0 0 0 0 rgba(34, 197, 94, 0.7); }
            70% { transform: scale(1); box-shadow: 0 0 0 8px rgba(34, 197, 94, 0); }
            100% { transform: scale(0.95); box-shadow: 0 0 0 0 rgba(34, 197, 94, 0); }
        }

        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
            gap: 1.25rem;
            margin-bottom: 2rem;
        }

        .card {
            background: var(--card-bg);
            border: 1px solid var(--card-border);
            border-radius: 16px;
            padding: 1.25rem;
            backdrop-filter: blur(12px);
            transition: transform 0.2s ease, border-color 0.2s ease;
        }

        .card:hover {
            transform: translateY(-2px);
            border-color: rgba(56, 189, 248, 0.3);
        }

        .card-label {
            color: var(--text-secondary);
            font-size: 0.85rem;
            margin-bottom: 0.5rem;
        }

        .card-value {
            font-size: 1.5rem;
            font-weight: 700;
            color: var(--text-primary);
        }

        .card-sub {
            font-size: 0.75rem;
            color: var(--accent-primary);
            margin-top: 0.35rem;
        }

        .section-title {
            font-size: 1.15rem;
            font-weight: 600;
            margin-bottom: 1rem;
            display: flex;
            align-items: center;
            gap: 0.5rem;
        }

        .stream-card {
            background: rgba(15, 23, 42, 0.6);
            border: 1px solid var(--card-border);
            border-radius: 14px;
            padding: 1.1rem;
            margin-bottom: 1.25rem;
            display: flex;
            flex-direction: column;
            gap: 0.75rem;
        }

        .stream-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .stream-type-tag {
            font-size: 0.8rem;
            font-weight: 700;
            padding: 0.25rem 0.6rem;
            border-radius: 6px;
        }

        .tag-http {
            background: rgba(56, 189, 248, 0.2);
            color: var(--accent-primary);
            border: 1px solid rgba(56, 189, 248, 0.3);
        }

        .tag-udp {
            background: rgba(192, 132, 252, 0.2);
            color: var(--accent-purple);
            border: 1px solid rgba(192, 132, 252, 0.3);
        }

        .stream-url-box {
            display: flex;
            justify-content: space-between;
            align-items: center;
            gap: 1rem;
            flex-wrap: wrap;
            background: rgba(0, 0, 0, 0.3);
            border-radius: 8px;
            padding: 0.6rem 0.85rem;
        }

        .url-text {
            font-family: monospace;
            color: var(--text-primary);
            font-size: 0.95rem;
            word-break: break-all;
        }

        .btn-group {
            display: flex;
            gap: 0.5rem;
        }

        .action-btn {
            background: #0284c7;
            color: #fff;
            border: none;
            padding: 0.45rem 0.85rem;
            border-radius: 8px;
            cursor: pointer;
            font-weight: 600;
            font-size: 0.85rem;
            transition: background 0.2s;
        }

        .action-btn:disabled {
            opacity: 0.45;
            cursor: not-allowed;
        }
        .action-btn:hover {
            background: #0369a1;
        }

        .action-btn.toggle-btn {
            background: #475569;
        }

        .action-btn.toggle-btn.on {
            background: #16a34a;
        }

        .channels-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(160px, 1fr));
            gap: 0.75rem;
            margin-bottom: 2rem;
        }

        .channel-btn {
            background: rgba(30, 41, 59, 0.8);
            border: 1px solid var(--card-border);
            color: var(--text-primary);
            padding: 0.85rem;
            border-radius: 12px;
            cursor: pointer;
            text-align: left;
            transition: all 0.2s;
        }

        .channel-btn:hover {
            background: rgba(56, 189, 248, 0.15);
            border-color: var(--accent-primary);
        }

        .channel-btn.active {
            background: linear-gradient(135deg, rgba(2, 132, 199, 0.4), rgba(56, 189, 248, 0.2));
            border-color: var(--accent-primary);
            box-shadow: 0 0 12px rgba(56, 189, 248, 0.2);
        }

        .ch-num {
            font-size: 0.8rem;
            color: var(--text-secondary);
        }

        .ch-name {
            font-size: 1rem;
            font-weight: 600;
            margin-top: 0.2rem;
        }

        footer {
            margin-top: auto;
            text-align: center;
            color: var(--text-secondary);
            font-size: 0.85rem;
            padding-top: 2rem;
        }

        /* 튜너 하드웨어 상세 스펙 카드 */
        .hardware-card {
            background: linear-gradient(135deg, rgba(15, 23, 42, 0.85), rgba(30, 41, 59, 0.75));
            border: 1px solid rgba(56, 189, 248, 0.3);
            border-radius: 16px;
            padding: 1.25rem 1.5rem;
            margin-bottom: 1.75rem;
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.25);
        }

        .hw-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 1rem;
            flex-wrap: wrap;
            gap: 0.5rem;
        }

        .hw-title {
            font-size: 1.05rem;
            font-weight: 700;
            display: flex;
            align-items: center;
            gap: 0.5rem;
            color: #f8fafc;
        }

        .hw-badge {
            font-size: 0.8rem;
            font-weight: 700;
            padding: 0.3rem 0.75rem;
            border-radius: 9999px;
            border: 1px solid rgba(34, 197, 94, 0.3);
            background: rgba(34, 197, 94, 0.15);
            color: var(--accent-green);
        }

        .hw-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(210px, 1fr));
            gap: 1rem;
        }

        .hw-item {
            background: rgba(0, 0, 0, 0.25);
            border: 1px solid rgba(255, 255, 255, 0.05);
            border-radius: 10px;
            padding: 0.75rem 1rem;
        }

        .hw-label {
            font-size: 0.75rem;
            color: var(--text-secondary);
            margin-bottom: 0.25rem;
        }

        .hw-value {
            font-size: 0.95rem;
            font-weight: 600;
            color: var(--text-primary);
            word-break: break-all;
        }

        .guide-card {
            background: rgba(15, 23, 42, 0.6);
            border: 1px dashed rgba(56, 189, 248, 0.4);
            border-radius: 14px;
            padding: 1rem 1.25rem;
            margin-top: 1rem;
            margin-bottom: 2rem;
            font-size: 0.9rem;
            color: var(--text-secondary);
            line-height: 1.5;
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <div class="logo-area">
                <div class="logo-badge">📡</div>
                <div>
                    <h1>Sideway TS Dual Server</h1>
                    <div style="font-size: 0.8rem; color: var(--text-secondary);">1:N Multicast & Dual Streaming Hub</div>
                </div>
            </div>
            <div class="badge-online">
                <div class="dot"></div>
                <span>서버 정상 가동 중</span>
            </div>
        </header>

        <!-- 1. TV 튜너 하드웨어 장치 정보 카드 -->
        <div class="hardware-card">
            <div class="hw-header">
                <div class="hw-title">🖥️ TV 튜너 하드웨어 장치 스펙 및 연결 상태</div>
                <div id="tunerBadge" class="hw-badge">🟢 하드웨어 튜너 연결됨 (ONLINE)</div>
            </div>
            <div class="hw-grid">
                <div class="hw-item">
                    <div class="hw-label">제품명 / 장치 모델</div>
                    <div class="hw-value" id="tunerProductName">Bluebird, WDM TsCapture (XC5000)</div>
                </div>
                <div class="hw-item">
                    <div class="hw-label">하드웨어 식별자 (PnP ID)</div>
                    <div class="hw-value" id="tunerHwId">USB\VID_0FE9&PID_D620</div>
                </div>
                <div class="hw-item">
                    <div class="hw-label">드라이버 동작 상태</div>
                    <div class="hw-value" id="tunerDriverText" style="color: var(--accent-green);">Windows BDA 드라이버 정상 (OK)</div>
                </div>
                <div class="hw-item">
                    <div class="hw-label">지원 방송 표준 규격</div>
                    <div class="hw-value" id="tunerStandards">ATSC (8VSB 지상파) / Clear QAM (256QAM)</div>
                </div>
            </div>
        </div>

        <!-- 2. 실시간 서버 핵심 상태 지표 -->
        <div class="grid">
            <div class="card">
                <div class="card-label">튜너 상태 (Lock)</div>
                <div class="card-value" id="tunerLockStatus" style="color: var(--accent-red);">확인 중</div>
                <div class="card-sub" id="tunerModulation">ATSC 8VSB</div>
            </div>
            <div class="card">
                <div class="card-label">현재 수신 채널</div>
                <div class="card-value" id="currentChannel">CH 15</div>
                <div class="card-sub" id="currentChannelName">KBS 1 (479 MHz)</div>
            </div>
            <div class="card">
                <div class="card-label">실시간 비트레이트</div>
                <div class="card-value" id="currentBitrate">0.00 <span style="font-size: 1rem;">Mbps</span></div>
                <div class="card-sub">1:N Fan-out Broadcasting</div>
            </div>
            <div class="card">
                <div class="card-label">동시 접속 플레이어</div>
                <div class="card-value" id="activeClients">0 <span style="font-size: 1rem;">대</span></div>
                <div class="card-sub" id="serverUptime">가동 시간: 00:00:00</div>
            </div>
        </div>

        <!-- 듀얼 스트리밍 주소 안내 -->
        <div class="section-title">📡 듀얼 스트리밍 엔드포인트 (Dual Stream)</div>

        <!-- 1. HTTP TS 스트림 -->
        <div class="stream-card">
            <div class="stream-header">
                <span class="stream-type-tag tag-http">🛡️ HTTP TS (신뢰성 전송 - 안드로이드 ExoPlayer 권장)</span>
                <span style="font-size: 0.8rem; color: var(--text-secondary);">TCP 무손실 &bull; Wi-Fi 최적화</span>
            </div>
            <div class="stream-url-box">
                <div class="url-text" id="streamUrlHttp" style="user-select: all;" title="터치하여 전체 선택">서버 주소 확인 중</div>
                <button class="action-btn" onclick="copyUrl('streamUrlHttp', this)">주소 복사</button>
            </div>
        </div>

        <!-- 2. UDP 멀티캐스트 스트림 -->
        <div class="stream-card">
            <div class="stream-header">
                <span class="stream-type-tag tag-udp">⚡ UDP Multicast (초저지연 전송 - IPTV / VLC 표준)</span>
                <div style="display: flex; align-items: center; gap: 0.5rem;">
                    <span id="udpStatusText" style="font-size: 0.8rem; color: var(--accent-green);">송출 중 (ON)</span>
                    <button class="action-btn toggle-btn on" id="udpToggleBtn" onclick="toggleUdp()">UDP On/Off</button>
                </div>
            </div>
            <div class="stream-url-box">
                <div class="url-text" id="streamUrlUdp" style="user-select: all;" title="터치하여 전체 선택">udp://@239.255.0.1:1234</div>
                <button class="action-btn" onclick="copyUrl('streamUrlUdp', this)">주소 복사</button>
            </div>
        </div>

        <!-- 3. RTP 멀티캐스트 스트림 -->
        <div class="stream-card">
            <div class="stream-header">
                <span class="stream-type-tag tag-udp" style="background:#8b5cf6;">🚀 RTP Multicast (RFC 2250 패킷 동기화 - VLC 권장)</span>
                <div style="display: flex; align-items: center; gap: 0.5rem;">
                    <span id="rtpStatusText" style="font-size: 0.8rem; color: var(--accent-green);">송출 중 (ON)</span>
                    <button class="action-btn toggle-btn on" id="rtpToggleBtn" onclick="toggleRtp()">RTP On/Off</button>
                </div>
            </div>
            <div class="stream-url-box">
                <div class="url-text" id="streamUrlRtp" style="user-select: all;" title="터치하여 전체 선택">rtp://@239.255.0.1:5004</div>
                <button class="action-btn" onclick="copyUrl('streamUrlRtp', this)">주소 복사</button>
            </div>
        </div>

        <!-- 4. RTSP 유니캐스트/인터리빙 스트림 -->
        <div class="stream-card">
            <div class="stream-header">
                <span class="stream-type-tag tag-http" style="background:#0ea5e9;">🎥 RTSP Stream (RFC 2326 / TCP Interleaved - 안드로이드 & VLC)</span>
                <div style="display: flex; align-items: center; gap: 0.5rem;">
                    <span id="rtspStatusText" style="font-size: 0.8rem; color: var(--accent-red);">중지됨 (OFF)</span>
                    <button class="action-btn toggle-btn" id="rtspToggleBtn" onclick="toggleRtsp()">RTSP 시작</button>
                </div>
            </div>
            <div class="stream-url-box">
                <div class="url-text" id="streamUrlRtsp" style="user-select: all;" title="터치하여 전체 선택">rtsp://확인 중:8554/live</div>
                <button class="action-btn" onclick="copyUrl('streamUrlRtsp', this)">주소 복사</button>
            </div>
        </div>

        <section class="guide-card">
            <h2>채널 스캔 및 방송 목록</h2>
            <p>스캔 중 방송 송출이 중단됩니다. 완료·취소 후 이전 방송으로 복귀합니다.</p>
            <label>입력 <select id="scanInput"><option value="cable">케이블</option><option value="antenna">안테나</option></select></label>
            <label>변조 <select id="scanModulation"><option value="8VSB">8VSB</option><option value="QAM256">QAM256</option></select></label>
            <label>시작 CH <input id="scanFirst" type="number" min="2" max="158" value="2" style="width:65px"></label>
            <label>끝 CH <input id="scanLast" type="number" min="2" max="158" value="135" style="width:65px"></label>
            <button class="action-btn" id="scanStart" onclick="startScan()">스캔 시작</button>
            <button class="action-btn" id="scanCancel" onclick="scanAction('/api/scan/cancel')" disabled>취소</button>
            <p id="scanProgress" role="status" aria-live="polite">채널 목록을 불러오는 중...</p>
            <progress id="scanMeter" value="0" max="1" style="width:100%"></progress>
            <label>저장된 방송 <select id="savedChannels" style="max-width:100%"></select></label>
            <button class="action-btn" id="channelSelect" onclick="toggleSavedChannel()">선택 방송 재생</button>
        </section>

        <!-- 🎛️ 스트리밍 및 디코딩 품질 설정 카드 -->
        <section class="guide-card" style="border-left: 4px solid var(--accent-purple);">
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 0.5rem;">
                <h2 style="color: var(--accent-purple); font-size: 1.15rem; font-weight: 700; margin: 0;">🎛️ 스트리밍 및 디코딩 품질 설정 (Quality Options)</h2>
                <span style="font-size: 0.75rem; color: var(--text-secondary);">quality.json 자동 저장</span>
            </div>
            <p style="font-size: 0.85rem; color: var(--text-secondary); margin-bottom: 0.75rem;">
                기기 사양 및 Wi-Fi 환경에 맞춘 최적 품질 파라미터를 JSON으로 영구 관리하며, 웹과 안드로이드 앱에서 실시간 동기화합니다.
            </p>
            <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 0.75rem; margin-bottom: 1rem;">
                <label style="display: flex; flex-direction: column; gap: 0.25rem; font-size: 0.85rem;">
                    <span>디인터레이싱 모드</span>
                    <select id="qualityDeinterlace" style="padding: 0.4rem; background: rgba(15, 23, 42, 0.6); color: var(--text-primary); border: 1px solid var(--card-border); border-radius: 6px;">
                        <option value="bob">Bob (초경량 · 저사양 태블릿 권장)</option>
                        <option value="yadif">Yadif (고화질 · PC 권장)</option>
                        <option value="blend">Blend</option>
                        <option value="linear">Linear</option>
                        <option value="off">Off (끄기)</option>
                    </select>
                </label>
                <label style="display: flex; flex-direction: column; gap: 0.25rem; font-size: 0.85rem;">
                    <span>디코딩 CPU 스레드</span>
                    <select id="qualityThreads" style="padding: 0.4rem; background: rgba(15, 23, 42, 0.6); color: var(--text-primary); border: 1px solid var(--card-border); border-radius: 6px;">
                        <option value="4">4 스레드 (기본 권장)</option>
                        <option value="2">2 스레드 (초저전력)</option>
                        <option value="8">8 스레드 (고성능)</option>
                        <option value="0">자동 (Auto)</option>
                    </select>
                </label>
                <label style="display: flex; flex-direction: column; gap: 0.25rem; font-size: 0.85rem;">
                    <span>네트워크 버퍼 캐시</span>
                    <select id="qualityCaching" style="padding: 0.4rem; background: rgba(15, 23, 42, 0.6); color: var(--text-primary); border: 1px solid var(--card-border); border-radius: 6px;">
                        <option value="500">500 ms (초저지연)</option>
                        <option value="1000">1000 ms (표준 안정 권장)</option>
                        <option value="1500">1500 ms (원거리 Wi-Fi)</option>
                        <option value="2000">2000 ms (고신뢰도 버퍼)</option>
                    </select>
                </label>
                <label style="display: flex; flex-direction: column; gap: 0.25rem; font-size: 0.85rem;">
                    <span>RTSP 전송 방식</span>
                    <select id="qualityRtspTransport" style="padding: 0.4rem; background: rgba(15, 23, 42, 0.6); color: var(--text-primary); border: 1px solid var(--card-border); border-radius: 6px;">
                        <option value="tcp">TCP Interleaved (무손실 권장)</option>
                        <option value="udp">UDP Unicast (초저지연)</option>
                    </select>
                </label>
                <label style="display: flex; flex-direction: column; gap: 0.25rem; font-size: 0.85rem;">
                    <span>하드웨어 가속</span>
                    <select id="qualityHwAccel" style="padding: 0.4rem; background: rgba(15, 23, 42, 0.6); color: var(--text-primary); border: 1px solid var(--card-border); border-radius: 6px;">
                        <option value="auto">자동 (Auto 하이브리드)</option>
                        <option value="on">강제 켜기 (Hardware Only)</option>
                        <option value="off">소프트웨어 (Software Only)</option>
                    </select>
                </label>
            </div>
            <div style="display: flex; align-items: center; gap: 0.5rem;">
                <button class="action-btn" id="btnSaveQuality" onclick="saveQualityConfig()" style="background: #0284c7;">품질 설정 저장</button>
                <button class="action-btn" id="btnResetQuality" onclick="resetQualityConfig()" style="background: #475569;">기본값 복원</button>
                <span id="qualityApplyStatus" style="font-size:0.8rem; color:var(--text-secondary)">Player 적용 확인 대기</span>
                <span id="qualitySaveFeedback" style="font-size: 0.85rem; font-weight: 600; color: var(--accent-green);"></span>
            </div>
        </section>

        <!-- 💡 실시간 영상 시청 가이드 -->
        <div class="guide-card">
            <div style="font-weight: 700; color: var(--accent-primary); margin-bottom: 0.5rem;">
                💡 실시간 영상 시청 방법 (VLC / 안드로이드 플레이어)
            </div>
            <div style="margin-bottom: 0.35rem;">
                &bull; <strong>VLC 미디어 재생기</strong>: 메뉴의 [미디어] ➡️ [네트워크 스트림 열기...] (단축키 <code>Ctrl+N</code>) ➡️ 위 <strong>HTTP TS 주소 복사</strong> 클릭 후 붙여넣고 재생
            </div>
            <div style="margin-bottom: 0.35rem;">
                &bull; <strong>RTSP 무손실/초저지연 재생</strong>: VLC 또는 안드로이드 플레이어에서 <code>rtsp://서버IP:8554/live</code> (기본 정지 상태이므로 위 버튼으로 시작 후 시청)
            </div>
            <div style="margin-bottom: 0.35rem;">
                &bull; <strong>RTP 초저지연 재생 (권장)</strong>: VLC에서 <code>rtp://@239.255.0.1:5004</code> 입력 시 시퀀스 동기화로 패킷 누락 없는 고화질 시청 가능
            </div>
            <div style="margin-bottom: 0.35rem;">
                &bull; <strong>UDP 멀티캐스트 재생</strong>: VLC에서 <code>udp://@239.255.0.1:1234</code> 입력 시 원본 초저지연 시청 가능
            </div>
            <div>
                &bull; <strong>안드로이드 기기 시청</strong>: 동일 Wi-Fi 망의 스마트폰/태블릿에서 안드로이드 전용 앱(LibVLC) 실행 시 위 주소로 원터치 재생
            </div>
        </div>

        <footer>
            Sideway TS Player & Server Ecosystem &bull; Made with 🐾 시연(柴然) & 찌링
        </footer>
    </div>

    <script>
        function updateStatus() {
            fetch('/api/status')
                .then(res => { if (!res.ok) throw new Error('서버 요청 실패'); return res.json(); })
                .then(data => {
                    const lockEl = document.getElementById('tunerLockStatus');
                    if (lockEl) {
                        lockEl.textContent = data.receiving ? (data.tunerLocked ? 'LOCKED · TS 수신' : 'TS 수신') : '방송 미수신';
                        lockEl.style.color = data.receiving ? 'var(--accent-green)' : 'var(--accent-red)';
                    }
                    const modEl = document.getElementById('tunerModulation');
                    if (modEl) modEl.textContent = data.modulation + (data.isVirtual ? ' (가상 모드)' : '');

                    const chEl = document.getElementById('currentChannel');
                    if (chEl) chEl.textContent = 'CH ' + data.currentChannel;

                    const chNameEl = document.getElementById('currentChannelName');
                    if (chNameEl) chNameEl.textContent = data.channelName;

                    const bitEl = document.getElementById('currentBitrate');
                    if (bitEl) bitEl.innerHTML = data.bitrateMbps.toFixed(2) + ' <span style="font-size: 1rem;">Mbps</span>';

                    const clientEl = document.getElementById('activeClients');
                    if (clientEl) clientEl.innerHTML = data.activeClients + ' <span style="font-size: 1rem;">대</span>';

                    const host = data.serverAddress || window.location.host;
                    const httpUrlEl = document.getElementById('streamUrlHttp');
                    if (httpUrlEl) {
                        httpUrlEl.textContent = 'http://' + host + '/stream';
                    }

                    const udpUrlEl = document.getElementById('streamUrlUdp');
                    if (udpUrlEl) udpUrlEl.textContent = 'udp://@' + data.udpTarget;

                    const udpBtn = document.getElementById('udpToggleBtn');
                    const udpTxt = document.getElementById('udpStatusText');
                    if (udpBtn && udpTxt) {
                        if (data.isUdpEnabled) {
                            udpBtn.className = 'action-btn toggle-btn on';
                            udpBtn.textContent = 'UDP 정지';
                            udpTxt.textContent = '송출 중 (ON)';
                            udpTxt.style.color = 'var(--accent-green)';
                        } else {
                            udpBtn.className = 'action-btn toggle-btn';
                            udpBtn.textContent = 'UDP 시작';
                            udpTxt.textContent = '중지됨 (OFF)';
                            udpTxt.style.color = 'var(--accent-red)';
                        }
                    }

                    const rtpUrlEl = document.getElementById('streamUrlRtp');
                    if (rtpUrlEl) rtpUrlEl.textContent = 'rtp://@' + (data.rtpTarget || '239.255.0.1:5004');

                    const rtpBtn = document.getElementById('rtpToggleBtn');
                    const rtpTxt = document.getElementById('rtpStatusText');
                    if (rtpBtn && rtpTxt) {
                        if (data.isRtpEnabled) {
                            rtpBtn.className = 'action-btn toggle-btn on';
                            rtpBtn.textContent = 'RTP 정지';
                            rtpTxt.textContent = '송출 중 (ON)';
                            rtpTxt.style.color = 'var(--accent-green)';
                        } else {
                            rtpBtn.className = 'action-btn toggle-btn';
                            rtpBtn.textContent = 'RTP 시작';
                            rtpTxt.textContent = '중지됨 (OFF)';
                            rtpTxt.style.color = 'var(--accent-red)';
                        }
                    }

                    const rtspPort = data.rtspPort || 8554;
                    const rtspUrlEl = document.getElementById('streamUrlRtsp');
                    if (rtspUrlEl) {
                        const hostIp = (data.serverAddress || window.location.host).split(':')[0];
                        rtspUrlEl.textContent = 'rtsp://' + hostIp + ':' + rtspPort + '/live';
                    }

                    const rtspBtn = document.getElementById('rtspToggleBtn');
                    const rtspTxt = document.getElementById('rtspStatusText');
                    if (rtspBtn && rtspTxt) {
                        if (data.isRtspEnabled) {
                            rtspBtn.className = 'action-btn toggle-btn on';
                            rtspBtn.textContent = 'RTSP 정지';
                            rtspTxt.textContent = '송출 중 (ON)';
                            rtspTxt.style.color = 'var(--accent-green)';
                        } else {
                            rtspBtn.className = 'action-btn toggle-btn';
                            rtspBtn.textContent = 'RTSP 시작';
                            rtspTxt.textContent = '중지됨 (OFF)';
                            rtspTxt.style.color = 'var(--accent-red)';
                        }
                    }

                    // 튜너 하드웨어 정보 갱신
                    const tunerBadge = document.getElementById('tunerBadge');
                    const tunerDriverText = document.getElementById('tunerDriverText');
                    const tunerProductName = document.getElementById('tunerProductName');
                    const tunerHwId = document.getElementById('tunerHwId');
                    const tunerStandards = document.getElementById('tunerStandards');

                    if (tunerBadge) {
                        if (data.hasHardwareTuner) {
                            tunerBadge.innerHTML = '🟢 하드웨어 튜너 연결됨 (ONLINE)';
                            tunerBadge.style.background = 'rgba(34, 197, 94, 0.2)';
                            tunerBadge.style.color = 'var(--accent-green)';
                            tunerBadge.style.borderColor = 'rgba(34, 197, 94, 0.3)';
                        } else {
                            tunerBadge.innerHTML = '🟡 하드웨어 튜너 미감지';
                            tunerBadge.style.background = 'rgba(234, 179, 8, 0.2)';
                            tunerBadge.style.color = '#eab308';
                            tunerBadge.style.borderColor = 'rgba(234, 179, 8, 0.3)';
                        }
                    }
                    if (tunerProductName) tunerProductName.textContent = data.deviceName;
                    if (tunerHwId) tunerHwId.textContent = data.hardwareId;
                    if (tunerDriverText) tunerDriverText.textContent = data.driverStatus;
                    if (tunerStandards) tunerStandards.textContent = data.supportedStandards;

                    const sec = data.uptimeSeconds;
                    const h = String(Math.floor(sec / 3600)).padStart(2, '0');
                    const m = String(Math.floor((sec % 3600) / 60)).padStart(2, '0');
                    const s = String(sec % 60).padStart(2, '0');
                    const uptimeEl = document.getElementById('serverUptime');
                    if (uptimeEl) uptimeEl.textContent = `가동 시간: ${h}:${m}:${s}`;
                    updateChannelSelectButton(!!(data.isBroadcasting || data.receiving));
                })
                .catch(() => {
                    const lockEl = document.getElementById('tunerLockStatus');
                    if (lockEl) { lockEl.textContent = '상태 조회 실패'; lockEl.style.color = 'var(--accent-red)'; }
                    document.getElementById('streamUrlHttp').textContent = '서버 주소 확인 실패';
                    document.getElementById('currentBitrate').textContent = '확인 불가';
                });
        }

        function toggleUdp() {
            const btn = document.getElementById('udpToggleBtn');
            const txt = document.getElementById('udpStatusText');
            if (btn && txt) {
                const isCurrentlyOn = btn.classList.contains('on');
                if (isCurrentlyOn) {
                    btn.className = 'action-btn toggle-btn';
                    btn.textContent = 'UDP 시작';
                    txt.textContent = '중지됨 (OFF)';
                    txt.style.color = 'var(--accent-red)';
                } else {
                    btn.className = 'action-btn toggle-btn on';
                    btn.textContent = 'UDP 정지';
                    txt.textContent = '송출 중 (ON)';
                    txt.style.color = 'var(--accent-green)';
                }
            }

            fetch('/api/udp/enabled', {method:'POST', headers:{'Content-Type':'application/json','X-TS-Action':'1'}, body:JSON.stringify({enabled:!!btn && btn.classList.contains('on')})})
                .then(res => { if (!res.ok) throw new Error('서버 요청 실패'); return res.json(); })
                .then(data => {
                    if (btn && txt) {
                        if (data.isUdpEnabled) {
                            btn.className = 'action-btn toggle-btn on';
                            btn.textContent = 'UDP 정지';
                            txt.textContent = '송출 중 (ON)';
                            txt.style.color = 'var(--accent-green)';
                        } else {
                            btn.className = 'action-btn toggle-btn';
                            btn.textContent = 'UDP 시작';
                            txt.textContent = '중지됨 (OFF)';
                            txt.style.color = 'var(--accent-red)';
                        }
                    }
                })
                .catch(() => updateStatus());
        }

        // RFC 2250 RTP 멀티캐스트 송출 On/Off 토글 (즉각적 낙관적 UI 갱신 + API 호출)
        function toggleRtp() {
            const btn = document.getElementById('rtpToggleBtn');
            const txt = document.getElementById('rtpStatusText');
            if (btn && txt) {
                const isCurrentlyOn = btn.classList.contains('on');
                if (isCurrentlyOn) {
                    btn.className = 'action-btn toggle-btn';
                    btn.textContent = 'RTP 시작';
                    txt.textContent = '중지됨 (OFF)';
                    txt.style.color = 'var(--accent-red)';
                } else {
                    btn.className = 'action-btn toggle-btn on';
                    btn.textContent = 'RTP 정지';
                    txt.textContent = '송출 중 (ON)';
                    txt.style.color = 'var(--accent-green)';
                }
            }

            fetch('/api/rtp/enabled', {method:'POST', headers:{'Content-Type':'application/json','X-TS-Action':'1'}, body:JSON.stringify({enabled:!!btn && btn.classList.contains('on')})})
                .then(res => { if (!res.ok) throw new Error('서버 요청 실패'); return res.json(); })
                .then(data => {
                    if (btn && txt) {
                        if (data.isRtpEnabled) {
                            btn.className = 'action-btn toggle-btn on';
                            btn.textContent = 'RTP 정지';
                            txt.textContent = '송출 중 (ON)';
                            txt.style.color = 'var(--accent-green)';
                        } else {
                            btn.className = 'action-btn toggle-btn';
                            btn.textContent = 'RTP 시작';
                            txt.textContent = '중지됨 (OFF)';
                            txt.style.color = 'var(--accent-red)';
                        }
                    }
                })
                .catch(() => updateStatus());
        }

        // RFC 2326 RTSP 송출 On/Off 토글 (즉각적 낙관적 UI 갱신 + API 호출)
        function toggleRtsp() {
            const btn = document.getElementById('rtspToggleBtn');
            const txt = document.getElementById('rtspStatusText');
            if (btn && txt) {
                const isCurrentlyOn = btn.classList.contains('on');
                if (isCurrentlyOn) {
                    btn.className = 'action-btn toggle-btn';
                    btn.textContent = 'RTSP 시작';
                    txt.textContent = '중지됨 (OFF)';
                    txt.style.color = 'var(--accent-red)';
                } else {
                    btn.className = 'action-btn toggle-btn on';
                    btn.textContent = 'RTSP 정지';
                    txt.textContent = '송출 중 (ON)';
                    txt.style.color = 'var(--accent-green)';
                }
            }

            fetch('/api/rtsp/enabled', {method:'POST', headers:{'Content-Type':'application/json','X-TS-Action':'1'}, body:JSON.stringify({enabled:!!btn && btn.classList.contains('on')})})
                .then(res => { if (!res.ok) throw new Error('서버 요청 실패'); return res.json(); })
                .then(data => {
                    if (btn && txt) {
                        if (data.isRtspEnabled) {
                            btn.className = 'action-btn toggle-btn on';
                            btn.textContent = 'RTSP 정지';
                            txt.textContent = '송출 중 (ON)';
                            txt.style.color = 'var(--accent-green)';
                        } else {
                            btn.className = 'action-btn toggle-btn';
                            btn.textContent = 'RTSP 시작';
                            txt.textContent = '중지됨 (OFF)';
                            txt.style.color = 'var(--accent-red)';
                        }
                    }
                })
                .catch(() => updateStatus());
        }

        function copyUrl(elementId, btnElement) {
            const urlEl = document.getElementById(elementId);
            const text = urlEl ? urlEl.textContent : "";
            if (!text) return;

            // 1. 최신 Clipboard API 시도 (HTTPS 또는 localhost 환경)
            if (navigator.clipboard && window.isSecureContext) {
                navigator.clipboard.writeText(text).then(() => {
                    showCopyFeedback(btnElement);
                }).catch(() => {
                    fallbackCopy(text, btnElement);
                });
                return;
            }

            // 2. 모바일/태블릿 HTTP 사설 IP 환경용 범용 Fallback (100% 호환)
            fallbackCopy(text, btnElement);
        }

        function fallbackCopy(text, btnElement) {
            const textArea = document.createElement("textarea");
            textArea.value = text;
            textArea.style.position = "fixed";
            textArea.style.left = "-9999px";
            textArea.style.top = "-9999px";
            textArea.setAttribute("readonly", "");
            document.body.appendChild(textArea);
            textArea.focus();
            textArea.select();
            textArea.setSelectionRange(0, 99999);

            let copied = false;
            try {
                copied = document.execCommand('copy');
            } catch (err) {
                copied = false;
            }
            document.body.removeChild(textArea);

            if (copied) {
                showCopyFeedback(btnElement);
            } else {
                prompt("주소를 복사하세요 (길게 터치 후 복사):", text);
            }
        }

        function showCopyFeedback(btnElement) {
            if (btnElement) {
                const origText = btnElement.textContent;
                btnElement.textContent = "복사 완료! ✓";
                btnElement.style.background = "#16a34a";
                setTimeout(() => {
                    btnElement.textContent = origText;
                    btnElement.style.background = "";
                }, 2000);
            } else {
                alert("스트리밍 주소가 복사되었습니다!");
            }
        }

        let channelSignature = '';
        async function refreshChannels() {
            try {
                const res = await fetch('/api/channels');
                if (!res.ok) throw new Error('채널 목록 조회 실패');
                const data = await res.json();
                const list = data.channels || [];
                const stateNames = {idle:'대기',scanning:'스캔 중',completed:'완료',cancelled:'취소',failed:'실패'};
                document.getElementById('scanProgress').textContent =
                    `${stateNames[data.state] || '대기'} · ${data.completed || 0}/${data.total || 0} · 발견 ${data.found || 0}개 · ${data.message || ''}`;
                document.getElementById('scanMeter').max = data.total || 1;
                document.getElementById('scanMeter').value = data.completed || 0;
                document.getElementById('scanStart').disabled = !!data.scanning;
                document.getElementById('scanCancel').disabled = !data.scanning;
                document.getElementById('channelSelect').disabled = !!data.scanning || !list.length;
                const select = document.getElementById('savedChannels');
                select.disabled = !!data.scanning;
                const signature = JSON.stringify([list,data.selected]);
                if (signature !== channelSignature) {
                    channelSignature = signature;
                    select.replaceChildren();
                    for (const c of list) {
                        const option = document.createElement('option');
                        option.value = c.id;
                        option.textContent = `${c.name} · CH ${c.physicalChannel} · ${c.modulation}`;
                        option.selected = c.id === data.selected;
                        select.appendChild(option);
                    }
                }
            } catch (error) { document.getElementById('scanProgress').textContent = error.message; }
        }
        async function scanAction(url) {
            try {
                const res = await fetch(url,{method:'POST',headers:{'X-TS-Action':'1'}});
                if (!res.ok) throw new Error(await res.text());
                await refreshChannels();
            } catch(error) { alert('작업 실패: ' + error.message); }
        }
        function startScan() {
            if (!confirm('스캔 중 방송 송출이 중단됩니다. 시작할까요?')) return;
            const value = id => document.getElementById(id).value;
            const first=Number(value('scanFirst')),last=Number(value('scanLast'));
            const max=value('scanInput')==='antenna'?69:158;
            if(!Number.isInteger(first)||!Number.isInteger(last)||first<2||last<first||last>max){alert('검색 범위를 확인하세요. 안테나는 CH 2~69, 케이블은 CH 2~158입니다.');return;}
            scanAction(`/api/scan/start?input=${value('scanInput')}&modulation=${value('scanModulation')}&first=${first}&last=${last}`);
        }
        let isBroadcastingActive = false;

        function updateChannelSelectButton(broadcasting) {
            isBroadcastingActive = broadcasting;
            const btn = document.getElementById('channelSelect');
            const select = document.getElementById('savedChannels');
            if (btn && (!select || !select.disabled)) {
                if (isBroadcastingActive) {
                    btn.textContent = '⏹️ 방송 종료';
                    btn.className = 'action-btn toggle-btn on';
                    btn.style.background = '#dc2626';
                } else {
                    btn.textContent = '▶️ 선택 방송 재생';
                    btn.className = 'action-btn';
                    btn.style.background = '#0284c7';
                }
            }
        }

        function toggleSavedChannel() {
            if (isBroadcastingActive) {
                updateChannelSelectButton(false);
                scanAction('/api/channels/stop');
            } else {
                const id = document.getElementById('savedChannels').value;
                if (!id) {
                    alert('목록에서 방송을 먼저 선택하세요.');
                    return;
                }
                updateChannelSelectButton(true);
                scanAction('/api/channels/select?id=' + encodeURIComponent(id));
            }
        }

        function selectSavedChannel() {
            toggleSavedChannel();
        }

        async function loadQualityConfig() {
            try {
                const res = await fetch('/api/config/quality');
                if (!res.ok) return;
                const config = await res.json();
                if (config.deinterlaceMode) {
                    const el = document.getElementById('qualityDeinterlace');
                    if (el) el.value = config.deinterlaceMode;
                }
                if (config.avcodecThreads !== undefined) {
                    const el = document.getElementById('qualityThreads');
                    if (el) el.value = String(config.avcodecThreads);
                }
                if (config.networkCachingMs !== undefined) {
                    const el = document.getElementById('qualityCaching');
                    if (el) el.value = String(config.networkCachingMs);
                }
                if (config.rtspTransport) {
                    const el = document.getElementById('qualityRtspTransport');
                    if (el) el.value = config.rtspTransport;
                }
                if (config.hardwareAcceleration) {
                    const el = document.getElementById('qualityHwAccel');
                    if (el) el.value = config.hardwareAcceleration;
                }
            } catch (e) {
                console.error("품질 설정 로드 실패:", e);
            }
        }

        async function saveQualityConfig() {
            const feedback = document.getElementById('qualitySaveFeedback');
            const deintEl = document.getElementById('qualityDeinterlace');
            const thEl = document.getElementById('qualityThreads');
            const cacheEl = document.getElementById('qualityCaching');
            const rtspEl = document.getElementById('qualityRtspTransport');
            const hwEl = document.getElementById('qualityHwAccel');

            const payload = {
                deinterlaceMode: deintEl ? deintEl.value : 'bob',
                avcodecThreads: thEl ? parseInt(thEl.value, 10) : 4,
                networkCachingMs: cacheEl ? parseInt(cacheEl.value, 10) : 1000,
                rtspTransport: rtspEl ? rtspEl.value : 'tcp',
                hardwareAcceleration: hwEl ? hwEl.value : 'auto'
            };

            try {
                const res = await fetch('/api/config/quality', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json', 'X-TS-Action': '1' },
                    body: JSON.stringify(payload)
                });
                if (!res.ok) throw new Error(await res.text());
                if (feedback) {
                    feedback.textContent = '설정 저장 완료! ✓';
                    feedback.style.color = 'var(--accent-green)';
                    setTimeout(() => { feedback.textContent = ''; }, 2500);
                }
            } catch (e) {
                if (feedback) {
                    feedback.textContent = '저장 실패: ' + e.message;
                    feedback.style.color = 'var(--accent-red)';
                }
            }
        }

        async function resetQualityConfig() {
            const deintEl = document.getElementById('qualityDeinterlace');
            const thEl = document.getElementById('qualityThreads');
            const cacheEl = document.getElementById('qualityCaching');
            const rtspEl = document.getElementById('qualityRtspTransport');
            const hwEl = document.getElementById('qualityHwAccel');

            if (deintEl) deintEl.value = 'bob';
            if (thEl) thEl.value = '4';
            if (cacheEl) cacheEl.value = '1000';
            if (rtspEl) rtspEl.value = 'tcp';
            if (hwEl) hwEl.value = 'auto';
            await saveQualityConfig();
        }

        async function refreshQualityApplication() {
            const element=document.getElementById('qualityApplyStatus');
            try {
                const res=await fetch('/api/config/quality/status');
                if(!res.ok)throw new Error('상태 조회 실패');
                const status=await res.json();
                const count=(status.clients || []).filter(client=>client.current).length;
                if(element)element.textContent=`설정 v${status.revision} · 최근 5분 Player 적용 보고 ${count}대`;
            } catch(error) {if(element)element.textContent='Player 적용 확인 대기';}
        }
        loadQualityConfig();
        setInterval(refreshQualityApplication,5000);
        refreshQualityApplication();

        setInterval(refreshChannels, 1000);
        refreshChannels();

        setInterval(updateStatus, 1000);
        updateStatus();

    </script>
</body>
</html>
)rawhtml";
}

} // namespace WebDashboard
