#pragma once

#include <memory>
#include <utility>

namespace simulator
{
    template<typename T, typename... Args>
    T* QtOwned(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...).release();
    }
}
