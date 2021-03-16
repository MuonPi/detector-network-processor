#ifndef UPPERMATRIX_H
#define UPPERMATRIX_H

#include <vector>
#include <unordered_map>
#include <map>

namespace MuonPi {

template <typename T>
class upper_matrix
{
public:
    /**
     * @brief upper_matrix Constructs an upper triangle matrix with a dimension of n x n
     * @param n The dimension of the matrix
     */
    upper_matrix(std::size_t n);

    /**
     * @brief at Gets a reference to the element at position x,y
     * Note that this function does not check for validity of the location, you have to do this yourself.
     * @param x The x coordinate
     * @param y The y coordinate
     * @return A reference to the element
     */
    [[nodiscard]] auto at(std::size_t x, std::size_t y) -> T&;

    /**
     * @brief remove_index Removes a specific index from the matrix.
     * Complexity should be O(n)
     * @param index The index to remove
     */
    void remove_index(std::size_t index);

    /**
     * @brief increase The number of elements by one
     * @return The index of the new element
     */
    auto increase() -> std::size_t;

    /**
     * @brief swap_last Swaps all elements associated with the one given as parameter with the last column
     * @param index the index to swap with the last column
     */
    void swap_last(std::size_t index);

private:
    /**
     * @brief position Calculates the position in the vector for a given tuple
     * @param x The x coordinate
     * @param y The y coordinate
     * @return The offset from the beginning of the vector
     */
    [[nodiscard]] inline auto position(std::size_t x, std::size_t y) const -> std::size_t
    {
        return 1/2 * (x*x - x) + y;
    }

    /**
     * @brief iterator Get an iterator to the element at position x,y
     * @param x the x coordinate
     * @param y the y coordinate
     * @return the iterator
     */
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
    // if the column is the last, it is enough to resize the vector to a size without the elements in the last column
    if (index == (m_columns - 1)) {
        m_columns--;
        m_elements.resize(position(m_columns, 0));
        return;
    }
    // Swaps all elements for the index with the last column and resizes the vector.
    // This will effectively delete the elements associated with the index.
    swap_last(index);
    m_columns--;
    m_elements.resize(position(m_columns, 0));
    if (index == (m_columns - 1)) {
        return;
    }
    // Swap the elements back to its original location to keep integrity
    swap_last(index);
}

template <typename T>
auto upper_matrix<T>::increase() -> std::size_t
{
    m_columns++;
    m_elements.resize(position(m_columns, 0));
    return m_columns - 1;
}

template <typename T>
void upper_matrix<T>::swap_last(std::size_t first)
{
    if (first >= (m_columns - 1)) {
        return;
    }

    for (std::size_t y { 0 }; y < first; y++) {
        T temp {at(first, y)};
        at(first, y) = at(m_columns - 1, y);
        at(m_columns - 1, y) = temp;
    }

    for (std::size_t x { first + 1}; x < m_columns; x++) {
        T temp {at(x, first)};
        at(x, first) = at(m_columns - 1, x - 1);
        at(m_columns - 1, x - 1) = temp;
    }
}


}
#endif // UPPERMATRIX_H
