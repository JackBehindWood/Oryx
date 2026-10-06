#pragma once

namespace oryx
{

inline constexpr uint32_t UTF8_REPLACEMENT = 0xFFFD;

// Returns the codepoint starting at `index` and advances past it. A stray continuation byte, a truncated, overlong or surrogate sequence,
// or a value above U+10FFFF yields UTF8_REPLACEMENT and consumes only the offending lead byte plus the valid continuations that follow it, so decoding always progresses.
[[nodiscard]] uint32_t decode_utf8(std::string_view text, size_t& index);
void encode_utf8(uint32_t codepoint, std::string& out);

} // namespace oryx
