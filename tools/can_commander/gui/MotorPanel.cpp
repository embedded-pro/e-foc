#include "tools/can_commander/gui/MotorPanel.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include "tools/can_commander/logic/CommandDescriptions.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include <QHBoxLayout>
#include <QSplitter>
#include <QVBoxLayout>
#include <chrono>
#include <cmath>
#include <utility>

namespace tool
{
    namespace
    {
        using ui::backend::qt::ToQt;
        using ui::theme::StatusLevel;
    }

    MotorPanel::MotorPanel(CanCommandClient& client, uint16_t nodeId, Reporter reporter, QWidget* parent)
        : QWidget(parent)
        , client(client)
        , nodeId(nodeId)
        , reporter(std::move(reporter))
    {
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(CreateHeader());

        commandPanel = new CommandPanel();
        telemetryPanel = new TelemetryPanel();
        auto* splitter = new QSplitter(Qt::Horizontal);
        splitter->addWidget(commandPanel);
        splitter->addWidget(telemetryPanel);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 2);
        layout->addWidget(splitter, 1);

        ConnectCommands();
        ConnectSetpoints();
        ConnectCommissioning();
        ConnectSettings();
        Refresh();
    }

    QWidget* MotorPanel::CreateHeader()
    {
        auto* header = new QWidget();
        auto* layout = new QHBoxLayout(header);
        layout->setContentsMargins(0, 0, 0, 0);
        onlineLabel = new QLabel();
        firmwareLabel = new QLabel();
        activityLabel = new QLabel();
        layout->addWidget(onlineLabel);
        layout->addWidget(firmwareLabel);
        layout->addWidget(activityLabel);
        layout->addStretch();
        return header;
    }

    uint16_t MotorPanel::NodeId() const
    {
        return nodeId;
    }

    QString MotorPanel::Title() const
    {
        return QString("Motor %1").arg(nodeId);
    }

    QString MotorPanel::Summary() const
    {
        if (!busConnected)
            return "Not connected";
        if (!nodeOnline.has_value())
            return "No heartbeat yet";
        if (!*nodeOnline)
            return "Offline";
        if (client.Compatibility(nodeId) == can::ContractCompatibility::incompatible)
            return "Incompatible firmware";
        if (lastFault.has_value() && *lastFault != FocFaultCode::none)
            return "Fault: " + TelemetryPanel::FaultCodeName(*lastFault);
        if (lastState.has_value())
            return TelemetryPanel::MotorStateName(*lastState);
        return "Online";
    }

    StatusLevel MotorPanel::Level() const
    {
        if (!busConnected || !nodeOnline.has_value())
            return StatusLevel::Neutral;
        if (!*nodeOnline)
            return StatusLevel::Warning;
        if (client.Compatibility(nodeId) == can::ContractCompatibility::incompatible || (lastFault.has_value() && *lastFault != FocFaultCode::none) || lastState == FocMotorState::fault)
            return StatusLevel::Fault;
        if (lastState == FocMotorState::running)
            return StatusLevel::Ok;
        return StatusLevel::Neutral;
    }

    bool MotorPanel::AcceptsEmergencyStop() const
    {
        return busConnected && client.Compatibility(nodeId) != can::ContractCompatibility::incompatible;
    }

    bool MotorPanel::SendEmergencyStop()
    {
        StopApplying("emergency stop");
        pendingCommand = "Emergency Stop";
        pendingCommandType = can::focEmergencyStopId;
        return client.SendEmergencyStop(nodeId);
    }

    void MotorPanel::ConnectCommands()
    {
        connect(commandPanel, &CommandPanel::StartMotorRequested, this, [this]()
            {
                Send("Start Motor", can::focStartId, [this]()
                    {
                        return client.SendStartMotor(nodeId);
                    });
            });
        connect(commandPanel, &CommandPanel::StopMotorRequested, this, [this]()
            {
                Send("Stop Motor", can::focStopId, [this]()
                    {
                        return client.SendStopMotor(nodeId);
                    });
            });
        connect(commandPanel, &CommandPanel::ClearFaultRequested, this, [this]()
            {
                Send("Clear Fault", can::focClearFaultId, [this]()
                    {
                        return client.SendClearFault(nodeId);
                    });
            });
        connect(commandPanel, &CommandPanel::RequestDataRequested, this, [this]()
            {
                client.RequestData(nodeId);
            });
        connect(commandPanel, &CommandPanel::SetControlModeRequested, this, [this](can::FocMotorMode mode)
            {
                commandPanel->ShowRequestedControlMode(mode);
                Send("Control Mode " + CommandPanel::ControlModeName(mode), can::focSelectControlModeId, [this, mode]()
                    {
                        return client.SendSetControlMode(nodeId, mode);
                    });
            });
    }

    void MotorPanel::ConnectSetpoints()
    {
        const auto bind = [this](void (CommandPanel::*signal)(float), const char* label, const char* unit, uint8_t commandType, bool (CanCommandClient::*send)(uint16_t, float))
        {
            connect(commandPanel, signal, this, [this, label, unit, commandType, send](float value)
                {
                    Send(QString("%1 %2 %3").arg(label).arg(value).arg(unit), commandType, [this, send, value]()
                        {
                            return (client.*send)(nodeId, value);
                        });
                });
        };

        bind(&CommandPanel::SetTorqueSetpointRequested, "Torque Setpoint", "A", can::focSetTorqueSetpointId, &CanCommandClient::SendSetTorqueSetpoint);
        bind(&CommandPanel::SetSpeedSetpointRequested, "Speed Setpoint", "rad/s", can::focSetSpeedSetpointId, &CanCommandClient::SendSetSpeedSetpoint);
        bind(&CommandPanel::SetPositionSetpointRequested, "Position Setpoint", "rad", can::focSetPositionSetpointId, &CanCommandClient::SendSetPositionSetpoint);
    }

    void MotorPanel::ConnectCommissioning()
    {
        auto& tab = commandPanel->MotorParameters();
        connect(&tab, &MotorParametersTab::AlignRequested, this, [this]()
            {
                Send("Align Rotor", can::focAlignId, [this]()
                    {
                        return client.SendAlign(nodeId);
                    });
            });
        connect(&tab, &MotorParametersTab::IdentifyElectricalRequested, this, [this]()
            {
                Send("Identify Electrical", can::focIdentifyElectricalId, [this]()
                    {
                        return client.SendIdentifyElectrical(nodeId);
                    });
            });
        connect(&tab, &MotorParametersTab::IdentifyMechanicalRequested, this, [this]()
            {
                Send("Identify Mechanical", can::focIdentifyMechanicalId, [this]()
                    {
                        return client.SendIdentifyMechanical(nodeId);
                    });
            });
    }

    void MotorPanel::ConnectSettings()
    {
        for (auto* page : commandPanel->SettingsPages())
        {
            connect(page, &SettingsPage::ApplyRequested, this, [this, page]()
                {
                    Apply(*page);
                });
            connect(page, &SettingsPage::RevertRequested, this, [this]()
                {
                    client.RequestSettings(nodeId);
                });
        }
    }

    bool MotorPanel::Send(const QString& command, uint8_t commandType, const std::function<bool()>& send)
    {
        pendingCommand = command;
        pendingCommandType = commandType;
        if (send())
            return true;

        const auto reason = client.Compatibility(nodeId) == can::ContractCompatibility::incompatible ? "its firmware contract is incompatible" : "the CAN adapter refused the frame";
        Report(IssueSeverity::error, QString("%1 was not sent: %2").arg(command, reason));
        return false;
    }

    // Each change waits for the previous one to be accepted, so a refusal leaves the rest unsent and still marked
    void MotorPanel::Apply(SettingsPage& page)
    {
        if (const auto problem = page.Problem(); !problem.isEmpty())
        {
            Report(IssueSeverity::warning, QString("settings not applied: %1").arg(problem));
            return;
        }

        auto plan = page.Plan();
        if (plan.changes.empty())
            return;

        applyingPage = &page;
        page.SetApplying(true);
        changes.Start(std::move(plan.changes));
        SendCurrentChange();
    }

    void MotorPanel::SendCurrentChange()
    {
        const auto change = changes.Current();
        if (!Send(Describe(change), CommandTypeOf(change, IsLegacy()), [this, change]()
                {
                    return SendChange(change);
                }))
            StopApplying("a change could not be sent", false);
    }

    bool MotorPanel::SendChange(const SettingChange& change)
    {
        if (change.kind == SettingChange::Kind::algorithm)
            return client.SendSelectAlgorithm(nodeId, change.loop, change.algorithm);

        if (!IsLegacy())
            return client.SendSetParameter(nodeId, change.parameter, change.value);

        switch (change.parameter)
        {
            case can::FocParameter::currentBandwidth:
                return client.SendSetCurrentBandwidth(nodeId, change.value);
            case can::FocParameter::speedBandwidth:
                return client.SendSetSpeedBandwidth(nodeId, change.value);
            case can::FocParameter::positionBandwidth:
                return client.SendSetPositionBandwidth(nodeId, change.value);
            case can::FocParameter::encoderResolution:
                return client.SendSetEncoderResolution(nodeId, static_cast<uint32_t>(std::lround(change.value)));
            default:
                return false;
        }
    }

    QString MotorPanel::Describe(const SettingChange& change) const
    {
        if (change.kind == SettingChange::Kind::algorithm)
            return QString("%1 controller %2").arg(ToQt(LoopName(change.loop)), ToQt(AlgorithmName(change.loop, change.algorithm)));

        if (const auto* editor = applyingPage != nullptr ? applyingPage->EditorFor(change.parameter) : nullptr)
            return editor->Describe(change.value);
        return ToQt(PresentationOf(change.parameter).name);
    }

    bool MotorPanel::IsChangeAnswer(uint8_t commandType) const
    {
        return changes.Active() && commandType == CommandTypeOf(changes.Current(), IsLegacy());
    }

    // Contract 2.0 does not read a setting back, so what it accepted is what it holds
    void MotorPanel::OnChangeAccepted()
    {
        const auto& change = changes.Current();
        if (IsLegacy() && change.kind == SettingChange::Kind::parameter)
            applyingPage->ShowValue(change.parameter, can::FocParameterValue{ change.value, false });

        if (changes.Advance())
        {
            SendCurrentChange();
            return;
        }

        // A motor model changes what the automatic settings derive, so everything is read again
        if (applyingPage == &commandPanel->MotorParameters())
            client.RequestSettings(nodeId);

        applyingPage->SetApplying(false);
        applyingPage = nullptr;
        Report(IssueSeverity::info, "settings applied");
    }

    void MotorPanel::StopApplying(const QString& reason, bool currentSent)
    {
        if (!changes.Active())
            return;

        const auto notSent = changes.Remaining() - (currentSent ? 1 : 0);
        changes.Stop();
        applyingPage->SetApplying(false);
        applyingPage = nullptr;

        if (notSent > 0)
            Report(IssueSeverity::warning, QString("applying settings stopped (%1): %2 not sent and still marked")
                                               .arg(reason, notSent == 1 ? QString("1 change") : QString("%1 changes").arg(notSent)));
    }

    // A refused mode change leaves the mode in force unknown until the controller reports it again
    void MotorPanel::ForgetControlMode()
    {
        commandPanel->ClearActiveControlMode();
        client.RequestSettings(nodeId);
    }

    bool MotorPanel::IsLegacy() const
    {
        return !client.SupportsSettings(nodeId);
    }

    void MotorPanel::Report(IssueSeverity severity, const QString& message) const
    {
        reporter(severity, QString("%1: %2").arg(Title(), message));
    }

    void MotorPanel::ForgetNodeState()
    {
        StopApplying("the controller left the bus");
        lastState.reset();
        lastFault.reset();
        telemetryPanel->Clear();
        commandPanel->ClearActiveControlMode();
        for (auto* page : commandPanel->SettingsPages())
            page->ForgetValues();
    }

    QString MotorPanel::CommandsUnavailableReason() const
    {
        if (!busConnected)
            return "Connect to a CAN interface to send commands.";
        if (nodeOnline != true)
            return QString("Node %1 is not on the bus: no heartbeat %2. Check its power, wiring and node ID.").arg(nodeId).arg(nodeOnline.has_value() ? "for 3 s" : "seen yet");

        switch (client.Compatibility(nodeId))
        {
            case can::ContractCompatibility::incompatible:
                return QString("Firmware contract %1 is incompatible with this tool (%2.x): commands are disabled. Update the firmware or the tool.")
                    .arg(contractVersion.isEmpty() ? QString("?") : contractVersion)
                    .arg(can::focContractVersionMajor);
            case can::ContractCompatibility::unknown:
                return QString("Waiting for node %1 to report its firmware version…").arg(nodeId);
            default:
                break;
        }

        if (client.IsBusy(nodeId))
            return QString("Waiting for the response to %1…").arg(pendingCommand);
        return {};
    }

    QString MotorPanel::SettingsUnavailableReason() const
    {
        if (auto reason = CommandsUnavailableReason(); !reason.isEmpty())
            return reason;
        if (lastState == FocMotorState::running)
            return "Stop the motor to change its controllers or motor parameters.";
        if (lastState == FocMotorState::calibrating)
            return "Wait for the calibration to finish.";
        return {};
    }

    void MotorPanel::Refresh()
    {
        commandPanel->SetCommandsAvailable(CommandsUnavailableReason(), !client.IsBusy(nodeId));
        commandPanel->SetSettingsAvailable(SettingsUnavailableReason());
        RefreshHeader();
        emit StatusChanged();
    }

    void MotorPanel::RefreshHeader()
    {
        onlineLabel->setText(QString("Node %1: %2").arg(nodeId).arg(Summary()));
        ui::backend::qt::StyleStatusLabel(*onlineLabel, Level());

        switch (client.Compatibility(nodeId))
        {
            case can::ContractCompatibility::compatible:
                firmwareLabel->setText(contractVersion.isEmpty() ? QString("Firmware compatible") : QString("Firmware contract %1").arg(contractVersion));
                ui::backend::qt::StyleStatusLabel(*firmwareLabel, StatusLevel::Ok);
                break;
            case can::ContractCompatibility::incompatible:
                firmwareLabel->setText(QString("Firmware contract %1: incompatible").arg(contractVersion));
                ui::backend::qt::StyleStatusLabel(*firmwareLabel, StatusLevel::Fault);
                break;
            default:
                firmwareLabel->setText("Firmware: unknown");
                ui::backend::qt::StyleStatusLabel(*firmwareLabel, StatusLevel::Neutral);
                break;
        }

        activityLabel->setText(client.IsBusy(nodeId) ? QString("Waiting for response to %1…").arg(pendingCommand) : QString{});
        ui::backend::qt::StyleStatusLabel(*activityLabel, StatusLevel::Warning);
    }

    void MotorPanel::SetBusConnected(bool connected)
    {
        busConnected = connected;
        nodeOnline.reset();
        ForgetNodeState();
        Refresh();
    }

    void MotorPanel::OnNodeOnlineChanged(bool online)
    {
        if (online)
            Report(IssueSeverity::info, "online");
        else
        {
            Report(IssueSeverity::warning, "stopped sending heartbeats");
            ForgetNodeState();
        }

        nodeOnline = online;
        Refresh();
    }

    void MotorPanel::OnBusyChanged(bool)
    {
        Refresh();
    }

    void MotorPanel::OnCommandTimeout(uint8_t commandType)
    {
        const auto timeout = std::chrono::duration_cast<std::chrono::seconds>(CanCommandClient::TimeoutFor(commandType));
        Report(IssueSeverity::warning, QString("no response to %1 within %2 s: check that the controller is powered")
                                           .arg(pendingCommand.isEmpty() ? QString("the last command") : pendingCommand)
                                           .arg(timeout.count()));

        if (IsChangeAnswer(commandType))
            StopApplying("no answer");
    }

    void MotorPanel::OnCommandAck(uint8_t categoryId, uint8_t commandType, services::CanAckStatus status)
    {
        const bool ownCategory = categoryId == can::focMotorCategoryId;
        auto name = ownCategory && commandType == pendingCommandType && !pendingCommand.isEmpty() ? pendingCommand : ToQt(CommandDisplayName(categoryId, commandType));
        if (name.isEmpty())
            name = QString("Command 0x%1/0x%2").arg(categoryId, 2, 16, QChar('0')).arg(commandType, 2, 16, QChar('0'));

        // A category error arrives first and names the reason; its acknowledgement adds nothing to the log
        const bool accepted = status == services::CanAckStatus::success;
        if (!ownCategory || status != services::CanAckStatus::categoryError)
            Report(accepted ? IssueSeverity::info : IssueSeverity::warning, QString("%1 %2").arg(name, ToQt(AckStatusDescription(status))));
        if (!ownCategory)
            return;

        if (commandType == can::focIdentifyMechanicalId && status == services::CanAckStatus::notImplemented)
            commandPanel->MotorParameters().SetMechanicalIdentificationUnavailable(
                "This controller's firmware does not identify the mechanical parameters over CAN: enter inertia and friction above instead.");

        if (commandType == can::focSelectControlModeId && !accepted)
            ForgetControlMode();

        if (IsChangeAnswer(commandType))
        {
            if (accepted)
                OnChangeAccepted();
            else
                StopApplying(QString("%1 was refused").arg(name));
        }
    }

    void MotorPanel::OnCommandRejected(uint8_t commandType, can::FocMotorCategoryError error)
    {
        auto name = commandType == pendingCommandType && !pendingCommand.isEmpty() ? pendingCommand : ToQt(CommandDisplayName(can::focMotorCategoryId, commandType));
        if (name.isEmpty())
            name = QString("Command 0x%1").arg(commandType, 2, 16, QChar('0'));

        Report(IssueSeverity::warning, QString("%1 %2").arg(name, ToQt(RejectionDescription(error))));
        if (commandType == can::focSelectControlModeId)
            ForgetControlMode();

        if (IsChangeAnswer(commandType))
            StopApplying(QString("%1 was refused").arg(name));

        // An interrupted write may still have landed, so the tabs show what the controller holds rather than what was sent
        if (commandType == can::focSetParameterId || commandType == can::focSelectAlgorithmId)
            client.RequestSettings(nodeId);
    }

    void MotorPanel::OnContractVersion(uint8_t major, uint8_t minor, bool compatible)
    {
        contractVersion = QString("%1.%2").arg(major).arg(minor);

        if (compatible)
        {
            Report(IssueSeverity::info, QString("firmware contract %1 is compatible").arg(contractVersion));
            for (auto* page : commandPanel->SettingsPages())
                page->SetLegacy(IsLegacy());
        }
        else
            Report(IssueSeverity::error, CommandsUnavailableReason());

        Refresh();
    }

    void MotorPanel::OnControlModeAcknowledged(can::FocMotorMode activeMode)
    {
        commandPanel->SetActiveControlMode(activeMode);
        Report(IssueSeverity::info, QString("control mode is now %1").arg(CommandPanel::ControlModeName(activeMode)));
    }

    void MotorPanel::OnMotorStatus(FocMotorState state, FocFaultCode fault)
    {
        telemetryPanel->OnMotorStatus(state, fault);

        const bool changed = lastState != state || lastFault != fault;
        if (lastState != state)
            Report(IssueSeverity::info, QString("state %1").arg(TelemetryPanel::MotorStateName(state)));

        if (lastFault != fault)
        {
            if (fault != FocFaultCode::none)
                Report(IssueSeverity::error, QString("fault: %1").arg(TelemetryPanel::FaultCodeName(fault)));
            else if (lastFault.has_value())
                Report(IssueSeverity::info, "fault cleared");
        }

        lastState = state;
        lastFault = fault;
        if (changed)
            Refresh();
    }

    void MotorPanel::OnCurrentMeasurement(float idCurrent, float iqCurrent)
    {
        telemetryPanel->OnCurrentMeasurement(idCurrent, iqCurrent);
    }

    void MotorPanel::OnSpeedPosition(float speed, float position)
    {
        telemetryPanel->OnSpeedPosition(speed, position);
    }

    void MotorPanel::OnBusVoltage(float voltage)
    {
        telemetryPanel->OnBusVoltage(voltage);
    }

    void MotorPanel::OnElectricalParameters(float resistanceOhm, float inductanceMilliHenry, uint8_t polePairs)
    {
        telemetryPanel->OnElectricalParameters(resistanceOhm, inductanceMilliHenry, polePairs);
        Report(IssueSeverity::info, QString("electrical parameters R = %1 Ω, L = %2 mH, %3 pole pairs").arg(resistanceOhm).arg(inductanceMilliHenry).arg(polePairs));
    }

    void MotorPanel::OnMechanicalParameters(float frictionNmSPerRad, float inertiaKgM2)
    {
        telemetryPanel->OnMechanicalParameters(frictionNmSPerRad, inertiaKgM2);
        Report(IssueSeverity::info, QString("mechanical parameters B = %1 N·m·s/rad, J = %2 kg·m²").arg(frictionNmSPerRad).arg(inertiaKgM2));
    }

    void MotorPanel::OnAlgorithms(const can::FocAlgorithms& algorithms)
    {
        commandPanel->Controllers().SetAlgorithms(algorithms);
        commandPanel->SetActiveControlMode(algorithms.mode);
    }

    void MotorPanel::OnParameter(can::FocParameter parameter, can::FocParameterValue value)
    {
        for (auto* page : commandPanel->SettingsPages())
            page->ShowValue(parameter, value);
    }
}
