---
description: "e-foc testing guidelines: prefer TEST_F for fixture-based tests, TYPED_TEST for multi-type tests, FOC transform correctness tests, PID anti-windup tests, SVM sector tests, host simulation for integration, Arrange-Act-Assert pattern. StrictMock required; NiceMock forbidden."
applyTo: "**/test/**"
---

# e-foc Testing Guidelines

## File Structure

- Unit test files: `source/foc/implementations/test/Test{ComponentName}.cpp`
- Hardware stubs for unit tests: `source/hardware/Host/`
- Host simulation models for integration: `source/tool/simulator/`
- CMake: tests added via `add_subdirectory(test)` in `CMakeLists.txt`

## Framework

- GoogleTest for assertions (`TEST_F`, `TYPED_TEST`, `TEST`)
- GoogleMock for mocking hardware interfaces (`testing::StrictMock<>`)
- Heap allocation (`std::make_unique`, `std::vector`, etc.) is permitted in host-side unit tests; the no-heap rule applies only to embedded target code and ISR-reachable paths

## TDD — Red-Green-Refactor

Follow the TDD cycle for every new behavior:

1. **Red** — Write a failing test that describes one specific behavior (input, expected output, edge case). The test must fail before writing any production code.
2. **Green** — Write the minimum production code needed to make the test pass. Nothing more.
3. **Refactor** — Clean up code and test (naming, duplication, structure) while keeping all tests green.
4. Repeat for the next behavior.

Tests act as executable specifications. Write tests **before** implementation, not after.

## Fixture Test Pattern (single type — most FOC tests)

```cpp
#include "source/foc/implementations/TransformsClarkePark.hpp"
#include <gtest/gtest.h>

namespace
{
    class TestTransformsClarkePark : public ::testing::Test
    {
    protected:
        foc::TransformsClarkePark transforms;
    };
}

TEST_F(TestTransformsClarkePark, clarke_produces_correct_alpha_from_balanced_currents)
{
    // Arrange
    // Act
    // Assert
}
```

## Typed Test Pattern (multiple numeric types — for templated algorithms)

```cpp
#include "numerical/filters/passive/FirFilter.hpp"
#include <gtest/gtest.h>

namespace
{
    template<typename T>
    class TestFirFilter : public ::testing::Test {};

    using TestTypes = ::testing::Types<float, math::Q15, math::Q31>;
    TYPED_TEST_SUITE(TestFirFilter, TestTypes);
}

TYPED_TEST(TestFirFilter, produces_correct_output)
{
    // Arrange, Act, Assert
}
```

## Rules

- Prefer `TEST_F` when tests share fixture state or setup/teardown logic
- Use plain `TEST()` for simple, stateless tests without shared setup — it is acceptable and used throughout the codebase
- Fixture class inside anonymous `namespace {}`
- Test macros (`TEST_F`, `TEST`, `TYPED_TEST`) **outside** the anonymous namespace
- Include `<gtest/gtest.h>` (not `<gmock/gmock.h>`) unless gmock matchers are needed
- Use `testing::StrictMock<MockType>` for ALL mock instances — `NiceMock` and `NaggyMock` are **FORBIDDEN**

## FOC-Specific Test Requirements

- **Clarke/Park transforms**: Verify against known analytical values (e.g., balanced symmetrical currents give known Iα, Iβ, Id, Iq)
- **SVM**: Test duty cycles for all 6 sectors and sector boundaries
- **PID anti-windup**: Verify that integrator is clamped and does not accumulate past output limits
- **Electrical angle conversion**: Verify `θe = θm · pole_pairs` for multiple pole-pair counts
- **Control loop integration**: Use host simulation models in `source/tool/simulator/` for end-to-end validation
- Test edge cases: zero current, maximum current, zero speed, maximum speed, angle wraparound at ±π

## Test Quality

- Use descriptive test names that state the scenario and expected outcome
- Keep tests focused — one behavior per test
- Use `EXPECT_NEAR` with explicit tolerance for floating-point assertions (not `EXPECT_EQ`)
- Tests must not require hardware — use stubs from `source/hardware/Host/`
- Follow Arrange-Act-Assert pattern
- Allman brace style applies to test code too
