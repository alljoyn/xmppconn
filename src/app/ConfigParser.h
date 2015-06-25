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
        std::map<std::string, std::string> GetConfigMap();
    private:
        ConfigParser() {} // Private to prevent use
        std::map<std::string, std::string> options;
        std::vector<std::string> errors;
        const char* configPath;
};
