#ifndef CRITERION_H
#define CRITERION_H

#include "messages/event.h"

namespace muonpi {

/**
 * @brief The Criterion class
 * Abstract class for a relationship between two events
 */

class criterion {
public:
    enum class Type {
        Invalid,
        Conflicting,
        Valid
    };

    struct score_t {
        Type type {};
        std::size_t true_e { 0 };

        [[nodiscard]] inline operator bool() const
        {
            return type >= Type::Conflicting;
        }
    };

    virtual ~criterion() = default;

    /**
     * @brief apply Assigns a value of type T to a pair of events
     * @param first The first event to check
     * @param second the second event to check
     * @return a value of type T corresponding to the relationship between both events
     */
    [[nodiscard]] auto apply(const event_t& first, const event_t& second) const -> score_t;

    /**
     * @brief compare Compare two timestamps to each other
     * @param difference difference between both timestamps
     * @return returns a value indicating the coincidence time between the two timestamps.
     */
    [[nodiscard]] virtual auto compare(const event_t::data_t& first, const event_t::data_t& second) const -> double = 0;

private:
    constexpr static double s_maximum_false { -0.3 };
    constexpr static double s_minimum_true { 0.5 };
};

}

#endif // CRITERION_H
