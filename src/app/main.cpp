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
#include "app/ConfigParser.h"
#include "common/xmppconnutil.h"
#include "common/CommonBusListener.h"
#include "SimpleBusObject.h"

#include <alljoyn/about/AboutPropertyStoreImpl.h>
#include <alljoyn/about/AnnouncementRegistrar.h>
#include <alljoyn/about/AboutServiceApi.h>
#include <alljoyn/services_common/GuidUtil.h>
#include <alljoyn/AboutObj.h>
#include <alljoyn/BusAttachment.h>
#include <qcc/StringUtil.h>
#include <alljoyn/Init.h>

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
#include <unistd.h>

using namespace ajn;
#ifndef NO_AJ_GATEWAY
using namespace ajn::gw;
#endif // !NO_AJ_GATEWAY

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
static bool s_Continue = true;
static const unsigned long s_AllJoynAnnouncementWait = 5;
static const unsigned long s_ReconnectWait = 30;
static int s_ConfigFileDescriptor = 0;
static int s_ConfigFileWatchDescriptor = 0;

static BusAttachment* s_Bus = 0;
static XMPPConnector* s_Conn = 0;
static AboutObj* aboutObj = NULL;
static CommonBusListener* busListener = NULL;
static ConfigDataStore* configDataStore = NULL;
static ConfigServiceListenerImpl* configServiceListener = NULL;
static ajn::services::ConfigService* configService = NULL;

static vector<string> s_Roster;

#ifndef NO_AJ_GATEWAY
const string CONF_DIR  = "/opt/alljoyn/apps/xmppconn/etc";
#else
const string CONF_DIR  = "/etc/xmppconn";
#endif // !NO_AJ_GATEWAY

const string CONF_FILE = CONF_DIR + "/xmppconn.conf";
const string FACTORY_FILE = CONF_DIR + "/xmppconn_factory.conf";


static string s_User = "test";
static string s_Password = "test";
static string s_ChatRoom;
static string s_ProductID;
static string s_SerialNumber;
static string s_AllJoynPasscode;
static string s_AppId;

static const qcc::String interface = "<node name='" + qcc::String(ALLJOYN_XMPP_CONFIG_PATH.c_str()) + "' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'"
                                    " xsi:noNamespaceSchemaLocation='http://www.allseenalliance.org/schemas/introspect.xsd'>"
                                    "<interface name='" + qcc::String(ALLJOYN_XMPP_CONFIG_INTERFACE_NAME.c_str()) + "'>"
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
    if(s_Conn)
    {
        s_Conn->Stop();
    }
}

void cleanup()
{
    if (s_Conn) {
        delete s_Conn;
        s_Conn = 0;
    }

    if(aboutObj){
        delete aboutObj;
        aboutObj = 0;
    }

    if(s_Bus) {
        delete s_Bus;
        s_Bus = 0;
    }

    if(configDataStore){
        delete configDataStore;
        configDataStore = 0;
    }

    if(busListener){
        delete busListener;
        busListener = 0;
    }

    if(configServiceListener){
        delete configServiceListener;
        configServiceListener = 0;
    }

    if(configService){
        delete configService;
        configService = 0;
    }
}

static void SigIntHandler(int sig)
{
    LOG_RELEASE("Handling SIGINT");
    inotify_rm_watch(s_ConfigFileDescriptor, s_ConfigFileWatchDescriptor);
    s_Continue = false;
    if(s_Conn)
    {
        s_Conn->Stop();
    }
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

    s_User            = configParser.GetField("UserJID");
    s_Password        = configParser.GetField("UserPassword");
    s_Roster          = configParser.GetRoster();
    s_ChatRoom        = configParser.GetField("RoomJID");
    s_SerialNumber    = configParser.GetField("SerialNumber");
    s_ProductID       = configParser.GetField("ProductID");
    s_AllJoynPasscode = configParser.GetField("AllJoynPasscode");

    s_AppId = configParser.GetField("AppId");
    if (s_AppId.empty())
    {
        s_AppId = util::generateAppId();
        configParser.SetField("AppId", s_AppId.c_str());
    }

    string tmp = configParser.GetField("Compress");
    if(tmp != "")
        istringstream(tmp) >> s_Compress;
    else
        s_Compress = 0;
}

int main(int argc, char** argv)
{
    QStatus status = AllJoynInit();
    if (status != ER_OK)
    {
        LOG_RELEASE("Failed to initialize AllJoyn: %s", QCC_StatusText(status));
        return status;
    }

    signal(SIGINT, SigIntHandler);

    // Ensure that we can open our config file
    ifstream conf_file(CONF_FILE.c_str());
    if ( !conf_file.is_open() )
    {
        // If it's not there try to copy the factory reset file over it
        ifstream factory(FACTORY_FILE.c_str(), std::ios::binary);
        if ( !factory.is_open() )
        {
            LOG_RELEASE("Could not open %s!", FACTORY_FILE.c_str());
            return 1;
        }
        std::ofstream newconffile(CONF_FILE.c_str(), std::ios::binary);
        if ( !newconffile.is_open() )
        {
            LOG_RELEASE("Could not open %s!", CONF_FILE.c_str());
            return 1;
        }

        // Now copy the factory file to the main conf file
        newconffile << factory.rdbuf();
    }
    conf_file.close();

    bool resetbus = true;
    bool waitForConfigChange = false;

    do{

    
        // We need to do this to get our product ID and serial number
        getConfigurationFields();

        if ( resetbus )
        {
            s_Bus = new BusAttachment("XMPPConnector", true);
    
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
        
            configDataStore = new ConfigDataStore(FACTORY_FILE.c_str(),
                                                  CONF_FILE.c_str(),
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
        
            SessionPort sp = 900;
            TransportMask transportMask = TRANSPORT_ANY;
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
            string ifaceName = ALLJOYN_URL_SUFFIX + ".C" + s_AppId;
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
        
            configServiceListener = new ConfigServiceListenerImpl(*configDataStore, 
                                                                 *s_Bus, busListener,
                                                                 onRestart,
                                                                 CONF_FILE
                                                                 );
        
            configService = new ajn::services::ConfigService(*s_Bus, *configDataStore, *configServiceListener);
        
            status = s_Bus->CreateInterfacesFromXml(interface.c_str());
            if ( ER_OK != status ){
                LOG_RELEASE("Failed to interfaces from xml! Reason: %s", QCC_StatusText(status));
                cleanup();
                return status;
            }
        
            SimpleBusObject busObject(*s_Bus, ALLJOYN_XMPP_CONFIG_PATH.c_str());
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
        }

        resetbus = false;
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
            s_Conn = new XMPPConnector(
                    s_Bus,
                    ALLJOYN_XMPP_CONFIG_INTERFACE_NAME,
                    "xmppconn",
                    getJID(),
                    getPassword(), 
                    getRoster(),
                    getChatRoom(),
                    s_Compress);

            LOG_RELEASE("Initializing XMPPConnector");
            QStatus status = s_Conn->Init();
            if(ER_OK != status)
            {
                LOG_RELEASE("Could not initialize XMPPConnector! Reason: %s", QCC_StatusText(status));
                waitForConfigChange = true;
            }
            else
            {
                s_Conn->AddSessionPortMatch("org.alljoyn.ControlPanel.ControlPanel", 1000);
                s_Conn->AddSessionPortMatch("org.alljoyn.bus.samples.chat", 27);
                // TODO: We will likely need to handle different types of errors,
                // instead of the generic "!ER_OK"
                // For now, only the authentication failure is returned
                if (s_Conn->Start() != ER_OK)
                {
                    waitForConfigChange = true;
                }
                resetbus = true;
            }

            if(s_Conn){
                delete s_Conn;
                s_Conn = 0;
            }
        }
        else
        {
            s_ConfigFileDescriptor = inotify_init();
            if ( -1 == s_ConfigFileDescriptor )
            {
                LOG_RELEASE("Could not initialize inotify! Exiting...");
                cleanup();
                return errno;
            }

            s_ConfigFileWatchDescriptor = inotify_add_watch(s_ConfigFileDescriptor, CONF_FILE.c_str(), IN_MODIFY);
            if ( -1 == s_ConfigFileWatchDescriptor )
            {
                LOG_RELEASE("Could not add watch on conf file! Exiting...");
                cleanup();
                return errno;
            }

            LOG_RELEASE("Waiting for configuration changes before trying to connect to the XMPP server...");
            inotify_event evt = {};

            fd_set descriptors;
            FD_ZERO(&descriptors);
            FD_SET(s_ConfigFileDescriptor, &descriptors);

            timeval time_to_wait = { s_ReconnectWait };

            int ret = select(s_ConfigFileDescriptor + 1, &descriptors, NULL, NULL, &time_to_wait);

            if ( ret < 0 )
            {
                LOG_RELEASE("Could not monitor config file for changes");
            }

            else if ( ret == 0 )
            {
                LOG_RELEASE("Waited %lu s, will attempt to reconnect", s_ReconnectWait);
            }

            else if ( FD_ISSET(s_ConfigFileDescriptor, &descriptors) )
            {
                LOG_DEBUG("Config file was modified");
                // Process the inotify events, then remove inotify watch and close the file
                read(s_ConfigFileDescriptor, &evt, sizeof(evt));
                inotify_rm_watch( s_ConfigFileDescriptor, s_ConfigFileWatchDescriptor );
                close(s_ConfigFileDescriptor);

                // TODO: This is necessary to allow the other thread to complete writing to the file before we
                //  continue. We should do this in a more robust manner.
                if (s_Continue)
                {
                    sleep(s_AllJoynAnnouncementWait);
                }
            }

            waitForConfigChange = false;
        }

        if ( s_Conn )
        {
            s_Conn->Stop();
        }

        if ( resetbus )
        {
            cleanup();
        }

    }while(s_Continue);

    AllJoynShutdown();
}

