#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "e_foc/BuildInfo.hpp"
#include "integration_tests/support/CanParameters.hpp"
#include "integration_tests/support/Fixture.hpp"
#include <gtest/gtest.h>

using namespace integration;

namespace
{
    void ExpectReported(Fixture& fixture, can::FocParameter parameter, float expected)
    {
        const auto value = ReadParameterValue(fixture, parameter);
        ASSERT_TRUE(value.has_value()) << "The target did not answer QueryParameter";
        EXPECT_NEAR(*value, expected, 0.001f);
    }
}

THEN(R"(the target shall report the firmware version of this build)")
{
    auto& fixture = context.Get<Fixture>();

    ExpectReported(fixture, can::FocParameter::firmwareVersionMajor, build_info::versionMajor);
    ExpectReported(fixture, can::FocParameter::firmwareVersionMinor, build_info::versionMinor);
    ExpectReported(fixture, can::FocParameter::firmwareVersionPatch, build_info::versionPatch);
}

THEN(R"(the target shall report board kind {int})", (int kind))
{
    ExpectReported(context.Get<Fixture>(), can::FocParameter::boardKind, static_cast<float>(kind));
}
