/******************************************************************************
* * 
*    Copyright (c) 2016 Open Connectivity Foundation and AllJoyn Open
*    Source Project Contributors and others.
*    
*    All rights reserved. This program and the accompanying materials are
*    made available under the terms of the Apache License, Version 2.0
*    which accompanies this distribution, and is available at
*    http://www.apache.org/licenses/LICENSE-2.0

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
