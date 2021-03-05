#include "utility/uppermatrix.h"

#include <algorithm>

namespace MuonPi {

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
}

void detector_pairs::increase_count(std::size_t hash_1, std::size_t hash_2)
{
    auto it_1 = std::find(std::begin(m_detectors), std::end(m_detectors), hash_1);
    if (it_1 == m_detectors.end()) {
        return;
    }
    auto it_2 = std::find(std::begin(m_detectors), std::end(m_detectors), hash_2);
    if (it_2 == m_detectors.end()) {
        return;
    }
    m_data.at(std::distance(m_detectors.begin(), it_1), std::distance(m_detectors.begin(), it_2)) += 1;
}

auto detector_pairs::get_counts(std::size_t hash) -> std::unordered_map<std::size_t, std::size_t>
{
    auto it = std::find(std::begin(m_detectors), std::end(m_detectors), hash);
    if (it == m_detectors.end()) {
        return {};
    }
    std::size_t index {static_cast<std::size_t>(std::distance(m_detectors.begin(), it))};

    std::unordered_map<std::size_t, std::size_t> counts {};
    for (std::size_t i { 0 }; i < m_detectors.size(); i++) {
        auto detector {m_detectors.at(i)};
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
