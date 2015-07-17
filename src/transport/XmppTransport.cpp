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

#include "XmppTransport.h"
#include <stdint.h>
#include <algorithm>
#include "common/xmppconnutil.h"

using namespace std;

const int KEEPALIVE_IN_SECONDS = 60;
const int XMPP_TIMEOUT_IN_MILLISECONDS = 1;

XmppTransport::XmppTransport(
    TransportListener*    listener,
    const string&         jabberid,
    const string&         password,
    const vector<string>& roster,
    const string&         chatroom,
    const bool            compress
    ) :
    Transport( listener ),
    m_jabberid(jabberid),
    m_password(password),
    m_roster(roster),
    m_chatroom(chatroom),
    m_compress(compress)
{
    xmpp_initialize();
    m_xmppctx = xmpp_ctx_new(NULL, NULL);
    m_xmppconn = xmpp_conn_new(m_xmppctx);
}

XmppTransport::~XmppTransport()
{
    xmpp_conn_release(m_xmppconn);
    xmpp_ctx_free(m_xmppctx);
    xmpp_shutdown();
}

Transport::ConnectionError
XmppTransport::RunOnce()
{
    ConnectionError err = none;

    static bool initialized = false;
    if ( !initialized )
    {
        xmpp_conn_set_jid(m_xmppconn, m_jabberid.c_str());
        xmpp_conn_set_pass(m_xmppconn, m_password.c_str());
        // If we're using a chat room then listen to messages, otherwise
        //  assume directed communication (to m_roster)
        if ( m_chatroom.empty() ) {
            xmpp_handler_add(
                m_xmppconn, XmppStanzaHandler, NULL, NULL, "chat", this);
        }
        else {
            xmpp_handler_add(
                m_xmppconn, XmppStanzaHandler, NULL, "message", NULL, this);
        }
        xmpp_handler_add(
                m_xmppconn, XmppPresenceHandler, NULL, "presence", NULL, this);
        xmpp_handler_add(
                m_xmppconn, XmppRosterHandler, "jabber:iq:roster", "iq", NULL, this);
        if ( 0 != xmpp_connect_client(
                m_xmppconn, NULL, 0, XmppConnectionHandler, this) )
        {
            // TODO: Translate the error from libstrophe to ours
            err = unknown;
            LOG_RELEASE("Failed to connect to XMPP server.");
            return err;
        }
        initialized = true;
    }

    // Process keepalive if necessary
    static const int32_t next_keepalive = KEEPALIVE_IN_SECONDS * 1000;
    static int32_t keepalive_counter = 0;
    if ( keepalive_counter >= next_keepalive )
    {
        // We need to send a keepalive packet (a single space within a stanza)
        xmpp_stanza_t* stanza = xmpp_stanza_new(m_xmppctx);
        xmpp_stanza_set_text(stanza, " ");
        xmpp_send(m_xmppconn, stanza);
        xmpp_stanza_release(stanza);

        // Reset our keepalive counter
        keepalive_counter = 0;
    }
    // NOTE: We allow the XMPP client code to track this time. This means
    //  our keepalive isn't going to be exact. This function call could
    //  actually take a long time because event handlers are being called.
    //  It doesn't matter because we really just need to make sure we send
    //  something to the server every once in a while so it doesn't drop us.
    xmpp_run_once(m_xmppctx, XMPP_TIMEOUT_IN_MILLISECONDS);
    keepalive_counter++;

    return err;
}

void
XmppTransport::StopImpl()
{
    xmpp_disconnect(m_xmppconn);
    xmpp_handler_delete(m_xmppconn, XmppStanzaHandler);
}

Transport::ConnectionError
XmppTransport::SendImpl(
    const string& message
    )
{
    string processed_body;
    if ( m_compress )
    {
        processed_body = util::str::Compress(message);
    }
    else
    {
        processed_body = message;
        util::str::EscapeXml(processed_body);
    }

    xmpp_stanza_t* messageStanza = xmpp_stanza_new(m_xmppctx);
    xmpp_stanza_set_name(messageStanza, "message");
    if ( m_chatroom.empty() && !m_roster.empty() ) {
        // TODO: When roster is actually a roster and not a destination
        //  we will need to instead add a parameter to this function
        //  and pass it in. We will also need to keep track of which AllJoyn
        //  bits are travelling to/from each destination. That will be done
        //  outside this class, however, so this function will blindly send
        //  to whoever was passed in as the destination.
        xmpp_stanza_set_attribute(messageStanza, "to", m_roster.front().c_str());
        xmpp_stanza_set_type(messageStanza, "chat");
    }
    else{
        xmpp_stanza_set_attribute(messageStanza, "to", m_chatroom.c_str());
        xmpp_stanza_set_type(messageStanza, "groupchat");
    }

    xmpp_stanza_t* bodyStanza = xmpp_stanza_new(m_xmppctx);
    xmpp_stanza_set_name(bodyStanza, "body");

    xmpp_stanza_t* textStanza = xmpp_stanza_new(m_xmppctx);
    xmpp_stanza_set_text(textStanza, processed_body.c_str());

    xmpp_stanza_add_child(bodyStanza, textStanza);
    xmpp_stanza_release(textStanza);
    xmpp_stanza_add_child(messageStanza, bodyStanza);
    xmpp_stanza_release(bodyStanza);

    char* buf = NULL;
    size_t buflen = 0;
    xmpp_stanza_to_text(messageStanza, &buf, &buflen);
    LOG_DEBUG("Sending XMPP message");
    LOG_VERBOSE("Message: %s", buf);
    xmpp_free(m_xmppctx, buf);

    xmpp_send(m_xmppconn, messageStanza);
    xmpp_stanza_release(messageStanza);

    return none;
}

int
XmppTransport::XmppStanzaHandler(
    xmpp_conn_t* const   conn,
    xmpp_stanza_t* const stanza,
    void* const          userdata )
{
    XmppTransport* transport = static_cast<XmppTransport*>(userdata);

    // Get the stanza as text so we can parse it
    char* buf = 0;
    size_t buflen = 0;
    int result = xmpp_stanza_to_text( stanza, &buf, &buflen );
    if ( XMPP_EOK != result )
    {
        LOG_RELEASE("Failed to get message/chat stanza as text! %d", result);
        return 1;
    }
    string message(buf);
    xmpp_free(xmpp_conn_get_context(conn), buf);

    // Ignore if it isn't in our roster
    // TODO: Eventually this will be a full roster, right now we support only a single destination
    const char* fromAttr = xmpp_stanza_get_attribute(stanza, "from");

    std::string fromAttrTmp = transport->m_roster.front();
    std::string fromAttrLower = fromAttrTmp.substr(0, fromAttrTmp.find("@"));
    std::transform(fromAttrLower.begin(), fromAttrLower.end(), fromAttrLower.begin(), ::tolower);
    fromAttrTmp.replace(0, fromAttrTmp.find("@"), fromAttrLower);

    if ( transport->m_roster.empty() || fromAttrTmp != fromAttr )
    {
        LOG_DEBUG("Ignoring message/chat from non-roster entity: %s", fromAttr);
        return 1;
    }

    // Logging
    LOG_DEBUG("Received message/chat stanza");
    LOG_DEBUG("From: %s", fromAttr);
    LOG_VERBOSE("Stanza: %s", message.c_str());

    FNLOG
        if ( 0 == strcmp("message", xmpp_stanza_get_name(stanza)) ||
                0 == strcmp("chat", xmpp_stanza_get_name(stanza)) )
        {
            xmpp_stanza_t* body = 0;
            if(0 != (body = xmpp_stanza_get_child_by_name(stanza, "body")) &&
                    XMPP_EOK == xmpp_stanza_to_text(body, &buf, &buflen))
            {   
                string message_body(buf);
                xmpp_free(xmpp_conn_get_context(conn), buf);

                // Strip the tags from the message
                if(0 != message_body.find("<body>") &&
                        message_body.length() !=
                        message_body.find("</body>")+strlen("</body>"))
                {
                    // Received an empty message
                    LOG_RELEASE("Received an empty message.")
                        return 1;
                }
                message_body = message_body.substr(strlen("<body>"),
                        message_body.length()-strlen("<body></body>"));

                // Decompress the message
                if ( transport->m_compress )
                {
                    message_body = util::str::Decompress(message_body);
                }
                else
                {
                    util::str::UnescapeXml(message_body);
                }

                // Allow the sink to handle the message
                transport->MessageReceived( fromAttr, message_body );
            }
            else
            {
                LOG_RELEASE("Could not parse body from XMPP message.");
            }
        }

    return 1;
}

    int
XmppTransport::XmppPresenceHandler(
        xmpp_conn_t* const   conn,
        xmpp_stanza_t* const stanza,
        void* const          userdata
        )
{
    FNLOG
        XmppTransport* transport = static_cast<XmppTransport*>(userdata);

    if ( 0 == strcmp("presence", xmpp_stanza_get_name(stanza)) )
    {
        /*        // First determine if it's a probe and answer it if necessary
                  const char* type = xmpp_stanza_get_attribute(stanza, "type");
                  if ( type && string(type) == "probe" ) {
                  const char* from = xmpp_stanza_get_attribute(stanza, "from");
                  const char* to = 
                  return 1;
                  }
                  */
        // Get the stanza as text so we can parse it
        // NOTE: Doing this first before checking to ignore so that
        //  we output the stanza info to the log when debugging issues
        //  with presence notifications.
        char* buf = 0;
        size_t buflen = 0;
        int result = xmpp_stanza_to_text( stanza, &buf, &buflen );
        if ( XMPP_EOK != result ) {
            LOG_RELEASE("Failed to get presence stanza as text! %d", result);
            return 1;
        }
        LOG_VERBOSE("Stanza: %s", buf);
        string message(buf);
        xmpp_free(xmpp_conn_get_context(conn), buf);

        // Ignore if it isn't in our roster
        // TODO: Eventually this will be a full roster, right now we support only a single destination
        const char* fromAttr = xmpp_stanza_get_attribute(stanza, "from");
        std::string fromAttrTmp = transport->m_roster.front();
        std::string fromAttrLower = fromAttrTmp.substr(0, fromAttrTmp.find("@"));
        LOG_DEBUG("%s", fromAttrLower.c_str());
        std::transform(fromAttrLower.begin(), fromAttrLower.end(), fromAttrLower.begin(), ::tolower);
        LOG_DEBUG("%s", fromAttrLower.c_str());
        fromAttrTmp.replace(0, fromAttrTmp.find("@"), fromAttrLower);
        LOG_DEBUG("%s", fromAttrTmp.c_str());
        if ( transport->m_roster.empty() || fromAttrTmp != fromAttr ) {
            LOG_DEBUG("Ignoring presence from non-roster entity: %s", fromAttr);
            return 1;
        }

        // Logging
        LOG_DEBUG("Received Presence Stanza");
        LOG_DEBUG("From: %s", fromAttr);

        // Check if it's online/offline presence notification
        // Example of a presence stanza:
        //  <presence xml:lang=""
        //   to="test-connector@xmpp.chariot.global/test-connector"
        //   from="test-connector@muc.xmpp.chariot.global/test">
        //      <priority>1</priority>
        //      <c hash="sha-1" xmlns="http://jabber.org/protocol/caps"
        //       ver="71LAP/wlWGfun7j+Q4FCSSuAhQw=" node="http://pidgin.im/"/>
        //      <x xmlns="vcard-temp:x:update">
        //          <photo>677584b5b6a6f6a91cad4cfad0b13a4949d53faa</photo>
        //      </x>
        //      <x xmlns="http://jabber.org/protocol/muc#user">
        //          <item role="participant" affiliation="none"/>
        //      </x>
        //  </presence>
        const string participant_lookup("role=\"participant\""); // TODO: we should do something more robust than this
        const string moderator_lookup("role=\"moderator\""); // TODO: we should do something more robust than this
        if ( message.npos != message.find(participant_lookup) ||
                message.npos != message.find(moderator_lookup) )
        {
            // The remote entity has come online
            transport->RemoteSourcePresenceStateChanged( fromAttr, connected, none );
        }
        else
        {
            // The remote entity has gone offline
            transport->RemoteSourcePresenceStateChanged( fromAttr, disconnected, none );
        }
    }

    return 1;
}

    int
XmppTransport::XmppRosterHandler(
        xmpp_conn_t* const   conn,
        xmpp_stanza_t* const stanza,
        void* const          userdata
        )
{
    XmppTransport* transport = static_cast<XmppTransport*>(userdata);

    // TODO: Implement. Right now this is just a placeholder

    FNLOG
        if ( 0 == strcmp("iq", xmpp_stanza_get_name(stanza)) )
        {
            LOG_DEBUG("Received Roster Stanza");

            char* buf = 0;
            size_t buflen = 0;
            int result = xmpp_stanza_to_text( stanza, &buf, &buflen );
            if ( XMPP_EOK != result ) {
                LOG_RELEASE("Failed to get roster stanza as text! %d", result);
                return 1;
            }
            //LOG_VERBOSE("Stanza: %s", buf);
            string message(buf);
            xmpp_free(xmpp_conn_get_context(conn), buf);

            const char* fromAttr = xmpp_stanza_get_attribute(stanza, "from");
            LOG_VERBOSE("From: %s", fromAttr);
            const char* toAttr = xmpp_stanza_get_attribute(stanza, "to");
            LOG_VERBOSE("To: %s", toAttr);
        }

    return 1;
}

    void
XmppTransport::XmppConnectionHandler(
        xmpp_conn_t* const         conn,
        const xmpp_conn_event_t    event,
        const int                  err,
        xmpp_stream_error_t* const streamerror,
        void* const                userdata
        )
{
    FNLOG
        XmppTransport* transport = static_cast<XmppTransport*>(userdata);
    ConnectionState prevConnState = transport->GetConnectionState();

    switch(event)
    {
        case XMPP_CONN_CONNECT:
            {
                transport->GlobalConnectionStateChanged( connected, none );

                // Set up our presence message
                xmpp_stanza_t* presence = xmpp_stanza_new(xmpp_conn_get_context(conn));
                xmpp_stanza_set_name(presence, "presence");
                xmpp_stanza_set_attribute(presence, "from",
                        transport->m_jabberid.c_str());

                // If a chat room was specified then build that into the
                //  presence message
                if ( !transport->m_chatroom.empty() ) {
                    // Set the "to" field for this presence message to the chatroom
                    xmpp_stanza_set_attribute(presence, "to",
                            transport->m_chatroom.c_str());

                    // Create a child object of the presence message called 'x' to
                    //  specify the XML namespace and hold the history object
                    xmpp_stanza_t* x = xmpp_stanza_new(xmpp_conn_get_context(conn));
                    xmpp_stanza_set_name(x, "x");
                    xmpp_stanza_set_attribute(x, "xmlns", "http://jabber.org/protocol/muc");

                    // Create a child object of 'x' called 'history' and set it to 0
                    //  so that we don't keep the chat history
                    xmpp_stanza_t* history = xmpp_stanza_new(xmpp_conn_get_context(conn));
                    xmpp_stanza_set_name(history, "history");
                    xmpp_stanza_set_attribute(history, "maxchars", "0");

                    // Add the child XML nodes
                    xmpp_stanza_add_child(x, history);
                    xmpp_stanza_release(history);
                    xmpp_stanza_add_child(presence, x);
                    xmpp_stanza_release(x);
                }

                // Logging
                LOG_DEBUG("Sending XMPP presence message");
                char* buf = NULL;
                size_t buflen = 0;
                xmpp_stanza_to_text(presence, &buf, &buflen);
                LOG_VERBOSE("Presence Message: %s", buf);
                xmpp_free(xmpp_conn_get_context(conn), buf);

                // Send our presence message
                xmpp_send(conn, presence);
                xmpp_stanza_release(presence);
                break;
            }
        case XMPP_CONN_DISCONNECT:
            {
                LOG_RELEASE("Disconnected from XMPP server.");
                if ( disconnecting == transport->GetConnectionState() )
                {
                    LOG_RELEASE("Exiting.");

                    // Stop the XMPP event loop
                    transport->GlobalConnectionStateChanged( disconnected, none );
                }
                else if ( uninitialized != transport->GetConnectionState() )
                {
                    // TODO: Try to restart the connection. For now we will quit the
                    //  application because of this, but eventually we will try to
                    //  restart the connection
                    transport->GlobalConnectionStateChanged( disconnected, none );
                }
                else
                {
                    // TODO: Set the correct error
                    ConnectionError err = auth_failure;
                    LOG_RELEASE("Login failed.");

                    // Go ahead and quit the application by stopping the XMPP event loop
                    transport->GlobalConnectionStateChanged( disconnected, err );
                }
                break;
            }
        case XMPP_CONN_FAIL:
        default:
            {
                // TODO: Set the correct error
                ConnectionError err = unknown;
                LOG_RELEASE("XMPP error occurred. Exiting.");

                // Stop the XMPP event loop
                transport->GlobalConnectionStateChanged( error, err );

                break;
            }
    }
}
