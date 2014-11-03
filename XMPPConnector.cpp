#include <alljoyn/BusAttachment.h>
#include <alljoyn/BusListener.h>
#include <alljoyn/gateway/GatewayConnector.h>

// XMPPConnector.cpp
#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/TransportMask.h>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <strophe.h>
#include <pthread.h>
#include <cerrno>
#include <map>
#include <alljoyn/about/AboutService.h>
#include <alljoyn/about/AnnouncementRegistrar.h>

// For MsgArg_FromString()
#include <qcc/StringUtil.h>

// main.cpp
#include <csignal>

// TODO: P5 make naming and capitalization consistent

/*******************************************************************/
// XMPPConnector.h
/*******************************************************************/
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
	void handleIncomingMessage(std::string info);
	void handleIncomingSignal(std::string info);
	void handleIncomingJoinRequest(std::string info);
	void handleIncomingJoinResponse(std::string info);
	void handleIncomingSessionJoined(std::string info);
	void handleIncomingAnnounce(std::string info);

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









/*******************************************************************/
// XMPPConnector.cpp
/*******************************************************************/
using namespace ajn;
using namespace ajn::gw;
using namespace ajn::services;

using std::cout;
using std::endl;
using std::string;

#define ALLJOYN_CODE_ADVERTISEMENT  "ADVERTISEMENT"
#define ALLJOYN_CODE_MESSAGE        "MESSAGE"
#define ALLJOYN_CODE_SIGNAL         "SIGNAL"
#define ALLJOYN_CODE_NOTIFICATION   "NOTIFICATION"
#define ALLJOYN_CODE_JOIN_REQUEST   "JOIN_REQUEST"
#define ALLJOYN_CODE_JOIN_RESPONSE  "JOIN_RESPONSE"
#define ALLJOYN_CODE_SESSION_JOINED "SESSION_JOINED"
#define ALLJOYN_CODE_ANNOUNCE       "ANNOUNCE"

static inline void stringReplaceAll(string& str, string from, string to)
{
	size_t pos = str.find(from);
	while(pos != string::npos)
	{
		str.replace(pos, from.length(), to);
		pos = str.find(from, pos+to.length());
	}
}

static inline void unescapeXml(string& str)
{
	stringReplaceAll(str, "&quot;", "\"");
	stringReplaceAll(str, "&apos;", "'");
	stringReplaceAll(str, "&lt;",   "<");
	stringReplaceAll(str, "&gt;",   ">");
	stringReplaceAll(str, "&amp;",  "&");
}

class GenericBusAttachment : public BusAttachment
{
public:
	GenericBusAttachment(const char* applicationName) :
		BusAttachment(applicationName, true), m_AppName(applicationName), m_AdvertisedName(applicationName), m_BusListener(0), m_SessionIdMap()
	{
	}
	~GenericBusAttachment()
	{
		std::list<BusObject*>::iterator it;
		for(it = m_BusObjects.begin(); it != m_BusObjects.end(); ++it)
		{
			delete *it;
		}

		if(m_BusListener)
		{
			UnregisterBusListener(*m_BusListener);
			delete m_BusListener;
		}
	}

	void AddBusListener(BusListener* listener)
	{
		RegisterBusListener(*listener);
		m_BusListener = listener;
	}

	QStatus AddBusObject(BusObject* obj)
	{
		QStatus err = RegisterBusObject(*obj);
		if(err == ER_OK)
		{
			m_BusObjects.push_back(obj);
		}
		return err;
	}

	void AddSessionIdPair(SessionId remoteSessionId, SessionId localSessionId)
	{
		m_SessionIdMap[remoteSessionId] = localSessionId;
	}

	SessionId GetRemoteSessionId(SessionId local)
	{
		std::map<SessionId, SessionId>::iterator it;
		for(it = m_SessionIdMap.begin(); it != m_SessionIdMap.end(); ++it)
		{
			if(it->second == local)
			{
				return it->first;
			}
		}
		return 0;
	}

	SessionId GetLocalSessionId(SessionId remote)
	{
		if(m_SessionIdMap.find(remote) != m_SessionIdMap.end())
		{
			return m_SessionIdMap.at(remote);
		}
		return 0;
	}

	void SetAdvertisedName(string name) { m_AdvertisedName = name; }
	string GetAdvertisedName() { return m_AdvertisedName; }
	string GetAppName() { return m_AppName; }
	BusListener* GetBusListener() { return m_BusListener; }

private:
	string m_AppName;
	string m_AdvertisedName;
	std::list<BusObject*> m_BusObjects; // TODO: will have to be GenericBusObject and that class will have to have way to access objectPath if BusObject doesn't give you that info
	BusListener* m_BusListener;

	std::map<SessionId, SessionId> m_SessionIdMap;
};

// TODO: probably need an ajhandler per busattachment instead of one for all
class AllJoynHandler : public BusListener, /*public MessageReceiver,*/ public SessionPortListener, public ProxyBusObject::Listener, public AnnounceHandler//, public NotificationReceiver
{
public:
	// TODO: tmp, delete me
	/*AllJoynHandler(BusAttachment* bus){
		// recursively introspect the two chat clients...
		//std::list<XMPPConnector::RemoteBusObject> host_ifaces = getInterfacesFromAdvertisedName(bus, "org.alljoyn.bus.samples.chat.test");

		std::list<XMPPConnector::RemoteBusObject> joiner_ifaces;
		ProxyBusObject proxy(*bus, ":BXkJqQDu.121", "/", 0);
		getInterfacesFromAdvertisedNameRecursive(joiner_ifaces, NULL, proxy);
	}*/
	AllJoynHandler(XMPPConnector* connector, xmpp_conn_t* xmppConn) :
		BusListener(), m_Connector(connector), m_XmppConn(xmppConn), m_AssociatedBus(0),
		m_SessionJoinedSignalReceived(false), m_RemoteSessionId(0)
	{
		pthread_mutex_init(&m_SessionJoinedMutex, NULL);
		pthread_cond_init(&m_SessionJoinedWaitCond, NULL);
	}

	virtual ~AllJoynHandler()
	{
		pthread_mutex_destroy(&m_SessionJoinedMutex);
		pthread_cond_destroy(&m_SessionJoinedWaitCond);
	}

	void FoundAdvertisedName(const char* name, TransportMask transport, const char* namePrefix)
	{
		if(m_AssociatedBus)
		{
			// This means we are on a proxy bus attachment. We shouldn't even be looking for advertised names here.
			return;
		}

		if(name == strstr(name, "org.alljoyn.BusNode"))
		{
			return;
		}

		// Check to see if we are advertising this name
		if(m_Connector->advertisingName(name))
		{
			return;
		}

		cout << "Found advertised name: " << name << endl;

		ProxyBusObject* proxy = new ProxyBusObject(*(m_Connector->getBusAttachment()), name, "/", 0);
		QStatus err = proxy->IntrospectRemoteObjectAsync(
			this, static_cast<ProxyBusObject::Listener::IntrospectCB>(&AllJoynHandler::IntrospectCallback), proxy);
		if(err != ER_OK)
		{
			cout << "Could not introspect remote object: " << QCC_StatusText(err) << endl;
		}

		/*BusAttachment* mybus = m_Connector->getBusAttachment();
		m_proxy = new ProxyBusObject(*mybus, name, "/", 0);
		QStatus err = m_proxy->IntrospectRemoteObjectAsync(this, static_cast<ProxyBusObject::Listener::IntrospectCB>(&AllJoynHandler::IntrospectCB), NULL, 500);
		cout << "done: " << QCC_StatusText(err) << " " << m_proxy->GetServiceName() << endl;*/

		/*BusAttachment* mybus = m_Connector->getBusAttachment();
		mybus->EnableConcurrentCallbacks();
		ProxyBusObject proxy(*mybus, name, "/", 0);
		QStatus err = proxy.IntrospectRemoteObject(500);
		cout << "done: " << QCC_StatusText(err) << " " << proxy.GetServiceName() << endl;*/
	}

	void LostAdvertisedName(const char* name, TransportMask transport, const char* namePrefix)
	{
		// TODO: send via XMPP, on receipt should stop advertising and unregister/delete associated bus objects
	}

	/*void NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)
	{
		if(!newOwner)
		{
			return;
		}

		m_Connector->getBusAttachment()->EnableConcurrentCallbacks();

		cout << "NameOwnerChanged: " << newOwner << endl;
		std::list<XMPPConnector::RemoteBusObject> joiner_ifaces;
		ProxyBusObject proxy(*(m_Connector->getBusAttachment()), newOwner, "/", 0);
		getInterfacesFromAdvertisedNameRecursive(joiner_ifaces, NULL, proxy);
	}*/

	bool AcceptSessionJoiner(SessionPort sessionPort, const char* joiner, const SessionOpts& opts)
	{
		// TODO: send session join request via XMPP and attempt to join on remote end, save pair of session ports
		cout << "Session join requested." << endl;

		// If we don't know what bus attachment this is for, do not accept the session
		if(!m_AssociatedBus)
		{
			return false;
		}

		m_AssociatedBus->EnableConcurrentCallbacks();

		// Lock the session join mutex
		pthread_mutex_lock(&m_SessionJoinedMutex);
		m_SessionJoinedSignalReceived = false;
		m_RemoteSessionId = 0;

		// Pack the interfaces in an XMPP message and send them to the server
		xmpp_stanza_t* message = xmpp_stanza_new(xmpp_conn_get_context(m_XmppConn));
		createXmppSessionJoinerStanza(sessionPort, joiner, opts, message);

        char* buf = 0;
        size_t buflen = 0;
        xmpp_stanza_to_text(message, &buf, &buflen);
	    cout << "Sending XMPP message:\n" << buf << endl;
	    free(buf);

		xmpp_send(m_XmppConn, message);
		xmpp_stanza_release(message);

		// Wait for the XMPP response signal
		struct timespec wait_time;
		wait_time.tv_sec = time(NULL)+10; wait_time.tv_nsec = 0;
		while(!m_SessionJoinedSignalReceived)
		{
			if(ETIMEDOUT == pthread_cond_timedwait(&m_SessionJoinedWaitCond, &m_SessionJoinedMutex, &wait_time))
			{
				break;
			}
		}

		bool returnVal = (m_RemoteSessionId != 0);

		pthread_mutex_unlock(&m_SessionJoinedMutex);

		return returnVal;
	}

	void SessionJoined(SessionPort sessionPort, SessionId id, const char* joiner)
	{
		if(m_AssociatedBus)
		{
			// TODO: race condition, support 2 sessions joining simultaneously (could map joiner to remote id?)
			m_AssociatedBus->AddSessionIdPair(m_RemoteSessionId, id);
		}

		// Send the session Id back across the XMPP server
		xmpp_stanza_t* message = xmpp_stanza_new(xmpp_conn_get_context(m_XmppConn));
		createXmppSessionJoinedStanza(sessionPort, id, joiner, message);

        char* buf = 0;
        size_t buflen = 0;
        xmpp_stanza_to_text(message, &buf, &buflen);
	    cout << "Sending XMPP message:\n" << buf << endl;
	    free(buf);

		xmpp_send(m_XmppConn, message);
		xmpp_stanza_release(message);
	}

	void AllJoynMethodHandler(const InterfaceDescription::Member* member, Message& message)
	{
		size_t num_args = 0;
		const MsgArg* msgargs = 0;
		message->GetArgs(num_args, msgargs);
		cout << "Received message: " /*<< member->description.c_str() << "\n"*/ << MsgArg::ToString(msgargs, num_args).c_str() << endl;

		// TODO: pack message in XMPP, send to destination
		// member.destination? we need the WKN this was sent to
		// member.description
		// message.GetArgs() -> MsgArgs* -> ToString
	}

	void AllJoynSignalHandler(const InterfaceDescription::Member* member, const char* srcPath, Message& message)
	{
		// Pack the signal in an XMPP message and send it to the server
		xmpp_stanza_t* xmpp_message = xmpp_stanza_new(xmpp_conn_get_context(m_XmppConn));
		createXmppSignalStanza(member, srcPath, message, xmpp_message);

        char* buf = 0;
        size_t buflen = 0;
        xmpp_stanza_to_text(xmpp_message, &buf, &buflen);
	    cout << "Sending XMPP message:\n" << buf << endl;
	    free(buf);

		xmpp_send(m_XmppConn, xmpp_message);
		xmpp_stanza_release(xmpp_message);
	}

	void AssociateBusAttachment(GenericBusAttachment* bus)
	{
		m_AssociatedBus = bus;
	}

	void SignalSessionJoined(SessionId result)
	{
		pthread_mutex_lock(&m_SessionJoinedMutex);
		m_SessionJoinedSignalReceived = true;
		m_RemoteSessionId = result;
		pthread_cond_signal(&m_SessionJoinedWaitCond);
		pthread_mutex_unlock(&m_SessionJoinedMutex);
	}

	void Announce(
		uint16_t version, uint16_t port, const char* busName, const ObjectDescriptions& objectDescs, const AboutData& aboutData)
	{
		// Send the announcement along
		xmpp_stanza_t* message = xmpp_stanza_new(xmpp_conn_get_context(m_XmppConn));
		createXmppAnnounceStanza(version, port, busName, objectDescs, aboutData, message);

        char* buf = 0;
        size_t buflen = 0;
        xmpp_stanza_to_text(message, &buf, &buflen);
	    cout << "Sending XMPP Announce message" << endl;//:\n" << buf << endl;
	    free(buf);

		xmpp_send(m_XmppConn, message);
		xmpp_stanza_release(message);
	}

private:
	static std::vector<MsgArg*> MsgArg_ParseArray(qcc::String content, std::vector<bool>* variants = NULL)
	{
		std::vector<MsgArg*> array;

		// Get the MsgArgs for each element
		content = Trim(content);
		while(!content.empty())
		{
			size_t typeBeginPos = content.find_first_of('<')+1;
			size_t typeEndPos = content.find_first_of(" >", typeBeginPos);
			qcc::String elemType = content.substr(typeBeginPos, typeEndPos-typeBeginPos);
			qcc::String closeTag = "</"+elemType+">";

			// Find the closing tag for this element
			size_t closeTagPos = content.find(closeTag);
			size_t nestedTypeEndPos = typeEndPos;
			while(closeTagPos > content.find(elemType, nestedTypeEndPos))
			{
				closeTagPos = content.find(closeTag, closeTagPos+closeTag.length());
				nestedTypeEndPos = closeTagPos+2+elemType.length();
			}

			qcc::String element = content.substr(0, closeTagPos+closeTag.length());
			bool isVariant = false;
			array.push_back(MsgArg_FromString(element, &isVariant));
			if(variants)
			{
				variants->push_back(isVariant);
			}

			content = content.substr(closeTagPos+closeTag.length());
		}

		return array;
	}

public:
	// could be part of MsgArg
	static MsgArg* MsgArg_FromString(qcc::String argXml, bool* isVariant = NULL)
	{
		cout << argXml << endl;

		MsgArg* result = new MsgArg();
		bool variant = false;

		QStatus status = ER_OK;
		size_t pos = argXml.find_first_of('>')+1;
		qcc::String typeTag = Trim(argXml.substr(0, pos));
		qcc::String content = argXml.substr(pos, argXml.find_last_of('<')-pos);

		if(0 == typeTag.find("<array type_sig=")) {
			std::vector<bool> variants;
			std::vector<MsgArg*> array = MsgArg_ParseArray(content, &variants);

			MsgArg* testsomething = new MsgArg[array.size()];
			for(int i = 0; i < array.size(); ++i)
			{
				testsomething[i] = *array[i];
				delete array[i];
			}

			pos = typeTag.find_first_of("\"")+1;
			//qcc::String sig = "a"+typeTag.substr(pos,typeTag.find_last_of("\"")-pos);
			qcc::String sig = (variants.empty() || !variants[0]) ? "a*" : "av";
			status = result->Set(sig.c_str(), array.size(), &testsomething[0]);
		    result->SetOwnershipFlags(MsgArg::OwnsArgs, true);
		}
		else if(typeTag == "<boolean>") {
			status = result->Set("b", content == "1");
		}
		else if(typeTag == "<double>") {
			status = result->Set("d", StringToU64(content, 16));
		}
		else if(typeTag == "<dict_entry>") {
			std::vector<bool> variants;
			std::vector<MsgArg*> array = MsgArg_ParseArray(content, &variants);
			if(array.size() != 2)
			{
				status = ER_BUS_BAD_VALUE;
			}
			else
			{
				qcc::String sig1 = variants[0] ? "v" : "*";
				qcc::String sig2 = variants[1] ? "v" : "*";
				qcc::String sig = "{"+sig1+sig2+"}";
				status = result->Set(sig.c_str(), array[0], array[1]);
				result->SetOwnershipFlags(MsgArg::OwnsArgs, true);

				/*qcc::String sig1 = array[0]->Signature();
				qcc::String sig2 = array[1]->Signature();
				if(sig1.empty() || sig2.empty())
				{
					status = ER_INVALID_DATA;
				}
				else
				{
					if(sig1[0] == 'a'){ sig1 = "v"; }
					if(sig2[0] == 'a'){ sig2 = "v"; }

					qcc::String sig = "{"+sig1+sig2+"}";
					status = result->Set(sig.c_str(), array[0], array[1]);
					status = result->Set("{**}", array[0], array[1]);
					result->SetOwnershipFlags(MsgArg::OwnsArgs, true);
				}*/
			}
		}
		else if(typeTag == "<signature>") {
			status = result->Set("g", content.c_str());
			result->Stabilize();
		}
		else if(typeTag == "<int32>") {
			status = result->Set("i", StringToI32(content));
		}
		else if(typeTag == "<int16>") {
			status = result->Set("n", StringToI32(content));
		}
		else if(typeTag == "<object_path>") {
			status = result->Set("o", content.c_str());
			result->Stabilize();
		}
		else if(typeTag == "<uint16>") {
			status = result->Set("q", StringToU32(content));
		}
		else if(typeTag == "<struct>") {
			std::vector<MsgArg*> array = MsgArg_ParseArray(content);

			status = result->Set("r", array.size(), &array[0]);
		    result->SetOwnershipFlags(MsgArg::OwnsArgs, true);
		}
		else if(typeTag == "<string>") {
			status = result->Set("s", content.c_str());
			result->Stabilize();
		}
		else if(typeTag == "<uint64>") {
			status = result->Set("t", StringToU64(content));
		}
		else if(typeTag == "<uint32>") {
			status = result->Set("u", StringToU32(content));
		}
		else if(0 == typeTag.find("<variant signature=")) {
			/*std::vector<MsgArg*> array = MsgArg_ParseArray(content);
			if(array.size() != 1)
			{
				status = ER_INVALID_DATA;
			}
			else
			{
				status = result->Set("v", array[0]);
				result->SetOwnershipFlags(MsgArg::OwnsArgs, true);
			}*/
			result = MsgArg_FromString(content);
			variant = true;
		}
		else if(typeTag == "<int64>") {
			status = result->Set("x", StringToI64(content));
		}
		else if(typeTag == "<byte>") {
			status = result->Set("y", StringToU32(content));
		}
		else if(typeTag == "<handle>") {






		}
		else if(typeTag == "<array type=\"boolean\">") {
			content = Trim(content);
			std::vector<bool> elements;
			pos = 0;
			while((pos = content.find_first_not_of(" ", pos)) != qcc::String::npos)
			{
				size_t endPos = content.find_first_of(' ', pos);
				elements.push_back(content.substr(pos, endPos-pos) == "1");
				pos = endPos;
			}

			// std::vector<bool> is special so we must copy it to a usable array
			bool* array = new bool[elements.size()];
			std::copy(elements.begin(), elements.end(), array);
			status = result->Set("ab", elements.size(), array);
			result->Stabilize();
			delete[] array;
		}
		else if(typeTag == "<array type=\"double\">") {
			/* NOTE: double arrays not supported properly due to MsgArg::ToString not converting
			 * double arrays into bit-exact hex strings the same way it does single doubles */
			content = Trim(content);
			std::vector<double> elements;
			pos = 0;
			while((pos = content.find_first_not_of(" ", pos)) != qcc::String::npos)
			{
				size_t endPos = content.find_first_of(' ', pos);
				elements.push_back(StringToDouble(content.substr(pos, endPos-pos)));
				pos = endPos;
			}
			status = result->Set("ad", elements.size(), &elements[0]);
			result->Stabilize();
		}
		else if(typeTag == "<array type=\"int32\">") {
			content = Trim(content);
			std::vector<int32_t> elements;
			pos = 0;
			while((pos = content.find_first_not_of(" ", pos)) != qcc::String::npos)
			{
				size_t endPos = content.find_first_of(' ', pos);
				elements.push_back(StringToI32(content.substr(pos, endPos-pos)));
				pos = endPos;
			}
			status = result->Set("ai", elements.size(), &elements[0]);
			result->Stabilize();
		}
		else if(typeTag == "<array type=\"int16\">") {
			content = Trim(content);
			std::vector<int16_t> elements;
			pos = 0;
			while((pos = content.find_first_not_of(" ", pos)) != qcc::String::npos)
			{
				size_t endPos = content.find_first_of(' ', pos);
				elements.push_back(StringToI32(content.substr(pos, endPos-pos)));
				pos = endPos;
			}
			status = result->Set("an", elements.size(), &elements[0]);
			result->Stabilize();
		}
		else if(typeTag == "<array type=\"uint16\">") {
			content = Trim(content);
			std::vector<uint16_t> elements;
			pos = 0;
			while((pos = content.find_first_not_of(" ", pos)) != qcc::String::npos)
			{
				size_t endPos = content.find_first_of(' ', pos);
				elements.push_back(StringToU32(content.substr(pos, endPos-pos)));
				pos = endPos;
			}
			status = result->Set("aq", elements.size(), &elements[0]);
			result->Stabilize();
		}
		else if(typeTag == "<array type=\"uint64\">") {
			content = Trim(content);
			std::vector<uint64_t> elements;
			pos = 0;
			while((pos = content.find_first_not_of(" ", pos)) != qcc::String::npos)
			{
				size_t endPos = content.find_first_of(' ', pos);
				elements.push_back(StringToU64(content.substr(pos, endPos-pos)));
				pos = endPos;
			}
			status = result->Set("at", elements.size(), &elements[0]);
			result->Stabilize();
		}
		else if(typeTag == "<array type=\"uint32\">") {
			content = Trim(content);
			std::vector<uint32_t> elements;
			pos = 0;
			while((pos = content.find_first_not_of(" ", pos)) != qcc::String::npos)
			{
				size_t endPos = content.find_first_of(' ', pos);
				elements.push_back(StringToU32(content.substr(pos, endPos-pos)));
				pos = endPos;
			}
			status = result->Set("au", elements.size(), &elements[0]);
			result->Stabilize();
		}
		else if(typeTag == "<array type=\"int64\">") {
			content = Trim(content);
			std::vector<int64_t> elements;
			pos = 0;
			while((pos = content.find_first_not_of(" ", pos)) != qcc::String::npos)
			{
				size_t endPos = content.find_first_of(' ', pos);
				elements.push_back(StringToI64(content.substr(pos, endPos-pos)));
				pos = endPos;
			}
			status = result->Set("ax", elements.size(), &elements[0]);
			result->Stabilize();
		}
		else if(typeTag == "<array type=\"byte\">") {
			content = Trim(content);
			std::vector<uint8_t> elements;
			pos = 0;
			while((pos = content.find_first_not_of(" ", pos)) != qcc::String::npos)
			{
				size_t endPos = content.find_first_of(' ', pos);
				elements.push_back(StringToU32(content.substr(pos, endPos-pos)));
				pos = endPos;
			}
			status = result->Set("ay", elements.size(), &elements[0]);
			result->Stabilize();
		}

		if(status == ER_OK)
		{
			if(isVariant)
			{
				*isVariant = variant;
			}
			return result;
		}
		else
		{
			delete result;
			return NULL;
		}
	}

	// modeled after ProxyBusObject::GetInterfaces(), could be a member of MsgArgs class
	/*static size_t MsgArg_FromString(qcc::String argXml, const MsgArg** args = NULL, size_t numArgs = 0)
	{
		return 0;
	}*/

/*protected:
	std::list<XMPPConnector::RemoteBusObject> getInterfacesFromAdvertisedName(BusAttachment* bus, const char* name)
	{
		std::list<XMPPConnector::RemoteBusObject> ifaces;

		ProxyBusObject proxy(*bus, name, "/", 0);
		if(proxy.IsValid())
		{
			cout << "begin: " << name << endl;
			getInterfacesRecursive(ifaces, name, proxy);
			cout << "end" << endl;
		}

		return ifaces;
	}*/

private:
	void IntrospectCallback(QStatus status, ProxyBusObject* obj, void* context)
	{
		ProxyBusObject* proxy = (ProxyBusObject*)context;

		// Get the interfaces implemented by this advertised name
		BusAttachment* bus = m_Connector->getBusAttachment();
		bus->EnableConcurrentCallbacks();

		pthread_mutex_lock(&m_SessionJoinedMutex); // TODO: different mutex
		cout << "Introspect callback on " << proxy->GetServiceName().c_str() << endl;
		std::list<XMPPConnector::RemoteBusObject> busObjects;

		cout << proxy->GetServiceName() << endl;
		getInterfacesRecursive(busObjects, *proxy, proxy->GetServiceName().c_str());

		if(!busObjects.empty())
		{
			// Pack the interfaces in an XMPP message and send them to the server
			xmpp_stanza_t* message = xmpp_stanza_new(xmpp_conn_get_context(m_XmppConn));
			createXmppInterfaceStanza(proxy->GetServiceName().c_str(), busObjects, message);

			char* buf = 0;
			size_t buflen = 0;
			xmpp_stanza_to_text(message, &buf, &buflen);
			cout << "Sending XMPP message"/*:\n" << buf*/ << endl;
			free(buf);

			xmpp_send(m_XmppConn, message);
			xmpp_stanza_release(message);
		}
		pthread_mutex_unlock(&m_SessionJoinedMutex);

		delete proxy;
	}

	void getInterfacesRecursive(std::list<XMPPConnector::RemoteBusObject>& ifaces, ProxyBusObject& proxy, const char* advertiseName = NULL)
	{
		QStatus err = proxy.IntrospectRemoteObject(500);
		if(err != ER_OK)
		{
			return;
		}

		XMPPConnector::RemoteBusObject this_bus_object;
		this_bus_object.objectPath = proxy.GetPath().c_str();
		if(this_bus_object.objectPath.empty()) {
			this_bus_object.objectPath = "/";
		}

		cout << "  " << this_bus_object.objectPath << endl;

		// Get the interfaces implemented at this object path
		size_t num_ifaces = proxy.GetInterfaces();
		if(num_ifaces != 0)
		{
			InterfaceDescription** iface_list = (InterfaceDescription**)malloc(sizeof(InterfaceDescription*)*num_ifaces);
			num_ifaces = proxy.GetInterfaces((const InterfaceDescription**)iface_list, num_ifaces);

			// Find the interface(s) being advertised by this AJ device
			for(size_t i = 0; i < num_ifaces; ++i)
			{
				const char* iface_name = iface_list[i]->GetName();
				string iface_name_str(iface_name);
				if(iface_name_str != "org.freedesktop.DBus.Peer"                &&  // TODO: which ones and how to filter? filter by FindAdvertisedName? Need a filter at all?
				   iface_name_str != "org.freedesktop.DBus.Introspectable"      &&
				   iface_name_str != "org.freedesktop.DBus.Properties"          &&
				   iface_name_str != "org.allseen.Introspectable"               /*&&
				   iface_name_str != "org.freedesktop.DBus.Peer.Authentication" &&
				   iface_name_str != "org.alljoyn.Bus.Peer.HeaderCompression"*/   )
				{
					//if(!advertiseName || advertiseName == strstr(advertiseName, iface_name))
					{
						cout << "    " << iface_name << endl;//<< " at " << proxy.GetServiceName().c_str() << proxy.GetPath().c_str() << endl;
						this_bus_object.interfaces.push_back(iface_list[i]);

						// TODO: properties?
						/*MsgArg properties;
						proxy.GetAllProperties(iface_name, properties);
						cout << "\n\nPROPERTIES:\n" << properties.ToString() << "\n" << endl;*/
					}
				}
			}

			free(iface_list);

			if(!this_bus_object.interfaces.empty())
			{
				ifaces.push_back(this_bus_object);
			}
		}

		// Get the children of this object path
		size_t num_children = proxy.GetChildren();
		if(num_children != 0)
		{
			ProxyBusObject** children = (ProxyBusObject**)malloc(sizeof(ProxyBusObject*)*num_children);
			num_children = proxy.GetChildren(children, num_children);

			for(size_t i = 0; i < num_children; ++i)
			{
				getInterfacesRecursive(ifaces, *children[i], advertiseName);
			}

			free(children);
		}
	}

	void createXmppInterfaceStanza(const char* advertisedName, const std::list<XMPPConnector::RemoteBusObject>& BusObjects, xmpp_stanza_t* stanza)
	{
		// Construct the text that will be the body of our message
		std::stringstream msg_stream;
		msg_stream << ALLJOYN_CODE_ADVERTISEMENT << "\n";
		msg_stream << advertisedName << "\n\n";
		std::list<XMPPConnector::RemoteBusObject>::const_iterator it;
		for(it = BusObjects.begin(); it != BusObjects.end(); ++it)
		{
			cout << "." << std::flush;
			msg_stream << it->objectPath << "\n";
			std::list<const InterfaceDescription*>::const_iterator if_it;
			for(if_it = it->interfaces.begin(); if_it != it->interfaces.end(); ++if_it)
			{
				msg_stream << (*if_it)->GetName() << "\n";
				msg_stream << (*if_it)->Introspect().c_str() << "\n";

				// properties there?
				//cout << "\n\nproperties mehbeh?:\n" << (*if_it)->Introspect(2).c_str() << "\n" << endl;
			}

			msg_stream << "\n";
		}
		cout << endl;

		// Now wrap it in an XMPP stanza
		xmpp_ctx_t* xmppCtx = xmpp_conn_get_context(m_XmppConn);

		xmpp_stanza_set_name(stanza, "message");
		xmpp_stanza_set_attribute(stanza, "to", m_Connector->getChatroomJabberId().c_str());
		xmpp_stanza_set_type(stanza, "groupchat");

		xmpp_stanza_t* body = xmpp_stanza_new(xmppCtx);
		xmpp_stanza_set_name(body, "body");
		xmpp_stanza_t* text = xmpp_stanza_new(xmppCtx);
		xmpp_stanza_set_text(text, msg_stream.str().c_str());
		xmpp_stanza_add_child(body, text);
		xmpp_stanza_release(text);
		xmpp_stanza_add_child(stanza, body);
		xmpp_stanza_release(body);
	}

	void createXmppSignalStanza(const InterfaceDescription::Member* member, const char* srcPath, Message& message, xmpp_stanza_t* stanza)
	{
		string wellKnownName = "";
		/* TODO: if(!message->isBroadcastSignal())
		{*/
			wellKnownName = m_Connector->FindWellKnownName(message->GetRcvEndpointName());
			if(wellKnownName.empty())
			{
				cout << "Received signal for unknown endpoint." << endl;
				return;
			}
		//}

		size_t num_args = 0;
		const MsgArg* msgargs = 0;
		message->GetArgs(num_args, msgargs);
		/*cout << "SIGNAL" << endl;
		cout << "rcvendpoint: " << wellKnownName << endl;
		cout << "srcpath: " << srcPath << endl;
		cout << "session id: " << message->GetSessionId() << endl;
		cout << "interface description member..." << endl;
		cout << member->name << endl;
		cout << member->iface->Introspect().c_str() << endl;
		cout << MsgArg::ToString(msgargs, num_args).c_str() << endl;*/

		// Construct the text that will be the body of our message
		std::stringstream msg_stream;
		msg_stream << ALLJOYN_CODE_SIGNAL << "\n";
		msg_stream << wellKnownName << "\n";
		msg_stream << message->GetDestination() << endl;
		SessionId sid = message->GetSessionId();
		msg_stream << sid << "\n\n";
		msg_stream << member->name << "\n";
		msg_stream << member->iface->Introspect().c_str() << "\n";
		msg_stream << MsgArg::ToString(msgargs, num_args).c_str() << "\n";


		// Now wrap it in an XMPP stanza
		xmpp_ctx_t* xmppCtx = xmpp_conn_get_context(m_XmppConn);

		xmpp_stanza_set_name(stanza, "message");
		xmpp_stanza_set_attribute(stanza, "to", m_Connector->getChatroomJabberId().c_str());
		xmpp_stanza_set_type(stanza, "groupchat");

		xmpp_stanza_t* body = xmpp_stanza_new(xmppCtx);
		xmpp_stanza_set_name(body, "body");
		xmpp_stanza_t* text = xmpp_stanza_new(xmppCtx);
		xmpp_stanza_set_text(text, msg_stream.str().c_str());
		xmpp_stanza_add_child(body, text);
		xmpp_stanza_release(text);
		xmpp_stanza_add_child(stanza, body);
		xmpp_stanza_release(body);
	}

	void createXmppSessionJoinerStanza(SessionPort sessionPort, const char* joiner, const SessionOpts& opts, xmpp_stanza_t* stanza)
	{
		// Construct the text that will be the body of our message
		std::stringstream msg_stream;
		msg_stream << ALLJOYN_CODE_JOIN_REQUEST << "\n";
		msg_stream << m_AssociatedBus->GetAppName() << "\n";
		msg_stream << sessionPort << "\n";
		msg_stream << joiner << "\n"; // TODO: may need interfaces of joiner

		// We need the interfaces so that they may be implemented on the remote end
		std::list<XMPPConnector::RemoteBusObject> joiner_objects;
		ProxyBusObject proxy(*m_AssociatedBus, joiner, "/", 0);
		getInterfacesRecursive(joiner_objects, proxy);

		std::list<XMPPConnector::RemoteBusObject>::const_iterator it;
		for(it = joiner_objects.begin(); it != joiner_objects.end(); ++it)
		{
			msg_stream << it->objectPath << "\n";
			std::list<const InterfaceDescription*>::const_iterator if_it;
			for(if_it = it->interfaces.begin(); if_it != it->interfaces.end(); ++if_it)
			{
				string interfaceName = (*if_it)->GetName();
				if(interfaceName != "org.freedesktop.DBus.Peer" &&
				   interfaceName != "org.freedesktop.DBus.Introspectable" &&
				   interfaceName != "org.alljoyn.Bus.Peer.HeaderCompression")
				{
					msg_stream << interfaceName << "\n";
					msg_stream << (*if_it)->Introspect().c_str() << "\n";
				}
			}

			msg_stream << "\n";
		}




		// TODO: session opts?


		// Now wrap it in an XMPP stanza
		xmpp_ctx_t* xmppCtx = xmpp_conn_get_context(m_XmppConn);

		xmpp_stanza_set_name(stanza, "message");
		xmpp_stanza_set_attribute(stanza, "to", m_Connector->getChatroomJabberId().c_str());
		xmpp_stanza_set_type(stanza, "groupchat");

		xmpp_stanza_t* body = xmpp_stanza_new(xmppCtx);
		xmpp_stanza_set_name(body, "body");
		xmpp_stanza_t* text = xmpp_stanza_new(xmppCtx);
		xmpp_stanza_set_text(text, msg_stream.str().c_str());
		xmpp_stanza_add_child(body, text);
		xmpp_stanza_release(text);
		xmpp_stanza_add_child(stanza, body);
		xmpp_stanza_release(body);
	}

	void createXmppSessionJoinedStanza(SessionPort sessionPort, SessionId id, const char* joiner, xmpp_stanza_t* stanza)
	{
		// Construct the text that will be the body of our message
		std::stringstream msg_stream;
		msg_stream << ALLJOYN_CODE_SESSION_JOINED << "\n";
		msg_stream << joiner << "\n";
		msg_stream << m_AssociatedBus->GetRemoteSessionId(id) << "\n";
		msg_stream << id << "\n";

		// Now wrap it in an XMPP stanza
		xmpp_ctx_t* xmppCtx = xmpp_conn_get_context(m_XmppConn);

		xmpp_stanza_set_name(stanza, "message");
		xmpp_stanza_set_attribute(stanza, "to", m_Connector->getChatroomJabberId().c_str());
		xmpp_stanza_set_type(stanza, "groupchat");

		xmpp_stanza_t* body = xmpp_stanza_new(xmppCtx);
		xmpp_stanza_set_name(body, "body");
		xmpp_stanza_t* text = xmpp_stanza_new(xmppCtx);
		xmpp_stanza_set_text(text, msg_stream.str().c_str());
		xmpp_stanza_add_child(body, text);
		xmpp_stanza_release(text);
		xmpp_stanza_add_child(stanza, body);
		xmpp_stanza_release(body);
	}

	void createXmppAnnounceStanza(uint16_t version, uint16_t port, const char* busName, const ObjectDescriptions& objectDescs, const AboutData& aboutData, xmpp_stanza_t* stanza)
	{
		/*cout << "\n\n\nANNOUNCE:" << endl;

		cout << "version: " << version << " port: " << port << endl;
		cout << "bus name: " << busName << endl;

		cout << "\nObject descriptions:" << endl;
		ObjectDescriptions::const_iterator it;
		for(it = objectDescs.begin(); it != objectDescs.end(); ++it)
		{
			cout << it->first.c_str() << ":" << endl;
			std::vector<qcc::String>::const_iterator val_iter;
			for(val_iter = it->second.begin(); val_iter != it->second.end(); ++val_iter)
			{
				cout << "  " << val_iter->c_str() << endl;
			}
		}

		cout << "\nAboutData:" << endl;
		AboutData::const_iterator about_it;
		for(about_it = aboutData.begin(); about_it != aboutData.end(); ++about_it)
		{
			cout << about_it->first.c_str() << ":" << endl;
			cout << "  " << about_it->second.ToString() << endl;
		}

		cout << "\n\n" << endl;*/

		// Construct the text that will be the body of our message
		std::stringstream msg_stream;
		msg_stream << ALLJOYN_CODE_ANNOUNCE << "\n";
		msg_stream << version << "\n";
		msg_stream << port << "\n";
		msg_stream << busName << "\n";

		ObjectDescriptions::const_iterator it;
		for(it = objectDescs.begin(); it != objectDescs.end(); ++it)
		{
			msg_stream << it->first.c_str() << "\n";
			std::vector<qcc::String>::const_iterator val_iter;
			for(val_iter = it->second.begin(); val_iter != it->second.end(); ++val_iter)
			{
				msg_stream << val_iter->c_str() << "\n";
			}
		}

		msg_stream << "\n";

		AboutData::const_iterator about_it;
		for(about_it = aboutData.begin(); about_it != aboutData.end(); ++about_it)
		{
			msg_stream << about_it->first.c_str() << "\n";
			msg_stream << about_it->second.Signature() << "\n";
			msg_stream << about_it->second.ToString() << "\n";


			MsgArg* msgarg = AllJoynHandler::MsgArg_FromString(about_it->second.ToString());
			cout << msgarg->OwnsArgs;
			delete msgarg;
		}

		// Now wrap it in an XMPP stanza
		xmpp_ctx_t* xmppCtx = xmpp_conn_get_context(m_XmppConn);

		xmpp_stanza_set_name(stanza, "message");
		xmpp_stanza_set_attribute(stanza, "to", m_Connector->getChatroomJabberId().c_str());
		xmpp_stanza_set_type(stanza, "groupchat");

		xmpp_stanza_t* body = xmpp_stanza_new(xmppCtx);
		xmpp_stanza_set_name(body, "body");
		xmpp_stanza_t* text = xmpp_stanza_new(xmppCtx);
		xmpp_stanza_set_text(text, msg_stream.str().c_str());
		xmpp_stanza_add_child(body, text);
		xmpp_stanza_release(text);
		xmpp_stanza_add_child(stanza, body);
		xmpp_stanza_release(body);
	}

	XMPPConnector* m_Connector;
	xmpp_conn_t* m_XmppConn;
	GenericBusAttachment* m_AssociatedBus;

	bool m_SessionJoinedSignalReceived;
	SessionId m_RemoteSessionId;
	pthread_mutex_t m_SessionJoinedMutex;
	pthread_cond_t m_SessionJoinedWaitCond;
};

class GenericBusObject : public BusObject
{
public:
	GenericBusObject(std::string objectPath) :
		BusObject(objectPath.c_str())
	{
	}

	QStatus implementInterfaces(std::list<const InterfaceDescription*> interfaces, AllJoynHandler* ajHandler)
	{
		std::list<const InterfaceDescription*>::iterator it;
		for(it = interfaces.begin(); it != interfaces.end(); ++it)
		{
			QStatus err = AddInterface(**it);
			if(ER_OK != err)
			{
				return err;
			}

			// Register method handlers
			size_t num_members = (*it)->GetMembers();
			InterfaceDescription::Member** interface_members =
				(InterfaceDescription::Member**)malloc(sizeof(InterfaceDescription::Member*)*num_members);
			num_members = (*it)->GetMembers((const InterfaceDescription::Member**)interface_members, num_members);

			for(size_t i = 0; i < num_members; ++i)
			{
				/*if(interface_members[i]->isSessionlessSignal)
				{*/
					err = bus->RegisterSignalHandler(
						ajHandler, static_cast<MessageReceiver::SignalHandler>(&AllJoynHandler::AllJoynSignalHandler), interface_members[i], NULL);

				/*}
				else
				{*/
					err = AddMethodHandler(interface_members[i],
						static_cast<MessageReceiver::MethodHandler>(&AllJoynHandler::AllJoynMethodHandler), ajHandler);
				//}
				if(err != ER_OK)
				{
					cout << "Failed to add method handler for " << interface_members[i]->name.c_str() << ": ";
					cout << QCC_StatusText(err) << endl;
				}
			}

			free(interface_members);
		}

		return ER_OK;
	}

	QStatus Get(const char* ifcName, const char* propName, MsgArg& val)
	{
		cout << "\n\nGET REQUEST!!\n\n" << endl;
		return ER_BUS_NO_SUCH_PROPERTY;
	}

    QStatus Set(const char* ifcName, const char* propName, MsgArg& val)
    {
		cout << "\n\nSET REQUEST!!\n\n" << endl;
    	return ER_BUS_NO_SUCH_PROPERTY;
    }
};

class GenericPropertyStore : public PropertyStore
{
public:
	GenericPropertyStore(){}
	~GenericPropertyStore(){}

	QStatus ReadAll(const char* languageTag, Filter filter, ajn::MsgArg& all)
	{
		cout << "ReadAll called" << endl;

		// hard-coding required announce properties for now...
	    MsgArg argsAnnounceData[7];

	    MsgArg* arg = 0;
	    uint8_t id_array[] = {253,213,157,54,89,138,109,219,154,119,132,93,215,64,87,42};
	    arg = new MsgArg("ay", 16, id_array);
	    argsAnnounceData[0].Set("{sv}", "AppId", arg);

	    arg = new MsgArg("s", "Controlee");
	    argsAnnounceData[1].Set("{sv}", "AppName", arg);

	    arg = new MsgArg("s", "en");
	    argsAnnounceData[2].Set("{sv}", "DefaultLanguage", arg);

	    arg = new MsgArg("s", "fdd59d36598a6ddb9a77845dd740572a");
	    argsAnnounceData[3].Set("{sv}", "DeviceId", arg);

	    arg = new MsgArg("s", "PT Plug 40572a");
	    argsAnnounceData[4].Set("{sv}", "DeviceName", arg);

	    arg = new MsgArg("s", "Powertech");
	    argsAnnounceData[5].Set("{sv}", "Manufacturer", arg);

	    arg = new MsgArg("s", "ModelNumber");
	    argsAnnounceData[6].Set("{sv}", "Smart Plug", arg);

	    all.Set("a{sv}", 7, argsAnnounceData);
	    all.SetOwnershipFlags(MsgArg::OwnsArgs, true);

		return ER_OK;
	}

	QStatus Update(const char* name, const char* languageTag, const ajn::MsgArg* value)
	{
		return ER_NOT_IMPLEMENTED;
	}

	QStatus Delete(const char* name, const char* languageTag)
	{
		return ER_NOT_IMPLEMENTED;
	}
};

// BusObject we attach to a BusAttachment when we need to relay an announcement
class AboutBusObject : public AboutService
{
public:
	AboutBusObject(BusAttachment& bus, PropertyStore* store) : AboutService(bus, *store), m_PropertyStore(0) {}
	~AboutBusObject()
	{
		delete m_PropertyStore;
	}

	QStatus AddObjectDescriptionsFromXmppInfo(string info)
	{
		std::istringstream info_stream(info);
		string line, version, port_str, busName;

		// First line is the type (announcement)
		if(0 == std::getline(info_stream, line)){ return ER_FAIL; }
		if(line != ALLJOYN_CODE_ANNOUNCE){ return ER_FAIL; }

		// Get the info from the message
		if(0 == std::getline(info_stream, version)){ return ER_FAIL; }
		if(0 == std::getline(info_stream, port_str)){ return ER_FAIL; }
		if(0 == std::getline(info_stream, busName)){ return ER_FAIL; }

		// The object descriptions follow
		string objectPath = "";
		std::vector<qcc::String> interfaceNames;
		while(0 != std::getline(info_stream, line))
		{
			if(line.empty())
			{
				break;
			}

			if(objectPath.empty())
			{
				objectPath = line;
			}
			else
			{
				if(line[0] == '/')
				{
					// end of the object description
					this->AddObjectDescription(objectPath.c_str(), interfaceNames);
					objectPath = line;
					interfaceNames.clear();
				}
				else
				{
					interfaceNames.push_back(line.c_str());
				}
			}
		}

		return ER_OK;
	}

	static PropertyStore* CreatePropertyStoreFromXmppInfo(string info)
	{
		return new GenericPropertyStore();
	}

	QStatus Get(const char* ifcName, const char* propName, MsgArg& val)
	{
		cout << "\n\nGET REQUEST!!\n\n" << endl;
		return ER_BUS_NO_SUCH_PROPERTY;
	}

    QStatus Set(const char* ifcName, const char* propName, MsgArg& val)
    {
		cout << "\n\nSET REQUEST!!\n\n" << endl;
    	return ER_BUS_NO_SUCH_PROPERTY;
    }

private:
	PropertyStore* m_PropertyStore;
};

XMPPConnector::XMPPConnector(BusAttachment* bus, string appName, string jabberId, string password, string chatroomJabberId) :
	GatewayConnector(bus, appName.c_str()),
	m_Bus(bus), m_BusAttachments(), m_UnsentAnnouncements(),
	m_JabberId(jabberId), m_Password(password), m_ChatroomJabberId(chatroomJabberId)
{
	// Initialize our XMPP connection
	xmpp_initialize();
	m_XmppCtx = xmpp_ctx_new(NULL, NULL);
	m_XmppConn = xmpp_conn_new(m_XmppCtx);

	m_BusListener = new AllJoynHandler(this, m_XmppConn);
	m_Bus->RegisterBusListener(*m_BusListener);

	// Well-known ports that we need to bind (temporary)
	m_SessionPorts.push_back(27); // org.alljoyn.bus.samples.chat
}

XMPPConnector::~XMPPConnector()
{
	m_Bus->UnregisterBusListener(*m_BusListener);
	delete m_BusListener;

	xmpp_conn_release(m_XmppConn);
	xmpp_ctx_free(m_XmppCtx);
	xmpp_shutdown();

	std::list<BusAttachment*>::iterator it;
	for(it = m_BusAttachments.begin(); it != m_BusAttachments.end(); ++it)
	{
		(*it)->Disconnect();
		(*it)->Stop();
		delete(*it);
	}
}

QStatus XMPPConnector::start()
{
	// Set up our xmpp connection
	xmpp_conn_set_jid(m_XmppConn, m_JabberId.c_str());
	xmpp_conn_set_pass(m_XmppConn, m_Password.c_str());
	xmpp_handler_add(m_XmppConn, xmppStanzaHandler, NULL, "message", NULL, this);
	if(0 != xmpp_connect_client(m_XmppConn, NULL, 0, xmppConnectionHandler, this))
	{
		cout << "Failed to connect to XMPP server." << endl;
		return ER_FAIL;
	}

	// Listen for XMPP
	xmpp_run(m_XmppCtx);

	return ER_OK;
}

void XMPPConnector::stop()
{
	xmpp_disconnect(m_XmppConn);
	xmpp_handler_delete(m_XmppConn, xmppStanzaHandler);
}

QStatus XMPPConnector::addRemoteInterface(std::string name, std::list<RemoteBusObject> busObjects, bool advertise, BusAttachment** bus)
{
	QStatus err = ER_OK;
	GenericBusAttachment* new_attachment = new GenericBusAttachment(name.c_str());
	AllJoynHandler* ajHandler = new AllJoynHandler(this, m_XmppConn);
	new_attachment->AddBusListener(ajHandler);
	ajHandler->AssociateBusAttachment(new_attachment);

	// Add each bus object and its interfaces
	std::list<RemoteBusObject>::iterator obj_it;
	for(obj_it = busObjects.begin(); obj_it != busObjects.end(); ++obj_it)
	{
		GenericBusObject* new_object = new GenericBusObject(obj_it->objectPath.c_str());
		err = new_attachment->AddBusObject(new_object);
		if(err != ER_OK)
		{
			cout << "Could not register bus object: " << QCC_StatusText(err) << endl;
			delete new_attachment;
			return err;
		}

		QStatus err = new_object->implementInterfaces(obj_it->interfaces, ajHandler);
		if(err != ER_OK)
		{
			cout << "Could not add interfaces to bus object: " << QCC_StatusText(err) << endl;
			delete new_attachment;
			return err;
		}
	}

	// Start and connect the new bus attachment
	err = new_attachment->Start();
	if(err != ER_OK)
	{
		cout << "Could not start new bus attachment: " << QCC_StatusText(err) << endl;
		delete new_attachment;
		return err;
	}
	err = new_attachment->Connect();
	if(err != ER_OK)
	{
		cout << "Could not connect new bus attachment: " << QCC_StatusText(err) << endl;
		new_attachment->Stop();
		delete new_attachment;
		return err;
	}


	// Listen for session requests (TODO: temporary solution to session port problem)
	std::list<SessionPort>::iterator port_iter;
	for(port_iter = m_SessionPorts.begin(); port_iter != m_SessionPorts.end(); ++port_iter)
	{
		SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
		QStatus err = new_attachment->BindSessionPort(*port_iter, opts, *ajHandler); // TODO: traffic messages only?
		if(err != ER_OK)
		{
			cout << "Failed to bind AllJoyn session port " << *port_iter << ": " << QCC_StatusText(err) << endl;
			new_attachment->Disconnect();
			new_attachment->Stop();
			delete new_attachment;
			return err;
		}
	}

	m_BusAttachments.push_back(new_attachment);
	if(advertise)
	{
		if(name.find_first_of(":") != 0)
		{
			// Request and advertise the new attachment's name
			err = new_attachment->RequestName(name.c_str(), DBUS_NAME_FLAG_ALLOW_REPLACEMENT|DBUS_NAME_FLAG_DO_NOT_QUEUE);
			if(err != ER_OK)
			{
				cout << "Could not acquire well known name " << name << ": " << QCC_StatusText(err) << endl;
				m_BusAttachments.remove(new_attachment);
				new_attachment->Disconnect();
				new_attachment->Stop();
				delete new_attachment;
				return err;
			}
		}
		else
		{
			// We have a device advertising its unique name.
			name = new_attachment->GetUniqueName().c_str();
			new_attachment->SetAdvertisedName(name);
		}

		cout << "advertising name: " << name << endl;
		err = new_attachment->AdvertiseName(name.c_str(), TRANSPORT_ANY);
		if(err != ER_OK)
		{
			cout << "Could not advertise name " << name << ": " << QCC_StatusText(err) << endl;
			m_BusAttachments.remove(new_attachment);
			new_attachment->Disconnect();
			new_attachment->Stop();
			delete new_attachment;
			return err;
		}
	}

	if(bus)
	{
		*bus = new_attachment;
	}
	return err;
}

string XMPPConnector::FindWellKnownName(string uniqueName)
{
	std::list<BusAttachment*>::iterator it;
	for(it = m_BusAttachments.begin(); it != m_BusAttachments.end(); ++it)
	{
		if(uniqueName == (*it)->GetUniqueName().c_str())
		{
			return ((GenericBusAttachment*)(*it))->GetAdvertisedName();
		}
	}

	return "";
}

BusAttachment* XMPPConnector::getBusAttachment()
{
	return m_Bus;
}

BusListener* XMPPConnector::getBusListener()
{
	return m_BusListener;
}

string XMPPConnector::getJabberId()
{
	return m_JabberId;
}

string XMPPConnector::getPassword()
{
	return m_Password;
}

string XMPPConnector::getChatroomJabberId()
{
	return m_ChatroomJabberId;
}

bool XMPPConnector::advertisingName(string name)
{
	std::list<BusAttachment*>::iterator it;
	for(it = m_BusAttachments.begin(); it != m_BusAttachments.end(); ++it)
	{
		GenericBusAttachment* gen_bus = (GenericBusAttachment*)(*it);
		//cout << "COMPARING:                                                                                     " << name << " : " << gen_bus->GetAdvertisedName() << endl;
		if(name == gen_bus->GetAdvertisedName())
		{
			return true;
		}
	}
	return false;
}

void XMPPConnector::mergedAclUpdated()
{
	// TODO
}

void XMPPConnector::shutdown()
{
	// TODO
}

void XMPPConnector::receiveGetMergedAclAsync(QStatus unmarshalStatus, GatewayMergedAcl* response)
{
	// TODO
}

void XMPPConnector::relayAnnouncement(BusAttachment* bus, string info)
{
	GenericBusAttachment* gen_bus = (GenericBusAttachment*)bus;
	cout << "Relaying announcement for " << gen_bus->GetAdvertisedName() << endl;

	PropertyStore* properties = AboutBusObject::CreatePropertyStoreFromXmppInfo(info);
	AboutBusObject* about_obj = new AboutBusObject(*gen_bus, properties);
	about_obj->AddObjectDescriptionsFromXmppInfo(info);
	gen_bus->AddBusObject(about_obj);

	about_obj->Register(900); // TODO: get real port

	QStatus err = about_obj->Announce();
	if(err != ER_OK)
	{
		cout << "Failed to relay announcement: " << QCC_StatusText(err) << endl;
	}
}

void XMPPConnector::handleIncomingAdvertisement(string info)
{
	//cout << "\n\nADVERTISEMENT:\n" << info << endl;

	// Parse the required information
	string advertise_name;
	std::list<RemoteBusObject> bus_objects;

	std::istringstream info_stream(info);
	string line;

	// First line is the type (advertisement)
	if(0 == std::getline(info_stream, line)){ return; }
	if(line != ALLJOYN_CODE_ADVERTISEMENT){ return; }

	// Second line is the name to advertise
	if(0 == std::getline(info_stream, advertise_name)){ return;	}
	//cout << "advertise name: " << advertise_name << endl;

	// Following are the bus objects and their interface descriptions separated by newlines // TODO: copy-paste code
	RemoteBusObject new_bus_object;
	string interface_name = "";
	string interface_description = "";

	if(0 == std::getline(info_stream, line)){ return; }
	while(std::getline(info_stream, line))
	{
		if(line.empty())
		{
			if(!interface_description.empty())
			{
				// We've reached the end of an interface description. Put it in new_bus_object.
				unescapeXml(interface_description);
				//cout << "interface description:\n" << interface_description << endl;
				QStatus err = m_Bus->CreateInterfacesFromXml(interface_description.c_str());
				if(err == ER_OK)
				{
					cout << "Created InterfaceDescription " << interface_name << endl;
					const InterfaceDescription* new_interface = m_Bus->GetInterface(interface_name.c_str());
					if(new_interface)
					{
						// TODO: problem that we cannot activate the interface?
						new_bus_object.interfaces.push_back(new_interface);
					}
				}
				else
				{
					cout << "Failed to create InterfaceDescription " << interface_name << ": " << QCC_StatusText(err) << endl;
				}

				interface_name.clear();
				interface_description.clear();
			}
			else
			{
				// We've reached the end of a bus object.
				bus_objects.push_back(new_bus_object);

				new_bus_object.objectPath.clear();
				new_bus_object.interfaces.clear();
			}
		}
		else
		{
			if(new_bus_object.objectPath.empty())
			{
				//cout << "object path: " << line << endl;
				new_bus_object.objectPath = line;
			}
			else if(interface_name.empty())
			{
				//cout << "interface name: " << line << endl;
				interface_name = line;
			}
			else
			{
				interface_description.append(line + "/n");
			}
		}
	}

	// Now we need to actually implement the interfaces and advertise the name
	GenericBusAttachment* gen_bus;
	QStatus err = addRemoteInterface(advertise_name, bus_objects, true, (BusAttachment**)&gen_bus);
	if(err != ER_OK)
	{
		cout << "Could not implement remote advertisement." << endl;
	}
	else
	{
		// Do we have an announcement waiting to be sent for this new attachment?
		if(m_UnsentAnnouncements.find(advertise_name) != m_UnsentAnnouncements.end())
		{
			relayAnnouncement(gen_bus, m_UnsentAnnouncements.at(advertise_name));
			m_UnsentAnnouncements.erase(advertise_name);
		}
	}
}

void XMPPConnector::handleIncomingMessage(string info)
{
}

void XMPPConnector::handleIncomingSignal(string info)
{
	cout << "INCOMING SIGNAL:\n" << info << endl;
	// TODO:  on XMPP receipt, find bus object with srcPath (in XMPPConnector obj)

	// Parse the required information
	string advertise_name;
	std::list<RemoteBusObject> bus_objects;

	std::istringstream info_stream(info);
	string line, appName, destination, remoteId;

	// First line is the type (signal)
	if(0 == std::getline(info_stream, line)){ return; }
	if(line != ALLJOYN_CODE_SIGNAL){ return; }

	// Get the bus name and remote session ID
	if(0 == std::getline(info_stream, appName)){ return; }
	if(0 == std::getline(info_stream, destination)){ return; }
	if(0 == std::getline(info_stream, remoteId)){ return; }

	// Find the bus attachment with this busName
	GenericBusAttachment* found_bus = 0;

	std::list<BusAttachment*>::iterator it;
	for(it = m_BusAttachments.begin(); it != m_BusAttachments.end(); ++it)
	{
		GenericBusAttachment* gen_bus = (GenericBusAttachment*)(*it);
		if(gen_bus->GetAppName() == appName)
		{
			found_bus = gen_bus;
			break;
		}
	}

	if(!found_bus)
	{
		cout << "No bus attachment to handle incoming signal." << endl;
		return;
	}

	// Now find the bus object and interface member of the signal
}

void XMPPConnector::handleIncomingJoinRequest(string info)
{
	/*msg_stream << ALLJOYN_CODE_JOIN_REQUEST << "\n";
	msg_stream << sessionPort << "\n";
	msg_stream << joiner << "\n";*/

	string join_dest = "";
	string port_str = "";
	string joiner = "";

	std::istringstream info_stream(info);
	string line;

	// First line is the type (join request)
	if(0 == std::getline(info_stream, line)){ return; }
	if(line != ALLJOYN_CODE_JOIN_REQUEST){ return; }

	// Next is the session port, join destination, and the joiner
	if(0 == std::getline(info_stream, join_dest)){ return; }
	if(0 == std::getline(info_stream, port_str)){ return; }
	if(0 == std::getline(info_stream, joiner)){ return; }

	// Then follow the interfaces implemented by the joiner // TODO: copy-paste code
	std::list<RemoteBusObject> bus_objects;
	RemoteBusObject new_bus_object;
	string interface_name = "";
	string interface_description = "";

	while(std::getline(info_stream, line))
	{
		if(line.empty())
		{
			// We've reached the end of an interface description. Put it in new_bus_object.
			unescapeXml(interface_description);
			QStatus err = m_Bus->CreateInterfacesFromXml(interface_description.c_str());
			if(err == ER_OK)
			{
				const InterfaceDescription* new_interface = m_Bus->GetInterface(interface_name.c_str());
				if(new_interface)
				{
					// TODO: problem that we cannot activate the interface?
					new_bus_object.interfaces.push_back(new_interface);
				}
			}
			else
			{
				cout << "Failed to create InterfaceDescription: " << QCC_StatusText(err) << endl;
			}

			interface_name.clear();
			interface_description.clear();

			// If the NEXT line is empty, we've reached the end of a bus object.
			if(0 == std::getline(info_stream, line)){ return; }
			if(line.empty())
			{
				bus_objects.push_back(new_bus_object);

				new_bus_object.objectPath.clear();
				new_bus_object.interfaces.clear();
			}
		}
		else
		{
			if(new_bus_object.objectPath.empty())
			{
				new_bus_object.objectPath = line;
			}
			else if(interface_name.empty())
			{
				interface_name = line;
			}
			else
			{
				interface_description.append(line + "/n");
			}
		}
	}

	// Create a bus attachment to start a session with // TODO: look for existing one first
	GenericBusAttachment* new_bus = 0;
	SessionId id = 0;
	QStatus err = addRemoteInterface(joiner, bus_objects, false, (BusAttachment**)&new_bus);
	if(err == ER_OK && new_bus)
	{
		// Try to join a session of our own
		SessionPort port = strtol(port_str.c_str(), NULL, 10);
		SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
		err = new_bus->JoinSession(join_dest.c_str(), port, NULL, id, opts);
	}
	else if(err == ER_OK)
	{
		err = ER_FAIL;
	}

	// Send the status back to the original session joiner
	// TODO: direct XMPP message
	std::stringstream msg_stream;
	msg_stream << ALLJOYN_CODE_JOIN_RESPONSE << "\n"; // TODO: support more than 1 concurrent session request
	msg_stream << join_dest << "\n";
	msg_stream << (err == ER_OK ? id : 0) << "\n";


	// Now wrap it in an XMPP stanza
	xmpp_stanza_t* message = xmpp_stanza_new(xmpp_conn_get_context(m_XmppConn));

	xmpp_stanza_set_name(message, "message");
	xmpp_stanza_set_attribute(message, "to", m_ChatroomJabberId.c_str());
	xmpp_stanza_set_type(message, "groupchat");

	xmpp_stanza_t* body = xmpp_stanza_new(m_XmppCtx);
	xmpp_stanza_set_name(body, "body");
	xmpp_stanza_t* text = xmpp_stanza_new(m_XmppCtx);
	xmpp_stanza_set_text(text, msg_stream.str().c_str());
	xmpp_stanza_add_child(body, text);
	xmpp_stanza_release(text);
	xmpp_stanza_add_child(message, body);
	xmpp_stanza_release(body);

    char* buf = 0;
    size_t buflen = 0;
    xmpp_stanza_to_text(message, &buf, &buflen);
    cout << "Sending XMPP message:\n" << buf << endl;
    free(buf);

    // Send it back
	xmpp_send(m_XmppConn, message);
	xmpp_stanza_release(message);
}

void XMPPConnector::handleIncomingJoinResponse(string info)
{
	std::istringstream info_stream(info);
	string line;
	string appName = "";
	string result_str = "";

	// First line is the type (join response)
	if(0 == std::getline(info_stream, line)){ return; }
	if(line != ALLJOYN_CODE_JOIN_RESPONSE){ return; }

	// Get the info from the message
	if(0 == std::getline(info_stream, appName)){ return; }
	if(0 == std::getline(info_stream, result_str)){ return; }

	// Find the BusAttachment with the given app name
	std::list<BusAttachment*>::iterator it;
	for(it = m_BusAttachments.begin(); it != m_BusAttachments.end(); ++it)
	{
		GenericBusAttachment* gen_bus = (GenericBusAttachment*)(*it);
		if(gen_bus->GetAppName() == appName)
		{
			AllJoynHandler* ajHandler = (AllJoynHandler*)gen_bus->GetBusListener();
			ajHandler->SignalSessionJoined(strtol(result_str.c_str(), NULL, 10));
			break;
		}
	}
}

void XMPPConnector::handleIncomingSessionJoined(string info)
{
	std::istringstream info_stream(info);
	string line, joiner, remote_id_str, local_id_str;

	// First line is the type (session joined)
	if(0 == std::getline(info_stream, line)){ return; }
	if(line != ALLJOYN_CODE_SESSION_JOINED){ return; }

	// Get the info from the message
	if(0 == std::getline(info_stream, joiner)){ return; }
	if(0 == std::getline(info_stream, local_id_str)){ return; }
	if(0 == std::getline(info_stream, remote_id_str)){ return; }

	if(local_id_str.empty() || remote_id_str.empty())
	{
		return;
	}

	// Find the BusAttachment with the given app name
	std::list<BusAttachment*>::iterator it;
	for(it = m_BusAttachments.begin(); it != m_BusAttachments.end(); ++it)
	{
		GenericBusAttachment* gen_bus = (GenericBusAttachment*)(*it);
		if(gen_bus->GetAppName() == joiner)
		{
			// Add the pair of session IDs
			gen_bus->AddSessionIdPair(
				strtol(remote_id_str.c_str(), NULL, 10), strtol(local_id_str.c_str(), NULL, 10));
		}
	}
}

void XMPPConnector::handleIncomingAnnounce(string info) // TODO: in real thing, will probably have to create attachment on announce if !exists. same with foundadvertisedname
{
	std::istringstream info_stream(info);
	string line, version, port_str, busName;

	// First line is the type (announcement)
	if(0 == std::getline(info_stream, line)){ return; }
	if(line != ALLJOYN_CODE_ANNOUNCE){ return; }

	// Get the info from the message
	if(0 == std::getline(info_stream, version)){ return; }
	if(0 == std::getline(info_stream, port_str)){ return; }
	if(0 == std::getline(info_stream, busName)){ return; }

	// Find the BusAttachment with the given app name
	std::list<BusAttachment*>::iterator it;
	for(it = m_BusAttachments.begin(); it != m_BusAttachments.end(); ++it)
	{
		GenericBusAttachment* gen_bus = (GenericBusAttachment*)(*it);
		if(gen_bus->GetAppName() == busName)
		{
			relayAnnouncement(gen_bus, info);
			return;
		}
	}

	// We didn't find the bus attachment with the given name, save this announcement for later
	m_UnsentAnnouncements[busName] = info;
}

int XMPPConnector::xmppStanzaHandler(
	xmpp_conn_t* const conn,
	xmpp_stanza_t* const stanza,
	void* const userdata
	)
{
	XMPPConnector* xmppConnector = (XMPPConnector*)userdata;

	// Ignore stanzas from ourself
	string from_us = xmppConnector->getChatroomJabberId()+"/"+xmppConnector->getBusAttachment()->GetGlobalGUIDString().c_str();
	const char* from_attr = xmpp_stanza_get_attribute(stanza, "from");
	if(!from_attr || from_us == from_attr)
	{
		return 1;
	}

    char* buf = 0;
    size_t buflen = 0;
    xmpp_stanza_to_text(stanza, &buf, &buflen);
    free(buf);

	if ( 0 == strcmp("message", xmpp_stanza_get_name(stanza)) )
	{
		xmpp_stanza_t* body = 0;
		char* buf = 0;
		size_t buf_size = 0;
		if(0 != (body = xmpp_stanza_get_child_by_name(stanza, "body")) &&
		   0 == (buf_size = xmpp_stanza_to_text(body, &buf, &buf_size)))
		{
			string buf_str(buf);
			xmpp_free(xmpp_conn_get_context(conn), buf);

			// Strip the tags from the message
			if(0 != buf_str.find("<body>") && buf_str.length() != buf_str.find("</body>")+strlen("</body>"))
			{
				// Received an empty message
				return 1;
			}
			buf_str = buf_str.substr(strlen("<body>"), buf_str.length()-strlen("<body></body>"));

			// Handle the content of the message
			string type_code = buf_str.substr(0, buf_str.find_first_of('\n'));
		    cout << "Received XMPP message: " << type_code << endl;//:\n" << buf << endl;
			if(type_code == ALLJOYN_CODE_ADVERTISEMENT)
			{
				xmppConnector->handleIncomingAdvertisement(buf_str);
			}
			else if(type_code == ALLJOYN_CODE_MESSAGE)
			{
				//TODO: handleIncomingMessage(buf_str);
			}
			else if(type_code == ALLJOYN_CODE_SIGNAL)
			{
				xmppConnector->handleIncomingSignal(buf_str);
			}
			else if(type_code == ALLJOYN_CODE_NOTIFICATION)
			{
				//TODO: handleIncomingNotification(buf_str);
			}
			else if(type_code == ALLJOYN_CODE_JOIN_REQUEST)
			{
				xmppConnector->handleIncomingJoinRequest(buf_str);
			}
			else if(type_code == ALLJOYN_CODE_JOIN_RESPONSE)
			{
				xmppConnector->handleIncomingJoinResponse(buf_str);
			}
			else if(type_code == ALLJOYN_CODE_SESSION_JOINED)
			{
				xmppConnector->handleIncomingSessionJoined(buf_str);
			}
			else if(type_code == ALLJOYN_CODE_ANNOUNCE)
			{
				xmppConnector->handleIncomingAnnounce(buf_str);
			}
		}
		else
		{
			cout << "Could not parse body from XMPP message." << endl;
			return true;
		}
	}

	return 1;
}

void XMPPConnector::xmppConnectionHandler(
    xmpp_conn_t* const conn,
    const xmpp_conn_event_t event,
    const int error,
    xmpp_stream_error_t* const streamError,
    void* const userdata
    )
{
	XMPPConnector* xmppConnector = (XMPPConnector*)userdata;
	BusAttachment* bus = xmppConnector->getBusAttachment();

	if(event == XMPP_CONN_CONNECT)
	{
		// Send presence to chatroom
        xmpp_stanza_t* message = 0;
        message = xmpp_stanza_new(xmpp_conn_get_context(conn));
        xmpp_stanza_set_name(message, "presence");
        xmpp_stanza_set_attribute(message, "from", xmppConnector->getJabberId().c_str());
        xmpp_stanza_set_attribute(message, "to",
            (xmppConnector->getChatroomJabberId()+"/"+bus->GetGlobalGUIDString().c_str()).c_str());

        char* buf = 0;
        size_t buflen = 0;
        xmpp_stanza_to_text(message, &buf, &buflen);
        cout << "Sending XMPP message:\n" << buf << endl;
        free(buf);

        xmpp_send(conn, message);
        xmpp_stanza_release(message);

		// Start listening for advertisements
		QStatus err = bus->FindAdvertisedName("");
		if(err != ER_OK)
		{
			cout << "Could not find advertised names: " << QCC_StatusText(err) << endl;
		}

        // Listen for announcements
        err = AnnouncementRegistrar::RegisterAnnounceHandler(
			*bus, *((AllJoynHandler*)(xmppConnector->getBusListener())), NULL, 0);
	}
	else
	{
		cout << "Disconnected from XMPP server." << endl;

		// Stop listening for advertisements
		bus->CancelFindAdvertisedName("");

		// Stop the XMPP event loop
		xmpp_stop(xmpp_conn_get_context(conn));
	}

	// TODO: send connection status to Gateway Management app
	cout << "CONNECTION HANDLER" << endl;
}









/*******************************************************************/
// main.cpp
/*******************************************************************/
using namespace ajn;
using namespace ajn::gw;

using std::cout;
using std::endl;

static BusAttachment* s_Bus = 0;
static XMPPConnector* s_Conn = 0;

void cleanup()
{
	if (s_Conn) {
		s_Conn->stop();
		delete s_Conn;
		s_Conn = 0;
	}

	if(s_Bus) {
		s_Bus->Disconnect();
		s_Bus->Stop();
		delete s_Bus;
		s_Bus = 0;
	}
}

static void SigIntHandler(int sig)
{
	if(s_Conn)
	{
		s_Conn->stop();
	}
	cleanup();
	exit(0);
}

/*class ok : public ProxyBusObject::Listener
{
public:
	void IntrospectCB(QStatus status, ProxyBusObject* obj, void* context)
	{
		cout << "\n\nHELLOOOOOOOOOO\n\n" << endl;
	}
};*/

int main(int argc, char** argv)
{
	// hard-coding required announce properties for now...
	MsgArg all;
    MsgArg argsAnnounceData[7];

    MsgArg arg;
    uint8_t id_array[] = {253,213,157,54,89,138,109,219,154,119,132,93,215,64,87,42};
    arg.Set("ay", 16, id_array);
    qcc::String tmp = "AppId";
    argsAnnounceData[0].Set("{sv}", tmp.c_str(), &arg);
    argsAnnounceData[0].Stabilize();

    tmp = "Controlee";
    arg.Set("s", tmp.c_str());
    arg.Stabilize();
    tmp = "AppName";
    argsAnnounceData[1].Set("{sv}", tmp.c_str(), &arg);
    argsAnnounceData[1].Stabilize();

    tmp = "en";
    arg.Set("s", tmp.c_str());
    arg.Stabilize();
    argsAnnounceData[2].Set("{sv}", tmp.c_str(), &arg);
    argsAnnounceData[2].Stabilize();

    tmp = "fdd59d36598a6ddb9a77845dd740572a";
    arg.Set("s", tmp.c_str());
    arg.Stabilize();
    tmp = "DeviceId";
    argsAnnounceData[3].Set("{sv}", tmp.c_str(), &arg);
    argsAnnounceData[3].Stabilize();

    tmp = "PT Plug 40572a";
    arg.Set("s", tmp.c_str());
    arg.Stabilize();
    tmp = "DeviceName";
    argsAnnounceData[4].Set("{sv}", tmp.c_str(), &arg);
    argsAnnounceData[4].Stabilize();

    tmp = "Powertech";
    arg.Set("s", tmp.c_str());
    arg.Stabilize();
    argsAnnounceData[5].Set("{sv}", tmp.c_str(), &arg);
    argsAnnounceData[5].Stabilize();

    tmp = "ModelNumber";
    arg.Set("s", tmp.c_str());
    arg.Stabilize();
    tmp = "SmartPlug";
    argsAnnounceData[6].Set("{sv}", tmp.c_str(), &arg);
    argsAnnounceData[6].Stabilize();

    QStatus result = all.Set("a*"/*{sv}"*/, 7, argsAnnounceData);

	MsgArg* marg = AllJoynHandler::MsgArg_FromString(all.ToString());
	cout << marg->ToString() << endl;
	delete marg;

	cout << "blahblah" << endl;


	// TODO: test nested types



	//cout << qcc::StringToI32("0") << endl;

	//uint64_t array[] = {0, -1, -9999, 65535, 9999};
	/*MsgArg arg("a{sv}", 0);
	cout << arg.ToString() << endl;
	cout << AllJoynHandler::MsgArg_FromString(arg.ToString()).ToString() << endl;*/

    /*signal(SIGINT, SigIntHandler);
	s_Bus = new BusAttachment("XMPPConnector", true);

	// TODO: set up bus attachment
    QStatus status = s_Bus->Start();
    if (ER_OK != status) {
        cout << "Error starting bus: " << QCC_StatusText(status) << endl;
        cleanup();
        return 1;
    }

    status = s_Bus->Connect();
    if (ER_OK != status) {
        cout << "Error connecting bus: " << QCC_StatusText(status) << endl;
        cleanup();
        return 1;
    }*/

    /*ok okay;
	ProxyBusObject proxy(*s_Bus, "org.alljoyn.bus.samples.chat.local", "/", 0);
	QStatus err = proxy.IntrospectRemoteObjectAsync(&okay, static_cast<ProxyBusObject::Listener::IntrospectCB>(&ok::IntrospectCB), NULL, 500);
	cout << "done: " << QCC_StatusText(err) << " " << proxy.GetServiceName() << endl;

	sleep(15);*/

    // TODO: create our XMPP connector
	/*s_Conn = new XMPPConnector(s_Bus, "XMPP", "pub@aus1.affinegy.com", "pub", "alljoyn@muc.aus1.affinegy.com");
	s_Conn->start();

	cleanup();*/

	/*ProxyBusObject proxy(*s_Bus, ":8YmOhHyt.3", "/", 0);
	QStatus err = proxy.IntrospectRemoteObject();
	cout << "done: " << QCC_StatusText(err) << " " << proxy.GetPath() << endl;*/

	/*s_Bus = new BusAttachment("XMPPConnector", true);
    MyBusListener buslistener;

    InterfaceDescription* chatIntf = NULL;
    s_Bus->CreateInterface("org.alljoyn.bus.samples.chat", chatIntf);
    chatIntf->AddSignal("Chat", "s",  "str", 0);
    chatIntf->Activate();

    s_Bus->RegisterBusListener(buslistener);
	s_Bus->Start();

	//maybe register bus objs before connecting?
	MyBusObject busobj(s_Bus, "/chatService");
	s_Bus->RegisterBusObject(busobj);

	s_Bus->Connect();

	SessionPort sp = 27;
	QStatus err =s_Bus->BindSessionPort(sp, SessionOpts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY), s_SessionListener);
	if(err != ER_OK)
	{
		cout << "Could not bind session port: " << QCC_StatusText(err) << endl;
	}

	err = s_Bus->AdvertiseName("org.alljoyn.bus.samples.chat.xmpp", TRANSPORT_ANY);
	if(err != ER_OK) // TODO: only print if not "already advertising" or something
	{
		cout << "Could not advertise name: " << QCC_StatusText(err) << endl;
	}

	while(1) sleep(5);*/
}
