#include "tools/firmware_image/KeySet.hpp"
#include "uECC.h"
#include <gmock/gmock.h>
#include <variant>

namespace
{
    using namespace firmware_image;

    class TestKeySet
        : public ::testing::Test
    {
    public:
        RandomSource random{ [this](std::span<uint8_t> bytes)
            {
                for (auto& byte : bytes)
                    byte = static_cast<uint8_t>(++counter * 37);
                return true;
            } };

        KeySet Generate()
        {
            auto keys = GenerateKeySet(random);
            EXPECT_TRUE(keys.has_value());
            return keys.value_or(KeySet{});
        }

        uint32_t counter{ 0 };
    };
}

TEST_F(TestKeySet, a_generated_public_key_belongs_to_its_private_key)
{
    const auto keys = Generate();
    std::array<uint8_t, 64> computed{};

    ASSERT_EQ(1, uECC_compute_public_key(keys.privateKey.data(), computed.data(), uECC_secp256r1()));
    EXPECT_EQ(computed, keys.publicKey);
}

TEST_F(TestKeySet, the_image_key_comes_from_the_random_source)
{
    const auto keys = Generate();

    EXPECT_NE((std::array<uint8_t, 16>{}), keys.aesKey);
}

TEST_F(TestKeySet, two_generated_sets_differ)
{
    const auto first = Generate();
    const auto second = Generate();

    EXPECT_NE(first.privateKey, second.privateKey);
    EXPECT_NE(first.aesKey, second.aesKey);
}

TEST_F(TestKeySet, a_failing_random_source_gives_no_key_set)
{
    const RandomSource failing{ [](std::span<uint8_t>)
        {
            return false;
        } };

    EXPECT_FALSE(GenerateKeySet(failing).has_value());
}

TEST_F(TestKeySet, a_key_set_file_holds_the_image_key_the_public_key_and_the_private_key)
{
    const auto keys = Generate();
    const auto file = Serialize(keys);

    ASSERT_EQ(keySetFileSize, file.size());
    EXPECT_TRUE(std::equal(keys.aesKey.begin(), keys.aesKey.end(), file.begin()));
    EXPECT_TRUE(std::equal(keys.publicKey.begin(), keys.publicKey.end(), file.begin() + 16));
    EXPECT_TRUE(std::equal(keys.privateKey.begin(), keys.privateKey.end(), file.begin() + 80));
}

TEST_F(TestKeySet, a_written_key_set_reads_back_the_same)
{
    const auto keys = Generate();
    const auto parsed = ParseKeySet(Serialize(keys));

    ASSERT_TRUE(std::holds_alternative<KeySet>(parsed));
    EXPECT_EQ(keys, std::get<KeySet>(parsed));
}

TEST_F(TestKeySet, a_file_of_another_size_is_not_a_key_set)
{
    auto file = Serialize(Generate());
    file.pop_back();

    const auto parsed = ParseKeySet(file);

    ASSERT_TRUE(std::holds_alternative<KeySetError>(parsed));
    EXPECT_EQ(KeySetError::wrongSize, std::get<KeySetError>(parsed));
}

TEST_F(TestKeySet, a_public_key_that_does_not_belong_to_the_private_key_is_refused)
{
    auto file = Serialize(Generate());
    file[20] ^= 0x01;

    const auto parsed = ParseKeySet(file);

    ASSERT_TRUE(std::holds_alternative<KeySetError>(parsed));
    EXPECT_EQ(KeySetError::publicKeyMismatch, std::get<KeySetError>(parsed));
}

TEST_F(TestKeySet, the_bootloader_key_slot_holds_the_public_key_then_the_image_key)
{
    const auto keys = Generate();
    const auto slot = BootloaderKeySlot(keys);

    ASSERT_EQ(80u, slot.size());
    EXPECT_TRUE(std::equal(keys.publicKey.begin(), keys.publicKey.end(), slot.begin()));
    EXPECT_TRUE(std::equal(keys.aesKey.begin(), keys.aesKey.end(), slot.begin() + 64));
}

TEST_F(TestKeySet, the_bootloader_key_slot_carries_no_private_key_byte)
{
    const auto keys = Generate();
    const auto slot = BootloaderKeySlot(keys);

    EXPECT_EQ(slot.end(), std::search(slot.begin(), slot.end(), keys.privateKey.begin(), keys.privateKey.begin() + 8));
}
