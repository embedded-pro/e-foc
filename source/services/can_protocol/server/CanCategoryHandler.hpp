#pragma once

#include "hal/interfaces/Can.hpp"
#include "source/services/can_protocol/core/CanProtocolDefinitions.hpp"

namespace services
{
    class CanCategoryHandler
    {
    protected:
        CanCategoryHandler() = default;
        CanCategoryHandler(const CanCategoryHandler&) = delete;
        CanCategoryHandler& operator=(const CanCategoryHandler&) = delete;
        ~CanCategoryHandler() = default;

    public:
        virtual CanCategory Category() const = 0;
        virtual bool RequiresSequenceValidation() const;
        virtual void Handle(CanMessageType messageType, const hal::Can::Message& data) = 0;
    };
}
