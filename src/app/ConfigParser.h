#include <string>
#include <vector>
#include <map>

class ConfigParser
{
public:
    ConfigParser( const std::string& filepath );
    ~ConfigParser();

    std::vector<std::string> GetErrors() const;

    std::string GetChatRoom() const;
    std::string GetService() const;
    std::string GetServer() const;
    std::string GetUser() const;
    std::string GetPassword() const;
    int         GetVerbosity() const;

private:
    ConfigParser() {} // Private to prevent use
    bool ParseLine( const unsigned int lineno, const std::string& line );

    std::map<std::string, std::string> options;
    std::vector<std::string> errors;
};
