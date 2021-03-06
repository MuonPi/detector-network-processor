#ifndef STATESUPERVISOR_H
#define STATESUPERVISOR_H

#include "analysis/detectorstation.h"
#include "messages/clusterlog.h"
#include "messages/detectorstatus.h"
#include "messages/event.h"

#include <muonpi/sink/base.h>

#include <muonpi/analysis/dataseries.h>
#include <muonpi/analysis/ratemeasurement.h>

#include <muonpi/supervision/resource.h>

#include <muonpi/source/base.h>

#include <chrono>
#include <cinttypes>
#include <fstream>
#include <map>
#include <mutex>
#include <vector>

namespace muonpi::supervision {

/**
 * @brief The state_supervisor class Supervises the program and collects metadata
 */
class state : public thread_runner, public source::base<cluster_log_t> {
public:
    struct configuration {
        std::string station_id {};
        std::chrono::steady_clock::duration clusterlog_interval;
    };
    /**
     * @brief state_supervisor
     * @param log_sink The specific log sinks to send metadata to
     */
    state(sink::base<cluster_log_t>& log_sink, configuration config);

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
    void on_detector_status(std::size_t hash, detector_status::status status);

    /**
     * @brief process_event gets called when an event arrives or gets send off
     * @param event the event which should be processed
     * @param incoming true if the event is incoming, false if it a processed one
     */
    void process_event(const event_t& event, bool incoming = false);

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

protected:
    /**
     * @brief step Gets called from the core class.
     * @return 0 if everything is okay
     */
    [[nodiscard]] auto step() -> int override;

    [[nodiscard]] auto post_run() -> int override;

private:
    std::map<std::size_t, detector_status::status> m_detectors;
    std::chrono::milliseconds m_timeout {};
    std::chrono::milliseconds m_timebase {};
    std::chrono::system_clock::time_point m_start { std::chrono::system_clock::now() };
    std::chrono::system_clock::time_point m_startup { std::chrono::system_clock::now() };

    constexpr static std::chrono::seconds s_rate_interval { 5 };

    data_series<float> m_process_cpu_load { 10 };
    data_series<float> m_system_cpu_load { 10 };
    data_series<float> m_plausibility_level { 100 };

    rate_measurement<double> m_incoming_rate { 100, s_rate_interval };
    rate_measurement<double> m_outgoing_rate { 100, s_rate_interval };

    struct forward {
        thread_runner& runner;
    };

    bool m_failure { false };

    std::vector<forward> m_threads;

    cluster_log_t m_current_data;
    std::mutex m_outgoing_mutex;
    std::chrono::system_clock::time_point m_last { std::chrono::system_clock::now() };

    resource m_resource_tracker {};

    configuration m_config {};
};

}

#endif // STATESUPERVISOR_H
