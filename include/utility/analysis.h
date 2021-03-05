#ifndef ANALYSIS_H
#define ANALYSIS_H

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <numeric>

namespace MuonPi {

template <typename T>
class buffered_value
{
public:
    /**
     * @brief buffered_value
     * @param calculation The calculation to perform in order to get a new value
     * @param marker The criterium to determine whether a new value should be calculated or not. Return true for new calculation
     * @param invoke if true, calculation gets called immediatly upon construction
     */
    explicit buffered_value(std::function<T()> calculation, std::function<bool()> marker, bool invoke = false)
        : m_calculation { calculation }
        , m_marker { marker }
        , m_value {}
    {
        if (invoke) {
            m_value = m_calculation();
        }
    }

    /**
     * @brief get Get the buffered value. If the function marker returns true, a new value is calculated.
     * @return The most recent value
     */
    [[nodiscard]] inline auto get() -> T
    {
        if (m_marker()) {
            m_value = m_calculation();
        }
        return m_value;
    }

    /**
     * @brief operator() Get the buffered value. If the function marker returns true, a new value is calculated.
     * @return The most recent value
     */
    [[nodiscard]] inline auto operator()() -> T
    {
        return get();
    }

private:
    std::function<T()> m_calculation {};
    std::function<bool()> m_marker;
    T m_value {};
};

template <typename T, std::size_t N, bool Sample = false>
class data_series {
    void add(T value);
    [[nodiscard]] auto mean() const -> T;
    [[nodiscard]] auto stddev() const -> T;
    [[nodiscard]] auto variance() const -> T;
    [[nodiscard]] auto entries() const -> std::size_t;
    [[nodiscard]] auto current() const -> T;

private:
    [[nodiscard]] inline auto private_mean() const -> T;
    [[nodiscard]] inline auto private_stddev() const -> T;
    [[nodiscard]] inline auto private_variance() const -> T;

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
    bool m_mean_dirty { false };
    bool m_var_dirty { false };
    bool m_stddev_dirty { false };
    buffered_value<T> m_mean{[this]{return private_mean();}, [this]{return dirty(m_mean_dirty);}};
    buffered_value<T> m_stddev{[this]{return private_stddev();}, [this]{return dirty(m_stddev_dirty);}};
    buffered_value<T> m_variance{[this]{return private_variance();}, [this]{return dirty(m_var_dirty);}};
};

template <typename T, std::size_t N>
class histogram
{
public:
    struct bin
    {
        T lower {};
        T upper {};
        std::size_t count { 0 };
    };

    explicit histogram(T width);
    explicit histogram(T lower, T upper);

    void add(T value);


    [[nodiscard]] auto bins() const -> const std::array<bin, N>&;

private:
    T m_lower {};
    T m_upper {};
    T m_width {};
    std::array<bin, N> m_bins {};
};

template <std::size_t N, std::size_t T, bool Sample = false>
class rate_measurement : public data_series<double, N, Sample> {
public:
    /**
     * @brief increase_counter Increases the counter in the current interval
     */
    void increase_counter();

    /**
     * @brief step Called periodically
     * @return True if the timeout was reached and the rates have been determined in this step
     */
    auto step() -> bool;

private:
    std::size_t m_current_n { 0 };
    std::chrono::steady_clock::time_point m_last { std::chrono::steady_clock::now() };
};


// +++++++++++++++++++++++++++++++
// implementation part starts here
// +++++++++++++++++++++++++++++++


// +++++++++++++++++++++++++++++++
// class histogram
template <typename T, std::size_t N>
histogram<T, N>::histogram(T width)
    : m_lower { 0 }
    , m_upper { width * N }
    , m_width { width }
{
    T last { m_lower };
    for (auto& [i, bin]: m_bins) {
        bin.lower = last;
        last += m_width;
        bin.upper = last;
    }
}

template <typename T, std::size_t N>
histogram<T, N>::histogram(T lower, T upper)
    : m_lower { lower }
    , m_upper { upper }
    , m_width { (upper - lower) / N }
{
    T last { m_lower };
    for (auto& [i, bin]: m_bins) {
        bin.lower = last;
        last += m_width;
        bin.upper = last;
    }
}

template <typename T, std::size_t N>
void histogram<T, N>::add(T value)
{
    if ((value < m_lower) || (value >= m_upper)) {
        return;
    }
    const std::size_t n { std::floor((value - m_lower) / m_width) };
    m_bins[n].count++;
}

template <typename T, std::size_t N>
auto histogram<T, N>::bins() const -> const std::array<bin, N>&
{
    return m_bins;
}
// -------------------------------



// +++++++++++++++++++++++++++++++
// class data_series
template <typename T, std::size_t N, bool Sample>
auto data_series<T, N, Sample>::private_mean() const -> T
{
    const auto n {m_full?N:(std::max<double>(m_index, 1.0))};
    const auto end {m_full?m_buffer.end():m_buffer.begin() + m_index};
    const auto begin {m_buffer.begin()};

    return std::accumulate(begin, end, 0.0) / n;
}

template <typename T, std::size_t N, bool Sample>
auto data_series<T, N, Sample>::private_variance() const -> T
{
    const auto n {m_full?N:(std::max<double>(m_index, 1.0))};
    const auto end {m_full?m_buffer.end():m_buffer.begin() + m_index};
    const auto begin {m_buffer.begin()};
    const auto denominator { Sample?(n - 1.0):n};
    const auto mean { m_mean() };


    return 1.0 / (denominator) * std::inner_product(
                       begin, end, begin, 0.0, [](T const& x, T const& y) { return x + y; }, [mean](T const& x, T const& y) { return (x - mean) * (y - mean); });
}

template <typename T, std::size_t N, bool Sample>
auto data_series<T, N, Sample>::private_stddev() const -> T
{
    return std::sqrt(variance());
}

template <typename T, std::size_t N, bool Sample>
void data_series<T, N, Sample>::add(T value)
{
    if (m_full || (m_index > 0)) {
        m_index = (m_index + 1) % N;
        if (m_index == 0) {
            m_full = true;
        }
    }
    m_buffer[m_index] = value;
    m_mean_dirty = true;
    m_stddev_dirty = true;
    m_var_dirty = true;
}

template <typename T, std::size_t N, bool Sample>
auto data_series<T, N, Sample>::entries() const -> std::size_t
{
    return ((m_full) ? N : m_index);
}

template <typename T, std::size_t N, bool Sample>
auto data_series<T, N, Sample>::mean() const -> T
{
    return m_mean.get();
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
// -------------------------------


// +++++++++++++++++++++++++++++++
// class rate_measurement
template <std::size_t N, std::size_t T, bool Sample>
void rate_measurement<N, T, Sample>::increase_counter()
{
    m_current_n = m_current_n + 1;
}

template <std::size_t N, std::size_t T, bool Sample>
auto rate_measurement<N, T, Sample>::step() -> bool
{
    std::chrono::steady_clock::time_point now { std::chrono::steady_clock::now() };
    if (static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last).count()) >= T) {
        m_last = now;
        data_series<double, N, Sample>::add(static_cast<double>(m_current_n) * 1000.0 / static_cast<double>(T));
        m_current_n = 0;
        return true;
    }
    return false;
}
// -------------------------------


}
#endif // ANALYSIS_H
