/******************************************************************************
*    Copyright (c) Open Connectivity Foundation (OCF), AllJoyn Open Source
*    Project (AJOSP) Contributors and others.
*    
*    SPDX-License-Identifier: Apache-2.0
*    
*    All rights reserved. This program and the accompanying materials are
*    made available under the terms of the Apache License, Version 2.0
*    which accompanies this distribution, and is available at
*    http://www.apache.org/licenses/LICENSE-2.0
*    
*    Copyright (c) Open Connectivity Foundation and Contributors to AllSeen
*    Alliance. All rights reserved.
*    
*    Permission to use, copy, modify, and/or distribute this software for
*    any purpose with or without fee is hereby granted, provided that the
*    above copyright notice and this permission notice appear in all
*    copies.
*    
*    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
*    WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
*    WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
*    AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
*    DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
*    PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
*    TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
*    PERFORMANCE OF THIS SOFTWARE.
******************************************************************************/

#ifndef SRPKEYLISTENER_H_
#define SRPKEYLISTENER_H_
#pragma once

#include <alljoyn/AuthListener.h>

class SrpKeyXListener : public ajn::AuthListener {
    public:
        SrpKeyXListener();
        virtual ~SrpKeyXListener();
        bool RequestCredentials(const char* authMechanism,
                                const char* authPeer,
                                uint16_t authCount,
                                const char* userId,
                                uint16_t credMask,
                                Credentials& creds);
        void setPassCode(qcc::String const& passCode);
        void setGetPassCode(void (*getPassCode)(qcc::String&));
        void AuthenticationComplete(const char* authMechanism, const char* authPeer, bool success);
    private:
        qcc::String m_PassCode;
        void (*m_GetPassCode)(qcc::String&);
};

#endif
