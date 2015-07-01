#ifndef COMMONBUSLISTENER_H_
#define COMMONBUSLISTENER_H_

#include <alljoyn/BusAttachment.h>
#include <alljoyn/BusListener.h>
#include <alljoyn/SessionPortListener.h>
#include <vector>

class CommonBusListener : public ajn::BusListener, public ajn::SessionPortListener, public ajn::SessionListener {

  public:
    CommonBusListener(ajn::BusAttachment* bus = NULL, void(*daemonDisconnectCB)() = NULL);
    ~CommonBusListener();
    bool AcceptSessionJoiner(ajn::SessionPort sessionPort, const char* joiner, const ajn::SessionOpts& opts);
    void setSessionPort(ajn::SessionPort sessionPort);
    void SessionJoined(ajn::SessionPort sessionPort, ajn::SessionId id, const char* joiner);
    void SessionLost(ajn::SessionId sessionId, SessionLostReason reason);
    void BusDisconnected();
    const std::vector<ajn::SessionId>& getSessionIds() const;
    ajn::SessionPort getSessionPort();
  private:
    ajn::SessionPort m_SessionPort;
    ajn::BusAttachment* m_Bus;
    std::vector<ajn::SessionId> m_SessionIds;
    void (*m_DaemonDisconnectCB)();

};

#endif
