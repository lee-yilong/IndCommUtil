#include "ProtocolTypes.h"

namespace icu {

FieldValue::FieldValue(const std::string& s) : m_type(Type::String), m_str(s) {}

FieldValue::FieldValue(const char* s) : m_type(Type::String), m_str(s ? s : "") {}

FieldValue::FieldValue(int v) : m_type(Type::Int), m_int(v) {}

FieldValue::FieldValue(bool v) : m_type(Type::Bool), m_bool(v) {}

std::string FieldValue::Serialize() const {
    switch (m_type) {
        case Type::String:
            return m_str;
        case Type::Int:
            return std::to_string(m_int);
        case Type::Bool:
            return m_bool ? "1" : "0";
        case Type::Choice:
            return m_str;
    }
    return "";
}

FieldValue FieldValue::Deserialize(const std::string& s, Type t) {
    switch (t) {
        case Type::String:
            return FieldValue(s);
        case Type::Int: {
            try {
                return FieldValue(std::stoi(s));
            } catch (...) {
                return FieldValue(0);
            }
        }
        case Type::Bool:
            return FieldValue(s == "1" || s == "true" || s == "yes");
        case Type::Choice:
            return FieldValue(Type::Choice, s);
    }
    return FieldValue();
}

}  // namespace icu
