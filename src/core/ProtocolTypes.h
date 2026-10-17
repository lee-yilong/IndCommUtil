#pragma once

// =========================================================================
// 协议元数据类型
//   FieldValue       —— 单个字段的值（String / Int / Bool / Choice）
//   FieldDescriptor  —— 字段描述（UI 自动生成用）
//   ActiveProfile    —— 一个"活动配置"：协议 ID + 模式 + 字段值
// =========================================================================

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace icu {

// -------------------------------------------------------------------------
// FieldValue
// -------------------------------------------------------------------------
class FieldValue {
public:
    enum class Type { String, Int, Bool, Choice };

    FieldValue() = default;
    FieldValue(const std::string& s);
    FieldValue(const char* s);
    FieldValue(int v);
    FieldValue(bool v);

    Type GetType() const { return m_type; }
    std::string AsString() const { return m_str; }
    int AsInt() const { return m_int; }
    bool AsBool() const { return m_bool; }
    std::string AsChoice() const { return m_str; }

    std::string Serialize() const;
    static FieldValue Deserialize(const std::string& s, Type t);

    bool operator==(const FieldValue& o) const {
        if (m_type != o.m_type) return false;
        switch (m_type) {
            case Type::String:
            case Type::Choice:
                return m_str == o.m_str;
            case Type::Int:
                return m_int == o.m_int;
            case Type::Bool:
                return m_bool == o.m_bool;
        }
        return false;
    }
    bool operator!=(const FieldValue& o) const { return !(*this == o); }

private:
    // 私有构造：供 Deserialize 等内部逻辑精确设置类型
    FieldValue(Type t, const std::string& s) : m_type(t), m_str(s) {}

    Type m_type = Type::String;
    std::string m_str;
    int m_int = 0;
    bool m_bool = false;
};

// -------------------------------------------------------------------------
// FieldDescriptor
// -------------------------------------------------------------------------
struct FieldDescriptor {
    std::string key;
    std::string label;  // 翻译 key
    FieldValue::Type type = FieldValue::Type::String;
    FieldValue defaultValue;

    int minValue = 0;
    int maxValue = 100;

    std::vector<std::pair<std::string, std::string>> choices;
    //                                                {value, label}

    // 新增：分组名（如果为空，则默认平铺）
    std::string group;
};

// -------------------------------------------------------------------------
// ActiveProfile
// -------------------------------------------------------------------------
struct ActiveProfile {
    std::string protocolId;
    int modeIndex = 0;
    std::map<std::string, FieldValue> fields;

    void Clear() {
        protocolId.clear();
        modeIndex = 0;
        fields.clear();
    }

    bool Empty() const { return protocolId.empty(); }

    bool operator==(const ActiveProfile& o) const {
        return protocolId == o.protocolId && modeIndex == o.modeIndex && fields == o.fields;
    }
    bool operator!=(const ActiveProfile& o) const { return !(*this == o); }
};

}  // namespace icu
