#include "analysis/uppermatrix.h"

#include <algorithm>
#include <cinttypes>

namespace muonpi {

void detector_pairs::add_detector(std::size_t hash)
{
    m_data.increase();
    m_detectors.emplace_back(hash);
}

void detector_pairs::remove_detector(std::size_t hash)
{
    auto it = std::find(std::begin(m_detectors), std::end(m_detectors), hash);
    if (it == m_detectors.end()) {
        return;
    }
    m_data.remove_index(std::distance(m_detectors.begin(), it));
    m_detectors.erase(it);
}

void detector_pairs::increase_count(std::size_t hash_1, std::size_t hash_2)
{
    std::size_t first { 0 };
    std::size_t second { 0 };
    std::uint8_t found { 0x3 };
    for (std::size_t i { 0 }; i < m_detectors.size(); i++) {
        if (m_detectors[i] == hash_1) {
            first = i;
            found &= ~0x1U;
        } else if (m_detectors[i] == hash_2) {
            second = i;
            found &= ~0x2U;
        }
        if (found == 0) {
            break;
        }
    }
    if (found > 0) {
        return;
    }
    m_data.at(first, second) += 1;
}

auto detector_pairs::get_counts(std::size_t hash) -> std::unordered_map<std::size_t, std::size_t>
{
    auto it = std::find(std::begin(m_detectors), std::end(m_detectors), hash);
    if (it == m_detectors.end()) {
        return {};
    }
    std::size_t index { static_cast<std::size_t>(std::distance(m_detectors.begin(), it)) };

    std::unordered_map<std::size_t, std::size_t> counts {};
    for (std::size_t i { 0 }; i < m_detectors.size(); i++) {
        auto detector { m_detectors.at(i) };
        if (detector == hash) {
            continue;
        }
        if (i < index) {
            counts.emplace(detector, m_data.at(index, i));
        } else {
            counts.emplace(detector, m_data.at(i, index));
        }
    }
    return counts;
}

}
