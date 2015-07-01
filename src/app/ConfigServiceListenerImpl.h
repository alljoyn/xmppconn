#ifndef CONFIGSERVICELISTENERIMPL_H_
#define CONFIGSERVICELISTENERIMPL_H_

#include "ConfigDataStore.h"
#include "app/XMPPConnector.h"
#include "common/CommonBusListener.h"

#include <alljoyn/config/ConfigService.h>

class ConfigServiceListenerImpl : public ajn::services::ConfigService::Listener {
    public:
        ConfigServiceListenerImpl(ConfigDataStore& store, ajn::BusAttachment& bus, CommonBusListener* busListener, void(*func)());
        virtual QStatus Restart();
        virtual QStatus FactoryReset();
        virtual QStatus SetPassphrase(const char* daemonRealm, size_t passcodeSize, const char* passcode, ajn::SessionId sessionId);
        virtual ~ConfigServiceListenerImpl();
    private:
        ConfigDataStore* m_ConfigDataStore;
        ajn::BusAttachment* m_Bus;
        CommonBusListener* m_BusListener;
        void (*m_onRestartCallback)();
        void PersistPassword(const char* daemonRealm, const char* passcode);
};

#endif
