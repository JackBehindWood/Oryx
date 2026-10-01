#include "oxpch.h"
#include "Oryx/Simulation/BuildInfo.h"

namespace oryx
{

namespace
{

std::string git_hash()
{
#ifdef OX_GIT_HASH
    return OX_STRINGIFY_MACRO(OX_GIT_HASH);
#else
    return "unknown";
#endif
}

std::string profile_name()
{
#if defined(OX_DEBUG)
    return "debug";
#elif defined(OX_RELEASE)
    return "release";
#elif defined(OX_DIST)
    return "dist";
#else
    return "unknown";
#endif
}

std::string compiler_name()
{
#if defined(__clang__)
    return std::string("clang ") + __clang_version__;
#elif defined(__GNUC__)
    return std::string("gcc ") + __VERSION__;
#elif defined(_MSC_VER)
    return "msvc " + std::to_string(_MSC_VER);
#else
    return "unknown";
#endif
}

std::string platform_name()
{
#if defined(OX_PLATFORM_MACOS)
    std::string os = "macos";
#elif defined(OX_PLATFORM_LINUX)
    std::string os = "linux";
#else
    std::string os = "unknown";
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
    return os + "-arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return os + "-x64";
#else
    return os;
#endif
}

} // namespace

BuildInfo build_info()
{
    BuildInfo info;
    info.version = std::to_string(VERSION_MAJOR) + "." + std::to_string(VERSION_MINOR) + "." + std::to_string(VERSION_PATCH);
    info.git_hash = git_hash();
    info.profile = profile_name();
    info.compiler = compiler_name();
    info.platform = platform_name();
    return info;
}

} // namespace oryx
