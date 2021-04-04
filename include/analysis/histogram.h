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
template <std::size_t N, typename T, typename C, T Min, T Max>
class histogram {
public:
    static_assert (std::is_integral<C>::value);
    static_assert (std::is_arithmetic<T>::value);
    static_assert (Max > Min);

    struct bin {
        T lower {};
        T upper {};
        std::size_t count { 0 };
    };

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
    std::array<C, N> m_bins {};
};

// +++++++++++++++++++++++++++++++
// implementation part starts here
// +++++++++++++++++++++++++++++++

template <std::size_t N, typename T, typename C, T Min, T Max>
void histogram<N, T, C, Min, Max>::add(T value)
{
    if ((value < Min) || (value > Max)) {
        return;
    }

    const std::size_t i { (value - Min)*(N - 1) / (Max - Min) };

    m_bins[i]++;
}

template <std::size_t N, typename T, typename C, T Min, T Max>
auto histogram<N, T, C, Min, Max>::bins() const -> const std::array<C, N>&
{
    return m_bins;
}

template <std::size_t N, typename T, typename C, T Min, T Max>
auto histogram<N, T, C, Min, Max>::qualified_bins() const -> std::array<bin, N>
{
    std::array<bin, N> bins;
    T last { Min };
    for (auto& [i, b] : bins) {
        b.lower = last;
        last += Max - Min;
        b.upper = last;
        b.count = m_bins[i];
    }
    return bins;
}
}
#endif // HISTOGRAM_H
