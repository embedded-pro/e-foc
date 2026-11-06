#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/services/cli/StartupBanner.hpp"
#include "infra/stream/StdStringOutputStream.hpp"
#include "services/tracer/Tracer.hpp"
#include "gtest/gtest.h"
#include <string>

namespace
{
    class TestStartupBanner
        : public ::testing::Test
    {
    public:
        services::StartupBanner Banner(infra::BoundedConstString faultStatus = "")
        {
            return services::StartupBanner{
                .targetName = infra::BoundedConstString("sync_foc_sensored"),
                .vdc = foc::Volts{ 24.0f },
                .systemClock = hal::Hertz{ 120000000 },
                .resetCause = application::ResetCause::watchdog,
                .faultStatus = faultStatus,
                .version = "1.2.3",
                .revision = "abc12345"
            };
        }

        std::string Traced(const services::StartupBanner& banner)
        {
            infra::StdStringOutputStream::WithStorage stream;
            services::TracerToStream tracer{ stream };
            services::TraceStartupBanner(tracer, banner);
            return stream.Storage();
        }
    };
}

TEST_F(TestStartupBanner, the_banner_names_the_application_its_version_and_the_reset_cause)
{
    const auto text = Traced(Banner());

    EXPECT_NE(std::string::npos, text.find("e-foc:sync_foc_sensored"));
    EXPECT_NE(std::string::npos, text.find("Version: 1.2.3"));
    EXPECT_NE(std::string::npos, text.find("Revision: abc12345"));
    EXPECT_NE(std::string::npos, text.find("System Clock: 120000000 Hz"));
    EXPECT_NE(std::string::npos, text.find("Reset Cause: "));
}

TEST_F(TestStartupBanner, a_fault_left_by_a_crash_is_traced_between_delimiters)
{
    const auto text = Traced(Banner("HardFault at 0x08001234"));

    const auto begin = text.find("!!! FAULT DATA FROM PREVIOUS SESSION !!!");
    const auto fault = text.find("HardFault at 0x08001234");
    const auto end = text.find("!!! END FAULT DATA !!!");
    ASSERT_NE(std::string::npos, begin);
    EXPECT_LT(begin, fault);
    EXPECT_LT(fault, end);
}

TEST_F(TestStartupBanner, the_banner_alone_invites_no_commands)
{
    const auto text = Traced(Banner());

    EXPECT_EQ(std::string::npos, text.find("Ready to accept commands"));
}
