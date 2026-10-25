#include "tools/can_commander/gui/SettingsPage.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QHBoxLayout>
#include <QScrollArea>
#include <algorithm>

namespace tool
{
    SettingsPage::SettingsPage(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);

        auto* scroll = new QScrollArea();
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto* inner = new QWidget();
        content = new QVBoxLayout(inner);
        content->setContentsMargins(0, 0, 0, 0);
        scroll->setWidget(inner);

        layout->addWidget(scroll, 1);
        layout->addWidget(CreateApplyBar());
    }

    QWidget* SettingsPage::CreateApplyBar()
    {
        auto* bar = new QWidget();
        auto* layout = new QHBoxLayout(bar);
        layout->setContentsMargins(0, 0, 0, 0);

        summary = new QLabel();
        summary->setWordWrap(true);
        revertButton = new QPushButton("&Revert");
        revertButton->setToolTip("Drop the changes and read the values in force again");
        applyButton = new QPushButton("&Apply");
        layout->addWidget(summary, 1);
        layout->addWidget(revertButton);
        layout->addWidget(applyButton);

        connect(applyButton, &QPushButton::clicked, this, &SettingsPage::ApplyRequested);
        connect(revertButton, &QPushButton::clicked, this, [this]()
            {
                Revert();
                emit RevertRequested();
            });
        return bar;
    }

    QVBoxLayout& SettingsPage::Content()
    {
        return *content;
    }

    QFormLayout& SettingsPage::AddGroup(const QString& title, QGroupBox*& group)
    {
        group = new QGroupBox(title);
        auto* form = new QFormLayout(group);
        content->addWidget(group);
        return *form;
    }

    void SettingsPage::AddEditors(QFormLayout& form, std::span<const SettingPresentation> settings)
    {
        for (const auto& setting : settings)
        {
            auto* editor = new SettingEditor(setting);
            form.addRow(editor->Label(), editor);
            editors.push_back(editor);
            connect(editor, &SettingEditor::Edited, this, [this]()
                {
                    Refresh();
                });
        }
    }

    void SettingsPage::ShowValue(can::FocParameter parameter, can::FocParameterValue value)
    {
        if (auto* editor = EditorFor(parameter))
        {
            editor->SetHeld(value);
            Refresh();
        }
    }

    void SettingsPage::ForgetValues()
    {
        for (auto* editor : editors)
            editor->SetHeld(std::nullopt);
        Refresh();
    }

    void SettingsPage::Revert()
    {
        for (auto* editor : editors)
            editor->Revert();
        Refresh();
    }

    void SettingsPage::SetLegacy(bool legacyContract)
    {
        legacy = legacyContract;
        for (auto* editor : editors)
            editor->SetLegacy(legacy);
        Refresh();
    }

    void SettingsPage::SetUnavailable(const QString& reason)
    {
        unavailableReason = reason;
        Refresh();
    }

    void SettingsPage::SetApplying(bool active)
    {
        applying = active;
        Refresh();
    }

    ChangePlan SettingsPage::Plan() const
    {
        ChangePlan plan;
        for (const auto* editor : editors)
            if (Applies(*editor) && editor->IsEdited() && editor->IsValid())
                plan.changes.push_back(SettingChange::Parameter(editor->Parameter(), editor->EditedValue()));
        return plan;
    }

    QString SettingsPage::Problem() const
    {
        for (const auto* editor : editors)
            if (Applies(*editor) && !editor->IsValid())
                return editor->Problem();
        return {};
    }

    SettingEditor* SettingsPage::EditorFor(can::FocParameter parameter) const
    {
        const auto editor = std::find_if(editors.begin(), editors.end(), [parameter](const SettingEditor* candidate)
            {
                return candidate->Parameter() == parameter;
            });
        return editor != editors.end() ? *editor : nullptr;
    }

    bool SettingsPage::Applies(const SettingEditor& editor) const
    {
        return editor.IsAvailable();
    }

    int SettingsPage::EditCount() const
    {
        return static_cast<int>(std::count_if(editors.begin(), editors.end(), [this](const SettingEditor* editor)
            {
                return Applies(*editor) && editor->IsEdited();
            }));
    }

    void SettingsPage::Refresh()
    {
        const auto problem = Problem();
        const auto edits = EditCount();

        auto level = ui::theme::StatusLevel::Neutral;
        QString text;
        if (applying)
            text = "Applying…";
        else if (!problem.isEmpty())
            text = problem;
        else if (edits > 0)
            text = edits == 1 ? QString("1 change not applied") : QString("%1 changes not applied").arg(edits);

        if (!text.isEmpty())
            level = ui::theme::StatusLevel::Warning;
        summary->setText(text);
        ui::backend::qt::StyleStatusLabel(*summary, level);

        const bool available = unavailableReason.isEmpty() && !applying;
        applyButton->setEnabled(available && edits > 0 && problem.isEmpty());
        applyButton->setToolTip(!unavailableReason.isEmpty() ? unavailableReason
                                : !problem.isEmpty()         ? problem
                                : edits == 0                 ? QString("Nothing changed")
                                                             : QString("Send the changed values to the controller, one at a time"));
        revertButton->setEnabled(available || (edits > 0 && !applying));
    }

    const std::vector<SettingEditor*>& SettingsPage::Editors() const
    {
        return editors;
    }

    bool SettingsPage::IsLegacy() const
    {
        return legacy;
    }
}
