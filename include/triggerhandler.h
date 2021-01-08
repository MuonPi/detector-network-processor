#ifndef TRIGGERHANDLER_H
#define TRIGGERHANDLER_H

#include "sink/base.h"
#include "source/base.h"

#include "defaults.h"
#include "messages/trigger.h"

#include <future>
#include <restbed>

namespace restbed {
typedef std::shared_ptr<Session> session_ptr;
typedef std::function<void(const restbed::session_ptr)> callback;
}

namespace MuonPi {

class TriggerHandler : public Source::Base<Trigger::Detector::Action> {
public:
    TriggerHandler(Sink::Base<Trigger::Detector::Action>& sink, Config::Rest rest_config, Config::Ldap ldap_config);

    ~TriggerHandler() override;

private:
    void save();
    void load();

    void handle_authentication(const restbed::session_ptr session, const restbed::callback& callback);

    [[nodiscard]] auto authenticate(const std::string& user, const std::string& pw) -> bool;

    void handle_post(const restbed::session_ptr session);
    void handle_get(const restbed::session_ptr session);
    void handle_delete(const restbed::session_ptr session);

    std::shared_ptr<restbed::Resource> m_resource { std::make_shared<restbed::Resource>() };
    std::shared_ptr<restbed::SSLSettings> m_ssl_settings { std::make_shared<restbed::SSLSettings>() };
    std::shared_ptr<restbed::Settings> m_settings { std::make_shared<restbed::Settings>() };

    restbed::Service m_service {};

    std::map<std::size_t, Trigger::Detector::Setting> m_detector_trigger {};

    std::future<void> m_future;

    Config::Rest m_rest { Config::rest };
    Config::Ldap m_ldap { Config::ldap };
};

}

#endif // TRIGGERHANDLER_H
