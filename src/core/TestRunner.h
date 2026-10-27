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

enum class StepStatus { Running, Passed, Failed, Skipped };

// 步骤失败后由 UI 决定：重试当前步骤 / 跳过 / 结束流程
enum class StepFailureAction { Retry, Skip, Stop };

class TestRunner {
public:
    TestRunner(IProtocol* protocol, ISession* session, std::vector<ITestStepPtr> steps,
               std::map<std::string, CommVariable>& variables);
    ~TestRunner();

    using StepUpdateCallback =
        std::function<void(int stepIndex, StepStatus status, const std::string& msg)>;
    using FinishCallback = std::function<void(bool overallSuccess)>;
    using WaitCallback = std::function<void(int nextIndex)>;
    using FailureCallback = std::function<StepFailureAction(int stepIndex, const std::string& msg)>;

    // 阶段 10：底层报文日志回调
    using StepLogCallback = std::function<void(const std::string& logMsg)>;
    // 阶段 11：数据记录回调（用于 SQLite 与图表）
    using DataRecordedCallback =
        std::function<void(const std::string& varName, const std::string& value)>;

    void OnStepUpdated(StepUpdateCallback cb) { m_stepCb = std::move(cb); }
    void OnFinished(FinishCallback cb) { m_finishCb = std::move(cb); }
    void OnWaitForNextStep(WaitCallback cb) { m_waitCb = std::move(cb); }
    void OnStepFailed(FailureCallback cb) { m_failCb = std::move(cb); }
    void OnStepLog(StepLogCallback cb) { m_stepLogCb = std::move(cb); }
    void OnDataRecorded(DataRecordedCallback cb) { m_dataRecordCb = std::move(cb); }

    void SetRunMode(TestRunMode mode) { m_runMode = mode; }
    void Start();
    void Stop();
    void Continue();
    void Pause();
    void Resume();
    void JumpToStep(int index);
    bool IsPaused() const { return m_pauseFlag.load(); }
    int CurrentStepIndex() const { return m_currentIndex.load(); }
    void WaitForCompletion();
    bool IsRunning() const { return m_thread.joinable(); }

private:
    void RunLoop();
    void WaitIfPaused();
    // 分段等待，保证 Stop() 能及时退出
    void WaitWithStop(int ms);

    IProtocol* m_protocol;
    ISession* m_session;
    std::vector<ITestStepPtr> m_steps;
    std::map<std::string, CommVariable>& m_vars;

    std::thread m_thread;
    std::atomic<bool> m_stopFlag{false};
    std::atomic<bool> m_pauseFlag{false};
    std::atomic<int> m_jumpRequest{-1};
    std::atomic<int> m_currentIndex{-1};
    TestRunMode m_runMode = TestRunMode::Auto;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::condition_variable m_pauseCv;
    bool m_waitingForContinue = false;

    StepUpdateCallback m_stepCb;
    FinishCallback m_finishCb;
    WaitCallback m_waitCb;
    FailureCallback m_failCb;
    StepLogCallback m_stepLogCb;
    DataRecordedCallback m_dataRecordCb;
};

}  // namespace icu
