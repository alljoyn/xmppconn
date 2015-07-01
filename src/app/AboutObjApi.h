#ifndef ABOUT_DATA_API_H
#define ABOUT_DATA_API_H

#include <qcc/Log.h>
#include <alljoyn/version.h>
#include <alljoyn/AboutObj.h>
#include <alljoyn/AboutData.h>

#if defined(QCC_OS_GROUP_WINDOWS)
#pragma warning(push)
#pragma warning(disable: 4100)
#endif

namespace ajn {
    namespace services {
        class AboutObjApi  {
            public:
                static AboutObjApi* getInstance();
                static void Init(ajn::BusAttachment* bus, AboutData* aboutData, AboutObj* aboutObj);
                static void DestroyInstance();
                void SetPort(SessionPort sessionPort);
                QStatus Announce();

            private:
                AboutObjApi();
                virtual ~AboutObjApi();
                static AboutObjApi* m_instance;
                static BusAttachment* m_BusAttachment;
                static AboutData* m_AboutData;
                static AboutObj* m_AboutObj;
                static SessionPort m_sessionPort;
        };

    }
}
#endif
