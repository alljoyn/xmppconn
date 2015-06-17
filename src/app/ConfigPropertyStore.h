#ifndef CONFIGPROPERTYSTORE_H_
#define CONFIGPROPERTYSTORE_H_

#include <stdio.h>
#include <iostream>

#include <alljoyn/about/AboutServiceApi.h>
#include <alljoyn/about/AboutPropertyStoreImpl.h>

class ConfigPropertyStore : public ajn::services::AboutPropertyStoreImpl {

  public:
    ConfigPropertyStore(const char* factoryConfigFile, const char* configFile);
    void FactoryReset();
    const qcc::String& GetConfigFileName();
    virtual ~ConfigPropertyStore();
    virtual QStatus ReadAll(const char* languageTag, Filter filter, ajn::MsgArg& all);
    virtual QStatus Update(const char* name, const char* languageTag, const ajn::MsgArg* value);
    virtual QStatus Delete(const char* name, const char* languageTag);
    void Initialize();
  private:
    ajn::services::PropertyMap m_PropertiesReadOnly;
    bool m_IsInitialized;
    qcc::String m_configFileName;
    qcc::String m_factoryConfigFileName;
    bool persistUpdate(const char* key, const char* value);
    ajn::services::PropertyStoreKey getPropertyStoreKeyFromName(qcc::String const& propertyStoreName);
};

#endif
