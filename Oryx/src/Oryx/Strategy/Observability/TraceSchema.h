#pragma once

namespace oryx
{

// Additive fields and kinds never bump the version; readers skip what they do not know and reject a newer version.
constexpr int32_t k_trace_schema_version = 1;

namespace trace_kind
{
constexpr const char* header = "header";
constexpr const char* decision = "decision";
constexpr const char* match_end = "match_end";
} // namespace trace_kind

} // namespace oryx
