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

std::string JsonToStr(const json& v) {
    if (v.is_string()) return v.get<std::string>();
    if (v.is_number_integer()) return std::to_string(v.get<long long>());
    if (v.is_number_unsigned()) return std::to_string(v.get<unsigned long long>());
    if (v.is_boolean()) return v.get<bool>() ? "1" : "0";
    return {};
}

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

void ParseSession(const json& j, SessionConfig& outSession) {
    if (!j.contains("session") || !j["session"].is_object()) return;
    const auto& s = j["session"];

    if (s.contains("active_mode") && s["active_mode"].is_string()) {
        std::string mode = s["active_mode"].get<std::string>();
        if (mode == "manual")
            outSession.activeMode = SessionMode::Manual;
        else if (mode == "interactive")
            outSession.activeMode = SessionMode::Interactive;
        else if (mode == "auto")
            outSession.activeMode = SessionMode::Auto;
    }

    if (s.contains("variables") && s["variables"].is_object()) {
        for (auto& [key, v] : s["variables"].items()) {
            CommVariable var;
            var.name = key;
            var.type = v.value("type", "string");
            var.access = v.value("access", "RO");
            if (v.contains("params")) var.params = v["params"];
            var.enabled = v.value("enabled", true);
            outSession.variables[key] = var;
        }
    }

    if (s.contains("manual_mode") && s["manual_mode"].is_object()) {
        outSession.manualMode.sendPayload = s["manual_mode"].value("send_payload", "");
        outSession.manualMode.appendNewline = s["manual_mode"].value("append_newline", true);
    }

    if (s.contains("interactive_mode") && s["interactive_mode"].is_object()) {
        const auto& m = s["interactive_mode"];
        outSession.interactiveMode.currentStepIndex = m.value("current_step_index", 0);
        outSession.interactiveMode.startStepIndex = m.value("start_step_index", 0);
        outSession.interactiveMode.endStepIndex = m.value("end_step_index", 0);
        if (m.contains("test_flow") && m["test_flow"].is_array()) {
            outSession.interactiveMode.testFlow = m["test_flow"];
        }
    }

    if (s.contains("auto_mode") && s["auto_mode"].is_object()) {
        const auto& m = s["auto_mode"];
        outSession.autoMode.startStepIndex = m.value("start_step_index", 0);
        outSession.autoMode.endStepIndex = m.value("end_step_index", 0);
        if (m.contains("test_flow") && m["test_flow"].is_array()) {
            outSession.autoMode.testFlow = m["test_flow"];
        }
    }
}

void WriteSession(json& j, const SessionConfig& session) {
    j["session"] = json::object();
    auto& s = j["session"];

    if (session.activeMode == SessionMode::Manual)
        s["active_mode"] = "manual";
    else if (session.activeMode == SessionMode::Interactive)
        s["active_mode"] = "interactive";
    else if (session.activeMode == SessionMode::Auto)
        s["active_mode"] = "auto";

    s["variables"] = json::object();
    for (const auto& [key, var] : session.variables) {
        json v;
        v["type"] = var.type;
        v["access"] = var.access;
        v["params"] = var.params;
        v["enabled"] = var.enabled;
        s["variables"][key] = v;
    }

    s["manual_mode"] = json::object();
    s["manual_mode"]["send_payload"] = session.manualMode.sendPayload;
    s["manual_mode"]["append_newline"] = session.manualMode.appendNewline;

    s["interactive_mode"] = json::object();
    s["interactive_mode"]["current_step_index"] = session.interactiveMode.currentStepIndex;
    s["interactive_mode"]["start_step_index"] = session.interactiveMode.startStepIndex;
    s["interactive_mode"]["end_step_index"] = session.interactiveMode.endStepIndex;
    s["interactive_mode"]["test_flow"] = session.interactiveMode.testFlow;

    s["auto_mode"] = json::object();
    s["auto_mode"]["start_step_index"] = session.autoMode.startStepIndex;
    s["auto_mode"]["end_step_index"] = session.autoMode.endStepIndex;
    s["auto_mode"]["test_flow"] = session.autoMode.testFlow;
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

    std::string buf(static_cast<size_t>(len), '\0');
    const size_t got = f.Read(buf.data(), static_cast<size_t>(len));
    f.Close();
    if (got != static_cast<size_t>(len)) return false;

    json j;
    try {
        j = json::parse(buf);
    } catch (...) {
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
    std::string modeName = "";
    if (conn.contains("mode") && conn["mode"].is_string()) {
        modeName = conn["mode"].get<std::string>();
        const auto modes = proto->ModeNames();
        for (size_t i = 0; i < modes.size(); ++i) {
            if (modes[i] == modeName) {
                modeIndex = static_cast<int>(i);
                break;
            }
        }
    }

    std::map<std::string, FieldValue> fields;

    // 1. 兼容旧版：读取 connection 下平铺的字段
    for (const auto& fd : proto->FieldsForMode(modeIndex)) {
        if (conn.contains(fd.key)) {
            const std::string s = JsonToStr(conn[fd.key]);
            if (!s.empty() || conn[fd.key].is_string()) {
                fields[fd.key] = FieldValue::Deserialize(s, fd.type);
            }
        }
    }

    // 2. 读取新版：分层结构 connection[ModeName][key]
    if (!modeName.empty() && conn.contains(modeName) && conn[modeName].is_object()) {
        const auto& modeObj = conn[modeName];
        for (const auto& fd : proto->FieldsForMode(modeIndex)) {
            if (modeObj.contains(fd.key)) {
                const std::string s = JsonToStr(modeObj[fd.key]);
                if (!s.empty() || modeObj[fd.key].is_string()) {
                    fields[fd.key] = FieldValue::Deserialize(s, fd.type);
                }
            }
        }
    }

    out.protocolId = protocolId;
    out.modeIndex = modeIndex;
    out.fields = std::move(fields);

    out.session = SessionConfig{};
    ParseSession(j, out.session);

    return true;
}

bool ConnectionProfile::Save(const wxString& path, const std::string& protocolId, int modeIndex,
                             IProtocol* protocol, const std::map<std::string, FieldValue>& fields,
                             const SessionConfig& session) {
    if (!protocol || path.IsEmpty()) return false;

    wxLogNull noLog;
    wxFileName fn(path);
    const wxString dir = fn.GetPath();
    if (!dir.IsEmpty() && !wxFileName::DirExists(dir)) {
        if (!wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) return false;
    }

    json j;
    j["connection"] = json::object();
    j["connection"]["protocol"] = protocolId;

    const auto modes = protocol->ModeNames();
    std::string modeName = "";
    if (modeIndex >= 0 && modeIndex < static_cast<int>(modes.size())) {
        modeName = modes[modeIndex];
        j["connection"]["mode"] = modeName;
    }

    // ★ 新版分层结构写入
    if (!modeName.empty()) {
        json modeParams = json::object();
        for (const auto& fd : protocol->FieldsForMode(modeIndex)) {
            auto it = fields.find(fd.key);
            if (it == fields.end()) continue;
            WriteField(modeParams, fd.key, it->second, fd.type);
        }
        j["connection"][modeName] = modeParams;
    } else {
        // 兜底：单模式无名称的协议直接平铺
        for (const auto& fd : protocol->FieldsForMode(modeIndex)) {
            auto it = fields.find(fd.key);
            if (it == fields.end()) continue;
            WriteField(j["connection"], fd.key, it->second, fd.type);
        }
    }

    WriteSession(j, session);

    const std::string text = j.dump(2);

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
