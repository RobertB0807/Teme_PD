#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define SERVICE_NAME L"HelloWorldService"
#define DISPLAY_NAME L"Hello World Service"

SERVICE_STATUS g_service_status = { 0 };
SERVICE_STATUS_HANDLE g_status_handle = NULL;

VOID ServiceCtrlHandler(DWORD dwCtrl);
VOID ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv);

VOID ServiceCtrlHandler(DWORD dwCtrl) {
    switch (dwCtrl) {
        case SERVICE_CONTROL_STOP:
            g_service_status.dwCurrentState = SERVICE_STOPPED;
            SetServiceStatus(g_status_handle, &g_service_status);
            break;
        case SERVICE_CONTROL_PAUSE:
            g_service_status.dwCurrentState = SERVICE_PAUSED;
            SetServiceStatus(g_status_handle, &g_service_status);
            break;
        case SERVICE_CONTROL_CONTINUE:
            g_service_status.dwCurrentState = SERVICE_RUNNING;
            SetServiceStatus(g_status_handle, &g_service_status);
            break;
        case SERVICE_CONTROL_INTERROGATE:
            SetServiceStatus(g_status_handle, &g_service_status);
            break;
        default:
            break;
    }
}

VOID ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv) {
    // Initialize the service status
    g_service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_service_status.dwCurrentState = SERVICE_START_PENDING;
    g_service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE;
    g_service_status.dwWin32ExitCode = 0;
    g_service_status.dwServiceSpecificExitCode = 0;
    g_service_status.dwCheckPoint = 0;
    g_service_status.dwWaitHint = 0;

    // Register the handler
    g_status_handle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
    if (!g_status_handle) {
        return;
    }

    // Write "Hello World!" to Event Log
    HANDLE hEventLog = RegisterEventSource(NULL, SERVICE_NAME);
    if (hEventLog) {
        LPCWSTR lpszStrings[1];
        lpszStrings[0] = L"Hello World! Service initialized successfully.";
        ReportEvent(hEventLog, EVENTLOG_INFORMATION_TYPE, 0, 1000, NULL, 1, 0, lpszStrings, NULL);
        DeregisterEventSource(hEventLog);
    }

    // Service is now running
    g_service_status.dwCurrentState = SERVICE_RUNNING;
    g_service_status.dwWaitHint = 0;
    SetServiceStatus(g_status_handle, &g_service_status);

    // Keep the service running
    while (g_service_status.dwCurrentState == SERVICE_RUNNING || g_service_status.dwCurrentState == SERVICE_PAUSED) {
        Sleep(1000);
    }

    g_service_status.dwCurrentState = SERVICE_STOPPED;
    g_service_status.dwWaitHint = 0;
    SetServiceStatus(g_status_handle, &g_service_status);
}

int wmain(int argc, wchar_t *argv[]) {
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        { (LPWSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcher(ServiceTable)) {
        wprintf(L"StartServiceCtrlDispatcher failed (%ld)\n", GetLastError());
        return 1;
    }

    return 0;
}
