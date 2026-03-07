#pragma once

// WindowsCanAdapter provides CAN bus access on Windows through three backends,
// all loaded at runtime — no SDK is required at compile time:
//
//   PCAN:<N>       — Peak System PCANBasic.dll, e.g. "PCAN:1" for PCAN_USBBUS1
//   Kvaser:<N>     — Kvaser canlib64.dll / canlib32.dll, e.g. "Kvaser:0"
//   CANable:<COMx> — slcan-firmware CANable over serial port, e.g. "CANable:COM3"
//
// FileDescriptor() always returns -1 on Windows; the GUI layer must call
// ProcessReadEvent() periodically (e.g. via a 1 ms QTimer) to dispatch
// received frames delivered by the internal receive thread.

#include "source/tool/can_commander/adapter/CanBusAdapter.hpp"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <windows.h>

namespace tool
{
    enum class WindowsCanBackend
    {
        none,
        pcan,
        kvaser,
        canable
    };

    class WindowsCanAdapter : public CanBusAdapter
    {
    public:
        WindowsCanAdapter();
        ~WindowsCanAdapter() override;

        bool Connect(infra::BoundedConstString interfaceName, uint32_t bitrate) override;
        void Disconnect() override;
        bool IsConnected() const override;
        bool Send(uint32_t id, const CanFrame& data) override;

        int FileDescriptor() const override
        {
            return -1;
        }

        void ProcessReadEvent() override;

        std::vector<std::string> AvailableInterfaces() const override;

        // Throws std::runtime_error if none of the supported CAN driver
        // backends (PCAN, Kvaser, CANable/serial) can be found.
        void ValidateDriverAvailability() const override;

    private:
        struct Event
        {
            enum class Type
            {
                frame,
                error
            } type;
            uint32_t id{};
            CanFrame data;
            std::string errorMsg;
        };

        void ReceiveLoop();

        bool ConnectPcan(int channelIndex, uint32_t bitrate);
        bool ConnectKvaser(int channelIndex, uint32_t bitrate);
        bool ConnectCanable(const std::string& port, uint32_t bitrate);
        void DisconnectCurrent();

        bool SendPcan(uint32_t id, const CanFrame& data);
        bool SendKvaser(uint32_t id, const CanFrame& data);
        bool SendCanable(uint32_t id, const CanFrame& data);

        bool ReadPcan(uint32_t& id, CanFrame& data);
        bool ReadKvaser(uint32_t& id, CanFrame& data);
        bool ReadCanable(uint32_t& id, CanFrame& data);

        std::vector<std::string> PcanInterfaces() const;
        std::vector<std::string> KvaserInterfaces() const;
        std::vector<std::string> CanableInterfaces() const;

        void PushFrame(uint32_t id, const CanFrame& data);
        void PushError(const char* msg);

        template<typename T>
        T PcanFn(const char* name) const
        {
            return reinterpret_cast<T>(GetProcAddress(pcanDll, name));
        }

        template<typename T>
        T KvaserFn(const char* name) const
        {
            return reinterpret_cast<T>(GetProcAddress(kvaserDll, name));
        }

        WindowsCanBackend backend{ WindowsCanBackend::none };

        HMODULE pcanDll{ nullptr };
        WORD pcanChannel{ 0 };

        HMODULE kvaserDll{ nullptr };
        int kvaserHandle{ -1 };

        HANDLE comHandle{ INVALID_HANDLE_VALUE };
        std::string slcanRxBuf;

        std::thread receiveThread;
        std::atomic<bool> running{ false };

        std::mutex eventMutex;
        std::queue<Event> eventQueue;
    };
}

#endif // _WIN32
