#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

class PayloadReader
{
public:
    PayloadReader(const std::vector<uint8_t>& data) : buffer(data) {}

    template <typename T> bool Read(T& value)
    {
        if (offset + sizeof(T) > buffer.size())
            return false;

        std::memcpy(&value, buffer.data() + offset, sizeof(T));
        offset += sizeof(T);

        return true;
    }

    bool ReadBytes(std::vector<uint8_t>& out, size_t size)
    {
        if (offset + size > buffer.size())
            return false;

        out.assign(buffer.begin() + offset, buffer.begin() + offset + size);

        offset += size;

        return true;
    }

    bool ReadString(std::string& value, size_t size)
    {
        if (offset + size > buffer.size())
            return false;

        value.assign(reinterpret_cast<const char*>(buffer.data() + offset),
                     strnlen(reinterpret_cast<const char*>(buffer.data() + offset), size));

        offset += size;

        return true;
    }

    size_t Remaining() const
    {
        return buffer.size() - offset;
    }

private:
    const std::vector<uint8_t>& buffer;
    size_t offset = 0;
};