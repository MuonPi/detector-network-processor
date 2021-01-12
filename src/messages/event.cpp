#include "messages/event.h"
#include "utility/log.h"

namespace MuonPi {

Event::Event(std::size_t hash, Data data) noexcept
    : m_hash { hash }
    , m_data { std::move(data) }
{
}

Event::Event(Event event, bool /*foreign*/) noexcept
    : Event { event.hash(), event.data() }
{
    m_data.end = m_data.start;

    m_events.push_back(std::move(event));
}

Event::Event() noexcept
    : m_valid { false }
{
}

Event::~Event() noexcept = default;

auto Event::start() const noexcept -> std::int_fast64_t
{
    return m_data.start;
}

auto Event::duration() const noexcept -> std::int_fast64_t
{
    return m_data.end - m_data.start;
}

auto Event::end() const noexcept -> std::int_fast64_t
{
    return m_data.end;
}

auto Event::hash() const noexcept -> std::size_t
{
    return m_hash;
}

auto Event::n() const noexcept -> std::size_t
{
    return m_n;
}

void Event::add_event(Event event) noexcept
{
    if (event.n() > 1) {
        for (auto& e : event.events()) {
            add_event(e);
        }
        return;
    }

    if (event.start() < start()) {
        m_data.start = event.start();
    } else if (event.start() > end()) {
        m_data.end = event.start();
    }

    m_events.push_back(std::move(event));
    m_n++;
}

auto Event::events() const -> const std::vector<Event>&
{
    return m_events;
}

auto Event::valid() const -> bool
{
    return m_valid;
}
auto Event::data() const -> Data
{
    return m_data;
}

void Event::set_data(const Data& data)
{
    m_data = data;
}

void Event::set_detector_info(Location location, UserInfo user)
{
    m_location = location;
    m_user_info = user;
}

auto Event::location() const -> Location
{
    return m_location;
}
/*
auto Event::time_info() const -> Time
{
    return m_time_info;
}
*/
auto Event::user_info() const -> UserInfo
{
    return m_user_info;
}

}
