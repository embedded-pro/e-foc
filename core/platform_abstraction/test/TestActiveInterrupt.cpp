#include "core/platform_abstraction/ActiveInterrupt.hpp"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>

namespace
{
    constexpr uint32_t firstExternalException{ 16 };

    std::array<uint32_t, 8> PendingOnly(uint32_t irq)
    {
        std::array<uint32_t, 8> words{};
        words[irq / 32] = 1u << (irq % 32);
        return words;
    }
}

TEST(TestActiveInterrupt, the_active_interrupt_pending_again_is_reported)
{
    const auto pending = PendingOnly(14);

    EXPECT_TRUE(application::ActiveInterruptPendsAgain(firstExternalException + 14, pending.data()));
}

TEST(TestActiveInterrupt, another_interrupt_pending_is_not_reported)
{
    const auto pending = PendingOnly(15);

    EXPECT_FALSE(application::ActiveInterruptPendsAgain(firstExternalException + 14, pending.data()));
}

TEST(TestActiveInterrupt, an_interrupt_number_above_31_reads_its_own_pending_word)
{
    const auto pending = PendingOnly(45);

    EXPECT_TRUE(application::ActiveInterruptPendsAgain(firstExternalException + 45, pending.data()));
    EXPECT_FALSE(application::ActiveInterruptPendsAgain(firstExternalException + 13, pending.data()));
}

TEST(TestActiveInterrupt, the_other_icsr_fields_do_not_change_the_answer)
{
    const auto pending = PendingOnly(14);
    constexpr uint32_t pendingVectorAndFlags{ 0x0040'f000u };

    EXPECT_TRUE(application::ActiveInterruptPendsAgain(pendingVectorAndFlags | (firstExternalException + 14), pending.data()));
}

TEST(TestActiveInterrupt, a_system_exception_is_never_reported)
{
    const auto pending = PendingOnly(0);

    EXPECT_FALSE(application::ActiveInterruptPendsAgain(15, pending.data()));
    EXPECT_FALSE(application::ActiveInterruptPendsAgain(0, pending.data()));
}
