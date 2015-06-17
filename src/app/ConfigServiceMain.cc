/******************************************************************************
 * Copyright (c) 2013 - 2014, AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#include <stdio.h>
#include <signal.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/about/AboutIconService.h>
#include <alljoyn/about/AboutServiceApi.h>
#include <alljoyn/config/ConfigService.h>

#include <SrpKeyXListener.h>
#include <CommonBusListener.h>
#include <CommonSampleUtil.h>
#include <IniParser.h>

#include "PropertyStoreImpl.h"
#include "ConfigServiceListenerImpl.h"
#include "OptParser.h"
#include <alljoyn/services_common/LogModulesNames.h>

#define DEFAULTPASSCODE "000000"
#define SERVICE_EXIT_OK       0
#define SERVICE_OPTION_ERROR  1
#define SERVICE_CONFIG_ERROR  2

using namespace ajn;
using namespace services;

/** static variables need for sample */
static BusAttachment* msgBus = NULL;
static SrpKeyXListener* keyListener = NULL;
static ConfigService* configService = NULL;
static AboutIconService* aboutIconService = NULL;
static PropertyStoreImpl* propertyStore = NULL;
static ConfigServiceListenerImpl* configServiceListener = NULL;
static CommonBusListener* busListener = NULL;
static SessionPort SERVICE_PORT;
static qcc::String configFile;
static volatile sig_atomic_t s_interrupt = false;
static void SigIntHandler(int sig) {
    s_interrupt = true;
}

static void cleanup() {
    if (AboutServiceApi::getInstance()) {
        AboutServiceApi::DestroyInstance();
    }
    if (configService) {
        delete configService;
        configService = NULL;
    }
    if (configServiceListener) {
        delete configServiceListener;
        configServiceListener = NULL;
    }

    if (keyListener) {
        delete keyListener;
        keyListener = NULL;
    }

    if (busListener) {
        msgBus->UnregisterBusListener(*busListener);
        delete busListener;
        busListener = NULL;
    }

    if (aboutIconService) {
        delete aboutIconService;
        aboutIconService = NULL;
    }

    if (propertyStore) {
        delete propertyStore;
        propertyStore = NULL;
    }

    /* Clean up msg bus */
    delete msgBus;
    msgBus = NULL;

}

const char* readPassword() {
    std::map<std::string, std::string> data;
    if (!IniParser::ParseFile(configFile.c_str(), data)) {
        return NULL;
    }

    std::map<std::string, std::string>::iterator iter = data.find("passcode");
    if (iter == data.end()) {
        return NULL;
    }

    return iter->second.c_str();
}

/** Advertise the service name, report the result to stdout, and return the status code. */
QStatus AdvertiseName(TransportMask mask) {
    QStatus status = ER_BUS_ESTABLISH_FAILED;
    if (msgBus->IsConnected() && msgBus->GetUniqueName().size() > 0) {
        status = msgBus->AdvertiseName(msgBus->GetUniqueName().c_str(), mask);
        std::cout << "AdvertiseName " << msgBus->GetUniqueName().c_str() << " =" << QCC_StatusText(status) << std::endl;
    }
    return status;
}

void WaitForSigInt(void) {
    while (s_interrupt == false) {
#ifdef _WIN32
        Sleep(100);
#else
        usleep(100 * 1000);
#endif
    }
}

int main(int argc, char**argv, char**envArg) {
    QCC_SetDebugLevel(logModules::CONFIG_MODULE_LOG_NAME, logModules::ALL_LOG_LEVELS);

    SERVICE_PORT = opts.GetPort();
    std::cout << "using port " << opts.GetPort() << std::endl;

    if (!opts.GetConfigFile().empty()) {
        configFile = opts.GetConfigFile().c_str();
        std::cout << "using Config-file " << configFile.c_str() << std::endl;
        if (!opts.ParseExternalXML()) {
            return 1;
        }
    }

    if (!opts.GetAppId().empty()) {
        std::cout << "using appID " << opts.GetAppId().c_str() << std::endl;
    }

    /* Install SIGINT handler so Ctrl + C deallocates memory properly */
    signal(SIGINT, SigIntHandler);

    //set Daemon password only for bundled app
    #ifdef QCC_USING_BD
    PasswordManager::SetCredentials("ALLJOYN_PIN_KEYX", "000000");
    #endif

    /* Create message bus */
    keyListener = new SrpKeyXListener();
    keyListener->setPassCode(DEFAULTPASSCODE);
    keyListener->setGetPassCode(readPassword);

    msgBus = CommonSampleUtil::prepareBusAttachment(keyListener);
    if (msgBus == NULL) {
        std::cout << "Could not initialize BusAttachment." << std::endl;
        delete keyListener;
        return 1;
    }

    busListener = new CommonBusListener(msgBus);
    busListener->setSessionPort(SERVICE_PORT);

    propertyStore = new PropertyStoreImpl(opts.GetFactoryConfigFile().c_str(), opts.GetConfigFile().c_str());
    status = CommonSampleUtil::fillPropertyStore(propertyStore, opts.GetAppId(), opts.GetAppName(), opts.GetDeviceId(),
                                                 opts.GetDeviceName(), opts.GetDefaultLanguage());
    propertyStore->Initialize();
    if (status != ER_OK) {
        std::cout << "Could not fill PropertyStore." << std::endl;
        cleanup();
        return 1;
    }

    status = CommonSampleUtil::prepareAboutService(msgBus, propertyStore, busListener, SERVICE_PORT);
    if (status != ER_OK) {
        std::cout << "Could not set up the AboutService." << std::endl;
        cleanup();
        return 1;
    }

    AboutService* aboutService = AboutServiceApi::getInstance();
    if (!aboutService) {
        std::cout << "Could not set up the AboutService." << std::endl;
        cleanup();
        return 1;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    //ConfigService

    configServiceListener = new ConfigService(*propertyStore, *msgBus, *busListener);
    configService = new ConfigService(*msgBus, *propertyStore, *configServiceListener);

    interfaces.clear();
    interfaces.push_back("org.alljoyn.Config");
    aboutService->AddObjectDescription("/Config", interfaces);

    status = configService->Register();
    if (status != ER_OK) {
        std::cout << "Could not register the ConfigService." << std::endl;
        cleanup();
        return 1;
    }

    status = msgBus->RegisterBusObject(*configService);
    if (status != ER_OK) {
        std::cout << "Could not register the ConfigService BusObject." << std::endl;
        cleanup();
        return 1;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////

    const TransportMask SERVICE_TRANSPORT_TYPE = TRANSPORT_ANY;

    if (ER_OK == status) {
        status = AdvertiseName(SERVICE_TRANSPORT_TYPE);
    }

    if (ER_OK == status) {
        status = aboutService->Announce();
    }

    /* Perform the service asynchronously until the user signals for an exit. */
    if (ER_OK == status) {
        WaitForSigInt();
    }

    cleanup();

    return 0;
} /* main() */
