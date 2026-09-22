Feature: FOC Control Performance
  Reaching the running state says nothing about how well a loop tracks its
  setpoint. The plant reports its own trajectory on the control-tick time base,
  and the harness measures the step response of each loop: rise time, settling
  time, overshoot and steady-state error. The setpoint is applied before the
  motor is enabled, so enabling is the step and the onset is exact.

  The limits below are pinned from a characterisation run against the nominal
  plant (the Teknic M-2310P-LN-04K reference motor) with a margin, and each
  family of laws carries its own row because they are designed to differ:
  two-DOF trades speed for overshoot, deadbeat settles in a sample, sliding mode
  keeps a boundary-layer band.

  The duty cycle reaches the inverter in whole percent, so the current loop
  ripples around its setpoint by about a tenth of an ampere; the @sil current
  rows carry the envelope the product holds today and the @sil-known-defect rows
  the envelope the laws should meet. See documentation/design/software-in-the-loop.md.

  @REQ-SPD-008
  Scenario Outline: The <algorithm> speed loop steps from rest to 20 rad/s
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 800 samples
    And the motor is already calibrated
    And the motor boots in speed mode
    And the speed loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    Then the speed loop shall be running the <algorithm> algorithm
    When a speed setpoint of 20 rad/s is applied
    And the motor is enabled
    And the response is captured for 550 ms after enable
    Then the speed step response shall settle into a <band_pct> % band within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the steady-state speed error shall be below <error> rad/s
    And the response shall have no dropped samples
    When the motor is disabled
    Then the state machine shall be in the Ready state

    @sil
    Examples:
      | algorithm | band_pct | settle_ms | overshoot_pct | error |
      | pid       | 10       | 60        | 15            | 0.5   |
      | adrc      | 10       | 80        | 15            | 0.5   |
      | twodof    | 10       | 60        | 15            | 0.5   |
      | lqi       | 10       | 60        | 15            | 0.5   |

  @REQ-SPD-008
  Scenario Outline: The <algorithm> speed loop follows a setpoint change to <target> rad/s while running
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 2000 samples
    And the motor is already calibrated
    And the motor boots in speed mode
    And the speed loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    Then the speed loop shall be running the <algorithm> algorithm
    When a speed setpoint of 20 rad/s is applied
    And the motor is enabled
    And the response is captured for 400 ms after enable
    And a speed setpoint of <target> rad/s is applied
    And the response is captured for 550 ms after the last setpoint
    Then the speed step response shall settle into a <band_pct> % band within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the steady-state speed error shall be below <error> rad/s
    And the response shall have no dropped samples

    @sil
    Examples:
      | algorithm | target | band_pct | settle_ms | overshoot_pct | error |
      | pid       | 40     | 10       | 60        | 15            | 0.5   |
      | pid       | -20    | 10       | 60        | 15            | 0.5   |
      | adrc      | 40     | 10       | 80        | 15            | 0.5   |
      | adrc      | -20    | 10       | 80        | 15            | 0.5   |
      | twodof    | 40     | 10       | 60        | 15            | 0.5   |
      | twodof    | -20    | 10       | 60        | 15            | 0.5   |
      | lqi       | 40     | 10       | 60        | 15            | 0.5   |
      | lqi       | -20    | 10       | 60        | 15            | 0.5   |

  @sil @REQ-POS-009
  Scenario Outline: The <algorithm> position loop steps from rest to 1.5 rad
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 800 samples
    And the motor is already calibrated
    And the motor boots in position mode
    And the position loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    Then the position loop shall be running the <algorithm> algorithm
    When a position setpoint of 1.5 rad is applied
    And the motor is enabled
    And the response is captured for 550 ms after enable
    Then the position step response shall settle into a <band_pct> % band within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the steady-state position error shall be below <error> rad
    And the response shall have no dropped samples

    Examples:
      | algorithm | band_pct | settle_ms | overshoot_pct | error |
      | pid       | 10       | 200       | 15            | 0.1   |
      | cascadep  | 10       | 200       | 10            | 0.02  |
      | lqr       | 10       | 60        | 15            | 0.01  |
      | lqi       | 10       | 60        | 50            | 0.01  |
      | twodof    | 10       | 300       | 15            | 0.1   |

  @sil @REQ-POS-009
  Scenario Outline: The <algorithm> position loop follows a setpoint change to <target> rad while holding
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 2000 samples
    And the motor is already calibrated
    And the motor boots in position mode
    And the position loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    Then the position loop shall be running the <algorithm> algorithm
    When a position setpoint of 1.5 rad is applied
    And the motor is enabled
    And the response is captured for 400 ms after enable
    And a position setpoint of <target> rad is applied
    And the response is captured for 550 ms after the last setpoint
    Then the position step response shall settle into a <band_pct> % band within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the steady-state position error shall be below <error> rad
    And the response shall have no dropped samples

    Examples:
      | algorithm | target | band_pct | settle_ms | overshoot_pct | error |
      | pid       | -1.5   | 10       | 200       | 15            | 0.15  |
      | cascadep  | -1.5   | 10       | 200       | 10            | 0.02  |
      | lqr       | -1.5   | 10       | 60        | 15            | 0.01  |
      | lqi       | -1.5   | 10       | 60        | 40            | 0.01  |
      | twodof    | -1.5   | 10       | 300       | 15            | 0.15  |

  @REQ-TRQ-007
  Scenario Outline: The <algorithm> current loop steps from rest to 0.5 A
    Given a nominal motor plant
    And the plant response is recorded at 20000 Hz for up to 400 samples
    And the motor is already calibrated
    And the motor boots in torque mode
    And the current loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    Then the current loop shall be running the <algorithm> algorithm
    When a torque setpoint of 0.5 A is applied
    And the motor is enabled
    And the response is captured for 15 ms after enable
    Then the current step response shall settle into a <band_pct> % band within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the steady-state current error shall be below <error> A
    And the response shall have no dropped samples

    @sil
    Examples:
      | algorithm | band_pct | settle_ms | overshoot_pct | error |
      | pid       | 40       | 12        | 20            | 0.1   |
      | decoupled | 40       | 12        | 40            | 0.1   |
      | deadbeat  | 40       | 5         | 20            | 0.1   |
      | sliding   | 40       | 5         | 70            | 0.1   |

    @sil-known-defect
    Examples:
      | algorithm | band_pct | settle_ms | overshoot_pct | error |
      | pid       | 10       | 3         | 30            | 0.05  |
      | decoupled | 10       | 3         | 30            | 0.05  |
      | deadbeat  | 10       | 1         | 30            | 0.05  |
      | sliding   | 10       | 3         | 30            | 0.05  |
