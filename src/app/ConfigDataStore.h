#ifndef CONFIG_DATA_STORE_H_
#define CONFIG_DATA_STORE_H_

#include "app/ConfigParser.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <alljoyn/config/AboutDataStoreInterface.h>
#include <alljoyn/BusAttachment.h>

class ConfigDataStore : public AboutDataStoreInterface {
  public:
    typedef void (*RestartCallback)();

    ConfigDataStore(const char* factoryConfigFile, const char* configFile, const char* appId, const char* deviceId, RestartCallback func);
    void Initialize();
    void FactoryReset();
    std::string GetConfigFileName() const;
    virtual ~ConfigDataStore();
    virtual QStatus ReadAll(const char* languageTag, DataPermission::Filter filter, ajn::MsgArg& all);
    virtual QStatus Update(const char* name, const char* languageTag, const ajn::MsgArg* value);
    virtual QStatus Delete(const char* name, const char* languageTag);
  private:
    bool m_IsInitialized;
    std::string m_configFileName;
    std::string m_factoryConfigFileName;
    QStatus IsLanguageSupported(const char* languageTag);
    RestartCallback m_restartCallback;
    ConfigParser* m_configParser;
    std::string m_appId;
    std::string m_deviceId;
};

#endif
