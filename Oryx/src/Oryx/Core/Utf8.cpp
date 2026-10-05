#include "oxpch.h"
#include "Oryx/Core/Utf8.h"

namespace oryx
{

namespace
{

bool is_continuation(char byte)
{
    return (static_cast<uint8_t>(byte) & 0xC0) == 0x80;
}

} // namespace

uint32_t decode_utf8(std::string_view text, size_t& index)
{
    const uint8_t lead = static_cast<uint8_t>(text[index]);
    if (lead < 0x80)
    {
        ++index;
        return lead;
    }
    uint32_t length = 0;
    uint32_t codepoint = 0;
    uint32_t minimum = 0;
    if ((lead & 0xE0) == 0xC0)
    {
        length = 2;
        codepoint = lead & 0x1F;
        minimum = 0x80;
    }
    else if ((lead & 0xF0) == 0xE0)
    {
        length = 3;
        codepoint = lead & 0x0F;
        minimum = 0x800;
    }
    else if ((lead & 0xF8) == 0xF0)
    {
        length = 4;
        codepoint = lead & 0x07;
        minimum = 0x10000;
    }
    ++index;
    if (length == 0)
    {
        return UTF8_REPLACEMENT;
    }
    for (uint32_t i = 1; i < length; ++i)
    {
        if (index >= text.size() || !is_continuation(text[index]))
        {
            return UTF8_REPLACEMENT;
        }
        codepoint = (codepoint << 6) | (static_cast<uint8_t>(text[index]) & 0x3F);
        ++index;
    }
    const bool surrogate = codepoint >= 0xD800 && codepoint <= 0xDFFF;
    return (codepoint < minimum || surrogate || codepoint > 0x10FFFF) ? UTF8_REPLACEMENT : codepoint;
}

void encode_utf8(uint32_t codepoint, std::string& out)
{
    if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF))
    {
        codepoint = UTF8_REPLACEMENT;
    }
    if (codepoint < 0x80)
    {
        out.push_back(static_cast<char>(codepoint));
    }
    else if (codepoint < 0x800)
    {
        out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    else if (codepoint < 0x10000)
    {
        out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    else
    {
        out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

} // namespace oryx
