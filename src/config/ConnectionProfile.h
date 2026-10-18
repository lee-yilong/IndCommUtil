#pragma once

#include <wx/string.h>

#include <map>
#include <string>

#include "core/IProtocol.h"
#include "core/ProtocolTypes.h"
#include "core/SessionConfig.h"  // <--- 确保用 core/ 前缀

namespace icu {

// .json 文件读写（协议无关）
class ConnectionProfile {
public:
    // 完整加载：自动从 ProtocolRegistry 查协议
    static bool Load(const wxString& path, ActiveProfile& out);

    // 保存
    static bool Save(const wxString& path, const std::string& protocolId, int modeIndex,
                     IProtocol* protocol, const std::map<std::string, FieldValue>& fields,
                     const SessionConfig& session);  // <--- 增加了 SessionConfig 参数

    // 默认文件名（委托给 IProtocol::MakeDefaultFileName）
    static wxString MakeDefaultFileName(IProtocol* protocol, int modeIndex,
                                        const std::map<std::string, FieldValue>& fields);
};

}  // namespace icu
