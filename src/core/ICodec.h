#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace icu {

// 编解码层：只管 消息 <-> 字节流 的转换
class ICodec {
public:
    virtual ~ICodec() = default;

    // 上层消息 → 字节流
    virtual bool Encode(const std::vector<uint8_t>& message, std::vector<uint8_t>& out) = 0;

    // 字节流 → 上层消息（可能一次产出多条）
    // 实现要自己维护接收缓冲区，处理半包 / 粘包
    using MessageCallback = std::function<void(const std::vector<uint8_t>&)>;
    virtual void Feed(const uint8_t* data, size_t len, MessageCallback onMessage) = 0;

    // 复位缓冲区（重连时调用）
    virtual void Reset() = 0;

    // 用于 UI 显示的名字
    virtual std::string Name() const = 0;
};

using ICodecPtr = std::unique_ptr<ICodec>;

}  // namespace icu
