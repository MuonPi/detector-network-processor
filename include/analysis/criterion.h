#ifndef CRITERION_H
#define CRITERION_H

#include <memory>

namespace muonpi {

struct event_t;

/**
 * @brief The Criterion class
 * Abstract class for a relationship between two events
 */
class criterion {
public:
    virtual ~criterion() = default;
    /**
     * @brief apply Assigns a value of type T to a pair of events
     * @param first The first event to check
     * @param second the second event to check
     * @return a value of type T corresponding to the relationship between both events
     */
    [[nodiscard]] virtual auto apply(const event_t& first, const event_t& second) const -> double = 0;

    /**
     * @brief maximum_false
     * @return The upper limit where the criterion is false.
     */
    [[nodiscard]] virtual auto maximum_false() const -> double = 0;

    /**
     * @brief minimum_true
     * @return The lower limit where the criterion is true.
     */
    [[nodiscard]] virtual auto minimum_true() const -> double = 0;
};

}

#endif // CRITERION_H
