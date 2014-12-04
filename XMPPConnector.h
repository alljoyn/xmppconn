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
#include <string>
#include <vector>
#include <list>
#include <map>
#include <pthread.h>

#include <alljoyn/about/AboutPropertyStoreImpl.h>

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
    friend class XmppTransport;

public:
    XMPPConnector(
        BusAttachment*     bus,
        const std::string& appName,
        const std::string& xmppJid,
        const std::string& xmppPassword,
        const std::string& xmppChatroom
        );

    virtual ~XMPPConnector();

    // Blocks until stop() is called, listens for XMPP
    QStatus Start();
    void Stop();

    bool IsAdvertisingName(
        const std::string& name
        );

protected:
#ifndef NO_AJ_GATEWAY
    virtual void mergedAclUpdated();
    virtual void shutdown();
    virtual void receiveGetMergedAclAsync(QStatus unmarshalStatus, GatewayMergedAcl* response);
#endif // !NO_AJ_GATEWAY

private:
    struct RemoteBusObject
    {
        std::string                        path;
        std::map<std::string, std::string> interfaces;
    };

    ProxyBusAttachment* GetRemoteProxy(
        const std::string&                  proxyName,
        const std::vector<RemoteBusObject>* objects = NULL
        );
    void DeleteRemoteProxy(
        ProxyBusAttachment*& proxy
        );

    BusAttachment*   m_bus;
    AllJoynListener* m_listener;

    std::list<ProxyBusAttachment*> m_proxyAttachments;
    pthread_mutex_t                m_proxyAttachmentsMutex;


    // TODO: make stuff like this implement-able from outside this class. need to be able to register xmpp send/receive handlers
    //ajn::services::PropertyStore* m_AboutPropertyStore;
    //ajn::services::NotificationService* m_NotifService;
    //ajn::services::NotificationSender* m_NotifSender;

    XmppTransport* m_xmppTransport;
};

} //namespace gw
} //namespace ajn



#endif // XMPPCONNECTOR_H_

