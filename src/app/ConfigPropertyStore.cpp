#include "ConfigPropertyStore.h"
#include "AboutPropertyStore.h"
#include <fstream>
#include <iostream>
#include <sys/stat.h>

#define CHECK(x) if ((status = x) != ER_OK) { break; }
#define CHECK_RETURN(x) if ((status = x) != ER_OK) { return status; }

using namespace ajn;
using namespace services;

ConfigPropertyStore::ConfigPropertyStore(const char* factoryConfigFile, const char* configFile) : m_IsInitialized(false)
{
    m_configFileName.assign(configFile);
    m_factoryConfigFileName.assign(factoryConfigFile);
}

void ConfigPropertyStore::Initialize()
{
    m_IsInitialized = true;
    m_PropertiesReadOnly.clear();
    m_PropertiesReadOnly.insert(m_Properties.begin(), m_Properties.end());
}

void ConfigPropertyStore::FactoryReset()
{
    std::ifstream factoryConfigFile(m_factoryConfigFileName.c_str(), std::ios::binary);
    std::ofstream configFile(m_configFileName.c_str(), std::ios::binary);

    if (factoryConfigFile && configFile) {
        configFile << factoryConfigFile.rdbuf();

        configFile.close();
        factoryConfigFile.close();
    } else {
        std::cout << "Factory reset failed" << std::endl;
    }

    m_Properties.clear();
    m_Properties.insert(m_PropertiesReadOnly.begin(), m_PropertiesReadOnly.end());
}

const qcc::String& ConfigPropertyStore::GetConfigFileName()
{
    return m_configFileName;
}

ConfigPropertyStore::~ConfigPropertyStore()
{
}

QStatus ConfigPropertyStore::ReadAll(const char* languageTag, Filter filter, ajn::MsgArg& all)
{
    if (!m_IsInitialized) {
        return ER_FAIL;
    }

    if (filter == ANNOUNCE || filter == READ) {
        return AboutPropertyStore::ReadAll(languageTag, filter, all);
    }

    if (filter != WRITE) {
        return ER_FAIL;
    }

    QStatus status = ER_OK;
    if (languageTag != NULL && languageTag[0] != 0) { // check that the language is in the supported languages;
        CHECK_RETURN(isLanguageSupported(languageTag))
    } else {
        PropertyMap::iterator it = m_Properties.find(DEFAULT_LANG);
        if (it == m_Properties.end()) {
            return ER_LANGUAGE_NOT_SUPPORTED;
        }
        CHECK_RETURN(it->second.getPropertyValue().Get("s", &languageTag))
    }

    MsgArg* argsWriteData = new MsgArg[m_Properties.size()];
    uint32_t writeArgCount = 0;
    do {
        for (PropertyMap::const_iterator it = m_Properties.begin(); it != m_Properties.end(); ++it) {
            const PropertyStoreProperty& property = it->second;

            if (!property.getIsWritable()) {
                continue;
            }

            // check that it is from the defaultLanguage or empty.
            if (!(property.getLanguage().empty() || property.getLanguage().compare(languageTag) == 0)) {
                continue;
            }

            CHECK(argsWriteData[writeArgCount].Set("{sv}", property.getPropertyName().c_str(),
                                                   new MsgArg(property.getPropertyValue())))
            argsWriteData[writeArgCount].SetOwnershipFlags(MsgArg::OwnsArgs, true);
            writeArgCount++;
        }
        CHECK(all.Set("a{sv}", writeArgCount, argsWriteData))
        all.SetOwnershipFlags(MsgArg::OwnsArgs, true);
    } while (0);

    if (status != ER_OK) {
        delete[] argsWriteData;
    }

    return status;
}

QStatus ConfigPropertyStore::Update(const char* name, const char* languageTag, const ajn::MsgArg* value)
{
    if (!m_IsInitialized) {
        return ER_FAIL;
    }

    PropertyStoreKey propertyKey = getPropertyStoreKeyFromName(name);
    if (propertyKey >= NUMBER_OF_KEYS) {
        return ER_FEATURE_NOT_AVAILABLE;
    }

    if (propertyKey == DEFAULT_LANG || propertyKey == DEVICE_NAME) {
        languageTag = NULL;
    }

    QStatus status = ER_OK;
    if (languageTag != NULL) {         // check that the language is in the supported languages;
        CHECK_RETURN(isLanguageSupported(languageTag))
    }

    //validate that the value is acceptable
    qcc::String languageString = languageTag ? languageTag : "";
    status = validateValue(propertyKey, *value, languageString);
    if (status != ER_OK) {
        std::cout << "New Value failed validation. Will not update" << std::endl;
        return status;
    }

    PropertyStoreProperty* temp = NULL;
    std::pair<PropertyMap::iterator, PropertyMap::iterator> propertiesIter = m_Properties.equal_range(propertyKey);

    for (PropertyMap::iterator it = propertiesIter.first; it != propertiesIter.second; it++) {
        const PropertyStoreProperty& property = it->second;
        if (property.getIsWritable()) {
            if ((languageTag == NULL && property.getLanguage().empty()) ||
                (languageTag != NULL && property.getLanguage().compare(languageTag) == 0)) {
                temp = new PropertyStoreProperty(property.getPropertyName(), *value, property.getIsPublic(),
                                                 property.getIsWritable(), property.getIsAnnouncable());
                if (languageTag) {
                    temp->setLanguage(languageTag);
                }
                m_Properties.erase(it);
                break;
            }
        }
    }

    if (temp == NULL) {
        return ER_INVALID_VALUE;
    }

    m_Properties.insert(PropertyPair(propertyKey, *temp));

    if (persistUpdate(temp->getPropertyName().c_str(), value->v_string.str)) {
        AboutService* aboutService = AboutServiceApi::getInstance();
        if (aboutService) {
            aboutService->Announce();
            std::cout << "Calling Announce after UpdateConfiguration" << std::endl;
        }
        delete temp;
        return ER_OK;
    } else {
        delete temp;
        return ER_INVALID_VALUE;
    }
}

QStatus ConfigPropertyStore::Delete(const char* name, const char* languageTag)
{
    if (!m_IsInitialized) {
        return ER_FAIL;
    }

    PropertyStoreKey propertyKey = getPropertyStoreKeyFromName(name);
    if (propertyKey >= NUMBER_OF_KEYS) {
        return ER_FEATURE_NOT_AVAILABLE;
    }

    if (propertyKey == DEFAULT_LANG || propertyKey == DEVICE_NAME) {
        languageTag = NULL;
    }

    QStatus status = ER_OK;
    if (languageTag != NULL) {         // check that the language is in the supported languages;
        CHECK_RETURN(isLanguageSupported(languageTag))
    }

    bool deleted = false;
    std::pair<PropertyMap::iterator, PropertyMap::iterator> propertiesIter = m_Properties.equal_range(propertyKey);

    for (PropertyMap::iterator it = propertiesIter.first; it != propertiesIter.second; it++) {
        const PropertyStoreProperty& property = it->second;
        if (property.getIsWritable()) {
            if ((languageTag == NULL && property.getLanguage().empty()) ||
                (languageTag != NULL && property.getLanguage().compare(languageTag) == 0)) {
                m_Properties.erase(it);
                // insert from backup.
                deleted = true;
                break;
            }
        }
    }

    if (!deleted) {
        if (languageTag != NULL) {
            return ER_LANGUAGE_NOT_SUPPORTED;
        } else {
            return ER_INVALID_VALUE;
        }
    }

    propertiesIter = m_PropertiesReadOnly.equal_range(propertyKey);

    for (PropertyMap::iterator it = propertiesIter.first; it != propertiesIter.second; it++) {
        const PropertyStoreProperty& property = it->second;
        if (property.getIsWritable()) {
            if ((languageTag == NULL && property.getLanguage().empty()) ||
                (languageTag != NULL && property.getLanguage().compare(languageTag) == 0)) {

                m_Properties.insert(PropertyPair(it->first, it->second));
                char* value;
                it->second.getPropertyValue().Get("s", &value);
                if (persistUpdate(it->second.getPropertyName().c_str(), value)) {
                    AboutService* aboutService = AboutServiceApi::getInstance();
                    if (aboutService) {
                        aboutService->Announce();
                        std::cout << "Calling Announce after ResetConfiguration" << std::endl;
                    }
                    return ER_OK;
                }
            }
        }
    }
    return ER_INVALID_VALUE;
}

bool ConfigPropertyStore::persistUpdate(const char* key, const char* value)
{
    std::map<std::string, std::string> data;
    data[key] = value;
    //return IniParser::UpdateFile(m_configFileName.c_str(), data);
    return true;
}

PropertyStoreKey ConfigPropertyStore::getPropertyStoreKeyFromName(qcc::String const& propertyStoreName)
{
    for (int indx = 0; indx < NUMBER_OF_KEYS; indx++) {
        if (PropertyStoreName[indx].compare(propertyStoreName) == 0) {
            return (PropertyStoreKey)indx;
        }
    }
    return NUMBER_OF_KEYS;
}
