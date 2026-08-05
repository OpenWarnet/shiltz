#include "ScrTable.h"

#include <charconv>
#include <fstream>
#include <stdexcept>
#include <system_error>

std::string ScrTable::ReadFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        throw std::runtime_error("ScrTable: failed to open " + path.string());
    }

    std::string contents(static_cast<size_t>(file.tellg()), '\0');
    file.seekg(0);
    file.read(contents.data(), static_cast<std::streamsize>(contents.size()));

    return contents;
}

std::string_view ScrTable::MakeToken(std::string_view text, size_t start, size_t end)
{
    while (start < end && (text[start] == ' ' || text[start] == '\t'))
    {
        ++start;
    }

    while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t'))
    {
        --end;
    }

    return text.substr(start, end - start);
}

std::int64_t ScrTable::ParseInt64(std::string_view token)
{
    std::int64_t value = 0;
    auto result = std::from_chars(token.data(), token.data() + token.size(), value);

    if (result.ec != std::errc())
    {
        return 0;
    }

    return value;
}
