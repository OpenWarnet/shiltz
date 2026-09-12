#include "PacketCapture.h"

#include "HexDump.h"

#include <chrono>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    struct QueuedEntry
    {
        PacketCapture::Direction dir;
        uint32_t opcode;
        std::string opcodeName;
        std::vector<uint8_t> payload;
        bool unhandled;
    };

    std::string Timestamp()
    {
        using namespace std::chrono;

        auto now = system_clock::now();
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        std::time_t t = system_clock::to_time_t(now);

        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif

        std::ostringstream out;
        out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0')
            << ms.count();
        return out.str();
    }

    void WriteEntry(std::ofstream& file, PacketCapture::Direction dir, uint32_t opcode,
                    std::string_view opcodeName, std::span<const uint8_t> payload, bool unhandled)
    {
        if (!file.is_open())
        {
            return;
        }

        const char* arrow = dir == PacketCapture::Direction::Inbound ? "C -> S" : "S -> C";

        file << '[' << Timestamp() << "] " << arrow << " | " << opcodeName << " (0x" << std::hex
             << opcode << " / " << std::dec << opcode << ") | " << payload.size() << " bytes"
             << (unhandled ? " | UNHANDLED" : "") << '\n';

        file << FormatHexDump(payload) << '\n';
        file.flush();
    }

    // Owns the two log files and the background thread that writes to them.
    // Log* calls just push a copy onto m_queue and return; Run() drains it
    // and does the actual (locked, flushed) disk I/O off the caller's thread.
    // Declared after g_packetsLog/g_unhandledLog below so it's destroyed
    // first (reverse construction order), guaranteeing the writer thread is
    // stopped and joined -- so no pending write can touch the streams --
    // before those streams themselves are destroyed.
    class LogWriter
    {
    public:
        void Start() { m_thread = std::thread([this] { Run(); }); }

        void Push(QueuedEntry entry)
        {
            {
                std::lock_guard lock(m_mutex);
                m_queue.push_back(std::move(entry));
            }
            m_cv.notify_one();
        }

        ~LogWriter()
        {
            {
                std::lock_guard lock(m_mutex);
                m_stop = true;
            }
            m_cv.notify_one();
            if (m_thread.joinable())
            {
                m_thread.join();
            }
        }

    private:
        void Run();

        std::mutex m_mutex;
        std::condition_variable m_cv;
        std::deque<QueuedEntry> m_queue;
        std::thread m_thread;
        bool m_stop = false;
    };

    std::ofstream g_packetsLog;
    std::ofstream g_unhandledLog;
    LogWriter g_writer;

    void LogWriter::Run()
    {
        std::deque<QueuedEntry> batch;
        while (true)
        {
            {
                std::unique_lock lock(m_mutex);
                m_cv.wait(lock, [this] { return m_stop || !m_queue.empty(); });
                if (m_queue.empty() && m_stop)
                {
                    return;
                }
                batch.swap(m_queue);
            }

            for (const auto& entry : batch)
            {
                WriteEntry(g_packetsLog, entry.dir, entry.opcode, entry.opcodeName, entry.payload,
                           entry.unhandled);
                if (entry.unhandled)
                {
                    WriteEntry(g_unhandledLog, PacketCapture::Direction::Inbound, entry.opcode,
                               entry.opcodeName, entry.payload, true);
                }
            }
            batch.clear();
        }
    }
}

namespace PacketCapture
{
    void Init(const std::string& serverName)
    {
        std::filesystem::create_directories("logs");

        g_packetsLog.open("logs/" + serverName + "_packets.log", std::ios::app);
        g_unhandledLog.open("logs/" + serverName + "_unhandled.log", std::ios::app);

        g_writer.Start();
    }

    void LogHandled(Direction dir, uint32_t opcode, std::string_view opcodeName,
                    std::span<const uint8_t> payload)
    {
        g_writer.Push(QueuedEntry{dir, opcode, std::string(opcodeName),
                                   std::vector<uint8_t>(payload.begin(), payload.end()), false});
    }

    void LogUnhandled(uint32_t opcode, std::string_view opcodeName,
                      std::span<const uint8_t> payload)
    {
        g_writer.Push(QueuedEntry{Direction::Inbound, opcode, std::string(opcodeName),
                                   std::vector<uint8_t>(payload.begin(), payload.end()), true});
    }
}
