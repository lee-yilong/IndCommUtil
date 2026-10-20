#include "Database.h"

#include <sqlite3.h>

#include <chrono>
#include <iostream>
#include <vector>

#include "log/Log.h"
#include "spdlog/spdlog.h"

namespace icu {

Database::~Database() {
    Close();
}

bool Database::Open(const std::string& dbPath) {
    Close();
    m_lastError.clear();

    int rc = sqlite3_open(dbPath.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        ICU_SPDLOG_ERROR(_("Failed to open database: %s"), wxString::FromUTF8(m_lastError));
        return false;
    }

    char* errMsg = nullptr;
    const char* pragmas = "PRAGMA foreign_keys = ON; PRAGMA journal_mode = WAL;";
    sqlite3_exec(m_db, pragmas, nullptr, nullptr, &errMsg);
    if (errMsg) sqlite3_free(errMsg);

    return CreateTables();
}

void Database::Close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool Database::CreateTables() {
    const char* sqlRuns = R"(
        CREATE TABLE IF NOT EXISTS test_runs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            project_path TEXT,
            start_time DATETIME,
            status TEXT
        );
    )";

    // ★ 增加 time_sec REAL 字段用于高精度绘图
    const char* sqlData = R"(
        CREATE TABLE IF NOT EXISTS run_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id INTEGER,
            timestamp DATETIME,
            time_sec REAL,
            variable TEXT,
            value REAL,
            FOREIGN KEY(run_id) REFERENCES test_runs(id)
        );
    )";

    char* errMsg = nullptr;
    if (sqlite3_exec(m_db, sqlRuns, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        m_lastError = errMsg ? errMsg : "Unknown error";
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(m_db, sqlData, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        m_lastError = errMsg ? errMsg : "Unknown error";
        sqlite3_free(errMsg);
        return false;
    }

    // 兼容旧表：尝试添加 time_sec 列，如果已存在会报错，直接忽略
    sqlite3_exec(m_db, "ALTER TABLE run_data ADD COLUMN time_sec REAL;", nullptr, nullptr, nullptr);

    return true;
}

int64_t Database::CreateTestRun(const std::string& projectPath) {
    if (!m_db) return -1;

    const char* sql =
        "INSERT INTO test_runs (project_path, start_time, status) VALUES (?, datetime('now', "
        "'localtime'), 'RUNNING');";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, projectPath.c_str(), -1, SQLITE_TRANSIENT);

    int64_t runId = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        runId = sqlite3_last_insert_rowid(m_db);
    } else {
        m_lastError = sqlite3_errmsg(m_db);
    }

    sqlite3_finalize(stmt);
    return runId;
}

void Database::UpdateTestRunStatus(int64_t runId, const std::string& status) {
    if (!m_db || runId < 0) return;

    const char* sql = "UPDATE test_runs SET status = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, runId);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

bool Database::InsertRunData(int64_t runId, const std::string& variableName, double value,
                             double timeSec) {
    if (!m_db || runId < 0) return false;

    const char* sql =
        "INSERT INTO run_data (run_id, timestamp, time_sec, variable, value) VALUES (?, "
        "datetime('now', 'localtime'), ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        return false;
    }

    sqlite3_bind_int64(stmt, 1, runId);
    sqlite3_bind_double(stmt, 2, timeSec);
    sqlite3_bind_text(stmt, 3, variableName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, value);

    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        ok = true;
    } else {
        m_lastError = sqlite3_errmsg(m_db);
    }

    sqlite3_finalize(stmt);
    return ok;
}

std::vector<DataRecord> Database::QueryHistory(const std::string& projectPath, double startTimeSec,
                                               double endTimeSec) {
    std::vector<DataRecord> records;
    if (!m_db) return records;

    const char* sql = R"(
        SELECT rd.time_sec, rd.variable, rd.value
        FROM run_data rd
        JOIN test_runs tr ON rd.run_id = tr.id
        WHERE tr.project_path = ?
          AND rd.time_sec >= ?
          AND rd.time_sec <= ?
        ORDER BY rd.time_sec ASC;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        return records;
    }

    sqlite3_bind_text(stmt, 1, projectPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, startTimeSec);
    sqlite3_bind_double(stmt, 3, endTimeSec);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DataRecord rec;
        rec.timeSec = sqlite3_column_double(stmt, 0);
        const unsigned char* varName = sqlite3_column_text(stmt, 1);
        rec.varName = varName ? reinterpret_cast<const char*>(varName) : "";
        rec.value = sqlite3_column_double(stmt, 2);
        records.push_back(rec);
    }

    sqlite3_finalize(stmt);
    return records;
}

}  // namespace icu
