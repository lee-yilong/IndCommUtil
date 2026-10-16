#include "ConnectionProfile.h"

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/log.h>

#include <nlohmann/json.hpp>

#include "core/ProtocolRegistry.h"
#include "log/Log.h"
#include "spdlog/spdlog.h"

namespace icu {

namespace {

using json = nlohmann::json;

// 把 JSON 字段值转成 FieldValue::Deserialize 需要的字符串
std::string JsonToStr(const json& v) {
    if (v.is_string()) return v.get<std::string>();
    if (v.is_number_integer()) return std::to_string(v.get<long long>());
    if (v.is_number_unsigned()) return std::to_string(v.get<unsigned long long>());
    if (v.is_boolean()) return v.get<bool>() ? "1" : "0";
    return {};
}

// 按 FieldDescriptor 声明的类型，把 FieldValue 写进 JSON
void WriteField(json& obj, const std::string& key, const FieldValue& v, FieldValue::Type type) {
    switch (type) {
        case FieldValue::Type::String:
        case FieldValue::Type::Choice:
            obj[key] = v.Serialize();
            break;
        case FieldValue::Type::Int: {
            try {
                obj[key] = std::stoi(v.Serialize());
            } catch (...) {
                obj[key] = 0;
            }
            break;
        }
        case FieldValue::Type::Bool: {
            const std::string s = v.Serialize();
            obj[key] = (s == "1" || s == "true" || s == "yes");
            break;
        }
    }
}

}  // namespace

bool ConnectionProfile::Load(const wxString& path, ActiveProfile& out) {
    wxLogNull noLog;

    if (!wxFileName::FileExists(path)) return false;

    wxFile f;
    if (!f.Open(path, wxFile::read)) return false;

    const wxFileOffset len = f.Length();
    if (len <= 0) {
        f.Close();
        return false;
    }

    const size_t need = static_cast<size_t>(len);
    std::string buf(need, '\0');
    const size_t got = f.Read(buf.data(), need);
    f.Close();

    if (got != need) return false;

    json j;
    try {
        j = json::parse(buf);
    } catch (const json::parse_error& e) {
        ICU_SPDLOG_WARN(_("Failed to parse JSON: %s"), wxString::FromUTF8(e.what()));
        return false;
    }

    if (!j.is_object() || !j.contains("connection") || !j["connection"].is_object()) {
        return false;
    }

    const auto& conn = j["connection"];
    if (!conn.contains("protocol") || !conn["protocol"].is_string()) return false;

    const std::string protocolId = conn["protocol"].get<std::string>();
    if (protocolId.empty()) return false;

    auto* proto = ProtocolRegistry::Instance().Find(protocolId);
    if (!proto) return false;

    int modeIndex = 0;
    if (conn.contains("mode") && conn["mode"].is_string()) {
        const std::string modeName = conn["mode"].get<std::string>();
        const auto modes = proto->ModeNames();
        for (size_t i = 0; i < modes.size(); ++i) {
            if (modes[i] == modeName) {
                modeIndex = static_cast<int>(i);
                break;
            }
        }
    }

    std::map<std::string, FieldValue> fields;
    for (const auto& fd : proto->FieldsForMode(modeIndex)) {
        if (!conn.contains(fd.key)) continue;
        const std::string s = JsonToStr(conn[fd.key]);
        if (s.empty() && !conn[fd.key].is_string()) continue;
        fields[fd.key] = FieldValue::Deserialize(s, fd.type);
    }

    out.protocolId = protocolId;
    out.modeIndex = modeIndex;
    out.fields = std::move(fields);
    return true;
}

bool ConnectionProfile::Save(const wxString& path, const std::string& protocolId, int modeIndex,
                             IProtocol* protocol, const std::map<std::string, FieldValue>& fields) {
    if (!protocol) return false;
    if (path.IsEmpty()) return false;

    wxLogNull noLog;

    // 确保目标目录存在
    wxFileName fn(path);
    const wxString dir = fn.GetPath();
    if (!dir.IsEmpty() && !wxFileName::DirExists(dir)) {
        if (!wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
            return false;
        }
    }

    json j;
    j["connection"] = json::object();
    j["connection"]["protocol"] = protocolId;

    const auto modes = protocol->ModeNames();
    if (modeIndex >= 0 && modeIndex < static_cast<int>(modes.size())) {
        j["connection"]["mode"] = modes[modeIndex];
    }

    for (const auto& fd : protocol->FieldsForMode(modeIndex)) {
        auto it = fields.find(fd.key);
        if (it == fields.end()) continue;
        WriteField(j["connection"], fd.key, it->second, fd.type);
    }

    const std::string text = j.dump(2);  // 2 空格缩进

    wxFile f;
    if (!f.Create(path, true)) return false;
    const size_t written = f.Write(text.data(), text.size());
    f.Close();

    return written == text.size();
}

wxString ConnectionProfile::MakeDefaultFileName(IProtocol* protocol, int modeIndex,
                                                const std::map<std::string, FieldValue>& fields) {
    if (!protocol) return "Untitled.json";
    std::string name = protocol->MakeDefaultFileName(modeIndex, fields);
    if (name.empty()) name = "Untitled.json";
    return wxString::FromUTF8(name);
}

}  // namespace icu
