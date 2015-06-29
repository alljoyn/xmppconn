#include "app/ConfigParser.h"
#include "common/xmppconnutil.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"

#include <cstdio>
#include <cstdlib>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <sstream>

using namespace std;
using namespace util::str;
using namespace rapidjson;

ConfigParser::ConfigParser( const char* filepath )
{
    // Ensure that we can open our config file
    ifstream conf_file(filepath);
    if ( !conf_file.is_open() )
    {
        stringstream err;
        err << "Could not open " << filepath << "!";
	errors.push_back(err.str());
        return;
    }

    configPath = filepath;
}

ConfigParser::~ConfigParser()
{
}

vector<string> ConfigParser::GetErrors() const
{
    return errors;
}

string ConfigParser::GetField(const char* field)
{
    stringstream err;
    FILE* fp = fopen(configPath, "rb");

    if(!fp){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return NULL; 
    }

    char readBuffer[65536];
    FileReadStream configStream(fp, readBuffer, sizeof(readBuffer));

    Document d;
    d.ParseStream(configStream);

    if(d.HasMember(field)){
        Value& s = d[field];
        fclose(fp);
        return s.GetString();
    }

    err << "Could not find field " << field << "!";
    errors.push_back(err.str());
    fclose(fp);
}
int ConfigParser::SetField(const char* field, const char* value)
{
    stringstream err;
    FILE* fp = fopen(configPath, "rwb");

    if(!fp){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return NULL; 
    }

    char readBuffer[65536];
    FileReadStream configStream(fp, readBuffer, sizeof(readBuffer));

    Document d;
    d.ParseStream(configStream);

    if(!d.HasMember(field)){
        fclose(fp);
        err << "Could not set field " << field << "! NOT FOUND";
        return 1;
    }

    Value& tmp = d[field];

    char writeBuffer[65536];
    FileWriteStream configWriteStream(fp, writeBuffer, sizeof(writeBuffer));
    Writer<FileWriteStream> writer(configWriteStream);

    tmp[field].SetString(value, sizeof(field)/sizeof(field[0]), d.GetAllocator());
    d.Accept(writer);

    fclose(fp);
}

std::map<std::string, std::string> ConfigParser::GetConfigMap(){
    std::map<std::string, std::string> configMap;

    stringstream err;
    FILE* fp = fopen(configPath, "rb");

    if(!fp){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return configMap;
    }

    char readBuffer[65536];
    FileReadStream configStream(fp, readBuffer, sizeof(readBuffer));

    Document d;
    d.ParseStream(configStream);

    if(!d.IsObject()){
       return configMap; 
    }


    for(Value::MemberIterator it = d.MemberBegin(); it != d.MemberEnd(); ++it){
        /*if(it->name.GetString() == "Port"){
          char *intStr = itoa(it->value.GetInt());
          configMap[it->name.GetString()] = string(intStr);
          }
          else{ */
        configMap[it->name.GetString()] = it->value.GetString();
        //}
    }

    fclose(fp);

    return configMap;
}

bool ConfigParser::isValidConfig(){
    map<string, string> configMap = GetConfigMap();

    if(configMap.empty()){
        return false;
    }

    //TODO: Checker for valid XMPP Conf file format
    for(map<string, string>::iterator it = configMap.begin(); it != configMap.end(); ++it){
        if(it->first == "Sever"){

        }
        else if(it->first == "Port"){

        }
        else if(it->first == "UserJID"){

        }
        else if(it->first == "Room"){

        }
        else if(it->first == "Password"){

        }
        else if(it->first == "Roster"){

        }
        else{

        }
    }
    return true;
}
