#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <memory>
#include <string>
#include <syslog.h>
#include <vector>

namespace muonpi::Log {

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

struct Message {
    const Level level { Level::Info };
    const std::string message {};
};

class sink {
public:
    sink(Level level);

    virtual ~sink();
    [[nodiscard]] auto level() const -> Level;

protected:
    friend class Log;

    [[nodiscard]] static auto to_string(Level level) -> std::string;

    virtual void get(const Message& msg) = 0;

private:
    Level m_level { Level::Info };
};

class stream_sink : public sink {
public:
    stream_sink(std::ostream& ostream, Level level = Level::Debug);

protected:
    void get(const Message& msg) override;

private:
    std::ostream& m_ostream;
};

class syslog_sink : public sink {
public:
    syslog_sink(Level level = Level::Debug);

    ~syslog_sink();

protected:
    void get(const Message& msg) override;
};

class Log {
public:
    template <Level L>
    class Logger {
    public:
        Logger(Log& log);

        auto operator<<(std::string message) -> Logger<L>&;

    private:
        Log& m_log;
    };

    void add_sink(std::shared_ptr<sink> sink);

    [[nodiscard]] static auto singleton() -> std::shared_ptr<Log>;

    Logger<Level::Debug> m_debug { *this };
    Logger<Level::Info> m_info { *this };
    Logger<Level::Notice> m_notice { *this };
    Logger<Level::Warning> m_warning { *this };
    Logger<Level::Error> m_error { *this };
    Logger<Level::Critical> m_crititcal { *this };
    Logger<Level::Alert> m_alert { *this };
    Logger<Level::Emergency> m_emergency { *this };

private:
    std::vector<std::shared_ptr<sink>> m_sinks {};

    void send(const Message& msg);

    static std::shared_ptr<Log> s_singleton;
};

template <Level L>
Log::Logger<L>::Logger(Log& log)
    : m_log { log }
{
}

template <Level L>
auto Log::Logger<L>::operator<<(std::string message) -> Logger<L>&
{
    m_log.send(Message { L, message });
    return *this;
}

[[nodiscard]] auto debug() -> Log::Logger<Level::Debug>&;
[[nodiscard]] auto info() -> Log::Logger<Level::Info>&;
[[nodiscard]] auto notice() -> Log::Logger<Level::Notice>&;
[[nodiscard]] auto warning() -> Log::Logger<Level::Warning>&;
[[nodiscard]] auto error() -> Log::Logger<Level::Error>&;
[[nodiscard]] auto critical() -> Log::Logger<Level::Critical>&;
[[nodiscard]] auto alert() -> Log::Logger<Level::Alert>&;
[[nodiscard]] auto emergency() -> Log::Logger<Level::Emergency>&;

}

#endif // LOG_H
