
ifeq ($(MAKECMDGOALS),ipq)
	CROSS_COMPILE=../qsdk/qsdk/staging_dir/toolchain-arm_v7-a_gcc-4.6-linaro_uClibc-0.9.33.2_eabi/bin/arm-openwrt-linux-
endif
CXX := $(CROSS_COMPILE)g++
CC  := $(CROSS_COMPILE)gcc

#AJ_BASE_PATH := ../alljoyn-suite-14.06.00a-src/core/alljoyn
#AJ_MID_PATH := gateway/gwagent/build
AJ_BASE_PATH := ../allseen
AJ_MID_PATH := services/base/sample_apps/build
AJ_PATH := $(AJ_BASE_PATH)/$(AJ_MID_PATH)/linux/x86/debug/dist

#WITHGATEWAY:=true
ifeq ($(WITHGATEWAY),true)
	GW_LIB:=-lalljoyn_gwConnector
	GW_INC:=-I$(AJ_PATH)/gatewayConnector/inc
else
	DEFINES:=-DNO_AJ_GATEWAY
endif

CXXFLAGS = -Wall -pipe -std=c++98 -fno-rtti -fno-exceptions -Wno-long-long -Wno-deprecated -g -DQCC_OS_LINUX -DQCC_OS_GROUP_POSIX -DQCC_CPU_X86 $(DEFINES)

LIBS = $(GW_LIB) -lalljoyn_notification -lalljoyn_about -lalljoyn_services_common -lalljoyn -lstrophe -lexpat -lssl -lresolv -lstdc++ -lcrypto -lpthread -lrt

INCLUDES = -I$(AJ_PATH)/cpp/inc -I$(AJ_PATH)/gatewayMgmtApp/inc -I$(AJ_PATH)/notification/inc -I$(AJ_PATH)/services_common/inc $(GW_INC)

.PHONY: default clean

default: XMPPConnector

clean:
	rm -f main.o XMPPConnector.o XMPPConnector

ipq: XMPPConnector

all: XMPPConnector

XMPPConnector: XMPPConnector.o main.o
	$(CXX) -o $@ XMPPConnector.o main.o -L. $(LIBS)
	
main.o: main.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) -o $@ main.cpp

XMPPConnector.o: XMPPConnector.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) -o $@ XMPPConnector.cpp

