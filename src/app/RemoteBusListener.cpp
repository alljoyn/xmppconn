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

#include "RemoteBusListener.h"
#include <pthread.h>
#include "XMPPConnector.h"
#include "RemoteBusAttachment.h"

using namespace std;
using namespace ajn;

RemoteBusListener::RemoteBusListener(
    RemoteBusAttachment* bus,
    XMPPConnector*       connector
    ) :
    m_bus(bus),
    m_connector(connector),
    m_sessionJoinedSignalReceived(false),
    m_remoteSessionId(0),
    m_pendingSessionJoiners()
{
    pthread_mutex_init(&m_sessionJoinedMutex, NULL);
    pthread_cond_init(&m_sessionJoinedWaitCond, NULL);
}

RemoteBusListener::~RemoteBusListener()
{
    pthread_mutex_destroy(&m_sessionJoinedMutex);
    pthread_cond_destroy(&m_sessionJoinedWaitCond);
}

bool
RemoteBusListener::AcceptSessionJoiner(
    SessionPort        sessionPort,
    const char*        joiner,
    const SessionOpts& opts
    )
{
    m_bus->EnableConcurrentCallbacks();

    // Gather interfaces to be implemented on the remote end
    vector<util::bus::BusObjectInfo> busObjects;
    ProxyBusObject proxy(*m_bus, joiner, "/", 0);
    util::bus::GetBusObjectsRecursive(busObjects, proxy);

    // Lock the session join mutex
    pthread_mutex_lock(&m_sessionJoinedMutex);
    m_sessionJoinedSignalReceived = false;
    m_remoteSessionId = 0;

    // Send the session join request via XMPP
    m_connector->SendJoinRequest(
            m_bus->RemoteName(), sessionPort, joiner, opts, busObjects);

    // Wait for the XMPP response signal
    timespec wait_time = {time(NULL)+10, 0};
    while(!m_sessionJoinedSignalReceived)
    {
        if(ETIMEDOUT == pthread_cond_timedwait(
                &m_sessionJoinedWaitCond,
                &m_sessionJoinedMutex,
                &wait_time))
        {
            break;
        }
    }

    bool returnVal = (m_remoteSessionId != 0);
    if(returnVal)
    {
        m_pendingSessionJoiners[joiner] = m_remoteSessionId;
    }

    pthread_mutex_unlock(&m_sessionJoinedMutex);

    return returnVal;
}

void
RemoteBusListener::SessionJoined(
    SessionPort port,
    SessionId   id,
    const char* joiner)
{
    // Find the id of the remote session
    SessionId remoteSessionId = 0;
    pthread_mutex_lock(&m_sessionJoinedMutex);
    map<string, SessionId>::iterator idIter =
            m_pendingSessionJoiners.find(joiner);
    if(idIter != m_pendingSessionJoiners.end())
    {
        remoteSessionId = idIter->second;
        m_pendingSessionJoiners.erase(idIter);
    }
    pthread_mutex_unlock(&m_sessionJoinedMutex);

    // Register as a session listener
    m_bus->SetSessionListener(id, this);

    // Store the remote/local session id pair
    m_bus->AddSession(id, joiner, port, remoteSessionId);

    // Send the session Id back across the XMPP server
    m_connector->SendSessionJoined(
            joiner, m_bus->RemoteName(), port, remoteSessionId, id);

    LOG_DEBUG("Session joined between %s (remote) and %s (local). Port: %d Id: %d",
            joiner, m_bus->RemoteName().c_str(), port, id);
}

void
RemoteBusListener::SessionLost(
    SessionId         id,
    SessionLostReason reason
    )
{
    QCC_UNUSED(reason);
    LOG_DEBUG("Session lost. Attachment: %s Id: %d",
            m_bus->RemoteName().c_str(), id);

    string peer = m_bus->GetPeerBySessionId(id);
    if(!peer.empty())
    {
        m_bus->RemoveSession(id);
        m_connector->SendSessionLost(peer, id);
    }
}

void
RemoteBusListener::SignalSessionJoined(
    SessionId result
    )
{
    pthread_mutex_lock(&m_sessionJoinedMutex);
    m_sessionJoinedSignalReceived = true;
    m_remoteSessionId = result;
    pthread_cond_signal(&m_sessionJoinedWaitCond);
    pthread_mutex_unlock(&m_sessionJoinedMutex);
}
