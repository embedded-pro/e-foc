#include "source/services/can_protocol/server/CanCategoryHandler.hpp"

namespace services
{
    bool CanCategoryHandler::RequiresSequenceValidation() const
    {
        return true;
    }
}
