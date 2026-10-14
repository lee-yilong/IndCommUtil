#pragma once

#include <wx/string.h>

#include <map>
#include <string>

#include "core/ProtocolTypes.h"
#include "core/IProtocol.h"

namespace icu {

// .conn 文件读写（协议无关）
class ConnectionProfile {
public:
    // 完整加载：自动从 ProtocolRegistry 查协议
    static bool Load(const wxString& path, ActiveProfile& out);

    // 保存
    static bool Save(const wxString& path, const std::string& protocolId, int modeIndex,
                     IProtocol* protocol, const std::map<std::string, FieldValue>& fields);

    // 默认文件名（委托给 IProtocol::MakeDefaultFileName）
    static wxString MakeDefaultFileName(IProtocol* protocol, int modeIndex,
                                        const std::map<std::string, FieldValue>& fields);

private:
    static bool ReadHeader(const wxString& path, std::string& protocolId, std::string& modeName);

    static bool LoadFields(const wxString& path, IProtocol* protocol, int modeIndex,
                           std::map<std::string, FieldValue>& fields);
};

}  // namespace icu
