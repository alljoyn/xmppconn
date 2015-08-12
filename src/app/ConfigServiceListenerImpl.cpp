/** 
 * Copyright (c) 2015, Affinegy, Inc.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any 
 * purpose with or without fee is hereby granted, provided that the above 
 * copyright notice and this permission notice appear in all copies. 
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES 
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY 
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES 
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION 
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */ 

#include "app/ConfigServiceListenerImpl.h"
#include "app/SrpKeyXListener.h"
#include "app/ConfigParser.h"
#include "common/xmppconnutil.h"

#include <AboutObjApi.h>
#include <iostream>

using namespace ajn;
using namespace services;

ConfigServiceListenerImpl::ConfigServiceListenerImpl(
        ConfigDataStore& store,
        BusAttachment& bus,
        CommonBusListener* busListener,
        void(*func)(),
        const std::string& configFilePath
        ) :
        m_ConfigDataStore(&store),
        m_BusListener(busListener),
        m_KeyListener(new SrpKeyXListener()),
        m_Bus(&bus),
        m_ConfigFilePath(configFilePath),
        m_onRestartCallback(func),
        ConfigService::Listener()
{
    ConfigParser parser(m_ConfigFilePath);
    std::string passcode = parser.GetField("AllJoynPasscode");
    if ( passcode.empty() && parser.GetErrors().size() > 0 )
    {
        // Assume the error is that the passcode hasn't been set
        passcode = "000000";
        parser.SetField("AllJoynPasscode", passcode.c_str());
    }

    m_KeyListener->setPassCode(passcode.c_str());
    QStatus status = m_Bus->EnablePeerSecurity("ALLJOYN_PIN_KEYX ALLJOYN_SRP_KEYX ALLJOYN_ECDHE_PSK", dynamic_cast<AuthListener*>(m_KeyListener)); 
    if ( ER_OK != status ){
        LOG_RELEASE("Failed to enable AllJoyn Peer Security! Reason: %s", QCC_StatusText(status));
    }
}

QStatus ConfigServiceListenerImpl::Restart()
{
    m_onRestartCallback();
    return ER_OK;
}

QStatus ConfigServiceListenerImpl::FactoryReset()
{

    LOG_VERBOSE("FactoryRest has been called");
    QStatus status = ER_OK;
    m_ConfigDataStore->FactoryReset();
    m_Bus->ClearKeyStore();

    AboutObjApi* aboutObjApi = AboutObjApi::getInstance();
    if (aboutObjApi){
        status = aboutObjApi->Announce();
    }

    return status;
}

QStatus ConfigServiceListenerImpl::SetPassphrase(
        const char* daemonRealm,
        size_t passcodeSize,
        const char* passcode,
        ajn::SessionId sessionId)
{
    qcc::String passCodeString(passcode, passcodeSize);
    PersistPassword(daemonRealm, passCodeString.c_str());

    // TODO: What exactly is the purpose of the rest of this function?
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
    delete m_KeyListener;
}

void ConfigServiceListenerImpl::PersistPassword(const char* /*daemonRealm*/, const char* passcode)
{
    ConfigParser parser(m_ConfigFilePath);
    parser.SetField("AllJoynPasscode", passcode);
    m_KeyListener->setPassCode(passcode);
}
