#ifndef STATESUPERVISOR_H
#define STATESUPERVISOR_H

#include "analysis/detectorstation.h"
#include "messages/clusterlog.h"
#include "sink/base.h"

#include "analysis/dataseries.h"
#include "analysis/ratemeasurement.h"

#include "supervision/resource.h"

#include "source/base.h"

#include <chrono>
#include <cinttypes>
#include <fstream>
#include <map>
#include <vector>

namespace muonpi::supervision {

/**
 * @brief The state_supervisor class Supervises the program and collects metadata
 */
class state : public source::base<cluster_log_t> {
public:
    /**
     * @brief state_supervisor
     * @param log_sink The specific log sinks to send metadata to
     */
    state(sink::base<cluster_log_t>& log_sink);

    /**
     * @brief time_status Update the current timeout used
     * @param timeout the timeout in ms
     */
    void time_status(std::chrono::milliseconds timebase, std::chrono::milliseconds timeout);

    /**
     * @brief detector_status Update the status of one detector
     * @param hash The hashed detector identifier
     * @param status The new status of the detector
     */
    void detector_status(std::size_t hash, detector_station::Status status);

    /**
     * @brief step Gets called from the core class.
     * @return 0 if everything is okay
     */
    [[nodiscard]] auto step() -> int;

    /**
     * @brief increase_event_count gets called when an event arrives or gets processed
     * @param incoming true if the event is incoming, false if it a processed one
     * @param n The coincidence level of the event. Only used for processed events.
     */
    void increase_event_count(bool incoming, std::size_t n = 1);

    /**
     * @brief set_queue_size Update the current event constructor buffer size.
     * @param size The current size
     */
    void set_queue_size(std::size_t size);

    /**
     * @brief add_thread Add a thread to supervise. If this thread quits or has an error state, the main event loop will stop.
     * @param thread Pointer to the thread to supervise
     */
    void add_thread(thread_runner& thread);

    /**
     * @brief stop Signals all threads to stop execution
     */
    void stop();

private:
    std::map<std::size_t, detector_station::Status> m_detectors;
    std::chrono::milliseconds m_timeout {};
    std::chrono::milliseconds m_timebase {};
    std::chrono::system_clock::time_point m_start { std::chrono::system_clock::now() };
    std::chrono::system_clock::time_point m_startup { std::chrono::system_clock::now() };

    data_series<float, 100> m_process_cpu_load {};
    data_series<float, 100> m_system_cpu_load {};
    rate_measurement<100, 5000> m_incoming_rate {};
    rate_measurement<100, 5000> m_outgoing_rate {};

    struct forward
    {
        thread_runner& runner;
    };

    std::vector<forward> m_threads;

    cluster_log_t m_current_data;
    std::chrono::system_clock::time_point m_last { std::chrono::system_clock::now() };

    resource m_resource_tracker {};
};

}

#endif // STATESUPERVISOR_H
