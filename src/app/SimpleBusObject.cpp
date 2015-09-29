#include "SimpleBusObject.h"
#include "alljoyn/InterfaceDescription.h"
#include "common/xmppconnutil.h"
#include <string>


SimpleBusObject::SimpleBusObject(BusAttachment& bus, const char* path)
                                : BusObject(path)
{
    QStatus status;
    const InterfaceDescription* iface = bus.GetInterface(ALLJOYN_XMPP_CONFIG_INTERFACE_NAME.c_str());
    if (!iface)
    {
        throw BusAttachmentInterfaceException();
    }

    status = AddInterface(*iface, ANNOUNCED);
    if (status != ER_OK) {
        LOG_RELEASE("Failed to add %s interface to the BusObject", ALLJOYN_XMPP_CONFIG_INTERFACE_NAME.c_str());
    }
}

