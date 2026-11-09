#include "tools/can_commander/logic/FirmwareIdentityCollector.hpp"
#include "gtest/gtest.h"

namespace
{
    using tool::FirmwareIdentity;
    using tool::FirmwareIdentityCollector;

    class TestFirmwareIdentityCollector
        : public testing::Test
    {
    public:
        std::optional<FirmwareIdentity> CollectRunning(uint16_t nodeId)
        {
            std::optional<FirmwareIdentity> identity;
            identity = collector.Collect(nodeId, can::FocParameter::firmwareVersionMajor, 2.0f);
            EXPECT_FALSE(identity.has_value());
            identity = collector.Collect(nodeId, can::FocParameter::firmwareVersionMinor, 1.0f);
            EXPECT_FALSE(identity.has_value());
            identity = collector.Collect(nodeId, can::FocParameter::firmwareVersionPatch, 4.0f);
            EXPECT_FALSE(identity.has_value());
            return collector.Collect(nodeId, can::FocParameter::boardKind, 3.0f);
        }

        FirmwareIdentityCollector collector;
    };
}

TEST_F(TestFirmwareIdentityCollector, only_the_seven_identity_parameters_count)
{
    EXPECT_TRUE(FirmwareIdentityCollector::IsIdentityParameter(can::FocParameter::firmwareVersionMajor));
    EXPECT_TRUE(FirmwareIdentityCollector::IsIdentityParameter(can::FocParameter::stagedVersionPatch));
    EXPECT_FALSE(FirmwareIdentityCollector::IsIdentityParameter(can::FocParameter::currentBandwidth));
    EXPECT_FALSE(collector.Collect(3, can::FocParameter::currentBandwidth, 1.0f).has_value());
}

TEST_F(TestFirmwareIdentityCollector, the_identity_completes_once_running_and_staged_versions_arrived)
{
    EXPECT_FALSE(CollectRunning(3).has_value());
    EXPECT_FALSE(collector.Collect(3, can::FocParameter::stagedVersionMajor, 2.0f).has_value());
    EXPECT_FALSE(collector.Collect(3, can::FocParameter::stagedVersionMinor, 2.0f).has_value());
    const auto identity = collector.Collect(3, can::FocParameter::stagedVersionPatch, 0.0f);

    ASSERT_TRUE(identity.has_value());
    EXPECT_EQ((FirmwareIdentity{ { 2, 1, 4 }, 3, { 2, 2, 0 }, true }), *identity);
}

TEST_F(TestFirmwareIdentityCollector, a_refused_staged_read_completes_the_identity_without_an_upgrade_service)
{
    EXPECT_FALSE(CollectRunning(3).has_value());
    const auto identity = collector.Refused(3, can::FocParameter::stagedVersionMajor);

    ASSERT_TRUE(identity.has_value());
    EXPECT_EQ((FirmwareIdentity{ { 2, 1, 4 }, 3, {}, false }), *identity);
}

TEST_F(TestFirmwareIdentityCollector, a_refusal_waits_for_the_running_version)
{
    EXPECT_FALSE(collector.Refused(3, can::FocParameter::stagedVersionMajor).has_value());

    const auto identity = CollectRunning(3);
    ASSERT_TRUE(identity.has_value());
    EXPECT_FALSE(identity->hasUpgradeService);
}

TEST_F(TestFirmwareIdentityCollector, refusing_a_running_version_or_another_parameter_changes_nothing)
{
    EXPECT_FALSE(collector.Refused(3, can::FocParameter::firmwareVersionMajor).has_value());
    EXPECT_FALSE(collector.Refused(3, can::FocParameter::currentBandwidth).has_value());
    EXPECT_FALSE(CollectRunning(3).has_value());
}

TEST_F(TestFirmwareIdentityCollector, beginning_again_or_forgetting_discards_what_arrived)
{
    EXPECT_FALSE(CollectRunning(3).has_value());
    collector.Begin(3);
    EXPECT_FALSE(collector.Refused(3, can::FocParameter::stagedVersionMajor).has_value());

    collector.Forget(3);
    EXPECT_FALSE(CollectRunning(3).has_value());
}
