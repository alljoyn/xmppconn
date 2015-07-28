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
