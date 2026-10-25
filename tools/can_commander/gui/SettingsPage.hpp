#pragma once

#include "tools/can_commander/gui/SettingEditor.hpp"
#include "tools/can_commander/logic/MotorSettings.hpp"
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <span>
#include <vector>

namespace tool
{
    // A tab of settings the controller holds: each field shows the value in force until it is
    // edited, and Apply hands the edits to the motor panel, which sends them one at a time.
    class SettingsPage
        : public QWidget
    {
        Q_OBJECT

    public:
        explicit SettingsPage(QWidget* parent = nullptr);

        void ShowValue(can::FocParameter parameter, can::FocParameterValue value);
        virtual void ForgetValues();
        virtual void Revert();
        virtual void SetLegacy(bool legacyContract);
        void SetUnavailable(const QString& reason);
        void SetApplying(bool active);

        virtual ChangePlan Plan() const;
        virtual QString Problem() const;
        SettingEditor* EditorFor(can::FocParameter parameter) const;

    signals:
        void ApplyRequested();
        void RevertRequested();

    protected:
        QVBoxLayout& Content();
        QFormLayout& AddGroup(const QString& title, QGroupBox*& group);
        void AddEditors(QFormLayout& form, std::span<const SettingPresentation> settings);
        virtual bool Applies(const SettingEditor& editor) const;
        virtual int EditCount() const;
        virtual void Refresh();
        const std::vector<SettingEditor*>& Editors() const;
        bool IsLegacy() const;

    private:
        QWidget* CreateApplyBar();

        QVBoxLayout* content{ nullptr };
        QLabel* summary{ nullptr };
        QPushButton* applyButton{ nullptr };
        QPushButton* revertButton{ nullptr };
        std::vector<SettingEditor*> editors;
        QString unavailableReason;
        bool applying{ false };
        bool legacy{ false };
    };
}
