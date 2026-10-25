#pragma once

#include "core/can/FocMotorMessages.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace tool
{
    struct SettingPresentation
    {
        can::FocParameter parameter{};
        std::string_view name;
        std::string_view label;
        std::string_view suffix;
        std::string_view description;
        std::string_view automaticLabel;
        uint32_t usedBy{ 0xffffffff };
    };

    struct SettingChange
    {
        enum class Kind : uint8_t
        {
            parameter,
            algorithm
        };

        static SettingChange Parameter(can::FocParameter id, float newValue);
        static SettingChange Algorithm(can::FocControlLoop controlLoop, uint8_t index);

        Kind kind{ Kind::parameter };
        can::FocParameter parameter{};
        float value{ 0.0f };
        can::FocControlLoop loop{};
        uint8_t algorithm{ 0 };
    };

    struct LoopEdits
    {
        can::FocControlLoop loop{};
        std::optional<uint8_t> heldAlgorithm;
        std::optional<uint8_t> algorithm;
        std::vector<SettingChange> settings;
        std::optional<float> heldSwitchingGain;
        std::optional<float> heldBoundaryLayer;
    };

    struct ChangePlan
    {
        std::vector<SettingChange> changes;
        std::string_view problem;
    };

    std::span<const can::FocControlLoop> LoopsOf(can::FocMotorMode mode);
    std::string_view LoopName(can::FocControlLoop loop);
    std::string_view AlgorithmName(can::FocControlLoop loop, uint8_t algorithm);
    uint8_t AlgorithmOf(const can::FocAlgorithms& algorithms, can::FocControlLoop loop);
    bool RefusesSettings(can::FocControlLoop loop, uint8_t algorithm);

    std::span<const SettingPresentation> LoopSettings(can::FocControlLoop loop);
    std::span<const SettingPresentation> MotorParameterSettings();
    std::span<const SettingPresentation> StartupSettings();
    const SettingPresentation& PresentationOf(can::FocParameter parameter);
    bool IsUsedBy(const SettingPresentation& setting, uint8_t algorithm);
    bool IsBandwidth(can::FocParameter parameter);
    bool IsInteger(can::FocParameter parameter);
    bool IsAcceptableEntry(can::FocParameter parameter, float value, bool legacy);
    std::optional<uint8_t> LegacyCommandOf(can::FocParameter parameter);
    uint8_t CommandTypeOf(const SettingChange& change, bool legacy);

    ChangePlan PlanLoop(const LoopEdits& edits);

    class ChangeSequence
    {
    public:
        void Start(std::vector<SettingChange> sequence);
        void Stop();
        bool Advance();

        bool Active() const;
        const SettingChange& Current() const;
        std::size_t Remaining() const;

    private:
        std::vector<SettingChange> changes;
        std::size_t current{ 0 };
        bool active{ false };
    };
}
