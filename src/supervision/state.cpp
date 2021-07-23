#include "supervision/state.h"

#include "defaults.h"

#include <muonpi/log.h>

#include <sstream>

namespace muonpi::supervision {

state::state(sink::base<cluster_log_t>& log_sink, configuration config)
    : thread_runner { "muon::state" }
    , source::base<cluster_log_t> { log_sink }
    , m_config { std::move(config) }
{
}

void state::time_status(std::chrono::milliseconds timebase, std::chrono::milliseconds timeout)
{
    m_timebase = timebase;
    m_timeout = timeout;
}

void state::on_detector_status(std::size_t hash, detector_status::status status)
{
    m_detectors[hash] = status;
    if (status == detector_status::deleted) {
        if (m_detectors.find(hash) != m_detectors.end()) {
            m_detectors.erase(hash);
        }
    }

    std::size_t reliable { 0 };
    for (auto& [h, detector] : m_detectors) {
        if (detector == detector_status::reliable) {
            reliable++;
        }
    }

    m_current_data.total_detectors = m_detectors.size();
    m_current_data.reliable_detectors = reliable;
}

auto state::step() -> int
{
    using namespace std::chrono;

    for (auto& fwd : m_threads) {
        if (fwd.runner.state() <= thread_runner::State::Stopped) {
            log::warning() << "The thread '" << fwd.runner.name() << "' stopped: " << fwd.runner.state_string();
            m_failure = true;
            stop();
            return 0;
        }
    }

    system_clock::time_point now { system_clock::now() };

    auto data = m_resource_tracker.get_data();
    m_current_data.memory_usage = data.memory_usage;
    m_process_cpu_load.add(data.process_cpu_load);
    m_current_data.process_cpu_load = m_process_cpu_load.mean();
    m_system_cpu_load.add(data.system_cpu_load);
    m_current_data.system_cpu_load = m_system_cpu_load.mean();
    m_current_data.plausibility_level = m_plausibility_level.mean();

    if ((now - m_last) >= m_config.clusterlog_interval) {
        m_last = now;

        m_current_data.station_id = m_config.station_id;
        source::base<cluster_log_t>::put(m_current_data);

        m_current_data.incoming = 0;
        m_current_data.outgoing.clear();
    }

    if (m_outgoing_rate.step(now)) {
        m_incoming_rate.step(now);
        m_current_data.timeout = duration_cast<milliseconds>(m_timeout).count();
        m_current_data.timebase = duration_cast<milliseconds>(m_timebase).count();
        m_current_data.uptime = duration_cast<minutes>(now - m_startup).count();

        m_current_data.frequency.single_in = m_incoming_rate.mean();
        m_current_data.frequency.l1_out = m_outgoing_rate.mean();
    }

    std::mutex mx;
    std::unique_lock<std::mutex> lock { mx };
    m_condition.wait_for(lock, s_rate_interval);
    return 0;
}

auto state::post_run() -> int
{
    for (auto& fwd : m_threads) {
        fwd.runner.stop();
    }
    int result { 0 };
    for (auto& fwd : m_threads) {
        result += fwd.runner.wait();
    }
    return m_failure ? -1 : result;
}

void state::process_event(const event_t& event, bool incoming)
{
    if (incoming) {
        m_current_data.incoming++;
        m_incoming_rate.increase_counter();
        return;
    }
    const std::size_t n { event.n() };

    if (m_current_data.outgoing.count(n) < 1) {
        m_current_data.outgoing.emplace(n, 1);
    } else {
        m_current_data.outgoing[n] = m_current_data.outgoing.at(n) + 1;
    }

    if (m_current_data.maximum_n < n) {
        m_current_data.maximum_n = n;
    }
    if (n > 1) {
        m_outgoing_rate.increase_counter();
        m_plausibility_level.add(static_cast<float>(event.true_e) / (static_cast<float>(n * n - n) * 0.5F));
    }
}

void state::set_queue_size(std::size_t size)
{
    m_current_data.buffer_length = size;
}

void state::add_thread(thread_runner& thread)
{
    m_threads.emplace_back(forward { thread });
}
} // namespace muonpi::supervision
