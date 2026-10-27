#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

#include "core/Database.h"
#include "doctest.h"

using namespace icu;

TEST_CASE("Database: 历史记录按项目和编号查询") {
    const auto path = (std::filesystem::temp_directory_path() / "icu_history_test.db").string();
    std::filesystem::remove(path);

    Database db;
    REQUIRE(db.Open(path));

    auto now = std::chrono::system_clock::now();
    const double timeSec = std::chrono::duration<double>(now.time_since_epoch()).count();

    const int64_t runId = db.CreateTestRun("Untitled");
    REQUIRE(runId >= 0);
    REQUIRE(db.InsertRunData(runId, "new_var", 123.0, timeSec));

    auto records = db.QueryHistory("Untitled", timeSec - 1.0, timeSec + 1.0);
    REQUIRE(records.size() == 1);
    CHECK(records[0].varName == "new_var");
    CHECK(records[0].value == doctest::Approx(123.0));
    CHECK(records[0].timeSec == doctest::Approx(timeSec));

    // 未保存的会话统一使用 Untitled，而不是空字符串；空 key 不应命中
    CHECK(db.QueryHistory("", timeSec - 1.0, timeSec + 1.0).empty());

    db.Close();
    std::filesystem::remove(path);
}
