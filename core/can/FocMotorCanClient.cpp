#include "core/can/FocMotorCanClient.hpp"

namespace can
{
    FocMotorCanClient::FocMotorCanClient(hal::Can& can)
        : originTracker{ can }
        , protocolClient{ originTracker }
        , categoryClient{ protocolClient.Transport(), protocolClient, originTracker }
    {
        protocolClient.RegisterCategory(categoryClient);
    }

    services::CanProtocolClient& FocMotorCanClient::ProtocolClient()
    {
        return protocolClient;
    }

    FocMotorCategoryClient& FocMotorCanClient::CategoryClient()
    {
        return categoryClient;
    }

    const FocMotorCategoryClient& FocMotorCanClient::CategoryClient() const
    {
        return categoryClient;
    }
}
