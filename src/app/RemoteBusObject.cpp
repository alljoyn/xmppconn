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

#include "RemoteBusObject.h"
#include "RemoteBusAttachment.h"
#include "ScopedTransactionLocker.h"
#include "XMPPConnector.h"

using namespace std;
using namespace ajn;

RemoteBusObject::RemoteBusObject(
    RemoteBusAttachment* bus,
    const string&        path,
    XMPPConnector*       connector
    ) :
    BusObject(path.c_str()),
    m_bus(bus),
    m_connector(connector)
{}

RemoteBusObject::~RemoteBusObject()
{}

void
RemoteBusObject::AllJoynMethodHandler(
    const InterfaceDescription::Member* member,
    Message&                            message
    )
{
    LOG_DEBUG("Received method call: %s", member->name.c_str());
    bool replyReceived = false;
    string replyStr;

    {
        ScopedTransactionLocker transLock(&m_reply);

        m_connector->SendMethodCall(
                member, message, m_bus->RemoteName(), GetPath());

        // Wait for the XMPP response signal
        transLock.ReceiveReply(replyReceived, replyStr);
    }

    if(replyReceived)
    {
        // A successful reply is a MsgArg XML object and starts with a '<'
        const string successReplyPrefix("<");
        if(0 == replyStr.compare(0, successReplyPrefix.length(), successReplyPrefix))
        {
            vector<MsgArg> replyArgs =
                    util::msgarg::VectorFromString(replyStr);
            QStatus err = MethodReply(
                    message, &replyArgs[0], replyArgs.size());
            if(err != ER_OK)
            {
                LOG_RELEASE("Failed to reply to method call: %s",
                        QCC_StatusText(err));
            }
        }
        else
        {
            // Check for an error result and generate a new reply
            stringstream ss(replyStr);
            uint32_t status = 0;
            ss >> status;
            if((ss.rdstate() & stringstream::failbit ) != 0)
            {
                // The conversion from string to uint32_t failed
                LOG_RELEASE("Failed to convert from status code to uint32_t when handling a method call!");
                status = ER_FAIL;
            }
            QStatus err = MethodReply(
                    message, static_cast<QStatus>(status));
            if(err != ER_OK)
            {
                LOG_RELEASE("Failed to send error reply of '%s' to method call: %s",
                        replyStr.c_str(), QCC_StatusText(err));
            }
        }
    }
    else
    {
        LOG_RELEASE("Failed to receive the method reply for %s in a timely manner.",
                member->name.c_str());
        QStatus err = MethodReply(
            message, ER_FAIL);
        if(err != ER_OK)
        {
            LOG_RELEASE("Failed to send an ER_FAIL reply to a method call: %s",
                QCC_StatusText(err));
        }
    }
}

QStatus
RemoteBusObject::ImplementInterfaces(
    const vector<InterfaceDescriptionData>& interfaces
    )
{
    FNLOG;
    vector<InterfaceDescriptionData>::const_iterator it;
    for(it = interfaces.begin(); it != interfaces.end(); ++it)
    {
        QStatus err = AddInterface(*it->desc, it->announced ? ANNOUNCED : UNANNOUNCED);
        if(ER_OK != err)
        {
            LOG_RELEASE("Failed to add interface %s: %s",
                    it->desc->GetName(), QCC_StatusText(err));
            return err;
        }

        m_interfaces.push_back(*it);

        // Register method handlers
        size_t numMembers = it->desc->GetMembers();
        InterfaceDescription::Member** interfaceMembers =
                new InterfaceDescription::Member*[numMembers];
        numMembers = it->desc->GetMembers(
                (const InterfaceDescription::Member**)interfaceMembers,
                numMembers);

        for(uint32_t i = 0; i < numMembers; ++i)
        {
            if(interfaceMembers[i]->memberType == MESSAGE_SIGNAL)
            {
                err = m_bus->RegisterSignalHandler(interfaceMembers[i]);
                LOG_VERBOSE("Registered signal handler for %s%s: %s",
                    m_bus->RemoteName().c_str(), GetPath(),
                    interfaceMembers[i]->name.c_str());
            }
            else
            {
                err = AddMethodHandler(interfaceMembers[i],
                        static_cast<MessageReceiver::MethodHandler>(
                        &RemoteBusObject::AllJoynMethodHandler));
                if(err == ER_OK)
                {
                    LOG_VERBOSE("Registered method handler for %s%s: %s",
                        m_bus->RemoteName().c_str(), GetPath(),
                        interfaceMembers[i]->name.c_str());
                }
            }
            if(err != ER_OK)
            {
                LOG_RELEASE("Failed to add method handler for %s: %s",
                        interfaceMembers[i]->name.c_str(), QCC_StatusText(err));
            }
        }

        delete[] interfaceMembers;
    }

    return ER_OK;
}

void
RemoteBusObject::SendSignal(
    const string&         destination,
    SessionId             sessionId,
    const string&         ifaceName,
    const string&         ifaceMember,
    const vector<MsgArg>& msgArgs
    )
{
    QStatus err = ER_FAIL;

    // Get the InterfaceDescription::Member
    vector<InterfaceDescriptionData>::iterator ifaceIter;
    for(ifaceIter = m_interfaces.begin();
        ifaceIter != m_interfaces.end();
        ++ifaceIter)
    {
        if(ifaceName == ifaceIter->desc->GetName())
        {
            size_t numMembers = ifaceIter->desc->GetMembers();
            InterfaceDescription::Member** members =
                    new InterfaceDescription::Member*[numMembers];
            numMembers = ifaceIter->desc->GetMembers(
                    (const InterfaceDescription::Member**)members,
                    numMembers);
            for(uint32_t i = 0; i < numMembers; ++i)
            {
                if(ifaceMember == members[i]->name.c_str())
                {
                    uint16_t ttl = 0;
                    uint8_t flags = 0;
                    Message* msg = NULL;
                    if(sessionId == 0)
                    {
                        flags = ALLJOYN_FLAG_SESSIONLESS;
                    }
                    err = Signal(
                            (destination.empty() ?
                            NULL : destination.c_str()), sessionId,
                            *members[i], (msgArgs.size() > 0 ? &msgArgs[0] : NULL), msgArgs.size(),
                            ttl, flags, msg);
                    if(err != ER_OK)
                    {
                        LOG_RELEASE("Failed to send signal: %s",
                                QCC_StatusText(err));
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
        LOG_RELEASE("Could not find interface member of signal to relay! %s",
                QCC_StatusText(err));
    }
}

void
RemoteBusObject::SignalReplyReceived(
    const string& replyStr
    )
{
    m_reply.SetReply(replyStr);
}

QStatus
RemoteBusObject::Get(
    const char* ifaceName,
    const char* propName,
    MsgArg&     val
    )
{
    LOG_DEBUG("Received AllJoyn Get request for %s: %s",
            ifaceName, propName);
    bool replyReceived = false;
    string replyStr;

    {
        ScopedTransactionLocker transLock(&m_reply);

        m_connector->SendGetRequest(
                ifaceName, propName, m_bus->RemoteName(), GetPath());

        // Wait for the XMPP response signal
        transLock.ReceiveReply(replyReceived, replyStr);
    }

    if(replyReceived)
    {
        // A successful reply is a MsgArg XML object and starts with a '<'
        const string successReplyPrefix("<");
        if(0 == replyStr.compare(0, successReplyPrefix.length(), successReplyPrefix))
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
            // Check for an error result and generate a new reply
            stringstream ss(replyStr);
            uint32_t status = 0;
            ss >> status;
            if((ss.rdstate() & stringstream::failbit ) != 0)
            {
                // The conversion from string to uint32_t failed
                LOG_RELEASE("Failed to convert from status code to uint32_t when handling Get call!");
                status = ER_FAIL;
            }
            return static_cast<QStatus>(status);
        }
    }
    else
    {
        LOG_RELEASE("Failed to receive the get reply for %s, %s in a timely manner.",
                ifaceName, propName);
        return ER_FAIL;
    }
}

QStatus
RemoteBusObject::Set(
    const char* ifaceName,
    const char* propName,
    MsgArg&     val
    )
{
    LOG_DEBUG("Received AllJoyn Set request for %s: %s",
            ifaceName, propName);
    bool replyReceived = false;
    string replyStr;

    {
        ScopedTransactionLocker transLock(&m_reply);

        m_connector->SendSetRequest(
                ifaceName, propName, val, m_bus->RemoteName(), GetPath());

        // Wait for the XMPP response signal
        transLock.ReceiveReply(replyReceived, replyStr);
    }

    if(replyReceived)
    {
        // Check for an error result and generate a new reply
        stringstream ss(replyStr);
        uint32_t status = 0;
        ss >> status;
        if((ss.rdstate() & stringstream::failbit ) != 0)
        {
            // The conversion from string to uint32_t failed
            status = ER_FAIL;
            LOG_RELEASE("Failed to convert from status code to uint32_t when handling Set call!");
        }
       return static_cast<QStatus>(status);
    }
    else
    {
        LOG_RELEASE("Failed to receive the Set reply for %s, %s in a timely manner.",
                ifaceName, propName);
        return ER_FAIL;
    }
}

void
RemoteBusObject::GetAllProps(
    const InterfaceDescription::Member* member,
    Message&                            msg
    )
{
	// NOTE: The only way to get the correct interface name is to dig
    //  through the Message structure. It should always be the first
    //  argument, and it should be a string value.
    string ifaceName(msg->GetArg(0)->v_string.str);
    LOG_DEBUG("Received AllJoyn GetAllProps request for %s: %s",
        ifaceName.c_str(), member->name.c_str());
    bool replyReceived = false;
    string replyStr;

    {
        ScopedTransactionLocker transLock(&m_reply);

        m_connector->SendGetAllRequest(
                ifaceName, member, m_bus->RemoteName(), GetPath());

        // Wait for the XMPP response signal
        transLock.ReceiveReply(replyReceived, replyStr);
    }

    if(replyReceived)
    {
        // A successful reply is a MsgArg XML object and starts with a '<'
        const string successReplyPrefix("<");
        if(0 == replyStr.compare(0, successReplyPrefix.length(), successReplyPrefix))
        {
            MsgArg result = util::msgarg::FromString(replyStr);
            QStatus err = MethodReply(msg, &result, 1);
            if(err != ER_OK)
            {
                LOG_RELEASE("Failed to send method reply to GetAllProps request: %s",
                        QCC_StatusText(err));
            }
        }
        else
        {
            // Check for an error result and generate a new reply
            stringstream ss(replyStr);
            uint32_t status = 0;
            ss >> status;
            if((ss.rdstate() & stringstream::failbit ) != 0)
            {
                // The conversion from string to uint32_t failed
                LOG_RELEASE("Failed to convert from status code to uint32_t when handling GetAllProps call!");
                status = ER_FAIL;
            }
            QStatus err = MethodReply(
                    msg, static_cast<QStatus>(status));
            if(err != ER_OK)
            {
                LOG_RELEASE("Failed to send error reply of '%s' to GetAllProps request: %s",
                        replyStr.c_str(), QCC_StatusText(err));
            }
        }
    }
    else
    {
        LOG_RELEASE("Failed to receive the GetAllProps reply for %s, %s in a timely manner.",
                ifaceName.c_str(), member->name.c_str());
        QStatus err = MethodReply(
                msg, ER_FAIL);
        if(err != ER_OK)
        {
            LOG_RELEASE("Failed to send an ER_FAIL reply to GetAllProps request: %s",
                    QCC_StatusText(err));
        }
    }
}

void
RemoteBusObject::ObjectRegistered()
{
    string remoteName = m_bus->RemoteName();
    LOG_DEBUG("%s %s registered",
            (remoteName.at(0) == ':' ? bus->GetUniqueName().c_str() : remoteName.c_str()),
            GetPath());
}
