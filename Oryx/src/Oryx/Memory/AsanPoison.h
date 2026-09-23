#pragma once

#if defined(__has_feature)
    #if __has_feature(address_sanitizer)
        #define OX_ASAN_ENABLED 1
    #endif
#endif
#if defined(__SANITIZE_ADDRESS__) && !defined(OX_ASAN_ENABLED)
    #define OX_ASAN_ENABLED 1
#endif

#ifdef OX_ASAN_ENABLED
    #include <sanitizer/asan_interface.h>
    #define OX_ASAN_POISON(address, size) ASAN_POISON_MEMORY_REGION(address, size)
    #define OX_ASAN_UNPOISON(address, size) ASAN_UNPOISON_MEMORY_REGION(address, size)
#else
    #define OX_ASAN_POISON(address, size) ((void)(address), (void)(size))
    #define OX_ASAN_UNPOISON(address, size) ((void)(address), (void)(size))
#endif
