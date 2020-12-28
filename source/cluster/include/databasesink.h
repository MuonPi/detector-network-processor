#ifndef DATABASESINK_H
#define DATABASESINK_H

#include "abstractsink.h"

#include "databaselink.h"
#include "utility.h"
#include "log.h"
#include "clusterlog.h"
#include "detectorsummary.h"
#include "event.h"

#include <sstream>
#include <memory>

namespace MuonPi {

template <class T>
/**
 * @brief The DatabaseLogSink class
 */
class DatabaseSink : public AbstractSink<T>
{
public:
	/**
     * @brief DatabaseLogSink
	 * @param link a DatabaseLink instance
     */
    DatabaseSink(DatabaseLink& link);

protected:
	/**
     * @brief step implementation from ThreadRunner
     * @return zero if the step succeeded.
     */
    [[nodiscard]] auto step() -> int override;

private:
	void process(T log);
	DatabaseLink& m_link;
};

// +++++++++++++++++++++++++++++++
// implementation part starts here
// +++++++++++++++++++++++++++++++

template <class T>
DatabaseSink<T>::DatabaseSink(DatabaseLink& link)
    : m_link { link }
{
    AbstractSink<T>::start();
}

template <class T>
auto DatabaseSink<T>::step() -> int
{
    if (AbstractSink<T>::has_items()) {
        process(AbstractSink<T>::next_item());
    }
    std::this_thread::sleep_for(std::chrono::microseconds{50});
    return 0;
}

template <>
void DatabaseSink<ClusterLog>::process(ClusterLog log)
{
    auto nanosecondsUTC { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
    auto fields { std::move(m_link.measurement("cluster_summary")
            <<Influx::Tag{"cluster_id", m_link.cluster_id }
            <<Influx::Field{"timeout", log.data().timeout}
            <<Influx::Field{"frequency_in", log.data().frequency.single_in}
            <<Influx::Field{"frequency_l1_out", log.data().frequency.l1_out}
            <<Influx::Field{"buffer_length", log.data().buffer_length}
            <<Influx::Field{"total_detectors", log.data().total_detectors}
            <<Influx::Field{"reliable_detectors", log.data().reliable_detectors}
            <<Influx::Field{"max_multiplicity", log.data().maximum_n}
            <<Influx::Field{"incoming", log.data().incoming}
            )};

    for (auto& [level, n]: log.data().outgoing) {
		if (level == 1) {
			continue;
		}
		entry.fields().push_back(std::make_pair("outgoing"+ std::to_string(level), static_cast<long int>(n)));
    }

	if (!m_link.write_entry(entry)) {
		Log::error()<<"Could not write event to database.";
		return;
	}
}

template <>
void DatabaseSink<DetectorSummary>::process(DetectorSummary log)
{
    auto nanosecondsUTC { std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
    auto result { std::move(m_link.measurement("detector_summary")
            <<Influx::Tag{"cluster_id", m_link.cluster_id }
            <<Influx::Tag{"user", log.user_info().username}
            <<Influx::Tag{"detector", log.user_info().station_id}
            <<Influx::Tag{"site_id", log.user_info().site_id()}
            <<Influx::Field{"eventrate", log.data().mean_eventrate}
            <<Influx::Field{"time_acc", log.data().mean_time_acc}
            <<Influx::Field{"pulselength", log.data().mean_pulselength}
            <<Influx::Field{"incoming", log.data().incoming}
            <<Influx::Field{"ublox_counter_progress", log.data().ublox_counter_progress}
            <<Influx::Field{"deadtime_factor", log.data().deadtime}
            <<nanosecondsUTC
            )};

    if (!result) {
        Log::error()<<"Could not write event to database.";
        return;
    }
}

template <>
void DatabaseSink<Event>::process(Event event)
{
    if (event.n() == 1) {
        // by default, don't write the single events to the db
        return;
    }

    const std::int64_t cluster_coinc_time = event.end() - event.start();
    GUID guid{event.hash(), static_cast<std::uint64_t>(event.start())};
    for (auto& evt: event.events()) {
        bool result = m_link.measurement("L1Event")
                <<Influx::Tag{"user", evt.data().user}
                <<Influx::Tag{"detector", evt.data().station_id}
                <<Influx::Tag{"site_id", evt.data().user + evt.data().station_id}
                <<Influx::Field{"accuracy", evt.data().time_acc}
                <<Influx::Field{"uuid", guid.to_string()}
                <<Influx::Field{"coinc_level", event.n()}
                <<Influx::Field{"counter", evt.data().ublox_counter}
                <<Influx::Field{"length", evt.duration()}
                <<Influx::Field{"coinc_time", evt.start() - event.start()}
                <<Influx::Field{"cluster_coinc_time", cluster_coinc_time}
                <<Influx::Field{"time_ref", evt.data().gnss_time_grid}
                <<Influx::Field{"valid_fix", evt.data().fix}
                <<evt.start();


        if (!result) {
            Log::error()<<"Could not write event to database.";
            return;
        }
    }
}

} // namespace MuonPi
#endif // DATABASESINK_H
