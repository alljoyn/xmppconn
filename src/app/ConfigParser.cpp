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

    char readBuffer[65536];
    FileReadStream configStream(fp, readBuffer, sizeof(readBuffer));

    Document d;
    d.ParseStream(configStream);
    Value& tmp = d[field];

    char writeBuffer[65536];
    FileWriteStream configWriteStream(fp, writeBuffer, sizeof(writeBuffer));
    Writer<FileWriteStream> writer(configWriteStream);

    if(d.HasMember(field)){
        tmp[field].SetString(value, sizeof(field)/sizeof(field[0]), d.GetAllocator());
        d.Accept(writer);
        fclose(fp);
        return 0;
    }

    err << "Could not set field" << field << "!";
    errors.push_back(err.str());
    fclose(fp);
}
