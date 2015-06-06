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

#ifndef REMOTEBUSOBJECT_H_
#define REMOTEBUSOBJECT_H_

#include <string>
#include <vector>
#include "alljoyn/BusObject.h"
#include "alljoyn/InterfaceDescription.h"
#include "Reply.h"

class XMPPConnector;
class RemoteBusAttachment;

class RemoteBusObject :
    public ajn::BusObject
{
public:
    RemoteBusObject(
        RemoteBusAttachment* bus,
        const std::string&   path,
        XMPPConnector*       connector
        );

    virtual
    ~RemoteBusObject();

    void
    AllJoynMethodHandler(
        const ajn::InterfaceDescription::Member* member,
        ajn::Message&                            message
        );

    QStatus
    ImplementInterfaces(
        const std::vector<const ajn::InterfaceDescription*>& interfaces
        );

    void
    SendSignal(
        const std::string&              destination,
        ajn::SessionId                  sessionId,
        const std::string&              ifaceName,
        const std::string&              ifaceMember,
        const std::vector<ajn::MsgArg>& msgArgs
        );

    void
    SignalReplyReceived(
        const std::string& replyStr
        );

protected:
    QStatus
    Get(
        const char*  ifaceName,
        const char*  propName,
        ajn::MsgArg& val
        );

    QStatus
    Set(
        const char*  ifaceName,
        const char*  propName,
        ajn::MsgArg& val
        );

    void
    GetAllProps(
        const ajn::InterfaceDescription::Member* member,
        ajn::Message&                            msg
        );

    void
    ObjectRegistered();

private:

    RemoteBusAttachment*                           m_bus;
    std::vector<const ajn::InterfaceDescription*>  m_interfaces;
    Reply                                          m_reply;
    XMPPConnector*                                 m_connector;
};

#endif // REMOTEBUSOBJECT_H_
