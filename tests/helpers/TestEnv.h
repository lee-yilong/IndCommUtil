#pragma once

// =========================================================================
// TestEnv —— 测试用临时目录
//
// 构造时创建一个唯一命名的临时目录，析构时递归删除。
// =========================================================================

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

namespace icu {
namespace test {

class TestEnv {
public:
    TestEnv() {
        namespace fs = std::filesystem;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_dir = TempRoot() / ("icu_test_" + std::to_string(stamp));
        fs::create_directories(m_dir);
    }

    ~TestEnv() {
        std::error_code ec;
        std::filesystem::remove_all(m_dir, ec);
    }

    TestEnv(const TestEnv&) = delete;
    TestEnv& operator=(const TestEnv&) = delete;

    std::string Path(const std::string& name) const { return (m_dir / name).string(); }

    std::string Dir() const { return m_dir.string(); }

private:
    // 系统临时目录：TEMP/TMP 无效时逐个回退，保证测试在任何环境下都能跑
    static std::filesystem::path TempRoot() {
        namespace fs = std::filesystem;
        std::error_code ec;

        const fs::path systemTemp = fs::temp_directory_path(ec);
        if (!ec && !systemTemp.empty()) {
            return systemTemp;
        }

        for (const char* var : {"TEMP", "TMP"}) {
            const char* value = std::getenv(var);
            if (value == nullptr || *value == '\0') {
                continue;
            }
            const fs::path candidate(value);
            if (fs::is_directory(candidate, ec) && !ec) {
                return candidate;
            }
            ec.clear();
        }

        return fs::current_path(ec);
    }

    std::filesystem::path m_dir;
};

}  // namespace test
}  // namespace icu
