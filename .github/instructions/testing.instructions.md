---
description: "e-foc testing guidelines: prefer TEST_F for fixture-based tests, TYPED_TEST for multi-type tests, FOC transform correctness tests, PID anti-windup tests, SVM sector tests, host simulation for integration, Arrange-Act-Assert pattern. StrictMock required; NiceMock forbidden."
applyTo: "**/test/**,**/test_doubles/**"
---

# e-foc Testing Guidelines

Follow AGENTS.md — see §Testing (TDD, GoogleTest macros, `StrictMock` only, `EXPECT_NEAR`, file names, host stubs, heap in tests) and §Async callback lifetime safety rule 6 (fixtures holding `WithSharedAccess` members). This file adds only the test specifics.

## Fixture Test Pattern (single type — most FOC tests)

```cpp
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include <gtest/gtest.h>

namespace
{
    class TestClarkePark : public ::testing::Test
    {
    protected:
        foc::ClarkePark clarkePark;
    };
}

TEST_F(TestClarkePark, forward_of_balanced_currents_at_zero_angle_gives_expected_d_and_q)
{
    // Arrange
    // Act
    // Assert
}
```

## Typed Test Pattern (templated code instantiated for more than one type)

```cpp
#include "core/foc/<area>/<Component>.hpp"
#include <gtest/gtest.h>

namespace
{
    template<typename T>
    class TestComponent : public ::testing::Test {};

    using TestTypes = ::testing::Types<float, double>;
    TYPED_TEST_SUITE(TestComponent, TestTypes);
}

TYPED_TEST(TestComponent, produces_correct_output)
{
    // Arrange, Act, Assert
}
```

## Rules

- Include `<gtest/gtest.h>` (not `<gmock/gmock.h>`) unless gmock matchers are needed
- Tests must not require hardware — use the stubs in `targets/platform_implementations/host/`
- Descriptive test names that state the scenario and expected outcome; one behavior per test
- Arrange-Act-Assert

## FOC-Specific Test Requirements

- **Clarke/Park transforms**: Verify against known analytical values (e.g., balanced symmetrical currents give known Iα, Iβ, Id, Iq)
- **SVM**: Test duty cycles for all 6 sectors and sector boundaries
- **PID anti-windup**: Verify that integrator is clamped and does not accumulate past output limits
- **Electrical angle conversion**: Verify `θe = θm · pole_pairs` for multiple pole-pair counts
- **Control loop integration**: model-in-the-loop (`integration_tests/model_in_the_loop`) for sweeps, `@sil` scenarios in `integration_tests/features/` for the firmware — see AGENTS.md §Build
- Test edge cases: zero current, maximum current, zero speed, maximum speed, angle wraparound at ±π
