#ifndef DATABASESINK_H
#define DATABASESINK_H

#include "messages/clusterlog.h"
#include "messages/detectorlog.h"
#include "messages/detectorsummary.h"
#include "messages/event.h"
#include "messages/trigger.h"

#include <muonpi/link/influx.h>
#include <muonpi/log.h>
#include <muonpi/sink/base.h>
#include <muonpi/utility.h>

#include <memory>
#include <sstream>

namespace muonpi::sink {

template <class T>
/**
 * @brief The database class
 */
class database : public base<T> {
public:
    /**
     * @brief databaseLogsink
     * @param link a link::database instance
     */
    database(link::influx& link);

    /**
     * @brief get Reimplemented from sink::base
     * @param message
     */
    void get(T message) override;

private:
    link::influx& m_link;

    using tag = link::influx::tag;
    template <typename F>
    using field = link::influx::field<F>;
};

// +++++++++++++++++++++++++++++++
// implementation part starts here
// +++++++++++++++++++++++++++++++

template <class T>
database<T>::database(link::influx& link)
    : m_link { link }
{
}

template <>
void database<cluster_log_t>::get(cluster_log_t log)
{
    const auto nanosecondsUTC { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
    auto fields { std::move(m_link.measurement("cluster_summary")
        << tag { "cluster_id", log.station_id }
        << field<std::string> { "version", Version::dnp::string() }
        << field<std::int_fast64_t> { "timeout", log.timeout }
        << field<std::int_fast64_t> { "timebase", log.timebase }
        << field<std::int_fast64_t> { "uptime", log.uptime }
        << field<double> { "frequency_in", log.frequency.single_in }
        << field<double> { "frequency_l1_out", log.frequency.l1_out }
        << field<std::size_t> { "buffer_length", log.buffer_length }
        << field<std::size_t> { "total_detectors", log.total_detectors }
        << field<std::size_t> { "reliable_detectors", log.reliable_detectors }
        << field<std::size_t> { "max_multiplicity", log.maximum_n }
        << field<float> { "cpu_load", log.system_cpu_load }
        << field<float> { "process_cpu_load", log.process_cpu_load }
        << field<float> { "memory_usage", log.memory_usage }
        << field<std::size_t> { "incoming", log.incoming }
        << field<float> { "plausibility_level", log.plausibility_level }) };

    std::size_t total_n { 0 };

    for (auto& [level, n] : log.outgoing) {
        if (level == 1) {
            continue;
        }
        fields << field<std::size_t> { "outgoing" + std::to_string(level), n };
        total_n += n;
    }

    fields << field<std::size_t> { "outgoing", total_n };

    if (!fields.commit(nanosecondsUTC)) {
        log::warning("influx") << "error writing cluster_log_t item to DB";
    }
}

template <>
void database<detector_summary_t>::get(detector_summary_t log)
{
    const auto nanosecondsUTC { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
    auto result { std::move((m_link.measurement("detector_summary")
        << tag { "user", log.userinfo.username }
        << tag { "detector", log.userinfo.station_id }
        << tag { "site_id", log.userinfo.site_id() }
        << field<double> { "eventrate", log.mean_eventrate }
        << field<double> { "eventrate_stddev", log.stddev_eventrate }
        << field<double> { "time_acc", log.mean_time_acc }
        << field<double> { "pulselength", log.mean_pulselength }
        << field<std::uint64_t> { "incoming", log.incoming }
        << field<std::int64_t> { "ublox_counter_progress", log.ublox_counter_progress }
        << field<double> { "deadtime_factor", log.deadtime })
                                .commit(nanosecondsUTC)) };

    if (!result) {
        log::warning("influx") << "error writing detector summary item to DB";
    }
}

template <>
void database<trigger::detector>::get(trigger::detector trig)
{
    const auto nanosecondsUTC { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
    auto result { std::move(m_link.measurement("trigger")
        << tag { "user", trig.userinfo.username }
        << tag { "detector", trig.userinfo.station_id }
        << tag { "site_id", trig.userinfo.site_id() }
        << field<std::string> { "type", detector_status::to_string(trig.status) }
        << field<std::string> { "reason", detector_status::to_string(trig.reason) })
                      .commit(nanosecondsUTC) };

    if (!result) {
        log::warning("influx") << "error writing trigger to DB";
    }
}

template <>
void database<event_t>::get(event_t event)
{
    if (event.n() < 2) {
        return;
    }

    const std::int64_t cluster_coinc_time = event.duration();
    guid uuid { event.data.hash, static_cast<std::uint64_t>(event.data.start) };
    double plausibility { static_cast<double>(event.true_e) / (static_cast<double>(event.n() * event.n() - event.n()) * 0.5) };
    for (auto& evt : event.events) {
        if (!(m_link.measurement("L1Event")
                << tag { "user", evt.user }
                << tag { "detector", evt.station_id }
                << tag { "site_id", evt.user + evt.station_id }
                << field<std::uint32_t> { "accuracy", evt.time_acc }
                << field<std::string> { "uuid", uuid.to_string() }
                << field<std::size_t> { "coinc_level", event.n() }
                << field<std::uint16_t> { "counter", evt.ublox_counter }
                << field<std::int_fast64_t> { "length", evt.duration() }
                << field<std::int_fast64_t> { "coinc_time", evt.start - event.data.start }
                << field<std::int_fast64_t> { "cluster_coinc_time", cluster_coinc_time }
                << field<std::uint8_t> { "time_ref", evt.gnss_time_grid }
                << field<std::uint8_t> { "valid_fix", evt.fix }
                << field<bool> { "conflicting", event.conflicting }
                << field<double> { "plausibility", plausibility })
                 .commit(evt.start)) {
            log::warning("influx") << "error writing L1event_t item to DB";
            return;
        }
    }
}

template <>
void database<detector_log_t>::get(detector_log_t log)
{
    auto nanosecondsUTC { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
    auto entry { m_link.measurement("detector_log") };
    entry << tag { "user", log.userinfo.username }
          << tag { "detector", log.userinfo.station_id }
          << tag { "site_id", log.userinfo.site_id() };

    while (!log.items.empty()) {
        detector_log_t::item item { log.get() };
        if (item.type == detector_log_t::item::Type::Double) {
            entry << field<double> { item.name, item.value_d };
        } else if (item.type == detector_log_t::item::Type::Int) {
            entry << field<int> { item.name, item.value_i };
        } else {
            entry << field<std::string> { item.name, item.value_s };
        }
    }

    if (!entry.commit(nanosecondsUTC)) {
        log::warning("influx") << "error writing DetectorLog item to DB";
    }
}

} // namespace muonpi
#endif // DATABASESINK_H
