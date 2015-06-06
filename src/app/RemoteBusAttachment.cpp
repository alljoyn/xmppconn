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

#include "RemoteBusAttachment.h"
#include "alljoyn/about/AnnounceHandler.h"
#include "XMPPConnector.h"

using namespace std;
using namespace ajn;
using namespace ajn::services;

RemoteBusAttachment::RemoteBusAttachment(
    const string&  remoteName,
    XMPPConnector* connector
    ) :
    BusAttachment(remoteName.c_str(), true),
    m_connector(connector),
    m_remoteName(remoteName),
    m_wellKnownName(""),
    m_listener(this, connector),
    m_objects(),
    m_activeSessions(),
    m_aboutPropertyStore(NULL),
    m_aboutBusObject(NULL)
{
    pthread_mutex_init(&m_activeSessionsMutex, NULL);

    RegisterBusListener(m_listener);
}

RemoteBusAttachment::~RemoteBusAttachment()
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
RemoteBusAttachment::BindSessionPort(
    SessionPort port
    )
{
    SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true,
            SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
    return BusAttachment::BindSessionPort(port, opts, m_listener);
}

QStatus
RemoteBusAttachment::JoinSession(
    const string& host,
    SessionPort   port,
    SessionId&    id
    )
{
    FNLOG
    SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true,
            SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
    return BusAttachment::JoinSession(
            host.c_str(), port, &m_listener, id, opts);
}

QStatus
RemoteBusAttachment::RegisterSignalHandler(
    const InterfaceDescription::Member* member
    )
{
    FNLOG
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
RemoteBusAttachment::AllJoynSignalHandler(
    const InterfaceDescription::Member* member,
    const char*                         srcPath,
    Message&                            message
    )
{
    LOG_DEBUG("Received signal from %s", message->GetSender());
    m_connector->SendSignal(member, srcPath, message);
}

QStatus
RemoteBusAttachment::AddRemoteObject(
    const string&                       path,
    vector<const InterfaceDescription*> interfaces
    )
{
    QStatus err = ER_OK;
    RemoteBusObject* newObj = new RemoteBusObject(this, path, m_connector); // TODO: why pointers?

    err = newObj->ImplementInterfaces(interfaces);
    if(err != ER_OK)
    {
        delete newObj;
        return err;
    }

    err = RegisterBusObject(*newObj);
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to register remote bus object: %s",
                QCC_StatusText(err));
        delete newObj;
        return err;
    }

    m_objects.push_back(newObj);
    return err;
}

string
RemoteBusAttachment::RequestWellKnownName(
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
            LOG_RELEASE("Could not acquire well known name %s: %s",
                    remoteName.c_str(), QCC_StatusText(err));
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
RemoteBusAttachment::AddSession(
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
RemoteBusAttachment::RemoveSession(
    SessionId localId
    )
{
    pthread_mutex_lock(&m_activeSessionsMutex);
    m_activeSessions.erase(localId);
    pthread_mutex_unlock(&m_activeSessionsMutex);
}

SessionId
RemoteBusAttachment::GetLocalSessionId(
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
RemoteBusAttachment::GetSessionIdByPeer(
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
RemoteBusAttachment::GetPeerBySessionId(
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
RemoteBusAttachment::RelayAnnouncement(
    uint16_t                                   version,
    uint16_t                                   port,
    const string&                              busName,
    const AnnounceHandler::ObjectDescriptions& objectDescs,
    const AnnounceHandler::AboutData&          aboutData
    )
{
    QStatus err = ER_OK;
    LOG_DEBUG("Relaying announcement for %s", m_wellKnownName.c_str());

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
        LOG_RELEASE("Failed to set About announcement args for %s: %s",
                m_wellKnownName.c_str(), QCC_StatusText(err));
        delete m_aboutPropertyStore;
        m_aboutPropertyStore = 0;
        return;
    }

    m_aboutBusObject = new AboutBusObject(this, *m_aboutPropertyStore);
    err = m_aboutBusObject->AddObjectDescriptions(objectDescs);
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to add About object descriptions for %s: %s",
                m_wellKnownName.c_str(), QCC_StatusText(err));
        return;
    }

    // Bind and register the announced session port
    err = BindSessionPort(port);
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to bind About announcement session port for %s: %s",
                m_wellKnownName.c_str(), QCC_StatusText(err));
        return;
    }
    err = m_aboutBusObject->Register(port);
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to register About announcement port for %s: %s",
                m_wellKnownName.c_str(), QCC_StatusText(err));
        return;
    }

    // Register the bus object
    err = RegisterBusObject(*m_aboutBusObject);
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to register AboutService bus object: %s",
                QCC_StatusText(err));
    }

    // Make the announcement
    err = m_aboutBusObject->Announce();
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to relay announcement for %s: %s",
                m_wellKnownName.c_str(), QCC_StatusText(err));
        return;
    }
}

void
RemoteBusAttachment::RelaySignal(
    const string&         destination,
    SessionId             sessionId,
    const string&         objectPath,
    const string&         ifaceName,
    const string&         ifaceMember,
    const vector<MsgArg>& msgArgs
    )
{
    LOG_DEBUG("Relaying signal on %s", m_remoteName.c_str());

    // Find the bus object to relay the signal
    RemoteBusObject* busObject = 0;
    vector<RemoteBusObject*>::iterator objIter;
    for(objIter = m_objects.begin(); objIter != m_objects.end(); ++objIter)
    {
        if(objectPath == (*objIter)->GetPath())
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
        LOG_RELEASE("Could not find bus object to relay signal");
    }
}

void
RemoteBusAttachment::SignalReplyReceived(
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
RemoteBusAttachment::SignalSessionJoined(
    SessionId result
    )
{
    m_listener.SignalSessionJoined(result);
}

string
RemoteBusAttachment::WellKnownName() const
{
    return m_wellKnownName;
}

string
RemoteBusAttachment::RemoteName() const
{
    return m_remoteName;
}
