
#ALLJOYN_DIST := ../alljoyn-suite-14.06.00a-src/core/alljoyn/build/linux/x86/debug/dist/cpp
AJ_BASE_PATH := /home/kevin/dev/allseen
ALLJOYN_DIST := $(AJ_BASE_PATH)/gateway/gwagent/build/linux/x86/debug/dist

CXXFLAGS = -Wall -pipe -std=c++98 -fno-rtti -fno-exceptions -Wno-long-long -Wno-deprecated -g -DQCC_OS_LINUX -DQCC_OS_GROUP_POSIX -DQCC_CPU_X86

LIBS = -lalljoyn_gwConnector -lalljoyn_notification -lalljoyn_about -lalljoyn_services_common -lalljoyn -lstrophe -lexpat -lssl -lresolv -lstdc++ -lcrypto -lpthread -lrt

INCLUDES = -I$(ALLJOYN_DIST)/cpp/inc -I$(ALLJOYN_DIST)/gatewayConnector/inc -I$(ALLJOYN_DIST)/gatewayMgmtApp/inc -I$(ALLJOYN_DIST)/notification/inc -I$(ALLJOYN_DIST)/services_common/inc

.PHONY: default clean

default: XMPPConnector

clean:
	rm -f main.o XMPPConnector.o XMPPConnector

all: XMPPConnector

XMPPConnector: XMPPConnector.o main.o
	$(CXX) -o $@ XMPPConnector.o main.o -L. $(LIBS)
	
main.o: main.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) -o $@ main.cpp

XMPPConnector.o: XMPPConnector.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) -o $@ XMPPConnector.cpp

