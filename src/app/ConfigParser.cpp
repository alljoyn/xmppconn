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
#include <sstream>

using namespace std;
using namespace util::str;
using namespace rapidjson;

// Possible approach to validation
// Map with keys to all possible fields with
// values of valid regex.

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
        return ""; 
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
    return "";
}
char** ConfigParser::GetRosters(){
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

    if(d.HasMember("Roster") && d["Roster"].IsArray()){
        const Value& s = d["Roster"];
        fclose(fp);
        char** tmp = new char*[s.Size()]; 
        for(SizeType i = 0; i < s.Size(); i++){
            const char* tmpString = s[i].GetString();
            tmp[i] = new char[strlen(tmpString)];
            std::strcpy(tmp[i], tmpString); 
        }
        return tmp;
    }

    err << "Could not find field Roster!";
    errors.push_back(err.str());
    fclose(fp);
    return NULL; 
    
}
int ConfigParser::SetRosters(char** value, size_t numRosters){
    stringstream err;
    FILE* fpRead = fopen(configPath, "rb");

    if(!fpRead){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return -1; 
    }

    char readBuffer[65536];
    FileReadStream configStream(fpRead, readBuffer, sizeof(readBuffer));

    Document d;
    d.ParseStream(configStream);

    if(!d.HasMember("Roster")){
        fclose(fpRead);
        err << "Could not set field Roster! NOT FOUND";
        return 1;
    }

    Value& tmp = d["Roster"];
    fclose(fpRead);


    FILE* fpWrite = fopen(configPath, "wb");

    if(!fpWrite){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return NULL; 
    }

    char writeBuffer[65536];
    FileWriteStream configWriteStream(fpWrite, writeBuffer, sizeof(writeBuffer));
    PrettyWriter<FileWriteStream> writer(configWriteStream);

    Value& tmpArray = d["Roster"];
    tmpArray.SetArray();
    Value newValue;
    for(int i = 0; i < numRosters; i++){
        newValue.SetString(value[i], strlen(value[i]));
        tmpArray.PushBack(newValue, d.GetAllocator());

    }
    d.Accept(writer);

    std::ofstream of(configPath);
    of << writeBuffer;

    fclose(fpWrite);

}
int ConfigParser::GetPort(){
    stringstream err;
    FILE* fp = fopen(configPath, "rb");

    if(!fp){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return -1;
    }

    char readBuffer[65536];
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
int ConfigParser::SetPort(int value){
    stringstream err;
    FILE* fpRead = fopen(configPath, "rb");

    if(!fpRead){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return -1; 
    }

    char readBuffer[65536];
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


    FILE* fpWrite = fopen(configPath, "wb");

    if(!fpWrite){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return NULL; 
    }

    char writeBuffer[65536];
    FileWriteStream configWriteStream(fpWrite, writeBuffer, sizeof(writeBuffer));
    PrettyWriter<FileWriteStream> writer(configWriteStream);

    tmp.SetInt(value);
    d.Accept(writer);

    std::ofstream of(configPath);
    of << writeBuffer;

    fclose(fpWrite);

}
int ConfigParser::SetField(const char* field, const char* value)
{
    stringstream err;
    FILE* fpRead = fopen(configPath, "rb");

    if(!fpRead){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return NULL; 
    }

    char readBuffer[65536];
    FileReadStream configStream(fpRead, readBuffer, sizeof(readBuffer));

    Document d;
    d.ParseStream(configStream);

    if(!d.HasMember(field)){
        fclose(fpRead);
        err << "Could not set field " << field << "! NOT FOUND";
        return 1;
    }

    Value& tmp = d[field];
    fclose(fpRead);


    FILE* fpWrite = fopen(configPath, "wb");

    if(!fpWrite){
        err << "Could not open file " << configPath << "!";
        errors.push_back(err.str());
        return NULL; 
    }

    char writeBuffer[65536];
    FileWriteStream configWriteStream(fpWrite, writeBuffer, sizeof(writeBuffer));
    PrettyWriter<FileWriteStream> writer(configWriteStream);

    tmp.SetString(value, strlen(value));
    d.Accept(writer);

    std::ofstream of(configPath);
    of << writeBuffer;

    fclose(fpWrite);
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
        if(strcmp(it->name.GetString(),"Port")){
            configMap[it->name.GetString()] = "";
        }
        else if(strcmp(it->name.GetString(),"Roster")){
            configMap[it->name.GetString()] = "";
        }
        else{ 
            configMap[it->name.GetString()] = it->value.GetString();
        }
    }

    fclose(fp);

    return configMap;
}

bool ConfigParser::isConfigValid(){
    map<string, string> configMap = GetConfigMap();
    unsigned short foundRequiredCount = 0;

    if(configMap.empty()){
        return false;
    }

    //TODO: Checker for valid XMPP Conf file format
    for(map<string, string>::iterator it = configMap.begin(); it != configMap.end(); ++it){
        if(it->first == "Server"){
            foundRequiredCount++;

        }
        else if(it->first == "Port"){
        }
        else if(it->first == "UserJID"){
            foundRequiredCount++;
        }
        else if(it->first == "Room"){

        }
        else if(it->first == "Password"){
            foundRequiredCount++;
        }
        else if(it->first == "Roster"){
            foundRequiredCount++;

        }
        else if(it->first == "Compress"){

        }
        else if(it->first == "Verbosity"){

        }
        else if(it->first == "Resource"){
            foundRequiredCount++;

        }
        else if(it->first == "ProductID"){

        }
        else if(it->first == "SerialNumber"){

        }
        else{
            std::cout << "Invalid Field Found: " << it->first << std::endl;
            return false;
        }
    }

    if(foundRequiredCount < 5){
        std::cout << "Missing required fields!" << std::endl;
    }
    return true;
}
