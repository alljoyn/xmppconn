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
#include <string.h>
#include <stdio.h>
#include <vector>

using namespace ajn;
using namespace services;
using namespace std;

ConfigDataStore::ConfigDataStore(const char* factoryConfigFile, const char* configFile, const char* appId, const char* deviceId, RestartCallback func) :
    AboutDataStoreInterface(factoryConfigFile, configFile),
    m_IsInitialized(false), m_configFileName(configFile), m_factoryConfigFileName(factoryConfigFile),
    m_restartCallback(func), m_configParser(new ConfigParser(configFile)),
    m_appId(appId), m_deviceId(deviceId)
{
    SetNewFieldDetails("Server",       REQUIRED,   "s");
    SetNewFieldDetails("UserJID",      REQUIRED,   "s");
    SetNewFieldDetails("UserPassword",     REQUIRED,   "s");
    //TODO: SetNewFieldDetails("Roster",       REQUIRED,   "as");
    SetNewFieldDetails("Roster",       REQUIRED,   "s");
    SetNewFieldDetails("SerialNumber", REQUIRED,   "s");
    SetNewFieldDetails("ProductID",    REQUIRED,   "s");
    SetNewFieldDetails("Port",         EMPTY_MASK, "i");
    SetNewFieldDetails("RoomJID",         EMPTY_MASK, "s");

    SetDefaultLanguage("en");
    SetSupportedLanguage("en");
    SetDeviceName("My Device Name");
    SetDeviceId("93c06771-c725-48c2-b1ff-6a2a59d445b8");
    SetAppName("Application");
    SetManufacturer("Manufacturer");
    SetModelNumber("123456");
    SetDescription("A poetic description of this application");
    SetDateOfManufacture("2014-03-24");
    SetSoftwareVersion("0.1.2");
    SetHardwareVersion("0.0.1");
    SetSupportUrl("http://www.example.org");
}

void ConfigDataStore::Initialize()
{
    if(m_configParser->isConfigValid() == true){
        MsgArg value; 
        std::map<std::string,std::string> configMap = m_configParser->GetConfigMap();
        for(std::map<std::string,std::string>::iterator it = configMap.begin(); it != configMap.end(); ++it){
            if(strcmp(it->first.c_str(), "Port") == 0){
                value.Set("i", m_configParser->GetPort()); 
            }
            else if(strcmp(it->first.c_str(), "Roster") == 0){
                /* TODO: Use this for an array
                vector<string> roster = m_configParser->GetRoster();
                const char** tmp = new const char*[roster.size()];
                size_t index(0);
                for ( vector<string>::const_iterator it(roster.begin());
                    roster.end() != it; ++it, ++index )
                {
                    tmp[index] = it->c_str();
                }
                value.Set("as", roster.size(), tmp);
                delete[] tmp;
                */
                /////////////// TEMPORARY
                vector<string> roster = m_configParser->GetRoster();
                string firstvalue = roster.empty() ? string() : roster.front();
                value.Set("s", firstvalue.c_str());
                /////////////// END TEMPORARY
            }
            else if(strcmp(it->first.c_str(), "Password") == 0){ 
                value.Set("s", "******");
            }
            else {
                value.Set("s", it->second.c_str());
            }
            SetField(it->first.c_str(), value);
        }

        SetAppId(m_appId.c_str());
        SetDeviceId(m_deviceId.c_str());
        m_IsInitialized = true;
    }
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
    m_restartCallback();
}

ConfigDataStore::~ConfigDataStore()
{
    delete m_configParser;
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
    int32_t intVal = 0;
    if(strcmp(name, "Port") == 0){
        status = value->Get("i", &intVal);
        if (status == ER_OK) {
            MsgArg value;
            m_configParser->SetPort(intVal);
            value.Set("i", &intVal);
            SetField(name, value);

            AboutServiceApi* aboutObjApi = AboutServiceApi::getInstance();
            if (aboutObjApi) {
                status = aboutObjApi->Announce();
            }
        }
    }
    else if(strcmp(name, "Roster") == 0){
        /* TODO: This will need to be an array
           int32_t numItems = 0;
           status = value->Get("as", &numItems, NULL);
           char** tmpArray = new char*[numItems];
           status = value->Get("as", &numItems, tmpArray);
           vector<string> roster;
           for ( int32_t index = 0; index < numItems; ++index )
           {
           roster.push_back(tmpArray[index]);
           }
           if(status == ER_OK){
           MsgArg value;
           m_configParser->SetRoster( roster );
           value.Set("as", numItems, tmpArray);
           SetField(name, value, "en");
           }
           delete[] tmpArray;
           */
        /////////////// TEMPORARY
        status = value->Get("s", &chval);
        if (status == ER_OK) {
            // Update the config file
            vector<string> roster;
            roster.push_back(chval);
            m_configParser->SetRoster( roster );

            // Update our About service
            MsgArg value;
            value.Set("s", chval);
            SetField(name, value);
        }
        /////////////// END TEMPORARY

        // Re-announce
        AboutServiceApi* aboutObjApi = AboutServiceApi::getInstance();
        if (aboutObjApi) {
            status = aboutObjApi->Announce();
        }
    }
    else if(strcmp(name, "Password") == 0){
        status = value->Get("s", &chval);
        if (status == ER_OK) {
            m_configParser->SetField(name, chval);
        }
    }
    else{
        status = value->Get("s", &chval);
        if (status == ER_OK) {
            MsgArg value;

            m_configParser->SetField(name, chval);
            value.Set("s", chval);
            SetField(name, value);

            AboutServiceApi* aboutObjApi = AboutServiceApi::getInstance();
            if (aboutObjApi) {
                status = aboutObjApi->Announce();
            }
        }
    }

    std::ifstream configFile(m_factoryConfigFileName.c_str(), std::ios::binary);
    if (configFile) {
        std::string str((std::istreambuf_iterator<char>(configFile)),
                std::istreambuf_iterator<char>());
    }
    m_restartCallback();
    return status;
}

QStatus ConfigDataStore::Delete(const char* name, const char* languageTag)
{
    ConfigParser* factoryParser = new ConfigParser(m_factoryConfigFileName.c_str());
    QStatus status = ER_INVALID_VALUE;

    std::ifstream configFile(m_factoryConfigFileName.c_str(), std::ios::binary);

    char* chval = NULL;
    MsgArg* value = new MsgArg;
    status = GetField(name, value, languageTag);
    if (status == ER_OK) {
        std::string tmp = factoryParser->GetField(name);
        status = value->Set("s", tmp.c_str());
        status = SetField(name, *value, languageTag);

        m_configParser->SetField(name, tmp.c_str());

        AboutServiceApi* aboutObjApi = AboutServiceApi::getInstance();
        if (aboutObjApi) {
            status = aboutObjApi->Announce();
        }
    }
    m_restartCallback();
    return status;
}

std::string ConfigDataStore::GetConfigFileName() const
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
