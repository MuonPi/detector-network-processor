#include "supervision/station_coincidence.h"

#include <algorithm>

namespace muonpi::supervision {

void station_coincidence::get(event_t event)
{
    if (event.n() == 0) {
        return;
    }

    for (std::size_t i { 0 }; i < (event.n() - 1); i++) {
        const std::size_t first_h { event.events.at(i).hash };
        auto it_1 { std::find(m_detectors.begin(), m_detectors.end(), first_h) };
        if (it_1 == m_detectors.end()) {
            continue;
        }
        const std::size_t first { static_cast<std::size_t>(std::distance(m_detectors.begin(), it_1)) };
        const auto first_t { event.events.at(i).start };
        for (std::size_t j { i + 1 }; j < event.n(); j++) {
            const std::size_t second_h { event.events.at(j).hash };
            auto it_2 { std::find(m_detectors.begin(), m_detectors.end(), first_h) };
            if (it_2 == m_detectors.end()) {
                continue;
            }
            const std::size_t second { static_cast<std::size_t>(std::distance(m_detectors.begin(), it_2)) };
            const auto second_t { event.events.at(j).start };
            if (second_h > first_h) {
                m_data.at(std::max(first, second), std::min(first, second)).add(static_cast<std::int32_t>(first_t - second_t));
            } else {
                m_data.at(std::max(first, second), std::min(first, second)).add(static_cast<std::int32_t>(second_t - first_t));
            }
        }
    }
}

void station_coincidence::get(trigger::detector trig)
{
    if (trig.setting.type == trigger::detector::setting_t::Online) {
        m_data.increase();
        m_detectors.emplace_back(trig.hash);
    }
}

}
