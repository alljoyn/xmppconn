#include "alljoyn/BusObject.h"
#include "alljoyn/BusAttachment.h"

class SimpleBusObject : public ajn::BusObject
{
    public:
        SimpleBusObject(ajn::BusAttachment& bus, const char* path);
};
