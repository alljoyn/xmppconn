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

#include "app/XMPPConnector.h"
#include "app/AboutObjApi.h"
#include "app/ConfigDataStore.h"
#include "app/ConfigServiceListenerImpl.h"
#include "common/xmppconnutil.h"
#include "common/CommonBusListener.h"
#include "app/ConfigParser.h"

#include <alljoyn/about/AboutPropertyStoreImpl.h>
#include <alljoyn/about/AnnouncementRegistrar.h>
#include <alljoyn/about/AboutServiceApi.h>
#include <alljoyn/services_common/GuidUtil.h>
#include <alljoyn/AboutObj.h>
#include <alljoyn/BusAttachment.h>
#include <qcc/StringUtil.h>
#include <uuid/uuid.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <csignal>
#include <string>
#include <vector>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <sys/inotify.h>

using namespace ajn;
using namespace ajn::gw;

using std::stringstream;
using std::ifstream;
using std::getline;
using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::find_if;
using std::ptr_fun;
using std::isspace;
using std::not1;
using std::istringstream;
using std::map;

static bool s_Compress = true;
static bool s_Continue = false;
static BusAttachment* s_Bus = 0;
static XMPPConnector* s_Conn = 0;
static AboutObj* aboutObj = NULL;
static CommonBusListener* busListener = NULL;
static ConfigDataStore* configDataStore = NULL;
static ConfigServiceListenerImpl* configServiceListener = NULL;
static ajn::services::ConfigService* configService = NULL;
const string CONF_DIR  = "/etc/xmppconn";
const string CONF_FILE = CONF_DIR + "/xmppconn.conf";
static string s_User = "test";
static string s_Password = "test";
static string s_ChatRoom;
static string s_ProductID;
static string s_SerialNumber;
static string s_AllJoynPasscode;
static string s_AppId;
static vector<string> s_Roster;
static string ifaceName = "org.alljoyn.Config.Chariot.Xmpp";
static const qcc::String interface = "<node name='/Config/Chariot/XMPP' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'"
                                    " xsi:noNamespaceSchemaLocation='http://www.allseenalliance.org/schemas/introspect.xsd'>"
                                    "<interface name='" + qcc::String(ifaceName.c_str()) + "'>"
                                    "<property name='Version' type='q' access='read'/>"
                                    "<method name='FactoryReset'>"
                                    "<annotation name='org.freedesktop.DBus.Method.NoReply' value='true'/>"
                                    "</method>"
                                    "<method name='Restart'>"
                                    "<annotation name='org.freedesktop.DBus.Method.NoReply' value='true'/>"
                                    "</method>"
                                    "<method name='SetPasscode'>"
                                    "<arg name='daemonRealm' type='s' direction='in'/>"
                                    "<arg name='newPasscode' type='ay' direction='in'/>"
                                    "</method>"
                                    "<method name='GetConfigurations'>"
                                    "<arg name='languageTag' type='s' direction='in'/>"
                                    "<arg name='configData' type='a{sv}' direction='out'/>"
                                    "</method>"
                                    "<method name='UpdateConfigurations'>"
                                    "<arg name='languageTag' type='s' direction='in'/>"
                                    "<arg name='configMap' type='a{sv}' direction='in'/>"
                                    "</method>"
                                    "<method name='ResetConfigurations'>"
                                    "<arg name='languageTag' type='s' direction='in'/>"
                                    "<arg name='fieldList' type='as' direction='in'/>"
                                    "</method>"
                                    "</interface>"
                                    "</node>";

static void simpleCallback()
{
}

static void onRestart(){
    s_Continue = true;
    if(s_Conn)
    {
        s_Conn->Stop();
    }
}

class SimpleBusObject : public BusObject {
    public:
        SimpleBusObject(BusAttachment& bus, const char* path)
            : BusObject(path) {
                QStatus status;
                const InterfaceDescription* iface = bus.GetInterface(ifaceName.c_str());
                assert(iface != NULL);

                status = AddInterface(*iface, ANNOUNCED);
                if (status != ER_OK) {
                    printf("Failed to add %s interface to the BusObject\n", ifaceName.c_str());
                }
            }
};

class SimplePropertyStore :
    public services::PropertyStore
{
    public:
        SimplePropertyStore(
                MsgArg& allProperties
                ) :
            m_AllProperties(allProperties)
    {
        m_AllProperties.Stabilize();
    }

        ~SimplePropertyStore()
        {}

        QStatus
            ReadAll(
                    const char* languageTag,
                    Filter      filter,
                    MsgArg&     all
                   )
            {
                all = m_AllProperties;
                all.Stabilize();
                return ER_OK;
            }

        QStatus
            Update(
                    const char*   name,
                    const char*   languageTag,
                    const MsgArg* value
                  )
            {
                LOG_DEBUG("UPDATE CALLED");
                return ER_NOT_IMPLEMENTED;
            }

        QStatus
            Delete(
                    const char* name,
                    const char* languageTag
                  )
            {
                LOG_DEBUG("DELETE CALLED");
                return ER_NOT_IMPLEMENTED;
            }

    private:
        MsgArg m_AllProperties;
};

class AlertReceiver :
    public XMPPConnector::MessageReceiver,
    public SessionPortListener
{
    public:
        AlertReceiver(
                BusAttachment* bus
                ) :
            m_bus(bus),
            m_aboutPropertyStore(NULL),
            m_notifService(NULL),
            m_notifSender(NULL)
    {
        qcc::String deviceid;
        services::GuidUtil::GetInstance()->GetDeviceIdString(&deviceid);
        qcc::String appid;
        services::GuidUtil::GetInstance()->GenerateGUID(&appid);
        size_t len = appid.length()/2;
        uint8_t* bytes = new uint8_t[len];
        qcc::HexStringToBytes(appid, bytes, len);
        MsgArg appidArg("ay", len, bytes);
        appidArg.Stabilize();

        // Create our About PropertyStore
        vector<MsgArg> dictEntries(7);
        string key, valueStr; MsgArg value;

        key = "AppId";
        value = appidArg; value.Stabilize();
        dictEntries[0].Set("{sv}", key.c_str(), &value);
        dictEntries[0].Stabilize();

        key = "AppName"; valueStr = "Notifier";
        value.Set("s", valueStr.c_str()); value.Stabilize();
        dictEntries[1].Set("{sv}", key.c_str(), &value);
        dictEntries[1].Stabilize();

        key = "DefaultLanguage"; valueStr = "en";
        value.Set("s", valueStr.c_str()); value.Stabilize();
        dictEntries[2].Set("{sv}", key.c_str(), &value);
        dictEntries[2].Stabilize();

        key = "DeviceId";
        value.Set("s", deviceid.c_str()); value.Stabilize();
        dictEntries[3].Set("{sv}", key.c_str(), &value);
        dictEntries[3].Stabilize();

        key = "DeviceName"; valueStr = "Alarm Service";
        value.Set("s", valueStr.c_str()); value.Stabilize();
        dictEntries[4].Set("{sv}", key.c_str(), &value);
        dictEntries[4].Stabilize();

        key = "Manufacturer"; valueStr = "Affinegy";
        value.Set("s", valueStr.c_str()); value.Stabilize();
        dictEntries[5].Set("{sv}", key.c_str(), &value);
        dictEntries[5].Stabilize();

        key = "ModelNumber"; valueStr = "1.0";
        value.Set("s", valueStr.c_str()); value.Stabilize();
        dictEntries[6].Set("{sv}", key.c_str(), &value);
        dictEntries[6].Stabilize();

        MsgArg aboutArgs("a{sv}", dictEntries.size(), &dictEntries[0]);

        m_aboutPropertyStore = new SimplePropertyStore(aboutArgs);
        ajn::services::AboutServiceApi::Init(*m_bus, *m_aboutPropertyStore);
        ajn::services::AboutServiceApi* aboutService = services::AboutServiceApi::getInstance();
        SessionPort sp = 900;
        SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false,
                SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
        m_bus->BindSessionPort(sp, opts, *this);
        aboutService->Register(sp);
        m_bus->RegisterBusObject(*aboutService);
        aboutService->Announce();

        m_notifService = services::NotificationService::getInstance();
        m_notifSender = m_notifService->initSend(m_bus, m_aboutPropertyStore);
    }

        ~AlertReceiver()
        {
            // TODO: delete stuff
        }

        bool
            AcceptSessionJoiner(
                    SessionPort        sessionPort,
                    const char*        joiner,
                    const SessionOpts& opts
                    )
            {
                return true;
            }

        void
            AlertHandler(
                    const string& from,
                    const string& key,
                    const string& message,
                    void*         userdata
                    )
            {
                // Parse the message
                istringstream msg_stream(message);
                string token;
                vector<string> lines;
                while (getline(msg_stream, token, '\n'))
                {
                    lines.push_back(token);
                }
                map<string,string> alarm_data;
                for ( vector<string>::iterator it(lines.begin()); lines.end() != it; ++it )
                {
                    istringstream line_stream(*it);
                    vector<string> tokens;
                    while (getline(line_stream, token, ':'))
                    {
                        tokens.push_back(token);
                    }
                    // Ignore lines with more than one colon
                    if ( tokens.size() == 2 )
                    {
                        // Add the key/value pair
                        alarm_data[tokens.at(0)] = tokens.at(1);
                    }
                }

                // Send the Description as an AllJoyn Notification
                if ( alarm_data.end() != alarm_data.find("Description") )
                {
                    // TODO:
                    LOG_DEBUG("Alarm Description: %s", alarm_data.at("Description").c_str());
                    services::NotificationMessageType message_type = services::INFO;
                    services::NotificationText message("en", alarm_data.at("Description").c_str());
                    vector<services::NotificationText> messages;
                    messages.push_back(message);
                    services::Notification notification(message_type, messages);
                    QStatus status = m_notifSender->send(notification, 7200);
                    if (status != ER_OK)
                    {
                        LOG_RELEASE("Failed to send Alarm notification! Reason; %s", QCC_StatusText(status));
                    }
                    else
                    {
                        LOG_RELEASE("Successfully sent Alarm notification! Reason; %s", QCC_StatusText(status));
                    }
                }
            }

    private:
        BusAttachment*                 m_bus;
        SimplePropertyStore*           m_aboutPropertyStore;
        services::NotificationService* m_notifService;
        services::NotificationSender*  m_notifSender;
};

void cleanup()
{
    if (s_Conn) {
        delete s_Conn;
        s_Conn = 0;
    }

    if(s_Bus) {
        delete s_Bus;
        s_Bus = 0;
    }
}

static void SigIntHandler(int sig)
{
    LOG_RELEASE("Handling SIGINT");
    if(s_Conn)
    {
        s_Conn->Stop();
    }
    s_Continue = false;
    exit(1);
}

string getJID()
{
    return s_User;
}

string getPassword()
{
    return s_Password;
}

vector<string> getRoster()
{
    return s_Roster;
}

string getChatRoom()
{
    return s_ChatRoom;
}

string getAppId( ConfigParser& configParser )
{
    uuid_t uuid;
    string uuidString;
    // Try to find the AppId from ConfigParser and create+add it if it isn't there
    if(configParser.GetField("AppId").empty()){
        uuid_generate_random(uuid);
        char uuid_str[37];
        uuid_unparse_lower(uuid, uuid_str);
        uuidString = uuid_str;
        configParser.SetField("AppId", uuid_str);
    }
    else{
        uuidString = configParser.GetField("AppId");
    }
    return uuidString;
}

void getConfigurationFields(){
    ConfigParser configParser(CONF_FILE.c_str());
    if(!configParser.isConfigValid()){
        LOG_RELEASE("Error parsing Config File: Invalid format");
        cleanup();
        exit(1);
    }

    string verbosity = configParser.GetField("Verbosity");
    if(verbosity == "2"){
        util::_dbglogging = true;
        util::_verboselogging = true;
    }
    else if( verbosity == "1" ){
        util::_dbglogging = true;
        util::_verboselogging = false;
    }
    else{
        util::_dbglogging = false;
        util::_verboselogging = false;
    }

    s_User = configParser.GetField("UserJID");
    s_Password = configParser.GetField("UserPassword");
    s_Roster = configParser.GetRoster();
    s_ChatRoom = configParser.GetField("RoomJID");
    s_SerialNumber = configParser.GetField("SerialNumber");
    s_ProductID = configParser.GetField("ProductID");
    s_AllJoynPasscode = configParser.GetField("AllJoynPasscode");
    s_AppId = getAppId(configParser);

    string tmp = configParser.GetField("Compress");
    if(tmp != "")
        istringstream(tmp) >> s_Compress;
    else
        s_Compress = 0;
}

int main(int argc, char** argv)
{
    signal(SIGINT, SigIntHandler);

    // Read in the configuration file
    ifstream conf_file(CONF_FILE.c_str());
    if ( !conf_file.is_open() )
    {
        LOG_RELEASE("Could not open %s!", CONF_FILE.c_str());
        exit(1);
    }
    conf_file.close();


    s_Bus = new BusAttachment("XMPPConnector", true);

    // We need to do this to get our product ID and serial number
    getConfigurationFields();

    // Set up bus attachment
    QStatus status = s_Bus->Start();
    if (ER_OK != status) {
        LOG_RELEASE("Error starting bus: %s", QCC_StatusText(status));
        cleanup();
        return status;
    }

    status = s_Bus->Connect();
    if (ER_OK != status) {
        LOG_RELEASE("Error connecting bus: %s", QCC_StatusText(status));
        cleanup();
        return status;
    }

    configDataStore = new ConfigDataStore("/etc/xmppconn/xmppconn_factory.conf",
                                          "/etc/xmppconn/xmppconn.conf",
                                          s_AppId.c_str(),
                                          (s_ProductID + s_SerialNumber).c_str(),
                                          onRestart);
    configDataStore->Initialize();

    aboutObj = new ajn::AboutObj(*s_Bus, BusObject::ANNOUNCED);
    ajn::services::AboutObjApi::Init(s_Bus, (configDataStore), aboutObj);
    ajn::services::AboutObjApi* aboutService = ajn::services::AboutObjApi::getInstance();
    if (!aboutService){
        LOG_RELEASE("Failed to get About Service instance!");
        return ER_BUS_NOT_ALLOWED;
    }

    busListener = new CommonBusListener(s_Bus, simpleCallback);

    TransportMask transportMask = TRANSPORT_ANY;
    SessionPort sp = 900;
    SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, transportMask);

    busListener->setSessionPort(sp);

    s_Bus->RegisterBusListener(*busListener);
    status = s_Bus->BindSessionPort(sp, opts, *busListener);
    if (status != ER_OK) {
        LOG_RELEASE("Failed to bind session port %d! Reason: %s", sp, QCC_StatusText(status));
        cleanup();
        return status;
    }

    // Build the interface name so we can advertise it
    string ifaceName = "global.chariot.C" + s_AppId;
    printf("ANDREY: In app main, ifaceName = %s\n", ifaceName.c_str());
    ifaceName.erase(std::remove(ifaceName.begin(), ifaceName.end(), '-'), ifaceName.end());
    LOG_DEBUG("Interface Name: %s", ifaceName.c_str());

    // Advertise the connector
    aboutService->SetPort(sp);
    status = s_Bus->RequestName(ifaceName.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE);
    if ( ER_OK != status ){
        LOG_RELEASE("Failed to request advertised name %s! Reason: %s", ifaceName.c_str(), QCC_StatusText(status));
        cleanup();
        return status;
    }
    status = s_Bus->AdvertiseName(ifaceName.c_str(), TRANSPORT_ANY);
    if ( ER_OK != status ){
        LOG_RELEASE("Failed to advertised name %s! Reason: %s", ifaceName.c_str(), QCC_StatusText(status));
        cleanup();
        return status;
    }

    configServiceListener = new ConfigServiceListenerImpl(*configDataStore, *s_Bus, busListener, onRestart, CONF_FILE);

    configService = new ajn::services::ConfigService(*s_Bus, *configDataStore, *configServiceListener);


    status = s_Bus->CreateInterfacesFromXml(interface.c_str());
    if ( ER_OK != status ){
        LOG_RELEASE("Failed to interfaces from xml! Reason: %s", QCC_StatusText(status));
        cleanup();
        return status;
    }
    const InterfaceDescription* iface = s_Bus->GetInterface(ifaceName.c_str());  

    SimpleBusObject busObject(*s_Bus, "/Config/Chariot/XMPP");
    status = s_Bus->RegisterBusObject(busObject);
    if ( ER_OK != status ){
        LOG_RELEASE("Failed to register bus object! Reason: %s", QCC_StatusText(status));
        cleanup();
        return status;
    }

    status = configService->Register();
    if(status != ER_OK) {
        LOG_RELEASE("Could not register the ConfigService. Reason: %s", QCC_StatusText(status));
        cleanup();
        return status;
    }

    status = s_Bus->RegisterBusObject(*configService);
    if(status != ER_OK) {
        LOG_RELEASE("Could not register the ConfigService BusObject. Reason: %s", QCC_StatusText(status));
        cleanup();
        return status;
    }

    status = aboutService->Announce();
    if(status != ER_OK) {
        LOG_RELEASE("Could not announce the About Service! Reason: %s", QCC_StatusText(status));
        cleanup();
        return status;
    }

    do{
        bool waitForConfigChange(false);
        getConfigurationFields();
        if ( getJID().empty() ||
             getPassword().empty() ||
             getRoster().empty()
            )
        {
            LOG_RELEASE("Configuration doesn't contain XMPP parameters.");
            waitForConfigChange = true;
        }

        if ( !waitForConfigChange )
        {
            s_Conn = new XMPPConnector(s_Bus, "XMPP", getJID(),
                    getPassword(), 
                    getRoster(),
                    getChatRoom(),
                    s_Compress);

            LOG_RELEASE("Initializing XMPPConnector");
            status = s_Conn->Init();
            if(ER_OK != status)
            {
                LOG_RELEASE("Could not initialize XMPPConnector! Reason: %s", QCC_StatusText(status));
                waitForConfigChange = true;
            }
            else
            {
                s_Conn->AddSessionPortMatch("org.alljoyn.ControlPanel.ControlPanel", 1000);
                s_Conn->AddSessionPortMatch("org.alljoyn.bus.samples.chat", 27);
                s_Conn->Start();
            }

            if(s_Conn){
                delete s_Conn;
                s_Conn = 0;
            }
        }
        
        if ( waitForConfigChange )
        {
            int fd = inotify_init();
            if ( -1 == fd )
            {
                LOG_RELEASE("Could not initialize inotify! Exiting...");
                cleanup();
                return errno;
            }
            int wd = inotify_add_watch(fd, CONF_FILE.c_str(), IN_MODIFY);
            if ( -1 == wd )
            {
                LOG_RELEASE("Could not add watch on conf file! Exiting...");
                cleanup();
                return errno;
            }
            LOG_RELEASE("Waiting for configuration changes before trying to connect to the XMPP server...");
            inotify_event evt = {};
            read(fd, &evt, sizeof(evt));
            inotify_rm_watch(fd, wd);
            close(fd);
            // TODO: This is necessary to allow the other thread to complete writing to the file before we 
            //  continue. We should do this in a more robust manner.
            sleep(1);

            waitForConfigChange = false;
            s_Continue = true;
        }

    printf("ANDREY: Waiting\n");
    }while(s_Continue);

    cleanup();
}

