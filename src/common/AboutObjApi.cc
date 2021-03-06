#include <alljoyn/AboutObj.h>
#include <alljoyn/AboutData.h>
#include "AboutObjApi.h"

using namespace ajn;
using namespace services;

AboutObjApi* AboutObjApi::m_instance = NULL;
BusAttachment* AboutObjApi::m_BusAttachment = NULL;
AboutData* AboutObjApi::m_AboutData = NULL;
AboutObj* AboutObjApi::m_AboutObj = NULL;
SessionPort AboutObjApi::m_sessionPort = 0;

AboutObjApi::AboutObjApi() {
}

AboutObjApi::~AboutObjApi() {
}

AboutObjApi* AboutObjApi::getInstance() {

    if (!m_instance) {
        m_instance = new AboutObjApi();
    }
    return m_instance;
}

void AboutObjApi::Init(ajn::BusAttachment* bus, AboutData* aboutData, AboutObj* aboutObj) {
    m_BusAttachment = bus;
    m_AboutData = aboutData;
    m_AboutObj = aboutObj;
}

void AboutObjApi::DestroyInstance() {
    if (m_instance) {
        delete m_instance;
        m_instance = NULL;
    }
}

void AboutObjApi::SetPort(SessionPort sessionPort) {
    m_sessionPort = sessionPort;
}

QStatus AboutObjApi::Announce() {
    if (m_AboutObj == NULL) {
        return ER_FAIL;
    }
    return m_AboutObj->Announce(m_sessionPort, *m_AboutData);
}
