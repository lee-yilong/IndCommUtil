#pragma once

#include <map>
#include <memory>
#include <vector>

#include "IProtocol.h"

namespace icu {

// 协议注册中心（单例）
class ProtocolRegistry {
public:
    static ProtocolRegistry& Instance();

    void Register(IProtocolPtr proto);
    IProtocol* Find(const std::string& id) const;
    std::vector<IProtocol*> All() const;

private:
    ProtocolRegistry() = default;
    std::map<std::string, IProtocolPtr> m_protocols;
};

// 在 App::OnInit 里调用一次
void RegisterBuiltinProtocols();

}  // namespace icu
