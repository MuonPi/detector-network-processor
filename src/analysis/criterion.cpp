#include "analysis/criterion.h"

#include <vector>

namespace muonpi {

auto criterion::apply(const event_t& first, const event_t& second) const -> score_t
{
    if ((first.n() < 2) && (second.n() < 2)) {
        if (compare(first.data, second.data) > 0.0) {
            return score_t{Type::Valid, 1};
        }
        return score_t{Type::Invalid};
    }

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
    std::size_t n { 0 };
    std::size_t valid { 0 };

    for (const auto& data_f : first_data) {
        for (const auto& data_s : second_data) {
            const double v = compare(data_f, data_s);
            sum += v;
            n++;
            if (v > 0.0) {
                valid++;
            }
        }
    }

    sum /= static_cast<double>(n);

    if (sum < s_maximum_false) {
        return score_t{Type::Invalid};
    }

    if ((sum > s_minimum_true) && (n == valid)) {
        return score_t{Type::Valid, valid};
    }
    return score_t{Type::Conflicting, valid};
}

} // namespace muonpi
