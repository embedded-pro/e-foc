#include "core/services/non_volatile_memory/NvmBytes.hpp"
#include <array>
#include <gtest/gtest.h>

namespace
{
    using Bytes = std::array<uint8_t, 8>;

    Bytes Filled()
    {
        Bytes bytes{};
        bytes.fill(0xAA);
        return bytes;
    }
}

TEST(NvmBytesTest, unsigned_values_are_stored_little_endian)
{
    auto bytes = Filled();

    services::nvm::Put<uint32_t>(infra::MakeByteRange(bytes), 1, 0xCAFEF00Du);
    services::nvm::Put<uint16_t>(infra::MakeByteRange(bytes), 5, 0x1234u);

    EXPECT_EQ((Bytes{ 0xAA, 0x0D, 0xF0, 0xFE, 0xCA, 0x34, 0x12, 0xAA }), bytes);
}

TEST(NvmBytesTest, signed_values_are_stored_as_twos_complement_little_endian)
{
    auto bytes = Filled();

    services::nvm::Put<int16_t>(infra::MakeByteRange(bytes), 0, -2);
    services::nvm::Put<int32_t>(infra::MakeByteRange(bytes), 2, -0x01020304);

    EXPECT_EQ((Bytes{ 0xFE, 0xFF, 0xFC, 0xFC, 0xFD, 0xFE, 0xAA, 0xAA }), bytes);
}

TEST(NvmBytesTest, values_read_back_from_the_stored_bytes)
{
    const Bytes bytes{ 0xFE, 0xFF, 0x0D, 0xF0, 0xFE, 0xCA, 0x00, 0x80 };

    EXPECT_EQ(-2, services::nvm::Get<int16_t>(infra::MakeConstByteRange(bytes), 0));
    EXPECT_EQ(0xCAFEF00Du, services::nvm::Get<uint32_t>(infra::MakeConstByteRange(bytes), 2));
    EXPECT_EQ(static_cast<int16_t>(0x8000), services::nvm::Get<int16_t>(infra::MakeConstByteRange(bytes), 6));
}
