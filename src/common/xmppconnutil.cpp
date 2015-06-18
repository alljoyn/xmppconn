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

#include "xmppconnutil.h"
#include <string.h>
#include <sstream>
#include <string>
#include <iostream>

namespace util {
volatile bool _dbglogging = false;
volatile bool _verboselogging = false;
namespace str {

    /* Replace all occurances in 'str' of string 'from' with string 'to'. */
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
    }

    /* Escape certain characters in an XML string. */
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
    }

    /* Unescape the escape sequences in an XML string. */
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
    }

    /* Trim the whitespace before and after a string. */
    string
    Trim(
        const string& str
        )
    {
        return qcc::Trim(str.c_str()).c_str();
    }

    vector<string>
    Split(
        const string &str,
        char delim
        )
    {
        vector<string> elems;
        std::stringstream ss(str);
        string item;
        while (getline(ss, item, delim)) {
            elems.push_back(item);
        }
        return elems;
    }

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
    string
    ToString(
        const MsgArg& arg,
        size_t        indent
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
    string
    ToString(
        const MsgArg* args,
        size_t        numArgs,
        size_t        indent
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
    vector<MsgArg>
    VectorFromString(
        string content
        );

    /* Convert a string back into a MsgArg. */
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
            LOG_RELEASE("Could not create MsgArg from string: %s",
                    QCC_StatusText(status));
        }
        return result;
    }

    /* Convert a string into a vector of MsgArgs. */
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

    class GetBusObjectsTreeNode;

    struct GetBusObjectsAsyncContext {
        ProxyBusObject*                                rootProxy;
        GetBusObjectsAsyncReceiver*                    receiver;
        GetBusObjectsAsyncReceiver::CallbackHandler    handler;
        GetBusObjectsAsyncIntrospectReceiver*          introspectReceiver;
        vector<BusObjectInfo>*                         busObjects;
        GetBusObjectsTreeNode*                         completionTree;
        void*                                          internalCtx;
    };

    class GetBusObjectsTreeNode {
        GetBusObjectsTreeNode() : ctx(0), parent(0), completed(false) {}
    public:
        GetBusObjectsTreeNode(string const& name, GetBusObjectsAsyncContext* ctx) : name(name), ctx(ctx), parent(0), completed(false) {}
        GetBusObjectsTreeNode(string const& name, GetBusObjectsAsyncContext* ctx, GetBusObjectsTreeNode* parent) : name(name), ctx(ctx), parent(parent), completed(false) {}
        void AddChild( GetBusObjectsTreeNode* node ) {
            children.push_back(node);
        }
        bool IsRoot() const {
            return !parent;
        }
        bool IsTreeComplete() const {
            const GetBusObjectsTreeNode* root = this;
            while ( root->parent ) {
                root = root->parent;
            }
            return RecursiveIsNodeComplete(root);
        }
        bool IsComplete() const {
            return completed;
        }
        void Complete() {
            LOG_VERBOSE("Marked node %s as complete", name.c_str());
            completed = true;
        }
        void DeleteTree() {
            GetBusObjectsTreeNode* root = this;
            while ( root->parent ) {
                root = root->parent;
            }
            RecursiveDelete( root );
            delete this; // bye bye!
        }
        GetBusObjectsAsyncContext* GetContext() const {
            return ctx;
        }
    private:
        string name;
        GetBusObjectsAsyncContext* ctx;
        GetBusObjectsTreeNode* parent;
        list<GetBusObjectsTreeNode*> children;
        bool completed;

        bool RecursiveIsNodeComplete( const GetBusObjectsTreeNode* node ) const {
            if ( !node->IsComplete() ) {
                return false;
            }
            for ( list<GetBusObjectsTreeNode*>::const_iterator it(node->children.begin());
                node->children.end() != it; ++it )
            {
                if ( !RecursiveIsNodeComplete(*it) ) {
                    return false;
                }
            }
            LOG_VERBOSE("Node %s is complete!!", name.c_str());
            return true;
        }
        void RecursiveDelete( GetBusObjectsTreeNode* node ) {
            for ( list<GetBusObjectsTreeNode*>::iterator it(node->children.begin());
                node->children.end() != it; ++it )
            {
                RecursiveDelete(*it);
            }
        }
    };

    void GetBusObjectsAsyncInternal(
        ProxyBusObject*        proxy,
        GetBusObjectsTreeNode* node
        )
    {
        QStatus err = proxy->IntrospectRemoteObjectAsync(
            node->GetContext()->introspectReceiver,
            static_cast<ProxyBusObject::Listener::IntrospectCB>(
                &GetBusObjectsAsyncIntrospectReceiver::GetBusObjectsAsyncIntrospectCallback),
            node);
        if(err != ER_OK)
        {
            LOG_RELEASE("Failed recursive asynchronous introspect for bus objects: %s",
                    QCC_StatusText(err));
            return;
        }
    }

    void GetBusObjectsAsyncIntrospectReceiver::GetBusObjectsAsyncIntrospectCallback(
        QStatus         status,
        ProxyBusObject* proxy,
        void*           context
        )
    {
        FNLOG
        GetBusObjectsTreeNode* node = reinterpret_cast<GetBusObjectsTreeNode*>(context);
        GetBusObjectsAsyncContext* ctx = node->GetContext();

        BusObjectInfo thisObj;
        thisObj.path = proxy->GetPath().c_str();
        if(thisObj.path.empty()) {
            thisObj.path = "/";
        }
                                                                                //cout << "  " << thisObj.path << endl;
        // Get the interfaces implemented at this object path
        size_t numIfaces = proxy->GetInterfaces();
        if(numIfaces != 0)
        {
            InterfaceDescription** ifaceList =
                    new InterfaceDescription*[numIfaces];
            numIfaces = proxy->GetInterfaces(
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
                ctx->busObjects->push_back(thisObj);
            }
        }

        // Set our completion node as complete since we just finished
        //  the work for this node
        node->Complete();

        // Get the children of this object path
        size_t num_children = proxy->GetChildren();

        if(num_children != 0)
        {
            ProxyBusObject** children = new ProxyBusObject*[num_children]; // will be deleted later
            num_children = proxy->GetChildren(children, num_children);

            for(uint32_t i = 0; i < num_children; ++i)
            {
                string childName = children[i]->GetPath().c_str();
                LOG_VERBOSE("Child %d, Path: %s", i, childName.c_str());
                GetBusObjectsTreeNode* childNode = new GetBusObjectsTreeNode(childName, ctx, node);
                node->AddChild( childNode );
                GetBusObjectsAsyncInternal(children[i], childNode);
            }

        }

        // If we're handling the root proxy object we now need to call
        //  the callback
        if ( node->IsTreeComplete() )
        {
            LOG_VERBOSE("Tree is complete.");
            ((ctx->receiver)->*(ctx->handler))(proxy, *ctx->busObjects, ctx->internalCtx);
            delete ctx->busObjects;
            delete ctx->introspectReceiver;
            //delete ctx->rootProxy; // TODO: Need to figure out how to delete this properly
            delete ctx;
            node->DeleteTree(); // node pointer is now invalid, don't access it
        }
    }

    /* Recursively get BusObject information from an attachment. */
    void
    GetBusObjectsRecursive(
        vector<BusObjectInfo>& busObjects,
        ProxyBusObject&        proxy
        )
    {
        FNLOG
        QStatus err = proxy.IntrospectRemoteObject(500);
        if(err != ER_OK)
        {
            LOG_RELEASE("Failed to introspect remote object %s (%s): %s",
                    proxy.GetServiceName().c_str(), proxy.GetPath().c_str(),
                    QCC_StatusText(err));
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

    /* Asynchronously get BusObject information from an attachment. */
    void
    GetBusObjectsAsync(
        ProxyBusObject*                                proxy,
        GetBusObjectsAsyncReceiver*                    receiver,
        GetBusObjectsAsyncReceiver::CallbackHandler    handler,
        void*                                          context
        )
    {
        FNLOG
        vector<BusObjectInfo>* busObjects = new vector<BusObjectInfo>();
        GetBusObjectsAsyncIntrospectReceiver* introspectReceiver = new
            GetBusObjectsAsyncIntrospectReceiver();
        GetBusObjectsAsyncContext* ctx = new GetBusObjectsAsyncContext();
        ctx->rootProxy = proxy;
        ctx->receiver = receiver;
        ctx->handler = handler;
        ctx->introspectReceiver = introspectReceiver;
        ctx->busObjects = busObjects;
        ctx->internalCtx = context;
        ctx->completionTree = new GetBusObjectsTreeNode( proxy->GetPath().c_str(), ctx );

        QStatus err = proxy->IntrospectRemoteObjectAsync(
            introspectReceiver,
            static_cast<ProxyBusObject::Listener::IntrospectCB>(
                &GetBusObjectsAsyncIntrospectReceiver::GetBusObjectsAsyncIntrospectCallback),
            ctx->completionTree);
        if(err != ER_OK)
        {
            LOG_RELEASE("Failed asynchronous introspect for bus objects: %s",
                    QCC_StatusText(err));
            return;
        }
    }

} // namespace bus
} // namespace util
