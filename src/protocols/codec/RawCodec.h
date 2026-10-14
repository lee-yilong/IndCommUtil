#pragma once

#include "core/ICodec.h"

namespace icu {

class RawCodec : public ICodec {
public:
    bool Encode(const std::vector<uint8_t>& message, std::vector<uint8_t>& out) override {
        out = message;
        return true;
    }

    void Feed(const uint8_t* data, size_t len, MessageCallback onMessage) override {
        if (len > 0) onMessage(std::vector<uint8_t>(data, data + len));
    }

    void Reset() override {}

    std::string Name() const override { return "Raw"; }
};

}  // namespace icu
