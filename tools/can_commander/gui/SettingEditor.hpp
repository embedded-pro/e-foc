#pragma once

#include "core/can/FocMotorMessages.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include "tools/can_commander/logic/MotorSettings.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QString>
#include <QWidget>
#include <optional>

namespace tool
{
    // One setting as the controller holds it and as the operator edits it. The text shown for the
    // value in force is what an edit is compared with, so a value read back with more digits than
    // the field shows never reads as changed.
    class SettingEditor
        : public QWidget
    {
        Q_OBJECT

    public:
        explicit SettingEditor(const SettingPresentation& presentation, QWidget* parent = nullptr);

        const SettingPresentation& Presentation() const;
        can::FocParameter Parameter() const;
        QLabel* Label() const;

        void SetHeld(std::optional<can::FocParameterValue> value);
        void SetLegacy(bool legacyContract);
        void SetUnavailable(const QString& reason);
        void Revert();

        std::optional<float> Held() const;
        bool IsAvailable() const;
        bool IsEdited() const;
        bool IsValid() const;
        float EditedValue() const;
        QString Problem() const;
        QString Describe(float value) const;

    signals:
        void Edited();

    private:
        void BuildNumber(QHBoxLayout& layout);
        void BuildToggle(QHBoxLayout& layout);
        void BuildChoice(QHBoxLayout& layout);
        void ShowHeld();
        void UpdateEnabled();
        void MarkEdited();
        QString Format(float value) const;
        std::optional<float> Parse() const;
        bool IsAutomaticChosen() const;

        const SettingPresentation& presentation;
        const can::ParameterDescriptor descriptor;
        QLocale numberLocale;
        QLabel* label{ nullptr };
        QLineEdit* number{ nullptr };
        QCheckBox* automatic{ nullptr };
        QCheckBox* toggle{ nullptr };
        QComboBox* choice{ nullptr };

        std::optional<can::FocParameterValue> held;
        QString heldText;
        QString unavailableReason;
        bool legacy{ false };
    };
}
