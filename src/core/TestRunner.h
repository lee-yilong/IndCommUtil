#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ITestStep.h"
#include "SessionConfig.h"

namespace icu {

class IProtocol;
class ISession;

enum class TestRunMode { Auto, SingleStep };

class TestRunner {
public:
    TestRunner(IProtocol* protocol, ISession* session, std::vector<ITestStepPtr> steps,
               std::map<std::string, CommVariable>& variables);
    ~TestRunner();

    using StepUpdateCallback =
        std::function<void(int stepIndex, bool isSuccess, const std::string& msg)>;
    using FinishCallback = std::function<void(bool overallSuccess)>;
    using WaitCallback = std::function<void(int nextIndex)>;

    // 阶段 10：底层报文日志回调
    using StepLogCallback = std::function<void(const std::string& logMsg)>;
    // 阶段 11：数据记录回调（用于 SQLite 与图表）
    using DataRecordedCallback = std::function<void(const std::string& varName, double value)>;

    void OnStepUpdated(StepUpdateCallback cb) { m_stepCb = std::move(cb); }
    void OnFinished(FinishCallback cb) { m_finishCb = std::move(cb); }
    void OnWaitForNextStep(WaitCallback cb) { m_waitCb = std::move(cb); }
    void OnStepLog(StepLogCallback cb) { m_stepLogCb = std::move(cb); }
    void OnDataRecorded(DataRecordedCallback cb) { m_dataRecordCb = std::move(cb); }

    void SetRunMode(TestRunMode mode) { m_runMode = mode; }
    void Start();
    void Stop();
    void Continue();
    void WaitForCompletion();
    bool IsRunning() const { return m_thread.joinable(); }

private:
    void RunLoop();

    IProtocol* m_protocol;
    ISession* m_session;
    std::vector<ITestStepPtr> m_steps;
    std::map<std::string, CommVariable>& m_vars;

    std::thread m_thread;
    std::atomic<bool> m_stopFlag{false};
    TestRunMode m_runMode = TestRunMode::Auto;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_waitingForContinue = false;

    StepUpdateCallback m_stepCb;
    FinishCallback m_finishCb;
    WaitCallback m_waitCb;
    StepLogCallback m_stepLogCb;
    DataRecordedCallback m_dataRecordCb;
};

}  // namespace icu
