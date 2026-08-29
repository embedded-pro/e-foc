#include "targets/platform_implementations/qemu/implementation/ArrayEeprom.hpp"

namespace application
{
    uint32_t ArrayEeprom::Size() const
    {
        return storageSize;
    }

    void ArrayEeprom::WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        for (std::size_t i = 0; i < buffer.size() && (address + i) < storageSize; ++i)
            storage[address + i] = buffer[i];
        if (onDone)
            onDone();
    }

    void ArrayEeprom::ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        for (std::size_t i = 0; i < buffer.size() && (address + i) < storageSize; ++i)
            buffer[i] = storage[address + i];
        if (onDone)
            onDone();
    }

    void ArrayEeprom::Erase(infra::Function<void()> onDone)
    {
        storage.fill(0xFF);
        if (onDone)
            onDone();
    }
}
