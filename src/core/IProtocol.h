#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ProtocolTypes.h"

namespace icu {

class Session;
using SessionPtr = std::unique_ptr<Session>;

// 协议接口：所有协议实现这个接口
// 新增协议 = 新增一个 IProtocol 子类，不改 UI
class IProtocol {
public:
    virtual ~IProtocol() = default;

    // ---------- 元数据 ----------
    virtual std::string Id() const = 0;           // "tcp"
    virtual std::string DisplayName() const = 0;  // "&TCP/IP"（翻译 key）

    // ---------- 模式 ----------
    virtual bool HasModes() const = 0;
    virtual std::vector<std::string> ModeNames() const = 0;

    // ---------- 字段描述 ----------
    virtual std::vector<FieldDescriptor> FieldsForMode(int modeIndex) const = 0;

    // ---------- 会话创建 ----------
    virtual SessionPtr CreateSession(int modeIndex,
                                     const std::map<std::string, FieldValue>& fields) = 0;

    // ---------- 文件名生成 ----------
    virtual std::string MakeDefaultFileName(
        int modeIndex, const std::map<std::string, FieldValue>& fields) const = 0;
};

using IProtocolPtr = std::unique_ptr<IProtocol>;

}  // namespace icu
