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
void database<ClusterLog>::get(ClusterLog log)
{
    using namespace link::influx;
    auto nanosecondsUTC { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
    auto fields { std::move(m_link.measurement("cluster_summary")
        << tag { "cluster_id", Config::influx.cluster_id }
        << field { "timeout", log.data().timeout }
        << field { "timebase", log.data().timebase }
        << field { "uptime", log.data().uptime }
        << field { "frequency_in", log.data().frequency.single_in }
        << field { "frequency_l1_out", log.data().frequency.l1_out }
        << field { "buffer_length", log.data().buffer_length }
        << field { "total_detectors", log.data().total_detectors }
        << field { "reliable_detectors", log.data().reliable_detectors }
        << field { "max_multiplicity", log.data().maximum_n }
        << field { "cpu_load", log.data().system_cpu_load }
        << field { "process_cpu_load", log.data().process_cpu_load }
        << field { "memory_usage", log.data().memory_usage }
        << field { "incoming", log.data().incoming }) };

    std::size_t total_n { 0 };

    for (auto& [level, n] : log.data().outgoing) {
        if (level == 1) {
            continue;
        }
        fields << field { "outgoing" + std::to_string(level), n };
        total_n += n;
    }

    fields << field { "outgoing", total_n };

    if (!fields.commit(nanosecondsUTC)) {
        Log::warning() << "error writing ClusterLog item to DB";
    }
}

template <>
void database<DetectorSummary>::get(DetectorSummary log)
{
    auto nanosecondsUTC { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
    using namespace link::influx;
    auto result { std::move((m_link.measurement("detector_summary")
        << tag { "cluster_id", Config::influx.cluster_id }
        << tag { "user", log.user_info().username }
        << tag { "detector", log.user_info().station_id }
        << tag { "site_id", log.user_info().site_id() }
        << field { "eventrate", log.data().mean_eventrate }
        << field { "eventrate_stddev", log.data().stddev_eventrate }
        << field { "time_acc", log.data().mean_time_acc }
        << field { "pulselength", log.data().mean_pulselength }
        << field { "incoming", log.data().incoming }
        << field { "ublox_counter_progress", log.data().ublox_counter_progress }
        << field { "deadtime_factor", log.data().deadtime })
                                .commit(nanosecondsUTC)) };

    if (!result) {
        Log::warning() << "error writing DetectorSummary item to DB";
    }
}

template <>
void database<Event>::get(Event event)
{
    if (event.n() == 1) {
        // by default, don't write the single events to the db
        return;
    }

    const std::int64_t cluster_coinc_time = event.end() - event.start();
    GUID guid { event.hash(), static_cast<std::uint64_t>(event.start()) };
    for (auto& evt : event.events()) {
        using namespace link::influx;
        if (!(m_link.measurement("L1Event")
                << tag { "user", evt.data().user }
                << tag { "detector", evt.data().station_id }
                << tag { "site_id", evt.data().user + evt.data().station_id }
                << field { "accuracy", evt.data().time_acc }
                << field { "uuid", guid.to_string() }
                << field { "coinc_level", event.n() }
                << field { "counter", evt.data().ublox_counter }
                << field { "length", evt.duration() }
                << field { "coinc_time", evt.start() - event.start() }
                << field { "cluster_coinc_time", cluster_coinc_time }
                << field { "time_ref", evt.data().gnss_time_grid }
                << field { "valid_fix", evt.data().fix })
                 .commit(evt.start())) {
            Log::warning() << "error writing L1Event item to DB";
            return;
        }
    }
}

template <>
void database<DetectorLog>::get(DetectorLog log)
{
    auto nanosecondsUTC { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
    using namespace link::influx;
    auto entry { m_link.measurement("detector_log") };
    entry << tag { "user", log.user_info().username }
          << tag { "detector", log.user_info().station_id }
          << tag { "site_id", log.user_info().site_id() };

    while (log.has_items()) {
        DetectorLogItem item { log.next_item() };
        entry << field { item.name, item.value };
    }

    if (!entry.commit(nanosecondsUTC)) {
        Log::warning() << "error writing DetectorLog item to DB";
    }
}

} // namespace muonpi
#endif // DATABASESINK_H
