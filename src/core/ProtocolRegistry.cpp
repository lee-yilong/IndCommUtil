#include "ProtocolRegistry.h"

#include "protocols/serial/SerialProtocol.h"
#include "protocols/tcp/TcpProtocol.h"

namespace icu {

ProtocolRegistry& ProtocolRegistry::Instance() {
    static ProtocolRegistry inst;
    return inst;
}

void ProtocolRegistry::Register(IProtocolPtr proto) {
    m_protocols[proto->Id()] = std::move(proto);
}

IProtocol* ProtocolRegistry::Find(const std::string& id) const {
    auto it = m_protocols.find(id);
    return it == m_protocols.end() ? nullptr : it->second.get();
}

std::vector<IProtocol*> ProtocolRegistry::All() const {
    std::vector<IProtocol*> v;
    v.reserve(m_protocols.size());
    for (auto& kv : m_protocols) v.push_back(kv.second.get());
    return v;
}

void RegisterBuiltinProtocols() {
    auto& reg = ProtocolRegistry::Instance();
    reg.Register(std::make_unique<TcpProtocol>());
    reg.Register(std::make_unique<SerialProtocol>());
}

}  // namespace icu
