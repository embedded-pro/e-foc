Feature: Control-Loop Supervision
  The drive watches, from its event loop, that the 20 kHz control interrupt keeps completing while
  the power stage is started. When the conversion trigger is lost, the bridge would otherwise keep
  applying its last duty; the drive stops the power stage within three milliseconds of the last
  completed interrupt and reports the stall as its own fault.

  @sil @REQ-EH-015 @REQ-CAN-013
  Scenario: A lost conversion trigger stops the power stage and reports a control-loop stall
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 2000 samples
    And the control interrupt stops 50 ms after enable
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the motor start command is issued
    Then the power stage shall stop within 3.0 ms of the control interrupt stopping
    And the state machine shall report a control-loop-stall fault

  @sil @REQ-EH-015
  Scenario: A drive whose control interrupt keeps completing raises no stall
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a torque setpoint of 0.2 A is applied
    Then the rotor shall turn
    And the state machine shall be in the running state
