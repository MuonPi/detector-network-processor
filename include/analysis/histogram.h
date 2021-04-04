#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <numeric>

namespace muonpi {

/**
 * @brief The histogram class
 * @param N the number of bins to use
 * @param T The type of each datapoints
 * @param C The type of the counter variable
 */
template <std::size_t N, typename T = double, typename C = std::size_t>
class histogram {
public:
    static_assert (std::is_integral<C>::value);
    static_assert (std::is_arithmetic<T>::value);

    struct bin {
        T lower {};
        T upper {};
        std::size_t count { 0 };
    };

    /**
     * @brief histogram Create a histogram with a fixed bin width. Note that the lower bound in this case will be assumed as 0.
     * @param width The width of each bin
     */
    explicit histogram(T width);

    /**
     * @brief histogram Create a histogram between two values.
     * @param lower The lower bound of the histogram
     * @param upper The upper bound
     */
    explicit histogram(T lower, T upper);

    /**
     * @brief add Adds a value to the histogram.
     * The value is deemed inside the histogram interval when it is >= lower and < upper.
     * If the value is exactly on the bound between two bins, the upper one is chosen.
     * @param value The value to add.
     */
    void add(T value);

    /**
     * @brief bins Get all bins
     * @return a const ref to the std::array cointaining the bins
     */
    [[nodiscard]] auto bins() const -> const std::array<C, N>&;

    /**
     * @brief bins Get all bins
     * @return a const ref to the std::array cointaining the bins
     */
    [[nodiscard]] auto qualified_bins() const -> std::array<bin, N>;

private:
    T m_lower {};
    T m_upper {};
    T m_width {};
    std::array<C, N> m_bins {};
};

// +++++++++++++++++++++++++++++++
// implementation part starts here
// +++++++++++++++++++++++++++++++

template <std::size_t N, typename T, typename C>
histogram<N, T, C>::histogram(T width)
    : m_lower { 0 }
    , m_upper { width * N }
    , m_width { width }
{
}

template <std::size_t N, typename T, typename C>
histogram<N, T, C>::histogram(T lower, T upper)
    : m_lower { lower }
    , m_upper { upper }
    , m_width { (upper - lower) / N }
{
}

template <std::size_t N, typename T, typename C>
void histogram<N, T, C>::add(T value)
{
    if ((value < m_lower) || (value >= m_upper)) {
        return;
    }

    const std::size_t i { std::floor((value - m_lower) / m_width) };

    m_bins[i]++;
}

template <std::size_t N, typename T, typename C>
auto histogram<N, T, C>::bins() const -> const std::array<C, N>&
{
    return m_bins;
}

template <std::size_t N, typename T, typename C>
auto histogram<N, T, C>::qualified_bins() const -> std::array<bin, N>
{
    std::array<bin, N> bins;
    T last { m_lower };
    for (auto& [i, b] : bins) {
        b.lower = last;
        last += m_width;
        b.upper = last;
        b.count = m_bins[i];
    }
    return bins;
}
}
#endif // HISTOGRAM_H
