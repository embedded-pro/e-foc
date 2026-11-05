#include "integration_tests/support/interactor/interfaces/Files.hpp"
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace integration
{
    std::optional<std::vector<uint8_t>> ReadBinaryFile(const std::string& path)
    {
        std::ifstream file{ path, std::ios::binary };
        if (!file)
            return std::nullopt;
        return std::vector<uint8_t>{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
    }

    std::vector<uint8_t> RequireBinaryFile(const std::string& path)
    {
        auto contents = ReadBinaryFile(path);
        if (!contents)
            throw std::runtime_error{ "Cannot read " + path };
        return std::move(*contents);
    }
}
