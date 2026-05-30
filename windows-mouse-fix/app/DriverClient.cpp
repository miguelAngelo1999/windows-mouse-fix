//
// DriverClient.cpp
// Communicates with WmfVirtualPad via HidD_SetOutputReport on the HID child device.
//

#include "DriverClient.h"
#include <setupapi.h>
#include <hidsdi.h>
#include <cstdio>
#include <cstdlib>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

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
    if (tryOpenDriver()) {
        m_mode = DriverMode::VirtualDriver;
        // Write mode to log file for diagnostics
        FILE* f = nullptr;
        fopen_s(&f, "C:\\Users\\virgoh\\wmf_mode.txt", "w");
        if (f) { fprintf(f, "VirtualDriver\n"); fclose(f); }
        return m_mode;
    }
    if (m_touchInjector.init()) {
        m_mode = DriverMode::TouchInject;
        FILE* f = nullptr;
        fopen_s(&f, "C:\\Users\\virgoh\\wmf_mode.txt", "w");
        if (f) { fprintf(f, "TouchInject\n"); fclose(f); }
        return m_mode;
    }
    m_mode = DriverMode::NotConnected;
    FILE* f = nullptr;
    fopen_s(&f, "C:\\Users\\virgoh\\wmf_mode.txt", "w");
    if (f) { fprintf(f, "NotConnected\n"); fclose(f); }
    return m_mode;
}

bool DriverClient::tryOpenDriver() {
    if (m_hDevice != INVALID_HANDLE_VALUE) return true;

    // Find our HID child device (hidclass instance)
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO devInfo = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr,
        DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if (devInfo == INVALID_HANDLE_VALUE) return false;

    SP_DEVICE_INTERFACE_DATA ifData = {};
    ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(devInfo, nullptr, &hidGuid, i, &ifData); i++) {
        DWORD sz = 0;
        SetupDiGetDeviceInterfaceDetailW(devInfo, &ifData, nullptr, 0, &sz, nullptr);
        auto det = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)malloc(sz);
        if (!det) continue;
        det->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        SetupDiGetDeviceInterfaceDetailW(devInfo, &ifData, det, sz, nullptr, nullptr);

        // Our device path contains "hidclass"
        bool isOurs = (wcsstr(det->DevicePath, L"hidclass") != nullptr ||
                       wcsstr(det->DevicePath, L"HIDCLASS") != nullptr);

        if (isOurs) {
            HANDLE h = CreateFileW(det->DevicePath,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, 0, nullptr);

            if (h != INVALID_HANDLE_VALUE) {
                // Verify it's our touchpad by checking HID caps
                PHIDP_PREPARSED_DATA preparsed = nullptr;
                if (HidD_GetPreparsedData(h, &preparsed)) {
                    HIDP_CAPS caps;
                    if (HidP_GetCaps(preparsed, &caps) == HIDP_STATUS_SUCCESS &&
                        caps.UsagePage == 0x000D &&  // Digitizer
                        caps.Usage == 0x0005 &&       // Touch Pad
                        caps.OutputReportByteLength == 34) {
                        HidD_FreePreparsedData(preparsed);
                        free(det);
                        SetupDiDestroyDeviceInfoList(devInfo);
                        m_hDevice = h;
                        return true;
                    }
                    HidD_FreePreparsedData(preparsed);
                }
                CloseHandle(h);
            }
        }
        free(det);
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return false;
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

        // Build output report: [reportId=0x06][33 bytes of PTP data]
        // PTP data = WMF_PTP_REPORT without the report_id byte
        unsigned char outBuf[34];
        outBuf[0] = 0x06; // output report ID
        memcpy(outBuf + 1, ((const unsigned char*)&report) + 1, 33);

        BOOL ok = HidD_SetOutputReport(m_hDevice, outBuf, sizeof(outBuf));
        if (!ok) {
            m_lastError = GetLastError();
            close();
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
