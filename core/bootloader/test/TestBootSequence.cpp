#include "core/bootloader/BootSequence.hpp"
#include "core/bootloader/test/FlashFake.hpp"
#include "core/bootloader/test/PackBuilder.hpp"
#include "core/bootloader/test/TestImages.hpp"
#include "upgrade/boot_loader/DecryptorAesTiny.hpp"
#include "upgrade/boot_loader/VerifierEcDsa.hpp"
#include <gmock/gmock.h>

namespace
{
    using namespace bootloader;
    using application::UpgradePackStatus;
    using testing::StrictMock;

    constexpr uint32_t sectorSize = 1024;
    constexpr uint32_t slotSectors = 16;
    constexpr uint32_t slotSize = sectorSize * slotSectors;
    constexpr TargetLayout layout{ 0x20000000, 0x20040000, 0x00008000, 0x00008000 + slotSize };

    class ApplicationStarterMock
        : public ApplicationStarter
    {
    public:
        MOCK_METHOD(void, Start, (uint32_t slotBegin), (override));
        MOCK_METHOD(void, Halt, (), (override));
    };

    class TestBootSequence
        : public ::testing::Test
    {
    public:
        void Stage(const PackDescription& pack, UpgradePackStatus status)
        {
            packFlash.Load(BuildPack(pack, keys));
            journal.Record(status, 0);
        }

        void InstallOldApplication()
        {
            slotFlash.Load(Slot(oldImage, slotSize));
        }

        void ExpectStarted()
        {
            EXPECT_CALL(starter, Start(layout.slotBegin));
        }

        void ExpectHalted()
        {
            EXPECT_CALL(starter, Halt());
        }

        void Boot()
        {
            BootSequence{ dependencies }.Run();
        }

        bool SlotHolds(const std::vector<uint8_t>& image) const
        {
            return std::equal(image.begin(), image.end(), slotFlash.memory.begin());
        }

        TestKeyPair keys;
        std::vector<uint8_t> oldImage{ GoodImage(4096, 0x11) };
        std::vector<uint8_t> newImage{ GoodImage(6000, 0x22) };
        FlashFake packFlash{ 32, sectorSize };
        FlashFake journalFlash{ 1, 256 };
        FlashFake slotFlash{ slotSectors, sectorSize };
        StatusJournal journal{ journalFlash };
        application::DecryptorAesTiny decryptor{ infra::MakeRange(testAesKey) };
        application::DecryptorAesTiny checkDecryptor{ infra::MakeRange(testAesKey) };
        application::VerifierEcDsa256 verifier{ infra::MakeRange(keys.publicKey) };
        std::array<uint8_t, imageStartCheckSize> buffer{};
        ApplicationInstaller installer{ decryptor, checkDecryptor, slotFlash, layout, tm4c1294Identity, infra::MakeRange(buffer) };
        StrictMock<ApplicationStarterMock> starter;
        BootSequence::Dependencies dependencies{ packFlash, journal, installer, decryptor, verifier, tm4c1294ProductName,
            infra::MakeRange(slotFlash.memory), layout, tm4c1294Identity, starter };
    };
}

TEST_F(TestBootSequence, an_intact_application_starts_when_nothing_is_staged)
{
    InstallOldApplication();
    ExpectStarted();

    Boot();
}

TEST_F(TestBootSequence, an_erased_slot_halts)
{
    ExpectHalted();

    Boot();
}

TEST_F(TestBootSequence, a_pack_ready_to_deploy_is_installed_and_started)
{
    InstallOldApplication();
    Stage(PackDescription{ newImage }, UpgradePackStatus::readyToDeploy);
    ExpectStarted();

    Boot();

    EXPECT_TRUE(SlotHolds(newImage));
    EXPECT_EQ(UpgradePackStatus::deployed, journal.Status());
}

TEST_F(TestBootSequence, a_downloaded_pack_waits_for_the_apply)
{
    InstallOldApplication();
    Stage(PackDescription{ newImage }, UpgradePackStatus::downloaded);
    ExpectStarted();

    Boot();

    EXPECT_TRUE(SlotHolds(oldImage));
    EXPECT_EQ(UpgradePackStatus::downloaded, journal.Status());
}

TEST_F(TestBootSequence, an_interrupted_installation_is_repeated)
{
    InstallOldApplication();
    Stage(PackDescription{ newImage }, UpgradePackStatus::deployStarted);
    ExpectStarted();

    Boot();

    EXPECT_TRUE(SlotHolds(newImage));
    EXPECT_EQ(UpgradePackStatus::deployed, journal.Status());
}

TEST_F(TestBootSequence, a_pack_with_a_bad_signature_leaves_the_old_application)
{
    InstallOldApplication();
    auto pack = BuildPack(PackDescription{ newImage }, keys);
    pack.back() ^= 0x01;
    packFlash.Load(pack);
    journal.Record(UpgradePackStatus::readyToDeploy, 0);
    ExpectStarted();

    Boot();

    EXPECT_TRUE(SlotHolds(oldImage));
    EXPECT_EQ(0u, slotFlash.erasures);
    EXPECT_EQ(UpgradePackStatus::invalid, journal.Status());
    EXPECT_EQ(application::upgradeErrorCodeInvalidSignature, journal.ErrorCode());
}

TEST_F(TestBootSequence, a_pack_for_another_product_leaves_the_old_application)
{
    InstallOldApplication();
    Stage(PackDescription{ newImage, "e-foc NUCLEO-H563ZI STM32H563ZI" }, UpgradePackStatus::readyToDeploy);
    ExpectStarted();

    Boot();

    EXPECT_TRUE(SlotHolds(oldImage));
    EXPECT_EQ(0u, slotFlash.erasures);
    EXPECT_EQ(application::upgradeErrorCodeUnknownProductName, journal.ErrorCode());
}

TEST_F(TestBootSequence, a_signed_pack_whose_image_is_for_another_microcontroller_is_never_written)
{
    InstallOldApplication();
    Stage(PackDescription{ Image(stm32h563Identity, 0x20040000, 0x00008441, 6000) }, UpgradePackStatus::readyToDeploy);
    ExpectStarted();

    Boot();

    EXPECT_TRUE(SlotHolds(oldImage));
    EXPECT_EQ(0u, slotFlash.erasures);
    EXPECT_EQ(UpgradePackStatus::invalid, journal.Status());
    EXPECT_EQ(errorWrongTarget, journal.ErrorCode());
}

TEST_F(TestBootSequence, a_signed_pack_whose_vectors_point_into_another_chips_flash_is_never_written)
{
    InstallOldApplication();
    Stage(PackDescription{ Image(tm4c1294Identity, 0x20040000, 0x08008441, 6000) }, UpgradePackStatus::readyToDeploy);
    ExpectStarted();

    Boot();

    EXPECT_TRUE(SlotHolds(oldImage));
    EXPECT_EQ(errorWrongTarget, journal.ErrorCode());
}

TEST_F(TestBootSequence, an_image_for_another_destination_is_never_written)
{
    InstallOldApplication();
    Stage(PackDescription{ newImage, tm4c1294ProductName, 0x00000000 }, UpgradePackStatus::readyToDeploy);
    ExpectStarted();

    Boot();

    EXPECT_TRUE(SlotHolds(oldImage));
    EXPECT_EQ(errorWrongDestination, journal.ErrorCode());
}

TEST_F(TestBootSequence, an_image_larger_than_the_slot_is_never_written)
{
    InstallOldApplication();
    Stage(PackDescription{ GoodImage(slotSize - imageDescriptorSize + 4) }, UpgradePackStatus::readyToDeploy);
    ExpectStarted();

    Boot();

    EXPECT_TRUE(SlotHolds(oldImage));
    EXPECT_EQ(errorImageSize, journal.ErrorCode());
}

TEST_F(TestBootSequence, an_image_filling_the_slot_up_to_its_descriptor_is_installed)
{
    InstallOldApplication();
    const auto largest = GoodImage(slotSize - imageDescriptorSize, 0x33);
    Stage(PackDescription{ largest }, UpgradePackStatus::readyToDeploy);
    ExpectStarted();

    Boot();

    EXPECT_TRUE(SlotHolds(largest));
}

TEST_F(TestBootSequence, an_invalid_pack_is_not_tried_again)
{
    InstallOldApplication();
    Stage(PackDescription{ newImage, "e-foc NUCLEO-H563ZI STM32H563ZI" }, UpgradePackStatus::readyToDeploy);
    EXPECT_CALL(starter, Start(layout.slotBegin)).Times(2);

    Boot();
    Boot();

    EXPECT_EQ(0u, slotFlash.erasures);
}
