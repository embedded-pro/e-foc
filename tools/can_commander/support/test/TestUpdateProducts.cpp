#include "tools/can_commander/support/ApplicationInfo.hpp"
#include "tools/can_commander/support/Update.hpp"
#include "gtest/gtest.h"

TEST(TestUpdateProducts, the_can_commander_describes_itself_and_waits_for_the_bus_to_close)
{
    const auto product = tool::CommanderProduct();

    EXPECT_EQ("e-foc CAN Commander", product.displayName);
    EXPECT_EQ("CAN Commander", product.shortName);
    EXPECT_EQ(QString::fromUtf8(tool::application::version.data(), static_cast<qsizetype>(tool::application::version.size())), product.version);
    EXPECT_EQ("/repos/embedded-pro/e-foc/releases/latest", product.feedUrl.path());
    EXPECT_EQ("/embedded-pro/e-foc/issues/new", product.newIssueUrl.path());
    EXPECT_TRUE(product.userAgent.startsWith("e-foc-can-commander/"));
    EXPECT_EQ("Disconnect from the CAN bus", product.blockedAction);
}

TEST(TestUpdateProducts, the_virtual_motor_reports_under_its_own_name)
{
    const auto product = tool::VirtualMotorProduct();

    EXPECT_EQ("e-foc Virtual Motor", product.displayName);
    EXPECT_EQ("Virtual Motor", product.shortName);
    EXPECT_EQ(tool::CommanderProduct().newIssueUrl, product.newIssueUrl);
}
