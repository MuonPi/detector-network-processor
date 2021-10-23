#ifndef TIMEBASESUPERVISOR_H
#define TIMEBASESUPERVISOR_H

#include "messages/event.h"

#include <muonpi/pipeline/base.h>

#include <chrono>
#include <memory>

namespace muonpi::supervision {

/**
 * @brief The timebase_supervisor class
 */
class timebase : public pipeline::base<event_t>, public pipeline::base<timebase_t> {
public:
    /**
     * @brief timebase
     * @param event_sink The event sink to use
     * @param timebase_sink the timebase sink to use
     */
    timebase(sink::base<event_t>& event_sink, sink::base<timebase_t>& timebase_sink);

    /**
     * @brief get Reimplemented from pipeline::base
     * @param event
     */
    void get(event_t event) override;

    /**
     * @brief get Reimplemented from pipeline::base
     * @param tb
     */
    void get(timebase_t tb) override;

private:
    static constexpr std::chrono::system_clock::duration s_minimum { std::chrono::milliseconds { 800 } };
    static constexpr std::chrono::system_clock::duration s_maximum { std::chrono::minutes { 2 } };
    static constexpr std::chrono::system_clock::duration s_sample_time { std::chrono::seconds { 2 } };

    std::chrono::system_clock::time_point m_sample_start { std::chrono::system_clock::now() };

    std::int_fast64_t m_start { std::numeric_limits<std::int_fast64_t>::max() };
    std::int_fast64_t m_end { 0 };

    std::chrono::system_clock::duration m_current { s_minimum };
};

}

#endif // TIMEBASESUPERVISOR_H
