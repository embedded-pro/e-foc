#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousQuadratureEncoder.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <cstdint>
#include <numbers>

namespace application
{
    class QuadratureEncoderDecorator
    {
    protected:
        ~QuadratureEncoderDecorator() = default;

    public:
        virtual foc::Radians Read() = 0;
    };

    template<typename Impl>
    requires std::derived_from<Impl, hal::SynchronousQuadratureEncoder>
    class QuadratureEncoderDecoratorImpl
        : public QuadratureEncoderDecorator
    {
    public:
        template<typename... Args>
        explicit QuadratureEncoderDecoratorImpl(uint32_t resolution, Args&&... args);

        foc::Radians Read() override;

    private:
        Impl encoder;
        float radiansPerCount;
    };

    template<typename Impl>
    requires std::derived_from<Impl, hal::SynchronousQuadratureEncoder>
    template<typename... Args>
    QuadratureEncoderDecoratorImpl<Impl>::QuadratureEncoderDecoratorImpl(uint32_t resolution, Args&&... args)
        : encoder(std::forward<Args>(args)...)
        , radiansPerCount{ resolution != 0 ? 2.0f * std::numbers::pi_v<float> / static_cast<float>(resolution) : 0.0f }
    {
    }

    template<typename Impl>
    requires std::derived_from<Impl, hal::SynchronousQuadratureEncoder>
    OPTIMIZE_FOR_SPEED foc::Radians QuadratureEncoderDecoratorImpl<Impl>::Read()
    {
        static constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
        static constexpr float pi = std::numbers::pi_v<float>;

        const auto angle = static_cast<float>(encoder.Position()) * radiansPerCount;

        return foc::Radians{ angle < pi ? angle : angle - twoPi };
    }
}
