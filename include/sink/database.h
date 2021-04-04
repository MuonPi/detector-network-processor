#ifndef DATABASESINK_H
#define DATABASESINK_H

#include "sink/base.h"

#include "link/database.h"

#include "utility/log.h"
#include "utility/utility.h"

#include "messages/clusterlog.h"
#include "messages/detectorlog.h"
#include "messages/detectorsummary.h"
#include "messages/event.h"

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
    database(link::database& link);

    void get(T message) override;

private:
    link::database& m_link;
};

// +++++++++++++++++++++++++++++++
// implementation part starts here
// +++++++++++++++++++++++++++++++

template <class T>
database<T>::database(link::database& link)
    : m_link { link }
{
}

template <>
void database<cluster_log_t>::get(cluster_log_t log)
{
    using namespace link::influx;
    auto nanosecondsUTC { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
    auto fields { std::move(m_link.measurement("cluster_summary")
        << tag { "cluster_id", Config::influx.cluster_id }
        << field { "timeout", log.timeout }
        << field { "timebase", log.timebase }
        << field { "uptime", log.uptime }
        << field { "frequency_in", log.frequency.single_in }
        << field { "frequency_l1_out", log.frequency.l1_out }
        << field { "buffer_length", log.buffer_length }
        << field { "total_detectors", log.total_detectors }
        << field { "reliable_detectors", log.reliable_detectors }
        << field { "max_multiplicity", log.maximum_n }
        << field { "cpu_load", log.system_cpu_load }
        << field { "process_cpu_load", log.process_cpu_load }
        << field { "memory_usage", log.memory_usage }
        << field { "incoming", log.incoming }) };

    std::size_t total_n { 0 };

    for (auto& [level, n] : log.outgoing) {
        if (level == 1) {
            continue;
        }
        fields << field { "outgoing" + std::to_string(level), n };
        total_n += n;
    }

    fields << field { "outgoing", total_n };

    if (!fields.commit(nanosecondsUTC)) {
        log::warning() << "error writing cluster_log_t item to DB";
    }
}

template <>
void database<detector_summary_t>::get(detector_summary_t log)
{
    auto nanosecondsUTC { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
    using namespace link::influx;
    auto result { std::move((m_link.measurement("detector_summary")
        << tag { "cluster_id", Config::influx.cluster_id }
        << tag { "user", log.userinfo.username }
        << tag { "detector", log.userinfo.station_id }
        << tag { "site_id", log.userinfo.site_id() }
        << field { "eventrate", log.mean_eventrate }
        << field { "eventrate_stddev", log.stddev_eventrate }
        << field { "time_acc", log.mean_time_acc }
        << field { "pulselength", log.mean_pulselength }
        << field { "incoming", log.incoming }
        << field { "ublox_counter_progress", log.ublox_counter_progress }
        << field { "deadtime_factor", log.deadtime })
                                .commit(nanosecondsUTC)) };

    if (!result) {
        log::warning() << "error writing detector summary item to DB";
    }
}

template <>
void database<event_t>::get(event_t event)
{
    if (event.n() == 1) {
        // by default, don't write the single events to the db
        return;
    }

    const std::int64_t cluster_coinc_time = event.end - event.start;
    guid uuid { event.hash, static_cast<std::uint64_t>(event.start) };
    for (auto& evt : event.events) {
        using namespace link::influx;
        if (!(m_link.measurement("L1Event")
                << tag { "user", evt.user }
                << tag { "detector", evt.station_id }
                << tag { "site_id", evt.user + evt.station_id }
                << field { "accuracy", evt.time_acc }
                << field { "uuid", uuid.to_string() }
                << field { "coinc_level", event.n() }
                << field { "counter", evt.ublox_counter }
                << field { "length", evt.duration() }
                << field { "coinc_time", evt.start - event.start }
                << field { "cluster_coinc_time", cluster_coinc_time }
                << field { "time_ref", evt.gnss_time_grid }
                << field { "valid_fix", evt.fix })
                 .commit(evt.start)) {
            log::warning() << "error writing L1event_t item to DB";
            return;
        }
    }
}

template <>
void database<detector_log_t>::get(detector_log_t log)
{
    auto nanosecondsUTC { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
    using namespace link::influx;
    auto entry { m_link.measurement("detector_log") };
    entry << tag { "user", log.userinfo.username }
          << tag { "detector", log.userinfo.station_id }
          << tag { "site_id", log.userinfo.site_id() };

    while (!log.items.empty()) {
        detector_log_t::item item { log.get() };
        entry << field { item.name, item.value };
    }

    if (!entry.commit(nanosecondsUTC)) {
        log::warning() << "error writing DetectorLog item to DB";
    }
}

} // namespace muonpi
#endif // DATABASESINK_H
