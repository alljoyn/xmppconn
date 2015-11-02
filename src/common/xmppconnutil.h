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
#include <sys/types.h>
#include <sys/syscall.h>

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

const string ALLJOYN_URL_SUFFIX = "org.alljoyn.XmppConnector";
const string ALLJOYN_XMPP_SUFFIX = "XmppConnector";
const string ALLJOYN_XMPP_CONFIG_INTERFACE_NAME = "org.alljoyn.Config." + ALLJOYN_XMPP_SUFFIX;
const string ALLJOYN_XMPP_CONFIG_PATH = "/Config/XmppConnector";

namespace util {

extern volatile bool _dbglogging;
extern volatile bool _verboselogging;

#define LOG_RELEASE(fmt, ...) fprintf(stderr, "0x%08x - "fmt"\n",(unsigned int)syscall(SYS_gettid),##__VA_ARGS__);
#define LOG_DEBUG(fmt, ...) if(util::_dbglogging|util::_verboselogging)printf("0x%08x - "fmt"\n",(unsigned int)syscall(SYS_gettid),##__VA_ARGS__);
#define LOG_VERBOSE(fmt,...) if(util::_verboselogging)printf("0x%08x - "fmt"\n",(unsigned int)syscall(SYS_gettid),##__VA_ARGS__);

  class FnLog {
    public:
      FnLog(const string& fn_name) : fn_name_(fn_name){
        LOG_VERBOSE("__ENTERING_FUNCTION__: %s", fn_name_.c_str());
      };
      ~FnLog() {
        LOG_VERBOSE("__LEAVING_FUNCTION__: %s", fn_name_.c_str());
      }
    private:
      string fn_name_;
  };
  #define FNLOG util::FnLog fnlogger(__FUNCTION__);

namespace str {

    /* Replace all occurances in 'str' of string 'from' with string 'to'. */
    void
    ReplaceAll(
        string&       str,
        const string& from,
        const string& to
        );

    /* Escape certain characters in an XML string. */
    void
    EscapeXml(
        string& str
        );

    /* Unescape the escape sequences in an XML string. */
    void
    UnescapeXml(
        string& str
        );

    /* Trim the whitespace before and after a string. */
    string
    Trim(
        const string& str
        );

    std::vector<std::string>
    Split(
        const std::string &str,
        char delim
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

    /**
     * %GetBusObjectsAsyncReceiver is a pure-virtual base class that is
     * implemented by any class that calls GetBusObjectsAsync.
     */
    class GetBusObjectsAsyncReceiver
    {
      public:
        /** Destructor */
        virtual ~GetBusObjectsAsyncReceiver() { }

        /**
         * CallbackHandlers are %GetBusObjectsAsyncReceiver methods which
         * are called by GetBusObjectsAsync upon completion.
         *
         * @param proxy         Proxy bus object.
         * @param busObjects    The returned child bus objects.
         * @param context       Opaque context pointer that was passed in
         *                      to GetBusObjectsAsync
         */
        typedef void (GetBusObjectsAsyncReceiver::* CallbackHandler)(
            ProxyBusObject*                     proxy,
            vector<util::bus::BusObjectInfo>    busObjects,
            void*                               context
        );

      protected:
        void GetBusObjectsAsyncIntrospectCallback(
            QStatus status,
            ProxyBusObject* obj,
            void* context
        );
    };

    class GetBusObjectsAsyncIntrospectReceiver :
        public ProxyBusObject::Listener
    {
      public:
        /** Destructor */
        virtual ~GetBusObjectsAsyncIntrospectReceiver() { }

        void GetBusObjectsAsyncIntrospectCallback(
            QStatus status,
            ProxyBusObject* obj,
            void* context
        );
    };


    /* Recursively get BusObject information from an attachment. */
    void
    GetBusObjectsRecursive(
        vector<BusObjectInfo>& busObjects,
        ProxyBusObject&        proxy
        );

    /* Asynchronously get BusObject information from an attachment. */
    void
    GetBusObjectsAsync(
        ProxyBusObject*                                proxy,
        GetBusObjectsAsyncReceiver*                    receiver,
        GetBusObjectsAsyncReceiver::CallbackHandler    handler,
        void*                                          context
        );

} // namespace bus

    string generateAppId();

    // Helper function to deal with converting C char array to std::string safely
    inline string convertToString(const char* s)
    {
        return (s ? string(s) : string(""));
    }


} // namespace util

#endif // __XMPPCONN_UTIL_H__
