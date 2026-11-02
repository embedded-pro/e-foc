#include "tools/firmware_image/Command.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>

namespace firmware_image
{
    namespace
    {
        constexpr const char* usage = R"(Usage: e_foc.tool.firmware_image <command> [options]

Commands:
  generate-keys --output <file>
      Write a new key set: an AES-128 image key and an ECDSA-P256 key pair, 112 bytes.
      Never overwrites an existing file.
  bootloader-keys --keys <key set> --output <file>
      Write the bootloader's 80-byte key slot (public key, then image key) from a key set.

Key sets and the bootloader key slot: documentation/design/bootloader.md, Parts G and J.
)";

        constexpr int success = 0;
        constexpr int failure = 1;

        std::optional<std::string> Option(const std::vector<std::string>& arguments, const std::string& name)
        {
            for (std::size_t index = 1; index + 1 < arguments.size(); ++index)
                if (arguments[index] == name)
                    return arguments[index + 1];
            return std::nullopt;
        }

        std::optional<std::vector<uint8_t>> ReadFile(const std::string& path)
        {
            std::ifstream file{ path, std::ios::binary };
            if (!file)
                return std::nullopt;
            return std::vector<uint8_t>{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
        }

        bool WriteFile(const std::string& path, std::span<const uint8_t> bytes)
        {
            std::ofstream file{ path, std::ios::binary | std::ios::trunc };
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            return static_cast<bool>(file);
        }

        int Missing(std::ostream& err, const std::string& command, const char* option)
        {
            err << command << " needs " << option << ".\n\n"
                << usage;
            return failure;
        }

        int GenerateKeys(const std::vector<std::string>& arguments, std::ostream& out, std::ostream& err, const RandomSource& random)
        {
            const auto output = Option(arguments, "--output");
            if (!output)
                return Missing(err, arguments[0], "--output <file>");

            if (std::filesystem::exists(*output))
            {
                err << *output << " already exists. A key set is never overwritten: choose another file name, or delete the old one if nothing still needs it.\n";
                return failure;
            }

            const auto keys = GenerateKeySet(random);
            if (!keys)
            {
                err << "The random source failed; no key set was written.\n";
                return failure;
            }

            if (!WriteFile(*output, Serialize(*keys)))
            {
                err << "Cannot write " << *output << ".\n";
                return failure;
            }

            out << *output << ": new key set written. Keep it secret: it signs the packs your bootloaders accept.\n"
                << "Store it as a repository secret: base64 -w0 " << *output << " | gh secret set BOOTLOADER_KEYS_DEVELOPMENT (or BOOTLOADER_KEYS_RELEASE)\n";
            return success;
        }

        int ReportKeySetError(std::ostream& err, const std::string& path, std::size_t size, KeySetError error)
        {
            if (error == KeySetError::wrongSize)
                err << path << " is not a key set: it holds " << size << " bytes, a key set holds " << keySetFileSize << ".\n";
            else
                err << path << " is damaged: its public key is not the one its private key gives. Restore it from the repository secret.\n";
            return failure;
        }

        int BootloaderKeys(const std::vector<std::string>& arguments, std::ostream& out, std::ostream& err)
        {
            const auto keysPath = Option(arguments, "--keys");
            const auto output = Option(arguments, "--output");
            if (!keysPath)
                return Missing(err, arguments[0], "--keys <key set>");
            if (!output)
                return Missing(err, arguments[0], "--output <file>");

            const auto file = ReadFile(*keysPath);
            if (!file)
            {
                err << "Cannot read the key set " << *keysPath << ".\n";
                return failure;
            }

            const auto parsed = ParseKeySet(*file);
            if (const auto* error = std::get_if<KeySetError>(&parsed))
                return ReportKeySetError(err, *keysPath, file->size(), *error);

            if (!WriteFile(*output, BootloaderKeySlot(std::get<KeySet>(parsed))))
            {
                err << "Cannot write " << *output << ".\n";
                return failure;
            }

            out << *output << ": bootloader key slot written from " << *keysPath << ".\n";
            return success;
        }
    }

    int Run(const std::vector<std::string>& arguments, std::ostream& out, std::ostream& err, const RandomSource& random)
    {
        if (arguments.empty())
        {
            err << usage;
            return failure;
        }

        const auto& command = arguments[0];
        if (command == "--help" || command == "-h" || command == "help")
        {
            out << usage;
            return success;
        }
        if (command == "generate-keys")
            return GenerateKeys(arguments, out, err, random);
        if (command == "bootloader-keys")
            return BootloaderKeys(arguments, out, err);

        err << "Unknown command '" << command << "'.\n\n"
            << usage;
        return failure;
    }
}
