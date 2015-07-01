#include "app/ConfigDataStore.h"

#include <alljoyn/config/AboutDataStoreInterface.h>
#include <alljoyn/about/AboutServiceApi.h>
#include <alljoyn/AboutData.h>
#include <alljoyn/services_common/GuidUtil.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

using namespace ajn;
using namespace services;

ConfigDataStore::ConfigDataStore(const char* factoryConfigFile, const char* configFile) :
    AboutDataStoreInterface(factoryConfigFile, configFile), m_IsInitialized(false), configParser(new ConfigParser(configFile))
{
    m_configFileName.assign(configFile);
    m_factoryConfigFileName.assign(factoryConfigFile);

    SetNewFieldDetails("Server",       REQUIRED,   "s");
    SetNewFieldDetails("UserJID",      REQUIRED,   "s");
    SetNewFieldDetails("Password",     REQUIRED,   "s");
    SetNewFieldDetails("Roster",       REQUIRED,   "s");
    SetNewFieldDetails("SerialNumber", REQUIRED,   "s");
    SetNewFieldDetails("ProductID",    REQUIRED,   "s");
    SetNewFieldDetails("Port",         EMPTY_MASK, "i");
    SetNewFieldDetails("Room",         EMPTY_MASK, "s");


    uint8_t appId[] = { 0x01, 0xB3, 0xBA, 0x14,
                        0x1E, 0x82, 0x11, 0xE4,
                        0x86, 0x51, 0xD1, 0x56,
                        0x1D, 0x5D, 0x46, 0xB0 };
    this->SetAppId(appId, 16);
    this->SetDefaultLanguage("en");
    this->SetSupportedLanguage("en");
    this->SetDeviceName("My Device Name");
    this->SetDeviceId("93c06771-c725-48c2-b1ff-6a2a59d445b8");
    this->SetAppName("Application");
    this->SetManufacturer("Manufacturer");
    this->SetModelNumber("123456");
    this->SetDescription("A poetic description of this application");
    this->SetDateOfManufacture("2014-03-24");
    this->SetSoftwareVersion("0.1.2");
    this->SetHardwareVersion("0.0.1");
    this->SetSupportUrl("http://www.example.org");
}

void ConfigDataStore::Initialize(qcc::String deviceId, qcc::String appId)
{
    MsgArg value; 
    std::map<std::string,std::string> configMap = configParser->GetConfigMap();
    for(std::map<std::string,std::string>::iterator it = configMap.begin(); it != configMap.end(); ++it){
        if(it->first == "Port"){
            value.Set("i", atoi(it->second.c_str()));
        }
        else if(it->first == "Password"){
            value.Set("s", "******");
        }
        else {
            value.Set("s", it->second.c_str());
        }
        this->SetField(it->first.c_str(), value);
    }

    std::string deviceID = configParser->GetField("ProductID") + configParser->GetField("SerialNumber");
    this->SetDeviceId(deviceID.c_str());
    m_IsInitialized = true;
}

void ConfigDataStore::FactoryReset()
{
    m_IsInitialized = false;

    std::ifstream factoryConfigFile(m_factoryConfigFileName.c_str(), std::ios::binary);
    std::string str((std::istreambuf_iterator<char>(factoryConfigFile)),
            std::istreambuf_iterator<char>());
    factoryConfigFile.close();

    std::ofstream configFileWrite(m_configFileName.c_str(), std::ofstream::out | std::ofstream::trunc);
    configFileWrite.write(str.c_str(), str.length());
    configFileWrite.close();

    Initialize();
}

ConfigDataStore::~ConfigDataStore()
{
    delete configParser;
}

QStatus ConfigDataStore::ReadAll(const char* languageTag, DataPermission::Filter filter, ajn::MsgArg& all)
{
    QCC_UNUSED(filter);
    QStatus status = GetAboutData(&all, languageTag);
    return status;
}

QStatus ConfigDataStore::Update(const char* name, const char* languageTag, const ajn::MsgArg* value)
{
    QStatus status = ER_INVALID_VALUE;
    char* chval = NULL;
    status = value->Get("s", &chval);

    if (status == ER_OK) {
        MsgArg value;

        configParser->SetField(name, chval);
        value.Set("s", chval);
        this->SetField(name, value);

        AboutServiceApi* aboutObjApi = AboutServiceApi::getInstance();
        if (aboutObjApi) {
            status = aboutObjApi->Announce();
        }
    }
    std::ifstream configFile(m_factoryConfigFileName.c_str(), std::ios::binary);
    if (configFile) {
        std::string str((std::istreambuf_iterator<char>(configFile)),
                std::istreambuf_iterator<char>());
    }

    return status;
}

QStatus ConfigDataStore::Delete(const char* name, const char* languageTag)
{
    ConfigParser* factoryParser = new ConfigParser(m_factoryConfigFileName.c_str());
    QStatus status = ER_INVALID_VALUE;

    std::ifstream configFile(m_factoryConfigFileName.c_str(), std::ios::binary);

    char* chval = NULL;
    MsgArg* value = new MsgArg;
    status = this->GetField(name, value, languageTag);
    if (status == ER_OK) {
        std::string tmp = factoryParser->GetField(name);
        status = value->Set("s", tmp.c_str());
        status = this->SetField(name, *value, languageTag);

        configParser->SetField(name, tmp.c_str());

        AboutServiceApi* aboutObjApi = AboutServiceApi::getInstance();
        if (aboutObjApi) {
            status = aboutObjApi->Announce();
        }
    }
    return status;
}

const qcc::String& ConfigDataStore::GetConfigFileName()
{
    return m_configFileName;
}

QStatus ConfigDataStore::IsLanguageSupported(const char* languageTag)
{
    /*
     * This looks hacky. But we need this because ER_LANGUAGE_NOT_SUPPORTED was not a part of
     * AllJoyn Core in 14.06 and is defined in alljoyn/services/about/cpp/inc/alljoyn/about/PropertyStore.h
     * with a value 0xb001 whereas in 14.12 the About support was incorporated in AllJoyn Core and
     * ER_LANGUAGE_NOT_SUPPORTED is now a part of QStatus enum with a value of 0x911a and AboutData
     * returns this if a language is not supported
     */
    QStatus status = ((QStatus)0x911a);
    size_t langNum;
    langNum = GetSupportedLanguages();
    if (langNum > 0) {
        const char** langs = new const char*[langNum];
        GetSupportedLanguages(langs, langNum);
        for (size_t i = 0; i < langNum; ++i) {
            if (0 == strcmp(languageTag, langs[i])) {
                status = ER_OK;
                break;
            }
        }
        delete [] langs;
    }
    return status;
}
