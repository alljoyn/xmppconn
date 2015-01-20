
CXX := g++
CC  := gcc

CXXFLAGS = -Wall -pipe -std=c++98 -fno-rtti -fno-exceptions -Wno-long-long -Wno-deprecated -g -DQCC_OS_LINUX -DQCC_OS_GROUP_POSIX -DQCC_CPU_X86
LDFLAGS  =
LIBS     =
INCLUDES =
DEFINES  =

WITHGATEWAY:=true
ifeq ($(WITHGATEWAY),true)
  INCLUDES += -I$(AJ_PATH)/gatewayConnector/inc -I$(AJ_PATH)/gatewayMgmtApp/inc
  LDFLAGS += -L$(AJ_PATH)/gatewayConnector/lib -L$(AJ_PATH)/gatewayMgmtApp/lib
  LIBS += -lalljoyn_gwConnector
else
  CXXFLAGS += -DNO_AJ_GATEWAY
endif

LIBS += -lalljoyn_notification -lalljoyn_about -lalljoyn_services_common -lalljoyn -lstrophe -lexpat -lssl -lresolv -lstdc++ -lz -lcrypto -lpthread -lrt

ifdef AJ_PATH
  INCLUDES += -I$(AJ_PATH)/cpp/inc -I$(AJ_PATH)/about/inc -I$(AJ_PATH)/notification/inc -I$(AJ_PATH)/services_common/inc
  LDFLAGS += -L$(AJ_PATH)/cpp/lib -L$(AJ_PATH)/about/lib -L$(AJ_PATH)/notification/lib -L$(AJ_PATH)/services_common/lib
else
#  $(error AJ_PATH is undefined. Please set this to the base path that contains the AllJoyn cpp, notification, services_common, and other folders.)
endif

ifdef STROPHE_PATH
  INCLUDES += -I$(STROPHE_PATH)
  LDFLAGS += -L$(STROPHE_PATH)/.libs
else
#  $(error STROPHE_PATH is undefined. Please set this to the base path of your libstrophe source distribution)
endif

.PHONY: default clean

default: XMPPConnector

clean:
	rm -f main.o XMPPConnector.o XMPPConnector

all: XMPPConnector

XMPPConnector: XMPPConnector.o main.o
	$(CXX) -o $@ XMPPConnector.o main.o $(LDFLAGS) $(LIBS)
	
main.o: main.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) -o $@ main.cpp

XMPPConnector.o: XMPPConnector.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) -o $@ XMPPConnector.cpp

