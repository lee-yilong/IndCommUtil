#include "ConnectionProfile.h"

#include <wx/fileconf.h>
#include <wx/filename.h>

#include "core/ProtocolRegistry.h"

namespace icu {

bool ConnectionProfile::ReadHeader(const wxString& path, std::string& protocolId,
                                   std::string& modeName) {
    if (!wxFileName::FileExists(path)) return false;

    wxFileConfig fc(wxEmptyString, wxEmptyString, path, wxEmptyString, wxCONFIG_USE_LOCAL_FILE);

    wxString proto, mode;
    fc.Read("/Connection/Protocol", &proto, "");
    fc.Read("/Connection/Mode", &mode, "");

    protocolId = std::string(proto.utf8_str());
    modeName = std::string(mode.utf8_str());
    return !protocolId.empty();
}

bool ConnectionProfile::LoadFields(const wxString& path, IProtocol* protocol, int modeIndex,
                                   std::map<std::string, FieldValue>& fields) {
    if (!protocol) return false;
    if (!wxFileName::FileExists(path)) return false;

    wxFileConfig fc(wxEmptyString, wxEmptyString, path, wxEmptyString, wxCONFIG_USE_LOCAL_FILE);

    auto descriptors = protocol->FieldsForMode(modeIndex);
    for (const auto& fd : descriptors) {
        const wxString key = "/Connection/" + wxString::FromUTF8(fd.key);
        wxString value;
        if (!fc.Read(key, &value)) continue;

        fields[fd.key] = FieldValue::Deserialize(std::string(value.utf8_str()), fd.type);
    }
    return true;
}

bool ConnectionProfile::Load(const wxString& path, ActiveProfile& out) {
    std::string protocolId, modeName;
    if (!ReadHeader(path, protocolId, modeName)) return false;

    auto* proto = ProtocolRegistry::Instance().Find(protocolId);
    if (!proto) return false;

    // 查找 modeIndex
    int modeIndex = 0;
    auto modes = proto->ModeNames();
    for (size_t i = 0; i < modes.size(); ++i) {
        if (modes[i] == modeName) {
            modeIndex = (int)i;
            break;
        }
    }

    out.protocolId = protocolId;
    out.modeIndex = modeIndex;
    out.fields.clear();
    return LoadFields(path, proto, modeIndex, out.fields);
}

bool ConnectionProfile::Save(const wxString& path, const std::string& protocolId, int modeIndex,
                             IProtocol* protocol, const std::map<std::string, FieldValue>& fields) {
    if (!protocol) return false;

    wxFileConfig fc(wxEmptyString, wxEmptyString, path, wxEmptyString, wxCONFIG_USE_LOCAL_FILE);

    fc.Write("/Connection/Protocol", wxString::FromUTF8(protocolId));

    auto modes = protocol->ModeNames();
    if (modeIndex >= 0 && modeIndex < (int)modes.size())
        fc.Write("/Connection/Mode", wxString::FromUTF8(modes[modeIndex]));

    auto descriptors = protocol->FieldsForMode(modeIndex);
    for (const auto& fd : descriptors) {
        auto it = fields.find(fd.key);
        if (it == fields.end()) continue;

        fc.Write("/Connection/" + wxString::FromUTF8(fd.key),
                 wxString::FromUTF8(it->second.Serialize()));
    }

    return fc.Flush();
}

wxString ConnectionProfile::MakeDefaultFileName(IProtocol* protocol, int modeIndex,
                                                const std::map<std::string, FieldValue>& fields) {
    if (!protocol) return "Untitled.conn";
    std::string name = protocol->MakeDefaultFileName(modeIndex, fields);
    if (name.empty()) name = "Untitled.conn";
    return wxString::FromUTF8(name);
}

}  // namespace icu
