/*******************************************************************/
// XMPPConnector.h
/*******************************************************************/
#ifndef XMPPCONNECTOR_H_
#define XMPPCONNECTOR_H_

#ifndef NO_AJ_GATEWAY
#include <alljoyn/gateway/GatewayConnector.h>
#endif // !NO_AJ_GATEWAY
#include <alljoyn/BusAttachment.h>
#include <alljoyn/BusListener.h>
#include <alljoyn/notification/Notification.h>
#include <alljoyn/notification/NotificationService.h>
#include <alljoyn/notification/NotificationSender.h>
#include <strophe.h>
#include <string>
#include <vector>
#include <map>

#include <alljoyn/about/AboutPropertyStoreImpl.h>

// TODO: P5 make naming and capitalization consistent

namespace ajn {
namespace gw {

#ifdef NO_AJ_GATEWAY
class XMPPConnector
#else
class XMPPConnector : public GatewayConnector
#endif // NO_AJ_GATEWAY
{
public:
    struct RemoteBusObject
    {
        std::string objectPath;
        std::vector<const InterfaceDescription*> interfaces;
    };

    XMPPConnector(BusAttachment* bus, std::string appName, std::string jabberId, std::string password, std::string chatroomJabberId);
    virtual ~XMPPConnector();

    // Blocks until stop() is called, listens for XMPP
    QStatus Start();
    void Stop();

    QStatus AddRemoteInterface(std::string name, std::vector<RemoteBusObject> busObjects, bool advertise, BusAttachment** bus); // TODO: this and AJBusObject could be private or not in here (just need to include strophe.h to also make xmppConnection/StanzaHandler fns private static members)
    QStatus RemoveRemoteInterface(std::string name);

    std::string FindWellKnownName(std::string uniqueName);

    BusAttachment* GetBusAttachment(); // TODO: maybe make private
    BusListener* GetBusListener();

    std::string GetJabberId();
    std::string GetPassword();
    std::string GetChatroomJabberId();

    bool IsAdvertisingName(std::string name);

protected:
#ifndef NO_AJ_GATEWAY
    virtual void mergedAclUpdated();
    virtual void shutdown();
    virtual void receiveGetMergedAclAsync(QStatus unmarshalStatus, GatewayMergedAcl* response);
#endif // !NO_AJ_GATEWAY

private:
    void RelayAnnouncement(BusAttachment* bus, std::string info);

    void HandleIncomingAdvertisement(std::string info);
    void HandleIncomingMethodCall(std::string info);
    void HandleIncomingMethodReply(std::string info);
    void HandleIncomingSignal(std::string info);
    void HandleIncomingJoinRequest(std::string info);
    void HandleIncomingJoinResponse(std::string info);
    void HandleIncomingSessionJoined(std::string info);
    void HandleIncomingAnnounce(std::string info);
    void HandleIncomingGetRequest(std::string info);
    void HandleIncomingGetReply(std::string info);
    void HandleIncomingGetAll(std::string info);
    void HandleIncomingGetAllReply(std::string info);
    void HandleIncomingAlarm(std::string info);

    static int  XmppStanzaHandler(xmpp_conn_t* const conn, xmpp_stanza_t* const stanza, void* const userdata);
    static void XmppConnectionHandler(
        xmpp_conn_t* const conn, const xmpp_conn_event_t event, const int error,
        xmpp_stream_error_t* const streamError, void* const userdata);

private:
    BusAttachment* m_Bus;
    BusListener* m_BusListener;
    std::vector<SessionPort> m_SessionPorts;

    std::vector<BusAttachment*> m_BusAttachments;
    std::map<std::string, std::string> m_UnsentAnnouncements;

    xmpp_ctx_t* m_XmppCtx;
    xmpp_conn_t* m_XmppConn;

    std::string m_JabberId;
    std::string m_Password;
    std::string m_ChatroomJabberId;

    ajn::services::PropertyStore* m_AboutPropertyStore;
    ajn::services::NotificationService* m_NotifService;
    ajn::services::NotificationSender* m_NotifSender;
};

} //namespace gw
} //namespace ajn



#endif // XMPPCONNECTOR_H_

