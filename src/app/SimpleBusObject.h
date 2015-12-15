#include "alljoyn/BusObject.h"
#include "alljoyn/BusAttachment.h"
#include <stdexcept>

class SimpleBusObject : public ajn::BusObject
{
    public:
        SimpleBusObject(ajn::BusAttachment& bus, const char* path);
};
