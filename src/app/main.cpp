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
#include "app/SrpKeyXListener.h"

#include <alljoyn/about/AboutPropertyStoreImpl.h>
#include <alljoyn/about/AnnouncementRegistrar.h>
#include <alljoyn/about/AboutServiceApi.h>
#include <alljoyn/services_common/GuidUtil.h>
#include <alljoyn/AboutObj.h>
#include <alljoyn/BusAttachment.h>
#include <qcc/StringUtil.h>

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

using namespace ajn;
using namespace ajn::gw;

using std::stringstream;
using std::ifstream;
using std::getline;
using std::cout;
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
static bool CONTINUE_ON_RESTART = false;
static BusAttachment* s_Bus = 0;
static XMPPConnector* s_Conn = 0;
static AboutObj* aboutObj = NULL;
static CommonBusListener* busListener = NULL;
static ConfigDataStore* configDataStore = NULL;
static ConfigServiceListenerImpl* configServiceListener = NULL;
static ajn::services::ConfigService* configService = NULL;
static SrpKeyXListener* keyListener = NULL;
const string CONF_FILE = "/etc/xmppconn/xmppconn.conf";
static string s_Server = "xmpp.chariot.global";
static string s_ServiceName = "muc";
static string s_User = "test";
static string s_Password = "test";
static string s_ChatRoom;
static string s_Resource;
static string s_Roster;
static ConfigParser* configParser = NULL;
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
    CONTINUE_ON_RESTART = true;
    if(s_Conn) s_Conn->Stop();
    s_Conn->Stop();
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
                cout << "UPDATE CALLED" << endl;
                return ER_NOT_IMPLEMENTED;
            }

        QStatus
            Delete(
                    const char* name,
                    const char* languageTag
                  )
            {
                cout << "DELETE CALLED" << endl;
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
                    cout << "Alarm Description: " << alarm_data.at("Description") << endl;
                    services::NotificationMessageType message_type = services::INFO;
                    services::NotificationText message("en", alarm_data.at("Description").c_str());
                    vector<services::NotificationText> messages;
                    messages.push_back(message);
                    services::Notification notification(message_type, messages);
                    QStatus status = m_notifSender->send(notification, 7200);
                    if (status != ER_OK)
                    {
                        cout << "Failed to send Alarm notification!" << endl;
                    }
                    else
                    {
                        cout << "Successfully sent Alarm notification!" << endl;
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
    cout << "Handling SIGINT" << endl;
    if(s_Conn)
    {
        s_Conn->Stop();
    }
    CONTINUE_ON_RESTART = false;
    exit(1);
}

string getJID()
{
    return s_User + "@" + s_Server;
}

string getPassword()
{
    return s_Password;
}

string getRoster()
{
    return s_Roster;
}

string getChatRoom()
{
    if ( !s_ChatRoom.empty() ) {
        return s_ChatRoom + "@" + s_ServiceName + "." + s_Server;
    }
    return string();
}

string getResource()
{
    return s_Resource;
}

void getConfigurationFields(){

    if(!configParser->isConfigValid()){
        cout << "Error parsing Config File: Invalid format" << endl;
        cleanup();
        exit(1);
    }

    s_User = configParser->GetField("UserJID");
    s_Password = configParser->GetField("Password");
    s_ChatRoom = configParser->GetField("Room");
    s_Resource = configParser->GetField("Resource");


    char** tmpRosters = configParser->GetRosters();
    s_Roster = tmpRosters[0];


    string tmp = configParser->GetField("Compress");
    if(tmp != "")
        istringstream(tmp) >> s_Compress;
    else
        s_Compress = 0;

    string verbosity = configParser->GetField("Verbosity");

    if(verbosity == "0"){
        util::_dbglogging = false;
        util::_verboselogging = false;

    }
    else if( verbosity == "1" ){
        util::_dbglogging = true;
        util::_verboselogging = false;
    }
    else{
        util::_dbglogging = true;
        util::_verboselogging = true;
    }

    delete configParser;

}

int main(int argc, char** argv)
{
    signal(SIGINT, SigIntHandler);

    // Read in the configuration file
    ifstream conf_file(CONF_FILE.c_str());
    if ( !conf_file.is_open() )
    {
        cerr << "Could not open " << CONF_FILE << "!" << endl;
        exit(1);
    }
    conf_file.close();

    configParser = new ConfigParser(CONF_FILE.c_str());
    configParser->isConfigValid();

    //ifaceName += configParser->GetField("ProductID") + "-" + configParser->GetField("SerialNumber");

    keyListener = new SrpKeyXListener();
    keyListener->setPassCode("000000");

    s_Bus = new BusAttachment("XMPPConnector", true);

    string s_ProductID = configParser->GetField("ProductID");
    string s_SerialNumber = configParser->GetField("SerialNumber");
    string advertiseName = "global.chariot.direct." + s_ProductID + "_" + s_SerialNumber;
    s_Bus->AdvertiseName("global.chariot.direct", TRANSPORT_ANY);

    // Set up bus attachment
    QStatus status = s_Bus->Start();
    if (ER_OK != status) {
        cout << "Error starting bus: " << QCC_StatusText(status) << endl;
        cleanup();
        return 1;
    }

    status = s_Bus->Connect();
    if (ER_OK != status) {
        cout << "Error connecting bus: " << QCC_StatusText(status) << endl;
        cleanup();
        return 1;
    }

    status = s_Bus->EnablePeerSecurity("ALLJOYN_PIN_KEYX ALLJOYN_SRP_KEYX ALLJOYN_ECDHE_PSK", dynamic_cast<AuthListener*>(keyListener));


    configDataStore = new ConfigDataStore("/etc/xmppconn/xmppconn_factory.conf",
                                          "/etc/xmppconn/xmppconn.conf",
                                          onRestart);
    configDataStore->Initialize();

    aboutObj = new ajn::AboutObj(*s_Bus, BusObject::ANNOUNCED);
    ajn::services::AboutObjApi::Init(s_Bus, (configDataStore), aboutObj);
    ajn::services::AboutObjApi* aboutService = ajn::services::AboutObjApi::getInstance();
    if (!aboutService) return ER_BUS_NOT_ALLOWED;

    busListener = new CommonBusListener(s_Bus, simpleCallback);

    TransportMask transportMask = TRANSPORT_ANY;
    SessionPort sp = 900;
    SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, transportMask);

    busListener->setSessionPort(900);

    s_Bus->RegisterBusListener(*busListener);
    status = s_Bus->BindSessionPort(sp, opts, *busListener);
    if (status != ER_OK) {
        return status;
    }

    aboutService->SetPort(900);

    configServiceListener = new ConfigServiceListenerImpl(*configDataStore, *s_Bus, busListener, onRestart);
    configService = new ajn::services::ConfigService(*s_Bus, *configDataStore, *configServiceListener);


    status = s_Bus->CreateInterfacesFromXml(interface.c_str());
    const InterfaceDescription* iface = s_Bus->GetInterface(ifaceName.c_str());  

    SimpleBusObject busObject(*s_Bus, "/Config/Chariot/XMPP");
    status = s_Bus->RegisterBusObject(busObject);

    status = configService->Register();
    if(status != ER_OK) {
        std::cout << "Could not register the ConfigService" << std::endl;
    }

    status = s_Bus->RegisterBusObject(*configService);
    if(status != ER_OK) {
        std::cout << "Could not register the ConfigService BusObject" << std::endl;
        cleanup();
        return 1;
    }

    ajn::services::AboutObjApi* testService = ajn::services::AboutObjApi::getInstance();

    if (!testService) {
        cout << ER_BUS_NOT_ALLOWED << endl; 
    }

    status = aboutService->Announce();
    std::cout << QCC_StatusText(status) << endl;

    do{
        getConfigurationFields();
        s_Conn = new XMPPConnector(s_Bus, "XMPP", getJID(),
                getPassword(), 
                getRoster(),
                getChatRoom(), 
                getResource(), 
                s_Compress);

        if(ER_OK != s_Conn->Init())
        {
            cout << "Could not initialize XMPPConnector" << endl;
            cleanup();
            return 1;
        }

        s_Conn->AddSessionPortMatch("org.alljoyn.ControlPanel.ControlPanel", 1000);
        s_Conn->AddSessionPortMatch("org.alljoyn.bus.samples.chat", 27);
        s_Conn->Start();

        if(s_Conn){
            cout << "Destroying last instance" << endl;
            delete s_Conn;
        }

    }while(CONTINUE_ON_RESTART);

    cleanup();
}

