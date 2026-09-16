// 대시보드 초기 진입은 조회만 수행하고 미수신/조회 실패를 표시해야 한다.
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const html = fs.readFileSync(path.join(__dirname, '../include/WebDashboard.h'), 'utf8');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
async function run(fail) {
    const elements = new Map();
    const calls = [];
    const document = {
        getElementById(id) {
            if (!elements.has(id)) elements.set(id, { style: {}, textContent: '', innerHTML: '', replaceChildren() {}, appendChild() {} });
            return elements.get(id);
        }, createElement() { return {}; }
    };
    vm.runInNewContext(script, {
        document, console, window: { location: { host: 'localhost:8080' } },
        setInterval() {}, setTimeout() {}, alert() {},
        fetch: async (url) => {
            calls.push(url);
            if (fail) throw new Error('network failure');
            if (url === '/api/config/quality') {
                return { ok: true, json: async () => ({
                    deinterlaceMode: 'bob', avcodecThreads: 4, networkCachingMs: 1000,
                    rtspTransport: 'tcp', hardwareAcceleration: 'auto'
                }) };
            }
            return { ok: true, json: async () => ({
                receiving: false, tunerLocked: false, isClearQam: true, isVirtual: false,
                currentChannel: 15, bitrateMbps: 0, activeClients: 0, uptimeSeconds: 1,
                serverAddress: '192.0.2.10:8080', udpTarget: '239.255.0.1:1234',
                isUdpEnabled: true, hasHardwareTuner: false, modulation: 'Clear QAM 256',
                deviceName: '장치 없음', hardwareId: 'N/A', driverStatus: '미수신', supportedStandards: 'ATSC'
            }) };
        }
    });
    await new Promise(setImmediate);
    assert.deepEqual(calls, ['/api/config/quality', '/api/channels', '/api/status'], '초기 진입 시 조회만 수행해야 함');
    if (fail) {
        assert.equal(elements.get('tunerLockStatus').textContent, '상태 조회 실패');
        assert.equal(elements.get('streamUrlHttp').textContent, '서버 주소 확인 실패');
    } else {
        assert.equal(elements.get('tunerLockStatus').textContent, '방송 미수신');
        assert.equal(elements.get('streamUrlHttp').textContent, 'http://192.0.2.10:8080/stream');
        assert.equal(elements.get('streamUrlRtsp').textContent, 'rtsp://192.0.2.10:8554/live');
        assert.equal(elements.get('rtspStatusText').textContent, '중지됨 (OFF)');
        assert.equal(elements.get('rtspToggleBtn').textContent, 'RTSP 시작');
        assert.equal(elements.get('qualityDeinterlace').value, 'bob');
        assert.equal(elements.get('qualityThreads').value, '4');
        assert.equal(elements.get('qualityCaching').value, '1000');
        assert.equal(elements.get('qualityRtspTransport').value, 'tcp');
        assert.equal(elements.get('qualityHwAccel').value, 'auto');
        assert.equal(elements.get('tunerBadge').innerHTML, '🟡 하드웨어 튜너 미감지');
    }
}
(async () => {
    await run(false);
    await run(true);
    console.log('PASS: 조회 전용 초기 진입, LAN 주소, 미수신 및 조회 실패 표시');
})().catch(error => { console.error(error); process.exitCode = 1; });
