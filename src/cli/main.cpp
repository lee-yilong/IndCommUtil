#include <wx/init.h>
#include <wx/string.h>

#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#include "config/ConnectionProfile.h"
#include "core/DelayStep.h"
#include "core/ITransactionSession.h"
#include "core/ProtocolRegistry.h"
#include "core/TestRunner.h"

void PrintUsage() {
    std::cout << "Usage:\n"
              << "  1. Run flow (Auto):        indcomm-cli -c <json> -r\n"
              << "  2. Run flow (Interactive): indcomm-cli -c <json> -i\n"
              << "  3. Read variable:          indcomm-cli -c <json> --read <var_name>\n"
              << "  4. Write variable:         indcomm-cli -c <json> --write <var_name> <value>\n";
}

int main(int argc, char** argv) {
    wxInitializer wxInit;
    if (!wxInit.IsOk()) {
        std::cerr << "Failed to initialize wxWidgets base." << std::endl;
        return 1;
    }

    std::string configPath;
    std::string readVar;
    std::string writeVar;
    std::string writeValue;
    bool runFlow = false;
    bool interactiveMode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" || arg == "-c") {
            if (i + 1 < argc) configPath = argv[++i];
        } else if (arg == "--read") {
            if (i + 1 < argc) readVar = argv[++i];
        } else if (arg == "--write") {
            if (i + 2 < argc) {
                writeVar = argv[++i];
                writeValue = argv[++i];
            }
        } else if (arg == "--run" || arg == "-r") {
            runFlow = true;
            interactiveMode = false;
        } else if (arg == "--interactive" || arg == "-i") {
            runFlow = true;
            interactiveMode = true;
        }
    }

    if (configPath.empty()) {
        PrintUsage();
        return 1;
    }

    std::cout << "Loading config: " << configPath << std::endl;
    icu::RegisterBuiltinProtocols();

    icu::ActiveProfile profile;
    if (!icu::ConnectionProfile::Load(wxString::FromUTF8(configPath), profile)) {
        std::cerr << "Failed to load config file. Check syntax or if file exists." << std::endl;
        return 1;
    }

    auto* proto = icu::ProtocolRegistry::Instance().Find(profile.protocolId);
    if (!proto) {
        std::cerr << "Protocol not found: " << profile.protocolId << std::endl;
        return 1;
    }

    auto session = proto->CreateSession(profile.modeIndex, profile.fields);
    if (!session) {
        std::cerr << "Failed to create session." << std::endl;
        return 1;
    }

    std::cout << "Connecting to " << session->Target() << "..." << std::endl;
    session->Open();

    for (int i = 0; i < 50 && !session->IsConnected(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!session->IsConnected()) {
        std::cerr << "Connection timeout." << std::endl;
        return 1;
    }
    std::cout << "Connected successfully." << std::endl;

    // =========================================================================
    // 模式 1：直接读变量
    // =========================================================================
    if (!readVar.empty()) {
        if (!profile.session.variables.count(readVar)) {
            std::cerr << "Variable not found: " << readVar << std::endl;
            session->Close();
            return 1;
        }
        auto& var = profile.session.variables[readVar];
        auto txSession = dynamic_cast<icu::ITransactionSession*>(session.get());
        if (!txSession) {
            std::cerr << "Direct read/write only supported for transaction protocols." << std::endl;
            session->Close();
            return 1;
        }

        icu::TransactionRequest req;
        req.slaveId = var.params.value("slaveId", 1);
        req.functionCode = var.params.value("functionCode", 3);
        req.startAddress = var.params.value("startAddress", 0);
        req.count = var.params.value("count", 1);

        auto resp = txSession->Execute(req);
        if (resp.ok && !resp.registers.empty()) {
            std::cout << "Read Success: " << readVar << " = " << resp.registers[0] << std::endl;
            return 0;
        } else {
            std::cerr << "Read failed: " << resp.errorMessage << std::endl;
            return 1;
        }
    }

    // =========================================================================
    // 模式 2：直接写变量
    // =========================================================================
    if (!writeVar.empty()) {
        if (!profile.session.variables.count(writeVar)) {
            std::cerr << "Variable not found: " << writeVar << std::endl;
            session->Close();
            return 1;
        }
        auto& var = profile.session.variables[writeVar];
        auto txSession = dynamic_cast<icu::ITransactionSession*>(session.get());
        if (!txSession) {
            std::cerr << "Direct read/write only supported for transaction protocols." << std::endl;
            session->Close();
            return 1;
        }

        icu::TransactionRequest req;
        req.slaveId = var.params.value("slaveId", 1);
        int fc = var.params.value("functionCode", 6);
        if (fc == 1)
            fc = 5;
        else if (fc == 3)
            fc = 6;
        else if (fc == 4)
            fc = 6;
        req.functionCode = fc;
        req.startAddress = var.params.value("startAddress", 0);
        try {
            req.writeValues.push_back(std::stoi(writeValue));
        } catch (...) {
        }

        auto resp = txSession->Execute(req);
        if (resp.ok) {
            std::cout << "Write Success: " << writeVar << " = " << writeValue << std::endl;
            return 0;
        } else {
            std::cerr << "Write failed: " << resp.errorMessage << std::endl;
            return 1;
        }
    }

    // =========================================================================
    // 模式 3：执行 test_flow (自动 或 交互)
    // =========================================================================
    if (runFlow) {
        nlohmann::json testFlow = nlohmann::json::array();
        if (profile.session.activeMode == icu::SessionMode::Auto) {
            testFlow = profile.session.autoMode.testFlow;
        } else if (profile.session.activeMode == icu::SessionMode::Interactive) {
            testFlow = profile.session.interactiveMode.testFlow;
        }

        if (!testFlow.is_array() || testFlow.empty()) {
            if (profile.session.autoMode.testFlow.is_array() &&
                !profile.session.autoMode.testFlow.empty()) {
                testFlow = profile.session.autoMode.testFlow;
            } else if (profile.session.interactiveMode.testFlow.is_array() &&
                       !profile.session.interactiveMode.testFlow.empty()) {
                testFlow = profile.session.interactiveMode.testFlow;
            }
        }

        if (testFlow.empty()) {
            std::cerr << "[ERROR] No test flow defined." << std::endl;
            session->Close();
            return 1;
        }

        std::vector<icu::ITestStepPtr> steps;
        for (const auto& stepJson : testFlow) {
            std::string type = stepJson.value("action", "");
            icu::ITestStepPtr step;
            if (type == "delay")
                step = std::make_unique<icu::DelayStep>(stepJson);
            else
                step = proto->CreateTestStep(type, stepJson);
            if (step) steps.push_back(std::move(step));
        }

        std::cout << "Running " << steps.size() << " test steps..."
                  << (interactiveMode ? " (Interactive Mode)" : " (Auto Mode)") << std::endl;

        icu::TestRunner runner(proto, session.get(), std::move(steps), profile.session.variables);

        if (interactiveMode) {
            runner.SetRunMode(icu::TestRunMode::SingleStep);

            runner.OnWaitForNextStep([&runner](int nextIndex) {
                std::cout << "\n[WAIT] Ready for next step (" << nextIndex << ")." << std::endl;
                std::cout << "Press ENTER to continue, or type 'q' and press ENTER to quit: ";
                std::cout.flush();  // 确保提示立刻输出

                std::string input;
                std::getline(std::cin, input);

                if (input == "q" || input == "Q") {
                    std::cout << "Stopping by user request..." << std::endl;
                    runner.Stop();
                } else {
                    runner.Continue();  // 通知后台线程继续
                }
            });
        } else {
            runner.SetRunMode(icu::TestRunMode::Auto);
        }

        bool overallSuccess = true;

        // ★ 修改这里：增加变量池的输出
        runner.OnStepUpdated([&](int index, icu::StepStatus status, const std::string& msg) {
            const char* label = "[INFO]";
            switch (status) {
                case icu::StepStatus::Running:
                    label = "[RUN ]";
                    break;
                case icu::StepStatus::Passed:
                    label = "[PASS]";
                    break;
                case icu::StepStatus::Failed:
                    label = "[FAIL]";
                    break;
                case icu::StepStatus::Skipped:
                    label = "[SKIP]";
                    break;
            }
            std::cout << label << " Step " << index;
            if (!msg.empty()) std::cout << " - " << msg;
            std::cout << std::endl;

            // 打印变量池的当前值（让用户看到数据变化）
            std::cout << "       Current Variables: ";
            for (const auto& [name, var] : profile.session.variables) {
                std::cout << name << " = [" << var.currentValue << "]  ";
            }
            std::cout << std::endl;
        });

        runner.OnStepFailed([](int index, const std::string& msg) {
            std::cout << "\n[FAILED] Step " << index << ": " << msg << std::endl;
            std::cout << "Retry (r) / Skip (s) / Stop (q)? ";
            std::cout.flush();
            std::string input;
            std::getline(std::cin, input);
            if (input == "r" || input == "R") return icu::StepFailureAction::Retry;
            if (input == "s" || input == "S") return icu::StepFailureAction::Skip;
            return icu::StepFailureAction::Stop;
        });

        runner.OnFinished([&](bool success) { overallSuccess = overallSuccess && success; });

        runner.Start();
        runner.WaitForCompletion();

        std::cout << "Test execution finished. Result: " << (overallSuccess ? "PASS" : "FAIL")
                  << std::endl;
        session->Close();
        return overallSuccess ? 0 : 1;
    }

    PrintUsage();
    session->Close();
    return 0;
}
