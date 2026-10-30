#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"
#include <stdexcept>
#include <thread>

namespace integration
{
    namespace
    {
        TargetInteractor* g_instance{ nullptr };
    }

    TargetInteractor& TargetInteractor::Instance()
    {
        if (g_instance == nullptr)
            throw std::runtime_error{ "TargetInteractor instance not set" };
        return *g_instance;
    }

    void TargetInteractor::SetInstance(TargetInteractor& instance)
    {
        g_instance = &instance;
    }

    bool TargetInteractor::SupportsSimulatedPlant() const
    {
        return false;
    }

    void TargetInteractor::ConfigurePlant(const std::vector<uint8_t>&)
    {}

    void TargetInteractor::ConfigureNonVolatileMemory(const std::vector<uint8_t>&)
    {}

    std::size_t TargetInteractor::ReceivedCanFrameCount() const
    {
        return 0;
    }

    std::optional<hal::Can::Message> TargetInteractor::FindReceivedCanFrame(hal::Can::Id, std::size_t) const
    {
        return std::nullopt;
    }

    std::vector<hal::Can::Message> TargetInteractor::FindReceivedCanFrames(hal::Can::Id, std::size_t) const
    {
        return {};
    }

    std::optional<hal::Can::Id> TargetInteractor::FindReceivedCanFrameId(const std::function<bool(hal::Can::Id)>&, std::size_t) const
    {
        return std::nullopt;
    }

    void TargetInteractor::RestartTarget()
    {}

    std::vector<std::string> TargetInteractor::TakePlantRecord(std::chrono::milliseconds timeout)
    {
        std::this_thread::sleep_for(timeout);
        return {};
    }

    std::string TargetInteractor::DebugLog() const
    {
        return {};
    }
}
