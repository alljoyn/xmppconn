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

#ifndef SCOPEDTRANSACTIONLOCKER_H_
#define SCOPEDTRANSACTIONLOCKER_H_

#include <string>

class Reply;

// Facilitates AllJoyn transactions over XMPP via scoped mutex locking
class ScopedTransactionLocker
{
public:
    ScopedTransactionLocker(
        Reply* reply
        );
    ~ScopedTransactionLocker();

    void
    ReceiveReply(
        bool&        replyReceived,
        std::string& replyStr
        );

private:
    Reply* m_reply;
};

#endif // SCOPEDTRANSACTIONLOCKER_H_
