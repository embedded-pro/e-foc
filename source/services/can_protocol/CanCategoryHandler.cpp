#include "source/services/can_protocol/CanCategoryHandler.hpp"

namespace services
{
    bool CanCategoryHandler::RequiresSequenceValidation() const
    {
        return true;
    }
}
