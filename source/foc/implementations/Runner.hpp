#pragma once

#include "source/foc/interfaces/Driver.hpp"
#include "source/foc/interfaces/Foc.hpp"

namespace foc
{
    class Runner
    {
    public:
        Runner(MotorDriver& driver, Encoder& encoder, FocBase& foc);

    private:
        MotorDriver& driver;
        Encoder& encoder;
        FocBase& foc;
    };
}
