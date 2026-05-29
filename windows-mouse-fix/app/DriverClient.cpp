//
// DriverClient.cpp
//

#include "DriverClient.h"
#include <setupapi.h>
#include <cstdio>
#include <cstdlib>

#pragma comment(lib, "setupapi.lib")

DriverClient::DriverClient()
    : m_hDevice(INVALID_HANDLE_VALUE)
    , m_mode(DriverMode::NotConnected)
    , m_lastError(0)
{
}

DriverClient::~DriverClient() {
    close();
}

DriverMode DriverClient::open() {
    // Try the real driver first
    if (tryOpenDriver()) {
        m_mode = DriverMode::VirtualDriver;
        return m_mode;
    }

    // Fall back to InjectTouchInput
    if (m_touchInjector.init()) {
        m_mode = DriverMode::TouchInject;
        return m_mode;
    }

    m_mode = DriverMode::NotConnected;
    return m_mode;
}

bool DriverClient::tryOpenDriver() {
    if (m_hDevice != INVALID_HANDLE_VALUE) return true;

    // First try the symbolic link (works if driver creates one)
    m_hDevice = CreateFileW(
        WMF_DEVICE_SYMLINK,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (m_hDevice != INVALID_HANDLE_VALUE) return true;

    // Fall back to device interface discovery
    // GUID_DEVINTERFACE_WMF_VIRTUAL_PAD = {B5A2C4D1-3E7F-4A8B-9C6D-1F2E3A4B5C6D}
    static const GUID GUID_DEVINTERFACE_WMF = 
        { 0xB5A2C4D1, 0x3E7F, 0x4A8B, { 0x9C, 0x6D, 0x1F, 0x2E, 0x3A, 0x4B, 0x5C, 0x6D } };

    HDEVINFO devInfo = SetupDiGetClassDevsW(
        &GUID_DEVINTERFACE_WMF, nullptr, nullptr,
        DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

    if (devInfo == INVALID_HANDLE_VALUE) {
        m_lastError = GetLastError();
        return false;
    }

    SP_DEVICE_INTERFACE_DATA ifData = {};
    ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    if (!SetupDiEnumDeviceInterfaces(devInfo, nullptr, &GUID_DEVINTERFACE_WMF, 0, &ifData)) {
        SetupDiDestroyDeviceInfoList(devInfo);
        m_lastError = GetLastError();
        return false;
    }

    // Get required size
    DWORD requiredSize = 0;
    SetupDiGetDeviceInterfaceDetailW(devInfo, &ifData, nullptr, 0, &requiredSize, nullptr);

    auto detailBuf = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)malloc(requiredSize);
    if (!detailBuf) {
        SetupDiDestroyDeviceInfoList(devInfo);
        return false;
    }
    detailBuf->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

    if (!SetupDiGetDeviceInterfaceDetailW(devInfo, &ifData, detailBuf, requiredSize, nullptr, nullptr)) {
        free(detailBuf);
        SetupDiDestroyDeviceInfoList(devInfo);
        m_lastError = GetLastError();
        return false;
    }

    m_hDevice = CreateFileW(
        detailBuf->DevicePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    free(detailBuf);
    SetupDiDestroyDeviceInfoList(devInfo);

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
    m_mode = DriverMode::NotConnected;
}

bool DriverClient::submitReport(const WMF_PTP_REPORT& report) {
    switch (m_mode) {

    case DriverMode::VirtualDriver: {
        if (m_hDevice == INVALID_HANDLE_VALUE) {
            if (!tryReopen()) return false;
        }
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(
            m_hDevice,
            IOCTL_WMF_SUBMIT_REPORT,
            (LPVOID)&report,
            (DWORD)sizeof(WMF_PTP_REPORT),
            nullptr, 0,
            &bytesReturned,
            nullptr
        );
        if (!ok) {
            m_lastError = GetLastError();
            close();
            // Try falling back to touch injection
            if (m_touchInjector.isAvailable() || m_touchInjector.init()) {
                m_mode = DriverMode::TouchInject;
                return m_touchInjector.submitReport(report);
            }
            return false;
        }
        return true;
    }

    case DriverMode::TouchInject:
        return m_touchInjector.submitReport(report);

    default:
        return false;
    }
}

bool DriverClient::tryReopen() {
    static DWORD s_lastRetryTick = 0;
    DWORD now = GetTickCount();
    if (now - s_lastRetryTick < 1000) return false;
    s_lastRetryTick = now;
    return tryOpenDriver();
}

const wchar_t* DriverClient::statusText() const {
    switch (m_mode) {
    case DriverMode::VirtualDriver:
        return L"Windows Mouse Fix — Active (Full PTP mode)";
    case DriverMode::TouchInject:
        return L"Windows Mouse Fix — Active (Basic mode, install driver for rubber-band)";
    default:
        return L"Windows Mouse Fix — Driver not installed";
    }
}
