#ifndef COINCIDENCE_H
#define COINCIDENCE_H

#include "criterion.h"

#include <chrono>
#include <memory>

namespace muonpi {

/**
 * @brief The Coincidence class
 * Defines the parameters for a coincidence between two events
 */
class coincidence : public criterion {
public:
    ~coincidence() override;
    /**
     * @brief criterion Assigns a value of type T to a pair of events
     * @param first The first event to check
     * @param second the second event to check
     * @return true if the events have a coincidence
     */
    [[nodiscard]] auto apply(const event_t& first, const event_t& second) const -> double override;

    /**
     * @brief maximum_false
     * @return The upper limit where the criterion is false.
     */
    [[nodiscard]] auto maximum_false() const -> double override
    {
        return -3.5;
    }

    /**
     * @brief minimum_true
     * @return The lower limit where the criterion is true.
     */
    [[nodiscard]] auto minimum_true() const -> double override
    {
        return 3.5;
    }

private:
    /**
     * @brief compare Compare two timestamps to each other
     * @param difference difference between both timestamps
     * @return returns a value indicating the coincidence time between the two timestamps. @see maximum_false @see minimum_true for the limits of the values.
     */
    [[nodiscard]] auto compare(std::int_fast64_t t1, std::int_fast64_t t2) const -> double;

    std::int_fast64_t m_time { 100000 };
};

}

#endif // COINCIDENCE_H
