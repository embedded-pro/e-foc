#include "tools/virtual_motor/platform/FileEeprom.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>
#include <fstream>

namespace virtual_motor
{
    namespace
    {
        constexpr uint8_t erased{ 0xff };
    }

    FileEeprom::FileEeprom(std::string path)
        : path(std::move(path))
    {
        Load();
    }

    uint32_t FileEeprom::Size() const
    {
        return size;
    }

    void FileEeprom::WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        really_assert(address <= size && buffer.size() <= size - address);
        std::ranges::copy(buffer, memory.begin() + address);
        Store();
        Complete(onDone);
    }

    void FileEeprom::ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        really_assert(address <= size && buffer.size() <= size - address);
        std::copy_n(memory.begin() + address, buffer.size(), buffer.begin());
        Complete(onDone);
    }

    void FileEeprom::Erase(infra::Function<void()> onDone)
    {
        memory.fill(erased);
        Store();
        Complete(onDone);
    }

    void FileEeprom::Load()
    {
        memory.fill(erased);
        std::ifstream file{ path, std::ios::binary };
        std::array<uint8_t, size> stored{};
        if (file.read(reinterpret_cast<char*>(stored.data()), stored.size()) && file.gcount() == static_cast<std::streamsize>(stored.size()))
            memory = stored;
    }

    void FileEeprom::Store() const
    {
        std::ofstream file{ path, std::ios::binary | std::ios::trunc };
        file.write(reinterpret_cast<const char*>(memory.data()), memory.size());
    }

    void FileEeprom::Complete(const infra::Function<void()>& onDone)
    {
        infra::EventDispatcher::Instance().Schedule(onDone);
    }
}
