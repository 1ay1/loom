#pragma once

#include <string>
#include <optional>

namespace loom
{
    template<typename T, typename Tag>
    class StrongType
    {
    public:
        explicit StrongType(T value) : value_(std::move(value)) {}

        T get() const { return value_; }

        bool operator==(const StrongType& other) const { return value_ == other.value_; }
        bool operator!=(const StrongType& other) const { return value_ != other.value_; }

    private:
        T value_;
    };
}
