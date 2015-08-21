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

// Helper function to deal with converting C char array to std::string safely
inline string convertToString(const char* s)
{
    return (s ? string(s) : string(""));
}

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
    m_compress(compress),
    m_initialized(false)
{
    xmpp_initialize();
    //xmpp_log_t* m_log;
    //m_xmppctx = xmpp_ctx_new(NULL, NULL);
    m_xmppctx = xmpp_ctx_new(NULL, xmpp_get_default_logger(XMPP_LEVEL_DEBUG));
    m_xmppconn = xmpp_conn_new(m_xmppctx);
}

XmppTransport::~XmppTransport()
{
    xmpp_conn_release(m_xmppconn);
    xmpp_ctx_free(m_xmppctx);
    xmpp_shutdown();
}

#if 0
void log_handler(void * const userdata,
                 const xmpp_log_level_t level,
                 const char * const area,
                 const char * const msg)
{
    printf("Log msg: %s\n", msg);
}
#endif

#include <ctime>
Transport::ConnectionError
XmppTransport::RunOnce()
{
    ConnectionError err = none;
    //m_log.handler = xmpp_get_default_logger(XMPP_LEVEL_DEBUG);

    if ( !m_initialized )
    {
        printf("init\n");
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
                m_xmppconn, XmppRosterHandler, XMPP_NS_ROSTER, "iq", NULL, this);

        printf("Connecting to server\n");
        if ( 0 != xmpp_connect_client(
                m_xmppconn, NULL, 0, XmppConnectionHandler, this) )
        {
            // TODO: Translate the error from libstrophe to ours
            err = unknown;
            LOG_RELEASE("Failed to connect to XMPP server.");
            return err;
        }

        m_initialized = true;
    }

    if ( m_initialized && (disconnected == GetConnectionState()) )
    {
        return not_connected;
    }

    //{
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

        //printf("After xmpp_run_once called, conn state = %d\n", GetConnectionState());
        keepalive_counter++;

        if ( keepalive_counter % 10000 == 0 )
        {
            xmpp_disconnect(m_xmppconn);
            //printf("RunOnce %d\n", i);
            time_t     now = time(0);
            struct tm  tstruct;
            char       buf[80];
            tstruct = *localtime(&now);
            // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
            // for more information about date/time format
            strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
            printf("Time stamp: %s\n", buf);
            fflush(stdout);
        }
    //}
    //else
    //{
    //    err = not_connected;
    //}

    return err;
}

void
XmppTransport::StopImpl()
{
    if ( m_xmppconn )
    {
        xmpp_disconnect(m_xmppconn);
        xmpp_handler_delete(m_xmppconn, XmppStanzaHandler);
        m_xmppconn = NULL;
    }
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
        std::string tmp = m_chatroom;
        tmp = tmp.substr(0, tmp.find("/"));
        xmpp_stanza_set_attribute(messageStanza, "to", tmp.c_str());
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


void XmppTransport::AddToRoster(XmppTransport* transport)
{
    xmpp_stanza_t *iq(NULL), *query(NULL), *rosterItem(NULL);
    char* buf(NULL);
    size_t buflen = 0;

    // Add roster line from the config file to roster on the server
    // Example of a roster update request (push):
    // <iq id="roster_add" type="set">
    //     <query xmlns="jabber:iq:roster">
    //         <item jid="exampleR@server.com"/>
    //     </query>
    // </iq>
    // TODO: Support multiple entries in the roster
    iq = xmpp_stanza_new(transport->m_xmppctx);
    xmpp_stanza_set_name(iq, "iq");
    xmpp_stanza_set_type(iq, "set");
    xmpp_stanza_set_id(iq, "roster_add");

    query = xmpp_stanza_new(transport->m_xmppctx);
    xmpp_stanza_set_name(query, "query");
    xmpp_stanza_set_ns(query, XMPP_NS_ROSTER);

    rosterItem = xmpp_stanza_new(transport->m_xmppctx);
    xmpp_stanza_set_name(rosterItem, "item");
    xmpp_stanza_set_attribute(rosterItem, "jid", transport->m_roster.front().c_str());

    xmpp_stanza_add_child(query, rosterItem);
    xmpp_stanza_add_child(iq, query);
    xmpp_stanza_to_text(iq, &buf, &buflen);
    LOG_VERBOSE("Sending roster push: %s", buf);
    xmpp_send(transport->m_xmppconn, iq);

    xmpp_stanza_release(rosterItem);
    xmpp_stanza_release(query);
    xmpp_stanza_release(iq);
    xmpp_free(transport->m_xmppctx, buf);

    // Get the updated roster
    iq = xmpp_stanza_new(transport->m_xmppctx);
    xmpp_stanza_set_name(iq, "iq");
    xmpp_stanza_set_type(iq, "get");
    xmpp_stanza_set_id(iq, "roster_get");

    query = xmpp_stanza_new(transport->m_xmppctx);
    xmpp_stanza_set_name(query, "query");
    xmpp_stanza_set_ns(query, XMPP_NS_ROSTER);
    xmpp_stanza_add_child(iq, query);

    xmpp_stanza_to_text(iq, &buf, &buflen);
    LOG_VERBOSE("Sending roster get request: %s", buf);

    xmpp_send(transport->m_xmppconn, iq);
    xmpp_stanza_release(query);
    xmpp_stanza_release(iq);
    xmpp_free(transport->m_xmppctx, buf);
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
    string fromAttr = convertToString(xmpp_stanza_get_attribute(stanza, "from"));
    std::string fromAttrTmp("");

    if ( !(transport->m_roster.empty()) )
    {
        fromAttrTmp = transport->m_roster.front();
        std::string fromAttrLower = fromAttrTmp.substr(0, fromAttrTmp.find("@"));
        std::transform(fromAttrLower.begin(), fromAttrLower.end(), fromAttrLower.begin(), ::tolower);
        fromAttrTmp.replace(0, fromAttrTmp.find("@"), fromAttrLower);

        std::transform(fromAttr.begin(), fromAttr.end(), fromAttr.begin(), ::tolower);
    }

    if ( transport->m_roster.empty() || fromAttrTmp != fromAttr )
    {
        LOG_DEBUG("Ignoring message/chat from non-roster entity: %s", fromAttr.c_str());
        return 1;
    }

    // Logging
    LOG_DEBUG("Received message/chat stanza");
    LOG_DEBUG("From: %s", fromAttr.c_str());
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
    string fromAttr = convertToString(xmpp_stanza_get_attribute(stanza, "from"));
    std::string fromAttrTmp("");

    if ( !(transport->m_roster.empty()) )
    {
        fromAttrTmp = transport->m_roster.front();
        std::string fromAttrLower = fromAttrTmp.substr(0, fromAttrTmp.find("@"));
        LOG_DEBUG("Roster contains: %s", fromAttrLower.c_str());
        std::transform(fromAttrLower.begin(), fromAttrLower.end(), fromAttrLower.begin(), ::tolower);
        LOG_DEBUG("Roster contains: %s", fromAttrLower.c_str());
        fromAttrTmp.replace(0, fromAttrTmp.find("@"), fromAttrLower);
        LOG_DEBUG("Roster contains: %s", fromAttrTmp.c_str());

        std::transform(fromAttr.begin(), fromAttr.end(), fromAttr.begin(), ::tolower);
    }

    if ( transport->m_roster.empty() || fromAttrTmp != fromAttr ) {
        LOG_DEBUG("Ignoring presence from non-roster entity:  %s", fromAttr.c_str());
        return 1;
    }

    // Logging
    LOG_DEBUG("Received Presence Stanza");
    LOG_DEBUG("From: %s", fromAttr.c_str());

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


    string presenceType = convertToString(xmpp_stanza_get_type(stanza));

    if ( presenceType == "unavailable" )
    {
        LOG_DEBUG("Remote entity has gone offline");
        transport->RemoteSourcePresenceStateChanged( fromAttr, disconnected, none );
    }
    else
    {
        LOG_DEBUG("Remote entity has come online");
        transport->RemoteSourcePresenceStateChanged( fromAttr, connected, none );
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

    FNLOG
    LOG_DEBUG("Received Roster Stanza");

    char* buf = 0;
    size_t buflen = 0;
    int result = xmpp_stanza_to_text( stanza, &buf, &buflen );
    if ( XMPP_EOK != result ) {
        LOG_RELEASE("Failed to get roster stanza as text! %d", result);
        return 1;
    }
    LOG_VERBOSE("Stanza: %s", buf);
    string message(buf);
    xmpp_free(xmpp_conn_get_context(conn), buf);

    const char* fromAttr = xmpp_stanza_get_attribute(stanza, "from");
    LOG_VERBOSE("From: %s", fromAttr);
    const char* toAttr = xmpp_stanza_get_attribute(stanza, "to");
    LOG_VERBOSE("To: %s", toAttr);


    // If received a reply with the updated roster, send presence message
    string typeAttr = convertToString(xmpp_stanza_get_attribute(stanza, "type"));
    if (typeAttr == "result")
    {
        XmppTransport* transport = static_cast<XmppTransport*>(userdata);

        // Set up our presence message
        xmpp_ctx_t* xmppCtx = xmpp_conn_get_context(conn);
        xmpp_stanza_t* presence = xmpp_stanza_new(xmppCtx);
        xmpp_stanza_set_name(presence, "presence");
        xmpp_stanza_set_attribute(presence, "from",
                transport->m_jabberid.c_str());

        // If a chat room was specified then build that into the
        //  presence message
        if ( !transport->m_chatroom.empty() )
        {
            // Set the "to" field for this presence message to the chatroom
            xmpp_stanza_set_attribute(presence, "to",
                    transport->m_chatroom.c_str());

            // Create a child object of the presence message called 'x' to
            //  specify the XML namespace and hold the history object
            xmpp_stanza_t* x = xmpp_stanza_new(xmpp_conn_get_context(transport->m_xmppconn));
            xmpp_stanza_set_name(x, "x");
            xmpp_stanza_set_attribute(x, "xmlns", "http://jabber.org/protocol/muc");

            // Create a child object of 'x' called 'history' and set it to 0
            //  so that we don't keep the chat history
            xmpp_stanza_t* history = xmpp_stanza_new(xmpp_conn_get_context(transport->m_xmppconn));
            xmpp_stanza_set_name(history, "history");
            xmpp_stanza_set_attribute(history, "maxchars", "0");

            // Add the child XML nodes
            xmpp_stanza_add_child(x, history);
            xmpp_stanza_release(history);
            xmpp_stanza_add_child(presence, x);
            xmpp_stanza_release(x);
        }
        else    // Set the "to" field for this presence message to the roster entity
        {        // TODO: Handle multiple roster entries
            if ( !transport->m_roster.empty() )
            {
                xmpp_stanza_set_attribute(presence, "to",
                    transport->m_roster.front().c_str());
            }
        }

        // Logging
        LOG_DEBUG("Sending XMPP presence message");
        char* buf = NULL;
        size_t buflen = 0;
        xmpp_stanza_to_text(presence, &buf, &buflen);
        LOG_VERBOSE("Presence Message: %s", buf);
        xmpp_free(xmppCtx, buf);

        // Send our presence message
        xmpp_send(conn, presence);
        xmpp_stanza_release(presence);
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
    printf("XmppConnectionHandler, state = %d, event = %d\n", prevConnState, event);
    switch(event)
    {
        case XMPP_CONN_CONNECT:
            {
                printf("XMPP_CONN_CONNECT\n");
                transport->GlobalConnectionStateChanged( connected, none );
                if (!transport->m_roster.empty())
                {
                    AddToRoster(transport);
                }

#if 0   //TODO: Remove this code

                    printf("Disconnecting\n");
                    xmpp_disconnect(conn);
#endif
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
#if 0
                static int attempt = 1;
                printf("Attempting to re-connect, %d\n", attempt);
                if (attempt++ >= 3)
                    break;
                if ( 0 != xmpp_connect_client(
                        conn, NULL, 0, transport->XmppConnectionHandler, transport) )
                {

                    // TODO: Translate the error from libstrophe to ours
                    //err = unknown;
                    LOG_RELEASE("Failed to connect to XMPP server.");
                    return;
                }
#endif
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

    void
XmppTransport::XmppConnectionHandler2(
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
     printf("XmppConnectionHandler2, state = %d\n", prevConnState);
     switch(event)
     {
         case XMPP_CONN_CONNECT:
             {
                 printf("XMPP_CONN_CONNECT2\n");
                 transport->GlobalConnectionStateChanged( connected, none );
                 if (!transport->m_roster.empty())
                 {
                     AddToRoster(transport);
                 }

                 break;
             }
         case XMPP_CONN_DISCONNECT:
             {
                 LOG_RELEASE("Disconnected from XMPP server2.");
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
                 LOG_RELEASE("XMPP error occurred2. Exiting.");

                 // Stop the XMPP event loop
                 transport->GlobalConnectionStateChanged( error, err );

                 break;
             }
     }

 }


