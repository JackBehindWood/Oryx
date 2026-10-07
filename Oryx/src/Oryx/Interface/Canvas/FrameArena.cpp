#include "oxpch.h"
#include "Oryx/Interface/Canvas/FrameArena.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

int32_t vsnprintf_c(char* buffer, size_t size, const char* format, va_list args)
{
    static const locale_t c_locale = newlocale(LC_ALL_MASK, "C", nullptr);
    const locale_t previous = uselocale(c_locale);
    const int32_t length = std::vsnprintf(buffer, size, format, args);
    uselocale(previous);
    return length;
}

int32_t snprintf_c(char* buffer, size_t size, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const int32_t length = vsnprintf_c(buffer, size, format, args);
    va_end(args);
    return length;
}

FrameArena::~FrameArena()
{
    clear();
}

void FrameArena::reset()
{
    for (Chunk* chunk = m_head; chunk != nullptr; chunk = chunk->next)
    {
        chunk->used = 0;
    }
    m_current = m_head;
    m_used_total = 0;
}

void FrameArena::clear()
{
    while (m_head != nullptr)
    {
        Chunk* next = m_head->next;
        ::operator delete(m_head);
        m_head = next;
    }
    m_tail = nullptr;
    m_current = nullptr;
    m_used_total = 0;
}

void* FrameArena::allocate(size_t size, size_t alignment)
{
    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    {
        throw Error("FrameArena alignment must be a power of two");
    }
    for (; m_current != nullptr; m_current = m_current->next)
    {
        const uintptr_t base = reinterpret_cast<uintptr_t>(m_current->data);
        const size_t offset = ((base + m_current->used + alignment - 1) & ~(alignment - 1)) - base;
        if (offset + size <= m_current->size)
        {
            m_current->used = offset + size;
            m_used_total += size;
            return m_current->data + offset;
        }
    }
    const size_t chunk_bytes = std::max(m_chunk_size, size + alignment);
    Chunk* chunk = new (::operator new(sizeof(Chunk) + chunk_bytes)) Chunk;
    chunk->data = reinterpret_cast<uint8_t*>(chunk + 1);
    chunk->size = chunk_bytes;
    if (m_tail != nullptr)
    {
        m_tail->next = chunk;
    }
    else
    {
        m_head = chunk;
    }
    m_tail = chunk;
    m_current = chunk;
    return allocate(size, alignment);
}

std::string_view FrameArena::store(std::string_view text)
{
    char* destination = static_cast<char*>(allocate(text.size() + 1, 1));
    std::memcpy(destination, text.data(), text.size());
    destination[text.size()] = '\0';
    return std::string_view(destination, text.size());
}

std::string_view FrameArena::format(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    va_list measure;
    va_copy(measure, args);
    const int32_t length = vsnprintf_c(nullptr, 0, format, measure);
    va_end(measure);
    if (length < 0)
    {
        va_end(args);
        throw Error("FrameArena format failed");
    }
    char* destination = static_cast<char*>(allocate(static_cast<size_t>(length) + 1, 1));
    vsnprintf_c(destination, static_cast<size_t>(length) + 1, format, args);
    va_end(args);
    return std::string_view(destination, static_cast<size_t>(length));
}

size_t FrameArena::capacity() const
{
    size_t total = 0;
    for (const Chunk* chunk = m_head; chunk != nullptr; chunk = chunk->next)
    {
        total += chunk->size;
    }
    return total;
}

} // namespace oryx
