#include "tools/can_commander/gui/SettingEditor.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include <cmath>

namespace tool
{
    namespace
    {
        using ui::backend::qt::ToQt;

        QString BitrateText(uint32_t bitrate)
        {
            return bitrate >= 1000000 ? QString("%1 Mbit/s").arg(bitrate / 1000000) : QString("%1 kbit/s").arg(bitrate / 1000);
        }
    }

    SettingEditor::SettingEditor(const SettingPresentation& presentation, QWidget* parent)
        : QWidget(parent)
        , presentation(presentation)
        , descriptor(*can::FindParameter(presentation.parameter))
    {
        numberLocale.setNumberOptions(QLocale::OmitGroupSeparator);

        label = new QLabel(ToQt(presentation.label));
        label->setToolTip(ToQt(presentation.description));

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        if (presentation.parameter == can::FocParameter::canBitrate)
            BuildChoice(*layout);
        else if (IsToggle(presentation.parameter))
            BuildToggle(*layout);
        else
            BuildNumber(*layout);

        ShowHeld();
    }

    void SettingEditor::BuildNumber(QHBoxLayout& layout)
    {
        number = new QLineEdit();
        number->setPlaceholderText("—");
        number->setAccessibleName(ToQt(presentation.name));
        number->setToolTip(ToQt(presentation.description));
        layout.addWidget(number, 1);
        label->setBuddy(number);

        if (!presentation.suffix.empty())
            layout.addWidget(new QLabel(ToQt(presentation.suffix).trimmed()));

        connect(number, &QLineEdit::textEdited, this, [this]()
            {
                MarkEdited();
                emit Edited();
            });

        if (presentation.automaticLabel.empty())
            return;

        automatic = new QCheckBox(ToQt(presentation.automaticLabel));
        automatic->setAccessibleName(QString("%1: %2").arg(ToQt(presentation.name), ToQt(presentation.automaticLabel)));
        automatic->setToolTip(ToQt(presentation.description));
        layout.addWidget(automatic);

        connect(automatic, &QCheckBox::clicked, this, [this]()
            {
                UpdateEnabled();
                MarkEdited();
                emit Edited();
            });
    }

    void SettingEditor::BuildToggle(QHBoxLayout& layout)
    {
        toggle = new QCheckBox();
        toggle->setAccessibleName(ToQt(presentation.name));
        toggle->setToolTip(ToQt(presentation.description));
        layout.addWidget(toggle);
        layout.addStretch();
        label->setBuddy(toggle);

        connect(toggle, &QCheckBox::clicked, this, [this]()
            {
                MarkEdited();
                emit Edited();
            });
    }

    void SettingEditor::BuildChoice(QHBoxLayout& layout)
    {
        choice = new QComboBox();
        choice->setPlaceholderText("—");
        choice->setAccessibleName(ToQt(presentation.name));
        choice->setToolTip(ToQt(presentation.description));
        for (const auto bitrate : can::focCanBitrates)
            choice->addItem(BitrateText(bitrate), static_cast<qlonglong>(bitrate));
        layout.addWidget(choice, 1);
        label->setBuddy(choice);

        connect(choice, &QComboBox::activated, this, [this]()
            {
                MarkEdited();
                emit Edited();
            });
    }

    const SettingPresentation& SettingEditor::Presentation() const
    {
        return presentation;
    }

    can::FocParameter SettingEditor::Parameter() const
    {
        return presentation.parameter;
    }

    QLabel* SettingEditor::Label() const
    {
        return label;
    }

    void SettingEditor::SetHeld(std::optional<can::FocParameterValue> value)
    {
        const bool edited = IsEdited();
        held = value;
        heldText = held.has_value() ? Format(held->value) : QString{};

        if (!edited || !IsEdited())
            ShowHeld();
        else
            MarkEdited();
    }

    void SettingEditor::SetLegacy(bool legacyContract)
    {
        legacy = legacyContract;
        if (automatic != nullptr)
            automatic->setVisible(!legacy);

        UpdateEnabled();
        MarkEdited();
    }

    void SettingEditor::SetUnavailable(const QString& reason)
    {
        unavailableReason = reason;
        UpdateEnabled();
    }

    void SettingEditor::Revert()
    {
        ShowHeld();
    }

    std::optional<float> SettingEditor::Held() const
    {
        if (!held.has_value())
            return std::nullopt;
        return held->value;
    }

    bool SettingEditor::IsAvailable() const
    {
        return unavailableReason.isEmpty();
    }

    bool SettingEditor::IsEdited() const
    {
        if (toggle != nullptr)
            return held.has_value() && toggle->isChecked() != (held->value >= 0.5f);

        if (choice != nullptr)
            return choice->currentIndex() >= 0 && (!held.has_value() || choice->currentData().toLongLong() != std::llround(held->value));

        if (automatic != nullptr && !legacy && automatic->isChecked() != (held.has_value() && held->automatic))
            return true;

        if (IsAutomaticChosen() || number->text() == heldText)
            return false;

        const auto entry = Parse();
        return !held.has_value() || !entry.has_value() || *entry != held->value;
    }

    bool SettingEditor::IsValid() const
    {
        if (!IsEdited() || IsAutomaticChosen() || number == nullptr)
            return true;

        const auto entry = Parse();
        return entry.has_value() && IsAcceptableEntry(presentation.parameter, *entry, legacy);
    }

    float SettingEditor::EditedValue() const
    {
        if (toggle != nullptr)
            return toggle->isChecked() ? 1.0f : 0.0f;

        if (choice != nullptr)
            return static_cast<float>(choice->currentData().toLongLong());

        if (IsAutomaticChosen())
            return 0.0f;

        return Parse().value_or(0.0f);
    }

    QString SettingEditor::Problem() const
    {
        if (IsValid())
            return {};

        const bool whole = descriptor.integer || (legacy && IsBandwidth(presentation.parameter));
        return QString("%1: enter %2 from %3 to %4%5")
            .arg(ToQt(presentation.name))
            .arg(whole ? "a whole number" : "a value")
            .arg(Format(descriptor.minimum))
            .arg(Format(descriptor.maximum))
            .arg(ToQt(presentation.suffix));
    }

    QString SettingEditor::Describe(float value) const
    {
        const auto name = ToQt(presentation.name);
        if (descriptor.automatic && value == 0.0f)
            return QString("%1 %2").arg(name, ToQt(presentation.automaticLabel));
        if (toggle != nullptr)
            return QString("%1 %2").arg(name, value >= 0.5f ? "on" : "off");
        if (choice != nullptr)
            return QString("%1 %2").arg(name, BitrateText(static_cast<uint32_t>(value)));
        return QString("%1 %2%3").arg(name, Format(value), ToQt(presentation.suffix));
    }

    void SettingEditor::ShowHeld()
    {
        if (number != nullptr)
            number->setText(heldText);
        if (automatic != nullptr)
            automatic->setChecked(held.has_value() && held->automatic && !legacy);
        if (toggle != nullptr)
            toggle->setChecked(held.has_value() && held->value >= 0.5f);
        if (choice != nullptr)
            choice->setCurrentIndex(held.has_value() ? choice->findData(static_cast<qlonglong>(std::llround(held->value))) : -1);

        UpdateEnabled();
        MarkEdited();
    }

    void SettingEditor::UpdateEnabled()
    {
        const bool available = unavailableReason.isEmpty();
        label->setEnabled(available);
        setToolTip(unavailableReason);

        if (number != nullptr)
            number->setEnabled(available && !IsAutomaticChosen());
        if (automatic != nullptr)
            automatic->setEnabled(available);
        if (toggle != nullptr)
            toggle->setEnabled(available && held.has_value());
        if (choice != nullptr)
            choice->setEnabled(available);
    }

    void SettingEditor::MarkEdited()
    {
        const bool edited = IsEdited();
        auto font = label->font();
        font.setBold(edited);
        label->setFont(font);
        label->setText(edited ? "*" + ToQt(presentation.label) : ToQt(presentation.label));
    }

    QString SettingEditor::Format(float value) const
    {
        if (descriptor.integer)
            return numberLocale.toString(static_cast<qlonglong>(std::llround(value)));
        return numberLocale.toString(static_cast<double>(value), 'g', 6);
    }

    std::optional<float> SettingEditor::Parse() const
    {
        const auto text = number->text().trimmed();
        bool ok{ false };
        auto value = numberLocale.toDouble(text, &ok);
        if (!ok)
            value = QLocale::c().toDouble(text, &ok);

        if (!ok || !std::isfinite(value))
            return std::nullopt;
        return static_cast<float>(value);
    }

    bool SettingEditor::IsAutomaticChosen() const
    {
        return automatic != nullptr && !legacy && automatic->isChecked();
    }
}
