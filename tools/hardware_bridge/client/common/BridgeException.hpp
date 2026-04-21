#pragma once

#include <stdexcept>

namespace tool
{
    class BridgeException
        : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    class BridgeArgumentException
        : public BridgeException
    {
    public:
        using BridgeException::BridgeException;
    };

    class BridgeConnectionException
        : public BridgeException
    {
    public:
        using BridgeException::BridgeException;
    };
}
