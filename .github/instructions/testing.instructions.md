---
description: "e-foc test specifics: fixture and typed-test patterns, FOC test cases."
applyTo: "**/test/**,**/test_doubles/**"
---

# e-foc Test Specifics

## Fixture test (most FOC tests)

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
    ...
}
```

## Typed test (template code instantiated for more than one type)

```cpp
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

- Include `<gtest/gtest.h>`, not `<gmock/gmock.h>`, unless you need gmock matchers.
- No hardware: use the stubs in `targets/platform_implementations/host/`.
- The test name states the scenario and the expected outcome. One behaviour per test. Arrange-Act-Assert.

## FOC test cases

- Clarke/Park: check against analytical values (balanced currents give known Iα, Iβ, Id, Iq).
- SVM: duty cycles in all 6 sectors and at the sector boundaries.
- PID anti-windup: the integrator clamps and does not accumulate past the output limits.
- Electrical angle: `θe = θm · pole_pairs` for more than one pole-pair count.
- Control loop: model-in-the-loop (`integration_tests/model_in_the_loop`) for sweeps; `@sil` scenarios in
  `integration_tests/features/` for the firmware.
- Edge cases: zero and maximum current, zero and maximum speed, angle wrap at ±π.
