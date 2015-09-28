#include "SimpleBusObject.h"
#include "alljoyn/InterfaceDescription.h"
#include "common/xmppconnutil.h"
#include <string>


SimpleBusObject::SimpleBusObject(BusAttachment& bus, const char* path)
                                : BusObject(path)
{
    QStatus status;
    const InterfaceDescription* iface = bus.GetInterface(CHARIOT_XMPP_INTERFACE_NAME.c_str());
    assert(iface != NULL);

    status = AddInterface(*iface, ANNOUNCED);
    if (status != ER_OK) {
        LOG_RELEASE("Failed to add %s interface to the BusObject", CHARIOT_XMPP_INTERFACE_NAME.c_str());
    }
}

