#pragma once

#include "core/platform_abstraction/CanBusAdapter.hpp"
#include "infra/util/Function.hpp"
#include "targets/platform_implementations/qemu/implementation/EmulatedCan.hpp"
#include <optional>

namespace application
{
    class EmulatedCanBusAdapter
        : public CanBusAdapter
    {
    public:
        explicit EmulatedCanBusAdapter(uintptr_t uartBase);

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;
        void SetOnError(const infra::Function<void(CanError)>& handler) override;

        std::optional<sil::EmulatedCan::Frame> PollIncoming();

    private:
        sil::EmulatedCan can;
    };
}
