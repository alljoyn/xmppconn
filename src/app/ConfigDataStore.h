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
#ifndef CONFIG_DATA_STORE_H_
#define CONFIG_DATA_STORE_H_
#pragma once

#include <stdio.h>
#include <iostream>
#include <string>
#include <alljoyn/config/AboutDataStoreInterface.h>
#include <alljoyn/BusAttachment.h>

class ConfigDataStore : public AboutDataStoreInterface {
  public:
    typedef void (*RestartCallback)();
    std::string GetConfigFileName() const;

    ConfigDataStore(
        const char* factoryConfigFile,
        const char* configFile,
        const char* appId,
        const char* deviceId,
        RestartCallback func
        );
    virtual ~ConfigDataStore();

    void Initialize(bool reset = false);
    void FactoryReset();
    virtual QStatus ReadAll(const char* languageTag, DataPermission::Filter filter, ajn::MsgArg& all);
    virtual QStatus Update(const char* name, const char* languageTag, const ajn::MsgArg* value);
    virtual QStatus Delete(const char* name, const char* languageTag);
  private:
    std::string m_configFileName;
    std::string m_factoryConfigFileName;
    std::string m_appId;
    std::string m_deviceId;
    RestartCallback m_restartCallback;
    bool m_IsInitialized;
};
#endif
