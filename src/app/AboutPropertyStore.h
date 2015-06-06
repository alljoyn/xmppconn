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

#ifndef ABOUTPROPERTYSTORE_H_
#define ABOUTPROPERTYSTORE_H_

#include "alljoyn/about/PropertyStore.h"
#include "alljoyn/about/AnnounceHandler.h"

class AboutPropertyStore :
    public ajn::services::PropertyStore
{
public:
    AboutPropertyStore()
    {}

    QStatus
    SetAnnounceArgs(
        const ajn::services::AnnounceHandler::AboutData& aboutData
        )
    {
        // Construct the property store args
        std::vector<ajn::MsgArg> dictArgs;
        ajn::services::AnnounceHandler::AboutData::const_iterator aboutIter;
        for(aboutIter = aboutData.begin();
            aboutIter != aboutData.end();
            ++aboutIter)
        {
            ajn::MsgArg dictEntry("{sv}",
                    aboutIter->first.c_str(), &aboutIter->second);
            dictEntry.Stabilize();                                          //cout << dictEntry.ToString(4) << endl;

            dictArgs.push_back(dictEntry);
        }

        QStatus err = m_announceArgs.Set("a{sv}",
                dictArgs.size(), &dictArgs[0]);
        m_announceArgs.Stabilize();                                         //cout << m_announceArgs.ToString(4) << endl;

        return err;
    }

    QStatus
    ReadAll(
        const char* languageTag,
        Filter      filter,
        ajn::MsgArg&     all
        )
    {
        all = m_announceArgs;                                               //cout << "ReadAll called:" << all.ToString() << endl;
        return ER_OK;
    }

private:
    ajn::MsgArg m_announceArgs;
};

#endif // ABOUTPROPERTYSTORE_H_
