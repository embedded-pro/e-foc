#include "tools/can_commander/support/ReleaseVersion.hpp"
#include "gtest/gtest.h"

namespace
{
    using tool::ReleaseVersion;

    class TestReleaseVersion
        : public testing::Test
    {};
}

TEST_F(TestReleaseVersion, parses_release_please_tag)
{
    const auto version = ReleaseVersion::Parse("v1.2.3");

    ASSERT_TRUE(version.has_value());
    EXPECT_EQ(1u, version->major);
    EXPECT_EQ(2u, version->minor);
    EXPECT_EQ(3u, version->patch);
}

TEST_F(TestReleaseVersion, parses_project_version_without_prefix)
{
    EXPECT_EQ((ReleaseVersion{ 10, 0, 42 }), ReleaseVersion::Parse("10.0.42"));
}

TEST_F(TestReleaseVersion, rejects_malformed_versions)
{
    for (const auto* text : { "", "v", "1", "1.2", "1.2.", "1..3", "1.2.3.4", "1.2.3-rc.1", "1.2.3+build", "01.2.3", "-1.2.3", "dry-run-17", "v 1.2.3" })
        EXPECT_FALSE(ReleaseVersion::Parse(text).has_value()) << text;
}

TEST_F(TestReleaseVersion, orders_by_major_then_minor_then_patch)
{
    EXPECT_LT((ReleaseVersion{ 1, 9, 9 }), (ReleaseVersion{ 2, 0, 0 }));
    EXPECT_LT((ReleaseVersion{ 1, 2, 9 }), (ReleaseVersion{ 1, 10, 0 }));
    EXPECT_LT((ReleaseVersion{ 1, 2, 3 }), (ReleaseVersion{ 1, 2, 4 }));
    EXPECT_EQ((ReleaseVersion{ 1, 2, 3 }), (ReleaseVersion{ 1, 2, 3 }));
}

TEST_F(TestReleaseVersion, formats_without_prefix)
{
    EXPECT_EQ("3.14.159", (ReleaseVersion{ 3, 14, 159 }).ToString());
}
