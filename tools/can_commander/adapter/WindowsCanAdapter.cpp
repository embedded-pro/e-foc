#ifdef _WIN32

#include "tools/can_commander/adapter/WindowsCanAdapter.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace tool
{
    namespace
    {
        // -----------------------------------------------------------------------
        // PCAN (PCANBasic.dll) — types match PCANBasic.h
        // -----------------------------------------------------------------------
#pragma pack(push, 1)

        struct TPCANMsg
        {
            DWORD ID;
            BYTE MSGTYPE;
            BYTE LEN;
            BYTE DATA[8];
        };

        struct TPCANTimestamp
        {
            DWORD millis;
            WORD millis_overflow;
            WORD micros;
        };

#pragma pack(pop)

        constexpr DWORD PCAN_ERROR_OK = 0x00000U;
        constexpr DWORD PCAN_ERROR_QRCVEMPTY = 0x00020U;
        constexpr BYTE PCAN_CHANNEL_CONDITION = 0x03;
        constexpr DWORD PCAN_CHANNEL_AVAILABLE = 0x01;
        constexpr DWORD PCAN_CHANNEL_OCCUPIED = 0x02;
        constexpr BYTE PCAN_MESSAGE_EXTENDED = 0x04;

        // PCAN_USBBUS1 = 0x51, ..., PCAN_USBBUS16 = 0x60
        constexpr WORD PcanChannelHandle(int oneBasedIndex)
        {
            return static_cast<WORD>(0x51 + oneBasedIndex - 1);
        }

        constexpr WORD PcanBaudrateCode(uint32_t bitrate)
        {
            if (bitrate >= 1000000)
                return 0x0014; // PCAN_BAUD_1M
            if (bitrate >= 500000)
                return 0x001C; // PCAN_BAUD_500K
            if (bitrate >= 250000)
                return 0x011C; // PCAN_BAUD_250K
            return 0x031C;     // PCAN_BAUD_125K
        }

        using CAN_InitFn = DWORD(WINAPI*)(WORD, WORD, BYTE, DWORD, WORD);
        using CAN_UninitFn = DWORD(WINAPI*)(WORD);
        using CAN_ReadFn = DWORD(WINAPI*)(WORD, TPCANMsg*, TPCANTimestamp*);
        using CAN_WriteFn = DWORD(WINAPI*)(WORD, TPCANMsg*);
        using CAN_GetValFn = DWORD(WINAPI*)(WORD, BYTE, void*, DWORD);

        // -----------------------------------------------------------------------
        // Kvaser canlib — types match canlib.h
        // -----------------------------------------------------------------------
        constexpr int canOK = 0;
        constexpr int canERR_NOMSG = -2;
        constexpr int KVASER_BAUD_1M = -1;
        constexpr int KVASER_BAUD_500K = -2;
        constexpr int KVASER_BAUD_250K = -3;
        constexpr int KVASER_BAUD_125K = -4;
        constexpr int canOPEN_EXCLUSIVE = 0x0008;
        constexpr unsigned int canMSG_EXT = 0x0004;

        constexpr long KvaserBitrate(uint32_t bitrate)
        {
            if (bitrate >= 1000000)
                return KVASER_BAUD_1M;
            if (bitrate >= 500000)
                return KVASER_BAUD_500K;
            if (bitrate >= 250000)
                return KVASER_BAUD_250K;
            return KVASER_BAUD_125K;
        }

        using canInitLibFn = void (*)(void);
        using canGetNumChanFn = int (*)(int*);
        using canOpenChanFn = int (*)(int, int);
        using canSetBusFn = int (*)(int, long, unsigned, unsigned, unsigned, unsigned, unsigned);
        using canBusOnFn = int (*)(int);
        using canBusOffFn = int (*)(int);
        using canCloseFn = int (*)(int);
        using canWriteFn = int (*)(int, long, void*, unsigned, unsigned);
        using canReadFn = int (*)(int, long*, void*, unsigned*, unsigned*, unsigned long*);

        // -----------------------------------------------------------------------
        // slcan (CANable) helpers
        // -----------------------------------------------------------------------
        constexpr char SlcanBitrateCode(uint32_t rate)
        {
            if (rate >= 1000000)
                return '8';
            if (rate >= 800000)
                return '7';
            if (rate >= 500000)
                return '6';
            if (rate >= 250000)
                return '5';
            if (rate >= 125000)
                return '4';
            if (rate >= 100000)
                return '3';
            if (rate >= 50000)
                return '2';
            if (rate >= 20000)
                return '1';
            return '0';
        }

        bool SlcanWriteLine(HANDLE h, const char* line)
        {
            DWORD written = 0;
            return WriteFile(h, line, static_cast<DWORD>(strlen(line)), &written, nullptr) != 0;
        }

        // Parse one complete slcan line (without the trailing \r) into id + data.
        // Returns false if the line format is not a CAN data frame.
        bool ParseSlcanFrame(const std::string& line, uint32_t& outId, CanFrame& outData)
        {
            // Extended: T<8hexID><dlc><2*dlc hex bytes>
            // Standard: t<3hexID><dlc><2*dlc hex bytes>
            if (line.empty())
                return false;

            bool extended = (line[0] == 'T');
            if (line[0] != 'T' && line[0] != 't')
                return false;

            std::size_t idLen = extended ? 8 : 3;
            if (line.size() < 1 + idLen + 1)
                return false;

            try
            {
                outId = static_cast<uint32_t>(std::stoul(line.substr(1, idLen), nullptr, 16));
                uint8_t dlc = static_cast<uint8_t>(line[1 + idLen] - '0');
                if (dlc > 8)
                    return false;
                if (line.size() < 1 + idLen + 1 + 2u * dlc)
                    return false;

                outData.clear();
                for (uint8_t i = 0; i < dlc; ++i)
                {
                    std::string byteStr = line.substr(1 + idLen + 1 + 2u * i, 2);
                    outData.push_back(static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16)));
                }
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
    } // anonymous namespace

    // -----------------------------------------------------------------------
    // Constructor / Destructor
    // -----------------------------------------------------------------------
    WindowsCanAdapter::WindowsCanAdapter() = default;

    WindowsCanAdapter::~WindowsCanAdapter()
    {
        Disconnect();
    }

    // -----------------------------------------------------------------------
    // Connect – parses prefix and dispatches to backend
    // -----------------------------------------------------------------------
    bool WindowsCanAdapter::Connect(infra::BoundedConstString interfaceName, uint32_t bitrate)
    {
        if (IsConnected())
            Disconnect();

        std::string name(interfaceName.data(), interfaceName.size());
        bool ok = false;

        if (name.rfind("PCAN:", 0) == 0)
        {
            ok = ConnectPcan(std::stoi(name.substr(5)), bitrate);
        }
        else if (name.rfind("Kvaser:", 0) == 0)
        {
            ok = ConnectKvaser(std::stoi(name.substr(7)), bitrate);
        }
        else if (name.rfind("CANable:", 0) == 0)
        {
            ok = ConnectCanable(name.substr(8), bitrate);
        }
        else
        {
            PushError("Unknown interface prefix. Use PCAN:N, Kvaser:N, or CANable:COMx");
            return false;
        }

        if (ok)
        {
            running = true;
            receiveThread = std::thread(&WindowsCanAdapter::ReceiveLoop, this);
            NotifyObservers([](auto& obs)
                {
                    obs.OnConnectionChanged(true);
                });
        }
        return ok;
    }

    // -----------------------------------------------------------------------
    // Disconnect
    // -----------------------------------------------------------------------
    void WindowsCanAdapter::Disconnect()
    {
        running = false;
        if (receiveThread.joinable())
            receiveThread.join();

        DisconnectCurrent();
        NotifyObservers([](auto& obs)
            {
                obs.OnConnectionChanged(false);
            });
    }

    bool WindowsCanAdapter::IsConnected() const
    {
        return backend != WindowsCanBackend::none;
    }

    // -----------------------------------------------------------------------
    // Send
    // -----------------------------------------------------------------------
    void WindowsCanAdapter::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        if (!IsConnected())
        {
            actionOnCompletion(false);
            return;
        }

        uint32_t rawId = id.Get29BitId();
        bool success = false;

        switch (backend)
        {
            case WindowsCanBackend::pcan:
                success = SendPcan(rawId, data);
                break;
            case WindowsCanBackend::kvaser:
                success = SendKvaser(rawId, data);
                break;
            case WindowsCanBackend::canable:
                success = SendCanable(rawId, data);
                break;
            default:
                break;
        }

        if (success)
        {
            NotifyObservers([rawId, &data](auto& observer)
                {
                    observer.OnFrameLog(true, rawId, data);
                });
        }

        actionOnCompletion(success);
    }

    void WindowsCanAdapter::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        receiveCallback = receivedAction;
    }

    // -----------------------------------------------------------------------
    // ProcessReadEvent – called from the GUI's polling timer (main thread)
    // -----------------------------------------------------------------------
    void WindowsCanAdapter::ProcessReadEvent()
    {
        std::queue<Event> local;
        {
            std::lock_guard<std::mutex> lock(eventMutex);
            std::swap(local, eventQueue);
        }

        while (!local.empty())
        {
            auto ev = std::move(local.front());
            local.pop();

            if (ev.type == Event::Type::frame)
            {
                NotifyObservers([&ev](auto& obs)
                    {
                        obs.OnFrameLog(false, ev.id, ev.data);
                    });

                if (receiveCallback)
                    receiveCallback(hal::Can::Id::Create29BitId(ev.id), ev.data);
            }
            else
            {
                std::string msg = ev.errorMsg;
                NotifyObservers([&msg](auto& obs)
                    {
                        obs.OnError(infra::BoundedConstString(msg.data(), msg.size()));
                    });
            }
        }
    }

    // -----------------------------------------------------------------------
    // AvailableInterfaces
    // -----------------------------------------------------------------------
    void WindowsCanAdapter::ValidateDriverAvailability() const
    {
        bool hasPcan = false;
        HMODULE pcan = LoadLibraryA("PCANBasic.dll");
        if (pcan)
        {
            hasPcan = true;
            FreeLibrary(pcan);
        }

        bool hasKvaser = false;
        HMODULE kvaser = LoadLibraryA("canlib32.dll");
        if (!kvaser)
            kvaser = LoadLibraryA("canlib64.dll");
        if (kvaser)
        {
            hasKvaser = true;
            FreeLibrary(kvaser);
        }

        bool hasCanable = !CanableInterfaces().empty();

        if (!hasPcan && !hasKvaser && !hasCanable)
            throw std::runtime_error(
                "No CAN adapter driver found on this system.\n\n"
                "Install at least one of the following:\n"
                "  - Peak PCAN driver (PCANBasic.dll)\n"
                "  - Kvaser CANlib driver (canlib32.dll / canlib64.dll)\n"
                "  - CANable device (connected via USB/COM port)");
    }

    std::vector<std::string> WindowsCanAdapter::AvailableInterfaces() const
    {
        auto result = PcanInterfaces();
        auto kvaser = KvaserInterfaces();
        auto canable = CanableInterfaces();
        result.insert(result.end(), kvaser.begin(), kvaser.end());
        result.insert(result.end(), canable.begin(), canable.end());
        return result;
    }

    // -----------------------------------------------------------------------
    // Receive loop (runs on background thread)
    // -----------------------------------------------------------------------
    void WindowsCanAdapter::ReceiveLoop()
    {
        while (running)
        {
            uint32_t id = 0;
            CanFrame data;
            bool gotFrame = false;

            switch (backend)
            {
                case WindowsCanBackend::pcan:
                    gotFrame = ReadPcan(id, data);
                    break;
                case WindowsCanBackend::kvaser:
                    gotFrame = ReadKvaser(id, data);
                    break;
                case WindowsCanBackend::canable:
                    gotFrame = ReadCanable(id, data);
                    break;
                default:
                    break;
            }

            if (gotFrame)
                PushFrame(id, data);
            else
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // -----------------------------------------------------------------------
    // Queue helpers (thread-safe)
    // -----------------------------------------------------------------------
    void WindowsCanAdapter::PushFrame(uint32_t id, const CanFrame& data)
    {
        std::lock_guard<std::mutex> lock(eventMutex);
        eventQueue.push({ Event::Type::frame, id, data, {} });
    }

    void WindowsCanAdapter::PushError(const char* msg)
    {
        std::lock_guard<std::mutex> lock(eventMutex);
        eventQueue.push({ Event::Type::error, 0, {}, msg });
    }

    // -----------------------------------------------------------------------
    // PCAN backend
    // -----------------------------------------------------------------------
    bool WindowsCanAdapter::ConnectPcan(int channelIndex, uint32_t bitrate)
    {
        pcanDll = LoadLibraryA("PCANBasic.dll");
        if (!pcanDll)
        {
            PushError("PCANBasic.dll not found");
            return false;
        }

        auto init = PcanFn<CAN_InitFn>("CAN_Initialize");
        if (!init)
        {
            PushError("CAN_Initialize not found in PCANBasic.dll");
            FreeLibrary(pcanDll);
            pcanDll = nullptr;
            return false;
        }

        pcanChannel = PcanChannelHandle(channelIndex);
        DWORD status = init(pcanChannel, PcanBaudrateCode(bitrate), 0, 0, 0);
        if (status != PCAN_ERROR_OK)
        {
            FreeLibrary(pcanDll);
            pcanDll = nullptr;
            PushError("PCAN CAN_Initialize failed");
            return false;
        }

        backend = WindowsCanBackend::pcan;
        return true;
    }

    void WindowsCanAdapter::DisconnectCurrent()
    {
        if (backend == WindowsCanBackend::pcan && pcanDll)
        {
            auto uninit = PcanFn<CAN_UninitFn>("CAN_Uninitialize");
            if (uninit)
                uninit(pcanChannel);
            FreeLibrary(pcanDll);
            pcanDll = nullptr;
            pcanChannel = 0;
        }
        else if (backend == WindowsCanBackend::kvaser && kvaserDll)
        {
            auto busOff = KvaserFn<canBusOffFn>("canBusOff");
            auto close = KvaserFn<canCloseFn>("canClose");
            if (busOff && kvaserHandle >= 0)
                busOff(kvaserHandle);
            if (close && kvaserHandle >= 0)
                close(kvaserHandle);
            FreeLibrary(kvaserDll);
            kvaserDll = nullptr;
            kvaserHandle = -1;
        }
        else if (backend == WindowsCanBackend::canable && comHandle != INVALID_HANDLE_VALUE)
        {
            SlcanWriteLine(comHandle, "C\r");
            CloseHandle(comHandle);
            comHandle = INVALID_HANDLE_VALUE;
            slcanRxBuf.clear();
        }

        backend = WindowsCanBackend::none;
    }

    bool WindowsCanAdapter::SendPcan(uint32_t id, const CanFrame& data)
    {
        auto write = PcanFn<CAN_WriteFn>("CAN_Write");
        if (!write)
            return false;

        TPCANMsg msg{};
        msg.ID = id;
        msg.MSGTYPE = PCAN_MESSAGE_EXTENDED;
        msg.LEN = static_cast<BYTE>(std::min(data.size(), static_cast<std::size_t>(8)));
        std::memcpy(msg.DATA, data.data(), msg.LEN);

        return write(pcanChannel, &msg) == PCAN_ERROR_OK;
    }

    bool WindowsCanAdapter::ReadPcan(uint32_t& id, CanFrame& data)
    {
        auto read = PcanFn<CAN_ReadFn>("CAN_Read");
        if (!read)
            return false;

        TPCANMsg msg{};
        TPCANTimestamp ts{};
        DWORD status = read(pcanChannel, &msg, &ts);

        if (status != PCAN_ERROR_OK)
            return false;

        id = msg.ID;
        data.clear();
        for (BYTE i = 0; i < msg.LEN && i < 8; ++i)
            data.push_back(msg.DATA[i]);
        return true;
    }

    std::vector<std::string> WindowsCanAdapter::PcanInterfaces() const
    {
        HMODULE dll = LoadLibraryA("PCANBasic.dll");
        if (!dll)
            return {};

        auto getval = reinterpret_cast<CAN_GetValFn>(GetProcAddress(dll, "CAN_GetValue"));
        std::vector<std::string> result;

        if (getval)
        {
            for (int i = 1; i <= 16; ++i)
            {
                DWORD condition = 0;
                WORD ch = PcanChannelHandle(i);
                if (getval(ch, PCAN_CHANNEL_CONDITION, &condition, sizeof(condition)) == PCAN_ERROR_OK)
                {
                    if (condition == PCAN_CHANNEL_AVAILABLE || condition == PCAN_CHANNEL_OCCUPIED)
                        result.push_back("PCAN:" + std::to_string(i));
                }
            }
        }

        FreeLibrary(dll);
        return result;
    }

    // -----------------------------------------------------------------------
    // Kvaser backend
    // -----------------------------------------------------------------------
    bool WindowsCanAdapter::ConnectKvaser(int channelIndex, uint32_t bitrate)
    {
        kvaserDll = LoadLibraryA("canlib32.dll");
        if (!kvaserDll)
            kvaserDll = LoadLibraryA("canlib64.dll");
        if (!kvaserDll)
        {
            PushError("Kvaser canlib DLL not found");
            return false;
        }

        auto initLib = KvaserFn<canInitLibFn>("canInitializeLibrary");
        if (initLib)
            initLib();

        auto openChan = KvaserFn<canOpenChanFn>("canOpenChannel");
        auto setBus = KvaserFn<canSetBusFn>("canSetBusParams");
        auto busOn = KvaserFn<canBusOnFn>("canBusOn");

        if (!openChan || !setBus || !busOn)
        {
            PushError("Required Kvaser functions not found");
            FreeLibrary(kvaserDll);
            kvaserDll = nullptr;
            return false;
        }

        kvaserHandle = openChan(channelIndex, canOPEN_EXCLUSIVE);
        if (kvaserHandle < 0)
        {
            PushError("Kvaser canOpenChannel failed");
            FreeLibrary(kvaserDll);
            kvaserDll = nullptr;
            return false;
        }

        setBus(kvaserHandle, KvaserBitrate(bitrate), 0, 0, 0, 0, 0);

        if (busOn(kvaserHandle) != canOK)
        {
            PushError("Kvaser canBusOn failed");
            auto close = KvaserFn<canCloseFn>("canClose");
            if (close)
                close(kvaserHandle);
            FreeLibrary(kvaserDll);
            kvaserDll = nullptr;
            kvaserHandle = -1;
            return false;
        }

        backend = WindowsCanBackend::kvaser;
        return true;
    }

    bool WindowsCanAdapter::SendKvaser(uint32_t id, const CanFrame& data)
    {
        auto write = KvaserFn<canWriteFn>("canWrite");
        if (!write)
            return false;

        BYTE buf[8]{};
        unsigned dlc = static_cast<unsigned>(std::min(data.size(), static_cast<std::size_t>(8)));
        std::memcpy(buf, data.data(), dlc);

        return write(kvaserHandle, static_cast<long>(id), buf, dlc, canMSG_EXT) == canOK;
    }

    bool WindowsCanAdapter::ReadKvaser(uint32_t& id, CanFrame& data)
    {
        auto read = KvaserFn<canReadFn>("canRead");
        if (!read)
            return false;

        long msgId = 0;
        BYTE buf[8]{};
        unsigned dlc = 0;
        unsigned flags = 0;
        unsigned long t = 0;

        int status = read(kvaserHandle, &msgId, buf, &dlc, &flags, &t);
        if (status != canOK)
            return false;

        id = static_cast<uint32_t>(msgId);
        data.clear();
        for (unsigned i = 0; i < dlc && i < 8; ++i)
            data.push_back(buf[i]);
        return true;
    }

    std::vector<std::string> WindowsCanAdapter::KvaserInterfaces() const
    {
        HMODULE dll = LoadLibraryA("canlib32.dll");
        if (!dll)
            dll = LoadLibraryA("canlib64.dll");
        if (!dll)
            return {};

        auto initLib = reinterpret_cast<canInitLibFn>(GetProcAddress(dll, "canInitializeLibrary"));
        auto getCount = reinterpret_cast<canGetNumChanFn>(GetProcAddress(dll, "canGetNumberOfChannels"));

        std::vector<std::string> result;
        if (initLib)
            initLib();
        if (getCount)
        {
            int count = 0;
            if (getCount(&count) == canOK)
            {
                for (int i = 0; i < count; ++i)
                    result.push_back("Kvaser:" + std::to_string(i));
            }
        }

        FreeLibrary(dll);
        return result;
    }

    // -----------------------------------------------------------------------
    // CANable / slcan backend
    // -----------------------------------------------------------------------
    bool WindowsCanAdapter::ConnectCanable(const std::string& port, uint32_t bitrate)
    {
        // Open the COM port using the \\.\COMx form to support COM ports > 9
        std::string portPath = "\\\\.\\" + port;
        comHandle = CreateFileA(portPath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);

        if (comHandle == INVALID_HANDLE_VALUE)
        {
            PushError(("Failed to open " + port).c_str());
            return false;
        }

        DCB dcb{};
        dcb.DCBlength = sizeof(dcb);
        GetCommState(comHandle, &dcb);
        dcb.BaudRate = 115200;
        dcb.ByteSize = 8;
        dcb.StopBits = ONESTOPBIT;
        dcb.Parity = NOPARITY;
        SetCommState(comHandle, &dcb);

        // Non-blocking reads: return immediately with whatever is available
        COMMTIMEOUTS timeouts{};
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.ReadTotalTimeoutConstant = 0;
        SetCommTimeouts(comHandle, &timeouts);

        // Configure bitrate and open channel
        char setBaud[4] = { 'S', SlcanBitrateCode(bitrate), '\r', '\0' };
        SlcanWriteLine(comHandle, setBaud);
        SlcanWriteLine(comHandle, "O\r");

        slcanRxBuf.clear();
        backend = WindowsCanBackend::canable;
        return true;
    }

    bool WindowsCanAdapter::SendCanable(uint32_t id, const CanFrame& data)
    {
        if (comHandle == INVALID_HANDLE_VALUE)
            return false;

        // Use extended frame format: T<8 hex ID><DLC><data bytes>\r
        char buf[32];
        int pos = std::snprintf(buf, sizeof(buf), "T%08X%01X",
            static_cast<unsigned>(id),
            static_cast<unsigned>(std::min(data.size(), static_cast<std::size_t>(8))));

        for (std::size_t i = 0; i < data.size() && i < 8; ++i)
            pos += std::snprintf(buf + pos, sizeof(buf) - static_cast<std::size_t>(pos), "%02X", data[i]);

        buf[pos++] = '\r';
        buf[pos] = '\0';

        DWORD written = 0;
        return WriteFile(comHandle, buf, static_cast<DWORD>(pos), &written, nullptr) != 0;
    }

    bool WindowsCanAdapter::ReadCanable(uint32_t& outId, CanFrame& outData)
    {
        // Read any available bytes
        char chunk[64];
        DWORD bytesRead = 0;
        ReadFile(comHandle, chunk, sizeof(chunk) - 1, &bytesRead, nullptr);
        if (bytesRead > 0)
            slcanRxBuf.append(chunk, bytesRead);

        // Check for a complete frame (terminated by '\r')
        auto pos = slcanRxBuf.find('\r');
        if (pos == std::string::npos)
            return false;

        std::string line = slcanRxBuf.substr(0, pos);
        slcanRxBuf.erase(0, pos + 1);

        return ParseSlcanFrame(line, outId, outData);
    }

    std::vector<std::string> WindowsCanAdapter::CanableInterfaces() const
    {
        // Enumerate all COM ports visible to the system via the device namespace.
        char names[65536];
        std::memset(names, 0, sizeof(names));
        QueryDosDeviceA(nullptr, names, sizeof(names));

        std::vector<std::string> result;
        for (const char* p = names; *p; p += std::strlen(p) + 1)
        {
            if (std::strncmp(p, "COM", 3) == 0)
                result.push_back(std::string("CANable:") + p);
        }
        return result;
    }
} // namespace tool

#endif // _WIN32
