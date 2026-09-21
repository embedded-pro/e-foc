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

  The duty cycle reaches the inverter in whole percent, so every loop limit-cycles
  around its setpoint; the @sil rows carry the envelope the product holds today.
  The @sil-known-defect rows carry the envelope a law should meet and are held
  out of the default run: the deadbeat and sliding-mode current laws command no
  voltage after enabling, the decoupled law overshoots more than twofold, and the
  LQI position law is unstable. See documentation/design/software-in-the-loop.md.

  @sil @REQ-SPD-008
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

    Examples:
      | algorithm | band_pct | settle_ms | overshoot_pct | error |
      | pid       | 30 | 300 | 50 | 2.0 |
      | lqi       | 30 | 300 | 50 | 2.0 |
      | adrc      | 30 | 300 | 50 | 2.0 |
      | twodof    | 30 | 300 | 50 | 2.0 |

  @sil @REQ-SPD-008
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

    Examples:
      | algorithm | target | band_pct | settle_ms | overshoot_pct | error |
      | pid       | 40 | 30 | 300 | 50 | 2.0 |
      | pid       | -20 | 30 | 300 | 50 | 2.0 |
      | twodof    | 40 | 30 | 300 | 50 | 2.0 |
      | twodof    | -20 | 30 | 300 | 50 | 2.0 |

  @REQ-POS-009
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

    @sil
    Examples:
      | algorithm | band_pct | settle_ms | overshoot_pct | error |
      | pid       | 30 | 450 | 50 | 0.2 |
      | cascadep  | 30 | 450 | 50 | 0.2 |
      | lqr       | 30 | 450 | 50 | 0.2 |
      | twodof    | 30 | 450 | 50 | 0.2 |

    @sil-known-defect
    Examples:
      | algorithm | band_pct | settle_ms | overshoot_pct | error |
      | lqi       | 10 | 300 | 30 | 0.05 |

  @sil-known-defect @REQ-TRQ-007
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

    Examples:
      | algorithm | band_pct | settle_ms | overshoot_pct | error |
      | pid       | 30 | 10 | 50 | 0.1 |
      | decoupled | 30 | 10 | 50 | 0.1 |
      | deadbeat  | 30 | 10 | 50 | 0.1 |
      | sliding   | 30 | 10 | 50 | 0.1 |
