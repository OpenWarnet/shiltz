#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// Shared streaming reader for Seal Online's ".scr" data tables: a handful
// of leading lines that are bare tokens (row/element counts), followed by
// pipe-delimited rows of tokens -- one game-data record per row, trailing
// "|" optional. Blank lines are skipped.
//
// This is a pure tokenizer, not a data holder: Parse()/Load() scan the
// text once and invoke `onLine` inline, per non-blank line, as they go --
// there's no owned table to build and no second pass to walk it
// afterward. ScrTable doesn't know which lines are headers and which are
// rows -- that's a fact about the specific format (monster.scr has one
// header line, npcNN.scr has two), so it's left entirely to the caller,
// via the 0-based `lineIndex` handed to `onLine` (see MonsterScr.h and
// NpcScr.h, and add new ones here as more .scr tables come online).
//
// The token list handed to `onLine` is a scratch buffer reused for every
// line in the file, valid only for the duration of that one call; copy
// out whatever you need to keep (typically by converting straight to a
// typed record). Tokens themselves are string_views into the caller's own
// buffer, not owned strings -- ScrTable doesn't know or care whether a
// token is numeric (not every .scr table is a pure integer grid), so use
// ParseInt64() for numeric columns and construct an owned std::string
// yourself for a column you actually need to keep as text.
class ScrTable
{
public:
    // Scans `text` once. `onLine` is called for each non-blank line with
    // its 0-based index (counting only non-blank lines) and that line's
    // pipe-delimited tokens -- a bare line with no `|` still comes
    // through as a 1-token line.
    template <typename LineFn>
    static void Parse(std::string_view text, LineFn&& onLine);

    // Same, reading the file first. Throws std::runtime_error if it can't
    // be opened.
    template <typename LineFn>
    static void Load(const std::filesystem::path& path, LineFn&& onLine);

    // Parses a token as handed to onLine into an int64 -- empty or
    // non-numeric tokens (or one with no valid leading digit at all) read
    // as 0 rather than throwing. Shared by every typed parser that has
    // integer columns.
    static std::int64_t ParseInt64(std::string_view token);

private:
    static std::string ReadFile(const std::filesystem::path& path);

    // One trimmed token -- start/end are nudged inward past surrounding
    // spaces/tabs; the returned view is zero-copy (just a sub-span).
    static std::string_view MakeToken(std::string_view text, size_t start, size_t end);
};

template <typename LineFn>
void ScrTable::Parse(std::string_view text, LineFn&& onLine)
{
    const size_t n = text.size();
    size_t i = 0;
    size_t lineIndex = 0;

    // Reused for every line in the file -- after the first line grows it
    // to its natural width, clear() keeps that capacity, so later lines
    // cost zero further allocation.
    std::vector<std::string_view> tokens;

    while (i < n)
    {
        tokens.clear();
        size_t tokenStart = i;
        size_t lastNonEmptyLen = 0;
        bool hasContent = false;

        while (i < n && text[i] != '\n' && text[i] != '\r')
        {
            if (text[i] == '|')
            {
                tokens.push_back(MakeToken(text, tokenStart, i));
                if (!tokens.back().empty())
                {
                    lastNonEmptyLen = tokens.size();
                }

                hasContent = true;
                tokenStart = i + 1;
            }
            else if (text[i] != ' ' && text[i] != '\t')
            {
                hasContent = true;
            }

            ++i;
        }

        if (hasContent)
        {
            std::string_view trailing = MakeToken(text, tokenStart, i);
            tokens.push_back(trailing);
            if (!tokens.back().empty())
            {
                lastNonEmptyLen = tokens.size();
            }

            // Reproduces "strip trailing pipe(s), then split": a line that
            // ends right after a '|' drops that empty final cell, while an
            // empty cell in the middle (e.g. `3||355`) is kept as "".
            tokens.resize(lastNonEmptyLen);

            // A line that trims down to nothing but empty cells (e.g. a
            // stray "|" with no data on either side -- seen in the wild in
            // item21.scr) is equivalent to blank: skip it the same way,
            // rather than handing every BuildRecord() a zero-token row none
            // of them guard against (row[0] is assumed to exist).
            if (!tokens.empty())
            {
                onLine(lineIndex, static_cast<const std::vector<std::string_view>&>(tokens));
                ++lineIndex;
            }
        }
        // else: a blank line -- dropped, doesn't consume a line index.

        if (i < n)
        {
            if (text[i] == '\r' && i + 1 < n && text[i + 1] == '\n')
            {
                i += 2;
            }
            else
            {
                ++i;
            }
        }
    }
}

template <typename LineFn>
void ScrTable::Load(const std::filesystem::path& path, LineFn&& onLine)
{
    std::string text = ReadFile(path);
    Parse(std::string_view(text), std::forward<LineFn>(onLine));
}
