// Platform definition of the hook the Cortex-M fault handler calls before it writes any
// diagnostics. On ST this is deliberately empty: the target does not drive a bridge yet - both
// PinsAndPeripherals.hpp are empty and Encoder::Resolution() returns 0, so there is no timer
// instance to name here. Cutting the power stage means clearing TIM_BDTR_MOE on whichever
// advanced-control timer the board wires to the gate drivers, and that belongs with the work
// that makes the target functional.
extern "C" void CutPowerStage(void)
{}
