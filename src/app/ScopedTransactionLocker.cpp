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

#include "ScopedTransactionLocker.h"
#include <pthread.h>
#include <errno.h>
#include "Reply.h"

ScopedTransactionLocker::ScopedTransactionLocker(
    Reply* reply
    ) :
    m_reply(reply)
{
    pthread_mutex_lock(&m_reply->m_transactionMutex);
    pthread_mutex_lock(&m_reply->m_replyReceivedMutex);
}

ScopedTransactionLocker::~ScopedTransactionLocker()
{
    pthread_mutex_unlock(&m_reply->m_replyReceivedMutex);
    pthread_mutex_unlock(&m_reply->m_transactionMutex);
}

void
ScopedTransactionLocker::ReceiveReply(
    bool&        replyReceived,
    std::string& replyStr
    )
{
    timespec wait_time = {time(NULL)+10, 0};
    while(!m_reply->m_replyReceived)
    {
        if(ETIMEDOUT == pthread_cond_timedwait(
                &m_reply->m_replyReceivedWaitCond,
                &m_reply->m_replyReceivedMutex,
                &wait_time))
        {
            break;
        }
    }

    replyReceived = m_reply->m_replyReceived;
    replyStr = m_reply->m_replyStr;

    m_reply->m_replyReceived = false;
    m_reply->m_replyStr.clear();
}
