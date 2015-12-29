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

#ifndef REMOTEABOUTDATALISTENER_H_
#define REMOTEABOUTDATALISTENER_H_

#include "alljoyn/AboutDataListener.h"

class RemoteAboutDataListener :
    public ajn::AboutDataListener
{
public:
    RemoteAboutDataListener()
    {}

    QStatus
    SetAnnounceArgs(
        ajn::AboutData aboutData
        )
    {
        aboutData.GetAboutData( &m_aboutArgs );                              //cout << m_aboutArgs.ToString(4) << endl;
        aboutData.GetAnnouncedAboutData( &m_announceArgs );                  //cout << m_announceArgs.ToString(4) << endl;

        return ER_OK;
    }

    QStatus
    GetAboutData(
        ajn::MsgArg*     all,
        const char* language
        )
    {
        all = &m_aboutArgs;                                                   //cout << "ReadAll called:" << all->ToString() << endl;
        return ER_OK;
    }

    QStatus
    GetAnnouncedAboutData(
        ajn::MsgArg*     all
        )
    {
        all = &m_announceArgs;                                               //cout << "ReadAll called:" << all->ToString() << endl;
        return ER_OK;
    }

private:
    ajn::MsgArg m_aboutArgs;
    ajn::MsgArg m_announceArgs;
};

#endif // REMOTEABOUTDATALISTENER_H_
