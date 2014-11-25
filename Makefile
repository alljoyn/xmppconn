
ifeq ($(MAKECMDGOALS),ipq)
	QSDK_BASE:=../qsdk/qsdk
	QSDK_STAGING:=$(QSDK_BASE)/staging_dir
	QSDK_BUILD_DIR:=$(QSDK_BASE)/build_dir
	TOOLCHAIN_DIR:=$(QSDK_STAGING)/toolchain-arm_v7-a_gcc-4.6-linaro_uClibc-0.9.33.2_eabi
	TARGET_DIR:=$(QSDK_STAGING)/target-arm_v7-a_uClibc-0.9.33.2_eabi
	TARGET_BUILD_DIR:=$(QSDK_BUILD_DIR)/target-arm_v7-a_uClibc-0.9.33.2_eabi
	TOOLCHAIN_BUILD_DIR:=$(QSDK_BUILD_DIR)/toolchain-arm_v7-a_gcc-4.6-linaro_uClibc-0.9.33.2_eabi
	#CROSS_COMPILE:=$(TOOLCHAIN_DIR)/bin/arm-openwrt-linux-uclibcgnueabi-
	CROSS_COMPILE:=$(TOOLCHAIN_DIR)/bin/arm-openwrt-linux-
	CROSS_INCLUDE:=-I$(TARGET_BUILD_DIR)/libstrophe-0.8.7
	CROSS_LIBPATH:=-L$(TARGET_BUILD_DIR)/lib -L$(TARGET_BUILD_DIR)/usr/lib -L$(TOOLCHAIN_BUILD_DIR)/uClibc-0.9.33.2/lib -L$(TOOLCHAIN_DIR)/lib -L$(TARGET_BUILD_DIR)/root-ipq806x/lib -L$(TARGET_BUILD_DIR)/root-ipq806x/usr/lib -L$(TARGET_DIR)/root-ipq806x/lib -L$(TARGET_DIR)/usr/lib
endif
CXX := $(CROSS_COMPILE)g++
CC  := $(CROSS_COMPILE)gcc

#AJ_BASE_PATH := ../alljoyn-suite-14.06.00a-src/core/alljoyn
AJ_BASE_PATH := ../allseen
#AJ_MID_PATH := services/base/sample_apps/build
AJ_MID_PATH := gateway/gwagent/build
AJ_PATH := $(AJ_BASE_PATH)/$(AJ_MID_PATH)/linux/x86/debug/dist

#WITHGATEWAY:=true
ifeq ($(WITHGATEWAY),true)
	GW_LIB:=-lalljoyn_gwConnector
	GW_INC:=-I$(AJ_PATH)/gatewayConnector/inc -I$(AJ_PATH)/gatewayMgmtApp/inc
else
	DEFINES:=-DNO_AJ_GATEWAY
endif

CXXFLAGS = -Wall -pipe -std=c++98 -fno-rtti -fno-exceptions -Wno-long-long -Wno-deprecated -g -DQCC_OS_LINUX -DQCC_OS_GROUP_POSIX -DQCC_CPU_X86 $(DEFINES)
LDFLAGS = $(CROSS_LIBPATH)

LIBS = $(GW_LIB) -lalljoyn_notification -lalljoyn_about -lalljoyn_services_common -lalljoyn -lstrophe -lexpat -lssl -lresolv -lstdc++ -lcrypto -lpthread -lrt

INCLUDES = -I$(AJ_PATH)/cpp/inc -I$(AJ_PATH)/notification/inc -I$(AJ_PATH)/services_common/inc $(GW_INC) $(CROSS_INCLUDE)

.PHONY: default clean

default: XMPPConnector

clean:
	rm -f main.o XMPPConnector.o XMPPConnector

ipq: XMPPConnector

all: XMPPConnector

XMPPConnector: XMPPConnector.o main.o
	$(CXX) -o $@ XMPPConnector.o main.o -L. $(LDFLAGS) $(LIBS)
	
main.o: main.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) -o $@ main.cpp

XMPPConnector.o: XMPPConnector.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) -o $@ XMPPConnector.cpp

