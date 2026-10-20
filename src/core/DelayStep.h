#pragma once

#include <chrono>
#include <nlohmann/json.hpp>
#include <thread>

#include "ITestStep.h"

namespace icu {

class DelayStep : public ITestStep {
private:
    std::string m_name;
    int m_waitMs;

public:
    DelayStep(const nlohmann::json& params) {
        m_name = params.value("name", "Delay");
        m_waitMs = params.value("wait_ms", 1000);
    }
    std::string Name() const override { return m_name; }

    TestResult Execute(ISession*, std::map<std::string, CommVariable>&) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_waitMs));
        TestResult r;
        r.success = true;
        return r;
    }

    bool Assert(const TestResult&, std::string&) override { return true; }
    std::vector<std::string> Extract(const TestResult&,
                                     std::map<std::string, CommVariable>&) override {
        return {};
    }
};

}  // namespace icu
