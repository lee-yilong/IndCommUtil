#include "ProtocolRegistry.h"

#include "protocols/serial/SerialProtocol.h"
#include "protocols/tcp/TcpProtocol.h"

namespace icu {

ProtocolRegistry& ProtocolRegistry::Instance() {
    static ProtocolRegistry inst;
    return inst;
}

void ProtocolRegistry::Register(IProtocolPtr proto) {
    const std::string id = proto->Id();
    m_protocols.emplace_back(id, std::move(proto));
}

IProtocol* ProtocolRegistry::Find(const std::string& id) const {
    for (const auto& kv : m_protocols) {
        if (kv.first == id) return kv.second.get();
    }
    return nullptr;
}

std::vector<IProtocol*> ProtocolRegistry::All() const {
    std::vector<IProtocol*> v;
    v.reserve(m_protocols.size());
    for (const auto& kv : m_protocols) v.push_back(kv.second.get());
    return v;
}

void RegisterBuiltinProtocols() {
    auto& reg = ProtocolRegistry::Instance();
    reg.Register(std::make_unique<TcpProtocol>());
    reg.Register(std::make_unique<SerialProtocol>());
}

}  // namespace icu
