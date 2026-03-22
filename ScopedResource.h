#pragma once

#include <utility>
#include <type_traits>

// -----------------------------------------------------------------------------

template<typename T, auto FreeFn>
class ScopedResource
{
public:
    using value_type = T;

    ScopedResource() noexcept = default;
    explicit ScopedResource(T value) noexcept : value_(value)
    {}
    ScopedResource(const ScopedResource &) = delete;
    ScopedResource(ScopedResource &&other) = delete;

    ~ScopedResource() noexcept
    {
        reset();
    }

    ScopedResource &operator=(const ScopedResource &) = delete;
    ScopedResource &operator=(ScopedResource &&other) = delete;

    T get() const noexcept
    {
        return value_;
    }

    explicit operator bool() const noexcept
    {
        return value_ != T{};
    }

    T release() noexcept
    {
        T tmp = value_;
        value_ = T{};
        return tmp;
    }

    void reset(T newValue = T{}) noexcept
    {
        if (value_ != T{})
        {
            FreeFn(value_);
        }
        value_ = newValue;
    }

private:
    T value_ = T{};
};
