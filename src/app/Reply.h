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

#ifndef REPLY_H_
#define REPLY_H_

#include <string>
#include <pthread.h>

// Forward declaration for friend class status
class ScopedTransactionLocker;

// Holds the mutexes needed to perform AllJoyn conversations over XMPP
class Reply
{
    friend class ScopedTransactionLocker;

public:
    Reply();
    ~Reply();

    void
    SetReply(
        std::string const& replyStr
        );

private:
    bool        m_replyReceived;
    std::string m_replyStr;

    pthread_mutex_t m_transactionMutex;
    pthread_mutex_t m_replyReceivedMutex;
    pthread_cond_t m_replyReceivedWaitCond;
};

#endif // REPLYHANDLER_H_
