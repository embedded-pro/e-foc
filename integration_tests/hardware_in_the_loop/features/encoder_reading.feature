Feature: Quadrature Encoder Reading
  The hardware target reads a quadrature encoder and reports angular position.

  # REQ-HIL-003
  Scenario: Encoder reports position change after known mechanical displacement
    Given the hardware target is connected and responding
    When the encoder position is read after a known mechanical displacement
    Then the reported position change shall match the expected increment
