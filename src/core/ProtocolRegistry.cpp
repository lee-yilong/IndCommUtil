#include "ProtocolRegistry.h"

#include "protocols/modbus/ModbusProtocol.h"
#include "protocols/mqtt/MqttProtocol.h"
#include "protocols/serial/SerialProtocol.h"
#include "protocols/tcpip/TcpIpProtocol.h"

namespace icu {

ProtocolRegistry& ProtocolRegistry::Instance() {
    static ProtocolRegistry inst;
    return inst;
}

void ProtocolRegistry::Register(IProtocolPtr proto) {
    const std::string id = proto->Id();
    auto kind = (id == "modbus") ? ProtocolKind::Transaction : ProtocolKind::ByteStream;
    m_protocols.push_back({id, std::move(proto), kind});
}

IProtocol* ProtocolRegistry::Find(const std::string& id) const {
    for (const auto& e : m_protocols) {
        if (e.id == id) return e.proto.get();
    }
    return nullptr;
}

std::vector<IProtocol*> ProtocolRegistry::All() const {
    std::vector<IProtocol*> v;
    v.reserve(m_protocols.size());
    for (const auto& e : m_protocols) v.push_back(e.proto.get());
    return v;
}

ProtocolKind ProtocolRegistry::KindOf(const std::string& id) const {
    for (const auto& e : m_protocols) {
        if (e.id == id) return e.kind;
    }
    return ProtocolKind::ByteStream;
}

void RegisterBuiltinProtocols() {
    auto& reg = ProtocolRegistry::Instance();
    reg.Register(std::make_unique<TcpIpProtocol>());
    reg.Register(std::make_unique<SerialProtocol>());
    reg.Register(std::make_unique<ModbusProtocol>());
    reg.Register(std::make_unique<MqttProtocol>());
}

}  // namespace icu
