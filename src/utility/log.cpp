#include "utility/log.h"

namespace muonpi::log {

sink::sink(Level level)
    : m_level { level }
{
}

sink::~sink() = default;

auto sink::level() const -> Level
{
    return m_level;
}

auto sink::to_string(Level level) -> std::string
{
    switch (level) {
    case Level::Debug:
        return "Debug";
    case Level::Info:
        return "Info";
    case Level::Notice:
        return "Notice";
    case Level::Warning:
        return "Warning";
    case Level::Error:
        return "Error";
    case Level::Critical:
        return "Critical";
    case Level::Alert:
        return "Alert";
    case Level::Emergency:
        return "Emergency";
    }
    return {};
}

stream_sink::stream_sink(std::ostream& ostream, Level level)
    : sink { level }
    , m_ostream { ostream }
{
}

void stream_sink::get(const message_t& msg)
{
    m_ostream << to_string(msg.level) + ": " + msg.message + "\n"
              << std::flush;
}
std::shared_ptr<manager> manager::s_singleton { std::make_shared<manager>() };

void manager::add_sink(std::shared_ptr<sink> sink)
{
    m_sinks.push_back(sink);
}

syslog_sink::syslog_sink(Level level)
    : sink { level }
{
    setlogmask(LOG_UPTO(level));
    openlog(appname, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
}

syslog_sink::~syslog_sink()
{
    closelog();
}

void syslog_sink::get(const message_t& msg)
{
    syslog(level(), "%s", msg.message.c_str());
}

void manager::send(const message_t& msg)
{
    for (auto& sink : m_sinks) {
        if (sink->level() >= msg.level) {
            sink->get(msg);
        }
    }
}

auto manager::singleton() -> std::shared_ptr<manager>
{
    return s_singleton;
}

auto debug() -> manager::logger<Level::Debug>&
{
    return manager::singleton()->m_debug;
}

auto info() -> manager::logger<Level::Info>&
{
    return manager::singleton()->m_info;
}

auto notice() -> manager::logger<Level::Notice>&
{
    return manager::singleton()->m_notice;
}

auto warning() -> manager::logger<Level::Warning>&
{
    return manager::singleton()->m_warning;
}

auto error() -> manager::logger<Level::Error>&
{
    return manager::singleton()->m_error;
}

auto critical() -> manager::logger<Level::Critical>&
{
    return manager::singleton()->m_crititcal;
}

auto alert() -> manager::logger<Level::Alert>&
{
    return manager::singleton()->m_alert;
}

auto emergency() -> manager::logger<Level::Emergency>&
{
    return manager::singleton()->m_emergency;
}

}
