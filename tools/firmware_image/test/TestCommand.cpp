#include "tools/firmware_image/Command.hpp"
#include "tools/firmware_image/KeySet.hpp"
#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <iterator>
#include <sstream>
#include <variant>

namespace
{
    using namespace firmware_image;
    using testing::HasSubstr;

    class TestCommand
        : public ::testing::Test
    {
    public:
        TestCommand()
        {
            std::filesystem::create_directories(directory);
        }

        ~TestCommand() override
        {
            std::filesystem::remove_all(directory);
        }

        int Run(std::vector<std::string> arguments)
        {
            return firmware_image::Run(arguments, out, err, random);
        }

        std::string Path(const char* name) const
        {
            return (directory / name).string();
        }

        static std::vector<uint8_t> Read(const std::string& path)
        {
            std::ifstream file{ path, std::ios::binary };
            return { std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
        }

        static void Write(const std::string& path, const std::vector<uint8_t>& bytes)
        {
            std::ofstream file{ path, std::ios::binary };
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }

        std::filesystem::path directory{ std::filesystem::temp_directory_path() / ("firmware_image_" + std::string{ ::testing::UnitTest::GetInstance()->current_test_info()->name() }) };
        std::ostringstream out;
        std::ostringstream err;
        uint32_t counter{ 0 };
        RandomSource random{ [this](std::span<uint8_t> bytes)
            {
                for (auto& byte : bytes)
                    byte = static_cast<uint8_t>(++counter * 53);
                return true;
            } };
    };
}

TEST_F(TestCommand, help_lists_every_command)
{
    EXPECT_EQ(0, Run({ "--help" }));
    EXPECT_THAT(out.str(), HasSubstr("generate-keys"));
    EXPECT_THAT(out.str(), HasSubstr("bootloader-keys"));
}

TEST_F(TestCommand, no_command_shows_the_usage_and_fails)
{
    EXPECT_EQ(1, Run({}));
    EXPECT_THAT(err.str(), HasSubstr("Usage"));
}

TEST_F(TestCommand, an_unknown_command_is_named_and_fails)
{
    EXPECT_EQ(1, Run({ "make-coffee" }));
    EXPECT_THAT(err.str(), HasSubstr("make-coffee"));
}

TEST_F(TestCommand, generate_keys_writes_a_key_set)
{
    EXPECT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));

    EXPECT_TRUE(std::holds_alternative<KeySet>(ParseKeySet(Read(Path("keys.bin")))));
    EXPECT_THAT(out.str(), HasSubstr("secret"));
}

TEST_F(TestCommand, generate_keys_never_overwrites_a_file)
{
    Write(Path("keys.bin"), { 1, 2, 3 });

    EXPECT_EQ(1, Run({ "generate-keys", "--output", Path("keys.bin") }));

    EXPECT_EQ((std::vector<uint8_t>{ 1, 2, 3 }), Read(Path("keys.bin")));
    EXPECT_THAT(err.str(), HasSubstr("already exists"));
}

TEST_F(TestCommand, generate_keys_needs_an_output)
{
    EXPECT_EQ(1, Run({ "generate-keys" }));
    EXPECT_THAT(err.str(), HasSubstr("--output"));
}

TEST_F(TestCommand, bootloader_keys_writes_the_key_slot_of_a_key_set)
{
    ASSERT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));

    EXPECT_EQ(0, Run({ "bootloader-keys", "--keys", Path("keys.bin"), "--output", Path("slot.bin") }));

    const auto keys = std::get<KeySet>(ParseKeySet(Read(Path("keys.bin"))));
    const auto slot = BootloaderKeySlot(keys);
    EXPECT_EQ(std::vector<uint8_t>(slot.begin(), slot.end()), Read(Path("slot.bin")));
}

TEST_F(TestCommand, bootloader_keys_refuses_a_damaged_key_set_and_writes_nothing)
{
    ASSERT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));
    auto file = Read(Path("keys.bin"));
    file[20] ^= 0x01;
    Write(Path("keys.bin"), file);

    EXPECT_EQ(1, Run({ "bootloader-keys", "--keys", Path("keys.bin"), "--output", Path("slot.bin") }));

    EXPECT_FALSE(std::filesystem::exists(Path("slot.bin")));
    EXPECT_THAT(err.str(), HasSubstr("damaged"));
}

TEST_F(TestCommand, bootloader_keys_names_a_file_that_is_not_a_key_set)
{
    Write(Path("keys.bin"), { 1, 2, 3 });

    EXPECT_EQ(1, Run({ "bootloader-keys", "--keys", Path("keys.bin"), "--output", Path("slot.bin") }));

    EXPECT_THAT(err.str(), HasSubstr("not a key set"));
}

TEST_F(TestCommand, bootloader_keys_reports_a_missing_key_set)
{
    EXPECT_EQ(1, Run({ "bootloader-keys", "--keys", Path("absent.bin"), "--output", Path("slot.bin") }));

    EXPECT_THAT(err.str(), HasSubstr("Cannot read"));
}
