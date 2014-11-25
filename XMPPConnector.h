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
//#include <strophe.h>
#include <string>
#include <vector>
#include <map>

#include <alljoyn/about/AboutPropertyStoreImpl.h>

// TODO: P5 make naming and capitalization consistent

namespace ajn {
namespace gw {

class AllJoynListener;
class ProxyBusAttachment;
class XmppTransport;

#ifdef NO_AJ_GATEWAY
class XMPPConnector
#else
class XMPPConnector : public GatewayConnector
#endif // NO_AJ_GATEWAY
{
public:
    /*struct RemoteBusObject
    {
        std::string objectPath;
        std::vector<const InterfaceDescription*> interfaces;
    };*/

    XMPPConnector(
        BusAttachment* bus,
        std::string    appName,
        std::string    xmppJid,
        std::string    xmppPassword,
        std::string    xmppChatroom
        );

    virtual ~XMPPConnector();

    // Blocks until stop() is called, listens for XMPP
    QStatus Start();
    void Stop();

    bool IsAdvertisingName(
        std::string name
        );

    //QStatus AddRemoteInterface(std::string name, std::vector<RemoteBusObject> busObjects, bool advertise, BusAttachment** bus); // TODO: this and AJBusObject could be private or not in here (just need to include strophe.h to also make xmppConnection/StanzaHandler fns private static members)
    //QStatus RemoveRemoteInterface(std::string name);

    //std::string FindWellKnownName(std::string uniqueName);

    //BusAttachment* GetBusAttachment(); // TODO: maybe make private
    //BusListener* GetBusListener();

    //std::string GetJabberId();
    //std::string GetPassword();
    //std::string GetChatroomJabberId();

/*protected:
#ifndef NO_AJ_GATEWAY
    virtual void mergedAclUpdated();
    virtual void shutdown();
    virtual void receiveGetMergedAclAsync(QStatus unmarshalStatus, GatewayMergedAcl* response);
#endif // !NO_AJ_GATEWAY
*/

private:
    /*void RelayAnnouncement(BusAttachment* bus, std::string info);

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
    void HandleIncomingAlarm(std::string info);*/

private:
    BusAttachment* m_bus;
    AllJoynListener* m_listener;
    //BusListener* m_BusListener;
    //std::vector<SessionPort> m_SessionPorts;                                  // TODO: how to handle well-known ports we need to bind?

    std::vector<ProxyBusAttachment*> m_proxyAttachments;

    //std::string m_xmppJid;
    //std::string m_xmppPassword;
    //std::string m_xmppChatroom;

    //ajn::services::PropertyStore* m_AboutPropertyStore;
    //ajn::services::NotificationService* m_NotifService;
    //ajn::services::NotificationSender* m_NotifSender;

    XmppTransport* m_xmppTransport;
};

} //namespace gw
} //namespace ajn



#endif // XMPPCONNECTOR_H_

