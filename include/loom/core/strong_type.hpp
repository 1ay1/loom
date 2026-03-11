#pragma once

#include <algorithm>
#include <utility>

namespace loom
{
    template<typename T, typename Phantom>
    class StrongType
    {
        public:
        constexpr explicit StrongType(T value):
            value_(std::move(value)) {}

        constexpr const T& get() const noexcept
        {
            return value_;
        }

        private:
        T value_;
    };
}
