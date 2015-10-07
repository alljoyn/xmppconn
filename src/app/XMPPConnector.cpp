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

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cerrno>
#include <stdint.h>
#include <pthread.h>
#include <fstream>
#include <iostream>

#include "XMPPConnector.h"                                                      // TODO: internal documentation
#include "transport/XmppTransport.h"
#include "RemoteBusAttachment.h"
#include "RemoteBusListener.h"
#include "RemoteBusObject.h"


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

class AllJoynListener :
    public BusListener,
    public SessionPortListener,
    public AnnounceHandler,
    public ProxyBusObject::Listener,
    public util::bus::GetBusObjectsAsyncReceiver
{
public:
    AllJoynListener(
        XMPPConnector* connector,
        BusAttachment* bus
        ) :
        BusListener(),
        m_connector(connector),
        m_bus(bus)
    {
    }

    virtual
    ~AllJoynListener()
    {
        m_bus->UnregisterAllHandlers(this);
    }

    void
    GetBusObjectsCallback(
        ProxyBusObject*                     obj,
        vector<util::bus::BusObjectInfo>    busObjects,
        void*                               context
        )
    {
        FNLOG

        // Send the advertisement via XMPP
        m_connector->SendAdvertisement(
            obj->GetServiceName().c_str(), busObjects);

        IntrospectCallbackContext* ctx =
            static_cast<IntrospectCallbackContext*>(context);
        delete ctx->proxy;
        delete ctx;
    }

    void
    GetBusObjectsAnnouncementCallback(
        ProxyBusObject*                     obj,
        vector<util::bus::BusObjectInfo>    busObjects,
        void*                               context
        )
    {
        FNLOG
        IntrospectCallbackContext* ctx =
            static_cast<IntrospectCallbackContext*>(context);

        // Send the announcement via XMPP
        m_connector->SendAnnounce(
            ctx->AnnouncementInfo.version,
            ctx->AnnouncementInfo.port,
            ctx->AnnouncementInfo.busName,
            ctx->AnnouncementInfo.objectDescs,
            ctx->AnnouncementInfo.aboutData,
            busObjects);

        delete ctx->proxy;
        delete ctx;
    }

    void
    IntrospectCallback(
        QStatus         status,
        ProxyBusObject* obj,
        void*           context
        )
    {
        FNLOG
        /** TODO: REQUIRED
         * Register sessionless signal handlers for announcing/advertising apps
         * (need to implement the required interfaces on m_bus). Other method/
         * signal handlers are registered when a session is joined. This fix
         * MIGHT allow notifications to be handled naturally.
         */
        IntrospectCallbackContext* ctx =
                static_cast<IntrospectCallbackContext*>(context);

        if ( status != ER_OK )
        {
            LOG_RELEASE("Failed to introspect advertised attachment %s: %s",
                ctx->proxy->GetServiceName().c_str(), QCC_StatusText(status));
            delete ctx->proxy;
            delete ctx;
            return;
        }

        m_bus->EnableConcurrentCallbacks();

        if(ctx->introspectReason == IntrospectCallbackContext::advertisement)
        {
            if(status == ER_OK)
            {
                GetBusObjectsAsync(
                    ctx->proxy,
                    const_cast<GetBusObjectsAsyncReceiver*>(static_cast<const GetBusObjectsAsyncReceiver* const>(this)),
                    static_cast<GetBusObjectsAsyncReceiver::CallbackHandler>(&AllJoynListener::GetBusObjectsCallback),
                    ctx
                );
                // NOTE: Don't delete the ctx and ctx->proxy here because they
                //  will be deleted in the callback
                return;
            }
        }
        else if(ctx->introspectReason ==
                IntrospectCallbackContext::announcement)
        {
            if(status == ER_OK)
            {
                vector<util::bus::BusObjectInfo> busObjects;
                GetBusObjectsAsync(
                    ctx->proxy,
                    const_cast<GetBusObjectsAsyncReceiver*>(static_cast<const GetBusObjectsAsyncReceiver* const>(this)),
                    static_cast<GetBusObjectsAsyncReceiver::CallbackHandler>(&AllJoynListener::GetBusObjectsAnnouncementCallback),
                    ctx
                );
                // NOTE: Don't delete the ctx and ctx->proxy here because they
                //  will be deleted in the callback
                return;
            }
            else
            {
                LOG_RELEASE("Failed to introspect Announcing attachment: %s: %s",
                        obj->GetServiceName().c_str(), QCC_StatusText(status));
            }
        }

        // Clean up
        if(ctx->sessionId != 0)
        {
            m_bus->LeaveSession(ctx->sessionId);
        }
        delete ctx->proxy;
        delete ctx;
    }

    void
    FoundAdvertisedName(
        const char*   name,
        TransportMask transport,
        const char*   namePrefix
        )
    {
        // Do not re-advertise these
        if(name == strstr(name, "org.alljoyn.BusNode") ||
           name == strstr(name, "org.alljoyn.sl")      ||
           name == strstr(name, "org.alljoyn.About.sl"))
        {
            return;
        }

        // Do not send if we are the ones transmitting the advertisement
        if(m_connector->OwnsWellKnownName(name))
        {
            return;
        }

        FNLOG

        LOG_DEBUG("Found advertised name: %s", name);

        m_bus->EnableConcurrentCallbacks();

        // Get the objects and interfaces implemented by the advertising device
        ProxyBusObject* proxy = new ProxyBusObject(*m_bus, name, "/", 0);
        if(!proxy->IsValid())
        {
            LOG_RELEASE("Invalid ProxyBusObject for %s", name);
            delete proxy;
            return;
        }

        IntrospectCallbackContext* ctx = new IntrospectCallbackContext();
        ctx->introspectReason = IntrospectCallbackContext::advertisement;
        ctx->sessionId = 0;
        ctx->proxy = proxy;
        QStatus err = proxy->IntrospectRemoteObjectAsync(
                this,
                static_cast<ProxyBusObject::Listener::IntrospectCB>(
                &AllJoynListener::IntrospectCallback),
                ctx);
        if(err != ER_OK)
        {
            LOG_RELEASE("Failed asynchronous introspect for advertised attachment: %s",
                    QCC_StatusText(err));
            delete proxy;
            delete ctx;
            return;
        }
    }

    void
    LostAdvertisedName(
        const char*   name,
        TransportMask transport,
        const char*   namePrefix
        )
    {
        // These are not re-advertised by us
        if(name == strstr(name, "org.alljoyn.BusNode") ||
           name == strstr(name, "org.alljoyn.sl")     ||
           name == strstr(name, "org.alljoyn.About.sl"))
        {
            return;
        }
        FNLOG

        LOG_DEBUG("Lost advertised name: %s", name);
        m_connector->SendAdvertisementLost(name);
    }

    void
    NameOwnerChanged(
        const char* busName,
        const char* previousOwner,
        const char* newOwner
        )
    {
        FNLOG
        /**
         * TODO: REQUIRED
         * If owner changed to nobody, an Announcing app may have gone offline.
         * Send the busName to the XMPP server so that any remote connectors can
         * take down their copies of these apps.
         */

        LOG_DEBUG("Name owner for bus %s changed from %s to %s",
                    busName,
                    (previousOwner ? previousOwner : "<NOBODY>"),
                    (newOwner ? newOwner : "<NOBODY>")
                  );
        if(!busName) { return; }

        m_connector->NameOwnerChanged(busName, newOwner);
    }

    void
    Announce(
        uint16_t                  version,
        uint16_t                  port,
        const char*               busName,
        const ObjectDescriptions& objectDescs,
        const AboutData&          aboutData
        )
    {
        // Is this announcement coming from us?
        if(m_connector->OwnsWellKnownName(busName))
        {
            return;
        }
        FNLOG

        LOG_DEBUG("Received Announce: %s", busName);
        m_bus->EnableConcurrentCallbacks();

        // Get the objects and interfaces implemented by the announcing device
        SessionId sid = 0;
        SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true,
                SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
        QStatus err = m_bus->JoinSession(busName, port, NULL, sid, opts);
        if(err != ER_OK && err != ER_ALLJOYN_JOINSESSION_REPLY_ALREADY_JOINED)
        {
            LOG_RELEASE("Failed to join session with Announcing device: %s",
                    QCC_StatusText(err));
            return;
        }

        ProxyBusObject* proxy = new ProxyBusObject(*m_bus, busName, "/", 0);
        if(!proxy->IsValid())
        {
            LOG_RELEASE("Invalid ProxyBusObject for %s", busName);
            delete proxy;
            return;
        }

        IntrospectCallbackContext* ctx = new IntrospectCallbackContext();
        ctx->introspectReason = IntrospectCallbackContext::announcement;
        ctx->AnnouncementInfo.version     = version;
        ctx->AnnouncementInfo.port        = port;
        ctx->AnnouncementInfo.busName     = busName;
        ctx->AnnouncementInfo.objectDescs = objectDescs;
        ctx->AnnouncementInfo.aboutData   = aboutData;
        ctx->sessionId = sid;
        ctx->proxy = proxy;
        err = proxy->IntrospectRemoteObjectAsync(
                this,
                static_cast<ProxyBusObject::Listener::IntrospectCB>(
                &AllJoynListener::IntrospectCallback),
                ctx);
        if(err != ER_OK)
        {
            LOG_RELEASE("Failed asynchronous introspect for announcing attachment: %s",
                    QCC_StatusText(err));
            delete ctx;
            delete proxy;
            m_bus->LeaveSession(sid);
            return;
        }
    }

private:
    struct IntrospectCallbackContext
    {
        enum
        {
            advertisement,
            announcement
        } introspectReason;

        struct
        {
            uint16_t           version;
            uint16_t           port;
            string             busName;
            ObjectDescriptions objectDescs;
            AboutData          aboutData;
        } AnnouncementInfo;

        SessionId       sessionId;
        ProxyBusObject* proxy;
    };

    XMPPConnector* m_connector;
    BusAttachment* m_bus;
};

const string XMPPConnector::ALLJOYN_CODE_ADVERTISEMENT  = "__ADVERTISEMENT";
const string XMPPConnector::ALLJOYN_CODE_ADVERT_LOST    = "__ADVERT_LOST";
const string XMPPConnector::ALLJOYN_CODE_ANNOUNCE       = "__ANNOUNCE";
const string XMPPConnector::ALLJOYN_CODE_METHOD_CALL    = "__METHOD_CALL";
const string XMPPConnector::ALLJOYN_CODE_METHOD_REPLY   = "__METHOD_REPLY";
const string XMPPConnector::ALLJOYN_CODE_SIGNAL         = "__SIGNAL";
const string XMPPConnector::ALLJOYN_CODE_JOIN_REQUEST   = "__JOIN_REQUEST";
const string XMPPConnector::ALLJOYN_CODE_JOIN_RESPONSE  = "__JOIN_RESPONSE";
const string XMPPConnector::ALLJOYN_CODE_SESSION_JOINED = "__SESSION_JOINED";
const string XMPPConnector::ALLJOYN_CODE_SESSION_LOST   = "__SESSION_LOST";
const string XMPPConnector::ALLJOYN_CODE_GET_PROPERTY   = "__GET_PROPERTY";
const string XMPPConnector::ALLJOYN_CODE_GET_PROP_REPLY = "__GET_PROP_REPLY";
const string XMPPConnector::ALLJOYN_CODE_SET_PROPERTY   = "__SET_PROPERTY";
const string XMPPConnector::ALLJOYN_CODE_SET_PROP_REPLY = "__SET_PROP_REPLY";
const string XMPPConnector::ALLJOYN_CODE_GET_ALL        = "__GET_ALL";
const string XMPPConnector::ALLJOYN_CODE_GET_ALL_REPLY  = "__GET_ALL_REPLY";


XMPPConnector::XMPPConnector(
    BusAttachment*        bus,
    const string&         busInterfaceName,
    const string&         appName,
    const string&         xmppJid,
    const string&         xmppPassword,
    const vector<string>& xmppRoster,
    const string&         xmppChatroom,
    const bool            compress
    ) :
#ifndef NO_AJ_GATEWAY
    GatewayConnector(bus, appName.c_str()),
    m_manifestFilePath("/opt/alljoyn/apps/xmppconn/Manifest.xml"),
#endif // !NO_AJ_GATEWAY
    m_initialized(false),
    m_remoteAttachments(),
    m_propertyBus("propertyBus"),
    m_busInterfaceName(busInterfaceName)
{
    m_transport = new XmppTransport( this,
        xmppJid, xmppPassword, xmppRoster, xmppChatroom, compress);

    pthread_mutex_init(&m_remoteAttachmentsMutex, NULL);

    m_propertyBus.Start();
    m_propertyBus.Connect();
}

XMPPConnector::~XMPPConnector()
{
    m_propertyBus.Disconnect();
    m_propertyBus.Stop();

    pthread_mutex_lock(&m_remoteAttachmentsMutex);
    for ( map<string, list<RemoteBusAttachment*> >::iterator connections_it(m_remoteAttachments.begin());
        m_remoteAttachments.end() != connections_it; ++connections_it )
    {
        for(list<RemoteBusAttachment*>::iterator it = connections_it->second.begin();
            it != connections_it->second.end(); ++it)
        {
            delete(*it);
        }
    }
    m_remoteAttachments.clear();
    pthread_mutex_unlock(&m_remoteAttachmentsMutex);
    pthread_mutex_destroy(&m_remoteAttachmentsMutex);

    // Unregister and delete all the local bus attachments and listeners
    while ( m_buses.size() > 0 )
    {
        string from(m_buses.begin()->first);
        UnregisterFromAdvertisementsAndAnnouncements(from);
    }

    delete m_transport;
}

QStatus
XMPPConnector::Init()
{
    QStatus err = ER_OK;

    if(!m_initialized)
    {
#ifndef NO_AJ_GATEWAY
        err = GatewayConnector::init();
#endif
        if(err == ER_OK)
        {
            m_initialized = true;
        }
        else
        {
            LOG_RELEASE("Failed to initialize the Gateway Connector. Reason: %s",
                QCC_StatusText(err));
        }
    }

    return err;
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
    QStatus err = ER_OK;
    FNLOG
    if(!m_initialized)
    {
        LOG_RELEASE("XMPPConnector not initialized");
        return ER_INIT_FAILED;
    }


    // Listen for messages. Blocks until transport.Stop() is called.
    Transport::ConnectionError runerr = m_transport->Run();
    // TODO: Handle other errors
    if (runerr == Transport::auth_failure)
    {
        err = ER_AUTH_FAIL;
    }
    return err;
}

void XMPPConnector::Stop()
{
    FNLOG
    m_transport->Stop();
}

bool
XMPPConnector::OwnsWellKnownName(
    const string& name
    )
{
    bool result = false;

    pthread_mutex_lock(&m_remoteAttachmentsMutex);

    if ( m_busInterfaceName != name )
    {
        for ( map<string, list<RemoteBusAttachment*> >::const_iterator connections_it(m_remoteAttachments.begin());
            m_remoteAttachments.end() != connections_it; ++connections_it )
        {
            for(list<RemoteBusAttachment*>::const_iterator it = connections_it->second.begin(); it != connections_it->second.end(); ++it)
            {
                if(name == (*it)->WellKnownName())
                {
                    result = true;
                    break;
                }
            }
        }
    }
    else
    {
        result = true;
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

#ifndef NO_AJ_GATEWAY
void
XMPPConnector::mergedAclUpdated()
{
    LOG_DEBUG("Merged Acl updated");
    GatewayMergedAcl* mergedAcl = new GatewayMergedAcl();
    QStatus status = getMergedAclAsync(mergedAcl);
    if(ER_OK != status)
    {
        delete mergedAcl;
    }
}

void
XMPPConnector::shutdown()
{
    Stop();
}

void
XMPPConnector::receiveGetMergedAclAsync(
    QStatus unmarshalStatus,
    GatewayMergedAcl* response
    )
{
    // If there is nothing to remote, disconnect from the server
    if(response->m_ExposedServices.empty() && response->m_RemotedApps.empty())
    {
        //Stop();
    }

    delete response;
}
#endif // !NO_AJ_GATEWAY

RemoteBusAttachment*
XMPPConnector::GetRemoteAttachment(
    const string&                          from,
    const string&                          remoteName,
    const vector<RemoteObjectDescription>* objects
    )
{
    FNLOG;
    RemoteBusAttachment* result = NULL;
    pthread_mutex_lock(&m_remoteAttachmentsMutex);

    map<string, list<RemoteBusAttachment*> >::iterator connection_pair(m_remoteAttachments.find(from));
    if ( m_remoteAttachments.end() != connection_pair )
    {
        list<RemoteBusAttachment*>::iterator it;
        for(it = connection_pair->second.begin(); it != connection_pair->second.end(); ++it)
        {
            if(remoteName == (*it)->RemoteName())
            {
                result = *it;
                break;
            }
        }
    }

    if(!result && objects)
    {
        LOG_DEBUG("Creating new remote bus attachment: %s", remoteName.c_str());

        // We did not find a match. Create the new attachment.
        QStatus err = ER_OK;
        map<string, vector<SessionPort> > portsToBind;
        result = new RemoteBusAttachment(remoteName, this);

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
                    LOG_RELEASE("Failed to create InterfaceDescription %s: %s",
                            ifaceName.c_str(), QCC_StatusText(err));
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
                LOG_RELEASE("Failed to add remote object %s: %s", objPath.c_str(),
                        QCC_StatusText(err));
                break;
            }
        }

        // Start and connect the new attachment.
        if(err == ER_OK)
        {
            err = result->Start();
            if(err != ER_OK)
            {
                LOG_RELEASE("Failed to start new bus attachment %s: %s",
                        remoteName.c_str(), QCC_StatusText(err));
            }
        }
        if(err == ER_OK)
        {
            err = result->Connect();
            if(err != ER_OK)
            {
                LOG_RELEASE("Failed to connect new bus attachment %s: %s",
                        remoteName.c_str(), QCC_StatusText(err));
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
                    LOG_DEBUG("Binding session port %d for interface %s",
                            *spIter, spMapIter->first.c_str());
                    err = result->BindSessionPort(*spIter);
                    if(err != ER_OK)
                    {
                        LOG_RELEASE("Failed to bind session port: %s",
                                QCC_StatusText(err));
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
            m_remoteAttachments[from].push_back(result);
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
    const string& from,
    const string& advertisedName
    )
{
    RemoteBusAttachment* result = NULL;

    pthread_mutex_lock(&m_remoteAttachmentsMutex);
    map<string, list<RemoteBusAttachment*> >::iterator connection_pair(m_remoteAttachments.find(from));
    if ( m_remoteAttachments.end() == connection_pair )
    {
        pthread_mutex_unlock(&m_remoteAttachmentsMutex);
        return NULL;
    }
    list<RemoteBusAttachment*>::iterator it;
    for(it = connection_pair->second.begin(); it != connection_pair->second.end(); ++it)
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
    const string&         from,
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
    map<string, list<RemoteBusAttachment*> >::iterator connection_pair(m_remoteAttachments.find(from));
    if ( m_remoteAttachments.end() == connection_pair )
    {
        pthread_mutex_unlock(&m_remoteAttachmentsMutex);
        return;
    }
    connection_pair->second.remove(attachment);
    pthread_mutex_unlock(&m_remoteAttachmentsMutex);

    delete(attachment);
    attachment = NULL;

    LOG_DEBUG("Deleted remote bus attachment: %s", name.c_str());
}

BusAttachment* XMPPConnector::CreateBusAttachment(
    const std::string& from
    )
{
    FNLOG;
    BusAttachment* attachment = 0;

    pthread_mutex_lock(&m_remoteAttachmentsMutex);
    map<string, BusAttachment*>::iterator connection_pair(m_buses.find(from));
    if ( m_buses.end() == connection_pair )
    {
        LOG_VERBOSE("Creating bus attachment and bus listener for %s", from.c_str());

        // Create the BusAttachment and related BusListener and register for advertisements and announcements
        attachment = new BusAttachment(from.c_str(), true);

        QStatus status = attachment->Start();
        if (ER_OK != status) {
            LOG_RELEASE("Error starting bus for %s: %s", from.c_str(), QCC_StatusText(status));
            pthread_mutex_unlock(&m_remoteAttachmentsMutex);
            delete attachment;
            return 0;
        }

        status = attachment->Connect();
        if (ER_OK != status) {
            LOG_RELEASE("Error connecting bus for %s: %s", from.c_str(), QCC_StatusText(status));
            pthread_mutex_unlock(&m_remoteAttachmentsMutex);
            delete attachment;
            return 0;
        }

        AllJoynListener* listener = new AllJoynListener(this, attachment);
        attachment->RegisterBusListener(*listener);

        m_buses[from] = attachment;
        m_listeners[from] = listener;
    }
    else
    {
        attachment = connection_pair->second;
    }
    pthread_mutex_unlock(&m_remoteAttachmentsMutex);

    return attachment;
}

BusAttachment* XMPPConnector::GetBusAttachment(
    const std::string& from
    )
{
    FNLOG;
    BusAttachment* attachment = 0;

    pthread_mutex_lock(&m_remoteAttachmentsMutex);
    map<string, BusAttachment*>::iterator connection_pair(m_buses.find(from));
    if ( m_buses.end() == connection_pair )
    {
        LOG_VERBOSE("Asked for a BusAttachment for %s but it doesn't exist!", from.c_str());
    }
    else
    {
        attachment = connection_pair->second;
    }
    pthread_mutex_unlock(&m_remoteAttachmentsMutex);

    return attachment;
}

void XMPPConnector::DeleteBusAttachment(
    const std::string& from
    )
{
    FNLOG;
    AllJoynListener* listener = GetBusListener(from);
    pthread_mutex_lock(&m_remoteAttachmentsMutex);
    map<string, BusAttachment*>::iterator connection_pair(m_buses.find(from));
    if ( m_buses.end() == connection_pair )
    {
        pthread_mutex_unlock(&m_remoteAttachmentsMutex);
        return;
    }
    BusAttachment* attachment = connection_pair->second;
    attachment->UnregisterBusListener(*listener);
    DeleteBusListener(from);
    attachment->Stop();
    delete attachment;
    m_buses.erase(from);
    pthread_mutex_unlock(&m_remoteAttachmentsMutex);
}

AllJoynListener* XMPPConnector::GetBusListener(
    const std::string& from
    )
{
    FNLOG;
    AllJoynListener* listener = 0;

    pthread_mutex_lock(&m_remoteAttachmentsMutex);
    map<string, AllJoynListener*>::iterator connection_pair(m_listeners.find(from));
    if ( m_listeners.end() == connection_pair )
    {
        // It should already exist!
        LOG_VERBOSE("Asked for a BusListener for %s but it doesn't exist!", from.c_str());
    }
    else
    {
        listener = connection_pair->second;
    }
    pthread_mutex_unlock(&m_remoteAttachmentsMutex);

    return listener;
}

void XMPPConnector::DeleteBusListener(
    const std::string& from
    )
{
    //pthread_mutex_lock(&m_remoteAttachmentsMutex);
    map<string, AllJoynListener*>::iterator connection_pair(m_listeners.find(from));
    if ( m_listeners.end() == connection_pair )
    {
        pthread_mutex_unlock(&m_remoteAttachmentsMutex);
        return;
    }
    delete connection_pair->second;
    m_listeners.erase(from);
    //pthread_mutex_unlock(&m_remoteAttachmentsMutex);
}

void
XMPPConnector::NameOwnerChanged(
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

void
XMPPConnector::SendAdvertisement(
    const string&                           name,
    const vector<util::bus::BusObjectInfo>& busObjects
    )
{
    FNLOG
    // Skip sending the global Chariot advertisements
    size_t found = name.find(ALLJOYN_URL_SUFFIX);
    if (found != string::npos)
    {
        return;
    }

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
XMPPConnector::SendAdvertisementLost(
    const string& name
    )
{
    FNLOG
    // Construct the text that will be the body of our message
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_ADVERT_LOST << "\n";
    msgStream << name << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_ADVERT_LOST);
}

void
XMPPConnector::SendAnnounce(
    uint16_t                                   version,
    uint16_t                                   port,
    const string&                              busName,
    const AnnounceHandler::ObjectDescriptions& objectDescs,
    const AnnounceHandler::AboutData&          aboutData,
    const vector<util::bus::BusObjectInfo>&    busObjects
    )
{
    FNLOG
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
            // Skip sending the XMPP Chariot announcements
            size_t found = val_iter->find(ALLJOYN_XMPP_SUFFIX.c_str());
            if (found != string::npos)
            {
                return;
            }
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

    msgStream << "\n";

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

    SendMessage(msgStream.str(), ALLJOYN_CODE_ANNOUNCE);
}

void
XMPPConnector::SendJoinRequest(
    const string&                           remoteName,
    SessionPort                             sessionPort,
    const char*                             joiner,
    const SessionOpts&                      opts,
    const vector<util::bus::BusObjectInfo>& busObjects
    )
{
    FNLOG
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
               ifaceNameStr != "org.allseen.Introspectable")
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
XMPPConnector::SendJoinResponse(
    const string& joinee,
    SessionId     sessionId
    )
{
    FNLOG
    // Send the status back to the original session joiner
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_JOIN_RESPONSE << "\n";
    msgStream << joinee << "\n";
    msgStream << sessionId << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_JOIN_RESPONSE);
}

void
XMPPConnector::SendSessionJoined(
    const string& joiner,
    const string& joinee,
    SessionPort   port,
    SessionId     remoteId,
    SessionId     localId
    )
{
    FNLOG
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
XMPPConnector::SendSessionLost(
    const string& peer,
    SessionId     id
    )
{
    FNLOG
    // Construct the text that will be the body of our message
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_SESSION_LOST << "\n";
    msgStream << peer << "\n";
    msgStream << id << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_SESSION_LOST);
}

void
XMPPConnector::SendMethodCall(
    const InterfaceDescription::Member* member,
    Message&                            message,
    const string&                       busName,
    const string&                       objectPath
    )
{
    FNLOG
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
XMPPConnector::SendMethodReply(
    const string& destName,
    const string& destPath,
    Message&      reply
    )
{
    FNLOG
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
XMPPConnector::SendSignal(
    const InterfaceDescription::Member* member,
    const char*                         srcPath,
    Message&                            message
    )
{
    FNLOG
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
    msgStream << message->GetObjectPath() << "\n";
    msgStream << member->iface->GetName() << "\n";
    msgStream << member->name << "\n";
    msgStream << util::msgarg::ToString(msgArgs, numArgs) << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_SIGNAL);
}

void
XMPPConnector::SendGetRequest(
    const string& ifaceName,
    const string& propName,
    const string& destName,
    const string& destPath
    )
{
    FNLOG
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
XMPPConnector::SendGetReply(
    const string& destName,
    const string& destPath,
    const MsgArg& replyArg
    )
{
    FNLOG
    // Return the reply
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_GET_PROP_REPLY << "\n";
    msgStream << destName << "\n";
    msgStream << destPath << "\n";
    msgStream << util::msgarg::ToString(replyArg) << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_GET_PROP_REPLY);
}

void
XMPPConnector::SendSetRequest(
    const string& ifaceName,
    const string& propName,
    const MsgArg& msgArg,
    const string& destName,
    const string& destPath
    )
{
    FNLOG
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
XMPPConnector::SendSetReply(
    const string& destName,
    const string& destPath,
    QStatus       replyStatus
    )
{
    FNLOG
    // Return the reply
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_SET_PROP_REPLY << "\n";
    msgStream << destName << "\n";
    msgStream << destPath << "\n";
    msgStream << static_cast<uint32_t>(replyStatus) << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_SET_PROP_REPLY);
}

void
XMPPConnector::SendGetAllRequest(
    const InterfaceDescription::Member* member,
    const string& destName,
    const string& destPath
    )
{
    FNLOG
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
XMPPConnector::SendGetAllReply(
    const string& destName,
    const string& destPath,
    const MsgArg& replyArgs
    )
{
    FNLOG
    // Construct the text that will be the body of our message
    ostringstream msgStream;
    msgStream << ALLJOYN_CODE_GET_ALL_REPLY << "\n";
    msgStream << destName << "\n";
    msgStream << destPath << "\n";
    msgStream << util::msgarg::ToString(replyArgs) << "\n";

    SendMessage(msgStream.str(), ALLJOYN_CODE_GET_ALL_REPLY);
}

void
XMPPConnector::SendMessage(
    const string& body,
    const string& messageType
    )
{
    FNLOG
    LOG_DEBUG("Sending %smessage over transport.", (messageType.empty() ? "" : (messageType+" ").c_str()));
    Transport::ConnectionError status = m_transport->Send(body);
    if (Transport::none != status)
    {
        LOG_RELEASE("Failed to send message, connection error %d", status);
        //TODO: XmppTransport::SendImpl() currently always returns ConnectionError::none
        //Need to investigate if there is a way to change that
    }
}

vector<XMPPConnector::RemoteObjectDescription>
XMPPConnector::ParseBusObjectInfo(
    istringstream& msgStream
    )
{
    FNLOG
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
XMPPConnector::ReceiveAdvertisement(
    const string& from,
    const string& message
    )
{
    FNLOG
    istringstream msgStream(message);
    string line;

    // First line is the type (advertisement)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_ADVERTISEMENT){ return; }

    // Second line is the name to advertise
    string remoteName, advertisedName;
    if(0 == getline(msgStream, remoteName)){ return; }                          //cout << "received XMPP advertised name: " << remoteName << endl; cout << message << endl;
    if(0 == getline(msgStream, advertisedName)){ return; }

    LOG_DEBUG("Received remote advertisement: %s", remoteName.c_str());

    vector<XMPPConnector::RemoteObjectDescription> objects =
            ParseBusObjectInfo(msgStream);
    RemoteBusAttachment* bus = GetRemoteAttachment(
            from, remoteName, &objects);
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
            DeleteRemoteAttachment(from, bus);
            return;
        }
    }

    LOG_DEBUG("Advertising name: %s", wkn.c_str());
    QStatus err = bus->AdvertiseName(wkn.c_str(), TRANSPORT_ANY);
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to advertise %s: %s", wkn.c_str(),
                QCC_StatusText(err));
        DeleteRemoteAttachment(from, bus);
        return;
    }
}

void
XMPPConnector::ReceiveAdvertisementLost(
    const string& from,
    const string& message
    )
{
    FNLOG
    istringstream msgStream(message);
    string line, name;

    // First line is the type (advertisement)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_ADVERT_LOST){ return; }

    // Second line is the advertisement lost
    if(0 == getline(msgStream, name)){ return; }

    // Get the local bus attachment advertising this name
    RemoteBusAttachment* bus =
            GetRemoteAttachmentByAdvertisedName(from, name);
    if(bus)
    {
        DeleteRemoteAttachment(from, bus);
    }
}

void
XMPPConnector::ReceiveAnnounce(
    const string& from,
    const string& message
    )
{
    FNLOG
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

    LOG_DEBUG("Received remote announcement: %s", busName.c_str());

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
#ifndef NO_AJ_GATEWAY
                writeInterfaceToManifest(line);
#endif
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
            if(propName.empty())
            {
                break;
            }

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

    // Then the bus objects
    vector<XMPPConnector::RemoteObjectDescription> objects =
            ParseBusObjectInfo(msgStream);

    // Find or create the BusAttachment with the given app name
    RemoteBusAttachment* bus =
            GetRemoteAttachment(from, remoteName, &objects);
    if(bus)
    {
        // Request and announce our name
        string wkn = bus->WellKnownName();
        if(wkn.empty())
        {
            wkn = bus->RequestWellKnownName(busName);
            if(wkn.empty())
            {
                DeleteRemoteAttachment(from, bus);
                return;
            }
        }

        bus->RelayAnnouncement(
                strtoul(versionStr.c_str(), NULL, 10),
                strtoul(portStr.c_str(), NULL, 10),
                wkn,
                objDescs,
                aboutData);
    }
}

void
XMPPConnector::ReceiveJoinRequest(
    const string& from,
    const string& message
    )
{
    FNLOG
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
    RemoteBusAttachment* bus = GetRemoteAttachment(
            from, joiner, &objects);

    SessionId id = 0;
    SessionId lastId = bus->GetSessionIdByPeer(joinee);
    if(!bus)
    {
        LOG_RELEASE("Failed to create bus attachment to proxy session!");
    }
    else
    {
        // Try to join a session of our own
        SessionPort port = strtoul(portStr.c_str(), NULL, 10);

        QStatus err = bus->JoinSession(joinee, port, id);

        // Check for error scenarios nd retry join when:
        // - remote device left an active session without telling us, or
        // - previous join request never completed (didn't receive SessionJoined)
        if ( err == ER_ALLJOYN_JOINSESSION_REPLY_ALREADY_JOINED && id == 0 )
        {
            SessionId tempid = bus->GetSessionIdByPeer(joinee);
            tempid = lastId;
            if ( tempid == 0 )
            {
                SessionId pending = m_sessionTracker.GetSession(joiner);
                if ( pending != 0 )
                {
                    tempid = pending;
                    m_sessionTracker.SessionLost(joiner);
                }
            }
            if ( tempid != 0 )
            {
                // Leave and rejoin the session
                LOG_RELEASE("Recovering from a failed or a stale session with %s.", joinee.c_str());
                err = bus->LeaveSession(tempid);
                err = bus->JoinSession(joinee, port, id);
            }
        }

        if(err == ER_ALLJOYN_JOINSESSION_REPLY_ALREADY_JOINED)
        {
            id = bus->GetSessionIdByPeer(joinee);
            LOG_RELEASE("Session already joined between %s (local) and %s (remote). Port: %d Id: %d",
                    joiner.c_str(), joinee.c_str(), port, id);
        }
        else if(err != ER_OK)
        {
            LOG_RELEASE("Join session request rejected: %s",
                    QCC_StatusText(err));
        }
        else
        {
            LOG_DEBUG("Session joined between %s (local) and %s (remote). Port: %d Id: %d",
                    joiner.c_str(), joinee.c_str(), port, id);

            m_sessionTracker.JoinSent(joiner, id);

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
                                LOG_RELEASE("Could not register signal handler for %s: %s",
                                        interfaceName.c_str(), ifaceMembers[i]->name.c_str());
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
XMPPConnector::ReceiveJoinResponse(
    const string& from,
    const string& message
    )
{
    FNLOG
    istringstream msgStream(message);
    string line, appName, remoteSessionId;

    // First line is the type (join response)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_JOIN_RESPONSE){ return; }

    // Get the info from the message
    if(0 == getline(msgStream, appName)){ return; }
    if(0 == getline(msgStream, remoteSessionId)){ return; }

    // Find the BusAttachment with the given app name
    RemoteBusAttachment* bus = GetRemoteAttachment(from, appName);
    if(!bus)
    {
        LOG_RELEASE("Failed to find bus attachment to handle join response!");
    }
    else
    {
        bus->SignalSessionJoined(strtoul(remoteSessionId.c_str(), NULL, 10));
    }
}

void
XMPPConnector::ReceiveSessionJoined(
    const string& from,
    const string& message
    )
{
    FNLOG
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

    m_sessionTracker.JoinConfirmed(joiner);

    // Find the BusAttachment with the given app name
    RemoteBusAttachment* bus = GetRemoteAttachment(from, joiner);
    if(!bus)
    {
        LOG_RELEASE("Failed to find bus attachment to handle joined session!");
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
XMPPConnector::ReceiveSessionLost(
    const string& from,
    const string& message
    )
{
    FNLOG
    istringstream msgStream(message);
    string line, appName, idStr;

    // First line is the type (session joined)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_SESSION_LOST){ return; }

    // Get the info from the message
    if(0 == getline(msgStream, appName)){ return; }
    if(0 == getline(msgStream, idStr)){ return; }

    // Leave the local session
    RemoteBusAttachment* bus = GetRemoteAttachment(from, appName);
    if(bus)
    {
        SessionId localId = bus->GetLocalSessionId(
                strtoul(idStr.c_str(), NULL, 10));

        LOG_DEBUG("Ending session. Attachment: %s Id: %d", appName.c_str(), localId);

        QStatus err = bus->LeaveSession(localId);
        if(err != ER_OK && err != ER_ALLJOYN_LEAVESESSION_REPLY_NO_SESSION)
        {
            LOG_RELEASE("Failed to end session: %s", QCC_StatusText(err));
        }
        else
        {
            bus->RemoveSession(localId);
        }
    }
}

void
XMPPConnector::ReceiveMethodCall(
    const string& from,
    const string& message
    )
{
    FNLOG
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
    RemoteBusAttachment* bus = GetRemoteAttachment(from, remoteName);
    if(!bus)
    {
        LOG_RELEASE("No bus attachment to handle incoming method call. Message: %s",
                message.c_str());
        return;
    }

    // Call the method
    SessionId localSid = bus->GetLocalSessionId(
            strtoul(remoteSessionId.c_str(), NULL, 10));
    ProxyBusObject proxy(*bus, destName.c_str(), destPath.c_str(), localSid);
    QStatus err = proxy.IntrospectRemoteObject();
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to introspect remote object to relay method call: %s",
                QCC_StatusText(err));
        return;
    }

    vector<MsgArg> messageArgs =
            util::msgarg::VectorFromString(messageArgsString);
    Message reply(*bus);
    err = proxy.MethodCall(ifaceName.c_str(), memberName.c_str(),
            &messageArgs[0], messageArgs.size(), reply, 5000);
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to relay method call: %s", QCC_StatusText(err));
        return;
    }

    SendMethodReply(destName, destPath, reply);
}

void
XMPPConnector::ReceiveMethodReply(
    const string& from,
    const string& message
    )
{
    FNLOG
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
    RemoteBusAttachment* bus = GetRemoteAttachment(from, remoteName);
    if(!bus)
    {
        LOG_RELEASE("No bus attachment to handle incoming method call. Message: %s",
                message.c_str());
        return;
    }

    // Tell the attachment we received a message reply
    bus->SignalReplyReceived(objPath, messageArgsString);
}

void
XMPPConnector::ReceiveSignal(
    const string& from,
    const string& message
    )
{
    FNLOG
    // Parse the required information
    istringstream msgStream(message);
    string line, senderName, destination,
            remoteSessionId, objectPath, ifaceName, ifaceMember;

    // First line is the type (signal)
    if(0 == getline(msgStream, line)){ return; }
    if(line != ALLJOYN_CODE_SIGNAL){ return; }

    // Get the bus name and remote session ID
    if(0 == getline(msgStream, senderName)){ return; }
    if(0 == getline(msgStream, destination)){ return; }
    if(0 == getline(msgStream, remoteSessionId)){ return; }
    if(0 == getline(msgStream, objectPath)){ return; }
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
    RemoteBusAttachment* bus = GetRemoteAttachment(from, senderName);
    if(!bus)
    {
        LOG_RELEASE("No bus attachment to handle incoming signal. Sender: %s",
                senderName.c_str());
        return;
    }

    // Relay the signal
    vector<MsgArg> msgArgs = util::msgarg::VectorFromString(messageArgsString);
    SessionId localSessionId = bus->GetLocalSessionId(
            strtoul(remoteSessionId.c_str(), NULL, 10));
    bus->RelaySignal(
            destination, localSessionId, objectPath,
            ifaceName, ifaceMember, msgArgs);
}

void
XMPPConnector::ReceiveGetRequest(
    const string& from,
    const string& message
    )
{
    FNLOG
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

    LOG_DEBUG("Retrieving property: Destination: %s (%s) Iface: %s Property: %s",
            destName.c_str(), destPath.c_str(), ifaceName.c_str(), propName.c_str());

    // Get the property
    ProxyBusObject proxy(m_propertyBus, destName.c_str(), destPath.c_str(), 0);
    QStatus err = proxy.IntrospectRemoteObject();
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to introspect remote object to relay get request: %s",
                QCC_StatusText(err));
        return;
    }

    MsgArg value;
    err = proxy.GetProperty(ifaceName.c_str(), propName.c_str(), value, 5000);  //cout << "Got property value:\n" << util::msgarg::ToString(value) << endl;
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to relay Get request: %s", QCC_StatusText(err));
        return;                                                                 // TODO: send the actual response status back
    }

    // Return the reply
    SendGetReply(destName, destPath, value);
}

void
XMPPConnector::ReceiveGetReply(
    const string& from,
    const string& message
    )
{
    FNLOG
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
    RemoteBusAttachment* bus = GetRemoteAttachment(from, remoteName);
    if(!bus)
    {
        LOG_RELEASE("No bus attachment to handle incoming Get reply.");
        return;
    }

    // Tell the attachment we received a message reply
    bus->SignalReplyReceived(objPath, messageArgString);
}

void
XMPPConnector::ReceiveSetRequest(
    const string& from,
    const string& message
    )
{
    FNLOG
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

    LOG_DEBUG("Setting property: Destination: %s (%s) Interface: %s Property: %s",
            destName.c_str(), destPath.c_str(), ifaceName.c_str(), propName.c_str());

    // Set the property
    ProxyBusObject proxy(m_propertyBus, destName.c_str(), destPath.c_str(), 0);
    QStatus err = proxy.IntrospectRemoteObject();
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to introspect remote object to relay set request: %s",
                QCC_StatusText(err));
    }

    if(err == ER_OK)
    {
        MsgArg value = util::msgarg::FromString(messageArgString);
        err = proxy.SetProperty(
                ifaceName.c_str(), propName.c_str(), value, 5000);
        if(err != ER_OK)
        {
            LOG_RELEASE("Failed to relay Set request: %s",
                    QCC_StatusText(err));
        }
    }

    // Return the reply
    SendSetReply(destName, destPath, err);
}

void
XMPPConnector::ReceiveSetReply(
    const string& from,
    const string& message
    )
{
    FNLOG
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
    RemoteBusAttachment* bus = GetRemoteAttachment(from, remoteName);
    if(!bus)
    {
        LOG_RELEASE("No bus attachment to handle incoming Set reply.");
        return;
    }

    // Tell the attachment we received a message reply
    bus->SignalReplyReceived(objPath, status);
}

void
XMPPConnector::ReceiveGetAllRequest(
    const string& from,
    const string& message
    )
{
    FNLOG
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

    LOG_DEBUG("Retrieving property: Destination: %s (%s) Interface: %s Property: %s",
            destName.c_str(), destPath.c_str(), ifaceName.c_str(), memberName.c_str());

    // Call the method
    ProxyBusObject proxy(m_propertyBus, destName.c_str(), destPath.c_str(), 0);
    QStatus err = proxy.IntrospectRemoteObject();
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to introspect remote object to relay GetAll request: %s",
                QCC_StatusText(err));
        return;
    }

    MsgArg values;
    err = proxy.GetAllProperties(ifaceName.c_str(), values, 5000);              //cout << "Got all properties:\n" << util::msgarg::ToString(values, 2) << endl;
    if(err != ER_OK)
    {
        LOG_RELEASE("Failed to get all properties: %s",
               QCC_StatusText(err));
        return;                                                                 // TODO: send the actual response status back
    }

    // Return the reply
    SendGetAllReply(destName, destPath, values);
}

void
XMPPConnector::ReceiveGetAllReply(
    const string& from,
    const string& message
    )
{
    FNLOG
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
    RemoteBusAttachment* bus = GetRemoteAttachment(from, remoteName);
    if(!bus)
    {
        LOG_RELEASE("No bus attachment to handle incoming GetAll reply.");
        return;
    }

    // Tell the attachment we received a message reply
    bus->SignalReplyReceived(objPath, messageArgsString);
}

void
XMPPConnector::MessageReceived(
    const string& source,
    const string& message
    )
{
    // Handle the content of the message
    string typeCode =
            message.substr(0, message.find_first_of('\n'));
    LOG_DEBUG("Received message from transport: %s", typeCode.c_str());

    if(typeCode == ALLJOYN_CODE_ADVERTISEMENT)
    {
        ReceiveAdvertisement(source, message);
        LOG_DEBUG("Received Advertisement");
    }
    else if(typeCode == ALLJOYN_CODE_ADVERT_LOST)
    {
        ReceiveAdvertisementLost(source, message);
        LOG_DEBUG("Received Lost Advertisement");
    }
    else if(typeCode == ALLJOYN_CODE_ANNOUNCE)
    {
        ReceiveAnnounce(source, message);
        LOG_DEBUG("Received Announce");
    }
    else if(typeCode == ALLJOYN_CODE_METHOD_CALL)
    {
        ReceiveMethodCall(source, message);
        LOG_DEBUG("Receieved Method Call");
    }
    else if(typeCode == ALLJOYN_CODE_METHOD_REPLY)
    {
        ReceiveMethodReply(source, message);
        LOG_DEBUG("Received Method Reply");
    }
    else if(typeCode == ALLJOYN_CODE_SIGNAL)
    {
        ReceiveSignal(source, message);
        LOG_DEBUG("Received Signal");
    }
    else if(typeCode == ALLJOYN_CODE_JOIN_REQUEST)
    {
        ReceiveJoinRequest(source, message);
        LOG_DEBUG("Received Join Request");
    }
    else if(typeCode == ALLJOYN_CODE_JOIN_RESPONSE)
    {
        ReceiveJoinResponse(source, message);
        LOG_DEBUG("Received Join Response");
    }
    else if(typeCode == ALLJOYN_CODE_SESSION_JOINED)
    {
        ReceiveSessionJoined(source, message);
        LOG_DEBUG("Received Session Joined");
    }
    else if(typeCode == ALLJOYN_CODE_SESSION_LOST)
    {
        ReceiveSessionLost(source, message);
        LOG_DEBUG("Recieved Session Lost");
    }
    else if(typeCode == ALLJOYN_CODE_GET_PROPERTY)
    {
        ReceiveGetRequest(source, message);
        LOG_DEBUG("Received Get Request");
    }
    else if(typeCode == ALLJOYN_CODE_GET_PROP_REPLY)
    {
        ReceiveGetReply(source, message);
        LOG_DEBUG("Received Get Reply");
    }
    else if(typeCode == ALLJOYN_CODE_GET_ALL)
    {
        ReceiveGetAllRequest(source, message);
        LOG_DEBUG("Received Get All Requests");
    }
    else if(typeCode == ALLJOYN_CODE_GET_ALL_REPLY)
    {
        ReceiveGetAllReply(source, message);
        LOG_DEBUG("Received Get All Reply");
    }
    else
    {
        LOG_RELEASE("Received unrecognized message type: %s",
                typeCode.c_str());
    }
}

void
XMPPConnector::GlobalConnectionStateChanged(
    const Transport::ConnectionState& new_state,
    const Transport::ConnectionError& error
    )
{
    switch(new_state)
    {
    case Transport::connected:
    {
        // Update connection status and get remote profiles
#ifndef NO_AJ_GATEWAY
        QStatus err = updateConnectionStatus(GW_CS_CONNECTED);
        if(err == ER_OK)
        {
            mergedAclUpdated();
        }
#endif
        break;
    }
    case Transport::disconnecting:
    case Transport::disconnected:
    case Transport::error:
    default:
    {
#ifndef NO_AJ_GATEWAY
        updateConnectionStatus(GW_CS_NOT_CONNECTED);
#endif

        break;
    }
    }

}

void
XMPPConnector::RemoteSourcePresenceStateChanged(
    const std::string&                source,
    const Transport::ConnectionState& new_state,
    const Transport::ConnectionError& error
    )
{
    switch(new_state)
    {
    case Transport::connected:
    {
        // Create local bus attachments and listeners to begin listening and interacting with the local AllJoyn bus
        BusAttachment* attachment = CreateBusAttachment(source);
        AllJoynListener* listener = GetBusListener(source);

        if ( !attachment || !listener )
        {
            LOG_RELEASE("Failed to create bus attachment or listener!");
            return;
        }

        // Start listening for advertisements
        QStatus err = attachment->FindAdvertisedName("");
        if(err != ER_OK)
        {
            LOG_RELEASE("Could not find advertised names for %s: %s",
                    source.c_str(),
                    QCC_StatusText(err));
        }

        // Listen for announcements
        err = AnnouncementRegistrar::RegisterAnnounceHandler(
                *attachment, *listener, NULL, 0);
        if(err != ER_OK)
        {
            LOG_RELEASE("Could not register Announcement handler for %s: %s",
                    source.c_str(),
                    QCC_StatusText(err));
        }
        break;
    }
    case Transport::disconnecting:
    case Transport::disconnected:
    case Transport::error:
    default:
    {
        // Delete all the remote bus attachments
        pthread_mutex_lock(&m_remoteAttachmentsMutex);
        for ( map<string, list<RemoteBusAttachment*> >::iterator connections_it(m_remoteAttachments.begin());
            m_remoteAttachments.end() != connections_it; ++connections_it )
        {
            if ( connections_it->first == source )
            {
                for(list<RemoteBusAttachment*>::iterator it = connections_it->second.begin();
                    it != connections_it->second.end(); ++it)
                {
                    delete(*it);
                }
            }
        }
        m_remoteAttachments.clear();
        pthread_mutex_unlock(&m_remoteAttachmentsMutex);

        UnregisterFromAdvertisementsAndAnnouncements(source);
    }
    }

}

void XMPPConnector::UnregisterFromAdvertisementsAndAnnouncements(const std::string& source)
{
    BusAttachment* attachment = GetBusAttachment(source);
    AllJoynListener* listener = GetBusListener(source);

    if ( attachment && listener )
    {
        // Stop listening for advertisements and announcements
        AnnouncementRegistrar::UnRegisterAllAnnounceHandlers(*attachment);
        attachment->CancelFindAdvertisedName("");
        attachment->Stop();
    }

    // Delete the bus attachment
    DeleteBusAttachment(source);
}

#ifndef NO_AJ_GATEWAY
void XMPPConnector::addInterfaceXMLTag(xmlNode* currentKey, const char* elementProp, const char* elementValue){
    if (currentKey == NULL || currentKey->type != XML_ELEMENT_NODE || currentKey->children == NULL) {
        return;
    }

    xmlNode* obj_key = currentKey->children;
    if (obj_key == NULL || obj_key->type != XML_ELEMENT_NODE || obj_key->children == NULL) {
        return;
    }

    const xmlChar* keyName = currentKey->name;
    for(xmlNode* child_key = obj_key->children; child_key != NULL; child_key = child_key->next){
        if (child_key->type != XML_ELEMENT_NODE || child_key->children == NULL) {
            continue;
        }
        const xmlChar* keyName = child_key->name;

        if (xmlStrEqual(keyName, (const xmlChar*)"interfaces")) {
            xmlNode* node_child = xmlNewChild(child_key, NULL, BAD_CAST "interface", BAD_CAST elementValue);
            xmlNewProp(node_child, BAD_CAST "name", BAD_CAST elementProp);
            xmlAddChild(child_key, node_child);
        }
    }

}
#endif

#ifndef NO_AJ_GATEWAY
void XMPPConnector::writeInterfaceToManifest(const std::string& interfaceName  )
{
    std::ifstream ifs(m_manifestFilePath.c_str());
    std::string content((std::istreambuf_iterator<char>(ifs)),
            (std::istreambuf_iterator<char>()));
    ifs.close();

    if (content.empty()) {
        LOG_RELEASE("Could not read manifest file");
        return;
    }
    
    int remotedTagBeginPos = content.find("<remotedServices>");
    int exposedTagBeginPos = content.find("<exposedServices>");
    int remotedTagEndPos = content.rfind("</remotedServices>");
    int exposedTagEndPos = content.rfind("</exposedServices>");

    if( remotedTagBeginPos == std::string::npos ||
        exposedTagBeginPos == std::string::npos ||
        remotedTagEndPos == std::string::npos ||
        exposedTagEndPos == std::string::npos){
        LOG_RELEASE("Manifest file seems to have incorrect format");
        return;
    }

    if (content.substr(exposedTagBeginPos, exposedTagEndPos-exposedTagBeginPos).find(interfaceName) != std::string::npos){
        LOG_VERBOSE("Interface %s is already present in the Manifest exposedServices.", interfaceName.c_str());
        return;
    }
    if (content.substr(remotedTagBeginPos, remotedTagEndPos-remotedTagEndPos).find(interfaceName) != std::string::npos){
        LOG_VERBOSE("Interface %s is already present in the Manifest remotedServices.", interfaceName.c_str());
        return;
    }
    
    LOG_VERBOSE("Adding interface %s to Manifest file.", interfaceName.c_str());

    xmlNodePtr nodeptr=NULL , node = NULL , node_child =NULL;
    xmlDocPtr doc = xmlParseMemory(content.c_str(), content.size());
    if (doc == NULL) {
        LOG_RELEASE("Could not parse XML from memory");
        xmlFreeDoc(doc);
        xmlCleanupParser();
        return;
    }

    xmlNode* root_element = xmlDocGetRootElement(doc);

    for(xmlNode* currentKey = root_element->children; currentKey != NULL; currentKey = currentKey->next) {

        if (currentKey->type != XML_ELEMENT_NODE || currentKey->children == NULL) {
            continue;
        }

        const xmlChar* keyName = currentKey->name;
        const xmlChar* value = currentKey->children->content;


        std::string ifaceNameProp = interfaceName.substr(interfaceName.rfind('.')+1, std::string::npos) + "Interface";
        if (xmlStrEqual(keyName, (const xmlChar*)"exposedServices")) {
            addInterfaceXMLTag(currentKey, ifaceNameProp.c_str(), interfaceName.c_str());
        }
        else if (xmlStrEqual(keyName, (const xmlChar*)"remotedServices")) {
            addInterfaceXMLTag(currentKey, ifaceNameProp.c_str(), interfaceName.c_str());
        }
    }

    xmlLineNumbersDefault(1);
    xmlThrDefIndentTreeOutput(1);
    xmlKeepBlanksDefault(0);

    xmlSaveFormatFile(m_manifestFilePath.c_str(), doc, 1);
    xmlFreeDoc(doc);
    xmlCleanupParser();
}
#endif
