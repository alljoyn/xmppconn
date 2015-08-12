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

#include "app/ConfigParser.h"
#include "common/xmppconnutil.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"

#include <cstdio>
#include <cstdlib>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fstream>

using namespace std;
using namespace util::str;
using namespace rapidjson;

static const size_t CONFIGPARSER_BUF_SIZE = 2048;

ConfigParser::ConfigParser( const std::string& filepath ) : configPath(filepath)
{
    // Ensure that we can open our config file
    ifstream conf_file(configPath.c_str());
    if ( !conf_file.is_open() )
    {
        stringstream err;
        err << "Could not open " << filepath << "!";
        errors.push_back(err.str());
    }
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
    FILE* fp = fopen(configPath.c_str(), "rb");

    if(!fp){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return ""; 
    }

    char readBuffer[CONFIGPARSER_BUF_SIZE] = {};
    FileReadStream configStream(fp, readBuffer, sizeof(readBuffer));

    Document d;
    d.ParseStream(configStream);

    if(d.HasMember(field)){
        Value& s = d[field];
        string value;
        if ( s.IsString() )
        {
            value = s.GetString();
        }
        fclose(fp);
        return value;
    }

    err << "Could not find field " << field << "!";
    errors.push_back(err.str());
    fclose(fp);

    return "";
}
vector<string> ConfigParser::GetRoster() const
{
    vector<string> roster;
    stringstream err;
    FILE* fp = fopen(configPath.c_str(), "rb");

    if(!fp){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return roster; 
    }

    char readBuffer[CONFIGPARSER_BUF_SIZE] = {};
    FileReadStream configStream(fp, readBuffer, sizeof(readBuffer));

    Document d;
    d.ParseStream(configStream);

    if(d.HasMember("Roster") && d["Roster"].IsArray()){
        fclose(fp);

        const Value& s = d["Roster"];
        for(SizeType i = 0; i < s.Size(); i++){
            roster.push_back( s[i].GetString() );
        }

        return roster;
    }

    err << "Could not find field Roster!";
    errors.push_back(err.str());
    fclose(fp);

    return roster; 
    
}
int ConfigParser::SetRoster(vector<string> roster)
{
    stringstream err;
    FILE* fpRead = fopen(configPath.c_str(), "rb");

    if(!fpRead){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return -1; 
    }

    char readBuffer[CONFIGPARSER_BUF_SIZE] = {};
    FileReadStream configStream(fpRead, readBuffer, sizeof(readBuffer));

    Document d;
    d.ParseStream(configStream);

    if(!d.HasMember("Roster")){
        Value::AllocatorType& a(d.GetAllocator());
        d.AddMember("Roster", "", a);
    }

    Value& tmpArray = d["Roster"];
    tmpArray.SetArray();
    Value newValue;
    for( vector<string>::const_iterator it(roster.begin());
        roster.end() != it; ++it )
    {
        newValue.SetString(it->c_str(), it->size());
        tmpArray.PushBack(newValue, d.GetAllocator());
    }
    fclose(fpRead);

    FILE* fpWrite = fopen(configPath.c_str(), "wb");
    if(!fpWrite){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return -1; 
    }

    char writeBuffer[CONFIGPARSER_BUF_SIZE] = {};
    FileWriteStream configWriteStream(fpWrite, writeBuffer, sizeof(writeBuffer));
    PrettyWriter<FileWriteStream> writer(configWriteStream);
    d.Accept(writer);

    fclose(fpWrite);
}
int ConfigParser::GetPort()
{
    stringstream err;
    FILE* fp = fopen(configPath.c_str(), "rb");

    if(!fp){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return -1;
    }

    char readBuffer[CONFIGPARSER_BUF_SIZE] = {};
    FileReadStream configStream(fp, readBuffer, sizeof(readBuffer));

    Document d;
    d.ParseStream(configStream);

    if(d.HasMember("Port")){
        Value& s = d["Port"];
        fclose(fp);
        return s.GetInt();
    }

    err << "Could not find field [Port]!";
    errors.push_back(err.str());
    fclose(fp);

    return -1; 
}
int ConfigParser::SetPort(int value)
{
    stringstream err;
    FILE* fpRead = fopen(configPath.c_str(), "rb");

    if(!fpRead){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return -1; 
    }

    char readBuffer[CONFIGPARSER_BUF_SIZE] = {};
    FileReadStream configStream(fpRead, readBuffer, sizeof(readBuffer));

    Document d;
    d.ParseStream(configStream);

    if(!d.HasMember("Port")){
        fclose(fpRead);
        err << "Could not set field Port! NOT FOUND";
        return 1;
    }

    Value& tmp = d["Port"];
    fclose(fpRead);

    FILE* fpWrite = fopen(configPath.c_str(), "wb");
    if(!fpWrite){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return -1; 
    }

    char writeBuffer[CONFIGPARSER_BUF_SIZE] = {};
    FileWriteStream configWriteStream(fpWrite, writeBuffer, sizeof(writeBuffer));
    PrettyWriter<FileWriteStream> writer(configWriteStream);

    tmp.SetInt(value);
    d.Accept(writer);

    std::ofstream of(configPath.c_str());
    of << writeBuffer;

    fclose(fpWrite);
}

int ConfigParser::SetField(const char* field, const char* value)
{
    stringstream err;
    FILE* fpRead = fopen(configPath.c_str(), "rb");

    if(!fpRead){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return -1; 
    }
    char readBuffer[CONFIGPARSER_BUF_SIZE] = {};
    FileReadStream configStream(fpRead, readBuffer, sizeof(readBuffer));

    Document d;
    d.ParseStream(configStream);

    if(d.HasMember(field)){
        Value& tmp(d[field]);
        tmp.SetString(StringRef(value));
    }
    else
    {
        Value::AllocatorType& a(d.GetAllocator());
        d.AddMember(StringRef(field), StringRef(value), a);
        if(!d.HasMember(field)){
            fclose(fpRead);
            err << "Failed to set field '" << field << "'' in config file " << configPath << "!";
            errors.push_back(err.str());
            return -1; 
        }
    }
    fclose(fpRead);

    FILE* fpWrite = fopen(configPath.c_str(), "wb");
    if(!fpWrite){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return -1; 
    }

    char writeBuffer[CONFIGPARSER_BUF_SIZE] = {};
    FileWriteStream configWriteStream(fpWrite, writeBuffer, sizeof(writeBuffer));
    PrettyWriter<FileWriteStream> writer(configWriteStream);
    d.Accept(writer);

    fclose(fpWrite);
}

std::map<std::string, std::string> ConfigParser::GetConfigMap()
{
    std::map<std::string, std::string> configMap;

    stringstream err;
    FILE* fp = fopen(configPath.c_str(), "rb");

    if(!fp){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return configMap;
    }

    char readBuffer[CONFIGPARSER_BUF_SIZE] = {};
    FileReadStream configStream(fp, readBuffer, sizeof(readBuffer));

    Document d;
    d.ParseStream(configStream);

    if(!d.IsObject()){
        return configMap; 
    }


    for(Value::MemberIterator it = d.MemberBegin(); it != d.MemberEnd(); ++it){
        if ( !it->name.IsString() )
        {
            LOG_RELEASE("Warning: JSON key value is not a string.");
            continue;
        }
        if(strcmp(it->name.GetString(),"Port") == 0){
            configMap[it->name.GetString()] = "";
        }
        else if(strcmp(it->name.GetString(),"Roster") == 0){
            configMap[it->name.GetString()] = "";
        }
        else{
            // Ignore non-string
            if ( it->value.IsString() )
            {
                configMap[it->name.GetString()] = it->value.GetString();
            }
        }
    }
    fclose(fp);

    return configMap;
}

bool ConfigParser::isConfigValid()
{
    unsigned short foundRequiredCount = 0;

    map<string, string> configMap = GetConfigMap();
    if(configMap.empty()){
        return false;
    }

    //TODO: Checker for valid XMPP Conf file format
    for(map<string, string>::iterator it = configMap.begin(); it != configMap.end(); ++it){
        if(it->first == "ProductID" ||
           it->first == "SerialNumber"){
            foundRequiredCount++;
        }
        else if(it->first == "Server" ||
                it->first == "UserJID" ||
                it->first == "UserPassword" ||
                it->first == "Roster" ||
                it->first == "RoomJID" ||
                it->first == "Compress" ||
                it->first == "Verbosity" ||
                it->first == "Port" ||
                it->first == "AppId" ||
                it->first == "AllJoynPasscode" ||
                it->first == "DeviceName" ||
                it->first == "AppName" ||
                it->first == "Manufacturer" ||
                it->first == "ModelNumber" ||
                it->first == "Description" ||
                it->first == "DateOfManufacture" ||
                it->first == "SoftwareVersion" ||
                it->first == "HardwareVersion" ||
                it->first == "SupportUrl"){
            // noop
        }
        else{
            LOG_RELEASE("Invalid Field Found: %s", it->first.c_str());
            return false;
        }
    }

    if(foundRequiredCount < 2){
        LOG_RELEASE("Missing required fields!");
    }

    return true;
}
