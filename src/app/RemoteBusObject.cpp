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
}

QStatus
RemoteBusObject::ImplementInterfaces(
    const vector<const InterfaceDescription*>& interfaces
    )
{
    vector<const InterfaceDescription*>::const_iterator it;
    for(it = interfaces.begin(); it != interfaces.end(); ++it)
    {
        QStatus err = AddInterface(**it);
        if(ER_OK != err)
        {
            LOG_RELEASE("Failed to add interface %s: %s",
                    (*it)->GetName(), QCC_StatusText(err));
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
        return static_cast<QStatus>(strtol(replyStr.c_str(), NULL, 10));
    }
    else
    {
        return ER_BUS_NO_SUCH_PROPERTY;
    }
}

void
RemoteBusObject::GetAllProps(
    const InterfaceDescription::Member* member,
    Message&                            msg
    )
{
    LOG_DEBUG("Received AllJoyn GetAllProps request for %s: %s",
            member->iface->GetName(), member->name.c_str());
    bool replyReceived = false;
    string replyStr;

    {
        ScopedTransactionLocker transLock(&m_reply);

        m_connector->SendGetAllRequest(
                member, m_bus->RemoteName(), GetPath());

        // Wait for the XMPP response signal
        transLock.ReceiveReply(replyReceived, replyStr);
    }

    if(replyReceived)
    {
        MsgArg result = util::msgarg::FromString(replyStr);
        QStatus err = MethodReply(msg, &result, 1);
        if(err != ER_OK)
        {
            LOG_RELEASE("Failed to send method reply to GetAllProps request: %s",
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