#pragma once

#include <vector>

#include "core/ICodec.h"

namespace icu {

class ModbusRtuCodec : public ICodec {
public:
    bool Encode(const std::vector<uint8_t>& message, std::vector<uint8_t>& out) override;
    void Feed(const uint8_t* data, size_t len, MessageCallback onMessage) override;
    void Reset() override { m_buffer.clear(); }
    std::string Name() const override { return "Modbus RTU"; }

private:
    std::vector<uint8_t> m_buffer;
    static int ExpectedLength(const std::vector<uint8_t>& buf, size_t pos);
};

}  // namespace icu
