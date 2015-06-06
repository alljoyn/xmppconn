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

#ifndef REMOTEBUSLISTENER_H_
#define REMOTEBUSLISTENER_H_

#include <map>
#include <string>
#include "alljoyn/BusListener.h"
#include "alljoyn/SessionListener.h"
#include "alljoyn/SessionPortListener.h"

class RemoteBusAttachment;
class XMPPConnector;

class RemoteBusListener :
    public ajn::BusListener,
    public ajn::SessionListener,
    public ajn::SessionPortListener
{
public:
    RemoteBusListener(
        RemoteBusAttachment* bus,
        XMPPConnector*       connector
        );

    virtual
    ~RemoteBusListener();

    bool
    AcceptSessionJoiner(
        ajn::SessionPort        sessionPort,
        const char*             joiner,
        const ajn::SessionOpts& opts
        );

    void
    SessionJoined(
        ajn::SessionPort port,
        ajn::SessionId   id,
        const char*      joiner);

    void
    SessionLost(
        ajn::SessionId                          id,
        ajn::SessionListener::SessionLostReason reason
        );

    void
    SignalSessionJoined(
        ajn::SessionId result
        );

private:
    RemoteBusAttachment* m_bus;
    XMPPConnector*       m_connector;

    bool m_sessionJoinedSignalReceived;
    ajn::SessionId m_remoteSessionId;
    pthread_mutex_t m_sessionJoinedMutex;
    pthread_cond_t m_sessionJoinedWaitCond;

    std::map<std::string, ajn::SessionId> m_pendingSessionJoiners;
};

#endif // REMOTEBUSLISTENER_H_
