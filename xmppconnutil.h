// util.h
#ifndef __XMPPCONN_UTIL_H__
#define __XMPPCONN_UTIL_H__
#include <vector>
#include <iostream>
#include <sstream>
#include <cerrno>
#include <list>
#include <stdint.h>
#include <alljoyn/about/AnnouncementRegistrar.h>
#include <qcc/StringUtil.h>
#include <zlib.h>
#include <stdio.h>

const bool _dbglogging = true;
const bool _verboselogging = false;

#define LOG_DEBUG(...) if(_dbglogging|_verboselogging)printf(__VA_ARGS__);
#define LOG_VERBOSE(...) if(_verboselogging)printf(__VA_ARGS__);

using namespace ajn;
using namespace qcc;

using std::string;
using std::vector;
using std::list;
using std::map;
using std::cout;
using std::endl;
using std::istringstream;
using std::ostringstream;

namespace util {

  class FnLog {
    public:
      FnLog(const string& fn_name) : fn_name_(fn_name){
        LOG_DEBUG("__ENTERING_FUNCTION__: %s\n", fn_name_.c_str());
      };
      ~FnLog() {
        LOG_DEBUG("__LEAVING_FUNCTION__: %s\n", fn_name_.c_str());
      }
    private:
      string fn_name_;
  };
  #define FNLOG util::FnLog fnlogger(__FUNCTION__);

namespace str {

    /* Replace all occurances in 'str' of string 'from' with string 'to'. */
    /*static inline
    void
    ReplaceAll(
        string&       str,
        const string& from,
        const string& to
        )
    {
        size_t pos = str.find(from);
        while(pos != string::npos)
        {
            str.replace(pos, from.length(), to);
            pos = str.find(from, pos+to.length());
        }
    }*/

    /* Escape certain characters in an XML string. */
    /*static inline
    void
    EscapeXml(
        string& str
        )
    {
        ReplaceAll(str, "&",  "&amp;");
        ReplaceAll(str, "\"", "&quot;");
        ReplaceAll(str, "'",  "&apos;");
        ReplaceAll(str, "<",  "&lt;");
        ReplaceAll(str, ">",  "&gt;");
    }*/

    /* Unescape the escape sequences in an XML string. */
    /*static inline
    void
    UnescapeXml(
        string& str
        )
    {
        ReplaceAll(str, "&amp;",  "&");
        ReplaceAll(str, "&quot;", "\"");
        ReplaceAll(str, "&apos;", "'");
        ReplaceAll(str, "&lt;",   "<");
        ReplaceAll(str, "&gt;",   ">");
    }*/

    /* Trim the whitespace before and after a string. */
    string
    Trim(
        const string& str
        );

    string
    Compress(
        const string& str
        );

    string
    Decompress(
        const string& str
        );
} // namespace str

namespace msgarg {

    /* Convert a MsgArg into a string in XML format. */
    string
    ToString(
        const MsgArg& arg,
        size_t        indent = 0
        );

    /* Convert a list of MsgArgs into an XML string. */
    string
    ToString(
        const MsgArg* args,
        size_t        numArgs,
        size_t        indent = 0
        );

    /* Convert a string back into a MsgArg. */
    MsgArg
    FromString(
        string argXml
        );

    /* Convert a string into a vector of MsgArgs. */
    vector<MsgArg>
    VectorFromString(
        string content
        );

} // namespace msgarg

namespace bus {

    /* Info needed to make a BusObject */
    struct BusObjectInfo
    {
        string                              path;
        vector<const InterfaceDescription*> interfaces;
    };

    /* Recursively get BusObject information from an attachment. */
    void
    GetBusObjectsRecursive(
        vector<BusObjectInfo>& busObjects,
        ProxyBusObject&        proxy
        );

} // namespace bus
} // namespace util

#endif // __XMPPCONN_UTIL_H__
