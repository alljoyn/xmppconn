#include "XMPPConnector.h"                                                      // TODO: internal documentation
#include <vector>
#include <iostream>
#include <sstream>
#include <cerrno>
#include <stdint.h>
#include <pthread.h>
#include <strophe.h>

#include <alljoyn/about/AnnouncementRegistrar.h>
#include <qcc/StringUtil.h>

#include <zlib.h>


using namespace ajn;
using namespace ajn::gw;
using namespace ajn::services;
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
    static inline
    string
    Trim(
        const string& str
        )
    {
        return qcc::Trim(str.c_str()).c_str();
    }

    static
    string
    Compress(
        const string& str
        )
    {
        z_stream zs;
        memset(&zs, 0, sizeof(zs));

        if(deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK)
        {
            return str;
        }

        zs.next_in = (Bytef*)str.data();
        zs.avail_in = str.size();

        int ret;
        uint8_t outBuffer[32768];
        string byteString;
        do
        {
            zs.next_out = reinterpret_cast<Bytef*>(outBuffer);
            zs.avail_out = sizeof(outBuffer);

            ret = deflate(&zs, Z_FINISH);

            if(byteString.size() < zs.total_out)
            {
                byteString.append(
                        reinterpret_cast<char*>(outBuffer),
                        zs.total_out-byteString.size());
            }
        } while(ret == Z_OK);

        deflateEnd(&zs);

        if(ret != Z_STREAM_END)
        {
            return str;
        }

        return BytesToHexString(
                reinterpret_cast<const uint8_t*>(byteString.data()),
                byteString.size()).c_str();
    }

    static
    string
    Decompress(
        const string& str
        )
    {
        size_t numBytes = str.length()/2;
        uint8_t* bytes = new uint8_t[numBytes];
        if(numBytes != HexStringToBytes(str.c_str(), bytes, numBytes))
        {
            return str;
        }

        z_stream zs;
        memset(&zs, 0, sizeof(zs));

        if(inflateInit(&zs) != Z_OK)
        {
            return str;
        }

        zs.next_in = (Bytef*)bytes;
        zs.avail_in = numBytes;

        int ret;
        char outBuffer[32768];
        string outString;
        do
        {
            zs.next_out = reinterpret_cast<Bytef*>(outBuffer);
            zs.avail_out = sizeof(outBuffer);

            ret = inflate(&zs, 0);

            if(outString.size() < zs.total_out)
            {
                outString.append(outBuffer, zs.total_out-outString.size());
            }
        } while(ret == Z_OK);

        inflateEnd(&zs);

        if(ret != Z_STREAM_END)
        {
            return str;
        }

        return outString;
    }

} // namespace str

namespace msgarg {

    /* Convert a MsgArg into a string in XML format. */
    static
    string
    ToString(
        const MsgArg& arg,
        size_t        indent = 0
        )
    {
        string str;
    #define CHK_STR(s)  string((((s) == NULL) ? "" : (s)))
        string in(indent, ' ');

        str = in;

        indent += 2;

        switch (arg.typeId) {
        case ALLJOYN_ARRAY:
        {
            const MsgArg* elems = arg.v_array.GetElements();
            str += "<array type_sig=\"" +
                    CHK_STR(arg.v_array.GetElemSig()) + "\">";
            for (uint32_t i = 0; i < arg.v_array.GetNumElements(); i++)
            {
                str += "\n" + ToString(elems[i], indent);
            }
            str += "\n" + in + "</array>";
            break;
        }

        case ALLJOYN_BOOLEAN:
            str += arg.v_bool ? "<boolean>1</boolean>" : "<boolean>0</boolean>";
            break;

        case ALLJOYN_DOUBLE:
            // To be bit-exact stringify double as a 64-bit hex value
            str += "<double>"
                    "0x" + string(U64ToString(arg.v_uint64, 16).c_str()) +
                    "</double>";
            break;

        case ALLJOYN_DICT_ENTRY:
            str += "<dict_entry>\n" +
                   ToString(*arg.v_dictEntry.key, indent) + "\n" +
                   ToString(*arg.v_dictEntry.val, indent) + "\n" +
                   in + "</dict_entry>";
            break;

        case ALLJOYN_SIGNATURE:
            str += "<signature>" +
                    CHK_STR(arg.v_signature.sig) + "</signature>";
            break;

        case ALLJOYN_INT32:
            str += "<int32>" + string(I32ToString(arg.v_int32).c_str()) +
                    "</int32>";
            break;

        case ALLJOYN_INT16:
            str += "<int16>" + string(I32ToString(arg.v_int16).c_str()) +
                    "</int16>";
            break;

        case ALLJOYN_OBJECT_PATH:
            str += "<object_path>" +
                CHK_STR(arg.v_objPath.str) + "</object_path>";
            break;

        case ALLJOYN_UINT16:
            str += "<uint16>" + string(U32ToString(arg.v_uint16).c_str()) +
                    "</uint16>";
            break;

        case ALLJOYN_STRUCT:
            str += "<struct>\n";
            for (uint32_t i = 0; i < arg.v_struct.numMembers; i++)
            {
                str += ToString(arg.v_struct.members[i], indent) + "\n";
            }
            str += in + "</struct>";
            break;

        case ALLJOYN_STRING:
            str += "<string>" + CHK_STR(arg.v_string.str) + "</string>";
            break;

        case ALLJOYN_UINT64:
            str += "<uint64>" + string(U64ToString(arg.v_uint64).c_str()) +
                    "</uint64>";
            break;

        case ALLJOYN_UINT32:
            str += "<uint32>" + string(U32ToString(arg.v_uint32).c_str()) +
                    "</uint32>";
            break;

        case ALLJOYN_VARIANT:
            str += "<variant signature=\"" +
                    string(arg.v_variant.val->Signature().c_str()) + "\">\n";
            str += ToString(*arg.v_variant.val, indent);
            str += "\n" + in + "</variant>";
            break;

        case ALLJOYN_INT64:
            str += "<int64>" + string(I64ToString(arg.v_int64).c_str()) +
                    "</int64>";
            break;

        case ALLJOYN_BYTE:
            str += "<byte>" + string(U32ToString(arg.v_byte).c_str()) +
                    "</byte>";
            break;

        case ALLJOYN_HANDLE:
            str += "<handle>" +
                    string(BytesToHexString(
                    reinterpret_cast<const uint8_t*>(&arg.v_handle.fd),
                    sizeof(arg.v_handle.fd)).c_str()) + "</handle>";
            break;

        case ALLJOYN_BOOLEAN_ARRAY:
            str += "<array type=\"boolean\">";
            if (arg.v_scalarArray.numElements)
            {
                str += "\n" + string(indent, ' ');
                for (uint32_t i = 0; i < arg.v_scalarArray.numElements; i++)
                {
                    str += arg.v_scalarArray.v_bool[i] ? "1 " : "0 ";
                }
            }
            str += "\n" + in + "</array>";
            break;

        case ALLJOYN_DOUBLE_ARRAY:
            str += "<array type=\"double\">";
            if (arg.v_scalarArray.numElements)
            {
                str += "\n" + string(indent, ' ');
                for (uint32_t i = 0; i < arg.v_scalarArray.numElements; i++)
                {
                    if(sizeof(double) == sizeof(uint64_t))
                    {
                        // To be bit-exact stringify double as 64-bit hex
                        str += "0x" + string(U64ToString(
                                *reinterpret_cast<const uint64_t*>(
                                &arg.v_scalarArray.v_double[i]), 16).c_str()) +
                                " ";
                    }
                    else
                    {
                        str += string(U64ToString(static_cast<uint64_t>(
                                arg.v_scalarArray.v_double[i])).c_str()) + " ";
                    }
                }
            }
            str += "\n" + in + "</array>";
            break;

        case ALLJOYN_INT32_ARRAY:
            str += "<array type=\"int32\">";
            if (arg.v_scalarArray.numElements)
            {
                str += "\n" + string(indent, ' ');
                for (uint32_t i = 0; i < arg.v_scalarArray.numElements; i++)
                {
                    str += string(I32ToString(
                            arg.v_scalarArray.v_int32[i]).c_str()) + " ";
                }
            }
            str += "\n" + in + "</array>";
            break;

        case ALLJOYN_INT16_ARRAY:
            str += "<array type=\"int16\">";
            if (arg.v_scalarArray.numElements)
            {
                str += "\n" + string(indent, ' ');
                for (uint32_t i = 0; i < arg.v_scalarArray.numElements; i++)
                {
                    str += string(I32ToString(
                            arg.v_scalarArray.v_int16[i]).c_str()) + " ";
                }
            }
            str += "\n" + in + "</array>";
            break;

        case ALLJOYN_UINT16_ARRAY:
            str += "<array type=\"uint16\">";
            if (arg.v_scalarArray.numElements)
            {
                str += "\n" + string(indent, ' ');
                for (uint32_t i = 0; i < arg.v_scalarArray.numElements; i++)
                {
                    str += string(U32ToString(
                            arg.v_scalarArray.v_uint16[i]).c_str()) + " ";
                }
            }
            str += "\n" + in + "</array>";
            break;

        case ALLJOYN_UINT64_ARRAY:
            str += "<array type=\"uint64\">";
            if (arg.v_scalarArray.numElements)
            {
                str += "\n" + string(indent, ' ');
                for (uint32_t i = 0; i < arg.v_scalarArray.numElements; i++)
                {
                    str += string(U64ToString(
                            arg.v_scalarArray.v_uint64[i]).c_str()) + " ";
                }
            }
            str += "\n" + in + "</array>";
            break;

        case ALLJOYN_UINT32_ARRAY:
            str += "<array type=\"uint32\">";
            if (arg.v_scalarArray.numElements)
            {
                str += "\n" + string(indent, ' ');
                for (uint32_t i = 0; i < arg.v_scalarArray.numElements; i++)
                {
                    str += string(U32ToString(
                            arg.v_scalarArray.v_uint32[i]).c_str()) + " ";
                }
            }
            str += "\n" + in + "</array>";
            break;

        case ALLJOYN_INT64_ARRAY:
            str += "<array type=\"int64\">";
            if (arg.v_scalarArray.numElements)
            {
                str += "\n" + string(indent, ' ');
                for (uint32_t i = 0; i < arg.v_scalarArray.numElements; i++)
                {
                    str += string(I64ToString(
                            arg.v_scalarArray.v_int64[i]).c_str()) + " ";
                }
            }
            str += "\n" + in + "</array>";
            break;

        case ALLJOYN_BYTE_ARRAY:
            str += "<array type=\"byte\">";
            if (arg.v_scalarArray.numElements)
            {
                str += "\n" + string(indent, ' ');
                for (uint32_t i = 0; i < arg.v_scalarArray.numElements; i++)
                {
                    str += string(U32ToString(
                            arg.v_scalarArray.v_byte[i]).c_str()) + " ";
                }
            }
            str += "\n" + in + "</array>";
            break;

        default:
            str += "<invalid/>";
            break;
        }
    #undef CHK_STR
        return str;
    }

    /* Convert a list of MsgArgs into an XML string. */
    static
    string
    ToString(
        const MsgArg* args,
        size_t        numArgs,
        size_t        indent = 0
        )
    {
        string outStr;
        for (uint32_t i = 0; i < numArgs; ++i)
        {
            outStr += ToString(args[i], indent) + '\n';
        }
        return outStr;
    }

    // Forward declaration
    static vector<MsgArg>
    VectorFromString(
        string content
        );

    /* Convert a string back into a MsgArg. */
    static
    MsgArg
    FromString(
        string argXml
        )
    {
        using str::Trim;

        MsgArg result;

        QStatus status = ER_OK;
        size_t pos = argXml.find_first_of('>')+1;
        string typeTag = Trim(argXml.substr(0, pos));
        string content = argXml.substr(pos, argXml.find_last_of('<')-pos);

        if(0 == typeTag.find("<array type_sig="))
        {
            vector<MsgArg> array = VectorFromString(content);
            status = result.Set("a*", array.size(), &array[0]);
            result.Stabilize();
        }
        else if(typeTag == "<boolean>")
        {
            status = result.Set("b", content == "1");
        }
        else if(typeTag == "<double>")
        {
            status = result.Set("d", StringToU64(content.c_str(), 16));
        }
        else if(typeTag == "<dict_entry>")
        {
            vector<MsgArg> array = VectorFromString(content);
            if(array.size() != 2)
            {
                status = ER_BUS_BAD_VALUE;
            }
            else
            {
                status = result.Set("{**}", &array[0], &array[1]);
                result.Stabilize();
            }
        }
        else if(typeTag == "<signature>")
        {
            status = result.Set("g", content.c_str());
            result.Stabilize();
        }
        else if(typeTag == "<int32>")
        {
            status = result.Set("i", StringToI32(content.c_str()));
        }
        else if(typeTag == "<int16>")
        {
            status = result.Set("n", StringToI32(content.c_str()));
        }
        else if(typeTag == "<object_path>")
        {
            status = result.Set("o", content.c_str());
            result.Stabilize();
        }
        else if(typeTag == "<uint16>")
        {
            status = result.Set("q", StringToU32(content.c_str()));
        }
        else if(typeTag == "<struct>")
        {
            vector<MsgArg> array = VectorFromString(content);
            status = result.Set("r", array.size(), &array[0]);
            result.Stabilize();
        }
        else if(typeTag == "<string>")
        {
            status = result.Set("s", content.c_str());
            result.Stabilize();
        }
        else if(typeTag == "<uint64>")
        {
            status = result.Set("t", StringToU64(content.c_str()));
        }
        else if(typeTag == "<uint32>")
        {
            status = result.Set("u", StringToU32(content.c_str()));
        }
        else if(0 == typeTag.find("<variant signature="))
        {
            MsgArg varArg = FromString(content);
            result.Set("v", &varArg);
            result.Stabilize();
        }
        else if(typeTag == "<int64>")
        {
            status = result.Set("x", StringToI64(content.c_str()));
        }
        else if(typeTag == "<byte>")
        {
            status = result.Set("y", StringToU32(content.c_str()));
        }
        else if(typeTag == "<handle>")
        {
            content = Trim(content);
            size_t len = content.length()/2;
            uint8_t* bytes = new uint8_t[len];
            if(len != HexStringToBytes(content.c_str(), bytes, len))
            {
                status = ER_BUS_BAD_VALUE;
            }
            else
            {
                status = result.Set("h", bytes);
                result.Stabilize();
            }
            delete[] bytes;
        }
        else if(typeTag == "<array type=\"boolean\">") {
            content = Trim(content);
            vector<bool> elements;
            pos = 0;
            while((pos = content.find_first_not_of(" ", pos)) != string::npos)
            {
                size_t endPos = content.find_first_of(' ', pos);
                elements.push_back(content.substr(pos, endPos-pos) == "1");
                pos = endPos;
            }

            // vector<bool> is special so we must copy it to a usable array
            bool* array = new bool[elements.size()];
            copy(elements.begin(), elements.end(), array);
            status = result.Set("ab", elements.size(), array);
            result.Stabilize();
            delete[] array;
        }
        else if(typeTag == "<array type=\"double\">") {
            content = Trim(content);
            vector<double> elements;
            pos = 0;
            while((pos = content.find_first_not_of(" ", pos)) != string::npos)
            {
                size_t endPos = content.find_first_of(' ', pos);

                if(sizeof(double) == sizeof(uint64_t))
                {
                    uint64_t val = StringToU64(
                            content.substr(pos, endPos-pos).c_str(), 16);
                    elements.push_back(*reinterpret_cast<double*>(&val));
                    pos = endPos;
                }
                else
                {
                    elements.push_back(StringToDouble(
                            content.substr(pos, endPos-pos).c_str()));
                }
            }
            status = result.Set("ad", elements.size(), &elements[0]);
            result.Stabilize();
        }
        else if(typeTag == "<array type=\"int32\">")
        {
            content = Trim(content);
            vector<int32_t> elements;
            pos = 0;
            while((pos = content.find_first_not_of(" ", pos)) != string::npos)
            {
                size_t endPos = content.find_first_of(' ', pos);
                elements.push_back(StringToI32(content.substr(
                        pos, endPos-pos).c_str()));
                pos = endPos;
            }
            status = result.Set("ai", elements.size(), &elements[0]);
            result.Stabilize();
        }
        else if(typeTag == "<array type=\"int16\">")
        {
            content = Trim(content);
            vector<int16_t> elements;
            pos = 0;
            while((pos = content.find_first_not_of(" ", pos)) != string::npos)
            {
                size_t endPos = content.find_first_of(' ', pos);
                elements.push_back(StringToI32(content.substr(
                        pos, endPos-pos).c_str()));
                pos = endPos;
            }
            status = result.Set("an", elements.size(), &elements[0]);
            result.Stabilize();
        }
        else if(typeTag == "<array type=\"uint16\">")
        {
            content = Trim(content);
            vector<uint16_t> elements;
            pos = 0;
            while((pos = content.find_first_not_of(" ", pos)) != string::npos)
            {
                size_t endPos = content.find_first_of(' ', pos);
                elements.push_back(StringToU32(content.substr(
                        pos, endPos-pos).c_str()));
                pos = endPos;
            }
            status = result.Set("aq", elements.size(), &elements[0]);
            result.Stabilize();
        }
        else if(typeTag == "<array type=\"uint64\">")
        {
            content = Trim(content);
            vector<uint64_t> elements;
            pos = 0;
            while((pos = content.find_first_not_of(" ", pos)) != string::npos)
            {
                size_t endPos = content.find_first_of(' ', pos);
                elements.push_back(StringToU64(content.substr(
                        pos, endPos-pos).c_str()));
                pos = endPos;
            }
            status = result.Set("at", elements.size(), &elements[0]);
            result.Stabilize();
        }
        else if(typeTag == "<array type=\"uint32\">")
        {
            content = Trim(content);
            vector<uint32_t> elements;
            pos = 0;
            while((pos = content.find_first_not_of(" ", pos)) != string::npos)
            {
                size_t endPos = content.find_first_of(' ', pos);
                elements.push_back(StringToU32(content.substr(
                        pos, endPos-pos).c_str()));
                pos = endPos;
            }
            status = result.Set("au", elements.size(), &elements[0]);
            result.Stabilize();
        }
        else if(typeTag == "<array type=\"int64\">")
        {
            content = Trim(content);
            vector<int64_t> elements;
            pos = 0;
            while((pos = content.find_first_not_of(" ", pos)) != string::npos)
            {
                size_t endPos = content.find_first_of(' ', pos);
                elements.push_back(StringToI64(content.substr(
                        pos, endPos-pos).c_str()));
                pos = endPos;
            }
            status = result.Set("ax", elements.size(), &elements[0]);
            result.Stabilize();
        }
        else if(typeTag == "<array type=\"byte\">")
        {
            content = Trim(content);
            vector<uint8_t> elements;
            pos = 0;
            while((pos = content.find_first_not_of(" ", pos)) != string::npos)
            {
                size_t endPos = content.find_first_of(' ', pos);
                elements.push_back(StringToU32(content.substr(
                        pos, endPos-pos).c_str()));
                pos = endPos;
            }
            status = result.Set("ay", elements.size(), &elements[0]);
            result.Stabilize();
        }

        if(status != ER_OK)
        {
            cout << "Could not create MsgArg from string: " <<
                    QCC_StatusText(status) << endl;
        }
        return result;
    }

    /* Convert a string into a vector of MsgArgs. */
    static
    vector<MsgArg>
    VectorFromString(
        string content
        )
    {
        vector<MsgArg> array;

        // Get the MsgArgs for each element
        content = str::Trim(content);
        while(!content.empty())
        {
            size_t typeBeginPos = content.find_first_of('<')+1;                 // TODO: check typeBeginPos for npos, other stuff like that in these kinds of functions
            size_t typeEndPos = content.find_first_of(" >", typeBeginPos);
            string elemType = content.substr(
                    typeBeginPos, typeEndPos-typeBeginPos);
            string closeTag = "</"+elemType+">";

            // Find the closing tag for this element
            size_t closeTagPos = content.find(closeTag);
            size_t nestedTypeEndPos = typeEndPos;
            while(closeTagPos > content.find(elemType, nestedTypeEndPos))
            {
                nestedTypeEndPos = closeTagPos+2+elemType.length();
                closeTagPos = content.find(
                        closeTag, closeTagPos+closeTag.length());
            }

            string element = content.substr(0, closeTagPos+closeTag.length());
            array.push_back(FromString(element));

            content = content.substr(closeTagPos+closeTag.length());
        }

        return array;
    }

} // namespace msgarg

namespace bus {

    /* Info needed to make a BusObject */
    struct BusObjectInfo
    {
        string                              path;
        vector<const InterfaceDescription*> interfaces;
    };

    /* Recursively get BusObject information from an attachment. */
    static
    void
    GetBusObjectsRecursive(
        vector<BusObjectInfo>& busObjects,
        ProxyBusObject&        proxy
        )
    {
        QStatus err = proxy.IntrospectRemoteObject(500);
        if(err != ER_OK)
        {
            return;
        }

        BusObjectInfo thisObj;
        thisObj.path = proxy.GetPath().c_str();
        if(thisObj.path.empty()) {
            thisObj.path = "/";
        }
                                                                                //cout << "  " << thisObj.path << endl;
        // Get the interfaces implemented at this object path
        size_t numIfaces = proxy.GetInterfaces();
        if(numIfaces != 0)
        {
            InterfaceDescription** ifaceList =
                    new InterfaceDescription*[numIfaces];
            numIfaces = proxy.GetInterfaces(
                    (const InterfaceDescription**)ifaceList, numIfaces);

            // Find the interface(s) being advertised by this AJ device
            for(uint32_t i = 0; i < numIfaces; ++i)
            {
                const char* ifaceName = ifaceList[i]->GetName();
                string ifaceNameStr(ifaceName);
                if(ifaceNameStr != "org.freedesktop.DBus.Peer"           &&
                   ifaceNameStr != "org.freedesktop.DBus.Introspectable" &&
                   ifaceNameStr != "org.freedesktop.DBus.Properties"     &&
                   ifaceNameStr != "org.allseen.Introspectable"          )
                {                                                               //cout << "    " << ifaceNameStr << endl;
                    thisObj.interfaces.push_back(ifaceList[i]);
                }
            }

            delete[] ifaceList;

            if(!thisObj.interfaces.empty())
            {
                busObjects.push_back(thisObj);
            }
        }

        // Get the children of this object path
        size_t num_children = proxy.GetChildren();
        if(num_children != 0)
        {
            ProxyBusObject** children = new ProxyBusObject*[num_children];
            num_children = proxy.GetChildren(children, num_children);

            for(uint32_t i = 0; i < num_children; ++i)
            {
                GetBusObjectsRecursive(busObjects, *children[i]);
            }

            delete[] children;
        }
    }
} // namespace bus
} // namespace util

// Forward declaration of XmppTransport class due to circular dependencies
class ajn::gw::XmppTransport
{
public:
    class Listener
    {
    public:
        typedef enum {
            XMPP_CONNECT    = XMPP_CONN_CONNECT,
            XMPP_DISCONNECT = XMPP_CONN_DISCONNECT,
            XMPP_FAIL       = XMPP_CONN_FAIL
        } ConnectionStatus;

        typedef
        void
        (XmppTransport::Listener::* ConnectionCallback)(
            ConnectionStatus status,
            void*            userdata
            );
    };
	
    typedef enum {
        xmpp_uninitialized,
        xmpp_disconnected,
        xmpp_connected,
        xmpp_error,
        xmpp_aborting
    } XmppConnectionState;

    XmppTransport(
        XMPPConnector* connector,
        const string&  jabberId,
        const string&  password,
        const string&  chatroom,
        const string&  nickname
        );
    ~XmppTransport();

    void
    SetConnectionCallback(
        XmppTransport::Listener*                    listener,
        XmppTransport::Listener::ConnectionCallback callback,
        void*                                       userdata
        );
    void
    RemoveConnectionCallback();

    void
    NameOwnerChanged(
        const char* wellKnownName,
        const char* uniqueName
        );

    QStatus Run();
    void Stop();

    void
    SendAdvertisement(
        const string&                           name,
        const vector<util::bus::BusObjectInfo>& busObjects
        );
    void
    SendAdvertisementLost(
        const string& name
        );
    void
    SendAnnounce(
        uint16_t                                   version,
        uint16_t                                   port,
        const char*                                busName,
        const AnnounceHandler::ObjectDescriptions& objectDescs,
        const AnnounceHandler::AboutData&          aboutData
        );
    void
    SendJoinRequest(
        const string&                           remoteName,
        SessionPort                             sessionPort,
        const char*                             joiner,
        const SessionOpts&                      opts,
        const vector<util::bus::BusObjectInfo>& busObjects
        );
    void
    SendJoinResponse(
        const string& joinee,
        SessionId     sessionId
        );
    void
    SendSessionJoined(
        const string& joiner,
        const string& joinee,
        SessionPort   port,
        SessionId     remoteId,
        SessionId     localId
        );
    void
    SendSessionLost(
        const string& peer,
        SessionId     id
        );
    void
    SendMethodCall(
        const InterfaceDescription::Member* member,
        Message&                            message,
        const string&                       busName,
        const string&                       objectPath
        );
    void
    SendMethodReply(
        const string& destName,
        const string& destPath,
        Message& reply
        );
    void
    SendSignal(
        const InterfaceDescription::Member* member,
        const char*                         srcPath,
        Message&                            message
        );
    void
    SendGetRequest(
        const string& ifaceName,
        const string& propName,
        const string& destName,
        const string& destPath
        );
    void
    SendGetReply(
        const string& destName,
        const string& destPath,
        const MsgArg& replyArg
        );
    void
    SendSetRequest(
        const string& ifaceName,
        const string& propName,
        const MsgArg& msgArg,
        const string& destName,
        const string& destPath
        );
    void
    SendSetReply(
        const string& destName,
        const string& destPath,
        QStatus       replyStatus
        );
    void
    SendGetAllRequest(
        const InterfaceDescription::Member* member,
        const string& destName,
        const string& destPath
        );
    void
    SendGetAllReply(
        const string& destName,
        const string& destPath,
        const MsgArg& replyArgs
        );

    void
    SendMessage(
        const string& body,
        const string& messageType = ""
        );

private:
    vector<XMPPConnector::RemoteObjectDescription>
    ParseBusObjectInfo(
        istringstream& msgStream
        );

    void ReceiveAdvertisement(const string& message);
    void ReceiveAdvertisementLost(const string& message);
    void ReceiveAnnounce(const string& message);
    void ReceiveJoinRequest(const string& message);
    void ReceiveJoinResponse(const string& message);
    void ReceiveSessionJoined(const string& message);
    void ReceiveSessionLost(const string& message);
    void ReceiveMethodCall(const string& message);
    void ReceiveMethodReply(const string& message);
    void ReceiveSignal(const string& message);
    void ReceiveGetRequest(const string& message);
    void ReceiveGetReply(const string& message);
    void ReceiveSetRequest(const string& message);
    void ReceiveSetReply(const string& message);
    void ReceiveGetAllRequest(const string& message);
    void ReceiveGetAllReply(const string& message);

    static
    int
    XmppStanzaHandler(
        xmpp_conn_t* const   conn,
        xmpp_stanza_t* const stanza,
        void* const          userdata
        );

    static
    void
    XmppConnectionHandler(
        xmpp_conn_t* const         conn,
        const xmpp_conn_event_t    event,
        const int                  error,
        xmpp_stream_error_t* const streamError,
        void* const                userdata
        );

private:
    XMPPConnector* m_connector;
    const string   m_jabberId;
    const string   m_password;
    const string   m_chatroom;                                                  // TODO: moving away from using chatrooms
    const string   m_nickname;
    XmppConnectionState m_connectionState;

    xmpp_ctx_t*                  m_xmppCtx;
    xmpp_conn_t*                 m_xmppConn;
    Listener*                    m_callbackListener;
    Listener::ConnectionCallback m_connectionCallback;
    void*                        m_callbackUserdata;

    struct AnnounceInfo                                                         // TODO: Create attachment on announce if !exists. No need for AnnounceInfo or m_pendingAnnouncements
    {
        uint16_t                            version;
        uint16_t                            port;
        string                              busName;
        AnnounceHandler::ObjectDescriptions objectDescs;
        AnnounceHandler::AboutData          aboutData;
    };
    map<string, AnnounceInfo> m_pendingAnnouncements;
    pthread_mutex_t           m_pendingAnnouncementsMutex;

    map<string, string> m_wellKnownNameMap;

    BusAttachment m_propertyBus;


    static const string ALLJOYN_CODE_ADVERTISEMENT;
    static const string ALLJOYN_CODE_ADVERT_LOST;
    static const string ALLJOYN_CODE_ANNOUNCE;
    static const string ALLJOYN_CODE_JOIN_REQUEST;
    static const string ALLJOYN_CODE_JOIN_RESPONSE;
    static const string ALLJOYN_CODE_SESSION_JOINED;
    static const string ALLJOYN_CODE_SESSION_LOST;
    static const string ALLJOYN_CODE_METHOD_CALL;
    static const string ALLJOYN_CODE_METHOD_REPLY;
    static const string ALLJOYN_CODE_SIGNAL;
    static const string ALLJOYN_CODE_GET_PROPERTY;
    static const string ALLJOYN_CODE_GET_PROP_REPLY;
    static const string ALLJOYN_CODE_SET_PROPERTY;
    static const string ALLJOYN_CODE_SET_PROP_REPLY;
    static const string ALLJOYN_CODE_GET_ALL;
    static const string ALLJOYN_CODE_GET_ALL_REPLY;
};


class ajn::gw::RemoteBusAttachment :
    public BusAttachment
{
public:
    RemoteBusAttachment(
        const string&  remoteName,
        XmppTransport* transport
        ) :
        BusAttachment(remoteName.c_str(), true),
        m_transport(transport),
        m_remoteName(remoteName),
        m_wellKnownName(""),
        m_listener(this, transport),
        m_objects(),
        m_activeSessions(),
        m_aboutPropertyStore(NULL),
        m_aboutBusObject(NULL)
    {
        pthread_mutex_init(&m_activeSessionsMutex, NULL);

        RegisterBusListener(m_listener);
    }

    ~RemoteBusAttachment()
    {
        vector<RemoteBusObject*>::iterator it;
        for(it = m_objects.begin(); it != m_objects.end(); ++it)
        {
            UnregisterBusObject(**it);
            delete(*it);
        }

        if(m_aboutBusObject)
        {
            UnregisterBusObject(*m_aboutBusObject);
            m_aboutBusObject->Unregister();
            delete m_aboutBusObject;
            delete m_aboutPropertyStore;
        }

        UnregisterBusListener(m_listener);

        pthread_mutex_destroy(&m_activeSessionsMutex);
    }

    QStatus
    BindSessionPort(
        SessionPort port
        )
    {
        SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true,
                SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
        return BusAttachment::BindSessionPort(port, opts, m_listener);
    }

    QStatus
    JoinSession(
        const string& host,
        SessionPort   port,
        SessionId&    id
        )
    {
        SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true,
                SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
        return BusAttachment::JoinSession(
                host.c_str(), port, &m_listener, id, opts);
    }

    QStatus
    RegisterSignalHandler(
        const InterfaceDescription::Member* member
        )
    {
        const char* ifaceName = member->iface->GetName();
        if(ifaceName == strstr(ifaceName, "org.alljoyn.Bus."))
        {
            return ER_OK;
        }

        // Unregister first in case we already registered for this particular
        //  interface member.
        UnregisterSignalHandler(
                this,
                static_cast<MessageReceiver::SignalHandler>(
                &RemoteBusAttachment::AllJoynSignalHandler),
                member, NULL);

        return BusAttachment::RegisterSignalHandler(
                this,
                static_cast<MessageReceiver::SignalHandler>(
                &RemoteBusAttachment::AllJoynSignalHandler),
                member, NULL);
    }

    void
    AllJoynSignalHandler(
        const InterfaceDescription::Member* member,
        const char*                         srcPath,
        Message&                            message
        )
    {
        cout << "Received signal from " << message->GetSender() << endl;
        m_transport->SendSignal(member, srcPath, message);
    }

    QStatus
    AddRemoteObject(
        const string&                       path,
        vector<const InterfaceDescription*> interfaces
        )
    {
        QStatus err = ER_OK;
        RemoteBusObject* newObj = new RemoteBusObject(this, path, m_transport); // TODO: why pointers?

        err = newObj->ImplementInterfaces(interfaces);
        if(err != ER_OK)
        {
            delete newObj;
            return err;
        }

        err = RegisterBusObject(*newObj);
        if(err != ER_OK)
        {
            cout << "Failed to register remote bus object: " <<
                    QCC_StatusText(err) << endl;
            delete newObj;
            return err;
        }

        m_objects.push_back(newObj);
        return err;
    }

    string
    RequestWellKnownName(
        const string& remoteName
        )
    {
        if(remoteName.find_first_of(":") != 0)
        {
            // Request and advertise the new attachment's name
            QStatus err = RequestName(remoteName.c_str(),
                    DBUS_NAME_FLAG_ALLOW_REPLACEMENT|DBUS_NAME_FLAG_DO_NOT_QUEUE
                    );
            if(err != ER_OK)
            {
                cout << "Could not acquire well known name " << remoteName <<
                        ": " << QCC_StatusText(err) << endl;
                m_wellKnownName = "";
            }
            else
            {
                m_wellKnownName = remoteName;
            }
        }
        else
        {
            // We have a device advertising its unique name.
            m_wellKnownName = GetUniqueName().c_str();
        }

        return m_wellKnownName;
    }

    void
    AddSession(
        SessionId     localId,
        const string& peer,
        SessionPort   port,
        SessionId     remoteId
        )
    {
        SessionInfo info = {peer, port, remoteId};

        pthread_mutex_lock(&m_activeSessionsMutex);
        m_activeSessions[localId] = info;
        pthread_mutex_unlock(&m_activeSessionsMutex);
    }

    void
    RemoveSession(
        SessionId localId
        )
    {
        pthread_mutex_lock(&m_activeSessionsMutex);
        m_activeSessions.erase(localId);
        pthread_mutex_unlock(&m_activeSessionsMutex);
    }

    SessionId
    GetLocalSessionId(
        SessionId remoteId
        )
    {
        SessionId retval = 0;

        pthread_mutex_lock(&m_activeSessionsMutex);
        map<SessionId, SessionInfo>::iterator it;
        for(it = m_activeSessions.begin(); it != m_activeSessions.end(); ++it)
        {
            if(it->second.remoteId == remoteId)
            {
                retval = it->first;
                break;
            }
        }
        pthread_mutex_unlock(&m_activeSessionsMutex);

        return retval;
    }

    SessionId
    GetSessionIdByPeer(
        const string& peer
        )
    {
        SessionId retval = 0;

        pthread_mutex_lock(&m_activeSessionsMutex);
        map<SessionId, SessionInfo>::iterator it;
        for(it = m_activeSessions.begin(); it != m_activeSessions.end(); ++it)
        {
            if(it->second.peer == peer)
            {
                retval = it->first;
                break;
            }
        }
        pthread_mutex_unlock(&m_activeSessionsMutex);

        return retval;
    }

    string
    GetPeerBySessionId(
        SessionId id
        )
    {
        string retval = "";

        pthread_mutex_lock(&m_activeSessionsMutex);
        map<SessionId, SessionInfo>::iterator it = m_activeSessions.find(id);
        if(it != m_activeSessions.end())
        {
            retval = it->second.peer;
        }
        pthread_mutex_unlock(&m_activeSessionsMutex);

        return retval;
    }

    void
    RelayAnnouncement(
        uint16_t                                   version,
        uint16_t                                   port,
        const string&                              busName,
        const AnnounceHandler::ObjectDescriptions& objectDescs,
        const AnnounceHandler::AboutData&          aboutData
        )
    {
        QStatus err = ER_OK;
        cout << "Relaying announcement for " << m_wellKnownName << endl;

        if(m_aboutBusObject)
        {
            // Already announced. Announcement must have been updated.
            UnregisterBusObject(*m_aboutBusObject);
            delete m_aboutBusObject;
            delete m_aboutPropertyStore;
        }

        // Set up our About bus object
        m_aboutPropertyStore = new AboutPropertyStore();
        err = m_aboutPropertyStore->SetAnnounceArgs(aboutData);
        if(err != ER_OK)
        {
            cout << "Failed to set About announcement args for " <<
                    m_wellKnownName << ": " << QCC_StatusText(err) << endl;
            delete m_aboutPropertyStore;
            m_aboutPropertyStore = 0;
            return;
        }

        m_aboutBusObject = new AboutBusObject(this, *m_aboutPropertyStore);
        err = m_aboutBusObject->AddObjectDescriptions(objectDescs);
        if(err != ER_OK)
        {
            cout << "Failed to add About object descriptions for " <<
                    m_wellKnownName << ": " << QCC_StatusText(err) << endl;
            return;
        }

        // Bind and register the announced session port
        err = BindSessionPort(port);
        if(err != ER_OK)
        {
            cout << "Failed to bind About announcement session port for " <<
                    m_wellKnownName << ": " << QCC_StatusText(err) << endl;
            return;
        }
        err = m_aboutBusObject->Register(port);
        if(err != ER_OK)
        {
            cout << "Failed to register About announcement port for " <<
                    m_wellKnownName << ": " << QCC_StatusText(err) << endl;
            return;
        }

        // Register the bus object
        err = RegisterBusObject(*m_aboutBusObject);
        if(err != ER_OK)
        {
            cout << "Failed to register AboutService bus object: " <<
                    QCC_StatusText(err) << endl;
        }

        // Make the announcement
        err = m_aboutBusObject->Announce();
        if(err != ER_OK)
        {
            cout << "Failed to relay announcement for " << m_wellKnownName <<
                    ": " << QCC_StatusText(err) << endl;
            return;
        }
    }

    void
    RelaySignal(
        const string&         destination,
        SessionId             sessionId,
        const string&         ifaceName,
        const string&         ifaceMember,
        const vector<MsgArg>& msgArgs
        )
    {
        cout << "Relaying signal on " << m_remoteName << endl;

        // Find the bus object to relay the signal
        RemoteBusObject* busObject = 0;
        vector<RemoteBusObject*>::iterator objIter;
        for(objIter = m_objects.begin(); objIter != m_objects.end(); ++objIter)
        {
            if((*objIter)->HasInterface(ifaceName, ifaceMember))
            {
                busObject = *objIter;
                break;
            }
        }

        if(busObject)
        {
            busObject->SendSignal(
                    destination, sessionId,
                    ifaceName, ifaceMember, msgArgs);
        }
        else
        {
            cout << "Could not find bus object to relay signal" << endl;
        }
    }

    void
    SignalReplyReceived(
        const string& objectPath,
        const string& replyStr
        )
    {
        vector<RemoteBusObject*>::iterator it;
        for(it = m_objects.begin(); it != m_objects.end(); ++it)
        {
            if(objectPath == (*it)->GetPath())
            {
                (*it)->SignalReplyReceived(replyStr);
                break;
            }
        }
    }

    void
    SignalSessionJoined(
        SessionId result
        )
    {
        m_listener.SignalSessionJoined(result);
    }

    string WellKnownName() const { return m_wellKnownName; }
    string RemoteName() const { return m_remoteName; }

private:
    class AboutPropertyStore :
        public PropertyStore
    {
    public:
        AboutPropertyStore()
        {}

        QStatus
        SetAnnounceArgs(
            const AnnounceHandler::AboutData& aboutData
            )
        {
            // Construct the property store args
            vector<MsgArg> dictArgs;
            AnnounceHandler::AboutData::const_iterator aboutIter;
            for(aboutIter = aboutData.begin();
                aboutIter != aboutData.end();
                ++aboutIter)
            {
                MsgArg dictEntry("{sv}",
                        aboutIter->first.c_str(), &aboutIter->second);
                dictEntry.Stabilize();                                          //cout << dictEntry.ToString(4) << endl;

                dictArgs.push_back(dictEntry);
            }

            QStatus err = m_announceArgs.Set("a{sv}",
                    dictArgs.size(), &dictArgs[0]);
            m_announceArgs.Stabilize();                                         //cout << m_announceArgs.ToString(4) << endl;

            return err;
        }

        QStatus
        ReadAll(
            const char* languageTag,
            Filter      filter,
            MsgArg&     all
            )
        {
            all = m_announceArgs;                                               //cout << "ReadAll called:\n" << all.ToString() << endl;
            return ER_OK;
        }

    private:
        MsgArg m_announceArgs;
    };

    class AboutBusObject :
        public AboutService
    {
    public:
        AboutBusObject(
            RemoteBusAttachment* bus,
            AboutPropertyStore& propertyStore
            ) :
            AboutService(*bus, propertyStore)
        {}

        QStatus
        AddObjectDescriptions(
            const AnnounceHandler::ObjectDescriptions& objectDescs
            )
        {
            QStatus err = ER_OK;

            AnnounceHandler::ObjectDescriptions::const_iterator it;
            for(it = objectDescs.begin(); it != objectDescs.end(); ++it)
            {
                if(it->first == "/About")
                {
                    continue;
                }

                err = AddObjectDescription(it->first, it->second);
                if(err != ER_OK)
                {
                    cout << "Failed to add object description for " <<
                            it->first << ": " << QCC_StatusText(err) << endl;
                    break;
                }
            }

            return err;
        }
    };

    class RemoteBusListener :
        public BusListener,
        public SessionListener,
        public SessionPortListener
    {
    public:
        RemoteBusListener(
            RemoteBusAttachment* bus,
            XmppTransport*       transport
            ) :
            m_bus(bus),
            m_transport(transport),
            m_sessionJoinedSignalReceived(false),
            m_remoteSessionId(0),
            m_pendingSessionJoiners()
        {
            pthread_mutex_init(&m_sessionJoinedMutex, NULL);
            pthread_cond_init(&m_sessionJoinedWaitCond, NULL);
        }

        virtual
        ~RemoteBusListener()
        {
            pthread_mutex_destroy(&m_sessionJoinedMutex);
            pthread_cond_destroy(&m_sessionJoinedWaitCond);
        }

        bool
        AcceptSessionJoiner(
            SessionPort        sessionPort,
            const char*        joiner,
            const SessionOpts& opts
            )
        {
            m_bus->EnableConcurrentCallbacks();

            // Gather interfaces to be implemented on the remote end
            vector<util::bus::BusObjectInfo> busObjects;
            ProxyBusObject proxy(*m_bus, joiner, "/", 0);
            util::bus::GetBusObjectsRecursive(busObjects, proxy);

            // Lock the session join mutex
            pthread_mutex_lock(&m_sessionJoinedMutex);
            m_sessionJoinedSignalReceived = false;
            m_remoteSessionId = 0;

            // Send the session join request via XMPP
            m_transport->SendJoinRequest(
                    m_bus->RemoteName(), sessionPort, joiner, opts, busObjects);

            // Wait for the XMPP response signal
            timespec wait_time = {time(NULL)+10, 0};
            while(!m_sessionJoinedSignalReceived)
            {
                if(ETIMEDOUT == pthread_cond_timedwait(
                        &m_sessionJoinedWaitCond,
                        &m_sessionJoinedMutex,
                        &wait_time))
                {
                    break;
                }
            }

            bool returnVal = (m_remoteSessionId != 0);
            if(returnVal)
            {
                m_pendingSessionJoiners[joiner] = m_remoteSessionId;
            }

            pthread_mutex_unlock(&m_sessionJoinedMutex);

            return returnVal;
        }

        void
        SessionJoined(
            SessionPort port,
            SessionId   id,
            const char* joiner)
        {
            // Find the id of the remote session
            SessionId remoteSessionId = 0;
            pthread_mutex_lock(&m_sessionJoinedMutex);
            map<string, SessionId>::iterator idIter =
                    m_pendingSessionJoiners.find(joiner);
            if(idIter != m_pendingSessionJoiners.end())
            {
                remoteSessionId = idIter->second;
                m_pendingSessionJoiners.erase(idIter);
            }
            pthread_mutex_unlock(&m_sessionJoinedMutex);

            // Register as a session listener
            m_bus->SetSessionListener(id, this);

            // Store the remote/local session id pair
            m_bus->AddSession(id, joiner, port, remoteSessionId);

            // Send the session Id back across the XMPP server
            m_transport->SendSessionJoined(
                    joiner, m_bus->RemoteName(), port, remoteSessionId, id);

            cout << "Session joined between " << joiner << " (remote) and " <<
                    m_bus->RemoteName() << " (local). Port: " << port
                    << " Id: " << id << endl;
        }

        void
        SessionLost(
            SessionId         id,
            SessionLostReason reason
            )
        {
            cout << "Session lost. Attachment: " << m_bus->RemoteName() <<
                    " Id: " << id << endl;

            string peer = m_bus->GetPeerBySessionId(id);
            if(!peer.empty())
            {
                m_bus->RemoveSession(id);
                m_transport->SendSessionLost(peer, id);
            }
        }

        void
        SignalSessionJoined(
            SessionId result
            )
        {
            pthread_mutex_lock(&m_sessionJoinedMutex);
            m_sessionJoinedSignalReceived = true;
            m_remoteSessionId = result;
            pthread_cond_signal(&m_sessionJoinedWaitCond);
            pthread_mutex_unlock(&m_sessionJoinedMutex);
        }

    private:
        RemoteBusAttachment* m_bus;
        XmppTransport*       m_transport;

        bool m_sessionJoinedSignalReceived;
        SessionId m_remoteSessionId;
        pthread_mutex_t m_sessionJoinedMutex;
        pthread_cond_t m_sessionJoinedWaitCond;

        map<string, SessionId> m_pendingSessionJoiners;
    };

    class RemoteBusObject :
        public BusObject
    {
    public:
        RemoteBusObject(
            RemoteBusAttachment* bus,
            const string&        path,
            XmppTransport*       transport
            ) :
            BusObject(path.c_str()),
            m_bus(bus),
            m_transport(transport),
            m_interfaces(),
            m_replyReceived(false),
            m_replyStr("")
        {
            pthread_mutex_init(&m_replyReceivedMutex, NULL);
            pthread_cond_init(&m_replyReceivedWaitCond, NULL);
        }

        virtual
        ~RemoteBusObject()
        {
            pthread_mutex_destroy(&m_replyReceivedMutex);
            pthread_cond_destroy(&m_replyReceivedWaitCond);
        }

        void
        AllJoynMethodHandler(
            const InterfaceDescription::Member* member,
            Message&                            message
            )
        {
            cout << "Received method call: " << member->name << endl;

            pthread_mutex_lock(&m_replyReceivedMutex);
            m_replyReceived = false;
            m_replyStr = "";

            m_transport->SendMethodCall(
                    member, message, m_bus->RemoteName(), GetPath());

            // Wait for the XMPP response signal
            timespec wait_time = {time(NULL)+10, 0};
            while(!m_replyReceived)
            {
                if(ETIMEDOUT == pthread_cond_timedwait(
                        &m_replyReceivedWaitCond,
                        &m_replyReceivedMutex,
                        &wait_time))
                {
                    break;
                }
            }

            string replyStr = m_replyStr;
            pthread_mutex_unlock(&m_replyReceivedMutex);

            vector<MsgArg> replyArgs = util::msgarg::VectorFromString(replyStr);
            QStatus err = MethodReply(message, &replyArgs[0], replyArgs.size());
            if(err != ER_OK)
            {
                cout << "Failed to reply to method call: " <<
                        QCC_StatusText(err) << endl;
            }
        }

        QStatus
        ImplementInterfaces(
            const vector<const InterfaceDescription*>& interfaces
            )
        {
            vector<const InterfaceDescription*>::const_iterator it;
            for(it = interfaces.begin(); it != interfaces.end(); ++it)
            {
                QStatus err = AddInterface(**it);
                if(ER_OK != err)
                {
                    cout << "Failed to add interface " << (*it)->GetName() <<
                            ": " << QCC_StatusText(err) << endl;
                    return err;
                }

                m_interfaces.push_back(*it);

                // Register method handlers
                size_t numMembers = (*it)->GetMembers();
                InterfaceDescription::Member** interfaceMembers =
                        new InterfaceDescription::Member*[numMembers];
                numMembers = (*it)->GetMembers(
                        (const InterfaceDescription::Member**)interfaceMembers,
                        numMembers);

                for(uint32_t i = 0; i < numMembers; ++i)
                {
                    if(interfaceMembers[i]->memberType == MESSAGE_SIGNAL)
                    {
                        err = m_bus->RegisterSignalHandler(interfaceMembers[i]);
                    }
                    else
                    {
                        err = AddMethodHandler(interfaceMembers[i],
                                static_cast<MessageReceiver::MethodHandler>(
                                &RemoteBusObject::AllJoynMethodHandler));       //cout << "Registered method handler for " << m_bus->RemoteName() << GetPath() << ":" << interfaceMembers[i]->name << endl;
                    }
                    if(err != ER_OK)
                    {
                        cout << "Failed to add method handler for " <<
                                interfaceMembers[i]->name.c_str() << ": " <<
                                QCC_StatusText(err) << endl;
                    }
                }

                delete[] interfaceMembers;
            }

            return ER_OK;
        }

        bool
        HasInterface(
            const string& ifaceName,
            const string& memberName = ""
            )
        {
            vector<const InterfaceDescription*>::iterator ifaceIter;
            for(ifaceIter = m_interfaces.begin();
                ifaceIter != m_interfaces.end();
                ++ifaceIter)
            {
                if(ifaceName == (*ifaceIter)->GetName())
                {
                    if(memberName.empty())
                    {
                        return true;
                    }

                    size_t numMembers = (*ifaceIter)->GetMembers();
                    InterfaceDescription::Member** members =
                            new InterfaceDescription::Member*[numMembers];
                    numMembers = (*ifaceIter)->GetMembers(
                            (const InterfaceDescription::Member**)members,
                            numMembers);
                    for(uint32_t i = 0; i < numMembers; ++i)
                    {
                        if(memberName == members[i]->name.c_str())
                        {
                            delete[] members;
                            return true;
                        }
                    }
                    delete[] members;
                }
            }

            return false;
        }

        void
        SendSignal(
            const string&         destination,
            SessionId             sessionId,
            const string&         ifaceName,
            const string&         ifaceMember,
            const vector<MsgArg>& msgArgs
            )
        {
            QStatus err = ER_FAIL;

            // Get the InterfaceDescription::Member
            vector<const InterfaceDescription*>::iterator ifaceIter;
            for(ifaceIter = m_interfaces.begin();
                ifaceIter != m_interfaces.end();
                ++ifaceIter)
            {
                if(ifaceName == (*ifaceIter)->GetName())
                {
                    size_t numMembers = (*ifaceIter)->GetMembers();
                    InterfaceDescription::Member** members =
                            new InterfaceDescription::Member*[numMembers];
                    numMembers = (*ifaceIter)->GetMembers(
                            (const InterfaceDescription::Member**)members,
                            numMembers);
                    for(uint32_t i = 0; i < numMembers; ++i)
                    {
                        if(ifaceMember == members[i]->name.c_str())
                        {
                            err = Signal(
                                    (destination.empty() ?
                                    NULL : destination.c_str()), sessionId,
                                    *members[i], &msgArgs[0], msgArgs.size());
                            if(err != ER_OK)
                            {
                                cout << "Failed to send signal: " <<
                                        QCC_StatusText(err) << endl;
                                err = ER_OK;
                            }
                            break;
                        }
                    }

                    delete[] members;
                    break;
                }
            }

            if(err != ER_OK)
            {
                cout << "Could not find interface member of signal to relay!"
                        << endl;
            }
        }

        void
        SignalReplyReceived(
            const string& replyStr
            )
        {
            pthread_mutex_lock(&m_replyReceivedMutex);
            m_replyReceived = true;
            m_replyStr = replyStr;
            pthread_cond_signal(&m_replyReceivedWaitCond);
            pthread_mutex_unlock(&m_replyReceivedMutex);
        }

    protected:
        QStatus
        Get(
            const char* ifaceName,
            const char* propName,
            MsgArg&     val
            )
        {
            cout << "Received AllJoyn Get request for " << ifaceName << ":" <<
                    propName << endl;

            pthread_mutex_lock(&m_replyReceivedMutex);
            m_replyReceived = false;
            m_replyStr = "";

            m_transport->SendGetRequest(
                    ifaceName, propName, m_bus->RemoteName(), GetPath());

            // Wait for the XMPP response signal
            timespec wait_time = {time(NULL)+10, 0};
            while(!m_replyReceived)
            {
                if(ETIMEDOUT == pthread_cond_timedwait(
                        &m_replyReceivedWaitCond,
                        &m_replyReceivedMutex,
                        &wait_time))
                {
                    break;
                }
            }

            bool replyReceived = m_replyReceived;
            string replyStr = m_replyStr;

            pthread_mutex_unlock(&m_replyReceivedMutex);

            if(replyReceived)
            {
                MsgArg arg(util::msgarg::FromString(replyStr));
                if(arg.Signature() == "v") {
                    val = *arg.v_variant.val;
                } else {
                    val = arg;
                }
                val.Stabilize();
                return ER_OK;
            }
            else
            {
                return ER_BUS_NO_SUCH_PROPERTY;
            }
        }

        QStatus
        Set(
            const char* ifaceName,
            const char* propName,
            MsgArg&     val
            )
        {
            cout << "Received AllJoyn Set request for " << ifaceName << ":" <<
                    propName << endl;

            pthread_mutex_lock(&m_replyReceivedMutex);
            m_replyReceived = false;
            m_replyStr = "";

            m_transport->SendSetRequest(
                    ifaceName, propName, val, m_bus->RemoteName(), GetPath());

            // Wait for the XMPP response signal
            timespec wait_time = {time(NULL)+10, 0};
            while(!m_replyReceived)
            {
                if(ETIMEDOUT == pthread_cond_timedwait(
                        &m_replyReceivedWaitCond,
                        &m_replyReceivedMutex,
                        &wait_time))
                {
                    break;
                }
            }

            bool replyReceived = m_replyReceived;
            string replyStr = m_replyStr;

            pthread_mutex_unlock(&m_replyReceivedMutex);

            if(replyReceived)
            {
                return static_cast<QStatus>(strtol(replyStr.c_str(), NULL, 10));
            }
            else
            {
                return ER_BUS_NO_SUCH_PROPERTY;
            }
        }

        void
        GetAllProps(
            const InterfaceDescription::Member* member,
            Message&                            msg
            )
        {
            cout << "Received AllJoyn GetAllProps request for " <<
                    member->iface->GetName() << ":" << member->name << endl;

            pthread_mutex_lock(&m_replyReceivedMutex);
            m_replyReceived = false;
            m_replyStr = "";

            m_transport->SendGetAllRequest(
                    member, m_bus->RemoteName(), GetPath());

            // Wait for the XMPP response signal
            timespec wait_time = {time(NULL)+10, 0};
            while(!m_replyReceived)
            {
                if(ETIMEDOUT == pthread_cond_timedwait(
                        &m_replyReceivedWaitCond,
                        &m_replyReceivedMutex,
                        &wait_time))
                {
                    break;
                }
            }

            bool replyReceived = m_replyReceived;
            string replyStr = m_replyStr;

            pthread_mutex_unlock(&m_replyReceivedMutex);

            if(replyReceived)
            {
                MsgArg result = util::msgarg::FromString(replyStr);
                QStatus err = MethodReply(msg, &result, 1);
                if(err != ER_OK)
                {
                    cout << "Failed to send method reply to GetAllProps " <<
                            "request: " << QCC_StatusText(err) << endl;
                }
            }
        }

        void
        ObjectRegistered()
        {
            string remoteName = m_bus->RemoteName();
            cout << (remoteName.at(0) == ':' ?
                    bus->GetUniqueName().c_str() : remoteName)
                    << GetPath() << " registered" << endl;
        }

    private:
        RemoteBusAttachment*                m_bus;
        XmppTransport*                      m_transport;
        vector<const InterfaceDescription*> m_interfaces;

        bool            m_replyReceived;
        string          m_replyStr;
        pthread_mutex_t m_replyReceivedMutex;
        pthread_cond_t  m_replyReceivedWaitCond;
    };

    XmppTransport*           m_transport;
    string                   m_remoteName;
    string                   m_wellKnownName;
    RemoteBusListener        m_listener;
    vector<RemoteBusObject*> m_objects;

    struct SessionInfo
    {
        string      peer;
        SessionPort port;
        SessionId   remoteId;
    };
    map<SessionId, SessionInfo> m_activeSessions;
    pthread_mutex_t             m_activeSessionsMutex;

    AboutPropertyStore*       m_aboutPropertyStore;
    AboutBusObject*           m_aboutBusObject;
};

class ajn::gw::AllJoynListener :
    public BusListener,
    public SessionPortListener,
    public AnnounceHandler
{
public:
    AllJoynListener(
        XMPPConnector* connector,
        XmppTransport* transport,
        BusAttachment* bus
        ) :
        BusListener(),
        m_connector(connector),
        m_transport(transport),
        m_bus(bus)
    {
    }

    virtual
    ~AllJoynListener()
    {
        m_bus->UnregisterAllHandlers(this);
    }

    void
    FoundAdvertisedName(
        const char*   name,
        TransportMask transport,
        const char*   namePrefix
        )
    {
        // Do not advertise BusNodes
        if(name == strstr(name, "org.alljoyn.BusNode"))
        {
            return;
        }

        // Do not send if we are the ones transmitting the advertisement
        if(m_connector->IsAdvertisingName(name))
        {
            return;
        }

        cout << "Found advertised name: " << name << endl;

        m_bus->EnableConcurrentCallbacks();

        // Get the objects and interfaces implemented by the advertising device
        vector<util::bus::BusObjectInfo> busObjects;
        ProxyBusObject proxy(*m_bus, name, "/", 0);
        GetBusObjectsRecursive(busObjects, proxy);

        // Send the advertisement via XMPP
        m_transport->SendAdvertisement(name, busObjects);
    }

    void
    LostAdvertisedName(
        const char*   name,
        TransportMask transport,
        const char*   namePrefix
        )
    {
        // BusNodes not advertised
        if(name == strstr(name, "org.alljoyn.BusNode"))
        {
            return;
        }

        cout << "Lost advertised name: " << name << endl;
        m_transport->SendAdvertisementLost(name);
    }

    void
    NameOwnerChanged(
        const char* busName,
        const char* previousOwner,
        const char* newOwner
        )
    {
        if(!busName) { return; }

        //cout << "Detected name owner change: " << busName << endl;
        m_transport->NameOwnerChanged(busName, newOwner);
    }

    void
    Announce(
        uint16_t                  version,
        uint16_t                  port,
        const char*               busName,
        const ObjectDescriptions& objectDescs,
        const AboutData&          aboutData
        )
    {                                                                           // TODO: Create attachment on announce if !exists. Have to introspect here.
        if(m_connector->IsAdvertisingName(busName))
        {
            // This is our own announcement.
            return;
        }

        cout << "Received Announce: " << busName << endl;
        m_transport->SendAnnounce(
                version, port, busName, objectDescs, aboutData);
    }

private:
    XMPPConnector* m_connector;
    XmppTransport* m_transport;
    BusAttachment* m_bus;
};


XmppTransport::XmppTransport(
    XMPPConnector* connector,
    const string&  jabberId,
    const string&  password,
    const string&  chatroom,
    const string&  nickname
    ) :
    m_connector(connector),
    m_jabberId(jabberId),
    m_password(password),
    m_chatroom(chatroom),
    m_nickname(nickname),
    m_connectionState(xmpp_uninitialized),
    m_callbackListener(NULL),
    m_connectionCallback(NULL),
    m_callbackUserdata(NULL),
    m_propertyBus("propertyBus")
{
    xmpp_initialize();
    m_xmppCtx = xmpp_ctx_new(NULL, NULL);
    m_xmppConn = xmpp_conn_new(m_xmppCtx);

    m_propertyBus.Start();
    m_propertyBus.Connect();

    pthread_mutex_init(&m_pendingAnnouncementsMutex, NULL);
}

XmppTransport::~XmppTransport()
{
    pthread_mutex_destroy(&m_pendingAnnouncementsMutex);

    m_propertyBus.Disconnect();
    m_propertyBus.Stop();

    xmpp_conn_release(m_xmppConn);
    xmpp_ctx_free(m_xmppCtx);
    xmpp_shutdown();
}

void
XmppTransport::SetConnectionCallback(
    XmppTransport::Listener*                    listener,
    XmppTransport::Listener::ConnectionCallback callback,
    void*                                       userdata
    )
{
    m_callbackListener = listener;
    m_connectionCallback = callback;
    m_callbackUserdata = userdata;
}
void
XmppTransport::RemoveConnectionCallback()
{
    m_callbackListener = NULL;
    m_connectionCallback = NULL;
    m_callbackUserdata = NULL;
}

void
XmppTransport::NameOwnerChanged(
    const char* wellKnownName,
    const char* uniqueName
    )
{
    if(!wellKnownName)
    {
        return;
    }

    if(!uniqueName)
    {
        m_wellKnownNameMap.erase(wellKnownName);
    }
    else
    {
        m_wellKnownNameMap[wellKnownName] = uniqueName;
    }
}

QStatus
XmppTransport::Run()
{
    // Set up our xmpp connection
    xmpp_conn_set_jid(m_xmppConn, m_jabberId.c_str());
    xmpp_conn_set_pass(m_xmppConn, m_password.c_str());
    xmpp_handler_add(
            m_xmppConn, XmppStanzaHandler, NULL, "message", NULL, this);
    if(0 != xmpp_connect_client(
            m_xmppConn, NULL, 0, XmppConnectionHandler, this))
    {
        cout << "Failed to connect to XMPP server." << endl;
        return ER_FAIL;
    }

    // Listen for XMPP
    xmpp_run(m_xmppCtx);

    return ER_OK;
}

void
XmppTransport::Stop()
{
    m_connectionState = xmpp_aborting;
    xmpp_disconnect(m_xmppConn);
    xmpp_handler_delete(m_xmppConn, XmppStanzaHandler);
}

void
XmppTransport::SendAdvertisement(
    const string&                           name,
    const vector<util::bus::BusObjectInfo>& busObjects
    )
{
    // Find the unique name of the advertising attachment
    string uniqueName = name;
    map<string, string>::iterator wknIter = m_wellKnownNameMap.find(name);
    if(wknIter != m_wellKnownNameMap.end())
    {
        uniqueName = wknIter->second;
    }

    // Construct the text that will be the body of our message
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_ADVERTISEMENT << "\n";
    msgStream << uniqueName << "\n";
    msgStream << name << "\n";
    vector<util::bus::BusObjectInfo>::const_iterator it;
    for(it = busObjects.begin(); it != busObjects.end(); ++it)
    {
        msgStream << it->path << "\n";
        vector<const InterfaceDescription*>::const_iterator if_it;
        for(if_it = it->interfaces.begin();
            if_it != it->interfaces.end();
            ++if_it)
        {
            msgStream << (*if_it)->GetName() << "\n";
            msgStream << (*if_it)->Introspect().c_str() << "\n";
        }

        msgStream << "\n";
    }

    SendMessage(msgStream.str(), ALLJOYN_CODE_ADVERTISEMENT);
}

void
XmppTransport::SendAdvertisementLost(
    const string& name
    )
{
    // Construct the text that will be the body of our message
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_ADVERT_LOST << "\n";
    msgStream << name << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_ADVERT_LOST);
}

void
XmppTransport::SendAnnounce(
    uint16_t                                   version,
    uint16_t                                   port,
    const char*                                busName,
    const AnnounceHandler::ObjectDescriptions& objectDescs,
    const AnnounceHandler::AboutData&          aboutData
    )
{
    // Find the unique name of the announcing attachment
    string uniqueName = busName;
    map<string, string>::iterator wknIter =
            m_wellKnownNameMap.find(busName);
    if(wknIter != m_wellKnownNameMap.end())
    {
        uniqueName = wknIter->second;
    }

    // Construct the text that will be the body of our message
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_ANNOUNCE << "\n";
    msgStream << uniqueName << "\n";
    msgStream << version << "\n";
    msgStream << port << "\n";
    msgStream << busName << "\n";

    AnnounceHandler::ObjectDescriptions::const_iterator objIter;
    for(objIter = objectDescs.begin(); objIter != objectDescs.end(); ++objIter)
    {
        msgStream << objIter->first.c_str() << "\n";
        vector<String>::const_iterator val_iter;
        for(val_iter = objIter->second.begin();
            val_iter != objIter->second.end();
            ++val_iter)
        {
            msgStream << val_iter->c_str() << "\n";
        }
    }

    msgStream << "\n";

    AnnounceHandler::AboutData::const_iterator aboutIter;
    for(aboutIter = aboutData.begin();
        aboutIter != aboutData.end();
        ++aboutIter)
    {
        msgStream << aboutIter->first.c_str() << "\n";
        msgStream << util::msgarg::ToString(aboutIter->second) << "\n\n";
    }

    SendMessage(msgStream.str(), ALLJOYN_CODE_ANNOUNCE);
}

void
XmppTransport::SendJoinRequest(
    const string&                           remoteName,
    SessionPort                             sessionPort,
    const char*                             joiner,
    const SessionOpts&                      opts,
    const vector<util::bus::BusObjectInfo>& busObjects
    )
{
    // Construct the text that will be the body of our message
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_JOIN_REQUEST << "\n";
    msgStream << remoteName << "\n";
    msgStream << sessionPort << "\n";
    msgStream << joiner << "\n";

    // Send the objects/interfaces to be implemented on the remote end
    vector<util::bus::BusObjectInfo>::const_iterator objIter;
    for(objIter = busObjects.begin(); objIter != busObjects.end(); ++objIter)
    {
        msgStream << objIter->path << "\n";
        vector<const InterfaceDescription*>::const_iterator ifaceIter;
        for(ifaceIter = objIter->interfaces.begin();
            ifaceIter != objIter->interfaces.end();
            ++ifaceIter)
        {
            string ifaceNameStr = (*ifaceIter)->GetName();
            if(ifaceNameStr != "org.freedesktop.DBus.Peer"           &&
               ifaceNameStr != "org.freedesktop.DBus.Introspectable" &&
               ifaceNameStr != "org.freedesktop.DBus.Properties"     &&
               ifaceNameStr != "org.allseen.Introspectable"          )
            {
                msgStream << ifaceNameStr << "\n";
                msgStream << (*ifaceIter)->Introspect().c_str() << "\n";
            }
        }

        msgStream << "\n";
    }

    SendMessage(msgStream.str(), ALLJOYN_CODE_JOIN_REQUEST);
}

void
XmppTransport::SendJoinResponse(
    const string& joinee,
    SessionId     sessionId
    )
{
    // Send the status back to the original session joiner
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_JOIN_RESPONSE << "\n";
    msgStream << joinee << "\n";
    msgStream << sessionId << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_JOIN_RESPONSE);
}

void
XmppTransport::SendSessionJoined(
    const string& joiner,
    const string& joinee,
    SessionPort   port,
    SessionId     remoteId,
    SessionId     localId
    )
{
    // Construct the text that will be the body of our message
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_SESSION_JOINED << "\n";
    msgStream << joiner << "\n";
    msgStream << joinee << "\n";
    msgStream << port << "\n";
    msgStream << remoteId << "\n";
    msgStream << localId << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_SESSION_JOINED);
}

void
XmppTransport::SendSessionLost(
    const string& peer,
    SessionId     id
    )
{
    // Construct the text that will be the body of our message
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_SESSION_LOST << "\n";
    msgStream << peer << "\n";
    msgStream << id << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_SESSION_LOST);
}

void
XmppTransport::SendMethodCall(
    const InterfaceDescription::Member* member,
    Message&                            message,
    const string&                       busName,
    const string&                       objectPath
    )
{
    size_t numArgs = 0;
    const MsgArg* msgArgs = 0;
    message->GetArgs(numArgs, msgArgs);

    // Construct the text that will be the body of our message
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_METHOD_CALL << "\n";
    msgStream << message->GetSender() << "\n";
    msgStream << busName << "\n";
    msgStream << objectPath << "\n";
    msgStream << member->iface->GetName() << "\n";
    msgStream << member->name << "\n";
    msgStream << message->GetSessionId() << "\n";
    msgStream << util::msgarg::ToString(msgArgs, numArgs) << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_METHOD_CALL);
}

void
XmppTransport::SendMethodReply(
    const string& destName,
    const string& destPath,
    Message&      reply
    )
{
    size_t numReplyArgs;
    const MsgArg* replyArgs = 0;
    reply->GetArgs(numReplyArgs, replyArgs);

    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_METHOD_REPLY << "\n";
    msgStream << destName << "\n";
    msgStream << destPath << "\n";
    msgStream << util::msgarg::ToString(replyArgs, numReplyArgs) << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_METHOD_REPLY);
}

void
XmppTransport::SendSignal(
    const InterfaceDescription::Member* member,
    const char*                         srcPath,
    Message&                            message
    )
{
    // Find the unique name of the signal sender
    string senderUniqueName = message->GetSender();
    map<string, string>::iterator wknIter =
            m_wellKnownNameMap.find(senderUniqueName);
    if(wknIter != m_wellKnownNameMap.end())
    {
        senderUniqueName = wknIter->second;
    }

    // Get the MsgArgs
    size_t numArgs = 0;
    const MsgArg* msgArgs = 0;
    message->GetArgs(numArgs, msgArgs);

    // Construct the text that will be the body of our message
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_SIGNAL << "\n";
    msgStream << senderUniqueName << "\n";
    msgStream << message->GetDestination() << "\n";
    msgStream << message->GetSessionId() << "\n";
    msgStream << member->iface->GetName() << "\n";
    msgStream << member->name << "\n";
    msgStream << util::msgarg::ToString(msgArgs, numArgs) << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_SIGNAL);
}

void
XmppTransport::SendGetRequest(
    const string& ifaceName,
    const string& propName,
    const string& destName,
    const string& destPath
    )
{
    // Construct the text that will be the body of our message
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_GET_PROPERTY << "\n";
    msgStream << destName << "\n";
    msgStream << destPath << "\n";
    msgStream << ifaceName << "\n";
    msgStream << propName << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_GET_PROPERTY);
}

void
XmppTransport::SendGetReply(
    const string& destName,
    const string& destPath,
    const MsgArg& replyArg
    )
{
    // Return the reply
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_GET_PROP_REPLY << "\n";
    msgStream << destName << "\n";
    msgStream << destPath << "\n";
    msgStream << util::msgarg::ToString(replyArg) << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_GET_PROP_REPLY);
}

void
XmppTransport::SendSetRequest(
    const string& ifaceName,
    const string& propName,
    const MsgArg& msgArg,
    const string& destName,
    const string& destPath
    )
{
    // Construct the text that will be the body of our message
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_SET_PROPERTY << "\n";
    msgStream << destName << "\n";
    msgStream << destPath << "\n";
    msgStream << ifaceName << "\n";
    msgStream << propName << "\n";
    msgStream << util::msgarg::ToString(msgArg) << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_SET_PROPERTY);
}

void
XmppTransport::SendSetReply(
    const string& destName,
    const string& destPath,
    QStatus       replyStatus
    )
{
    // Return the reply
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_SET_PROP_REPLY << "\n";
    msgStream << destName << "\n";
    msgStream << destPath << "\n";
    msgStream << static_cast<uint32_t>(replyStatus) << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_SET_PROP_REPLY);
}

void
XmppTransport::SendGetAllRequest(
    const InterfaceDescription::Member* member,
    const string& destName,
    const string& destPath
    )
{
    // Construct the text that will be the body of our message
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_GET_ALL << "\n";
    msgStream << destName << "\n";
    msgStream << destPath << "\n";
    msgStream << member->iface->GetName() << "\n";
    msgStream << member->name << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_GET_ALL);
}

void
XmppTransport::SendGetAllReply(
    const string& destName,
    const string& destPath,
    const MsgArg& replyArgs
    )
{
    // Construct the text that will be the body of our message
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_GET_ALL_REPLY << "\n";
    msgStream << destName << "\n";
    msgStream << destPath << "\n";
    msgStream << util::msgarg::ToString(replyArgs) << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_GET_ALL_REPLY);
}

void
XmppTransport::SendMessage(
    const string& body,
    const string& messageType
    )
{
    string bodyHex = util::str::Compress(body);

    xmpp_stanza_t* messageStanza = xmpp_stanza_new(m_xmppCtx);
    xmpp_stanza_set_name(messageStanza, "message");
    xmpp_stanza_set_attribute(messageStanza, "to", m_chatroom.c_str());
    xmpp_stanza_set_type(messageStanza, "groupchat");

    xmpp_stanza_t* bodyStanza = xmpp_stanza_new(m_xmppCtx);
    xmpp_stanza_set_name(bodyStanza, "body");

    xmpp_stanza_t* textStanza = xmpp_stanza_new(m_xmppCtx);
    xmpp_stanza_set_text(textStanza, bodyHex.c_str());

    xmpp_stanza_add_child(bodyStanza, textStanza);
    xmpp_stanza_release(textStanza);
    xmpp_stanza_add_child(messageStanza, bodyStanza);
    xmpp_stanza_release(bodyStanza);

    //char* buf = NULL;
    //size_t buflen = 0;
    //xmpp_stanza_to_text(messageStanza, &buf, &buflen);
    cout << "Sending XMPP " << (messageType.empty() ? "" : messageType+" ") <<
            "message." << endl;
    //xmpp_free(m_xmppCtx, buf);

    xmpp_send(m_xmppConn, messageStanza);
    xmpp_stanza_release(messageStanza);
}

vector<XMPPConnector::RemoteObjectDescription>
XmppTransport::ParseBusObjectInfo(
    istringstream& msgStream
    )
{
    vector<XMPPConnector::RemoteObjectDescription> results;
    XMPPConnector::RemoteObjectDescription thisObj;
    string interfaceName = "";
    string interfaceDescription = "";

    string line = "";
    while(getline(msgStream, line))
    {
        if(line.empty())
        {
            if(!interfaceDescription.empty())
            {
                // We've reached the end of an interface description.
                //util::str::UnescapeXml(interfaceDescription);
                thisObj.interfaces[interfaceName] = interfaceDescription;

                interfaceName.clear();
                interfaceDescription.clear();
            }
            else
            {
                // We've reached the end of a bus object.
                results.push_back(thisObj);

                thisObj.path.clear();
                thisObj.interfaces.clear();
            }
        }
        else
        {
            if(thisObj.path.empty())
            {
                thisObj.path = line.c_str();
            }
            else if(interfaceName.empty())
            {
                interfaceName = line;
            }
            else
            {
                interfaceDescription.append(line + "\n");
            }
        }
    }

    return results;
}

void
XmppTransport::ReceiveAdvertisement(
    const string& message
    )
{
    istringstream msgStream(message);
    string line;

    // First line is the type (advertisement)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_ADVERTISEMENT){ return; }

    // Second line is the name to advertise
    string remoteName, advertisedName;
    if(0 == getline(msgStream, remoteName)){ return; }                          //cout << "received XMPP advertised name: " << remoteName << endl; cout << message << endl;
    if(0 == getline(msgStream, advertisedName)){ return; }

    cout << "Received remote advertisement: " << remoteName << endl;

    vector<XMPPConnector::RemoteObjectDescription> objects =
            ParseBusObjectInfo(msgStream);
    RemoteBusAttachment* bus = m_connector->GetRemoteAttachment(
            remoteName, &objects);
    if(!bus)
    {
        return;
    }

    // Request and advertise our name
    string wkn = bus->WellKnownName();
    if(wkn.empty())
    {
        wkn = bus->RequestWellKnownName(advertisedName);
        if(wkn.empty())
        {
            m_connector->DeleteRemoteAttachment(bus);
            return;
        }
    }

    cout << "Advertising name: " << wkn << endl;
    QStatus err = bus->AdvertiseName(wkn.c_str(), TRANSPORT_ANY);
    if(err != ER_OK)
    {
        cout << "Failed to advertise " << wkn << ": " <<
                QCC_StatusText(err) << endl;
        m_connector->DeleteRemoteAttachment(bus);
        return;
    }

    // Do we have a pending announcement?                                       // TODO: Create attachment on announce if !exists. no need for this
    pthread_mutex_lock(&m_pendingAnnouncementsMutex);
    map<string, AnnounceInfo>::iterator announceIter =
            m_pendingAnnouncements.find(remoteName);
    if(announceIter != m_pendingAnnouncements.end())
    {
        bus->RelayAnnouncement(
                announceIter->second.version,
                announceIter->second.port,
                wkn,
                announceIter->second.objectDescs,
                announceIter->second.aboutData);
        m_pendingAnnouncements.erase(announceIter);
    }                                                                           //else{cout << "No pending announcement found for " << wkn << endl;}
    pthread_mutex_unlock(&m_pendingAnnouncementsMutex);
}

void
XmppTransport::ReceiveAdvertisementLost(
    const string& message
    )
{
    istringstream msgStream(message);
    string line, name;

    // First line is the type (advertisement)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_ADVERT_LOST){ return; }

    // Second line is the advertisement lost
    if(0 == getline(msgStream, name)){ return; }

    // Get the local bus attachment advertising this name
    RemoteBusAttachment* bus =
            m_connector->GetRemoteAttachmentByAdvertisedName(name);
    if(bus)
    {
        m_connector->DeleteRemoteAttachment(bus);
    }
}

void
XmppTransport::ReceiveAnnounce(
    const string& message
    )
{
    istringstream msgStream(message);
    string line, remoteName, versionStr, portStr, busName;

    // First line is the type (announcement)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_ANNOUNCE){ return; }

    // Get the info from the message
    if(0 == getline(msgStream, remoteName)){ return; }
    if(0 == getline(msgStream, versionStr)){ return; }
    if(0 == getline(msgStream, portStr)){ return; }
    if(0 == getline(msgStream, busName)){ return; }

    cout << "Received remote announcement: " << busName << endl;

    // The object descriptions follow
    AnnounceHandler::ObjectDescriptions objDescs;
    qcc::String objectPath = "";
    vector<qcc::String> interfaceNames;
    while(0 != getline(msgStream, line))
    {
        if(line.empty())
        {
            objDescs[objectPath] = interfaceNames;
            break;
        }

        if(objectPath.empty())
        {
            objectPath = line.c_str();
        }
        else
        {
            if(line[0] == '/')
            {
                // end of the object description
                objDescs[objectPath] = interfaceNames;

                interfaceNames.clear();
                objectPath = line.c_str();
            }
            else
            {
                interfaceNames.push_back(line.c_str());
            }
        }
    }

    // Then come the properties
    AnnounceHandler::AboutData aboutData;
    string propName = "", propDesc = "";
    while(0 != getline(msgStream, line))
    {
        if(line.empty())
        {
            // reached the end of a property
            aboutData[propName.c_str()] = util::msgarg::FromString(propDesc);

            propName.clear();
            propDesc.clear();
        }

        if(propName.empty())
        {
            propName = line;
        }
        else
        {
            propDesc += line;
        }
    }

    // Find the BusAttachment with the given app name
    RemoteBusAttachment* bus = m_connector->GetRemoteAttachment(remoteName);
    if(bus)
    {
        bus->RelayAnnouncement(
                strtoul(versionStr.c_str(), NULL, 10),
                strtoul(portStr.c_str(), NULL, 10),
                bus->WellKnownName(),
                objDescs,
                aboutData);                                                     // TODO: Create attachment on announce if !exists. Will have remote attachment info here. no need for else or m_pendingAnnouncements
    }
    else
    {
        pthread_mutex_lock(&m_pendingAnnouncementsMutex);                       //cout << "Storing pending announcement for " << busName << endl;
        AnnounceInfo info = {
                strtoul(versionStr.c_str(), NULL, 10),
                strtoul(portStr.c_str(), NULL, 10),
                busName,
                objDescs,
                aboutData};
        m_pendingAnnouncements[remoteName] = info;
        pthread_mutex_unlock(&m_pendingAnnouncementsMutex);
    }
}

void
XmppTransport::ReceiveJoinRequest(
    const string& message
    )
{
    istringstream msgStream(message);
    string line, joiner, joinee, portStr;

    // First line is the type (join request)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_JOIN_REQUEST){ return; }

    // Next is the session port, joinee, and the joiner
    if(0 == getline(msgStream, joinee)){ return; }
    if(0 == getline(msgStream, portStr)){ return; }
    if(0 == getline(msgStream, joiner)){ return; }

    // Then follow the interfaces implemented by the joiner
    vector<XMPPConnector::RemoteObjectDescription> objects =
            ParseBusObjectInfo(msgStream);

    // Get or create a bus attachment to join from
    RemoteBusAttachment* bus = m_connector->GetRemoteAttachment(
            joiner, &objects);

    SessionId id = 0;
    if(!bus)
    {
        cout << "Failed to create bus attachment to proxy session!" << endl;
    }
    else
    {
        // Try to join a session of our own
        SessionPort port = strtoul(portStr.c_str(), NULL, 10);

        QStatus err = bus->JoinSession(joinee, port, id);
        if(err == ER_ALLJOYN_JOINSESSION_REPLY_ALREADY_JOINED)
        {
            id = bus->GetSessionIdByPeer(joinee);
            cout << "Session already joined between " << joiner <<
                    " (local) and " << joinee << " (remote). Port: "
                    << port << " Id: " << id << endl;
        }
        else if(err != ER_OK)
        {
            cout << "Join session request rejected: " <<
                    QCC_StatusText(err) << endl;
        }
        else
        {
            cout << "Session joined between " << joiner << " (local) and " <<
                    joinee << " (remote). Port: " << port
                    << " Id: " << id << endl;

            // Register signal handlers for the interfaces we're joining with   // TODO: this info could be sent via XMPP from the connector joinee instead of introspected again
            vector<util::bus::BusObjectInfo> joineeObjects;                     // TODO: do this before joining?
            ProxyBusObject proxy(*bus, joinee.c_str(), "/", id);
            util::bus::GetBusObjectsRecursive(joineeObjects, proxy);

            vector<util::bus::BusObjectInfo>::iterator objectIter;
            for(objectIter = joineeObjects.begin();
                objectIter != joineeObjects.end();
                ++objectIter)
            {
                vector<const InterfaceDescription*>::iterator ifaceIter;
                for(ifaceIter = objectIter->interfaces.begin();
                    ifaceIter != objectIter->interfaces.end();
                    ++ifaceIter)
                {
                    string interfaceName = (*ifaceIter)->GetName();

                    // Register signal listeners here.                          // TODO: sessionless signals? register on advertise/announce
                    size_t numMembers = (*ifaceIter)->GetMembers();
                    InterfaceDescription::Member** ifaceMembers =
                            new InterfaceDescription::Member*[numMembers];
                    numMembers = (*ifaceIter)->GetMembers(
                            (const InterfaceDescription::Member**)ifaceMembers,
                            numMembers);
                    for(uint32_t i = 0; i < numMembers; ++i)
                    {
                        if(ifaceMembers[i]->memberType == MESSAGE_SIGNAL)
                        {
                            err = bus->RegisterSignalHandler(ifaceMembers[i]);
                            if(err != ER_OK)
                            {
                                cout << "Could not register signal handler for "
                                        << interfaceName << ":" <<
                                        ifaceMembers[i]->name << endl;
                            }
                        }
                    }
                    delete[] ifaceMembers;
                }
            }
        }
    }

    SendJoinResponse(joinee, id);
}

void
XmppTransport::ReceiveJoinResponse(
    const string& message
    )
{
    istringstream msgStream(message);
    string line, appName, remoteSessionId;

    // First line is the type (join response)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_JOIN_RESPONSE){ return; }

    // Get the info from the message
    if(0 == getline(msgStream, appName)){ return; }
    if(0 == getline(msgStream, remoteSessionId)){ return; }

    // Find the BusAttachment with the given app name
    RemoteBusAttachment* bus = m_connector->GetRemoteAttachment(appName);
    if(!bus)
    {
        cout << "Failed to find bus attachment to handle join response!"
                << endl;
    }
    else
    {
        bus->SignalSessionJoined(strtoul(remoteSessionId.c_str(), NULL, 10));
    }
}

void
XmppTransport::ReceiveSessionJoined(
    const string& message
    )
{
    istringstream msgStream(message);
    string line, joiner, joinee, portStr, remoteIdStr, localIdStr;

    // First line is the type (session joined)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_SESSION_JOINED){ return; }

    // Get the info from the message
    if(0 == getline(msgStream, joiner)){ return; }
    if(0 == getline(msgStream, joinee)){ return; }
    if(0 == getline(msgStream, portStr)){ return; }
    if(0 == getline(msgStream, localIdStr)){ return; }
    if(0 == getline(msgStream, remoteIdStr)){ return; }

    if(localIdStr.empty() || remoteIdStr.empty())
    {
        return;
    }

    // Find the BusAttachment with the given app name
    RemoteBusAttachment* bus = m_connector->GetRemoteAttachment(joiner);
    if(!bus)
    {
        cout << "Failed to find bus attachment to handle joined session!"
                << endl;
    }
    else
    {
        bus->AddSession(
                strtoul(localIdStr.c_str(), NULL, 10),
                joinee,
                strtoul(portStr.c_str(), NULL, 10),
                strtoul(remoteIdStr.c_str(), NULL, 10));
    }
}

void
XmppTransport::ReceiveSessionLost(
    const string& message
    )
{
    istringstream msgStream(message);
    string line, appName, idStr;

    // First line is the type (session joined)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_SESSION_LOST){ return; }

    // Get the info from the message
    if(0 == getline(msgStream, appName)){ return; }
    if(0 == getline(msgStream, idStr)){ return; }

    // Leave the local session
    RemoteBusAttachment* bus = m_connector->GetRemoteAttachment(appName);
    if(bus)
    {
        SessionId localId = bus->GetLocalSessionId(
                strtoul(idStr.c_str(), NULL, 10));

        cout << "Ending session. Attachment: " << appName << " Id: " << localId
                << endl;

        QStatus err = bus->LeaveSession(localId);
        if(err != ER_OK && err != ER_ALLJOYN_LEAVESESSION_REPLY_NO_SESSION)
        {
            cout << "Failed to end session: " << QCC_StatusText(err) << endl;
        }
        else
        {
            bus->RemoveSession(localId);
        }
    }
}

void
XmppTransport::ReceiveMethodCall(
    const string& message
    )
{
    // Parse the required information
    istringstream msgStream(message);
    string line, remoteName, destName, destPath,
            ifaceName, memberName, remoteSessionId;

    // First line is the type (method call)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_METHOD_CALL){ return; }

    if(0 == getline(msgStream, remoteName)){ return; }
    if(0 == getline(msgStream, destName)){ return; }
    if(0 == getline(msgStream, destPath)){ return; }
    if(0 == getline(msgStream, ifaceName)){ return; }
    if(0 == getline(msgStream, memberName)){ return; }
    if(0 == getline(msgStream, remoteSessionId)){ return; }

    // The rest is the message arguments
    string messageArgsString = "";
    while(0 != getline(msgStream, line))
    {
        messageArgsString += line + "\n";
    }
    //util::str::UnescapeXml(messageArgsString);

    // Find the bus attachment with this busName
    RemoteBusAttachment* bus = m_connector->GetRemoteAttachment(remoteName);
    if(!bus)
    {
        cout << "No bus attachment to handle incoming method call." << endl;
        return;
    }

    // Call the method
    SessionId localSid = bus->GetLocalSessionId(
            strtoul(remoteSessionId.c_str(), NULL, 10));
    ProxyBusObject proxy(*bus, destName.c_str(), destPath.c_str(), localSid);
    QStatus err = proxy.IntrospectRemoteObject();
    if(err != ER_OK)
    {
        cout << "Failed to introspect remote object to relay method call: " <<
                QCC_StatusText(err) << endl;
        return;
    }

    vector<MsgArg> messageArgs =
            util::msgarg::VectorFromString(messageArgsString);
    Message reply(*bus);
    err = proxy.MethodCall(ifaceName.c_str(), memberName.c_str(),
            &messageArgs[0], messageArgs.size(), reply, 5000);
    if(err != ER_OK)
    {
        cout << "Failed to relay method call: " << QCC_StatusText(err) << endl;
        return;
    }

    SendMethodReply(destName, destPath, reply);
}

void
XmppTransport::ReceiveMethodReply(
    const string& message
    )
{
    // Parse the required information
    istringstream msgStream(message);
    string line, remoteName, objPath;

    // First line is the type (method reply)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_METHOD_REPLY){ return; }

    if(0 == getline(msgStream, remoteName)){ return; }
    if(0 == getline(msgStream, objPath)){ return; }

    // The rest is the message arguments
    string messageArgsString = "";
    while(0 != getline(msgStream, line))
    {
        messageArgsString += line + "\n";
    }
    //util::str::UnescapeXml(messageArgsString);

    // Find the bus attachment with this busName
    RemoteBusAttachment* bus = m_connector->GetRemoteAttachment(remoteName);
    if(!bus)
    {
        cout << "No bus attachment to handle incoming method call." << endl;
        return;
    }

    // Tell the attachment we received a message reply
    bus->SignalReplyReceived(objPath, messageArgsString);
}

void
XmppTransport::ReceiveSignal(
    const string& message
    )
{
    // Parse the required information
    istringstream msgStream(message);
    string line, senderName, destination,
            remoteSessionId, ifaceName, ifaceMember;

    // First line is the type (signal)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_SIGNAL){ return; }

    // Get the bus name and remote session ID
    if(0 == getline(msgStream, senderName)){ return; }
    if(0 == getline(msgStream, destination)){ return; }
    if(0 == getline(msgStream, remoteSessionId)){ return; }
    if(0 == getline(msgStream, ifaceName)){ return; }
    if(0 == getline(msgStream, ifaceMember)){ return; }

    // The rest is the message arguments
    string messageArgsString = "";
    while(0 != getline(msgStream, line))
    {
        messageArgsString += line + "\n";
    }
    //util::str::UnescapeXml(messageArgsString);

    // Find the bus attachment with this busName
    RemoteBusAttachment* bus = m_connector->GetRemoteAttachment(senderName);
    if(!bus)
    {
        cout << "No bus attachment to handle incoming signal. Sender: " <<
                senderName << endl;
        return;
    }

    // Relay the signal
    vector<MsgArg> msgArgs = util::msgarg::VectorFromString(messageArgsString);
    SessionId localSessionId = bus->GetLocalSessionId(
            strtoul(remoteSessionId.c_str(), NULL, 10));
    bus->RelaySignal(
            destination, localSessionId, ifaceName, ifaceMember, msgArgs);
}

void
XmppTransport::ReceiveGetRequest(
    const string& message
    )
{
    // Parse the required information
    istringstream msgStream(message);
    string line, destName, destPath, ifaceName, propName;

    // First line is the type (get request)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_GET_PROPERTY){ return; }

    if(0 == getline(msgStream, destName)){ return; }
    if(0 == getline(msgStream, destPath)){ return; }
    if(0 == getline(msgStream, ifaceName)){ return; }
    if(0 == getline(msgStream, propName)){ return; }

    cout << "Retrieving property:\n  " << destName << destPath << "\n  " <<
            ifaceName << ":" << propName << endl;

    // Get the property
    ProxyBusObject proxy(m_propertyBus, destName.c_str(), destPath.c_str(), 0);
    QStatus err = proxy.IntrospectRemoteObject();
    if(err != ER_OK)
    {
        cout << "Failed to introspect remote object to relay get request: " <<
                QCC_StatusText(err) << endl;
        return;
    }

    MsgArg value;
    err = proxy.GetProperty(ifaceName.c_str(), propName.c_str(), value, 5000);  //cout << "Got property value:\n" << util::msgarg::ToString(value) << endl;
    if(err != ER_OK)
    {
        cout << "Failed to relay Get request: " << QCC_StatusText(err) << endl;
        return;                                                                 // TODO: send the actual response status back
    }

    // Return the reply
    SendGetReply(destName, destPath, value);
}

void
XmppTransport::ReceiveGetReply(
    const string& message
    )
{
    // Parse the required information
    istringstream msgStream(message);
    string line, remoteName, objPath;

    // First line is the type (get reply)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_GET_PROP_REPLY){ return; }

    if(0 == getline(msgStream, remoteName)){ return; }
    if(0 == getline(msgStream, objPath)){ return; }

    // The rest is the property value
    string messageArgString = "";
    while(0 != getline(msgStream, line))
    {
        messageArgString += line + "\n";
    }
    //util::str::UnescapeXml(messageArgString);

    // Find the bus attachment with this busName
    RemoteBusAttachment* bus = m_connector->GetRemoteAttachment(remoteName);
    if(!bus)
    {
        cout << "No bus attachment to handle incoming Get reply." << endl;
        return;
    }

    // Tell the attachment we received a message reply
    bus->SignalReplyReceived(objPath, messageArgString);
}

void
XmppTransport::ReceiveSetRequest(
    const string& message
    )
{
    // Parse the required information
    istringstream msgStream(message);
    string line, destName, destPath, ifaceName, propName;

    // First line is the type (get request)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_SET_PROPERTY){ return; }

    if(0 == getline(msgStream, destName)){ return; }
    if(0 == getline(msgStream, destPath)){ return; }
    if(0 == getline(msgStream, ifaceName)){ return; }
    if(0 == getline(msgStream, propName)){ return; }

    // The rest is the property value
    string messageArgString = "";
    while(0 != getline(msgStream, line))
    {
        messageArgString += line + "\n";
    }

    cout << "Setting property:\n  " << destName << destPath << "\n  " <<
            ifaceName << ":" << propName << endl;

    // Set the property
    ProxyBusObject proxy(m_propertyBus, destName.c_str(), destPath.c_str(), 0);
    QStatus err = proxy.IntrospectRemoteObject();
    if(err != ER_OK)
    {
        cout << "Failed to introspect remote object to relay set request: " <<
                QCC_StatusText(err) << endl;
    }

    if(err == ER_OK)
    {
        MsgArg value = util::msgarg::FromString(messageArgString);
        err = proxy.SetProperty(
                ifaceName.c_str(), propName.c_str(), value, 5000);
        if(err != ER_OK)
        {
            cout << "Failed to relay Set request: " <<
                    QCC_StatusText(err) << endl;
        }
    }

    // Return the reply
    SendSetReply(destName, destPath, err);
}

void
XmppTransport::ReceiveSetReply(
    const string& message
    )
{
    // Parse the required information
    istringstream msgStream(message);
    string line, remoteName, objPath, status;

    // First line is the type (get reply)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_SET_PROP_REPLY){ return; }

    if(0 == getline(msgStream, remoteName)){ return; }
    if(0 == getline(msgStream, objPath)){ return; }
    if(0 == getline(msgStream, status)){ return; }

    // Find the bus attachment with this busName
    RemoteBusAttachment* bus = m_connector->GetRemoteAttachment(remoteName);
    if(!bus)
    {
        cout << "No bus attachment to handle incoming Get reply." << endl;
        return;
    }

    // Tell the attachment we received a message reply
    bus->SignalReplyReceived(objPath, status);
}

void
XmppTransport::ReceiveGetAllRequest(
    const string& message
    )
{
    // Parse the required information
    istringstream msgStream(message);
    string line, destName, destPath, ifaceName, memberName;

    // First line is the type (get request)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_GET_ALL){ return; }

    if(0 == getline(msgStream, destName)){ return; }
    if(0 == getline(msgStream, destPath)){ return; }
    if(0 == getline(msgStream, ifaceName)){ return; }
    if(0 == getline(msgStream, memberName)){ return; }

    cout << "Retrieving properties:\n  " << destName << destPath << "\n  " <<
            ifaceName << ":" << memberName << endl;

    // Call the method
    ProxyBusObject proxy(m_propertyBus, destName.c_str(), destPath.c_str(), 0);
    QStatus err = proxy.IntrospectRemoteObject();
    if(err != ER_OK)
    {
        cout << "Failed to introspect remote object to relay GetAll request: "
                << QCC_StatusText(err) << endl;
        return;
    }

    MsgArg values;
    err = proxy.GetAllProperties(ifaceName.c_str(), values, 5000);              //cout << "Got all properties:\n" << util::msgarg::ToString(values, 2) << endl;
    if(err != ER_OK)
    {
        cout << "Failed to get all properties: " << QCC_StatusText(err) << endl;
        return;                                                                 // TODO: send the actual response status back
    }

    // Return the reply
    SendGetAllReply(destName, destPath, values);
}

void
XmppTransport::ReceiveGetAllReply(
    const string& message
    )
{
    // Parse the required information
    istringstream msgStream(message);
    string line, remoteName, objPath;

    // First line is the type (get reply)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_GET_ALL_REPLY){ return; }

    if(0 == getline(msgStream, remoteName)){ return; }
    if(0 == getline(msgStream, objPath)){ return; }

    // The rest is the property values
    string messageArgsString = "";
    while(0 != getline(msgStream, line))
    {
        messageArgsString += line + "\n";
    }
    //util::str::UnescapeXml(messageArgsString);

    // Find the bus attachment with this busName
    RemoteBusAttachment* bus = m_connector->GetRemoteAttachment(remoteName);
    if(!bus)
    {
        cout << "No bus attachment to handle incoming GetAll reply." << endl;
        return;
    }

    // Tell the attachment we received a message reply
    bus->SignalReplyReceived(objPath, messageArgsString);
}

int
XmppTransport::XmppStanzaHandler(
    xmpp_conn_t* const   conn,
    xmpp_stanza_t* const stanza,
    void* const          userdata
    )
{
    XmppTransport* transport = static_cast<XmppTransport*>(userdata);

    // Ignore stanzas from ourself
    string fromLocal = transport->m_chatroom+"/"+transport->m_nickname;
    const char* fromAttr = xmpp_stanza_get_attribute(stanza, "from");
    if(!fromAttr || fromLocal == fromAttr)
    {
        return 1;
    }

    if ( 0 == strcmp("message", xmpp_stanza_get_name(stanza)) )
    {
        xmpp_stanza_t* body = NULL;
        char* buf = NULL;
        size_t bufSize = 0;
        if(0 != (body = xmpp_stanza_get_child_by_name(stanza, "body")) &&
           0 == (bufSize = xmpp_stanza_to_text(body, &buf, &bufSize)))
        {
            string message(buf);
            xmpp_free(xmpp_conn_get_context(conn), buf);

            // Strip the tags from the message
            if(0 != message.find("<body>") &&
                message.length() !=
                message.find("</body>")+strlen("</body>"))
            {
                // Received an empty message
                return 1;
            }
            message = message.substr(strlen("<body>"),
                    message.length()-strlen("<body></body>"));

            // Decompress the message
            message = util::str::Decompress(message);

            // Handle the content of the message
            string typeCode =
                    message.substr(0, message.find_first_of('\n'));
            cout << "Received XMPP message: " << typeCode << endl;

            if(typeCode == ALLJOYN_CODE_ADVERTISEMENT)
            {
                transport->ReceiveAdvertisement(message);
            }
            else if(typeCode == ALLJOYN_CODE_ADVERT_LOST)
            {
                transport->ReceiveAdvertisementLost(message);
            }
            else if(typeCode == ALLJOYN_CODE_ANNOUNCE)
            {
                transport->ReceiveAnnounce(message);
            }
            else if(typeCode == ALLJOYN_CODE_METHOD_CALL)
            {
                transport->ReceiveMethodCall(message);
            }
            else if(typeCode == ALLJOYN_CODE_METHOD_REPLY)
            {
                transport->ReceiveMethodReply(message);
            }
            else if(typeCode == ALLJOYN_CODE_SIGNAL)
            {
                transport->ReceiveSignal(message);
            }
            else if(typeCode == ALLJOYN_CODE_JOIN_REQUEST)
            {
                transport->ReceiveJoinRequest(message);
            }
            else if(typeCode == ALLJOYN_CODE_JOIN_RESPONSE)
            {
                transport->ReceiveJoinResponse(message);
            }
            else if(typeCode == ALLJOYN_CODE_SESSION_JOINED)
            {
                transport->ReceiveSessionJoined(message);
            }
            else if(typeCode == ALLJOYN_CODE_SESSION_LOST)
            {
                transport->ReceiveSessionLost(message);
            }
            else if(typeCode == ALLJOYN_CODE_GET_PROPERTY)
            {
                transport->ReceiveGetRequest(message);
            }
            else if(typeCode == ALLJOYN_CODE_GET_PROP_REPLY)
            {
                transport->ReceiveGetReply(message);
            }
            else if(typeCode == ALLJOYN_CODE_GET_ALL)
            {
                transport->ReceiveGetAllRequest(message);
            }
            else if(typeCode == ALLJOYN_CODE_GET_ALL_REPLY)
            {
                transport->ReceiveGetAllReply(message);
            }
            else
            {
                // Find and call the user-registered callback for this type
                map<string, XMPPConnector::MessageCallback>::iterator it =
                        transport->m_connector->
                        m_messageCallbackMap.find(typeCode);
                if(it != transport->m_connector->m_messageCallbackMap.end())
                {
                    size_t endOfTypePos = message.find_first_of('\n');
                    string sentMessage = ((endOfTypePos >= message.size()) ?
                            "" : message.substr(endOfTypePos+1));

                    XMPPConnector::MessageCallback callback = it->second;
                    (callback.receiver->*(callback.messageHandler))(
                            typeCode, sentMessage, callback.userdata);
                }
                else
                {
                    cout << "Received unrecognized message type: " <<
                            typeCode << endl;
                }
            }
        }
        else
        {
            cout << "Could not parse body from XMPP message." << endl;
        }
    }

    return 1;
}

void
XmppTransport::XmppConnectionHandler(
    xmpp_conn_t* const         conn,
    const xmpp_conn_event_t    event,
    const int                  error,
    xmpp_stream_error_t* const streamError,
    void* const                userdata
    )
{
    XmppTransport* transport = static_cast<XmppTransport*>(userdata);
    XmppConnectionState prevConnState = transport->m_connectionState;

    switch(event)
    {
    case XMPP_CONN_CONNECT:
    {
        transport->m_connectionState = xmpp_connected;

        // Send presence to chatroom
        xmpp_stanza_t* presence = NULL;
        presence = xmpp_stanza_new(xmpp_conn_get_context(conn));
        xmpp_stanza_set_name(presence, "presence");
        xmpp_stanza_set_attribute(presence, "from",
                transport->m_jabberId.c_str());
        xmpp_stanza_set_attribute(presence, "to",
                (transport->m_chatroom+"/"+transport->m_nickname).c_str());

        xmpp_stanza_t* x = xmpp_stanza_new(xmpp_conn_get_context(conn));
        xmpp_stanza_set_name(x, "x");
        xmpp_stanza_set_attribute(x, "xmlns", "http://jabber.org/protocol/muc");

        xmpp_stanza_t* history = xmpp_stanza_new(xmpp_conn_get_context(conn));
        xmpp_stanza_set_name(history, "history");
        xmpp_stanza_set_attribute(history, "maxchars", "0");

        xmpp_stanza_add_child(x, history);
        xmpp_stanza_release(history);
        xmpp_stanza_add_child(presence, x);
        xmpp_stanza_release(x);

        //char* buf = NULL;
        //size_t buflen = 0;
        //xmpp_stanza_to_text(presence, &buf, &buflen);
        cout << "Sending XMPP presence message" << endl;
        //free(buf);

        xmpp_send(conn, presence);
        xmpp_stanza_release(presence);

        break;
    }
    case XMPP_CONN_DISCONNECT:
    {
        cout << "Disconnected from XMPP server." << endl;
        if ( xmpp_aborting == transport->m_connectionState )
        {
            cout << "Exiting." << endl;
 
            // Stop the XMPP event loop
            xmpp_stop(xmpp_conn_get_context(conn));
        }
        else
        {
            if ( prevConnState != xmpp_uninitialized )
            {
                transport->m_connectionState = xmpp_disconnected;
            }
        }
        break;
    }
    case XMPP_CONN_FAIL:
    default:
    {
        cout << "XMPP error occurred. Exiting." << endl;

        transport->m_connectionState = xmpp_error;

        // Stop the XMPP event loop
        xmpp_stop(xmpp_conn_get_context(conn));
        break;
    }
    }

    // Call the connection callback
    if(transport->m_callbackListener && transport->m_connectionCallback)
    {
        (transport->m_callbackListener->*(transport->m_connectionCallback))(
                static_cast<Listener::ConnectionStatus>(event),
                transport->m_callbackUserdata);
    }

    // TODO: send connection status to Gateway Management app
}

const string XmppTransport::ALLJOYN_CODE_ADVERTISEMENT  = "__ADVERTISEMENT";
const string XmppTransport::ALLJOYN_CODE_ADVERT_LOST    = "__ADVERT_LOST";
const string XmppTransport::ALLJOYN_CODE_ANNOUNCE       = "__ANNOUNCE";
const string XmppTransport::ALLJOYN_CODE_METHOD_CALL    = "__METHOD_CALL";
const string XmppTransport::ALLJOYN_CODE_METHOD_REPLY   = "__METHOD_REPLY";
const string XmppTransport::ALLJOYN_CODE_SIGNAL         = "__SIGNAL";
const string XmppTransport::ALLJOYN_CODE_JOIN_REQUEST   = "__JOIN_REQUEST";
const string XmppTransport::ALLJOYN_CODE_JOIN_RESPONSE  = "__JOIN_RESPONSE";
const string XmppTransport::ALLJOYN_CODE_SESSION_JOINED = "__SESSION_JOINED";
const string XmppTransport::ALLJOYN_CODE_SESSION_LOST   = "__SESSION_LOST";
const string XmppTransport::ALLJOYN_CODE_GET_PROPERTY   = "__GET_PROPERTY";
const string XmppTransport::ALLJOYN_CODE_GET_PROP_REPLY = "__GET_PROP_REPLY";
const string XmppTransport::ALLJOYN_CODE_SET_PROPERTY   = "__SET_PROPERTY";
const string XmppTransport::ALLJOYN_CODE_SET_PROP_REPLY = "__SET_PROP_REPLY";
const string XmppTransport::ALLJOYN_CODE_GET_ALL        = "__GET_ALL";
const string XmppTransport::ALLJOYN_CODE_GET_ALL_REPLY  = "__GET_ALL_REPLY";


XMPPConnector::XMPPConnector(
    BusAttachment* bus,
    const string&  appName,
    const string&  xmppJid,
    const string&  xmppPassword,
    const string&  xmppChatroom
    ) :
#ifndef NO_AJ_GATEWAY
    GatewayConnector(bus, appName.c_str()),
#endif // !NO_AJ_GATEWAY
    m_bus(bus),
    m_remoteAttachments()
{
    m_xmppTransport = new XmppTransport( this,
            xmppJid, xmppPassword, xmppChatroom,
            bus->GetGlobalGUIDString().c_str());                                //cout << xmppChatroom << endl;
    m_listener = new AllJoynListener(this, m_xmppTransport, bus);

    pthread_mutex_init(&m_remoteAttachmentsMutex, NULL);

    m_bus->RegisterBusListener(*m_listener);
}

XMPPConnector::~XMPPConnector()
{
    pthread_mutex_lock(&m_remoteAttachmentsMutex);
    list<RemoteBusAttachment*>::iterator it;
    for(it = m_remoteAttachments.begin(); it != m_remoteAttachments.end(); ++it)
    {
        delete(*it);
    }
    pthread_mutex_unlock(&m_remoteAttachmentsMutex);
    pthread_mutex_destroy(&m_remoteAttachmentsMutex);

    m_bus->UnregisterBusListener(*m_listener);
    delete m_listener;
    delete m_xmppTransport;
}

void
XMPPConnector::AddSessionPortMatch(
    const string& interfaceName,
    SessionPort   port
    )
{
    m_sessionPortMap[interfaceName].push_back(port);
}

QStatus
XMPPConnector::Start()
{
    class XmppListener :
        public XmppTransport::Listener
    {
    public:
        void ConnectionCallback(
            ConnectionStatus status,
            void*            userdata
            )
        {
            XMPPConnector* connector = static_cast<XMPPConnector*>(userdata);

            switch(status)
            {
            case XMPP_CONNECT:
            {
                // Start listening for advertisements
                QStatus err = connector->m_bus->FindAdvertisedName("");
                if(err != ER_OK)
                {
                    cout << "Could not find advertised names: " <<
                            QCC_StatusText(err) << endl;
                }

                // Listen for announcements
                err = AnnouncementRegistrar::RegisterAnnounceHandler(
                        *connector->m_bus, *connector->m_listener, NULL, 0);
                if(err != ER_OK)
                {
                    cout << "Could not register Announcement handler: " <<
                            QCC_StatusText(err) << endl;
                }

                break;
            }
            case XMPP_DISCONNECT:
            case XMPP_FAIL:
            default:
            {
                // Stop listening for advertisements and announcements
                AnnouncementRegistrar::UnRegisterAnnounceHandler(
                        *connector->m_bus, *connector->m_listener, NULL, 0);

                connector->m_bus->CancelFindAdvertisedName("");

                break;
            }
            }
        }
    } xmppListener;

    m_xmppTransport->SetConnectionCallback(
            &xmppListener,
            static_cast<XmppTransport::Listener::ConnectionCallback>(
            &XmppListener::ConnectionCallback), this);

    // Listen for XMPP messages. Blocks until transport.Stop() is called.
    QStatus err = m_xmppTransport->Run();

    m_xmppTransport->RemoveConnectionCallback();

    return err;
}

void XMPPConnector::Stop()
{
    m_xmppTransport->Stop();
}

bool
XMPPConnector::IsAdvertisingName(
    const string& name
    )
{
    bool result = false;

    pthread_mutex_lock(&m_remoteAttachmentsMutex);
    list<RemoteBusAttachment*>::const_iterator it;
    for(it = m_remoteAttachments.begin(); it != m_remoteAttachments.end(); ++it)
    {
        if(name == (*it)->WellKnownName())
        {
            result = true;
            break;
        }
    }
    pthread_mutex_unlock(&m_remoteAttachmentsMutex);
    return result;
}

void
XMPPConnector::RegisterMessageHandler(
    const string&                   key,
    MessageReceiver*                receiver,
    MessageReceiver::MessageHandler messageHandler,
    void*                           userdata
    )
{
    if(!receiver || !messageHandler)
    {
        return;
    }

    MessageCallback& callback = m_messageCallbackMap[key];
    callback.receiver = receiver;
    callback.messageHandler = messageHandler;
    callback.userdata = userdata;
}

void
XMPPConnector::UnregisterMessageHandler(
    const string& key
    )
{
    m_messageCallbackMap.erase(key);
}

void
XMPPConnector::SendMessage(
    const string& key,
    const string& message
    )
{
    m_xmppTransport->SendMessage(key+"\n"+message, key);
}

RemoteBusAttachment*
XMPPConnector::GetRemoteAttachment(
    const string&                          remoteName,
    const vector<RemoteObjectDescription>* objects
    )
{
    RemoteBusAttachment* result = NULL;
    pthread_mutex_lock(&m_remoteAttachmentsMutex);

    list<RemoteBusAttachment*>::iterator it;
    for(it = m_remoteAttachments.begin(); it != m_remoteAttachments.end(); ++it)
    {
        if(remoteName == (*it)->RemoteName())
        {
            result = *it;
            break;
        }
    }

    if(!result && objects)
    {
        cout << "Creating new remote bus attachment: " << remoteName << endl;

        // We did not find a match. Create the new attachment.
        QStatus err = ER_OK;
        map<string, vector<SessionPort> > portsToBind;
        result = new RemoteBusAttachment(remoteName, m_xmppTransport);

        vector<RemoteObjectDescription>::const_iterator objIter;
        for(objIter = objects->begin(); objIter != objects->end(); ++objIter)
        {
            string objPath = objIter->path;
            vector<const InterfaceDescription*> interfaces;

            // Get the interface descriptions
            map<string, string>::const_iterator ifaceIter;
            for(ifaceIter = objIter->interfaces.begin();
                ifaceIter != objIter->interfaces.end();
                ++ifaceIter)
            {
                string ifaceName = ifaceIter->first;
                string ifaceXml  = ifaceIter->second;

                err = result->CreateInterfacesFromXml(ifaceXml.c_str());
                if(err == ER_OK)
                {
                    const InterfaceDescription* newInterface =
                            result->GetInterface(ifaceName.c_str());
                    if(newInterface)
                    {
                        interfaces.push_back(newInterface);

                        // Any SessionPorts to bind?
                        map<string, vector<SessionPort> >::iterator spMapIter =
                                m_sessionPortMap.find(ifaceName);
                        if(spMapIter != m_sessionPortMap.end())
                        {
                            portsToBind[ifaceName] = spMapIter->second;
                        }
                    }
                    else
                    {
                        err = ER_FAIL;
                    }
                }

                if(err != ER_OK)
                {
                    cout << "Failed to create InterfaceDescription " <<
                            ifaceName << ": " <<
                            QCC_StatusText(err) << endl;
                    break;
                }
            }
            if(err != ER_OK)
            {
                break;
            }

            // Add the bus object.
            err = result->AddRemoteObject(objPath, interfaces);
            if(err != ER_OK)
            {
                cout << "Failed to add remote object " << objPath << ": " <<
                        QCC_StatusText(err) << endl;
                break;
            }
        }

        // Start and connect the new attachment.
        if(err == ER_OK)
        {
            err = result->Start();
            if(err != ER_OK)
            {
                cout << "Failed to start new bus attachment " << remoteName <<
                        ": " << QCC_StatusText(err) << endl;
            }
        }
        if(err == ER_OK)
        {
            err = result->Connect();
            if(err != ER_OK)
            {
                cout << "Failed to connect new bus attachment " << remoteName <<
                        ": " << QCC_StatusText(err) << endl;
            }
        }

        // Bind any necessary SessionPorts
        if(err == ER_OK)
        {
            map<string, vector<SessionPort> >::iterator spMapIter;
            for(spMapIter = portsToBind.begin();
                spMapIter != portsToBind.end();
                ++spMapIter)
            {
                vector<SessionPort>::iterator spIter;
                for(spIter = spMapIter->second.begin();
                    spIter != spMapIter->second.end();
                    ++spIter)
                {
                    cout << "Binding session port " << *spIter <<
                            " for interface " << spMapIter->first << endl;
                    err = result->BindSessionPort(*spIter);
                    if(err != ER_OK)
                    {
                        cout << "Failed to bind session port: " <<
                                QCC_StatusText(err) << endl;
                        break;
                    }
                }

                if(err != ER_OK)
                {
                    break;
                }
            }
        }

        if(err == ER_OK)
        {
            m_remoteAttachments.push_back(result);
        }
        else
        {
            delete result;
            result = NULL;
        }
    }

    pthread_mutex_unlock(&m_remoteAttachmentsMutex);
    return result;
}

RemoteBusAttachment*
XMPPConnector::GetRemoteAttachmentByAdvertisedName(
    const string& advertisedName
    )
{
    RemoteBusAttachment* result = NULL;

    pthread_mutex_lock(&m_remoteAttachmentsMutex);
    list<RemoteBusAttachment*>::iterator it;
    for(it = m_remoteAttachments.begin(); it != m_remoteAttachments.end(); ++it)
    {
        if(advertisedName == (*it)->WellKnownName())
        {
            result = *it;
            break;
        }
    }
    pthread_mutex_unlock(&m_remoteAttachmentsMutex);

    return result;
}

void XMPPConnector::DeleteRemoteAttachment(
    RemoteBusAttachment*& attachment
    )
{
    if(!attachment)
    {
        return;
    }

    string name = attachment->RemoteName();

    attachment->Disconnect();
    attachment->Stop();
    attachment->Join();

    pthread_mutex_lock(&m_remoteAttachmentsMutex);
    m_remoteAttachments.remove(attachment);
    pthread_mutex_unlock(&m_remoteAttachmentsMutex);

    delete(attachment);
    attachment = NULL;

    cout << "Deleted remote bus attachment: " << name << endl;
}
