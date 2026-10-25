#include "core/state_machine/ConfigWriter.hpp"

namespace state_machine
{
    ConfigWriter::ConfigWriter(services::NonVolatileMemory& nvm, const services::ConfigData& config, services::Tracer& tracer,
        application::NvmActivity& nvmActivity)
        : nvm(nvm)
        , config(config)
        , tracer(tracer)
        , nvmActivity(nvmActivity)
    {}

    void ConfigWriter::Persist(const infra::Function<void(services::NvmStatus)>& onDone)
    {
        if (writing)
        {
            if (onDone != nullptr && onRewritten != nullptr)
            {
                onDone(services::NvmStatus::Busy);
                return;
            }

            rewrite = true;
            if (onDone != nullptr)
                onRewritten = onDone;
            return;
        }

        writing = true;
        onWritten = onDone;
        nvmActivity.Begin();
        Write();
    }

    bool ConfigWriter::HasPendingPersist() const
    {
        return writing;
    }

    void ConfigWriter::Write()
    {
        nvm.SaveConfig(config, [this](services::NvmStatus status)
            {
                OnWritten(status);
            });
    }

    // Busy is contention with another writer of the shared NVM, not a failure of this record
    void ConfigWriter::OnWritten(services::NvmStatus status)
    {
        if (status == services::NvmStatus::Busy)
        {
            retryTimer.Start(retryDelay, [this]()
                {
                    Write();
                });
            return;
        }

        if (status != services::NvmStatus::Ok)
            tracer.Trace() << "config persist failed";

        auto done = onWritten;
        onWritten = nullptr;

        if (rewrite)
        {
            rewrite = false;
            onWritten = onRewritten;
            onRewritten = nullptr;
            Write();
        }
        else
        {
            writing = false;
            nvmActivity.End();
        }

        if (done != nullptr)
            done(status);
    }
}
