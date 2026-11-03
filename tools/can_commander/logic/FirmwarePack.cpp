#include "tools/can_commander/logic/FirmwarePack.hpp"
#include "core/services/firmware_upgrade/PackHeaderCheck.hpp"
#include "infra/util/Crc.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <sstream>

namespace tool
{
    namespace
    {
        constexpr uint16_t knownHeaderVersion{ 1 };

        constexpr std::array<FirmwareBoard, 5> boards{ {
            { 1, "EK-TM4C1294XL", "TM4C1294NCPDT", E_FOC_EK_TM4C1294XL_PRODUCT_NAME, true },
            { 2, "NUCLEO-G474RE", "STM32G474RE", {}, false },
            { 3, "NUCLEO-H563ZI", "STM32H563ZI", {}, false },
            { 4, "EK-TM4C123GXL", "TM4C123GH6PM", {}, false },
            { 5, "emulated", "mps2-an386", E_FOC_EMULATED_PRODUCT_NAME, false },
        } };

        template<class T>
        bool ReadAt(const std::vector<uint8_t>& bytes, std::size_t offset, T& value)
        {
            if (offset > bytes.size() || bytes.size() - offset < sizeof(T))
                return false;

            std::memcpy(&value, bytes.data() + offset, sizeof(T));
            return true;
        }

        std::string ProductNameOf(const application::UpgradePackHeaderEpilogue& epilogue)
        {
            const auto end = std::find(epilogue.productName.begin(), epilogue.productName.end(), '\0');
            return std::string{ epilogue.productName.begin(), end };
        }

        void NameTarget(PackInspection& inspection, const std::string& productName)
        {
            std::istringstream words{ productName };
            std::string project;
            words >> project >> inspection.packBoard >> inspection.packMicrocontroller;
        }

        std::optional<PackProblem> SizeProblem(const std::vector<uint8_t>& bytes)
        {
            if (bytes.size() < services::minimumPackSize)
                return PackProblem::tooSmall;
            if (bytes.size() > stagingCapacity)
                return PackProblem::tooLarge;
            return std::nullopt;
        }

        uint32_t Crc32Of(const std::vector<uint8_t>& bytes)
        {
            infra::Crc32 crc;
            crc.Update(infra::ConstByteRange{ bytes.data(), bytes.data() + bytes.size() });
            return crc.Result();
        }
    }

    std::optional<FirmwareBoard> BoardOfKind(uint8_t kind)
    {
        const auto board = std::ranges::find(boards, kind, &FirmwareBoard::kind);
        if (board == boards.end())
            return std::nullopt;
        return *board;
    }

    bool CanBeUpdated(const FirmwareBoard& board)
    {
        return !board.productName.empty();
    }

    PackInspection InspectPack(const std::vector<uint8_t>& bytes, const FirmwareBoard& board)
    {
        PackInspection inspection;
        if (!CanBeUpdated(board))
            return PackInspection{ std::nullopt, PackProblem::boardHasNoBootloader, {}, {} };
        if (const auto problem = SizeProblem(bytes))
            return PackInspection{ std::nullopt, *problem, {}, {} };

        services::PackHeader header{};
        if (!ReadAt(bytes, 0, header.prologue) || header.prologue.magic != application::upgradePackMagic || !ReadAt(bytes, services::EpilogueOffset(header.prologue), header.epilogue))
            return PackInspection{ std::nullopt, PackProblem::notAPack, {}, {} };
        if (header.epilogue.headerVersion != knownHeaderVersion)
            return PackInspection{ std::nullopt, PackProblem::unknownHeaderVersion, {}, {} };

        const auto productName = ProductNameOf(header.epilogue);
        NameTarget(inspection, productName);
        if (productName != board.productName)
        {
            inspection.problem = PackProblem::anotherTarget;
            return inspection;
        }

        const auto version = ReadAt(bytes, header.epilogue.headerLength, header.image) ? services::CheckPackHeader(header, { infra::BoundedConstString{ board.productName.data(), board.productName.size() }, applicationCapacity, static_cast<uint32_t>(bytes.size()) }) : std::nullopt;
        if (!version)
        {
            inspection.problem = PackProblem::notAnApplication;
            return inspection;
        }

        inspection.pack = InspectedPack{ bytes, *version, Crc32Of(bytes) };
        return inspection;
    }

    std::string DescribePackProblem(const PackInspection& inspection, const FirmwareBoard& board)
    {
        switch (inspection.problem)
        {
            case PackProblem::boardHasNoBootloader:
                return "The " + std::string{ board.name } + " has no bootloader, so its firmware cannot be updated over CAN.";
            case PackProblem::tooSmall:
                return "The file is too small to be an upgrade pack.";
            case PackProblem::tooLarge:
                return "The pack is larger than the controller's 256 KB staging slot.";
            case PackProblem::notAPack:
                return "The file is not an e-foc upgrade pack.";
            case PackProblem::unknownHeaderVersion:
                return "The pack uses a header version this tool does not know.";
            case PackProblem::anotherTarget:
                return "The pack is for the " + inspection.packBoard + " (" + inspection.packMicrocontroller + "); this motor runs on the " + std::string{ board.name } + " (" + std::string{ board.microcontroller } + ").";
            case PackProblem::notAnApplication:
                return "The pack does not hold one application image with a release version this board can install.";
            default:
                return {};
        }
    }

    std::string VersionText(const services::FirmwareVersion& version)
    {
        return std::to_string(version.major) + "." + std::to_string(version.minor) + "." + std::to_string(version.patch);
    }
}
