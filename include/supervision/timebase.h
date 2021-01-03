#ifndef TIMEBASESUPERVISOR_H
#define TIMEBASESUPERVISOR_H

#include "pipeline.h"

#include "messages/event.h"

#include <memory>
#include <chrono>

namespace MuonPi {


/**
 * @brief The TimeBaseSupervisor class
 */
class TimeBaseSupervisor : public Pipeline<Event>, public Pipeline<TimeBase>
{
public:
    TimeBaseSupervisor(Sink::Base<Event>& event_sink, Sink::Base<TimeBase>& timebase_sink);

    void get(Event event) override;
    void get(TimeBase event) override;
private:
    static constexpr std::chrono::system_clock::duration s_minimum { std::chrono::milliseconds{800} };
    static constexpr std::chrono::system_clock::duration s_maximum { std::chrono::minutes{60} };

    std::chrono::system_clock::duration m_sample_time { std::chrono::seconds{2} };
    std::chrono::system_clock::time_point m_sample_start { std::chrono::system_clock::now() };

    std::int_fast64_t m_start { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() + 10000000000 };
    std::int_fast64_t m_end { -1000000000 };


    std::chrono::system_clock::duration m_current { s_minimum };

};

}

#endif // TIMEBASESUPERVISOR_H
