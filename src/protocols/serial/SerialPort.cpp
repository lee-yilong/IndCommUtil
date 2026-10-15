#include "SerialPort.h"

#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace icu {

#ifdef _WIN32

namespace {

// UTF-8 → UTF-16（CreateFileW 需要宽字符）
std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    const int len =
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring w(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), len);
    return w;
}

// 构造 Windows 串口路径：COM3 → \\.\COM3
std::wstring MakePortPath(const std::string& portName) {
    const std::wstring wide = Utf8ToWide(portName);
    if (wide.empty()) return {};
    if (wide.rfind(L"\\\\.\\", 0) == 0) return wide;
    return L"\\\\.\\" + wide;
}

BYTE ToWinParity(SerialPort::Parity p) {
    switch (p) {
        case SerialPort::Parity::None:
            return NOPARITY;
        case SerialPort::Parity::Even:
            return EVENPARITY;
        case SerialPort::Parity::Odd:
            return ODDPARITY;
        case SerialPort::Parity::Mark:
            return MARKPARITY;
        case SerialPort::Parity::Space:
            return SPACEPARITY;
    }
    return NOPARITY;
}

BYTE ToWinStopBits(SerialPort::StopBits s) {
    switch (s) {
        case SerialPort::StopBits::One:
            return ONESTOPBIT;
        case SerialPort::StopBits::OnePointFive:
            return ONE5STOPBITS;
        case SerialPort::StopBits::Two:
            return TWOSTOPBITS;
    }
    return ONESTOPBIT;
}

std::string WinErrorText(DWORD err) {
    LPWSTR buf = nullptr;
    const DWORD n = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&buf), 0,
        nullptr);
    if (n == 0 || buf == nullptr) {
        return "Windows error " + std::to_string(err);
    }
    const int utf8Len =
        WideCharToMultiByte(CP_UTF8, 0, buf, static_cast<int>(n), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(utf8Len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, buf, static_cast<int>(n), result.data(), utf8Len, nullptr,
                        nullptr);
    LocalFree(buf);
    while (!result.empty() && (result.back() == '\r' || result.back() == '\n')) {
        result.pop_back();
    }
    return result;
}

}  // namespace

SerialPort::SerialPort() = default;

SerialPort::~SerialPort() {
    Close();
}

bool SerialPort::Open(const Config& cfg, std::string& errorOut) {
    errorOut.clear();

    if (m_open) {
        errorOut = "SerialPort already open";
        return false;
    }
    if (cfg.portName.empty()) {
        errorOut = "Port name is empty";
        return false;
    }

    const std::wstring path = MakePortPath(cfg.portName);
    if (path.empty()) {
        errorOut = "Invalid port name: " + cfg.portName;
        return false;
    }

    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        errorOut = "Failed to open " + cfg.portName + ": " + WinErrorText(GetLastError());
        return false;
    }

    DCB dcb = {};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(h, &dcb)) {
        errorOut = "GetCommState failed: " + WinErrorText(GetLastError());
        CloseHandle(h);
        return false;
    }

    dcb.BaudRate = static_cast<DWORD>(cfg.baudRate);
    dcb.ByteSize = static_cast<BYTE>(cfg.dataBits);
    dcb.Parity = ToWinParity(cfg.parity);
    dcb.StopBits = ToWinStopBits(cfg.stopBits);
    dcb.fBinary = TRUE;
    dcb.fParity = (cfg.parity != Parity::None) ? TRUE : FALSE;

    switch (cfg.flowControl) {
        case FlowControl::RtsCts:
            dcb.fOutxCtsFlow = TRUE;
            dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
            dcb.fOutX = FALSE;
            dcb.fInX = FALSE;
            break;
        case FlowControl::XonXoff:
            dcb.fOutxCtsFlow = FALSE;
            dcb.fRtsControl = RTS_CONTROL_ENABLE;
            dcb.fOutX = TRUE;
            dcb.fInX = TRUE;
            break;
        case FlowControl::None:
        default:
            dcb.fOutxCtsFlow = FALSE;
            dcb.fRtsControl = RTS_CONTROL_ENABLE;
            dcb.fOutX = FALSE;
            dcb.fInX = FALSE;
            break;
    }

    dcb.fDtrControl = DTR_CONTROL_ENABLE;

    if (!SetCommState(h, &dcb)) {
        errorOut = "SetCommState failed: " + WinErrorText(GetLastError());
        CloseHandle(h);
        return false;
    }

    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = static_cast<DWORD>(cfg.readTimeoutMs);
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = static_cast<DWORD>(cfg.writeTimeoutMs);
    if (!SetCommTimeouts(h, &timeouts)) {
        errorOut = "SetCommTimeouts failed: " + WinErrorText(GetLastError());
        CloseHandle(h);
        return false;
    }

    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);

    m_handle = h;
    m_open = true;
    return true;
}

void SerialPort::Close() {
    if (!m_open) return;
    HANDLE h = static_cast<HANDLE>(m_handle);
    m_open = false;
    m_handle = nullptr;
    if (h != INVALID_HANDLE_VALUE && h != nullptr) {
        CloseHandle(h);
    }
}

int SerialPort::Read(uint8_t* buf, size_t len) {
    if (!m_open || buf == nullptr || len == 0) return -1;
    HANDLE h = static_cast<HANDLE>(m_handle);

    const DWORD toRead = (len > 0x7FFFFFFF) ? 0x7FFFFFFF : static_cast<DWORD>(len);
    DWORD bytesRead = 0;
    if (!ReadFile(h, buf, toRead, &bytesRead, nullptr)) {
        const DWORD err = GetLastError();
        if (err == ERROR_OPERATION_ABORTED) return 0;
        return -1;
    }
    return static_cast<int>(bytesRead);
}

int SerialPort::Write(const uint8_t* data, size_t len) {
    if (!m_open || data == nullptr || len == 0) return -1;
    HANDLE h = static_cast<HANDLE>(m_handle);

    const DWORD toWrite = (len > 0x7FFFFFFF) ? 0x7FFFFFFF : static_cast<DWORD>(len);
    DWORD bytesWritten = 0;
    if (!WriteFile(h, data, toWrite, &bytesWritten, nullptr)) {
        return -1;
    }
    return static_cast<int>(bytesWritten);
}

void SerialPort::Interrupt() {
    if (!m_open) return;
    HANDLE h = static_cast<HANDLE>(m_handle);
    CancelIoEx(h, nullptr);
}

std::vector<std::string> SerialPort::EnumeratePorts() {
    std::vector<std::string> ports;

    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &hKey) !=
        ERROR_SUCCESS) {
        return ports;
    }

    WCHAR valueName[256];
    BYTE data[256];
    DWORD index = 0;
    while (true) {
        DWORD nameSize = sizeof(valueName) / sizeof(WCHAR);
        DWORD dataSize = sizeof(data);
        DWORD type = 0;
        const LONG ret =
            RegEnumValueW(hKey, index, valueName, &nameSize, nullptr, &type, data, &dataSize);
        if (ret == ERROR_NO_MORE_ITEMS) break;
        if (ret != ERROR_SUCCESS) {
            ++index;
            continue;
        }

        const auto* wport = reinterpret_cast<const WCHAR*>(data);
        std::wstring wide(wport, dataSize / sizeof(WCHAR));
        while (!wide.empty() && wide.back() == L'\0') wide.pop_back();

        const int len =
            WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len > 1) {
            std::string port(static_cast<size_t>(len - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, port.data(), len, nullptr, nullptr);
            ports.push_back(port);
        }
        ++index;
    }

    RegCloseKey(hKey);
    return ports;
}

#else  // !_WIN32

SerialPort::SerialPort() = default;
SerialPort::~SerialPort() {
    Close();
}

bool SerialPort::Open(const Config&, std::string& errorOut) {
    errorOut = "SerialPort: Linux/macOS implementation not yet available";
    return false;
}

void SerialPort::Close() {}

int SerialPort::Read(uint8_t*, size_t) {
    return -1;
}

int SerialPort::Write(const uint8_t*, size_t) {
    return -1;
}

void SerialPort::Interrupt() {}

std::vector<std::string> SerialPort::EnumeratePorts() {
    return {};
}

#endif  // _WIN32

}  // namespace icu
