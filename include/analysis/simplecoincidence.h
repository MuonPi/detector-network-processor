#ifndef SIMPLECOINCIDENCE_H
#define SIMPLECOINCIDENCE_H

#include "analysis/criterion.h"

#include <chrono>
#include <memory>

namespace muonpi {

/**
 * @brief The Coincidence class
 * Defines the parameters for a coincidence between two events
 */
class simple_coincidence : public criterion {
public:
    ~simple_coincidence() override;

    /**
     * @brief compare Compare two timestamps to each other
     * @param difference difference between both timestamps
     * @return returns a value indicating the coincidence time between the two timestamps. @see maximum_false @see minimum_true for the limits of the values.
     */
    [[nodiscard]] auto compare(const event_t::data_t& first, const event_t::data_t& second) const -> double override;

private:
    std::int_fast64_t m_time { 100000 };
};

}

#endif // SIMPLECOINCIDENCE_H
