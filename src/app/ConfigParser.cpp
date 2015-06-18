#include "ConfigParser.h"
#include "common/xmppconnutil.h"
#include <stdlib.h>
#include <fstream>
#include <sstream>

using namespace std;
using namespace util::str;

ConfigParser::ConfigParser( const string& filepath )
{
    // Read in the configuration file
    ifstream conf_file(filepath.c_str());
    if ( !conf_file.is_open() )
    {
        stringstream err;
        err << "Could not open " << filepath << "!";
	errors.push_back(err.str());
        return;
    }
    string line;
    unsigned int lineno = 1;
    while ( getline( conf_file, line ) && ParseLine( lineno++, line ) );
}

ConfigParser::~ConfigParser()
{
}

vector<string> ConfigParser::GetErrors() const
{
    return errors;
}

string ConfigParser::GetChatRoom() const
{
    return options.at("CHATROOM");
}

string ConfigParser::GetService() const
{
    return options.at("SERVICE");
}

string ConfigParser::GetServer() const
{
    return options.at("SERVER");
}

string ConfigParser::GetUser() const
{
    return options.at("USER");
}

string ConfigParser::GetPassword() const
{
    return options.at("PASSWORD");
}

int ConfigParser::GetVerbosity() const
{
    return atoi(Trim(options.at("VERBOSITY")).c_str());
}

bool ConfigParser::ParseLine( const unsigned int lineno, const string& line )
{
    stringstream err;
    vector<string> tokens = Split(line, '=');
    if ( tokens.size() > 0 )
    {
        if ( tokens.size() > 2 )
        {
            err << "Too many tokens in line " << lineno << ": " << line;
            errors.push_back(err.str());
        }
        string key( Trim(tokens[0]) );
        if ( tokens.size() > 1 )
        {
            string value( Trim(tokens[1]) );
            options[key] = value;
        }
        if ( options.at(key).empty() )
        {
            err << key << " cannot specified as a blank value.";
            return false;
        }
    }
    else
    {
        err << "Not enough tokens in line " << lineno << ": " << line;
        errors.push_back(err.str());
    }

    return true;
}

