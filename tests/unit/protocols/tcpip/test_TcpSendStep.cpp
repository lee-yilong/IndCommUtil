#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/StreamSession.h"
#include "doctest.h"
#include "mocks/MockTransport.h"
#include "protocols/codec/RawCodec.h"
#include "protocols/tcpip/TcpSendStep.h"

using namespace icu;
using icu::test::MockTransport;

namespace {

struct TcpStepFixture {
    MockTransport* transport = nullptr;
    std::unique_ptr<StreamSession> session;
    std::map<std::string, CommVariable> vars;

    TcpStepFixture() {
        auto mock = std::make_unique<MockTransport>();
        transport = mock.get();
        session = std::make_unique<StreamSession>(std::move(mock), std::make_unique<RawCodec>());
        session->Open();
        transport->SimulateState(TransportState::Open);

        CommVariable var;
        var.name = "new_var";
        var.type = "int32";
        var.access = "RW";
        vars["new_var"] = var;
    }
};

nlohmann::json WriteStepJson(const std::string& value) {
    return {{"name", "write"},
            {"action", "write"},
            {"variable", "new_var"},
            {"value", value},
            {"wait_ms", 0}};
}

nlohmann::json ReadStepJson(int waitMs, const std::string& assertType = "",
                            const std::string& expected = "") {
    nlohmann::json step = {
        {"name", "read"},
        {"action", "read"},
        {"variable", "new_var"},
        {"wait_ms", waitMs},
    };
    if (!assertType.empty()) step["assert"] = {{"type", assertType}, {"expected", expected}};
    return step;
}

}  // namespace

TEST_CASE("TcpSendStep: 写步骤发送步骤里配置的值") {
    TcpStepFixture f;
    TcpSendStep step(WriteStepJson("1"));

    auto result = step.Execute(f.session.get(), f.vars);

    REQUIRE(result.success);
    CHECK(result.isSendAction);
    CHECK(result.rawResponse == "1");
    CHECK(f.transport->SentBytes() == std::vector<uint8_t>{0x00, 0x00, 0x00, 0x01});

    std::string error;
    CHECK(step.Assert(result, error));

    auto updated = step.Extract(result, f.vars);
    REQUIRE(updated.size() == 1);
    CHECK(f.vars["new_var"].currentValue == "1");
}

TEST_CASE("TcpSendStep: 写步骤优先使用步骤值而不是变量当前值") {
    TcpStepFixture f;
    f.vars["new_var"].currentValue = "123";
    TcpSendStep step(WriteStepJson("7"));

    auto result = step.Execute(f.session.get(), f.vars);

    REQUIRE(result.success);
    CHECK(f.transport->SentBytes() == std::vector<uint8_t>{0x00, 0x00, 0x00, 0x07});
}

TEST_CASE("TcpSendStep: 读步骤按变量类型解码并支持数值断言") {
    TcpStepFixture f;
    TcpSendStep step(ReadStepJson(500, "==", "1"));

    TestResult result;
    std::thread injector([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        f.transport->InjectData(0, std::vector<uint8_t>{0x00, 0x00, 0x00, 0x01});
    });
    result = step.Execute(f.session.get(), f.vars);
    injector.join();

    REQUIRE(result.success);
    CHECK(result.rawResponse == "1");
    CHECK(result.rawBytesHex == "00 00 00 01");

    std::string error;
    CHECK(step.Assert(result, error));
}

TEST_CASE("TcpSendStep: 读步骤没有响应时失败，不再使用旧值") {
    TcpStepFixture f;
    f.vars["new_var"].currentValue = "123";
    TcpSendStep step(ReadStepJson(80));

    auto result = step.Execute(f.session.get(), f.vars);

    CHECK_FALSE(result.success);
    CHECK(result.errorMessage.find("No response") != std::string::npos);
    CHECK(result.rawResponse.empty());
}

TEST_CASE("TcpSendStep: 数值断言支持 >、<、!= 与容差") {
    TcpStepFixture f;
    TestResult result;
    result.success = true;
    result.rawResponse = "123.0";

    TcpSendStep greater(ReadStepJson(1, ">", "122"));
    TcpSendStep less(ReadStepJson(1, "<", "124"));
    TcpSendStep equal(ReadStepJson(1, "==", "123"));
    TcpSendStep notEqual(ReadStepJson(1, "!=", "100"));

    std::string error;
    CHECK(greater.Assert(result, error));
    CHECK(less.Assert(result, error));
    CHECK(equal.Assert(result, error));
    CHECK(notEqual.Assert(result, error));

    TcpSendStep wrong(ReadStepJson(1, ">", "200"));
    CHECK_FALSE(wrong.Assert(result, error));
}
