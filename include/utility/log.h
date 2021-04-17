#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <memory>
#include <string>
#include <syslog.h>
#include <vector>

namespace muonpi::log {

enum Level : int {
    Debug = LOG_DEBUG,
    Info = LOG_INFO,
    Notice = LOG_NOTICE,
    Warning = LOG_WARNING,
    Error = LOG_ERR,
    Critical = LOG_CRIT,
    Alert = LOG_ALERT,
    Emergency = LOG_EMERG
};

static constexpr const char* appname { "muondetector-cluster" };

struct message_t {
    const Level level { Level::Info };
    const std::string message {};
};

class sink {
public:
    sink(Level level);

    virtual ~sink();
    [[nodiscard]] auto level() const -> Level;

protected:
    friend class manager;

    [[nodiscard]] static auto to_string(Level level) -> std::string;

    virtual void get(const message_t& msg) = 0;

private:
    Level m_level { Level::Info };
};

class stream_sink : public sink {
public:
    stream_sink(std::ostream& ostream, Level level = Level::Debug);

protected:
    void get(const message_t& msg) override;

private:
    std::ostream& m_ostream;
};

class syslog_sink : public sink {
public:
    syslog_sink(Level level = Level::Debug);

    ~syslog_sink();

protected:
    void get(const message_t& msg) override;
};

class manager {
public:
    template <Level L>
    class logger {
    public:
        logger(manager& log);

        auto operator<<(std::string message) -> logger<L>&;

    private:
        manager& m_log;
    };

    void add_sink(const std::shared_ptr<sink>& sink);

    [[nodiscard]] static auto singleton() -> std::shared_ptr<manager>;

    logger<Level::Debug> m_debug { *this };
    logger<Level::Info> m_info { *this };
    logger<Level::Notice> m_notice { *this };
    logger<Level::Warning> m_warning { *this };
    logger<Level::Error> m_error { *this };
    logger<Level::Critical> m_crititcal { *this };
    logger<Level::Alert> m_alert { *this };
    logger<Level::Emergency> m_emergency { *this };

private:
    std::vector<std::shared_ptr<sink>> m_sinks {};

    void send(const message_t& msg);

    static std::shared_ptr<manager> s_singleton;
};

template <Level L>
manager::logger<L>::logger(manager& log)
    : m_log { log }
{
}

template <Level L>
auto manager::logger<L>::operator<<(std::string message) -> logger<L>&
{
    m_log.send(message_t { L, std::move(message) });
    return *this;
}

[[nodiscard]] auto debug() -> manager::logger<Level::Debug>&;
[[nodiscard]] auto info() -> manager::logger<Level::Info>&;
[[nodiscard]] auto notice() -> manager::logger<Level::Notice>&;
[[nodiscard]] auto warning() -> manager::logger<Level::Warning>&;
[[nodiscard]] auto error() -> manager::logger<Level::Error>&;
[[nodiscard]] auto critical() -> manager::logger<Level::Critical>&;
[[nodiscard]] auto alert() -> manager::logger<Level::Alert>&;
[[nodiscard]] auto emergency() -> manager::logger<Level::Emergency>&;

}

#endif // LOG_H
