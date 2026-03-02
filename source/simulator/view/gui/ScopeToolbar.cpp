#include "source/simulator/view/gui/ScopeToolbar.hpp"
#include <QHBoxLayout>
#include <QLabel>
#include <memory>

namespace simulator
{
    namespace
    {
        template<typename T, typename... Args>
        T* QtOwned(Args&&... args)
        {
            return std::make_unique<T>(std::forward<Args>(args)...).release();
        }

        struct TimeDivOption
        {
            const char* label;
            float seconds;
        };

        constexpr std::array timeDivOptions = {
            TimeDivOption{ "10 µs/div", 10e-6f },
            TimeDivOption{ "20 µs/div", 20e-6f },
            TimeDivOption{ "50 µs/div", 50e-6f },
            TimeDivOption{ "100 µs/div", 100e-6f },
            TimeDivOption{ "200 µs/div", 200e-6f },
            TimeDivOption{ "500 µs/div", 500e-6f },
            TimeDivOption{ "1 ms/div", 1e-3f },
            TimeDivOption{ "2 ms/div", 2e-3f },
            TimeDivOption{ "5 ms/div", 5e-3f },
            TimeDivOption{ "10 ms/div", 10e-3f },
            TimeDivOption{ "20 ms/div", 20e-3f },
            TimeDivOption{ "50 ms/div", 50e-3f },
            TimeDivOption{ "100 ms/div", 100e-3f },
            TimeDivOption{ "200 ms/div", 200e-3f },
            TimeDivOption{ "500 ms/div", 500e-3f },
            TimeDivOption{ "1 s/div", 1.0f },
        };

        constexpr int defaultTimeDivIndex = 9; // 10 ms/div
        constexpr float triggerSpinMin = -100.0f;
        constexpr float triggerSpinMax = 100.0f;
        constexpr float triggerSpinStep = 0.01f;
        constexpr int triggerSpinDecimals = 3;
    }

    ScopeToolbar::ScopeToolbar(ScopeWidget& scope, QWidget* parent)
        : QWidget(parent)
        , scope(scope)
    {
        SetupUi();
        ConnectSignals();
    }

    void ScopeToolbar::SetupUi()
    {
        auto* layout = QtOwned<QHBoxLayout>(this);
        layout->setContentsMargins(0, 0, 0, 0);

        // Time/div
        layout->addWidget(QtOwned<QLabel>("Time/div:", this));
        timeDivCombo = QtOwned<QComboBox>(this);
        PopulateTimeDivOptions();
        timeDivCombo->setCurrentIndex(defaultTimeDivIndex);
        layout->addWidget(timeDivCombo);

        layout->addSpacing(10);

        // Trigger mode
        layout->addWidget(QtOwned<QLabel>("Trigger:", this));
        triggerModeCombo = QtOwned<QComboBox>(this);
        triggerModeCombo->addItem("Auto", static_cast<int>(ScopeWidget::TriggerMode::Auto));
        triggerModeCombo->addItem("Normal", static_cast<int>(ScopeWidget::TriggerMode::Normal));
        triggerModeCombo->addItem("Single", static_cast<int>(ScopeWidget::TriggerMode::Single));
        layout->addWidget(triggerModeCombo);

        // Trigger edge
        triggerEdgeCombo = QtOwned<QComboBox>(this);
        triggerEdgeCombo->addItem("↑ Rising", static_cast<int>(ScopeWidget::TriggerEdge::Rising));
        triggerEdgeCombo->addItem("↓ Falling", static_cast<int>(ScopeWidget::TriggerEdge::Falling));
        layout->addWidget(triggerEdgeCombo);

        // Trigger level
        layout->addWidget(QtOwned<QLabel>("Level:", this));
        triggerLevelSpin = QtOwned<QDoubleSpinBox>(this);
        triggerLevelSpin->setRange(triggerSpinMin, triggerSpinMax);
        triggerLevelSpin->setSingleStep(triggerSpinStep);
        triggerLevelSpin->setDecimals(triggerSpinDecimals);
        triggerLevelSpin->setValue(0.0);
        triggerLevelSpin->setSuffix(" A");
        layout->addWidget(triggerLevelSpin);

        // Trigger channel
        layout->addWidget(QtOwned<QLabel>("Ch:", this));
        triggerChannelCombo = QtOwned<QComboBox>(this);
        layout->addWidget(triggerChannelCombo);

        layout->addSpacing(10);

        // Run/Stop
        runStopButton = QtOwned<QPushButton>("Stop", this);
        runStopButton->setStyleSheet("QPushButton { background-color: #c0392b; color: white; font-weight: bold; padding: 4px 12px; }");
        layout->addWidget(runStopButton);

        // Single shot
        singleButton = QtOwned<QPushButton>("Single", this);
        singleButton->setStyleSheet("QPushButton { background-color: #2980b9; color: white; padding: 4px 8px; }");
        layout->addWidget(singleButton);

        // Force trigger
        forceTriggerButton = QtOwned<QPushButton>("Force", this);
        forceTriggerButton->setStyleSheet("QPushButton { background-color: #7f8c8d; color: white; padding: 4px 8px; }");
        layout->addWidget(forceTriggerButton);

        layout->addStretch();
    }

    void ScopeToolbar::ConnectSignals()
    {
        connect(timeDivCombo, &QComboBox::currentIndexChanged, this, [this](int index)
            {
                if (index >= 0 && index < static_cast<int>(timeDivOptions.size()))
                {
                    auto seconds = timeDivOptions[static_cast<std::size_t>(index)].seconds;
                    scope.SetTimePerDivision(seconds);
                    emit timeDivChanged(seconds);
                }
            });

        connect(triggerLevelSpin, &QDoubleSpinBox::valueChanged, this, [this](double value)
            {
                scope.SetTriggerLevel(static_cast<float>(value));
                emit triggerLevelChanged(static_cast<float>(value));
            });

        connect(triggerModeCombo, &QComboBox::currentIndexChanged, this, [this](int index)
            {
                auto mode = static_cast<ScopeWidget::TriggerMode>(triggerModeCombo->itemData(index).toInt());
                scope.SetTriggerMode(mode);
                emit triggerModeChanged(mode);
            });

        connect(triggerEdgeCombo, &QComboBox::currentIndexChanged, this, [this](int index)
            {
                auto edge = static_cast<ScopeWidget::TriggerEdge>(triggerEdgeCombo->itemData(index).toInt());
                scope.SetTriggerEdge(edge);
                emit triggerEdgeChanged(edge);
            });

        connect(triggerChannelCombo, &QComboBox::currentIndexChanged, this, [this](int index)
            {
                if (index >= 0)
                {
                    scope.SetTriggerChannel(static_cast<std::size_t>(index));
                    emit triggerChannelChanged(static_cast<std::size_t>(index));
                }
            });

        connect(runStopButton, &QPushButton::clicked, this, [this]()
            {
                bool nowRunning = !scope.IsRunning();
                scope.SetRunning(nowRunning);

                if (nowRunning)
                {
                    runStopButton->setText("Stop");
                    runStopButton->setStyleSheet("QPushButton { background-color: #c0392b; color: white; font-weight: bold; padding: 4px 12px; }");
                }
                else
                {
                    runStopButton->setText("Run");
                    runStopButton->setStyleSheet("QPushButton { background-color: #27ae60; color: white; font-weight: bold; padding: 4px 12px; }");
                }

                emit runningChanged(nowRunning);
            });

        connect(singleButton, &QPushButton::clicked, this, [this]()
            {
                scope.SetTriggerMode(ScopeWidget::TriggerMode::Single);
                scope.SetRunning(true);
                triggerModeCombo->setCurrentIndex(2); // Single
                runStopButton->setText("Stop");
                runStopButton->setStyleSheet("QPushButton { background-color: #c0392b; color: white; font-weight: bold; padding: 4px 12px; }");
            });

        connect(forceTriggerButton, &QPushButton::clicked, this, [this]()
            {
                scope.ForceTrigger();
            });
    }

    void ScopeToolbar::PopulateTimeDivOptions()
    {
        for (const auto& option : timeDivOptions)
            timeDivCombo->addItem(option.label);
    }
}
