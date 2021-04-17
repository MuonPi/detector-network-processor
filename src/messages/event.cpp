#include "messages/event.h"

#include <algorithm>

namespace muonpi {

auto event_t::duration() const noexcept -> std::int_fast64_t
{
    return data.duration();
}

auto event_t::n() const noexcept -> std::size_t
{
    return std::max<std::size_t>(events.size(), 1);
}

void event_t::emplace(event_t event) noexcept
{
    if (event.n() > 1) {
        for (auto d : event.events) {
            emplace(std::move(d));
        }
        return;
    }

    emplace(std::move(event.data));
}

void event_t::emplace(data_t event) noexcept
{
    if (event.start < data.start) {
        data.start = event.start;
    } else if (event.start > data.end) {
        data.end = event.start;
    }

    events.emplace_back(std::move(event));
}

}
