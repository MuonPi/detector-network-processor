#ifndef STATION_COINCIDENCE_H
#define STATION_COINCIDENCE_H

#include "messages/event.h"
#include "messages/trigger.h"

#include "sink/base.h"

#include "analysis/uppermatrix.h"
#include "analysis/histogram.h"

namespace muonpi::supervision {

class station_coincidence : public sink::base<event_t>, public sink::base<trigger::detector>
{
public:
    void get(event_t event);
    void get(trigger::detector trig);

private:
    constexpr static std::size_t s_bins { 200 };
    constexpr static std::int32_t s_outer_limit { 200 };

    std::vector<std::size_t> m_detectors {};
    upper_matrix<histogram<s_bins, std::int32_t, std::int16_t, -s_outer_limit, s_outer_limit>> m_data { 0 };
};

}

#endif // STATION_COINCIDENCE_H
