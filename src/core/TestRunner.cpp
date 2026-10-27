#include "TestRunner.h"

#include <chrono>

#include "IProtocol.h"
#include "log/Log.h"
#include "spdlog/spdlog.h"

namespace icu {

TestRunner::TestRunner(IProtocol* protocol, ISession* session, std::vector<ITestStepPtr> steps,
                       std::map<std::string, CommVariable>& variables)
    : m_protocol(protocol), m_session(session), m_steps(std::move(steps)), m_vars(variables) {}

TestRunner::~TestRunner() {
    Stop();
}

void TestRunner::Start() {
    if (m_thread.joinable()) return;
    m_stopFlag = false;
    m_thread = std::thread(&TestRunner::RunLoop, this);
}

void TestRunner::Stop() {
    m_stopFlag = true;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_waitingForContinue = false;
    }
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

void TestRunner::Continue() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_waitingForContinue = false;
    }
    m_cv.notify_all();
}

void TestRunner::WaitWithStop(int ms) {
    const int slice = 50;
    while (ms > 0 && !m_stopFlag) {
        const int step = (ms < slice) ? ms : slice;
        std::this_thread::sleep_for(std::chrono::milliseconds(step));
        ms -= step;
    }
}

void TestRunner::RunLoop() {
    bool overallSuccess = true;
    bool aborted = false;

    for (size_t i = 0; i < m_steps.size() && !aborted; ++i) {
        if (m_stopFlag) {
            aborted = true;
            break;
        }

        auto& step = m_steps[i];
        const std::string stepName = step->Name();

        if (m_stepLogCb) m_stepLogCb(">>> " + stepName);
        if (m_stepCb) m_stepCb(static_cast<int>(i), StepStatus::Running, "");

        bool stepPassed = false;
        bool stepSkipped = false;

        while (!stepPassed && !stepSkipped) {
            if (m_stopFlag) {
                aborted = true;
                break;
            }

            // 1. 执行
            TestResult result = step->Execute(m_session, m_vars);

            // 2. 断言（执行本身失败时不再断言）
            std::string errorMsg;
            bool isSuccess = false;
            if (!result.success) {
                errorMsg = result.errorMessage.empty() ? "Execution failed." : result.errorMessage;
            } else {
                isSuccess = step->Assert(result, errorMsg);
                if (!isSuccess && errorMsg.empty()) errorMsg = "Assertion failed.";
            }

            if (isSuccess) {
                // 3. 提取变量（将结果写回变量池）
                auto updatedVars = step->Extract(result, m_vars);
                if (m_dataRecordCb) {
                    for (const auto& varName : updatedVars) {
                        if (m_vars.count(varName)) {
                            try {
                                double val = std::stod(m_vars[varName].currentValue);
                                m_dataRecordCb(varName, val);
                            } catch (...) {
                            }
                        }
                    }
                }

                // 4. 输出底层响应报文 / 写入值
                if (m_stepLogCb) {
                    if (!result.rawResponse.empty()) {
                        m_stepLogCb((result.isSendAction ? "=> " : "<<< ") + result.rawResponse);
                    } else if (!result.registers.empty()) {
                        std::string regStr = "Registers: ";
                        for (auto r : result.registers) regStr += std::to_string(r) + " ";
                        m_stepLogCb("<<< " + regStr);
                    }
                }

                if (m_stepCb) m_stepCb(static_cast<int>(i), StepStatus::Passed, "");
                stepPassed = true;
                break;
            }

            // 5. 失败：记录错误，并交给 UI 决定重试/跳过/结束
            if (m_stepLogCb) m_stepLogCb("!!! " + errorMsg);
            if (m_stepCb) m_stepCb(static_cast<int>(i), StepStatus::Failed, errorMsg);

            const StepFailureAction action =
                m_failCb ? m_failCb(static_cast<int>(i), errorMsg) : StepFailureAction::Stop;

            if (action == StepFailureAction::Retry) {
                if (m_stepLogCb) m_stepLogCb("^^^ Retrying: " + stepName);
                continue;
            }
            if (action == StepFailureAction::Skip) {
                overallSuccess = false;
                if (m_stepLogCb) m_stepLogCb("--- Skipped: " + stepName);
                if (m_stepCb) m_stepCb(static_cast<int>(i), StepStatus::Skipped, errorMsg);
                stepSkipped = true;
                break;
            }

            overallSuccess = false;
            aborted = true;
            break;
        }

        if (aborted) break;

        // 6. 步骤后等待（跳过的步骤不等待；步骤自己已经等待过的返回 0）
        if (!stepSkipped) {
            const int waitMs = step->WaitMs();
            if (waitMs > 0) WaitWithStop(waitMs);
        }

        // 7. 单步模式等待确认（跳过步骤后同样暂停）
        if (m_runMode == TestRunMode::SingleStep && i < m_steps.size() - 1) {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_waitingForContinue = true;
            }

            if (m_waitCb) m_waitCb(static_cast<int>(i) + 1);

            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() { return !m_waitingForContinue || m_stopFlag; });
            if (m_stopFlag) {
                aborted = true;
                break;
            }
        }
    }

    if (m_finishCb) {
        m_finishCb(overallSuccess && !m_stopFlag && !aborted);
    }
}

void TestRunner::WaitForCompletion() {
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

}  // namespace icu
