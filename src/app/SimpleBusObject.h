#include "alljoyn/BusObject.h"
#include "alljoyn/BusAttachment.h"
#include <stdexcept>

class BusAttachmentInterfaceException : public std::runtime_error
{
    public:
        BusAttachmentInterfaceException()
        : runtime_error( "Invalid interface for the BusAttachment object" )
        {}
};

class SimpleBusObject : public ajn::BusObject
{
    public:
        SimpleBusObject(ajn::BusAttachment& bus, const char* path);
};
