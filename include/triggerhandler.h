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

/**
 * @brief The TriggerHandler class
 */
class TriggerHandler : public Source::Base<Trigger::Detector::Action> {
public:
    /**
     * @brief TriggerHandler
     * @param sink The sink which should receive trigger actions
     * @param rest_config The REST configuration to use
     * @param ldap_config The LDAP configuration to use
     */
    TriggerHandler(Sink::Base<Trigger::Detector::Action>& sink, Config::Rest rest_config, Config::Ldap ldap_config);

    ~TriggerHandler() override;

private:
    /**
     * @brief save Saves the current triggers to a file
     */
    void save();
    /**
     * @brief load Loads the previoulsy saved triggers from a file
     */
    void load();

    /**
     * @brief handle_authentication Handles the authentication for a REST request
     * @param session the REST session
     * @param callback The function to call on successful authenitcation
     */
    void handle_authentication(const restbed::session_ptr session, const restbed::callback& callback);

    /**
     * @brief authenticate Authentication method to authenticate against LDAP
     * @param user The username
     * @param pw The user password
     * @return True if the user is successfully authenticated
     */
    [[nodiscard]] auto authenticate(const std::string& user, const std::string& pw) -> bool;

    /**
     * @brief handle_post Handles a post request
     * @param session The http session
     */
    void handle_post(const restbed::session_ptr session);

    /**
     * @brief handle_get Handles a get request
     * @param session The http session
     */
    void handle_get(const restbed::session_ptr session);

    /**
     * @brief handle_delete Handles a delete request
     * @param session The http session
     */
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
