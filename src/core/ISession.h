#pragma once

#include <functional>
#include <memory>
#include <string>

namespace icu {

enum class SessionState { Idle, Connecting, Reconnecting, Connected, Disconnected };

class ISession {
public:
    virtual ~ISession() = default;

    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual SessionState State() const = 0;
    virtual bool IsConnected() const = 0;
    virtual std::string Target() const = 0;
    virtual std::string TransportName() const = 0;

    // 上提到基类，方便 UI 统一判断
    virtual bool IsServerMode() const { return false; }

    using StateCallback = std::function<void(SessionState)>;
    virtual void OnStateChanged(StateCallback cb) = 0;

    virtual void DetachAllCallbacks() = 0;
};

using ISessionPtr = std::unique_ptr<ISession>;

}  // namespace icu
