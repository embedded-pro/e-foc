#include "tools/can_commander/gui/ControllersTab.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QStringList>
#include <algorithm>

namespace tool
{
    namespace
    {
        using ui::backend::qt::ToQt;
    }

    ControllersTab::ControllersTab(QWidget* parent)
        : SettingsPage(parent)
    {
        hint = new QLabel();
        hint->setWordWrap(true);
        ui::backend::qt::StyleStatusLabel(*hint, ui::theme::StatusLevel::Warning);
        Content().addWidget(hint);

        BuildLoop(loops[0], can::FocControlLoop::current);
        BuildLoop(loops[1], can::FocControlLoop::speed);
        BuildLoop(loops[2], can::FocControlLoop::position);
        Content().addStretch();

        Refresh();
    }

    void ControllersTab::BuildLoop(Loop& loop, can::FocControlLoop id)
    {
        loop.id = id;
        loop.form = &AddGroup(ToQt(LoopName(id)), loop.group);

        loop.algorithm = new QComboBox();
        loop.algorithm->setPlaceholderText("—");
        loop.algorithm->setAccessibleName(QString("%1 controller").arg(ToQt(LoopName(id))));
        loop.algorithm->setToolTip("The control law this loop runs. Only the settings it uses are shown.");
        for (uint8_t index = 0; index != can::AlgorithmCount(id); ++index)
            loop.algorithm->addItem(ToQt(AlgorithmName(id, index)), static_cast<int>(index));

        loop.algorithmLabel = new QLabel("Controller:");
        loop.algorithmLabel->setBuddy(loop.algorithm);
        loop.form->addRow(loop.algorithmLabel, loop.algorithm);

        const auto first = static_cast<std::ptrdiff_t>(Editors().size());
        AddEditors(*loop.form, LoopSettings(id));
        loop.editors.assign(Editors().begin() + first, Editors().end());

        connect(loop.algorithm, &QComboBox::activated, this, [this]()
            {
                Refresh();
            });
    }

    void ControllersTab::SetMode(std::optional<can::FocMotorMode> activeMode)
    {
        mode = activeMode;
        Refresh();
    }

    void ControllersTab::SetAlgorithms(const can::FocAlgorithms& algorithms)
    {
        mode = algorithms.mode;
        for (auto& loop : loops)
        {
            const bool edited = IsAlgorithmEdited(loop);
            loop.held = AlgorithmOf(algorithms, loop.id);
            if (!edited || !IsAlgorithmEdited(loop))
                ShowHeldAlgorithm(loop);
        }

        Refresh();
    }

    void ControllersTab::ForgetValues()
    {
        for (auto& loop : loops)
        {
            const bool edited = IsAlgorithmEdited(loop);
            loop.held.reset();
            if (!edited)
                ShowHeldAlgorithm(loop);
        }

        SettingsPage::ForgetValues();
    }

    void ControllersTab::Revert()
    {
        for (auto& loop : loops)
            ShowHeldAlgorithm(loop);

        SettingsPage::Revert();
    }

    ChangePlan ControllersTab::Plan() const
    {
        return PlanLoops();
    }

    QString ControllersTab::Problem() const
    {
        if (auto problem = SettingsPage::Problem(); !problem.isEmpty())
            return problem;

        return ToQt(PlanLoops().problem);
    }

    bool ControllersTab::Applies(const SettingEditor& editor) const
    {
        const auto* loop = LoopOf(editor);
        return loop != nullptr && IsLoopShown(*loop) && Shows(*loop, editor) && editor.IsAvailable();
    }

    int ControllersTab::EditCount() const
    {
        const auto algorithms = std::count_if(loops.begin(), loops.end(), [this](const Loop& loop)
            {
                return IsAlgorithmEdited(loop);
            });
        return SettingsPage::EditCount() + static_cast<int>(algorithms);
    }

    void ControllersTab::Refresh()
    {
        for (auto& loop : loops)
        {
            loop.group->setVisible(IsLoopShown(loop));
            loop.form->setRowVisible(loop.algorithm, !IsLegacy());
            for (auto* editor : loop.editors)
                loop.form->setRowVisible(editor, Shows(loop, *editor));

            const bool edited = IsAlgorithmEdited(loop);
            auto font = loop.algorithmLabel->font();
            font.setBold(edited);
            loop.algorithmLabel->setFont(font);
            loop.algorithmLabel->setText(edited ? "*Controller:" : "Controller:");
        }

        hint->setText(Hint());
        hint->setVisible(!hint->text().isEmpty());
        SettingsPage::Refresh();
    }

    std::optional<uint8_t> ControllersTab::ChosenAlgorithm(const Loop& loop) const
    {
        if (loop.algorithm->currentIndex() < 0)
            return loop.held;
        return static_cast<uint8_t>(loop.algorithm->currentData().toInt());
    }

    bool ControllersTab::IsAlgorithmEdited(const Loop& loop) const
    {
        return !IsLegacy() && IsLoopShown(loop) && loop.algorithm->currentIndex() >= 0 && ChosenAlgorithm(loop) != loop.held;
    }

    bool ControllersTab::Shows(const Loop& loop, const SettingEditor& editor) const
    {
        if (IsLegacy())
            return IsBandwidth(editor.Parameter());

        const auto chosen = ChosenAlgorithm(loop);
        return !chosen.has_value() || IsUsedBy(editor.Presentation(), *chosen);
    }

    bool ControllersTab::IsLoopShown(const Loop& loop) const
    {
        const auto shown = LoopsOf(mode.value_or(can::FocMotorMode::torque));
        return std::find(shown.begin(), shown.end(), loop.id) != shown.end();
    }

    const ControllersTab::Loop* ControllersTab::LoopOf(const SettingEditor& editor) const
    {
        const auto loop = std::find_if(loops.begin(), loops.end(), [&editor](const Loop& candidate)
            {
                return std::find(candidate.editors.begin(), candidate.editors.end(), &editor) != candidate.editors.end();
            });
        return loop != loops.end() ? &*loop : nullptr;
    }

    void ControllersTab::ShowHeldAlgorithm(Loop& loop)
    {
        loop.algorithm->setCurrentIndex(loop.held.has_value() ? loop.algorithm->findData(static_cast<int>(*loop.held)) : -1);
    }

    ChangePlan ControllersTab::PlanLoops() const
    {
        ChangePlan plan;
        for (const auto& loop : loops)
        {
            if (!IsLoopShown(loop))
                continue;

            LoopEdits edits;
            edits.loop = loop.id;
            edits.heldAlgorithm = loop.held;
            edits.algorithm = IsAlgorithmEdited(loop) ? ChosenAlgorithm(loop) : loop.held;
            for (const auto* editor : loop.editors)
                if (Applies(*editor) && editor->IsEdited() && editor->IsValid())
                    edits.settings.push_back(SettingChange::Parameter(editor->Parameter(), editor->EditedValue()));

            if (loop.id == can::FocControlLoop::current)
            {
                edits.heldSwitchingGain = EditorFor(can::FocParameter::currentSwitchingGain)->Held();
                edits.heldBoundaryLayer = EditorFor(can::FocParameter::currentBoundaryLayer)->Held();
            }

            auto loopPlan = PlanLoop(edits);
            if (!loopPlan.problem.empty())
                return loopPlan;

            plan.changes.insert(plan.changes.end(), loopPlan.changes.begin(), loopPlan.changes.end());
        }

        return plan;
    }

    QString ControllersTab::Hint() const
    {
        QStringList parts;
        if (IsLegacy())
            parts << "This controller has firmware contract 2.0: only the loop bandwidths can be set, and they are not read back. "
                     "Update its firmware to choose the controller of each loop and set the other settings.";
        if (!mode.has_value())
            parts << "The active control mode is not known yet: select one on the Motor Control tab to see the loops it runs.";
        return parts.join(' ');
    }
}
