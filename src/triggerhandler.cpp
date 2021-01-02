#include "triggerhandler.h"

#include "defaults.h"
#include "utility/log.h"
#include "utility/utility.h"

#include "crypto++/base64.h"
#include <sstream>
#include <ldap.h>

#include <regex>


#include <stdio.h>
#include <stdlib.h>
#include <ldap.h>
#include <sasl/sasl.h>
#include <fstream>






namespace MuonPi {

namespace Ldap {

auto my_sasl_interact(LDAP *ld, unsigned flags, void *defaults, void *in) -> int;

struct authdata {
    const char* username;
    const char* authname;
    const char* password;
};


auto my_sasl_interact(LDAP *ld, unsigned /*flags*/, void *defaults, void *in) -> int
{
    authdata *auth=static_cast<authdata*>(defaults);

    sasl_interact_t *interact = static_cast<sasl_interact_t*>(in);
    if( ld == nullptr ) return LDAP_PARAM_ERROR;

    while( interact->id != SASL_CB_LIST_END ) {

       const char* dflt = interact->defresult;

       switch( interact->id ) {
       case SASL_CB_GETREALM:
           dflt = nullptr;
           break;
       case SASL_CB_USER:
           dflt = auth->username;
           break;
       case SASL_CB_AUTHNAME:
           dflt = auth->authname;
           break;
       case SASL_CB_PASS:
           dflt = auth->password;
           break;
       default:
           MuonPi::Log::warning()<<"unknown ldap parameter" + std::to_string(interact->id);
       }
       interact->result = (dflt && *dflt) ? dflt : "";
       interact->len = strlen( static_cast<char*>(const_cast<void*>(interact->result)) );

       interact++;
    }
    return LDAP_SUCCESS;
}
}

TriggerHandler::TriggerHandler(Sink::Base<DetectorTrigger>& sink)
    : Source::Base<DetectorTrigger>{sink}
{
    m_resource->set_path("/trigger");
    m_resource->set_method_handler("POST", [this](const restbed::session_ptr session){
       handle_post(session);
    });
    m_resource->set_method_handler("GET", [this](const restbed::session_ptr session){
       handle_get(session);
    });
    m_resource->set_method_handler("DELETE", [this](const restbed::session_ptr session){
       handle_delete(session);
    });
/*
    m_ssl_settings->set_http_disabled(true);
    m_ssl_settings->set_private_key(restbed::Uri{"keyfile"});
    m_ssl_settings->set_certificate(restbed::Uri{"certificate"});
    m_ssl_settings->set_temporary_diffie_hellman(restbed::Uri{"temp"});
    m_settings->set_ssl_settings(m_ssl_settings);
*/
    m_settings->set_port( Config::rest.port );
    m_settings->set_default_header( "Connection", "close" );

    m_service.publish(m_resource);
    m_service.set_authentication_handler([this](const restbed::session_ptr session, const restbed::callback& callback){
        handle_authentication(session, callback);
    });

    load();

    m_future = std::async(std::launch::async, [this]{m_service.start(m_settings);});
}


TriggerHandler::~TriggerHandler()
{
    m_service.stop();
    m_future.wait();
    save();
}

void TriggerHandler::get(DetectorTrigger trigger)
{

    for (auto& [id, tr]: m_detector_trigger) {
        if (trigger.target != tr.target) {
            continue;
        }
        if (trigger.type == tr.type) {
            put(trigger);
        }
    }
}

auto TriggerHandler::authenticate(const std::string& user, const std::string& pw) -> bool
{
    LDAP* ldap { nullptr };
    auto code = ldap_initialize(&ldap, Config::ldap.server);
    if (code != LDAP_SUCCESS) {
        Log::warning()<<"Could not connect to ldap: " + std::string{ldap_err2string(code)};
        return false;
    }

    const int protocol { LDAP_VERSION3 };

    if( ldap_set_option(ldap, LDAP_OPT_PROTOCOL_VERSION, &protocol) != LDAP_OPT_SUCCESS ) {
        Log::warning()<<"Could not set ldap options.";
        return false;
    }

    LDAPMessage* result = nullptr;
    code = ldap_search_ext_s(ldap, "ou=users,dc=muonpi,dc=org", LDAP_SCOPE_ONELEVEL, ("(&(objectClass=inetOrgPerson)(memberof=cn=trigger,ou=groups,dc=muonpi,dc=org)(uid=" + user + "))").c_str(), nullptr, 0, nullptr, nullptr, nullptr, LDAP_NO_LIMIT, &result);

    if ( code != LDAP_SUCCESS ) {
        Log::warning()<<"Could not search in ldap: " + std::string{ldap_err2string(code)};
        return false;
    }

    if (ldap_count_entries(ldap, result) < 1) {
        Log::warning()<<"No search results.";
        return false;
    }

    std::string bind_dn { "uid=" + user + ",ou=users,dc=muonpi,dc=org" };

    berval credentials;
    credentials.bv_len = pw.size();
    credentials.bv_val = const_cast<char*>(pw.c_str());

    code = ldap_sasl_bind_s( ldap, bind_dn.c_str(),
                      nullptr, &credentials, nullptr,
                      nullptr, nullptr);

    if (code != LDAP_SUCCESS) {
        Log::warning()<<"Could not bind to ldap: " + std::string{ldap_err2string(code)} + " " + std::to_string(code);
        ldap_unbind_ext_s(ldap,nullptr,nullptr);
        return false;
    }

    ldap_unbind_ext_s(ldap, nullptr, nullptr);
    sasl_done();
    sasl_client_init( nullptr );
    return true;
}

void TriggerHandler::handle_authentication(const restbed::session_ptr session, const restbed::callback& callback)
{
    const auto request = session->get_request( );

    std::string authorisation{};

    CryptoPP::StringSource{request->get_header( "Authorization" ).substr(6), true,
                new CryptoPP::Base64Decoder{
                    new CryptoPP::StringSink{authorisation}
                }
    };

    auto delimiter = authorisation.find_first_of( ':' );
    auto username = authorisation.substr( 0, delimiter );
    auto password = authorisation.substr( delimiter + 1 );


    if (authenticate(username, password))
    {
        callback( session );
    }
    else
    {
        session->close( restbed::UNAUTHORIZED, { { "WWW-Authenticate", "Basic realm=\"MuonPi\""} } );
    }
}

void TriggerHandler::handle_post(const restbed::session_ptr session)
{
    auto request = session->get_request();


    std::size_t content_length = std::strtoul(request->get_header( "Content-Length", "" ).c_str(), nullptr, 10);

    std::string body;
    session->fetch( content_length, [&]( const restbed::session_ptr /*sess*/, const restbed::Bytes & bod )
    {
        for (auto& byte: bod) {
            body += static_cast<char>(byte);
        }
    } );

    MessageParser parser { body, ' '};

    if (parser.size() != 3) {
        return session->close(restbed::BAD_REQUEST);
    }

    std::size_t hash = std::hash<std::string>{}(parser[0] + parser[1] + parser[2]);

    if (m_detector_trigger.find(hash) != m_detector_trigger.end()) {
        return session->close(restbed::ALREADY_REPORTED);
    }

    DetectorTrigger trigger;
    trigger.target = std::hash<std::string>{}(parser[0] + parser[1]);
    trigger.username = parser[0];
    trigger.station = parser[1];
    if (parser[2] == "offline") {
        trigger.type = DetectorTrigger::Offline;
    } else if (parser[2] == "online") {
        trigger.type = DetectorTrigger::Online;
    } else if (parser[2] == "unreliable") {
        trigger.type = DetectorTrigger::Unreliable;
    } else if (parser[2] == "reliable") {
        trigger.type = DetectorTrigger::Reliable;
    } else {
        return session->close( restbed::METHOD_NOT_ALLOWED );
    }

    m_detector_trigger[hash] = trigger;

    return session->close( restbed::CREATED);
}

void TriggerHandler::handle_get(const restbed::session_ptr session)
{
    auto request = session->get_request();


    std::size_t content_length = std::strtoul(request->get_header( "Content-Length", "" ).c_str(), nullptr, 10);

    std::string body;
    session->fetch( content_length, [&]( const restbed::session_ptr /*sess*/, const restbed::Bytes & bod )
    {
        for (auto& byte: bod) {
            body += static_cast<char>(byte);
        }
    } );

    MessageParser parser { body, ' '};


    std::ostringstream stream {};
    std::size_t n { 0 };
    if (parser.size() == 1) {
        for (auto& [hash, trigger]: m_detector_trigger) {
            if (trigger.username == parser[0]) {
                n++;
                stream<<trigger.username<<' '<<trigger.station<<' ';
                switch (trigger.type) {
                case DetectorTrigger::Offline:
                    stream<<" offline";
                    break;
                case DetectorTrigger::Online:
                    stream<<" online";
                    break;
                case DetectorTrigger::Unreliable:
                    stream<<" unreliable";
                    break;
                case DetectorTrigger::Reliable:
                    stream<<" reliable";
                    break;
                }
                stream<<'\n';
            }
        }
    } else if (parser.size() == 2) {
        for (auto& [hash, trigger]: m_detector_trigger) {
            if ((trigger.username == parser[0]) && (trigger.station == parser[1])) {
                n++;
                stream<<trigger.username<<' '<<trigger.station<<' ';
                switch (trigger.type) {
                case DetectorTrigger::Offline:
                    stream<<" offline";
                    break;
                case DetectorTrigger::Online:
                    stream<<" online";
                    break;
                case DetectorTrigger::Unreliable:
                    stream<<" unreliable";
                    break;
                case DetectorTrigger::Reliable:
                    stream<<" reliable";
                    break;
                }
                stream<<'\n';
            }
        }
    }
    if (n == 0) {
        return session->close(restbed::NOT_FOUND);
    }
    std::string out { stream.str() };

    return session->close(restbed::OK, out, { { "Content-Length", std::to_string(out.length()) }});

    if (parser.size() == 3) {
        std::size_t hash = std::hash<std::string>{}(parser[0] + parser[1] + parser[2]);

        if (m_detector_trigger.find(hash) == m_detector_trigger.end()) {
            return session->close(restbed::NOT_FOUND);
        }
        return session->close(restbed::OK, body, { { "Content-Length", std::to_string(body.length()) }});
    }

    return session->close(restbed::BAD_REQUEST);
}

void TriggerHandler::handle_delete(const restbed::session_ptr session)
{
    auto request = session->get_request();


    std::size_t content_length = std::strtoul(request->get_header( "Content-Length", "" ).c_str(), nullptr, 10);

    std::string body;
    session->fetch( content_length, [&]( const restbed::session_ptr /*sess*/, const restbed::Bytes & bod )
    {
        for (auto& byte: bod) {
            body += static_cast<char>(byte);
        }
    } );


    MessageParser parser { body, ' '};

    if (parser.size() != 3) {
        return session->close(restbed::BAD_REQUEST);
    }

    std::size_t hash = std::hash<std::string>{}(parser[0] + parser[1] + parser[2]);

    if (m_detector_trigger.find(hash) == m_detector_trigger.end()) {
        return session->close(restbed::NOT_FOUND);
    }

    m_detector_trigger.erase(hash);
    return session->close(restbed::OK);
}

void TriggerHandler::save()
{
    std::ofstream out {Config::rest.save_file};
    if (!out.is_open()) {
        Log::warning()<<"Could not save trigger.";
        return;
    }
    for (auto& [hash, trigger]: m_detector_trigger) {
        out<<trigger.username<<' '<<trigger.station<<' ';
        switch (trigger.type) {
        case DetectorTrigger::Offline:
            out<<" offline";
            break;
        case DetectorTrigger::Online:
            out<<" online";
            break;
        case DetectorTrigger::Unreliable:
            out<<" unreliable";
            break;
        case DetectorTrigger::Reliable:
            out<<" reliable";
            break;
        }
        out<<'\n';
    }
    out.close();
}

void TriggerHandler::load()
{
    std::ifstream in {Config::rest.save_file};
    if (!in.is_open()) {
        Log::warning()<<"Could not load trigger.";
        return;
    }
    for (std::string line; std::getline(in, line); ) {

        MessageParser parser { line, ' '};

        if (parser.size() != 3) {
            continue;
        }

        std::size_t hash = std::hash<std::string>{}(parser[0] + parser[1] + parser[2]);

        if (m_detector_trigger.find(hash) != m_detector_trigger.end()) {
            continue;
        }

        DetectorTrigger trigger;
        trigger.target = std::hash<std::string>{}(parser[0] + parser[1]);
        trigger.username = parser[0];
        trigger.station = parser[1];
        if (parser[2] == "offline") {
            trigger.type = DetectorTrigger::Offline;
        } else if (parser[2] == "online") {
            trigger.type = DetectorTrigger::Online;
        } else if (parser[2] == "unreliable") {
            trigger.type = DetectorTrigger::Unreliable;
        } else if (parser[2] == "reliable") {
            trigger.type = DetectorTrigger::Reliable;
        }

        m_detector_trigger[hash] = trigger;
    }
}

}
