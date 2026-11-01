#pragma once

#include "infra/util/ByteRange.hpp"
#include "services/crypto/Sha256Software.hpp"
#include "uECC.h"
#include "upgrade/boot_loader/DecryptorAesTiny.hpp"
#include "upgrade/pack/UpgradePackHeader.hpp"
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace bootloader
{
    inline constexpr std::array<uint8_t, 16> testAesKey{ 0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87, 0x98, 0xa9, 0xba, 0xcb, 0xdc, 0xed, 0xfe, 0x0f };
    inline constexpr const char* tm4c1294ProductName = "e-foc EK-TM4C1294XL TM4C1294NCPDT";

    struct TestKeyPair
    {
        TestKeyPair()
        {
            EXPECT_EQ(1, uECC_make_key(publicKey.data(), privateKey.data(), uECC_secp256r1()));
        }

        std::array<uint8_t, 64> publicKey{};
        std::array<uint8_t, 32> privateKey{};
    };

    struct PackDescription
    {
        std::vector<uint8_t> image;
        std::string productName{ tm4c1294ProductName };
        uint32_t destination{ 0x00008000 };
        std::array<uint8_t, 16> initialCounter{ 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf };
    };

    template<class T>
    void Append(std::vector<uint8_t>& bytes, const T& value)
    {
        const auto range = infra::MakeByteRange(value);
        bytes.insert(bytes.end(), range.begin(), range.end());
    }

    inline void CopyName(std::array<char, 64>& field, const std::string& name)
    {
        field.fill(0);
        std::memcpy(field.data(), name.data(), std::min(name.size(), field.size() - 1));
    }

    inline std::vector<uint8_t> SignedContents(const PackDescription& pack)
    {
        application::UpgradePackHeaderEpilogue epilogue{};
        epilogue.headerVersion = 1;
        epilogue.headerLength = sizeof(application::UpgradePackHeaderPrologue) + 64 + sizeof(application::UpgradePackHeaderEpilogue);
        epilogue.numberOfImages = 1;
        CopyName(epilogue.productName, pack.productName);
        CopyName(epilogue.productVersion, "v1.7.0");
        CopyName(epilogue.componentName, "sync_foc_sensored");

        const auto paddedSize = (pack.image.size() + 3) / 4 * 4;
        application::ImageHeaderPrologue imagePrologue{};
        imagePrologue.lengthOfHeaderAndImage = static_cast<uint32_t>(sizeof(imagePrologue) + pack.initialCounter.size() + sizeof(application::ImageHeaderEpilogue) + paddedSize);
        std::memcpy(imagePrologue.targetName.data(), "app", 3);
        imagePrologue.encryptionAndMacMethod = 1;

        std::vector<uint8_t> encrypted;
        Append(encrypted, application::ImageHeaderEpilogue{ pack.destination, static_cast<uint32_t>(pack.image.size()) });
        encrypted.insert(encrypted.end(), pack.image.begin(), pack.image.end());
        encrypted.resize(sizeof(application::ImageHeaderEpilogue) + paddedSize, 0);

        application::DecryptorAesTiny encryptor{ infra::MakeRange(testAesKey) };
        auto state = encryptor.StateBuffer();
        std::copy(pack.initialCounter.begin(), pack.initialCounter.end(), state.begin());
        encryptor.Reset();
        encryptor.DecryptPart(infra::MakeRange(encrypted));

        std::vector<uint8_t> contents;
        Append(contents, epilogue);
        Append(contents, imagePrologue);
        contents.insert(contents.end(), pack.initialCounter.begin(), pack.initialCounter.end());
        contents.insert(contents.end(), encrypted.begin(), encrypted.end());
        return contents;
    }

    inline std::vector<uint8_t> BuildPack(const PackDescription& pack, const TestKeyPair& keys)
    {
        const auto contents = SignedContents(pack);
        const auto digest = services::Sha256Software{}.Calculate(infra::MakeRange(contents));

        std::array<uint8_t, 64> signature{};
        EXPECT_EQ(1, uECC_sign(keys.privateKey.data(), digest.data(), static_cast<unsigned>(digest.size()), signature.data(), uECC_secp256r1()));

        application::UpgradePackHeaderPrologue prologue{};
        prologue.status = application::UpgradePackStatus::empty;
        prologue.magic = application::upgradePackMagic;
        prologue.errorCode = 0xffffffff;
        prologue.signedContentsLength = static_cast<uint32_t>(contents.size());
        prologue.signatureMethod = 2;
        prologue.signatureLength = static_cast<uint16_t>(signature.size());

        std::vector<uint8_t> bytes;
        Append(bytes, prologue);
        bytes.insert(bytes.end(), signature.begin(), signature.end());
        bytes.insert(bytes.end(), contents.begin(), contents.end());
        return bytes;
    }
}
