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

namespace MuonPi::Sink {

template <class T>
/**
 * @brief The Database class
 */
class Database : public Base<T> {
public:
    /**
     * @brief DatabaseLogSink
     * @param link a Link::Database instance
     */
    Database(Link::Database& link);

    void get(T message) override;

private:
    Link::Database& m_link;
};

// +++++++++++++++++++++++++++++++
// implementation part starts here
// +++++++++++++++++++++++++++++++

template <class T>
Database<T>::Database(Link::Database& link)
    : m_link { link }
{
}

template <>
void Database<ClusterLog>::get(ClusterLog log)
{
    auto nanosecondsUTC { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
    auto fields { std::move(m_link.measurement("cluster_summary")
        << Link::Influx::Tag { "cluster_id", Config::influx.cluster_id }
        << Link::Influx::Field { "timeout", log.data().timeout }
        << Link::Influx::Field { "frequency_in", log.data().frequency.single_in }
        << Link::Influx::Field { "frequency_l1_out", log.data().frequency.l1_out }
        << Link::Influx::Field { "buffer_length", log.data().buffer_length }
        << Link::Influx::Field { "total_detectors", log.data().total_detectors }
        << Link::Influx::Field { "reliable_detectors", log.data().reliable_detectors }
        << Link::Influx::Field { "max_multiplicity", log.data().maximum_n }
        << Link::Influx::Field { "incoming", log.data().incoming }) };

    std::size_t total_n { 0 };
	
	for (auto& [level, n] : log.data().outgoing) {
        if (level == 1) {
            continue;
        }
        fields << Link::Influx::Field { "outgoing" + std::to_string(level), n };
		total_n += n;
    }

    fields << Link::Influx::Field { "outgoing", total_n };

    if (!fields.commit(nanosecondsUTC)) {
        Log::warning() << "error writing ClusterLog item to DB";
    }
}

template <>
void Database<DetectorSummary>::get(DetectorSummary log)
{
    auto nanosecondsUTC { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
    using namespace Link::Influx;
    auto result { std::move((m_link.measurement("detector_summary")
        << Tag { "cluster_id", Config::influx.cluster_id }
        << Tag { "user", log.user_info().username }
        << Tag { "detector", log.user_info().station_id }
        << Tag { "site_id", log.user_info().site_id() }
        << Field { "eventrate", log.data().mean_eventrate }
        << Field { "time_acc", log.data().mean_time_acc }
        << Field { "pulselength", log.data().mean_pulselength }
        << Field { "incoming", log.data().incoming }
        << Field { "ublox_counter_progress", log.data().ublox_counter_progress }
        << Field { "deadtime_factor", log.data().deadtime })
                                .commit(nanosecondsUTC)) };

    if (!result) {
        Log::warning() << "error writing DetectorSummary item to DB";
    }
}

template <>
void Database<Event>::get(Event event)
{
    if (event.n() == 1) {
        // by default, don't write the single events to the db
        return;
    }

    const std::int64_t cluster_coinc_time = event.end() - event.start();
    GUID guid { event.hash(), static_cast<std::uint64_t>(event.start()) };
    for (auto& evt : event.events()) {
        using namespace Link::Influx;
        if (!(m_link.measurement("L1Event")
                << Tag { "user", evt.data().user }
                << Tag { "detector", evt.data().station_id }
                << Tag { "site_id", evt.data().user + evt.data().station_id }
                << Field { "accuracy", evt.data().time_acc }
                << Field { "uuid", guid.to_string() }
                << Field { "coinc_level", event.n() }
                << Field { "counter", evt.data().ublox_counter }
                << Field { "length", evt.duration() }
                << Field { "coinc_time", evt.start() - event.start() }
                << Field { "cluster_coinc_time", cluster_coinc_time }
                << Field { "time_ref", evt.data().gnss_time_grid }
                << Field { "valid_fix", evt.data().fix })
                 .commit(evt.start())) {
            Log::warning() << "error writing L1Event item to DB";
            return;
        }
    }
}

template <>
void Database<DetectorLog>::get(DetectorLog log)
{
    auto nanosecondsUTC { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
    using namespace Link::Influx;
    auto entry { m_link.measurement("detector_log") };
    entry << Tag { "user", log.user_info().username }
          << Tag { "detector", log.user_info().station_id }
          << Tag { "site_id", log.user_info().site_id() };

    while (log.has_items()) {
        DetectorLogItem item { log.next_item() };
        entry << Field { item.name, item.value };
    }

    if (!entry.commit(nanosecondsUTC)) {
        Log::warning() << "error writing DetectorLog item to DB";
    }
}

} // namespace MuonPi
#endif // DATABASESINK_H
