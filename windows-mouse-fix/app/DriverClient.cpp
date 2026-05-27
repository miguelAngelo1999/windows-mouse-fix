//
// DriverClient.cpp
//

#include "DriverClient.h"
#include <cstdio>

DriverClient::DriverClient()
    : m_hDevice(INVALID_HANDLE_VALUE)
    , m_lastError(0)
{
}

DriverClient::~DriverClient() {
    close();
}

bool DriverClient::open() {
    if (m_hDevice != INVALID_HANDLE_VALUE) {
        return true; // already open
    }

    m_hDevice = CreateFileW(
        WMF_DEVICE_SYMLINK,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (m_hDevice == INVALID_HANDLE_VALUE) {
        m_lastError = GetLastError();
        return false;
    }

    return true;
}

void DriverClient::close() {
    if (m_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
}

bool DriverClient::submitReport(const WMF_PTP_REPORT& report) {
    if (m_hDevice == INVALID_HANDLE_VALUE) {
        if (!tryReopen()) return false;
    }

    DWORD bytesReturned = 0;
    BOOL  ok = DeviceIoControl(
        m_hDevice,
        IOCTL_WMF_SUBMIT_REPORT,
        (LPVOID)&report,
        (DWORD)sizeof(WMF_PTP_REPORT),
        nullptr,
        0,
        &bytesReturned,
        nullptr
    );

    if (!ok) {
        m_lastError = GetLastError();
        close(); // will attempt reopen on next call
        return false;
    }

    return true;
}

bool DriverClient::tryReopen() {
    // Wait 1 second before retrying to avoid hammering
    static DWORD s_lastRetryTick = 0;
    DWORD now = GetTickCount();
    if (now - s_lastRetryTick < 1000) {
        return false;
    }
    s_lastRetryTick = now;
    return open();
}
