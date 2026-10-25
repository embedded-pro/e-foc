#pragma once

#include "tools/can_commander/gui/SettingsPage.hpp"
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <array>
#include <optional>
#include <vector>

namespace tool
{
    // The loops of the active control mode, each with its controller and the settings that
    // controller uses; the loop's other settings stay stored on the controller, hidden here.
    class ControllersTab
        : public SettingsPage
    {
        Q_OBJECT

    public:
        explicit ControllersTab(QWidget* parent = nullptr);

        void SetMode(std::optional<can::FocMotorMode> activeMode);
        void SetAlgorithms(const can::FocAlgorithms& algorithms);
        void ForgetValues() override;
        void Revert() override;

        ChangePlan Plan() const override;
        QString Problem() const override;

    protected:
        bool Applies(const SettingEditor& editor) const override;
        int EditCount() const override;
        void Refresh() override;

    private:
        struct Loop
        {
            can::FocControlLoop id{};
            QGroupBox* group{ nullptr };
            QFormLayout* form{ nullptr };
            QLabel* algorithmLabel{ nullptr };
            QComboBox* algorithm{ nullptr };
            std::vector<SettingEditor*> editors;
            std::optional<uint8_t> held;
        };

        void BuildLoop(Loop& loop, can::FocControlLoop id);
        std::optional<uint8_t> ChosenAlgorithm(const Loop& loop) const;
        bool IsAlgorithmEdited(const Loop& loop) const;
        bool Shows(const Loop& loop, const SettingEditor& editor) const;
        bool IsLoopShown(const Loop& loop) const;
        const Loop* LoopOf(const SettingEditor& editor) const;
        void ShowHeldAlgorithm(Loop& loop);
        ChangePlan PlanLoops() const;
        QString Hint() const;

        QLabel* hint{ nullptr };
        std::array<Loop, 3> loops{};
        std::optional<can::FocMotorMode> mode;
    };
}
