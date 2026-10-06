#pragma once

#include "Oryx/Memory/IAllocator.h"
#include "Oryx/Memory/SmartPointers.h"

namespace oryx
{

class RefCounted;
template<typename T>
class Ref;

namespace detail
{

void destroy_now(RefCounted& object) noexcept;

} // namespace detail

// Intrusive atomic reference count for objects created by make_ref / allocate_ref, which must be the only way to create them.
class RefCounted
{
public:
    RefCounted(const RefCounted&) = delete;
    RefCounted& operator=(const RefCounted&) = delete;
    virtual ~RefCounted() = default;

    void add_ref() const noexcept { m_count.fetch_add(1, std::memory_order_relaxed); }

    void release() const noexcept
    {
        if (m_count.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            const_cast<RefCounted&>(*this).on_last_release();
        }
    }

    [[nodiscard]] uint32_t ref_count() const noexcept { return m_count.load(std::memory_order_relaxed); }

protected:
    RefCounted() = default;

    // Runs when the last reference drops; an override takes over destruction and must end in detail::destroy_now.
    virtual void on_last_release() noexcept { detail::destroy_now(*this); }

private:
    friend void detail::destroy_now(RefCounted& object) noexcept;
    template<typename T, typename... Args>
    friend Ref<T> allocate_ref(IAllocator& allocator, Args&&... args);

    mutable std::atomic<uint32_t> m_count{ 1 };
    uint32_t m_size = 0;
    uint32_t m_alignment = 0;
    IAllocator* m_allocator = nullptr;
};

template<typename T>
class Ref
{
public:
    using element_type = T;

    constexpr Ref() noexcept = default;
    constexpr Ref(std::nullptr_t) noexcept {}

    Ref(const Ref& other) noexcept
        : m_object(other.m_object)
    {
        retain();
    }

    template<typename U>
        requires std::is_convertible_v<U*, T*>
    Ref(const Ref<U>& other) noexcept
        : m_object(other.m_object)
    {
        retain();
    }

    Ref(Ref&& other) noexcept
        : m_object(std::exchange(other.m_object, nullptr))
    {
    }

    template<typename U>
        requires std::is_convertible_v<U*, T*>
    Ref(Ref<U>&& other) noexcept
        : m_object(std::exchange(other.m_object, nullptr))
    {
    }

    Ref& operator=(const Ref& other) noexcept
    {
        Ref(other).swap(*this);
        return *this;
    }

    Ref& operator=(Ref&& other) noexcept
    {
        Ref(std::move(other)).swap(*this);
        return *this;
    }

    template<typename U>
        requires std::is_convertible_v<U*, T*>
    Ref& operator=(const Ref<U>& other) noexcept
    {
        Ref(other).swap(*this);
        return *this;
    }

    template<typename U>
        requires std::is_convertible_v<U*, T*>
    Ref& operator=(Ref<U>&& other) noexcept
    {
        Ref(std::move(other)).swap(*this);
        return *this;
    }

    Ref& operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    ~Ref() { release(); }

    // Takes a new reference to an object another owner keeps alive, e.g. a pointer recorded while a Ref was in flight.
    [[nodiscard]] static Ref from_raw(T* object) noexcept
    {
        Ref reference(object);
        reference.retain();
        return reference;
    }

    void reset() noexcept
    {
        release();
        m_object = nullptr;
    }

    void swap(Ref& other) noexcept { std::swap(m_object, other.m_object); }

    [[nodiscard]] T* get() const noexcept { return m_object; }
    T& operator*() const noexcept { return *m_object; }
    T* operator->() const noexcept { return m_object; }
    explicit operator bool() const noexcept { return m_object != nullptr; }

    friend bool operator==(const Ref& reference, std::nullptr_t) noexcept { return reference.m_object == nullptr; }
    template<typename U>
    friend bool operator==(const Ref& a, const Ref<U>& b) noexcept { return a.get() == b.get(); }

private:
    explicit Ref(T* adopted) noexcept
        : m_object(adopted)
    {
    }

    void retain() noexcept
    {
        if (m_object != nullptr)
        {
            m_object->add_ref();
        }
    }

    void release() noexcept
    {
        if (m_object != nullptr)
        {
            m_object->release();
        }
    }

    template<typename>
    friend class Ref;
    template<typename U, typename... Args>
    friend Ref<U> allocate_ref(IAllocator& allocator, Args&&... args);

    T* m_object = nullptr;
};

template<typename T, typename... Args>
[[nodiscard]] Ref<T> allocate_ref(IAllocator& allocator, Args&&... args)
{
    using Object = std::remove_cv_t<T>;
    static_assert(std::is_base_of_v<RefCounted, Object>, "allocate_ref requires T to derive from RefCounted");
    void* block = allocator.allocate(sizeof(Object), alignof(Object));
    Object* object = nullptr;
    try
    {
        object = new (block) Object(std::forward<Args>(args)...);
    }
    catch (...)
    {
        allocator.deallocate(block, sizeof(Object), alignof(Object));
        throw;
    }

    RefCounted& base = *object;
    static_assert(sizeof(Object) <= std::numeric_limits<uint32_t>::max(), "RefCounted objects are limited to 4 GiB");
    OX_ASSERT(static_cast<void*>(&base) == block, "RefCounted must be the first base so the allocation can be freed through it");
    base.m_allocator = &allocator;
    base.m_size = static_cast<uint32_t>(sizeof(Object));
    base.m_alignment = static_cast<uint32_t>(alignof(Object));
    return Ref<T>(object);
}

template<typename T, typename... Args>
[[nodiscard]] Ref<T> make_ref(Args&&... args)
{
    return allocate_ref<T>(detail::object_allocator(), std::forward<Args>(args)...);
}

} // namespace oryx
