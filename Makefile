
ALLJOYN_DIST := ../alljoyn-suite-14.06.00a-src/core/alljoyn/build/linux/x86/debug/dist/cpp

ALLJOYN_LIB := $(ALLJOYN_DIST)/lib/liballjoyn.a

CXXFLAGS = -Wall -pipe -std=c++98 -fno-rtti -fno-exceptions -Wno-long-long -Wno-deprecated -g -DQCC_OS_LINUX -DQCC_OS_GROUP_POSIX -DQCC_CPU_X86

LIBS = -lalljoyn -lstdc++ -lcrypto -lpthread -lrt -lstrophe -lalljoyn_about -lssl -lexpat -lresolv

ALLJOYN_INC := ../allseen

INCLUDES = -I$(ALLJOYN_DIST)/inc -I$(ALLJOYN_INC)/gateway/gwagent/cpp/GatewayConnector/inc -I$(ALLJOYN_INC)/gateway/gwagent/cpp/GatewayMgmtApp/inc -I../alljoyn-suite-14.06.00a-src/core/alljoyn/services/about/cpp/inc

.PHONY: default clean

default: XMPPConnector

clean:
	rm -f main.o XMPPConnector.o XMPPConnector

XMPPConnector: XMPPConnector.o main.o $(ALLJOYN_LIB) ../allseen/gateway/gwagent/build/linux/x86/debug/dist/gatewayConnector/lib/liballjoyn_gwConnector.a
	$(CXX) -o $@ XMPPConnector.o main.o -L$(ALLJOYN_DIST)/lib -L../allseen/gateway/gwagent/build/linux/x86/debug/dist/gatewayConnector/lib -L. $(LIBS) -lalljoyn_gwConnector
	
main.o: main.cpp $(ALLJOYN_LIB)
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) -o $@ main.cpp

XMPPConnector.o: XMPPConnector.cpp $(ALLJOYN_LIB)
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) -o $@ XMPPConnector.cpp

