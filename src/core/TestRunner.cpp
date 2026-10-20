#include "TestRunner.h"

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

void TestRunner::RunLoop() {
    bool overallSuccess = true;

    for (size_t i = 0; i < m_steps.size(); ++i) {
        if (m_stopFlag) {
            overallSuccess = false;
            break;
        }

        auto& step = m_steps[i];
        std::string stepName = step->Name();

        // 1. 执行
        if (m_stepLogCb) m_stepLogCb(">>> " + stepName);
        TestResult result = step->Execute(m_session, m_vars);

        // 输出底层响应报文 (阶段 10 补回)
        if (m_stepLogCb) {
            if (!result.rawResponse.empty()) {
                m_stepLogCb("<<< " + result.rawResponse);
            } else if (!result.registers.empty()) {
                std::string regStr = "Registers: ";
                for (auto r : result.registers) regStr += std::to_string(r) + " ";
                m_stepLogCb("<<< " + regStr);
            }
            if (!result.errorMessage.empty()) {
                m_stepLogCb("!!! " + result.errorMessage);
            }
        }

        // 2. 断言
        std::string errorMsg;
        bool isSuccess = step->Assert(result, errorMsg);

        // 3. 提取变量（将结果写回变量池）
        if (isSuccess) {
            auto updatedVars = step->Extract(result, m_vars);

            // 触发数据记录回调 (阶段 11 新增)
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
        }

        // 4. 通知 UI
        if (m_stepCb) {
            if (!isSuccess) {
                overallSuccess = false;
                m_stepCb(static_cast<int>(i), false, errorMsg);
                break;
            } else {
                m_stepCb(static_cast<int>(i), true, "");
            }
        }

        // 5. 单步等待
        if (m_runMode == TestRunMode::SingleStep && i < m_steps.size() - 1) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_waitingForContinue = true;
            lock.unlock();

            if (m_waitCb) m_waitCb(static_cast<int>(i + 1));

            lock.lock();
            m_cv.wait(lock, [this]() { return !m_waitingForContinue || m_stopFlag; });
            if (m_stopFlag) {
                overallSuccess = false;
                break;
            }
        }
    }

    if (m_finishCb) {
        m_finishCb(overallSuccess && !m_stopFlag);
    }
}

void TestRunner::WaitForCompletion() {
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

}  // namespace icu
