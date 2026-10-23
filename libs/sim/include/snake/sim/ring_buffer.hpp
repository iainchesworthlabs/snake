// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <optional>

namespace snake::sim
{

/// A queue with a fixed capacity that can grow and shrink at both ends, with the storage inside the object.
/// Nothing is allocated. Index 0 is the front. The snake keeps its head at the front and its tail at the back.
///
/// The access functions (front, back, operator[]) require an element to exist: check size() first. The functions
/// that add an element return false and change nothing when the buffer is full.
template <typename T, std::size_t Capacity>
class RingBuffer
{
    static_assert(Capacity > 0, "a ring buffer needs room for at least one element");

public:
    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return m_Size;
    }

    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return m_Size == 0;
    }

    [[nodiscard]] constexpr bool full() const noexcept
    {
        return m_Size == Capacity;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept
    {
        return Capacity;
    }

    constexpr bool push_front(const T& value) noexcept
    {
        if (full())
        {
            return false;
        }
        m_Front = (m_Front + Capacity - 1) % Capacity;
        m_Items[m_Front] = value;
        ++m_Size;
        return true;
    }

    constexpr bool push_back(const T& value) noexcept
    {
        if (full())
        {
            return false;
        }
        m_Items[(m_Front + m_Size) % Capacity] = value;
        ++m_Size;
        return true;
    }

    /// Removes the front element. Returns false when the buffer is empty.
    constexpr bool pop_front() noexcept
    {
        if (empty())
        {
            return false;
        }
        m_Front = (m_Front + 1) % Capacity;
        --m_Size;
        return true;
    }

    /// Removes and returns the front element, or returns nothing when the buffer is empty.
    [[nodiscard]] constexpr std::optional<T> take_front() noexcept
    {
        if (empty())
        {
            return std::nullopt;
        }
        const T value = m_Items[m_Front];
        m_Front = (m_Front + 1) % Capacity;
        --m_Size;
        return value;
    }

    /// Removes the back element. Returns false when the buffer is empty.
    constexpr bool pop_back() noexcept
    {
        if (empty())
        {
            return false;
        }
        --m_Size;
        return true;
    }

    constexpr void clear() noexcept
    {
        m_Front = 0;
        m_Size = 0;
    }

    [[nodiscard]] constexpr const T& front() const noexcept
    {
        return m_Items[m_Front];
    }

    [[nodiscard]] constexpr const T& back() const noexcept
    {
        return m_Items[(m_Front + m_Size - 1) % Capacity];
    }

    /// The element `position` places behind the front. The position must be below size().
    [[nodiscard]] constexpr const T& operator[](std::size_t position) const noexcept
    {
        return m_Items[(m_Front + position) % Capacity];
    }

private:
    std::array<T, Capacity> m_Items{};
    std::size_t m_Front = 0;
    std::size_t m_Size = 0;
};

} // namespace snake::sim
