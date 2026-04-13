#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousQuadratureEncoder.hpp"
#include <cmath>
#include <numbers>

namespace application
{
    class QuadratureEncoderDecorator
    {
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
    };

    // Implementation

    template<typename Impl>
        requires std::derived_from<Impl, hal::SynchronousQuadratureEncoder>
    template<typename... Args>
    QuadratureEncoderDecoratorImpl<Impl>::QuadratureEncoderDecoratorImpl(uint32_t, Args&&... args)
        : encoder(std::forward<Args>(args)...)
    {
    }

    template<typename Impl>
        requires std::derived_from<Impl, hal::SynchronousQuadratureEncoder>
    foc::Radians QuadratureEncoderDecoratorImpl<Impl>::Read()
    {
        static constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
        static constexpr float pi = std::numbers::pi_v<float>;

        const auto count = static_cast<float>(encoder.Position());
        const auto resolution = static_cast<float>(encoder.Resolution());
        const auto angle = count * twoPi / resolution;
        const auto angle_plus_pi = angle + pi;
        const auto wrapped = angle - twoPi * std::floor(angle_plus_pi / twoPi);

        return foc::Radians{ wrapped };
    }
}
