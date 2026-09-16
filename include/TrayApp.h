/**
 * @file TrayApp.h
 * @brief Windows 시스템 트레이 아이콘 관리 및 관제 콘솔 바로가기 애플리케이션
 * @author Sideway Team
 * @date 2026-09-16
 */

#pragma once

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <functional>

/**
 * @class TrayApp
 * @brief Windows 작업 표시줄 시스템 트레이 아이콘, 컨텍스트 메뉴 및 알림 팝업을 관리는 클래스
 */
class TrayApp {
public:
    /**
     * @brief TrayApp 생성자
     * @param hInstance 인스턴스 핸들
     * @param port 웹 관제 콘솔 접속 포트 번호
     * @param onExitCallback 종료 메뉴 선택 시 호출되는 콜백 함수
     */
    TrayApp(HINSTANCE hInstance, int port, std::function<void()> onExitCallback);

    /**
     * @brief TrayApp 소멸자 (트레이 아이콘 제거)
     */
    ~TrayApp();

    /**
     * @brief 윈도우 클래스 등록, 메시지 창 생성 및 시스템 트레이 아이콘 등록
     * @return 성공 시 true, 실패 시 false
     */
    bool Initialize();

    /**
     * @brief Windows 표준 메세지 루프 실행 (서버 종료 시까지 대기)
     */
    void RunMessageLoop();

    /**
     * @brief 트레이 애플리케이션 및 메세지 루프 종료
     */
    void Stop();

    /**
     * @brief Windows 트레이 풍선 알림(Balloon Notification) 표출
     * @param title 알림 제목
     * @param message 알림 상세 본문
     */
    void ShowNotification(const std::wstring& title, const std::wstring& message);

private:
    /**
     * @brief Windows 메세지 처리 콜백 프로시저
     */
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * @brief 우클릭 트레이 컨텍스트 메뉴 생성 및 처리 (웹콘솔 열기, 서버 종료 등)
     */
    void HandleTrayMenu();

    /**
     * @brief 기본 브라우저를 호출하여 웹 관제 대시보드(http://localhost:port) 오픈
     */
    void OpenWebDashboard();

    HINSTANCE m_hInstance;                       ///< Windows 애플리케이션 인스턴스 핸들
    int m_port;                                  ///< 웹 대시보드 포트 번호
    std::function<void()> m_onExitCallback;     ///< 종료 콜백 함수

    HWND m_hwnd = nullptr;                       ///< 메세지 전용 윈도우 핸들
    NOTIFYICONDATAW m_nid{};                    ///< Windows 트레이 아이콘 데이터 구조체
    bool m_running = false;                      ///< 루프 가동 플래그

    static TrayApp* s_instance;                  ///< WndProc 연동용 싱글톤 인스턴스 포인터
};
