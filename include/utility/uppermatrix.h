#ifndef UPPERMATRIX_H
#define UPPERMATRIX_H

#include <vector>
#include <unordered_map>

namespace MuonPi {

template <typename T>
class upper_matrix
{
public:
    upper_matrix(std::size_t n);

    [[nodiscard]] auto at(std::size_t x, std::size_t y) -> T&;
    void remove_index(std::size_t index);
    auto increase() -> std::size_t;

private:
    [[nodiscard]] auto position(std::size_t x, std::size_t y) const -> std::size_t;
    [[nodiscard]] inline auto iterator(std::size_t x, std::size_t y) const
    {
        return m_elements.begin() + position(std::move(x), std::move(y));
    }

    std::size_t m_columns;
    std::vector<T> m_elements;
};


class detector_pairs
{
public:
    void add_detector(std::size_t hash);
    void remove_detector(std::size_t hash);

    void increase_count(std::size_t hash_1, std::size_t hash_2);
    [[nodiscard]] auto get_counts(std::size_t hash) -> std::unordered_map<std::size_t, std::size_t>;

private:
    std::vector<std::size_t> m_detectors {};
    upper_matrix<std::size_t> m_data {0};
};

template <typename T>
upper_matrix<T>::upper_matrix(std::size_t n)
    : m_columns { n }
    , m_elements { std::vector<T>{position(n, 0)} }
{
}

template <typename T>
auto upper_matrix<T>::position(std::size_t x, std::size_t y) const -> std::size_t
{
    return 1/2 * (x*x - x) + y;
}

template <typename T>
auto upper_matrix<T>::at(std::size_t x, std::size_t y) -> T&
{
    return m_elements.at(position(std::move(x), std::move(y)));
}

template <typename T>
void upper_matrix<T>::remove_index(std::size_t index)
{
    if (index >= m_columns) {
        return;
    }
    std::vector<T> buffer {std::move(m_elements)};
    m_elements = std::vector<T>{m_columns - 1};
    for (std::size_t x { 1 }; x < index; x++) {
        for (std::size_t y {0}; y < x; y++) {
            m_elements.emplace(iterator(x, y), buffer.at(position(x,y)));
        }
    }
    for (std::size_t x {index + 1}; x < m_columns; x++) {
        for (std::size_t y {0}; y < index; y++) {
            m_elements.emplace(iterator(x - 1, y), buffer.at(position(x,y)));
        }
    }
    for (std::size_t x {index + 1}; x < m_columns; x++) {
        for (std::size_t y {index + 1}; y < x; y++) {
            m_elements.emplace(iterator(x - 1, y - 1), buffer.at(position(x,y)));
        }
    }
    m_columns--;
}

template <typename T>
auto upper_matrix<T>::increase() -> std::size_t
{
    m_columns++;
    m_elements.resize(position(m_columns, 0));
    return m_columns - 1;
}

}
#endif // UPPERMATRIX_H
