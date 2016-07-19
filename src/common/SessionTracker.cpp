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

#include "SessionTracker.h"
#include "common/xmppconnutil.h"

SessionTracker::SessionTracker()
{
    pthread_mutex_init(&m_mutex, NULL);
}
SessionTracker::~SessionTracker()
{
    pthread_mutex_destroy(&m_mutex);
}

void SessionTracker::JoinSent( const string& name, const SessionId& session_id )
{
    FNLOG;
    Lock();
    m_pending[name] = session_id;
    Unlock();
}

void SessionTracker::JoinConfirmed( const string& name )
{
    FNLOG;
    Lock();
    if ( m_pending.end() != m_pending.find(name) )
    {
        m_sessions[name] = m_pending.at(name);
        m_pending.erase(name);
    }
    else
    {
        LOG_RELEASE("Tried to confirm a session join for '%s' but the join appears to have never been sent!", name.c_str());
    }
    Unlock();
}

void SessionTracker::SessionLost( const string& name )
{
    FNLOG;
    Lock();
    if ( m_sessions.end() != m_sessions.find(name) )
    {
        m_sessions.erase(name);
    }
    // NOTE: We don't remove from pending sessions list here because
    //  we may be inside of a pending session when we need to call
    //  this function and need to keep it in that list.
    Unlock();
}

bool SessionTracker::IsSessionPending( const string& name ) const
{
    FNLOG;
    bool found = false;
    Lock();
    if ( m_pending.end() != m_pending.find(name) )
    {
        found = true;
    }
    Unlock();
    return found;
}

bool SessionTracker::IsSessionJoined( const string& name ) const
{
    FNLOG;
    bool found = false;
    Lock();
    if ( m_sessions.end() != m_sessions.find(name) )
    {
        found = true;
    }
    Unlock();
    return found;
}

bool SessionTracker::IsSessionPendingOrJoined( const string& name ) const
{
    FNLOG;
    bool found = false;
    Lock();
    if ( m_sessions.end() != m_sessions.find(name) ||
         m_pending.end() != m_pending.find(name) )
    {
        found = true;
    }
    Unlock();
    return found;
}

SessionId SessionTracker::GetSession( const string& name ) const
{
    FNLOG;
    SessionId session_id = 0;
    Lock();

    // There are two maps matching names to session IDs:
    // one is a map of pending sessions, the other one of current sessions.
    // It is currently possible to have both a pending session and a current
    // session of the same name (for example, if the session loss was never
    // received).
    // Currently, GetSession() will return the PENDING session if both are
    // available.
    // TODO: If there is a pending session and a current session for the same
    // joiner, might want to return some special status code to the caller to
    // indicate a possible problem.
    if ( m_pending.end() != m_pending.find(name) )
    {
        session_id = m_pending.at(name);
    }
    else if ( m_sessions.end() != m_sessions.find(name) )
    {
        session_id = m_sessions.at(name);
    }

    Unlock();
    return session_id;
}

void SessionTracker::Lock() const
{
    pthread_mutex_lock(&m_mutex);
}

void SessionTracker::Unlock() const
{
    pthread_mutex_unlock(&m_mutex);
}
