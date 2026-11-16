#include "tools/virtual_motor/platform/PlantDescription.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Crc.hpp"
#include "motor_parameters/PlantPresets.hpp"
#include "numerical/math/Math.hpp"
#include <array>
#include <charconv>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string_view>

namespace virtual_motor
{
    namespace
    {
        uint32_t ReadUint32(const uint8_t* source)
        {
            uint32_t value{ 0 };
            std::memcpy(&value, source, sizeof(value));
            return value;
        }

        std::optional<sil::SilPlantConfig> ReadPlantFile(const std::string& path)
        {
            std::ifstream file{ path, std::ios::binary };
            std::array<uint8_t, sil::plantConfigRecordSize> record{};
            if (!file.read(reinterpret_cast<char*>(record.data()), record.size()))
                return std::nullopt;

            if (ReadUint32(record.data() + sil::plantConfigMagicOffset) != sil::plantConfigMagic || record[sil::plantConfigVersionOffset] != sil::plantConfigLayoutVersion)
                return std::nullopt;

            sil::SilPlantConfig config{};
            std::memcpy(&config, record.data() + sil::plantConfigDataOffset, sizeof(config));

            infra::Crc32 crc;
            crc.Update(infra::MakeConstByteRange(config));
            if (crc.Result() != ReadUint32(record.data() + sil::plantConfigCrcOffset))
                return std::nullopt;

            return config;
        }

        constexpr std::string_view textHeader{ "e-foc plant description 1" };

        enum class Range : uint8_t
        {
            positive,
            notNegative
        };

        struct TextKey
        {
            std::string_view name;
            Range range;
            void (*apply)(sil::SilPlantConfig& config, float value);
        };

        const std::array<TextKey, 9> textKeys{ {
            { "resistance", Range::positive, [](sil::SilPlantConfig& config, float value)
                {
                    config.statorResistanceOhm = value;
                } },
            { "inductance", Range::positive, [](sil::SilPlantConfig& config, float value)
                {
                    config.dAxisInductanceHenry = value;
                    config.qAxisInductanceHenry = value;
                } },
            { "flux-linkage", Range::positive, [](sil::SilPlantConfig& config, float value)
                {
                    config.fluxLinkageWeber = value;
                } },
            { "rotor-inertia", Range::positive, [](sil::SilPlantConfig& config, float value)
                {
                    config.rotorInertiaKgM2 = value;
                } },
            { "viscous-friction", Range::notNegative, [](sil::SilPlantConfig& config, float value)
                {
                    config.viscousDampingNmSPerRad = value;
                } },
            { "coulomb-friction", Range::notNegative, [](sil::SilPlantConfig& config, float value)
                {
                    config.coulombFrictionNm = value;
                } },
            { "max-current", Range::positive, [](sil::SilPlantConfig& config, float value)
                {
                    config.maxSupportedCurrentAmpere = value;
                } },
            { "supply-voltage", Range::positive, [](sil::SilPlantConfig& config, float value)
                {
                    config.powerSupplyVoltageVolts = value;
                } },
            { "pole-pairs", Range::positive, [](sil::SilPlantConfig& config, float value)
                {
                    config.polePairs = static_cast<uint8_t>(value);
                } },
        } };

        std::optional<float> ParseNumber(std::string_view text, Range range)
        {
            float value{ 0.0f };
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size() || !math::IsFinite(value))
                return std::nullopt;
            if (range == Range::positive ? value <= 0.0f : value < 0.0f)
                return std::nullopt;
            return value;
        }

        bool ValidPolePairs(float value)
        {
            return value <= 255.0f && value == static_cast<float>(static_cast<uint32_t>(value));
        }

        class TextReader
        {
        public:
            std::optional<sil::SilPlantConfig> Read(std::istream& lines)
            {
                std::string line;
                while (std::getline(lines, line))
                    if (!line.empty() && !Apply(line))
                        return std::nullopt;

                config.rotorInertiaKgM2 += loadInertia;
                return config;
            }

        private:
            bool Apply(const std::string& line)
            {
                std::istringstream words{ line };
                std::string key;
                std::string value;
                std::string extra;
                if (!(words >> key >> value) || (words >> extra) || !seen.insert(key).second)
                    return false;

                if (key == "preset")
                    return seen.size() == 1 && sil::TryNamedPlant(value, config);
                if (key == "load-inertia")
                    return Store(ParseNumber(value, Range::notNegative), loadInertia);

                for (const auto& textKey : textKeys)
                    if (textKey.name == key)
                        return ApplyValue(textKey, value);

                return false;
            }

            bool ApplyValue(const TextKey& textKey, const std::string& text)
            {
                const auto value = ParseNumber(text, textKey.range);
                if (!value.has_value() || (textKey.name == "pole-pairs" && !ValidPolePairs(*value)))
                    return false;

                textKey.apply(config, *value);
                return true;
            }

            static bool Store(std::optional<float> value, float& result)
            {
                if (!value.has_value())
                    return false;
                result = *value;
                return true;
            }

            sil::SilPlantConfig config{ sil::NominalPlant() };
            float loadInertia{ 0.0f };
            std::set<std::string, std::less<>> seen;
        };

        std::optional<sil::SilPlantConfig> ReadTextFile(const std::string& path, bool& isText)
        {
            std::ifstream file{ path };
            std::string header;
            isText = std::getline(file, header) && header == textHeader;
            if (!isText)
                return std::nullopt;

            return TextReader{}.Read(file);
        }
    }

    std::optional<sil::SilPlantConfig> PlantDescription(const std::string& presetOrFile)
    {
        sil::SilPlantConfig config{};
        if (sil::TryNamedPlant(presetOrFile, config))
            return config;

        bool isText{ false };
        auto text = ReadTextFile(presetOrFile, isText);
        if (isText)
            return text;

        return ReadPlantFile(presetOrFile);
    }
}
