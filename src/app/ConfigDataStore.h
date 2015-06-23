#ifndef ABOUT_DATA_STORE_H_
#define ABOUT_DATA_STORE_H_

#include "app/ConfigParser.h"
#include <stdio.h>
#include <iostream>
#include <alljoyn/config/AboutDataStoreInterface.h>

class ConfigDataStore : public AboutDataStoreInterface {
  public:
    ConfigDataStore(const char* factoryConfigFile, const char* configFile);
    void FactoryReset();
    const qcc::String& GetConfigFileName();
    virtual ~ConfigDataStore();
    virtual QStatus ReadAll(const char* languageTag, DataPermission::Filter filter, ajn::MsgArg& all);
    virtual QStatus Update(const char* name, const char* languageTag, const ajn::MsgArg* value);
    virtual QStatus Delete(const char* name, const char* languageTag);
    void write();
    void Initialize(qcc::String deviceId = qcc::String(), qcc::String appId = qcc::String());
  private:
    bool m_IsInitialized;
    qcc::String m_configFileName;
    qcc::String m_factoryConfigFileName;
    QStatus IsLanguageSupported(const char* languageTag);
    ConfigParser* configParser;
};

#endif
