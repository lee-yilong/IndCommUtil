#pragma once

// =========================================================================
// SerialPort —— 跨平台串口封装（平台无关接口）
//
// 不依赖 wxWidgets，方便单测和跨平台。
// Windows 实现用 CreateFileW + SetCommState + ReadFile/WriteFile；
// Linux 实现留 #ifdef 骨架，阶段 5 不实现。
// =========================================================================

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace icu {

class SerialPort {
public:
    enum class Parity { None, Even, Odd, Mark, Space };
    enum class StopBits { One, OnePointFive, Two };
    enum class FlowControl { None, RtsCts, XonXoff };

    struct Config {
        std::string portName;  // "COM3"（Windows）/ "/dev/ttyUSB0"（Linux）
        int baudRate = 9600;   // 任意正整数，支持非标波特率
        int dataBits = 8;      // 5 / 6 / 7 / 8
        Parity parity = Parity::None;
        StopBits stopBits = StopBits::One;
        FlowControl flowControl = FlowControl::None;
        int readTimeoutMs = 100;    // 单次 Read 最长阻塞时间
        int writeTimeoutMs = 1000;  // 单次 Write 最长阻塞时间
    };

    SerialPort();
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    // 打开串口。失败时 errorOut 填入错误描述（UTF-8）。
    bool Open(const Config& cfg, std::string& errorOut);

    // 关闭串口。若当前有阻塞的 Read，需先调用 Interrupt()。
    void Close();

    bool IsOpen() const { return m_open; }

    // 阻塞读。返回实际读到的字节数；0 表示超时；-1 表示错误。
    int Read(uint8_t* buf, size_t len);

    // 阻塞写。返回实际写入的字节数；-1 表示错误。
    int Write(const uint8_t* data, size_t len);

    // 打断正在阻塞的 Read（从另一个线程调用）。
    // Windows 用 CancelIoEx；Linux 留空。
    void Interrupt();

    // 枚举当前系统上可用的串口名（UTF-8）。
    // Windows：读注册表 HARDWARE\DEVICEMAP\SERIALCOMM
    // Linux：TODO（阶段 6+ 实现）
    static std::vector<std::string> EnumeratePorts();

private:
    bool m_open = false;

#ifdef _WIN32
    void* m_handle = nullptr;  // HANDLE（避免头文件引入 windows.h）
#else
    int m_fd = -1;
#endif
};

}  // namespace icu
