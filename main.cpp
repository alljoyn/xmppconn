#include <alljoyn/BusAttachment.h>
#include "XMPPConnector.h"
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

static BusAttachment* s_Bus = 0;
static XMPPConnector* s_Conn = 0;
const string CONF_FILE = "/etc/XMPPConnector/XMPPConnector.conf";
static string s_Server = "swiftnet.acs.affinegy.com";
static string s_User = "alljoyn";
static string s_ChatRoom;

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
}

string getJID()
{
    return s_User + "@" + s_Server;
}

string getChatRoom()
{
    return s_User + "@" + s_ChatRoom + "." + s_Server;
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
        if ( tokens.size() == 2 && trim(tokens[0]) == "CHATROOM" )
        {
            s_ChatRoom = trim(tokens[1]);
            if ( s_ChatRoom.empty() )
            {
                cerr << "CHATROOM cannot be specified as a blank value." << endl;
                exit(1);
            }
        }
        else
        {
            if ( tokens.size() > 2 )
            {
                cerr << "Too many tokens in line: " << endl << line << endl;
            }
            else if ( tokens.size() > 0 )
            {
                cerr << "Found unknown configuration parameter: " << tokens[0] << endl;
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
    s_Conn = new XMPPConnector(s_Bus, "XMPP", getJID(), s_User, getChatRoom());
    s_Conn->Start();

    cleanup();
}

