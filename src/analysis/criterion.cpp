#include "analysis/criterion.h"

#include <vector>

namespace muonpi {

auto criterion::apply(const event_t& first, const event_t& second) const -> Type
{
    std::vector<event_t::data_t> first_data {};
    std::vector<event_t::data_t> second_data {};

    if (first.n() < 2) {
        first_data.emplace_back(first.data);
    } else {
        first_data = first.events;
    }

    if (second.n() < 2) {
        second_data.emplace_back(second.data);
    } else {
        second_data = second.events;
    }

    double sum {};
    double n { 0 };

    for (const auto& data_f : first_data) {
        for (const auto& data_s : second_data) {
            sum += compare(data_f, data_s);
            n += 1.0;
        }
    }

    sum /= n;

    if (sum < s_maximum_false) {
        return Type::Invalid;
    }
    if (sum > s_minimum_true) {
        return Type::Valid;
    }
    return Type::Conflicting;
}

}

