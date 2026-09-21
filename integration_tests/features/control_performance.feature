Feature: FOC Control Performance
  Reaching the running state says nothing about how well a loop tracks its
  setpoint. The plant reports its own trajectory on the control-tick time base,
  and the harness measures the step response of each loop: rise time, settling
  time, overshoot and steady-state error. The setpoint is applied before the
  motor is enabled, so enabling is the step and the onset is exact.

  The limits below are pinned from a characterisation run against the nominal
  plant with a margin, and each family of laws carries its own row because they
  are designed to differ: two-DOF trades speed for overshoot, deadbeat settles in
  a sample, sliding mode keeps a boundary-layer band.

  @sil @REQ-SPD-008
  Scenario Outline: The <algorithm> speed loop steps from rest to 20 rad/s
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 800 samples
    And the motor is already calibrated
    And the motor boots in speed mode
    And the speed loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    And a speed setpoint of 20 rad/s is applied
    And the motor is enabled
    And the response is captured for 550 ms after enable
    Then the speed step response shall settle within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the steady-state speed error shall be below <error> rad/s
    And the response shall have no dropped samples
    When the motor is disabled
    Then the state machine shall be in the Ready state

    Examples:
      | algorithm | settle_ms | overshoot_pct | error |
      | pid       | 300       | 50            | 2.0   |
      | lqi       | 300       | 50            | 2.0   |
      | adrc      | 300       | 50            | 2.0   |
      | twodof    | 300       | 50            | 2.0   |

  @sil @REQ-SPD-008
  Scenario Outline: The <algorithm> speed loop follows a setpoint change to <target> rad/s while running
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 2000 samples
    And the motor is already calibrated
    And the motor boots in speed mode
    And the speed loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    And a speed setpoint of 20 rad/s is applied
    And the motor is enabled
    And the response is captured for 400 ms after enable
    And a speed setpoint of <target> rad/s is applied
    And the response is captured for 550 ms after the last setpoint
    Then the speed step response shall settle within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the steady-state speed error shall be below <error> rad/s
    And the response shall have no dropped samples

    Examples:
      | algorithm | target | settle_ms | overshoot_pct | error |
      | pid       | 40     | 300       | 50            | 2.0   |
      | pid       | -20    | 300       | 50            | 2.0   |
      | twodof    | 40     | 300       | 50            | 2.0   |
      | twodof    | -20    | 300       | 50            | 2.0   |

  @sil @REQ-POS-009
  Scenario Outline: The <algorithm> position loop steps from rest to 1.5 rad
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 800 samples
    And the motor is already calibrated
    And the motor boots in position mode
    And the position loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    And a position setpoint of 1.5 rad is applied
    And the motor is enabled
    And the response is captured for 550 ms after enable
    Then the position step response shall settle within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the steady-state position error shall be below <error> rad
    And the response shall have no dropped samples

    Examples:
      | algorithm | settle_ms | overshoot_pct | error |
      | pid       | 450       | 50            | 0.2   |
      | cascadep  | 450       | 50            | 0.2   |
      | lqr       | 450       | 50            | 0.2   |
      | lqi       | 450       | 50            | 0.2   |
      | twodof    | 450       | 50            | 0.2   |

  @sil @REQ-TRQ-007
  Scenario Outline: The <algorithm> current loop steps from rest to 0.5 A
    Given a nominal motor plant
    And the plant response is recorded at 20000 Hz for up to 400 samples
    And the motor is already calibrated
    And the motor boots in torque mode
    And the current loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    And a torque setpoint of 0.5 A is applied
    And the motor is enabled
    And the response is captured for 15 ms after enable
    Then the current step response shall settle within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the steady-state current error shall be below <error> A
    And the response shall have no dropped samples

    Examples:
      | algorithm | settle_ms | overshoot_pct | error |
      | pid       | 10        | 50            | 0.1   |
      | decoupled | 10        | 50            | 0.1   |
      | deadbeat  | 10        | 50            | 0.1   |
      | sliding   | 10        | 50            | 0.1   |
