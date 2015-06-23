#include "ConfigParser.h"
#include "common/xmppconnutil.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"

#include <cstdio>
#include <stdlib.h>
#include <fstream>
#include <sstream>

using namespace std;
using namespace util::str;
using namespace rapidjson;

ConfigParser::ConfigParser( const string& filepath )
{
    // Ensure that we can open our config file
    ifstream conf_file(filepath.c_str());
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

string ConfigParser::GetField(const string& field)
{
    stringstream err;
    FILE* fp = fopen(configPath.c_str(), "rb");

    char readBuffer[65536];
    FileReadStream configStream(fp, readBuffer, sizeof(readBuffer));

    Document d;
    d.ParseStream(configStream);

    if(d.HasMember(field.c_str())){
        Value& s = d[field.c_str()];
        fclose(fp);
        return s.GetString();
    }

    err << "Could not find field " << field << "!";
    errors.push_back(err.str());
    fclose(fp);
}
int ConfigParser::SetField(const string& field)
{
    stringstream err;
    FILE* fp = fopen(configPath.c_str(), "rwb");

    char readBuffer[65536];
    FileReadStream configStream(fp, readBuffer, sizeof(readBuffer));

    Document d;
    d.ParseStream(configStream);
    Value& tmp = d[field.c_str()];

    char writeBuffer[65536];
    FileWriteStream configWriteStream(fp, writeBuffer, sizeof(writeBuffer));
    Writer<FileWriteStream> writer(configWriteStream);

    if(d.HasMember(field.c_str())){
        tmp.SetString(field.c_str(),field.length(), d.GetAllocator());
        d.Accept(writer);
        fclose(fp);
        return 0;
    }

    err << "Could not set field" << field << "!";
    errors.push_back(err.str());
    fclose(fp);
}

