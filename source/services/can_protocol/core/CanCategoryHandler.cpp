#include "source/services/can_protocol/core/CanCategoryHandler.hpp"

namespace services
{
    bool CanCategoryHandler::RequiresSequenceValidation() const
    {
        return true;
    }
}
