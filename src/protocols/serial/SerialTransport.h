#pragma once

// =========================================================================
// SerialTransport —— ITransport 的串口实现
//
// 组合 SerialPort + 后台读线程：
//   - 读线程阻塞在 SerialPort::Read()，读到数据经 CallAfter 抛回主线程
//   - Close() 顺序：停标志 → Interrupt() → join → 关闭串口
//
// 串口是点对点，没有客户端概念：IsServerMode() = false，clientId 恒为 0。
// =========================================================================

#include <wx/event.h>

#include <atomic>
#include <thread>

#include "core/ITransport.h"
#include "protocols/serial/SerialPort.h"

namespace icu {

class SerialTransport : public wxEvtHandler, public ITransport {
public:
    explicit SerialTransport(const SerialPort::Config& cfg);
    ~SerialTransport() override;

    bool Open() override;
    void Close() override;
    TransportState State() const override { return m_state; }
    bool IsOpen() const override { return m_state == TransportState::Open; }
    bool IsServerMode() const override { return false; }
    std::string Target() const override;

    size_t Send(const uint8_t* data, size_t len) override;

private:
    void ReadLoop();
    void SetState(TransportState st);

    SerialPort::Config m_cfg;
    SerialPort m_port;
    TransportState m_state = TransportState::Idle;

    std::thread m_readThread;
    std::atomic<bool> m_stopFlag{false};
};

}  // namespace icu
