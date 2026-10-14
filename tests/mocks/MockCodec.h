#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/ICodec.h"

namespace icu {
namespace test {

class MockCodec : public ICodec {
public:
    MockCodec() = default;
    ~MockCodec() override = default;

    bool Encode(const std::vector<uint8_t>& message, std::vector<uint8_t>& out) override {
        ++m_encodeCount;
        if (m_failEncode) return false;
        out = message;
        return true;
    }

    void Feed(const uint8_t* data, size_t len, MessageCallback onMessage) override {
        ++m_feedCount;
        m_totalFed += len;

        m_buffer.insert(m_buffer.end(), data, data + len);

        if (m_frameSize == 0) {
            if (!m_buffer.empty()) {
                onMessage(m_buffer);
                m_buffer.clear();
            }
            return;
        }

        while (m_buffer.size() >= m_frameSize) {
            std::vector<uint8_t> frame(m_buffer.begin(), m_buffer.begin() + m_frameSize);
            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + m_frameSize);
            onMessage(frame);
        }
    }

    void Reset() override {
        ++m_resetCount;
        m_buffer.clear();
    }

    std::string Name() const override { return "Mock"; }

    void SetFrameSize(size_t n) { m_frameSize = n; }
    void SetFailEncode(bool v) { m_failEncode = v; }

    int EncodeCount() const { return m_encodeCount; }
    int FeedCount() const { return m_feedCount; }
    int ResetCount() const { return m_resetCount; }
    size_t TotalFed() const { return m_totalFed; }
    size_t BufferedBytes() const { return m_buffer.size(); }

private:
    std::vector<uint8_t> m_buffer;
    size_t m_frameSize = 0;
    bool m_failEncode = false;
    int m_encodeCount = 0;
    int m_feedCount = 0;
    int m_resetCount = 0;
    size_t m_totalFed = 0;
};

}  // namespace test
}  // namespace icu
