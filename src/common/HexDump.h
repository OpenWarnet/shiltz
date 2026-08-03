#pragma once

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <span>
#include <string>

// Classic hexdump -C style formatting: 16 bytes per row, hex on the left,
// printable ASCII (or '.' for non-printable) on the right. Used by
// PacketCapture to render decrypted packet payloads for dev-facing logs.
inline std::string FormatHexDump(std::span<const uint8_t> data)
{
    std::ostringstream out;

    for (size_t row = 0; row < data.size(); row += 16)
    {
        size_t rowLen = std::min<size_t>(16, data.size() - row);

        out << std::hex << std::setw(8) << std::setfill('0') << row << "  ";

        for (size_t i = 0; i < 16; ++i)
        {
            if (i < rowLen)
            {
                out << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(data[row + i]) << ' ';
            }
            else
            {
                out << "   ";
            }
            if (i == 7)
            {
                out << ' ';
            }
        }

        out << " |";
        for (size_t i = 0; i < rowLen; ++i)
        {
            uint8_t c = data[row + i];
            out << (c >= 0x20 && c < 0x7f ? static_cast<char>(c) : '.');
        }
        out << "|\n";
    }

    return out.str();
}
