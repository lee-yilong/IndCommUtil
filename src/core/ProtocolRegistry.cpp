#include "ProtocolRegistry.h"

#include "protocols/modbus/ModbusProtocol.h"
#include "protocols/serial/SerialProtocol.h"
#include "protocols/tcp/TcpProtocol.h"

namespace icu {

ProtocolRegistry& ProtocolRegistry::Instance() {
    static ProtocolRegistry inst;
    return inst;
}

void ProtocolRegistry::Register(IProtocolPtr proto) {
    const std::string id = proto->Id();
    // 简单的通过 dynamic_cast 判断是不是事务型（因为 ModbusSession 会返回 ITransactionSession
    // 接口） 其实在协议创建时，我们无法直接知道，最好是用注册时的类型判断，或者直接根据 id
    // 硬编码判断。 为了简单，我们在注册时，如果是 Modbus，暂时用 id 判断一下，或者干脆通过
    // dynamic_cast 试试。 更好的方式是我们用一个 TypeId 去查，这里为了不破坏你现有的机制，我们用
    // dynamic_cast 尝试。

    // 不过其实 ModbusProtocol 自己清楚，我们可以在 Registration
    // 时指明，但现在为了简化，我们这么写：
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
    reg.Register(std::make_unique<TcpProtocol>());
    reg.Register(std::make_unique<SerialProtocol>());
    reg.Register(std::make_unique<ModbusProtocol>());
}

}  // namespace icu
