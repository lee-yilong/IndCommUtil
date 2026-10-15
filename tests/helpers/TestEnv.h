#pragma once

// =========================================================================
// TestEnv —— 测试用临时目录
//
// 构造时创建一个唯一命名的临时目录，析构时递归删除。
// 临时根优先用当前工作目录（ctest 下是 build/tests/，一定可写），
// 避免系统 %TEMP% 在受限环境下不可写（如 C:\msys64\tmp）。
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
        std::error_code ec;
        fs::create_directories(m_dir, ec);
        if (ec) {
            // 建不出来就让 m_dir 留空，测试用例会自然失败并报错
            m_dir.clear();
        }
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
    // 临时根：优先当前工作目录，兜底系统 temp
    static std::filesystem::path TempRoot() {
        namespace fs = std::filesystem;
        std::error_code ec;

        const fs::path cwd = fs::current_path(ec);
        if (!ec && !cwd.empty()) {
            return cwd;
        }

        ec.clear();
        return fs::temp_directory_path(ec);
    }

    std::filesystem::path m_dir;
};

}  // namespace test
}  // namespace icu
