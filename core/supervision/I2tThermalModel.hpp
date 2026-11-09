#pragma once

namespace supervision
{
    class I2tThermalModel
    {
    public:
        struct Rating
        {
            float continuous;
            float peak;
            float peakSeconds;
        };

        static float TimeConstant(const Rating& rating);

        void Update(float current, const Rating& rating, float stepSeconds);
        void Reset();
        float State() const;

    private:
        float squaredCurrentState{ 0.0f };
    };
}
