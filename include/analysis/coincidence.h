#ifndef COINCIDENCE_H
#define COINCIDENCE_H

#include "analysis/criterion.h"
#include "utility/units.h"

namespace muonpi {

/**
 * @brief The Coincidence class
 * Defines the parameters for a coincidence between two events
 */
class coincidence : public criterion {
public:
    ~coincidence() override;
    /**
     * @brief compare Compare two timestamps to each other
     * @param difference difference between both timestamps
     * @return returns a value indicating the coincidence time between the two timestamps.
     */
    [[nodiscard]] auto compare(const event_t::data_t& first, const event_t::data_t& second) const -> double override;

private:
    constexpr static double s_maximum_distance { 62.31836734693877 * units::kilometer };
    constexpr static double s_maximum_time { s_maximum_distance / consts::c_0 };
    constexpr static double s_minimum_time { 150.0 * units::nanosecond };
};

}

#endif // COINCIDENCE_H
