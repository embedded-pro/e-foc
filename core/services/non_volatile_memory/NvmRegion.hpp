#pragma once

#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <cstddef>

namespace services
{
    class NvmRegion
    {
    public:
        virtual ~NvmRegion() = default;

        virtual void Write(infra::ConstByteRange data, infra::Function<void()> onDone) = 0;
        virtual void Read(infra::ByteRange data, infra::Function<void()> onDone) = 0;
        virtual void Erase(infra::Function<void()> onDone) = 0;
        virtual std::size_t Size() const = 0;
    };
}
