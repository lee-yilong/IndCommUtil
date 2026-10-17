#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "IProtocol.h"

namespace icu {

enum class ProtocolKind { ByteStream, Transaction };

class ProtocolRegistry {
public:
    static ProtocolRegistry& Instance();

    // 统一成一个注册函数
    void Register(IProtocolPtr proto);

    IProtocol* Find(const std::string& id) const;
    std::vector<IProtocol*> All() const;

    ProtocolKind KindOf(const std::string& id) const;

private:
    ProtocolRegistry() = default;

    struct Entry {
        std::string id;
        IProtocolPtr proto;
        ProtocolKind kind;
    };

    std::vector<Entry> m_protocols;
};

void RegisterBuiltinProtocols();

}  // namespace icu
