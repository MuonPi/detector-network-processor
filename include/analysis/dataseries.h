#ifndef DATASERIES_H
#define DATASERIES_H

#include "analysis/cachedvalue.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <numeric>

namespace muonpi {

/**
 * @brief The data_series class
 * @param T The type of data points to process
 * @param N The maximum number of datapoints to store
 * @param Sample whether this should behave like a sample or a complete dataset (true for sample)
 */
template <typename T, std::size_t N, bool Sample = false>
class data_series {
    static_assert(std::is_arithmetic<T>::value);
    static_assert(N > 2);

public:
    /**
     * @brief add Adds a value to the data series
     * @param value The value to add
     */
    void add(T value);

    enum class mean_t {
        arithmetic,
        geometric,
        harmonic
    };

    /**
     * @brief mean Calculates the mean of all values. This value gets cached between data entries.
     * @return The mean
     */
    [[nodiscard]] auto mean(const mean_t& type = mean_t::arithmetic) const -> T;

    /**
     * @brief median Calculates the median of all values. This value gets cached between data entries.
     * @return The median
     */
    [[nodiscard]] auto median() const -> T;

    /**
     * @brief mean Calculates the standard deviation of all values. This value gets cached between data entries.
     * @return The standard deviation
     */
    [[nodiscard]] auto stddev() const -> T;

    /**
     * @brief mean Calculates the variance of all values. This value gets cached between data entries.
     * Depending on the template parameter given with Sample, this calculates the variance of a sample
     * @return The variance
     */
    [[nodiscard]] auto variance() const -> T;

    /**
     * @brief entries Get the number of entries entered into this data series
     * @return Number of entries
     */
    [[nodiscard]] auto entries() const -> std::size_t;

    /**
     * @brief current Gets the most recent value
     * @return The most recent entry
     */
    [[nodiscard]] auto current() const -> T;

private:
    [[nodiscard]] inline auto private_mean(const mean_t& type) const -> T
    {
        const auto n { m_full ? N : (std::max<double>(m_index, 1.0)) };
        const auto end { m_full ? m_buffer.end() : m_buffer.begin() + m_index };
        const auto begin { m_buffer.begin() };

        if (type == mean_t::geometric) {
            return std::pow(std::accumulate(begin, end, 0.0, std::multiplies<T>()), 1.0/static_cast<T>(n));
        } else if (type == mean_t::harmonic) {
            return static_cast<T>(n) / std::accumulate(begin, end, 0.0, [](const T& lhs, const T& rhs) {return lhs + 1.0/rhs;});
        }
        return std::accumulate(begin, end, 0.0) / n;
    }

    [[nodiscard]] inline auto private_median() const -> T
    {
        std::array<T, N> sorted { m_buffer };

        std::sort(sorted.begin(), sorted.end());

        if (N % 2 == 0) {
            return (sorted.at( N / 2 ) + sorted.at( N / 2 + 1)) / 2.0;
        }
        return sorted.at( N / 2 );
    }

    [[nodiscard]] inline auto private_stddev() const -> T
    {
        return std::sqrt(variance());
    }

    [[nodiscard]] inline auto private_variance() const -> T
    {
        const auto n { m_full ? N : (std::max<double>(m_index, 1.0)) };
        const auto end { m_full ? m_buffer.end() : m_buffer.begin() + m_index };
        const auto begin { m_buffer.begin() };
        const auto denominator { Sample ? (n - 1.0) : n };
        const auto m { mean() };

        return 1.0 / (denominator)*std::inner_product(
                   begin, end, begin, 0.0, [](T const& x, T const& y) { return x + y; }, [m](T const& x, T const& y) { return (x - m) * (y - m); });
    }

    [[nodiscard]] inline auto dirty(bool& var) -> bool
    {
        if (var) {
            var = false;
            return true;
        }
        return false;
    }

    std::array<T, N> m_buffer { T {} };
    std::size_t m_index { 0 };
    bool m_full { false };
    cached_value<T> m_geometric_mean { [this] { return private_mean(mean_t::geometric); }};
    cached_value<T> m_arithmetic_mean { [this] { return private_mean(mean_t::arithmetic); }};
    cached_value<T> m_harmonic_mean { [this] { return private_mean(mean_t::harmonic); }};
    cached_value<T> m_median { [this] { return private_median(); }};
    cached_value<T> m_stddev { [this] { return private_stddev(); }};
    cached_value<T> m_variance { [this] { return private_variance(); }};
};

// +++++++++++++++++++++++++++++++
// implementation part starts here
// +++++++++++++++++++++++++++++++

template <typename T, std::size_t N, bool Sample>
void data_series<T, N, Sample>::add(T value)
{
    m_buffer[m_index] = value;
    m_arithmetic_mean.mark_dirty();
    m_geometric_mean.mark_dirty();
    m_harmonic_mean.mark_dirty();
    m_median.mark_dirty();
    m_stddev.mark_dirty();
    m_variance.mark_dirty();
    m_index = (m_index + 1) % N;
    if (m_index == 0) {
        m_full = true;
    }
}

template <typename T, std::size_t N, bool Sample>
auto data_series<T, N, Sample>::entries() const -> std::size_t
{
    return ((m_full) ? N : m_index);
}

template <typename T, std::size_t N, bool Sample>
auto data_series<T, N, Sample>::mean(const mean_t& type) const -> T
{
    if (type == mean_t::geometric) {
        return m_geometric_mean.get();
    } else if (type == mean_t::harmonic) {
        return m_harmonic_mean.get();
    }
    return m_arithmetic_mean.get();
}

template <typename T, std::size_t N, bool Sample>
auto data_series<T, N, Sample>::median() const -> T
{
    return m_median.get();
}

template <typename T, std::size_t N, bool Sample>
auto data_series<T, N, Sample>::stddev() const -> T
{
    return m_stddev.get();
}

template <typename T, std::size_t N, bool Sample>
auto data_series<T, N, Sample>::variance() const -> T
{
    return m_variance.get();
}

template <typename T, std::size_t N, bool Sample>
auto data_series<T, N, Sample>::current() const -> T
{
    return m_buffer[m_index];
}

}
#endif // DATASERIES_H
