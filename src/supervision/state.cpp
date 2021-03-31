#include "supervision/state.h"

#include "defaults.h"
#include "utility/log.h"

#include <sstream>

namespace muonpi {

state_supervisor::state_supervisor(sink::base<ClusterLog>& log_sink)
    : source::base<ClusterLog> { log_sink }
{
}

void state_supervisor::time_status(std::chrono::milliseconds timebase, std::chrono::milliseconds timeout)
{
    m_timebase = timebase;
    m_timeout = timeout;
}

void state_supervisor::detector_status(std::size_t hash, Detector::Status status)
{
    m_detectors[hash] = status;
    if (status == Detector::Status::Deleted) {
        if (m_detectors.find(hash) != m_detectors.end()) {
            m_detectors.erase(hash);
        }
    }

    std::size_t reliable { 0 };
    for (auto& [h, detector] : m_detectors) {
        if (detector == Detector::Status::Reliable) {
            reliable++;
        }
    }

    m_current_data.total_detectors = m_detectors.size();
    m_current_data.reliable_detectors = reliable;
}

auto state_supervisor::step() -> int
{
    using namespace std::chrono;

    for (auto& thread : m_threads) {
        if (thread->state() <= thread_runner::State::Stopped) {
            Log::warning() << "The thread " + thread->name() + ": " + thread->state_string();
            return -1;
        }
    }
    steady_clock::time_point now { steady_clock::now() };
    if ((std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() % 2) == 0) {
        auto data = m_resource_tracker.get_data();
        m_current_data.memory_usage = data.memory_usage;
        m_process_cpu_load.add(data.process_cpu_load);
        m_current_data.process_cpu_load = m_process_cpu_load.mean();
        m_system_cpu_load.add(data.system_cpu_load);
        m_current_data.system_cpu_load = m_system_cpu_load.mean();
    }
    if ((now - m_last) >= Config::interval.clusterlog) {
        m_last = now;

        source::base<ClusterLog>::put(ClusterLog { m_current_data });

        m_current_data.incoming = 0;
        m_current_data.outgoing.clear();
    }

    if (m_outgoing_rate.step()) {
        m_incoming_rate.step();
        m_current_data.timeout = duration_cast<milliseconds>(m_timeout).count();
        m_current_data.timebase = duration_cast<milliseconds>(m_timebase).count();
        m_current_data.uptime = duration_cast<minutes>(system_clock::now() - m_startup).count();

        m_current_data.frequency.single_in = m_incoming_rate.mean();
        m_current_data.frequency.l1_out = m_outgoing_rate.mean();
    }
    return 0;
}

void state_supervisor::increase_event_count(bool incoming, std::size_t n)
{
    if (incoming) {
        m_current_data.incoming++;
        m_incoming_rate.increase_counter();
    } else {
        m_current_data.outgoing[n]++;

        if (m_current_data.maximum_n < n) {
            m_current_data.maximum_n = n;
        }
        if (n > 1) {
            m_outgoing_rate.increase_counter();
        }
    }
}

void state_supervisor::set_queue_size(std::size_t size)
{
    m_current_data.buffer_length = size;
}

void state_supervisor::add_thread(thread_runner* thread)
{
    m_threads.push_back(thread);
}

void state_supervisor::stop()
{
    for (auto& thread : m_threads) {
        thread->stop();
    }
}
}
