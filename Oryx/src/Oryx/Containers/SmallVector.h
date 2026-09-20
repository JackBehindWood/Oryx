#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

// N elements inline, spills to the heap past N (doubling); not thread-safe, same as std::vector.
template<typename T, size_t N>
class SmallVector
{
    static_assert(N > 0, "SmallVector needs at least one inline slot");

public:
    SmallVector() = default;

    explicit SmallVector(size_t count)
    {
        reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            emplace_back_default();
        }
    }

    SmallVector(size_t count, const T& value)
    {
        reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            push_back(value);
        }
    }

    SmallVector(std::initializer_list<T> values)
    {
        reserve(values.size());
        for (const T& value : values)
        {
            push_back(value);
        }
    }

    SmallVector(const SmallVector& other)
    {
        reserve(other.m_size);
        for (size_t i = 0; i < other.m_size; ++i)
        {
            push_back(other.m_data[i]);
        }
    }

    SmallVector(SmallVector&& other) noexcept
    {
        move_from(other);
    }

    SmallVector& operator=(const SmallVector& other)
    {
        if (this != &other)
        {
            clear();
            reserve(other.m_size);
            for (size_t i = 0; i < other.m_size; ++i)
            {
                push_back(other.m_data[i]);
            }
        }
        return *this;
    }

    SmallVector& operator=(SmallVector&& other) noexcept
    {
        if (this != &other)
        {
            destroy_all();
            free_heap();
            m_size = 0;
            move_from(other);
        }
        return *this;
    }

    ~SmallVector()
    {
        destroy_all();
        free_heap();
    }

    void push_back(const T& value)
    {
        if (m_size == m_capacity)
        {
            T copy(value);
            push_back(std::move(copy));
            return;
        }
        new (&m_data[m_size]) T(value);
        ++m_size;
    }

    void push_back(T&& value)
    {
        ensure_capacity(m_size + 1);
        new (&m_data[m_size]) T(std::move(value));
        ++m_size;
    }

    void pop_back()
    {
        --m_size;
        m_data[m_size].~T();
    }

    void assign(size_t count, const T& value)
    {
        clear();
        reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            push_back(value);
        }
    }

    void reserve(size_t new_capacity) { ensure_capacity(new_capacity); }

    void clear()
    {
        destroy_all();
        m_size = 0;
    }

    [[nodiscard]] T& operator[](size_t index) { return m_data[index]; }
    [[nodiscard]] const T& operator[](size_t index) const { return m_data[index]; }

    [[nodiscard]] T& front() { return m_data[0]; }
    [[nodiscard]] const T& front() const { return m_data[0]; }
    [[nodiscard]] T& back() { return m_data[m_size - 1]; }
    [[nodiscard]] const T& back() const { return m_data[m_size - 1]; }

    [[nodiscard]] size_t size() const { return m_size; }
    [[nodiscard]] bool empty() const { return m_size == 0; }
    [[nodiscard]] size_t capacity() const { return m_capacity; }

    [[nodiscard]] T* begin() { return m_data; }
    [[nodiscard]] T* end() { return m_data + m_size; }
    [[nodiscard]] const T* begin() const { return m_data; }
    [[nodiscard]] const T* end() const { return m_data + m_size; }

    [[nodiscard]] bool operator==(const SmallVector& other) const
    {
        if (m_size != other.m_size)
        {
            return false;
        }
        for (size_t i = 0; i < m_size; ++i)
        {
            if (!(m_data[i] == other.m_data[i]))
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool operator!=(const SmallVector& other) const { return !(*this == other); }

private:
    void emplace_back_default()
    {
        ensure_capacity(m_size + 1);
        new (&m_data[m_size]) T();
        ++m_size;
    }

    void destroy_all()
    {
        for (size_t i = 0; i < m_size; ++i)
        {
            m_data[i].~T();
        }
    }

    void free_heap()
    {
        if (m_is_heap)
        {
            ::operator delete(m_data);
            m_data = inline_data();
            m_capacity = N;
            m_is_heap = false;
        }
    }

    void grow_to(size_t new_capacity)
    {
        T* new_data = static_cast<T*>(::operator new(new_capacity * sizeof(T)));
        for (size_t i = 0; i < m_size; ++i)
        {
            new (&new_data[i]) T(std::move(m_data[i]));
            m_data[i].~T();
        }
        free_heap();
        m_data = new_data;
        m_capacity = new_capacity;
        m_is_heap = true;
    }

    void ensure_capacity(size_t needed)
    {
        if (needed <= m_capacity)
        {
            return;
        }
        size_t new_capacity = m_capacity;
        while (new_capacity < needed)
        {
            new_capacity *= 2;
        }
        grow_to(new_capacity);
    }

    // Assumes *this is already empty/inline.
    void move_from(SmallVector& other)
    {
        if (other.m_is_heap)
        {
            m_data = other.m_data;
            m_capacity = other.m_capacity;
            m_is_heap = true;
            m_size = other.m_size;

            other.m_data = other.inline_data();
            other.m_capacity = N;
            other.m_is_heap = false;
            other.m_size = 0;
        }
        else
        {
            m_size = 0;
            for (size_t i = 0; i < other.m_size; ++i)
            {
                new (&m_data[m_size]) T(std::move(other.m_data[i]));
                ++m_size;
            }
            other.destroy_all();
            other.m_size = 0;
        }
    }

    T* inline_data() { return reinterpret_cast<T*>(m_inline_storage); }

    alignas(T) unsigned char m_inline_storage[N * sizeof(T)];
    T* m_data = inline_data();
    size_t m_size = 0;
    size_t m_capacity = N;
    bool m_is_heap = false;
};

} // namespace oryx
