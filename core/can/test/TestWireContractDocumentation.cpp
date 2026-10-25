#include "core/can/FocMotorWireContract.hpp"
#include <fstream>
#include <gtest/gtest.h>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    std::string DesignDocument()
    {
        std::ifstream file{ std::string{ E_FOC_SOURCE_DIR } + "/documentation/design/service-can.md" };
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::map<std::string, int32_t> DocumentedScales()
    {
        std::map<std::string, int32_t> scales;
        std::istringstream lines{ DesignDocument() };
        std::string line;

        while (std::getline(lines, line))
        {
            if (line.size() < 2 || line[0] != '|')
                continue;

            std::istringstream cells{ line };
            std::string quantity;
            std::string wireType;
            std::string scale;

            std::getline(cells, quantity, '|');
            std::getline(cells, quantity, '|');
            std::getline(cells, wireType, '|');
            std::getline(cells, scale, '|');

            const auto trim = [](std::string value)
            {
                const auto first = value.find_first_not_of(" \t");
                const auto last = value.find_last_not_of(" \t");
                return first == std::string::npos ? std::string{} : value.substr(first, last - first + 1);
            };

            quantity = trim(quantity);
            scale = trim(scale);

            if (quantity.empty() || scale.empty())
                continue;

            try
            {
                scales[quantity] = static_cast<int32_t>(std::stol(scale));
            }
            catch (...)
            {
                continue;
            }
        }

        return scales;
    }

    std::string Trim(const std::string& value)
    {
        const auto first = value.find_first_not_of(" \t`");
        const auto last = value.find_last_not_of(" \t`");
        return first == std::string::npos ? std::string{} : value.substr(first, last - first + 1);
    }

    std::vector<std::string> Cells(const std::string& line)
    {
        std::vector<std::string> cells;
        std::istringstream stream{ line };
        std::string cell;

        std::getline(stream, cell, '|');
        while (std::getline(stream, cell, '|'))
            cells.push_back(Trim(cell));

        return cells;
    }

    struct DocumentedParameter
    {
        std::string number;
        std::string unit;
        std::string automatic;
    };

    std::map<std::string, DocumentedParameter> DocumentedParameters()
    {
        std::map<std::string, DocumentedParameter> parameters;
        std::istringstream lines{ DesignDocument() };
        std::string line;

        while (std::getline(lines, line))
        {
            if (line.size() < 2 || line[0] != '|')
                continue;

            const auto cells = Cells(line);
            if (cells.size() < 5)
                continue;

            for (const auto& parameter : can::focParameters)
                if (cells[1] == parameter.name)
                    parameters[cells[1]] = DocumentedParameter{ cells[0], cells[2], cells[4] };
        }

        return parameters;
    }

    std::string Hex(uint8_t value)
    {
        static constexpr const char* digits{ "0123456789ABCDEF" };
        return std::string{ "0x" } + digits[value >> 4] + digits[value & 0x0F];
    }
}

TEST(WireContractDocumentation, the_scale_table_matches_the_contract)
{
    const auto documented = DocumentedScales();
    ASSERT_FALSE(documented.empty()) << "no scale table found in service-can.md";

    EXPECT_EQ(can::focCurrentScale, documented.at("Current"));
    EXPECT_EQ(can::focSpeedScale, documented.at("Speed"));
    EXPECT_EQ(can::focPositionScale, documented.at("Position"));
    EXPECT_EQ(can::focVoltageScale, documented.at("Voltage"));
    EXPECT_EQ(can::focPidScale, documented.at("Bandwidth"));
    EXPECT_EQ(can::focResistanceScale, documented.at("Resistance"));
    EXPECT_EQ(can::focInductanceScale, documented.at("Inductance"));
    EXPECT_EQ(can::focFrictionScale, documented.at("Friction"));
    EXPECT_EQ(can::focInertiaScale, documented.at("Inertia"));
}

TEST(WireContractDocumentation, every_scale_the_contract_uses_is_documented)
{
    const auto documented = DocumentedScales();

    const auto isDocumented = [&documented](int32_t scale)
    {
        for (const auto& entry : documented)
        {
            if (entry.second == scale)
                return true;
        }

        return false;
    };

    const auto check = [&isDocumented](const auto& messages)
    {
        for (const auto& descriptor : messages)
        {
            for (uint8_t i = 0; i != descriptor.fieldCount; ++i)
            {
                if (descriptor.fields[i].type == can::FieldType::fixed16)
                {
                    EXPECT_TRUE(isDocumented(descriptor.fields[i].scale))
                        << descriptor.name << "." << descriptor.fields[i].name;
                }
            }
        }
    };

    check(can::focMotorMessages);
    check(can::focMotorResponses);
}

TEST(WireContractDocumentation, the_documented_version_matches_the_contract)
{
    const auto document = DesignDocument();

    EXPECT_NE(std::string::npos, document.find("QueryContractVersion"));
    EXPECT_NE(std::string::npos, document.find("ContractVersionResponse"));
}

TEST(WireContractDocumentation, every_parameter_is_documented_with_its_number_unit_and_automatic_flag)
{
    const auto documented = DocumentedParameters();

    for (const auto& parameter : can::focParameters)
    {
        const auto entry = documented.find(parameter.name);
        ASSERT_NE(documented.end(), entry) << parameter.name;
        EXPECT_EQ(Hex(static_cast<uint8_t>(parameter.id)), entry->second.number) << parameter.name;
        EXPECT_EQ(std::string{ parameter.unit }, entry->second.unit) << parameter.name;
        EXPECT_EQ(std::string{ parameter.automatic ? "yes" : "no" }, entry->second.automatic) << parameter.name;
    }

    EXPECT_EQ(can::focParameters.size(), documented.size());
}

TEST(WireContractDocumentation, the_algorithm_and_parameter_messages_are_documented)
{
    const auto document = DesignDocument();

    for (const auto* name : { "SelectAlgorithm", "QueryAlgorithms", "SetParameter", "QueryParameter", "AlgorithmsResponse", "ParameterResponse", "invalidParameters" })
        EXPECT_NE(std::string::npos, document.find(name)) << name;
}
