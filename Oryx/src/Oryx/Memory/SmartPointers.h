#pragma once

#include "Oryx/Memory/IAllocator.h"

namespace oryx
{

template<typename T>
class UniquePtr;
template<typename T>
class SharedPtr;

namespace detail
{

// Shared by UniquePtr and SharedPtr, so a UniquePtr moves into a SharedPtr without a new allocation.
struct AllocationRecord
{
    IAllocator* allocator;
    void (*destroy)(AllocationRecord*) noexcept;
    std::atomic<uint32_t> strong_count;
};

template<typename T>
struct BlockLayout
{
    static constexpr size_t alignment = std::max(alignof(AllocationRecord), alignof(T));
    static constexpr size_t offset = (sizeof(AllocationRecord) + alignof(T) - 1) / alignof(T) * alignof(T);
    static constexpr size_t size = offset + sizeof(T);
};

template<typename T>
T* object_in(AllocationRecord* record)
{
    return std::launder(reinterpret_cast<T*>(reinterpret_cast<std::byte*>(record) + BlockLayout<T>::offset));
}

template<typename T>
void destroy_block(AllocationRecord* record) noexcept
{
    IAllocator* allocator = record->allocator;
    object_in<T>(record)->~T();
    record->~AllocationRecord();
    allocator->deallocate(record, BlockLayout<T>::size, BlockLayout<T>::alignment);
}

template<typename T>
struct AdoptedRecord
{
    AllocationRecord record;
    T* object;
};

template<typename T>
void destroy_adopted(AllocationRecord* record) noexcept
{
    AdoptedRecord<T>* adopted = reinterpret_cast<AdoptedRecord<T>*>(record);
    IAllocator* allocator = record->allocator;
    delete adopted->object;
    adopted->~AdoptedRecord<T>();
    allocator->deallocate(adopted, sizeof(AdoptedRecord<T>), alignof(AdoptedRecord<T>));
}

// The allocator create_unique/create_shared use when none is given.
[[nodiscard]] IAllocator& object_allocator();

template<typename T, typename... Args>
AllocationRecord* construct_block(IAllocator& allocator, Args&&... args)
{
    using Object = std::remove_cv_t<T>;
    void* block = allocator.allocate(BlockLayout<Object>::size, BlockLayout<Object>::alignment);
    AllocationRecord* record = new (block) AllocationRecord{ &allocator, &destroy_block<Object>, 1 };
    try
    {
        new (static_cast<std::byte*>(block) + BlockLayout<Object>::offset) Object(std::forward<Args>(args)...);
    }
    catch (...)
    {
        record->~AllocationRecord();
        allocator.deallocate(block, BlockLayout<Object>::size, BlockLayout<Object>::alignment);
        throw;
    }
    return record;
}

} // namespace detail

template<typename T>
class UniquePtr
{
public:
    using element_type = T;

    constexpr UniquePtr() noexcept = default;
    constexpr UniquePtr(std::nullptr_t) noexcept {}

    UniquePtr(UniquePtr&& other) noexcept
        : m_object(std::exchange(other.m_object, nullptr))
        , m_record(std::exchange(other.m_record, nullptr))
    {
    }

    template<typename U>
        requires std::is_convertible_v<U*, T*>
    UniquePtr(UniquePtr<U>&& other) noexcept
        : m_object(std::exchange(other.m_object, nullptr))
        , m_record(std::exchange(other.m_record, nullptr))
    {
    }

    UniquePtr& operator=(UniquePtr&& other) noexcept
    {
        UniquePtr(std::move(other)).swap(*this);
        return *this;
    }

    template<typename U>
        requires std::is_convertible_v<U*, T*>
    UniquePtr& operator=(UniquePtr<U>&& other) noexcept
    {
        UniquePtr(std::move(other)).swap(*this);
        return *this;
    }

    UniquePtr& operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    ~UniquePtr() { reset(); }

    void reset() noexcept
    {
        detail::AllocationRecord* record = std::exchange(m_record, nullptr);
        m_object = nullptr;
        if (record != nullptr)
        {
            record->destroy(record);
        }
    }

    void swap(UniquePtr& other) noexcept
    {
        std::swap(m_object, other.m_object);
        std::swap(m_record, other.m_record);
    }

    [[nodiscard]] T* get() const noexcept { return m_object; }
    T& operator*() const noexcept { return *m_object; }
    T* operator->() const noexcept { return m_object; }
    explicit operator bool() const noexcept { return m_object != nullptr; }

    friend bool operator==(const UniquePtr& pointer, std::nullptr_t) noexcept { return pointer.m_object == nullptr; }

private:
    UniquePtr(T* object, detail::AllocationRecord* record) noexcept
        : m_object(object)
        , m_record(record)
    {
    }

    template<typename>
    friend class UniquePtr;
    template<typename>
    friend class SharedPtr;
    template<typename U, typename... Args>
    friend UniquePtr<U> allocate_unique(IAllocator& allocator, Args&&... args);

    T* m_object = nullptr;
    detail::AllocationRecord* m_record = nullptr;
};

template<typename T>
class SharedPtr
{
public:
    using element_type = T;

    constexpr SharedPtr() noexcept = default;
    constexpr SharedPtr(std::nullptr_t) noexcept {}

    // Only for pybind11's holder machinery, which adopts objects it made with new; Oryx code uses create_shared.
    explicit SharedPtr(T* adopted)
    {
        if (adopted == nullptr)
        {
            return;
        }
        IAllocator& allocator = detail::object_allocator();
        using Record = detail::AdoptedRecord<std::remove_cv_t<T>>;
        void* memory = allocator.allocate(sizeof(Record), alignof(Record));
        Record* record = new (memory) Record{ { &allocator, &detail::destroy_adopted<std::remove_cv_t<T>>, 1 }, const_cast<std::remove_cv_t<T>*>(adopted) };
        m_object = adopted;
        m_record = &record->record;
    }

    SharedPtr(const SharedPtr& other) noexcept
        : m_object(other.m_object)
        , m_record(other.m_record)
    {
        retain();
    }

    template<typename U>
        requires std::is_convertible_v<U*, T*>
    SharedPtr(const SharedPtr<U>& other) noexcept
        : m_object(other.m_object)
        , m_record(other.m_record)
    {
        retain();
    }

    SharedPtr(SharedPtr&& other) noexcept
        : m_object(std::exchange(other.m_object, nullptr))
        , m_record(std::exchange(other.m_record, nullptr))
    {
    }

    template<typename U>
        requires std::is_convertible_v<U*, T*>
    SharedPtr(SharedPtr<U>&& other) noexcept
        : m_object(std::exchange(other.m_object, nullptr))
        , m_record(std::exchange(other.m_record, nullptr))
    {
    }

    template<typename U>
        requires std::is_convertible_v<U*, T*>
    SharedPtr(UniquePtr<U>&& other) noexcept
        : m_object(std::exchange(other.m_object, nullptr))
        , m_record(std::exchange(other.m_record, nullptr))
    {
    }

    SharedPtr& operator=(const SharedPtr& other) noexcept
    {
        SharedPtr(other).swap(*this);
        return *this;
    }

    SharedPtr& operator=(SharedPtr&& other) noexcept
    {
        SharedPtr(std::move(other)).swap(*this);
        return *this;
    }

    template<typename U>
        requires std::is_convertible_v<U*, T*>
    SharedPtr& operator=(SharedPtr<U> other) noexcept
    {
        SharedPtr(std::move(other)).swap(*this);
        return *this;
    }

    template<typename U>
        requires std::is_convertible_v<U*, T*>
    SharedPtr& operator=(UniquePtr<U>&& other) noexcept
    {
        SharedPtr(std::move(other)).swap(*this);
        return *this;
    }

    SharedPtr& operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    ~SharedPtr() { release(); }

    void reset() noexcept
    {
        release();
        m_object = nullptr;
        m_record = nullptr;
    }

    void swap(SharedPtr& other) noexcept
    {
        std::swap(m_object, other.m_object);
        std::swap(m_record, other.m_record);
    }

    [[nodiscard]] T* get() const noexcept { return m_object; }
    template<typename U = T>
        requires(!std::is_void_v<U>)
    U& operator*() const noexcept
    {
        return *m_object;
    }
    T* operator->() const noexcept { return m_object; }
    explicit operator bool() const noexcept { return m_object != nullptr; }

    [[nodiscard]] uint32_t use_count() const noexcept { return m_record == nullptr ? 0 : m_record->strong_count.load(std::memory_order_relaxed); }

    friend bool operator==(const SharedPtr& pointer, std::nullptr_t) noexcept { return pointer.m_object == nullptr; }
    template<typename U>
    friend bool operator==(const SharedPtr& a, const SharedPtr<U>& b) noexcept { return a.get() == b.get(); }

private:
    SharedPtr(T* object, detail::AllocationRecord* record) noexcept
        : m_object(object)
        , m_record(record)
    {
    }

    void retain() noexcept
    {
        if (m_record != nullptr)
        {
            m_record->strong_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void release() noexcept
    {
        if (m_record != nullptr && m_record->strong_count.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            m_record->destroy(m_record);
        }
    }

    template<typename>
    friend class SharedPtr;
    template<typename U, typename... Args>
    friend SharedPtr<U> allocate_shared(IAllocator& allocator, Args&&... args);

    T* m_object = nullptr;
    detail::AllocationRecord* m_record = nullptr;
};

template<typename T, typename... Args>
[[nodiscard]] UniquePtr<T> allocate_unique(IAllocator& allocator, Args&&... args)
{
    detail::AllocationRecord* record = detail::construct_block<T>(allocator, std::forward<Args>(args)...);
    return UniquePtr<T>(detail::object_in<std::remove_cv_t<T>>(record), record);
}

template<typename T, typename... Args>
[[nodiscard]] SharedPtr<T> allocate_shared(IAllocator& allocator, Args&&... args)
{
    detail::AllocationRecord* record = detail::construct_block<T>(allocator, std::forward<Args>(args)...);
    return SharedPtr<T>(detail::object_in<std::remove_cv_t<T>>(record), record);
}

template<typename T, typename ... Args>
constexpr UniquePtr<T> create_unique(Args&& ... args)
{
    return allocate_unique<T>(detail::object_allocator(), std::forward<Args>(args)...);
}

template<typename T, typename ... Args>
constexpr SharedPtr<T> create_shared(Args&& ... args)
{
    return allocate_shared<T>(detail::object_allocator(), std::forward<Args>(args)...);
}

} // namespace oryx
