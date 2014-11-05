/*******************************************************************/
// XMPPConnector.h
/*******************************************************************/
#ifndef XMPPCONNECTOR_H_
#define XMPPCONNECTOR_H_

#include <alljoyn/BusAttachment.h>
#include <alljoyn/BusListener.h>
#include <alljoyn/gateway/GatewayConnector.h>
#include <strophe.h>
#include <string>
#include <list>
#include <map>

// TODO: P5 make naming and capitalization consistent

namespace ajn {
namespace gw {

class XMPPConnector : public GatewayConnector
{
public:
	struct RemoteBusObject
	{
		std::string objectPath;
		std::list<const InterfaceDescription*> interfaces;
	};

	XMPPConnector(BusAttachment* bus, std::string appName, std::string jabberId, std::string password, std::string chatroomJabberId);
	virtual ~XMPPConnector();

	// Blocks until stop() is called, listens for XMPP
	QStatus start();
	void stop();

	QStatus addRemoteInterface(std::string name, std::list<RemoteBusObject> busObjects, bool advertise, BusAttachment** bus); // TODO: this and AJBusObject could be private or not in here (just need to include strophe.h to also make xmppConnection/StanzaHandler fns private static members)
	QStatus removeRemoteInterface(std::string name);

	std::string FindWellKnownName(std::string uniqueName);

	BusAttachment* getBusAttachment(); // TODO: maybe make private
	BusListener* getBusListener();

	std::string getJabberId();
	std::string getPassword();
	std::string getChatroomJabberId();

	bool advertisingName(std::string name);

protected:
	virtual void mergedAclUpdated();
	virtual void shutdown();
	virtual void receiveGetMergedAclAsync(QStatus unmarshalStatus, GatewayMergedAcl* response);

private:
	void relayAnnouncement(BusAttachment* bus, std::string info);

	void handleIncomingAdvertisement(std::string info);
	void handleIncomingMethodCall(std::string info);
	void handleIncomingMethodReply(std::string info);
	void handleIncomingSignal(std::string info);
	void handleIncomingJoinRequest(std::string info);
	void handleIncomingJoinResponse(std::string info);
	void handleIncomingSessionJoined(std::string info);
	void handleIncomingAnnounce(std::string info);
	void handleIncomingGetRequest(std::string info);
	void handleIncomingGetReply(std::string info);
	void handleIncomingAlarm(std::string info);

	static int  xmppStanzaHandler(xmpp_conn_t* const conn, xmpp_stanza_t* const stanza, void* const userdata);
	static void xmppConnectionHandler(
		xmpp_conn_t* const conn, const xmpp_conn_event_t event, const int error,
		xmpp_stream_error_t* const streamError, void* const userdata);

private:
	BusAttachment* m_Bus;
	BusListener* m_BusListener;
	std::list<SessionPort> m_SessionPorts;

	std::list<BusAttachment*> m_BusAttachments;
	std::map<std::string, std::string> m_UnsentAnnouncements;

	xmpp_ctx_t* m_XmppCtx;
	xmpp_conn_t* m_XmppConn;

	std::string m_JabberId;
	std::string m_Password;
	std::string m_ChatroomJabberId;
};

} //namespace gw
} //namespace ajn



#endif // XMPPCONNECTOR_H_

