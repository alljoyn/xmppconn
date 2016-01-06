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
#include "XMPPConnector.h"

using namespace std;
using namespace ajn;

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
    m_aboutObj(NULL)
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

    if(m_aboutObj)
    {
        UnregisterBusObject(*m_aboutObj);
        m_aboutObj->Unannounce();
        delete m_aboutObj;
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
    vector<InterfaceDescriptionData> interfaces
    )
{
    FNLOG;
    QStatus err = ER_OK;
    RemoteBusObject* newObj = new RemoteBusObject(this, path, m_connector);
    LOG_VERBOSE("Adding remote bus object for path: %s", path.c_str());

    err = newObj->ImplementInterfaces(interfaces);
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to implement interfaces on remote bus object: %s",
                QCC_StatusText(err));
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
    uint16_t          version,
    uint16_t          port,
    const string&     busName,
    AboutData         aboutData
    )
{
    QStatus err = ER_OK;
    LOG_DEBUG("Relaying announcement for %s", m_wellKnownName.c_str());

    if(m_aboutObj)
    {
        // Already announced. Announcement must have been updated.
        UnregisterBusObject(*m_aboutObj);
        delete m_aboutObj;
    }

    // Check the AboutData to make sure it has everything that's needed for 15.04.
    // NOTE: This is for interoperability with older versions of AllJoyn
    if(!aboutData.IsValid())
    {
        uint8_t* appId = 0;
        size_t size = 0;
        if(ER_OK != aboutData.GetAppId(&appId, &size))
        {
            LOG_RELEASE("No AppId was provided for %s! Using default nil AppId.",
                m_wellKnownName.c_str());
            aboutData.SetAppId("00000000-0000-0000-000000000000");
        }
        char* str = 0;
        if(ER_OK != aboutData.GetAppName(&str))
        {
            LOG_RELEASE("No AppName was provided for %s! Using default of 'Unknown'.",
                m_wellKnownName.c_str());
            aboutData.SetAppName("Unknown");
        }
        if(ER_OK != aboutData.GetDefaultLanguage(&str))
        {
            LOG_RELEASE("No Default Language was provided for %s! Using default of 'en'",
                m_wellKnownName.c_str());
            aboutData.SetSupportedLanguage("en");
            aboutData.SetDefaultLanguage("en");
        }
        if(ER_OK != aboutData.GetDescription(&str))
        {
            LOG_RELEASE("No Description was provided for %s! Using default of 'Unknown'",
                m_wellKnownName.c_str());
            aboutData.SetDescription("Unknown");
        }
        if(ER_OK != aboutData.GetDeviceId(&str))
        {
            LOG_RELEASE("No DeviceId was provided for %s! Using default of nil.",
                m_wellKnownName.c_str());
            aboutData.SetDeviceId("00000000-0000-0000-000000000000");
        }
        if(ER_OK != aboutData.GetManufacturer(&str))
        {
            LOG_RELEASE("No Manufacturer was provided for %s! Using default of 'Unknown'.",
                m_wellKnownName.c_str());
            aboutData.SetManufacturer("Manufacturer");
        }
        if(ER_OK != aboutData.GetModelNumber(&str))
        {
            LOG_RELEASE("No ModelNumber was provided for %s! Using default of 'Unknown'.",
                m_wellKnownName.c_str());
            aboutData.SetModelNumber("Unknown");
        }
        if(ER_OK != aboutData.GetSoftwareVersion(&str))
        {
            LOG_RELEASE("No SoftwareVersion was provided for %s! Using default of 'Unknown'.",
                m_wellKnownName.c_str());
            aboutData.SetSoftwareVersion("Unknown");
        }
    }

    // Bind the session port
    err = BindSessionPort(port);
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to bind About announcement session port for %s: %s",
            m_wellKnownName.c_str(), QCC_StatusText(err));
        // NOTE: Attempt to announce anyways rather than bailing out here
    }

    // Create the AboutObj to relay the announcement
    m_aboutObj = new AboutObj(*this, ajn::BusObject::ANNOUNCED);

    // Make the announcement
    err = m_aboutObj->Announce(port, aboutData);
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
