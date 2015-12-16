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

#ifndef XMPPTRANSPORT_H_
#define XMPPTRANSPORT_H_

#include "transport/Transport.h"
#include <strophe.h>
#include <vector>


class XmppTransport : public Transport
{
    XmppTransport() : Transport(0), m_compress(false) {}
public:

    XmppTransport(
        TransportListener*              listener,
        const std::string&              jabberid,
        const std::string&              password,
        const std::vector<std::string>& roster,
        const std::string&              chatroom,
        const bool                      compress
        );
    virtual ~XmppTransport();

private:
    virtual
    ConnectionError
    RunOnce();

    virtual
    void
    StopImpl();

    virtual
    ConnectionError
    SendImpl(
        const std::string& message
    );

    static
    void
    AddToRoster(XmppTransport* transport);

    static
    int
    XmppStanzaHandler(
        xmpp_conn_t* const   conn,
        xmpp_stanza_t* const stanza,
        void* const          userdata
        );

    static
    int
    XmppPresenceHandler(
        xmpp_conn_t* const   conn,
        xmpp_stanza_t* const stanza,
        void* const          userdata
        );

    static
    int
    XmppRosterHandler(
        xmpp_conn_t* const   conn,
        xmpp_stanza_t* const stanza,
        void* const          userdata
        );

    static
    void
    XmppConnectionHandler(
        xmpp_conn_t* const         conn,
        const xmpp_conn_event_t    event,
        const int                  error,
        xmpp_stream_error_t* const streamerror,
        void* const                userdata
        );

private:
    const std::string              m_jabberid;
    const std::string              m_password;
    const std::string              m_chatroom;
    const std::vector<std::string> m_roster;
    const bool                     m_compress;
    xmpp_ctx_t*                    m_xmppctx;
    xmpp_conn_t*                   m_xmppconn;
    bool                           m_initialized;
};

#endif // XMPPTRANSPORT_H_

