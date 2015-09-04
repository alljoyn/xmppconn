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

#include "Transport.h"
#include <pthread.h>
#include "common/xmppconnutil.h"

using namespace std;

pthread_mutex_t* transport_get_mutex( void* mtx )
{
	return reinterpret_cast<pthread_mutex_t*>(mtx);
}

#define LOCK 	pthread_mutex_lock(transport_get_mutex(m_mtx))
#define UNLOCK 	pthread_mutex_unlock(transport_get_mutex(m_mtx))

Transport::Transport( TransportListener* listener ) :
	m_connection_state( uninitialized ),
	m_connection_error( none ),
	m_listener( listener ),
	m_mtx( 0 )
{
	pthread_mutex_t* mtx = new pthread_mutex_t();
	pthread_mutex_init( mtx, NULL );
	m_mtx = mtx;
}

Transport::~Transport()
{
	pthread_mutex_t* mtx = transport_get_mutex(m_mtx);
	delete mtx;
	m_mtx = 0;
}

Transport::ConnectionError
Transport::Run()
{
    Transport::ConnectionError error = none;
    Transport::ConnectionState conn_state = uninitialized;
    while ( conn_state != disconnected && error == none )
    {
        error = RunOnce();
        conn_state = Transport::GetConnectionState();
    }

    return error;
}

void
Transport::Stop()
{
	Transport::SetConnectionState( disconnecting );
	StopImpl();
}

Transport::ConnectionError
Transport::Send(
    const string& message
	)
{
	return SendImpl( message );
}

void
Transport::MessageReceived(
    const string& source,
    const string& message
    )
{
	if ( !m_listener )
	{
		return;
	}

	m_listener->MessageReceived( source, message );
}

void
Transport::GlobalConnectionStateChanged(
    const Transport::ConnectionState& new_state,
    const Transport::ConnectionError& error
    )
{
	if ( !m_listener )
	{
		return;
	}

	LOG_VERBOSE("Transport::GlobalConnectionStateChanged, setting conn state to %d", new_state);
	SetConnectionState( new_state );
	SetConnectionError( error );
	m_listener->GlobalConnectionStateChanged( new_state, error );
}

void
Transport::RemoteSourcePresenceStateChanged(
	const std::string&                source,
    const Transport::ConnectionState& new_state,
    const Transport::ConnectionError& error
    )
{
	if ( !m_listener )
	{
		return;
	}

	m_listener->RemoteSourcePresenceStateChanged( source, new_state, error );
}

Transport::ConnectionState
Transport::GetConnectionState() const
{
	LOCK;
	Transport::ConnectionState state = m_connection_state;
	UNLOCK;

	return state;
}

Transport::ConnectionState
Transport::SetConnectionState(
	const Transport::ConnectionState& new_state
	)
{
	LOCK;
	Transport::ConnectionState prev_state = m_connection_state;
	m_connection_state = new_state;
	UNLOCK;

	return prev_state;
}

Transport::ConnectionError
Transport::GetConnectionError() const
{
    LOCK;
    Transport::ConnectionError error = m_connection_error;
    UNLOCK;

    return error;
}

Transport::ConnectionError
Transport::SetConnectionError(
    const Transport::ConnectionError& error
    )
{
    LOCK;
    Transport::ConnectionError prev_error = m_connection_error;
    m_connection_error = error;
    UNLOCK;

    return prev_error;
}
