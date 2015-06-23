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
        std::string GetField(const std::string& field);
        int SetField(const std::string& field);
    private:
        ConfigParser() {} // Private to prevent use
        std::map<std::string, std::string> options;
        std::vector<std::string> errors;
        std::string configPath;
};
