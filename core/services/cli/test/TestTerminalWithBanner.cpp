#include "core/platform_abstraction/ResetCause.hpp"
#include "core/services/cli/TerminalWithBanner.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/stream/test/StreamMock.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include "services/util/Terminal.hpp"
#include "gmock/gmock.h"
#include <string>
#include <vector>

namespace
{
    class TerminalWithBannerTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        ::testing::StrictMock<infra::StreamWriterMock> streamWriterMock;
        infra::TextOutputStream::WithErrorPolicy stream{ streamWriterMock };
        services::TracerToStream tracer{ stream };
        ::testing::StrictMock<hal::SerialCommunicationMock> communication;
        std::vector<std::vector<uint8_t>> capturedOutputs;
        infra::Execute execute{ [this]()
            {
                EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_))
                    .Times(testing::AnyNumber())
                    .WillRepeatedly(testing::Invoke([this](infra::ConstByteRange range, infra::StreamErrorPolicy&)
                        {
                            capturedOutputs.push_back(std::vector<uint8_t>(range.begin(), range.end()));
                        }));
            } };
        services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<128, 5> terminalWithCommands{ communication, tracer };

        services::TerminalWithBanner::Banner CreateBanner(const char* targetName, float vdc, uint32_t systemClock)
        {
            services::TerminalWithBanner::Banner banner{
                .targetName = infra::BoundedConstString(targetName),
                .vdc = foc::Volts{ vdc },
                .systemClock = hal::Hertz{ systemClock }
            };
            return banner;
        }

        services::TerminalWithBanner::Banner CreateBannerWithResetAndFault(
            const char* targetName, float vdc, uint32_t systemClock,
            application::ResetCause resetCause, infra::BoundedConstString faultStatus = "")
        {
            services::TerminalWithBanner::Banner banner{
                .targetName = infra::BoundedConstString(targetName),
                .vdc = foc::Volts{ vdc },
                .systemClock = hal::Hertz{ systemClock },
                .resetCause = resetCause,
                .faultStatus = faultStatus
            };
            return banner;
        }

        bool OutputContains(const std::string& text)
        {
            std::vector<uint8_t> searchBytes(text.begin(), text.end());
            for (const auto& output : capturedOutputs)
            {
                if (output == searchBytes)
                    return true;
            }
            return false;
        }

        std::string GetFullOutput()
        {
            std::string result;
            for (const auto& output : capturedOutputs)
            {
                result.append(output.begin(), output.end());
            }
            return result;
        }
    };
}

TEST_F(TerminalWithBannerTest, construction_prints_banner_with_target_name)
{
    auto banner = CreateBanner("TestBoard", 12.0f, 80000000);
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    EXPECT_TRUE(OutputContains("TestBoard"));
}

TEST_F(TerminalWithBannerTest, construction_prints_banner_with_clear_screen_sequence)
{
    auto banner = CreateBanner("Board", 12.0f, 80000000);
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    EXPECT_TRUE(OutputContains("\033[2J\033[H"));
}

TEST_F(TerminalWithBannerTest, construction_prints_banner_with_separator_lines)
{
    auto banner = CreateBanner("Board", 12.0f, 80000000);
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    EXPECT_TRUE(OutputContains("================================================"));
}

TEST_F(TerminalWithBannerTest, construction_prints_banner_with_version)
{
    auto banner = CreateBanner("Board", 12.0f, 80000000);
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    EXPECT_TRUE(OutputContains("Version: 0.0.1"));
}

TEST_F(TerminalWithBannerTest, construction_prints_banner_with_e_foc_prefix)
{
    auto banner = CreateBanner("Board", 12.0f, 80000000);
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    EXPECT_TRUE(OutputContains("e-foc:"));
}

TEST_F(TerminalWithBannerTest, construction_prints_banner_with_ready_message)
{
    auto banner = CreateBanner("Board", 12.0f, 80000000);
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    EXPECT_TRUE(OutputContains("Ready to accept commands. Type 'help' for available commands."));
}

TEST_F(TerminalWithBannerTest, construction_prints_banner_with_different_target_names)
{
    auto banner = CreateBanner("STM32F407G-DISC1", 24.0f, 168000000);
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    EXPECT_TRUE(OutputContains("STM32F407G-DISC1"));
}

TEST_F(TerminalWithBannerTest, construction_includes_voltage_in_output)
{
    auto banner = CreateBanner("Board", 12.0f, 80000000);
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    std::string output = GetFullOutput();
    EXPECT_NE(output.find("12"), std::string::npos);
    EXPECT_NE(output.find("Power Supply Voltage"), std::string::npos);
}

TEST_F(TerminalWithBannerTest, construction_includes_system_clock_in_output)
{
    auto banner = CreateBanner("Board", 12.0f, 80000000);
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    std::string output = GetFullOutput();
    EXPECT_NE(output.find("80000000"), std::string::npos);
    EXPECT_NE(output.find("System Clock"), std::string::npos);
}

TEST_F(TerminalWithBannerTest, construction_includes_target_board_in_output)
{
    auto banner = CreateBanner("Board", 12.0f, 80000000);
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    std::string output = GetFullOutput();
    EXPECT_NE(output.find("Target: "), std::string::npos);
}

TEST_F(TerminalWithBannerTest, construction_includes_build_info_in_output)
{
    auto banner = CreateBanner("Board", 12.0f, 80000000);
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    std::string output = GetFullOutput();
    EXPECT_NE(output.find("Build: "), std::string::npos);
}

TEST_F(TerminalWithBannerTest, construction_prints_reset_cause)
{
    auto banner = CreateBanner("Board", 12.0f, 80000000);
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    std::string output = GetFullOutput();
    EXPECT_NE(output.find("Reset Cause: "), std::string::npos);
    EXPECT_NE(output.find("PowerUp"), std::string::npos);
}

TEST_F(TerminalWithBannerTest, construction_prints_watchdog_reset_cause)
{
    auto banner = CreateBannerWithResetAndFault("Board", 12.0f, 80000000, application::ResetCause::watchdog);
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    std::string output = GetFullOutput();
    EXPECT_NE(output.find("Reset Cause: "), std::string::npos);
    EXPECT_NE(output.find("Watchdog"), std::string::npos);
}

TEST_F(TerminalWithBannerTest, construction_prints_compiler_info)
{
    auto banner = CreateBanner("Board", 12.0f, 80000000);
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    std::string output = GetFullOutput();
    EXPECT_NE(output.find("Compiler: "), std::string::npos);
}

TEST_F(TerminalWithBannerTest, construction_prints_fault_data_when_present)
{
    auto banner = CreateBannerWithResetAndFault(
        "Board", 12.0f, 80000000, application::ResetCause::hardware, "PC=0x08001234 LR=0x08005678");
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    std::string output = GetFullOutput();
    EXPECT_NE(output.find("!!! FAULT DATA FROM PREVIOUS SESSION !!!"), std::string::npos);
    EXPECT_NE(output.find("PC=0x08001234 LR=0x08005678"), std::string::npos);
    EXPECT_NE(output.find("!!! END FAULT DATA !!!"), std::string::npos);
}

TEST_F(TerminalWithBannerTest, construction_does_not_print_fault_data_when_empty)
{
    auto banner = CreateBannerWithResetAndFault("Board", 12.0f, 80000000, application::ResetCause::powerUp);
    services::TerminalWithBanner::WithMaxSize<10> terminalWithBanner{ terminalWithCommands, tracer, banner };
    ExecuteAllActions();

    std::string output = GetFullOutput();
    EXPECT_EQ(output.find("!!! FAULT DATA FROM PREVIOUS SESSION !!!"), std::string::npos);
    EXPECT_EQ(output.find("!!! END FAULT DATA !!!"), std::string::npos);
}
