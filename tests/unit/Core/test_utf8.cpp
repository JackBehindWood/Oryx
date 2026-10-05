#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

namespace
{

uint32_t decode_one(const std::string& text, size_t& index)
{
    return decode_utf8(text, index);
}

} // namespace

TEST_CASE("utf8: codepoints round trip through every encoded length")
{
    const uint32_t samples[] = { 'A', 0x7F, 0x80, 0xE9, 0x7FF, 0x800, 0x20AC, 0xFFFF, 0x10000, 0x1F600, 0x10FFFF };
    for (uint32_t codepoint : samples)
    {
        std::string text;
        encode_utf8(codepoint, text);
        size_t index = 0;
        CHECK(decode_one(text, index) == codepoint);
        CHECK(index == text.size());
    }
}

TEST_CASE("utf8: invalid sequences decode to the replacement and always progress")
{
    const std::string cases[] = {
        std::string("\x80", 1),
        std::string("\xC3", 1),
        std::string("\xE2\x82", 2),
        std::string("\xC0\xAF", 2),
        std::string("\xE0\x80\xAF", 3),
        std::string("\xED\xA0\x80", 3),
        std::string("\xF4\x90\x80\x80", 4),
        std::string("\xFF", 1),
        std::string("\xF8\x88\x80\x80\x80", 5),
    };
    for (const std::string& text : cases)
    {
        size_t index = 0;
        size_t steps = 0;
        while (index < text.size())
        {
            const size_t before = index;
            CHECK(decode_one(text, index) == UTF8_REPLACEMENT);
            CHECK(index > before);
            ++steps;
        }
        CHECK(steps <= text.size());
    }
}

TEST_CASE("utf8: a truncated sequence does not swallow the next character")
{
    const std::string text = std::string("\xE2\x82", 2) + "A";
    size_t index = 0;
    CHECK(decode_one(text, index) == UTF8_REPLACEMENT);
    CHECK(decode_one(text, index) == 'A');
}

TEST_CASE("utf8: encoding an invalid codepoint writes the replacement")
{
    std::string text;
    encode_utf8(0xD800, text);
    encode_utf8(0x110000, text);
    size_t index = 0;
    CHECK(decode_one(text, index) == UTF8_REPLACEMENT);
    CHECK(decode_one(text, index) == UTF8_REPLACEMENT);
    CHECK(index == text.size());
}
