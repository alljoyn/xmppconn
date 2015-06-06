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

#include "AboutBusObject.h"
#include "common/xmppconnutil.h"
#include "RemoteBusAttachment.h"

using namespace ajn::services;

AboutBusObject::AboutBusObject(
    RemoteBusAttachment* bus,
    AboutPropertyStore& propertyStore
    ) :
    AboutService(*bus, propertyStore)
{
}

QStatus
AboutBusObject::AddObjectDescriptions(
    const AnnounceHandler::ObjectDescriptions& objectDescs
    )
{
    QStatus err = ER_OK;

    AnnounceHandler::ObjectDescriptions::const_iterator it;
    for(it = objectDescs.begin(); it != objectDescs.end(); ++it)
    {
        if(it->first == "/About")
        {
            continue;
        }

        err = AddObjectDescription(it->first, it->second);
        if(err != ER_OK)
        {
            LOG_RELEASE("Failed to add object description for %s: %s",
                    it->first.c_str(), QCC_StatusText(err));
            break;
        }
    }

    return err;
}
