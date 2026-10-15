#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "IProtocol.h"

namespace icu {

// 协议注册中心（单例）
// 用 vector 保留注册顺序，菜单按注册顺序显示
class ProtocolRegistry {
public:
    static ProtocolRegistry& Instance();

    void Register(IProtocolPtr proto);
    IProtocol* Find(const std::string& id) const;
    std::vector<IProtocol*> All() const;

private:
    ProtocolRegistry() = default;
    std::vector<std::pair<std::string, IProtocolPtr>> m_protocols;
};

// 在 App::OnInit 里调用一次
void RegisterBuiltinProtocols();

}  // namespace icu
