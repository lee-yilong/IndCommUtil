#include "SerialTransport.h"

#include <memory>
#include <vector>

#include "log/Log.h"
#include "spdlog/spdlog.h"

namespace icu {

SerialTransport::SerialTransport(const SerialPort::Config& cfg) : m_cfg(cfg) {}

SerialTransport::~SerialTransport() {
    Close();
}

std::string SerialTransport::Target() const {
    return m_cfg.portName + " @ " + std::to_string(m_cfg.baudRate);
}

bool SerialTransport::Open() {
    if (m_state == TransportState::Opening || m_state == TransportState::Open) return true;

    SetState(TransportState::Opening);

    std::string err;
    if (!m_port.Open(m_cfg, err)) {
        ICU_SPDLOG_ERROR(_("Failed to open serial port: %s"), wxString::FromUTF8(err.c_str()));
        SetState(TransportState::Closed);
        return false;
    }

    m_stopFlag.store(false);
    m_readThread = std::thread(&SerialTransport::ReadLoop, this);

    ICU_SPDLOG_INFO(_("Serial port opened: %s"), wxString::FromUTF8(Target().c_str()));
    SetState(TransportState::Open);
    return true;
}

void SerialTransport::Close() {
    if (m_state == TransportState::Closed && !m_readThread.joinable()) return;

    // 1. 通知读线程停止
    m_stopFlag.store(true);

    // 2. 打断阻塞在 ReadFile 的读线程（CancelIoEx）
    m_port.Interrupt();

    // 3. 等线程退出
    if (m_readThread.joinable()) {
        m_readThread.join();
    }

    // 4. 关串口
    m_port.Close();

    SetState(TransportState::Closed);
    SPDLOG_INFO(_("Serial port closed"));
}

size_t SerialTransport::Send(const uint8_t* data, size_t len) {
    if (m_state != TransportState::Open) return 0;
    const int n = m_port.Write(data, len);
    return (n > 0) ? static_cast<size_t>(n) : 0;
}

void SerialTransport::ReadLoop() {
    uint8_t buf[4096];
    while (!m_stopFlag.load()) {
        const int n = m_port.Read(buf, sizeof(buf));

        if (n > 0) {
            // 拷贝到堆上，CallAfter 抛回主线程
            auto data = std::make_shared<std::vector<uint8_t>>(buf, buf + n);
            CallAfter([this, data]() {
                if (m_stopFlag.load()) return;  // 已关闭，丢弃
                NotifyData(0, data->data(), data->size());
            });
        } else if (n < 0) {
            // 错误：串口可能被拔出或设备异常
            CallAfter([this]() {
                if (m_stopFlag.load()) return;
                ICU_SPDLOG_WARN(_("Serial port read error"));
                SetState(TransportState::Closed);
            });
            break;
        }
        // n == 0 是读超时，继续循环
    }
}

void SerialTransport::SetState(TransportState st) {
    if (m_state == st) return;
    m_state = st;
    NotifyState(st);
}

}  // namespace icu
