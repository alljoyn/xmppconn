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

#ifndef XMPPCONNECTOR_H_                                                        // TODO: document
#define XMPPCONNECTOR_H_

#ifndef NO_AJ_GATEWAY
#include <alljoyn/gateway/GatewayConnector.h>
#endif // !NO_AJ_GATEWAY
#include <alljoyn/BusAttachment.h>
#include <alljoyn/BusListener.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <pthread.h>

#include <alljoyn/about/AboutPropertyStoreImpl.h>
#include <alljoyn/about/AnnouncementRegistrar.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>

#include "transport/Transport.h"
#include "common/xmppconnutil.h"
#include "common/SessionTracker.h"

class AllJoynListener;
class RemoteBusAttachment;
class RemoteBusListener;
class RemoteBusObject;

#ifdef NO_AJ_GATEWAY
class XMPPConnector : public TransportListener
#else
class XMPPConnector : public ajn::gw::GatewayConnector, TransportListener
#endif // NO_AJ_GATEWAY
{
    friend class AllJoynListener;
    friend class RemoteBusAttachment;
    friend class RemoteBusListener;
    friend class RemoteBusObject;
public:
    XMPPConnector(
        ajn::BusAttachment*             bus,
        const std::string&              busInterfaceName,
        const std::string&              appName,
        const std::string&              xmppJid,
        const std::string&              xmppPassword,
        const std::vector<std::string>& xmppRoster,
        const std::string&              xmppChatroom,
        const bool                      compress
        );

    virtual ~XMPPConnector();

    QStatus Init();

    /**
     *  Add a SessionPort to bind when proxying the given interface. Needed to
     *  support well-known interfaces such as ControlPanel (port 1000) and any
     *  other interfaces that require bound SessionPorts.
     *
     *  @param[in] interfaceName  The name of the interface
     *  @param[in] port           The SessionPort to bind
     */
    void
    AddSessionPortMatch(
        const std::string& interfaceName,
        ajn::SessionPort   port
        );

    // Blocks until stop() is called, listens for XMPP
    QStatus Start();
    void Stop();

    bool
    OwnsWellKnownName(
        const std::string& name
        );


    // Class to handle custom XMPP messages
    class MessageReceiver
    {
    public:
        typedef
        void
        (XMPPConnector::MessageReceiver::* MessageHandler)(
            const std::string& from,
            const std::string& key,
            const std::string& message,
            void*              userdata
            );
    };

    void
    RegisterMessageHandler(
        const std::string&                             key,
        XMPPConnector::MessageReceiver*                receiver,
        XMPPConnector::MessageReceiver::MessageHandler messageHandler,
        void*                                          userdata
        );

    void
    UnregisterMessageHandler(
        const std::string& key
        );

    void
    SendMessage(
        const std::string& key,
        const std::string& message
        );

protected:
#ifndef NO_AJ_GATEWAY
    virtual void mergedAclUpdated();
    virtual void shutdown();
    virtual void receiveGetMergedAclAsync(
        QStatus unmarshalStatus, ajn::gw::GatewayMergedAcl* response);
#endif // !NO_AJ_GATEWAY

private:
#ifndef NO_AJ_GATEWAY
   vector<std::string> discoveredInterfaces;
   const std::string m_manifestFilePath;
#endif
    bool m_initialized;

    struct RemoteObjectDescription
    {
        std::string                        path;
        std::map<std::string, std::string> interfaces;
    };

    RemoteBusAttachment* GetRemoteAttachment(
            const std::string&                          from,
            const std::string&                          remoteName,
            const std::vector<RemoteObjectDescription>* objects = NULL
            );
    RemoteBusAttachment* GetRemoteAttachmentByAdvertisedName(
            const std::string& from,
            const std::string& advertisedName
            );
    void DeleteRemoteAttachment(
            const std::string&    from,
            RemoteBusAttachment*& attachment
            );

    ajn::BusAttachment* CreateBusAttachment(
            const std::string& from
            );
    ajn::BusAttachment* GetBusAttachment(
            const std::string& from
            );
    void DeleteBusAttachment(
            const std::string& from
            );
    AllJoynListener* GetBusListener(
            const std::string& from
            );
    void DeleteBusListener(
            const std::string& from
            );

    std::map<std::string,ajn::BusAttachment*>   m_buses;
    std::map<std::string,AllJoynListener*>      m_listeners;

    std::map<std::string, std::list<RemoteBusAttachment*> > m_remoteAttachments;
    pthread_mutex_t                 m_remoteAttachmentsMutex;

    std::map<std::string, std::vector<ajn::SessionPort> > m_sessionPortMap;

    struct MessageCallback
    {
        XMPPConnector::MessageReceiver*                receiver;
        XMPPConnector::MessageReceiver::MessageHandler messageHandler;
        void*                                          userdata;
    };
    std::map<std::string, MessageCallback> m_messageCallbackMap;

    Transport* m_transport;

    // Originally XmppTranport code, moved into XMPPConnector
    void
        NameOwnerChanged(
                const char* wellKnownName,
                const char* uniqueName
                );

    void
        SendAdvertisement(
                const std::string&                           name,
                const std::vector<util::bus::BusObjectInfo>& busObjects
                );
    void
        SendAdvertisementLost(
                const std::string& name
                );
    void
        SendAnnounce(
                uint16_t                                   version,
                uint16_t                                   port,
                const std::string&                         busName,
                const ajn::services::AnnounceHandler::ObjectDescriptions& objectDescs,
                const ajn::services::AnnounceHandler::AboutData&          aboutData,
                const vector<util::bus::BusObjectInfo>&    busObjects
                );
    void
        SendJoinRequest(
                const std::string&                           remoteName,
                SessionPort                                  sessionPort,
                const char*                                  joiner,
                const SessionOpts&                           opts,
                const std::vector<util::bus::BusObjectInfo>& busObjects
                );
    void
        SendJoinResponse(
                const std::string& joinee,
                ajn::SessionId     sessionId
                );
    void
        SendSessionJoined(
                const std::string& joiner,
                const std::string& joinee,
                ajn::SessionPort   port,
                ajn::SessionId     remoteId,
                ajn::SessionId     localId
                );
    void
        SendSessionLost(
                const std::string& peer,
                ajn::SessionId     id
                );
    void
        SendMethodCall(
                const InterfaceDescription::Member* member,
                ajn::Message&                       message,
                const std::string&                  busName,
                const std::string&                  objectPath
                );
    void
        SendMethodReply(
                const std::string& destName,
                const std::string& destPath,
                ajn::Message&      reply
                );
    void
        SendSignal(
                const InterfaceDescription::Member* member,
                const char*                         srcPath,
                ajn::Message&                       message
                );
    void
        SendGetRequest(
                const std::string& ifaceName,
                const std::string& propName,
                const std::string& destName,
                const std::string& destPath
                );
    void
        SendGetReply(
                const std::string& destName,
                const std::string& destPath,
                const ajn::MsgArg& replyArg
                );
    void
        SendSetRequest(
                const std::string& ifaceName,
                const std::string& propName,
                const ajn::MsgArg& msgArg,
                const std::string& destName,
                const std::string& destPath
                );
    void
        SendSetReply(
                const std::string& destName,
                const std::string& destPath,
                QStatus            replyStatus
                );
    void
        SendGetAllRequest(
                const InterfaceDescription::Member* member,
                const std::string&                  destName,
                const std::string&                  destPath
                );
    void
        SendGetAllReply(
                const std::string& destName,
                const std::string& destPath,
                const ajn::MsgArg& replyArgs
                );

    void
        SendNameOwnerChanged(
                const string& busName,
                const string& previousOwner,
                const string& newOwner
                );

    std::vector<XMPPConnector::RemoteObjectDescription>
        ParseBusObjectInfo(
                std::istringstream& msgStream
                );

    void ReceiveAdvertisement(const std::string& from, const std::string& message);
    void ReceiveAdvertisementLost(const std::string& from, const std::string& message);
    void ReceiveAnnounce(const std::string& from, const std::string& message);
    void ReceiveJoinRequest(const std::string& from, const std::string& message);
    void ReceiveJoinResponse(const std::string& from, const std::string& message);
    void ReceiveSessionJoined(const std::string& from, const std::string& message);
    void ReceiveSessionLost(const std::string& from, const std::string& message);
    void ReceiveMethodCall(const std::string& from, const std::string& message);
    void ReceiveMethodReply(const std::string& from, const std::string& message);
    void ReceiveSignal(const std::string& from, const std::string& message);
    void ReceiveGetRequest(const std::string& from, const std::string& message);
    void ReceiveGetReply(const std::string& from, const std::string& message);
    void ReceiveSetRequest(const std::string& from, const std::string& message);
    void ReceiveSetReply(const std::string& from, const std::string& message);
    void ReceiveGetAllRequest(const std::string& from, const std::string& message);
    void ReceiveGetAllReply(const std::string& from, const std::string& message);
    void ReceiveNameOwnerChanged(const std::string& from, const std::string& message);
    void
        MessageReceived(
                const std::string& source,
                const std::string& message
                );
    void
        GlobalConnectionStateChanged(
                const Transport::ConnectionState& new_state,
                const Transport::ConnectionError& error
                );
    void
        RemoteSourcePresenceStateChanged(
                const std::string&                source,
                const Transport::ConnectionState& new_state,
                const Transport::ConnectionError& error
                );

    void
        UnregisterFromAdvertisementsAndAnnouncements(const std::string& source);

#ifndef NO_AJ_GATEWAY
    void
        addInterfaceXMLTag(xmlNode* currentKey,
                           const char* elementProp,
                           const char* elementValue);

    void
        writeInterfaceToManifest(
                const std::string& interfaceName
                );
#endif

    std::map<std::string, std::string> m_wellKnownNameMap;

    BusAttachment m_propertyBus;
    string m_busInterfaceName;
    SessionTracker m_sessionTracker;

    static const std::string ALLJOYN_CODE_ADVERTISEMENT;
    static const std::string ALLJOYN_CODE_ADVERT_LOST;
    static const std::string ALLJOYN_CODE_ANNOUNCE;
    static const std::string ALLJOYN_CODE_JOIN_REQUEST;
    static const std::string ALLJOYN_CODE_JOIN_RESPONSE;
    static const std::string ALLJOYN_CODE_SESSION_JOINED;
    static const std::string ALLJOYN_CODE_SESSION_LOST;
    static const std::string ALLJOYN_CODE_METHOD_CALL;
    static const std::string ALLJOYN_CODE_METHOD_REPLY;
    static const std::string ALLJOYN_CODE_SIGNAL;
    static const std::string ALLJOYN_CODE_GET_PROPERTY;
    static const std::string ALLJOYN_CODE_GET_PROP_REPLY;
    static const std::string ALLJOYN_CODE_SET_PROPERTY;
    static const std::string ALLJOYN_CODE_SET_PROP_REPLY;
    static const std::string ALLJOYN_CODE_GET_ALL;
    static const std::string ALLJOYN_CODE_GET_ALL_REPLY;
    static const std::string ALLJOYN_CODE_NAME_OWNER_CHANGED;
};


#endif // XMPPCONNECTOR_H_

