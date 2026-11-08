#include "core/services/firmware_upgrade/test_doubles/TestPack.hpp"
#include <cstring>

namespace services::test
{
    namespace
    {
        template<class T>
        void Append(std::vector<uint8_t>& bytes, const T& value)
        {
            const auto offset = bytes.size();
            bytes.resize(offset + sizeof(T));
            std::memcpy(bytes.data() + offset, &value, sizeof(T));
        }
    }

    application::StagedPackHeader MakeTestPackHeader(const TestPackDescription& description)
    {
        application::StagedPackHeader header{};
        header.prologue = application::UpgradePackHeaderPrologue{ application::UpgradePackStatus::readyToDeploy, description.magic, 0, 0, 2, testPackSignatureLength };
        header.epilogue.headerVersion = description.headerVersion;
        header.epilogue.headerLength = sizeof(header.prologue) + testPackSignatureLength + sizeof(header.epilogue);
        header.epilogue.numberOfImages = description.numberOfImages;
        header.epilogue.productName = Text<64>(description.productName);
        header.epilogue.productVersion = Text<64>(description.productVersion);
        header.epilogue.componentName = Text<64>("sync_foc_sensored");
        header.image.lengthOfHeaderAndImage = sizeof(header.image) + testPackAesCounterSize + sizeof(application::ImageHeaderEpilogue) + description.imageSize;
        header.image.targetName = Text<8>(description.target);
        header.image.encryptionAndMacMethod = 1;
        return header;
    }

    uint32_t TestPackSize(const TestPackDescription& description)
    {
        const auto header = MakeTestPackHeader(description);
        return header.epilogue.headerLength + header.image.lengthOfHeaderAndImage + description.trailing;
    }

    std::vector<uint8_t> MakeTestPack(const TestPackDescription& description)
    {
        const auto header = MakeTestPackHeader(description);
        std::vector<uint8_t> pack;
        Append(pack, header.prologue);
        pack.insert(pack.end(), testPackSignatureLength, 0x5a);
        Append(pack, header.epilogue);
        Append(pack, header.image);
        for (uint32_t i = 0; i != header.image.lengthOfHeaderAndImage - sizeof(header.image) + description.trailing; ++i)
            pack.push_back(static_cast<uint8_t>(i * 7 + 3));
        return pack;
    }
}
