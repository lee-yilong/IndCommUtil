#pragma once

// =========================================================================
// TestEnv —— 测试用临时目录
//
// 构造时创建一个唯一命名的临时目录，析构时递归删除。
// =========================================================================

#include <chrono>
#include <filesystem>
#include <string>

namespace icu {
namespace test {

class TestEnv {
public:
    TestEnv() {
        namespace fs = std::filesystem;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_dir = fs::temp_directory_path() / ("icu_test_" + std::to_string(stamp));
        fs::create_directories(m_dir);
    }

    ~TestEnv() {
        std::error_code ec;
        std::filesystem::remove_all(m_dir, ec);
    }

    TestEnv(const TestEnv&) = delete;
    TestEnv& operator=(const TestEnv&) = delete;

    std::string Path(const std::string& name) const {
        return (m_dir / name).string();
    }

    std::string Dir() const { return m_dir.string(); }

private:
    std::filesystem::path m_dir;
};

}  // namespace test
}  // namespace icu
