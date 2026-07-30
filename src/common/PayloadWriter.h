#pragma once

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

class PayloadWriter
{
public:
    template <typename T> void Write(const T& value)
    {
        size_t offset = buffer.size();
        buffer.resize(offset + sizeof(T));

        std::memcpy(buffer.data() + offset, &value, sizeof(T));
    }

    void WriteString(const std::string& value, size_t size)
    {
        size_t offset = buffer.size();

        buffer.resize(offset + size, 0);

        size_t copySize = value.size() < size ? value.size() : size;

        std::memcpy(buffer.data() + offset, value.data(), copySize);
    }

    const std::vector<uint8_t>& Data() const
    {
        return buffer;
    }

private:
    std::vector<uint8_t> buffer;
};