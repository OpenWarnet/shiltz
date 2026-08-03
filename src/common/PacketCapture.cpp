#include "PacketCapture.h"

#include "HexDump.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace
{
    std::mutex g_mutex;
    std::ofstream g_packetsLog;
    std::ofstream g_unhandledLog;

    std::string Timestamp()
    {
        using namespace std::chrono;

        auto now = system_clock::now();
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        std::time_t t = system_clock::to_time_t(now);

        std::tm tm{};
        localtime_s(&tm, &t);

        std::ostringstream out;
        out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0')
            << ms.count();
        return out.str();
    }

    std::string SocketLabel(SOCKET clientSocket)
    {
        if (clientSocket == INVALID_SOCKET)
        {
            return "n/a";
        }
        return std::to_string(clientSocket);
    }

    void WriteEntry(std::ofstream& file, PacketCapture::Direction dir, SOCKET clientSocket,
                     uint32_t opcode, std::string_view opcodeName, std::span<const uint8_t> payload,
                     bool unhandled)
    {
        if (!file.is_open())
        {
            return;
        }

        const char* arrow = dir == PacketCapture::Direction::Inbound ? "C -> S" : "S -> C";

        file << '[' << Timestamp() << "] " << arrow << " | socket=" << SocketLabel(clientSocket)
             << " | " << opcodeName << " (0x" << std::hex << opcode << " / " << std::dec << opcode
             << ") | " << payload.size() << " bytes" << (unhandled ? " | UNHANDLED" : "") << '\n';

        file << FormatHexDump(payload) << '\n';
        file.flush();
    }
}

namespace PacketCapture
{
    void Init(const std::string& serverName)
    {
        std::filesystem::create_directories("logs");

        std::lock_guard lock(g_mutex);
        g_packetsLog.open("logs/" + serverName + "_packets.log", std::ios::app);
        g_unhandledLog.open("logs/" + serverName + "_unhandled.log", std::ios::app);
    }

    void LogHandled(Direction dir, SOCKET clientSocket, uint32_t opcode,
                     std::string_view opcodeName, std::span<const uint8_t> payload)
    {
        std::lock_guard lock(g_mutex);
        WriteEntry(g_packetsLog, dir, clientSocket, opcode, opcodeName, payload, false);
    }

    void LogUnhandled(SOCKET clientSocket, uint32_t opcode, std::string_view opcodeName,
                       std::span<const uint8_t> payload)
    {
        std::lock_guard lock(g_mutex);
        WriteEntry(g_packetsLog, Direction::Inbound, clientSocket, opcode, opcodeName, payload, true);
        WriteEntry(g_unhandledLog, Direction::Inbound, clientSocket, opcode, opcodeName, payload, true);
    }
}
