#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/ITestStep.h"
#include "core/TestRunner.h"
#include "doctest.h"

using namespace icu;

namespace {

class FakeStep : public ITestStep {
public:
    FakeStep(std::string name, int failTimes = 0, int waitMs = 0, bool executeOk = true)
        : m_name(std::move(name)),
          m_failTimes(failTimes),
          m_waitMs(waitMs),
          m_executeOk(executeOk) {}

    std::string Name() const override { return m_name; }

    TestResult Execute(ISession*, std::map<std::string, CommVariable>&) override {
        ++executes;
        TestResult result;
        result.success = m_executeOk;
        if (!m_executeOk) result.errorMessage = "execute failed";
        return result;
    }

    bool Assert(const TestResult&, std::string& err) override {
        if (executes > m_failTimes) return true;
        err = "assert failed";
        return false;
    }

    std::vector<std::string> Extract(const TestResult&,
                                     std::map<std::string, CommVariable>&) override {
        return {};
    }

    int WaitMs() const override { return m_waitMs; }

    int executes = 0;

private:
    std::string m_name;
    int m_failTimes;
    int m_waitMs;
    bool m_executeOk;
};

struct Recorder {
    std::mutex mutex;
    std::vector<StepStatus> statuses;
    int finishCalls = 0;
    bool finishedOk = false;

    void OnStatus(int, StepStatus status, const std::string&) {
        std::lock_guard<std::mutex> lock(mutex);
        statuses.push_back(status);
    }
};

int Count(const Recorder& rec, StepStatus status) {
    std::lock_guard<std::mutex> lock(const_cast<Recorder&>(rec).mutex);
    int n = 0;
    for (auto s : rec.statuses)
        if (s == status) ++n;
    return n;
}

}  // namespace

TEST_CASE("TestRunner: 断言失败后可以重试") {
    std::map<std::string, CommVariable> vars;
    std::vector<ITestStepPtr> steps;
    auto* fake = new FakeStep("flaky", 1);
    steps.emplace_back(fake);

    TestRunner runner(nullptr, nullptr, std::move(steps), vars);
    Recorder rec;
    int attempts = 0;

    runner.OnStepUpdated([&](int i, StepStatus s, const std::string& m) { rec.OnStatus(i, s, m); });
    runner.OnStepFailed([&](int, const std::string&) {
        ++attempts;
        return attempts == 1 ? StepFailureAction::Retry : StepFailureAction::Stop;
    });
    runner.OnFinished([&](bool ok) {
        std::lock_guard<std::mutex> lock(rec.mutex);
        rec.finishCalls++;
        rec.finishedOk = ok;
    });

    runner.Start();
    runner.WaitForCompletion();

    CHECK(fake->executes == 2);
    CHECK(attempts == 1);
    CHECK(Count(rec, StepStatus::Passed) == 1);
    CHECK(Count(rec, StepStatus::Failed) == 1);
    CHECK(rec.finishCalls == 1);
    CHECK(rec.finishedOk == true);
}

TEST_CASE("TestRunner: 断言失败后可以跳过并继续后续步骤") {
    std::map<std::string, CommVariable> vars;
    std::vector<ITestStepPtr> steps;
    auto* failing = new FakeStep("fail", 100);
    auto* following = new FakeStep("next", 0);
    steps.emplace_back(failing);
    steps.emplace_back(following);

    TestRunner runner(nullptr, nullptr, std::move(steps), vars);
    Recorder rec;

    runner.OnStepUpdated([&](int i, StepStatus s, const std::string& m) { rec.OnStatus(i, s, m); });
    runner.OnStepFailed([&](int, const std::string&) { return StepFailureAction::Skip; });
    runner.OnFinished([&](bool ok) {
        std::lock_guard<std::mutex> lock(rec.mutex);
        rec.finishCalls++;
        rec.finishedOk = ok;
    });

    runner.Start();
    runner.WaitForCompletion();

    CHECK(failing->executes == 1);
    CHECK(following->executes == 1);
    CHECK(Count(rec, StepStatus::Skipped) == 1);
    CHECK(Count(rec, StepStatus::Passed) == 1);
    CHECK(rec.finishedOk == false);  // 有步骤被跳过，整体不算成功
}

TEST_CASE("TestRunner: 失败后选择结束会停止后续步骤") {
    std::map<std::string, CommVariable> vars;
    std::vector<ITestStepPtr> steps;
    auto* failing = new FakeStep("fail", 100);
    auto* following = new FakeStep("next", 0);
    steps.emplace_back(failing);
    steps.emplace_back(following);

    TestRunner runner(nullptr, nullptr, std::move(steps), vars);
    runner.OnStepFailed([&](int, const std::string&) { return StepFailureAction::Stop; });

    runner.Start();
    runner.WaitForCompletion();

    CHECK(failing->executes == 1);
    CHECK(following->executes == 0);
}

TEST_CASE("TestRunner: Execute 失败即使没有断言也算失败") {
    std::map<std::string, CommVariable> vars;
    std::vector<ITestStepPtr> steps;
    auto* broken = new FakeStep("broken", 0, 0, false);
    steps.emplace_back(broken);

    TestRunner runner(nullptr, nullptr, std::move(steps), vars);
    int failures = 0;
    runner.OnStepFailed([&](int, const std::string&) {
        ++failures;
        return StepFailureAction::Stop;
    });

    runner.Start();
    runner.WaitForCompletion();

    CHECK(broken->executes == 1);
    CHECK(failures == 1);
}

TEST_CASE("TestRunner: WaitMs 会真正等待") {
    std::map<std::string, CommVariable> vars;
    std::vector<ITestStepPtr> steps;
    steps.emplace_back(new FakeStep("wait", 0, 120));

    TestRunner runner(nullptr, nullptr, std::move(steps), vars);
    runner.OnStepFailed([&](int, const std::string&) { return StepFailureAction::Stop; });

    const auto begin = std::chrono::steady_clock::now();
    runner.Start();
    runner.WaitForCompletion();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - begin)
                             .count();

    CHECK(elapsed >= 100);
}

TEST_CASE("TestRunner: 暂停后继续执行后续步骤") {
    std::map<std::string, CommVariable> vars;
    std::vector<ITestStepPtr> steps;
    auto* first = new FakeStep("first", 0, 300);
    auto* second = new FakeStep("second");
    steps.emplace_back(first);
    steps.emplace_back(second);

    TestRunner runner(nullptr, nullptr, std::move(steps), vars);
    runner.OnStepFailed([&](int, const std::string&) { return StepFailureAction::Stop; });

    runner.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    runner.Pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    CHECK(runner.IsPaused());
    CHECK(second->executes == 0);

    runner.Resume();
    runner.WaitForCompletion();

    CHECK(second->executes == 1);
}

TEST_CASE("TestRunner: 跳转到指定步骤") {
    std::map<std::string, CommVariable> vars;
    std::vector<ITestStepPtr> steps;
    auto* first = new FakeStep("first", 0, 100);
    auto* second = new FakeStep("second", 0, 100);
    auto* third = new FakeStep("third", 0, 100);
    steps.emplace_back(first);
    steps.emplace_back(second);
    steps.emplace_back(third);

    TestRunner runner(nullptr, nullptr, std::move(steps), vars);
    runner.OnStepFailed([&](int, const std::string&) { return StepFailureAction::Stop; });

    runner.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    runner.JumpToStep(2);
    runner.WaitForCompletion();

    CHECK(second->executes == 0);
    CHECK(third->executes == 1);
}
