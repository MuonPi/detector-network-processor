#ifndef TRIGGERHANDLER_H
#define TRIGGERHANDLER_H

#include "sink/base.h"
#include "source/base.h"

#include "defaults.h"
#include "messages/trigger.h"

#include "utility/rest_service.h"

namespace muonpi {

class trigger_handler : public source::base<Trigger::Detector::Action>, public rest::service_handler {
public:
    trigger_handler(sink::base<Trigger::Detector::Action>& sink, Config::Ldap ldap_config, Config::Trigger trigger_config);

    ~trigger_handler() override;

private:
    void save();
    void load();

    [[nodiscard]] auto authenticate(std::string_view user, std::string_view pw) -> bool;

    [[nodiscard]] auto handle(rest::request request) -> rest::response_type;

    std::map<std::size_t, Trigger::Detector::Setting> m_detector_trigger {};

    Config::Ldap m_ldap { Config::ldap };
    Config::Trigger m_trigger { Config::trigger };
};

}

#endif // TRIGGERHANDLER_H
