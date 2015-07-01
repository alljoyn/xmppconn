#include "app/ConfigServiceListenerImpl.h"

#include <AboutObjApi.h>
#include <iostream>

using namespace ajn;
using namespace services;

ConfigServiceListenerImpl::ConfigServiceListenerImpl(ConfigDataStore& store,
                                                     BusAttachment& bus,
                                                     CommonBusListener* busListener,
                                                     void(*func)()) :
ConfigService::Listener(), m_ConfigDataStore(&store), m_Bus(&bus), m_BusListener(busListener), m_onRestartCallback(func)
{
}

QStatus ConfigServiceListenerImpl::Restart()
{
    m_onRestartCallback();
    return ER_OK;
}

QStatus ConfigServiceListenerImpl::FactoryReset()
{
    QStatus status = ER_OK;
    m_ConfigDataStore->FactoryReset();
    m_Bus->ClearKeyStore();

    AboutObjApi* aboutObjApi = AboutObjApi::getInstance();
    if (aboutObjApi) {
        status = aboutObjApi->Announce();
    }

    return status;
}

QStatus ConfigServiceListenerImpl::SetPassphrase(const char* daemonRealm,
                                                 size_t passcodeSize,
                                                 const char* passcode,
                                                 ajn::SessionId sessionId)
{
    qcc::String passCodeString(passcode, passcodeSize);

    PersistPassword(daemonRealm, passCodeString.c_str());

    m_Bus->ClearKeyStore();
    m_Bus->EnableConcurrentCallbacks();

    std::vector<SessionId> sessionIds = m_BusListener->getSessionIds();
    for (size_t i = 0; i < sessionIds.size(); i++) {
        if (sessionIds[i] == sessionId) {
            continue;
        }
        m_Bus->LeaveSession(sessionIds[i]);
    }
    return ER_OK;
}

ConfigServiceListenerImpl::~ConfigServiceListenerImpl()
{
}

void ConfigServiceListenerImpl::PersistPassword(const char* daemonRealm, const char* passcode)
{
    MsgArg argPasscode;
    MsgArg argDaemonrealm;
    argPasscode.Set("s", passcode);
    argDaemonrealm.Set("s", daemonRealm);
    m_ConfigDataStore->SetField("Passcode", argPasscode);
    m_ConfigDataStore->SetField("Daemonrealm", argDaemonrealm);
}
