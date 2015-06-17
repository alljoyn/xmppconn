#include "ConfigServiceListener.h"
#include <IniParser.h>
#include <iostream>

using namespace ajn;
using namespace services;

ConfigServiceListener::ConfigServiceListener(PropertyStoreImpl& store, BusAttachment& bus, CommonBusListener& busListener) :
    ConfigServiceListener::Listener(), m_PropertyStore(&store), m_Bus(&bus), m_BusListener(&busListener)
{
}

QStatus ConfigServiceListener::Restart()
{
    std::cout << "Restart has been called !!!" << std::endl;
    return ER_OK;
}

QStatus ConfigServiceListener::FactoryReset()
{
    QStatus status = ER_OK;
    std::cout << "FactoryReset has been called!!!" << std::endl;
    m_PropertyStore->FactoryReset();
    std::cout << "Clearing Key Store" << std::endl;
    m_Bus->ClearKeyStore();

    AboutServiceApi* aboutService = AboutServiceApi::getInstance();
    if (aboutService) {
        status = aboutService->Announce();
        std::cout << "Announce for " << m_Bus->GetUniqueName().c_str() << " = " << QCC_StatusText(status) << std::endl;
    }

    return status;
}

QStatus ConfigServiceListener::SetPassphrase(const char* daemonRealm, size_t passcodeSize, const char* passcode, SessionId sessionId)
{
    qcc::String passCodeString(passcode, passcodeSize);
    //std::cout << "SetPassphrase has been called daemonRealm=" << daemonRealm << " passcode="
    //         << passCodeString.c_str() << " passcodeLength=" << passcodeSize << std::endl;

    PersistPassword(daemonRealm, passCodeString.c_str());

    std::cout << "Clearing Key Store" << std::endl;
    m_Bus->ClearKeyStore();
    m_Bus->EnableConcurrentCallbacks();

    std::vector<SessionId> sessionIds = m_BusListener->getSessionIds();
    for (size_t i = 0; i < sessionIds.size(); i++) {
        if (sessionIds[i] == sessionId) {
            continue;
        }
        m_Bus->LeaveSession(sessionIds[i]);
        std::cout << "Leaving session with id: " << sessionIds[i];
    }
    return ER_OK;
}

ConfigServiceListener::~ConfigServiceListener()
{
}

void ConfigServiceListener::PersistPassword(const char* daemonRealm, const char* passcode)
{
    std::map<std::string, std::string> data;
    data["daemonrealm"] = daemonRealm;
    data["passcode"] = passcode;
    IniParser::UpdateFile(m_PropertyStore->GetConfigFileName().c_str(), data);
}

