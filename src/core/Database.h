#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct sqlite3;

namespace icu {

struct DataRecord {
    std::string varName;
    double value;
    double timeSec;
};

struct TextDataRecord {
    std::string varName;
    std::string value;
    double timeSec;
};

class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool Open(const std::string& dbPath);
    void Close();

    int64_t CreateTestRun(const std::string& projectPath);
    void UpdateTestRunStatus(int64_t runId, const std::string& status);

    // 插入时记录毫秒时间戳
    bool InsertRunData(int64_t runId, const std::string& variableName, double value,
                       double timeSec);
    bool InsertRunTextData(int64_t runId, const std::string& variableName, const std::string& value,
                           double timeSec);

    // ★ 新增：查询指定项目、时间段的历史数据
    std::vector<DataRecord> QueryHistory(const std::string& projectPath, double startTimeSec,
                                         double endTimeSec);
    std::vector<TextDataRecord> QueryTextHistory(const std::string& projectPath,
                                                 double startTimeSec, double endTimeSec);

    std::string LastError() const { return m_lastError; }

private:
    bool CreateTables();

    sqlite3* m_db = nullptr;
    std::string m_lastError;
};

}  // namespace icu
