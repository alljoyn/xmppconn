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

#include <alljoyn/BusAttachment.h>
#include <alljoyn/about/AnnouncementRegistrar.h>
#include <alljoyn/services_common/GuidUtil.h>
#include <qcc/StringUtil.h>
#include "app/XMPPConnector.h"
#include "common/xmppconnutil.h"
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

static BusAttachment* s_Bus = 0;
static XMPPConnector* s_Conn = 0;
const string CONF_FILE = "/etc/xmppconn/xmppconn.conf";
static string s_Server = "xmpp.chariot.global";
static string s_ServiceName = "muc";
static string s_User = "test";
static string s_Password = "test";
static string s_ChatRoom;
static string s_Resource;
static string s_Roster;
static bool s_Compress = true;

static inline string &ltrim(string &s) {
        s.erase(s.begin(), find_if(s.begin(), s.end(), std::not1(ptr_fun<int, int>(isspace))));
        return s;
}

static inline string &rtrim(string &s) {
        s.erase(find_if(s.rbegin(), s.rend(), not1(ptr_fun<int, int>(isspace))).base(), s.end());
        return s;
}

static inline string &trim(string &s) {
        return ltrim(rtrim(s));
}

static inline vector<string> &split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

static inline vector<string> split(const string &s, char delim) {
    vector<string> elems;
    split(s, delim, elems);
    return elems;
}

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
        services::AboutServiceApi::Init(*m_bus, *m_aboutPropertyStore);
        services::AboutServiceApi* aboutService = services::AboutServiceApi::getInstance();
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
    string line;
    while ( getline( conf_file, line ) )
    {
        vector<string> tokens = split(line, '=');
        if ( tokens.size() > 0 && trim(tokens[0]) == "SERVER" )
        {
            if ( tokens.size() > 2 )
            {
                cerr << "Too many tokens in line: " << endl << line << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
            }
            if ( tokens.size() > 1 )
            {
                s_Server = trim(tokens[1]);
            }
            if ( s_Server.empty() )
            {
                cerr << "SERVER cannot be specified as a blank value." << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
                exit(1);
            }
        }
        else if ( tokens.size() > 0 && trim(tokens[0]) == "CHATROOM" )
        {
            if ( tokens.size() > 2 )
            {
                cerr << "Too many tokens in line: " << endl << line << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
            }
            if ( tokens.size() > 1 )
            {
                s_ChatRoom = trim(tokens[1]);
            }
            if ( s_ChatRoom.empty() )
            {
                cerr << "CHATROOM cannot be specified as a blank value." << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
                exit(1);
            }
        }
        else if ( tokens.size() > 0 && trim(tokens[0]) == "USER" )
        {
            if ( tokens.size() > 2 )
            {
                cerr << "Too many tokens in line: " << endl << line << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
            }
            if ( tokens.size() > 1 )
            {
                s_User = trim(tokens[1]);
            }
            if ( s_User.empty() )
            {
                cerr << "USER cannot be specified as a blank value." << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
                exit(1);
            }
        }
        else if ( tokens.size() > 0 && trim(tokens[0]) == "PASSWORD" )
        {
            if ( tokens.size() > 2 )
            {
                cerr << "Too many tokens in line: " << endl << line << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
            }
            if ( tokens.size() > 1 )
            {
                s_Password = trim(tokens[1]);
            }
            if ( s_Password.empty() )
            {
                cerr << "PASSWORD cannot be specified as a blank value." << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
                exit(1);
            }
        }
        else if ( tokens.size() > 0 && trim(tokens[0]) == "RESOURCE" )
        {
            if ( tokens.size() > 2 )
            {
                cerr << "Too many tokens in line: " << endl << line << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
            }
            if ( tokens.size() > 1 )
            {
                s_Resource = trim(tokens[1]);
            }
            if ( s_Resource.empty() )
            {
                cerr << "RESOURCE cannot be specified as a blank value." << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
                exit(1);
            }
        }
        else if ( tokens.size() > 0 && trim(tokens[0]) == "ROSTER" )
        {
            if ( tokens.size() > 2 )
            {
                cerr << "Too many tokens in line: " << endl << line << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
            }
            if ( tokens.size() > 1 )
            {
                s_Roster = trim(tokens[1]);
            }
            if ( s_Roster.empty() )
            {
                cerr << "ROSTER is empty." << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
            }
        }
        else if ( tokens.size() > 0 && trim(tokens[0]) == "COMPRESS" )
        {
            if ( tokens.size() > 2 )
            {
                cerr << "Too many tokens in line: " << endl << line << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
            }
            if ( tokens.size() > 1 )
            {
                int compress = atoi(trim(tokens[1]).c_str());
                if ( 0 == compress )
                {
                    s_Compress = false;
                }
                else
                {
                    s_Compress = true;
                }
            }
            else
            {
                cerr << "Not enough tokens in line: " << endl << line << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
            }
        }
        else if ( tokens.size() > 0 && trim(tokens[0]) == "VERBOSITY" )
        {
            if ( tokens.size() > 2 )
            {
                cerr << "Too many tokens in line: " << endl << line << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
                exit(1);
            }
            if ( tokens.size() > 1 )
            {
                int verbosity = atoi(trim(tokens[1]).c_str());
                if ( 0 == verbosity )
                {
                    util::_dbglogging = false;
                    util::_verboselogging = false;
                }
                else if ( 1 == verbosity )
                {
                    util::_dbglogging = true;
                    util::_verboselogging = false;
                }
                else
                {
                    util::_dbglogging = true;
                    util::_verboselogging = true;
                }
            }
            else
            {
                cerr << "Not enough tokens in line: " << endl << line << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
                exit(1);
            }
        }
        else
        {
            if ( tokens.size() > 2 )
            {
                cerr << "Too many tokens in line: " << endl << line << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
            }
            else if ( tokens.size() > 0 )
            {
                cerr << "Found unknown configuration parameter: " << tokens[0] << endl;
                cerr << "Please fix the configuration file " << CONF_FILE << endl;
            }
        }
    }
    conf_file.close();

    s_Bus = new BusAttachment("XMPPConnector", true);

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

    // Create our XMPP connector
    /* TODO: Support multiple items in roster */
    s_Conn = new XMPPConnector(s_Bus, "XMPP", getJID(), getPassword(), getRoster(), getChatRoom(), getResource(), s_Compress);
    if(ER_OK != s_Conn->Init())
    {
        cout << "Could not initialize XMPPConnector" << endl;
        cleanup();
        return 1;
    }

    // Add SessionPorts to support
    s_Conn->AddSessionPortMatch("org.alljoyn.ControlPanel.ControlPanel", 1000);
    s_Conn->AddSessionPortMatch("org.alljoyn.bus.samples.chat", 27);

    // Register an alert handler
    //AlertReceiver alertReceiver(s_Bus);
    //s_Conn->RegisterMessageHandler(
    //    "** Alert **",
    //    &alertReceiver,
    //    static_cast<XMPPConnector::MessageReceiver::MessageHandler>(&AlertReceiver::AlertHandler),
    //    NULL);

    // Start our XMPP connector (blocking).
    s_Conn->Start();

    cleanup();
}

