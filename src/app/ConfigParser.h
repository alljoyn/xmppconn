#ifndef CONFIG_PARSER_H_
#define CONFIG_PARSER_H_

#include <string>
#include <vector>
#include <map>
#include <iostream>

class ConfigParser
{
    public:
        ConfigParser( const char* filepath );
        ~ConfigParser();
        std::vector<std::string> GetErrors() const;
        std::string GetField(const char* field);
        int SetField(const char* field, const char* value);
        int GetPort();
        int SetPort(int value);
        std::map<std::string, std::string> GetConfigMap();
        char** GetRosters();
        int SetRosters(char** value, size_t numRosters);
        bool isConfigValid();
    private:
        ConfigParser() {} // Private to prevent use
        std::map<std::string, std::string> options;
        std::vector<std::string> errors;
        const char* configPath;
};

#endif
