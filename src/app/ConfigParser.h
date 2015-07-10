#ifndef CONFIG_PARSER_H_
#define CONFIG_PARSER_H_

#include <string>
#include <vector>
#include <map>
#include <iostream>

class ConfigParser
{
    public:
        ConfigParser( const std::string& filepath );
        ~ConfigParser();
        std::vector<std::string> GetErrors() const;
        std::string GetField(const char* field);
        int SetField(const char* field, const char* value);
        int GetPort();
        int SetPort(int value);
        std::map<std::string, std::string> GetConfigMap();
        std::vector<std::string> GetRoster() const;
        int SetRoster(std::vector<std::string> roster);
        bool isConfigValid();
    private:
        ConfigParser() {} // Private to prevent use
        std::map<std::string, std::string> options;
        mutable std::vector<std::string> errors;
        const std::string configPath;
};

#endif
