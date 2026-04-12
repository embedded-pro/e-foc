#include "integration_tests/software_in_the_loop/support/EepromStub.hpp"
#include <algorithm>

namespace integration
{
    EepromStub::EepromStub()
    {
        storage.fill(0xFF);
    }

    uint32_t EepromStub::Size() const
    {
        return stubSize;
    }

    void EepromStub::WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        really_assert(address + buffer.size() <= stubSize);
        std::copy(buffer.begin(), buffer.end(), storage.begin() + static_cast<std::ptrdiff_t>(address));
        onDone();
    }

    void EepromStub::ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        really_assert(address + buffer.size() <= stubSize);
        std::copy(storage.begin() + static_cast<std::ptrdiff_t>(address),
            storage.begin() + static_cast<std::ptrdiff_t>(address + buffer.size()),
            buffer.begin());
        onDone();
    }

    void EepromStub::Erase(infra::Function<void()> onDone)
    {
        storage.fill(0xFF);
        onDone();
    }
}
