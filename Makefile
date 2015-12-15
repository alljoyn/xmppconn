ifeq	($(CPU),arm)
    CC	:= $(WORKING)/build/bin/arm-linux-gnueabihf-g++
    LD	:= $(WORKING)/build/bin/arm-linux-gnueabihf-g++
else
    CC	:= g++
    LD	:= g++
endif

CFLAGS    := -std=c++98 -DQCC_OS_GROUP_POSIX -DQCC_OS_LINUX -g3 -g
ifeq ($(NO_AJ_GATEWAY), 1)
	override CFLAGS += -DNO_AJ_GATEWAY
endif
#CFLAGS    := -Wall -pipe -std=c++98 -fno-rtti -fno-exceptions -Wno-long-long -Wno-deprecated -DDEBUG -g3 -g -DQCC_OS_LINUX -DQCC_OS_GROUP_POSIX
#CXXFLAGS  := -Wall -pipe -std=c++98 -fno-rtti -fno-exceptions -Wno-long-long -Wno-deprecated -DDEBUG -g3 -g -DQCC_OS_LINUX -DQCC_OS_GROUP_POSIX

TARGET    := xmppconn

MODULES   := common app transport
SRC_DIR   := $(addprefix src/,$(MODULES))
BUILD_ROOT:= build/

SRC       := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.cpp))
OBJ       := $(patsubst src/%.cpp,build/%.o,$(SRC))

BUILD_DIR := $(addprefix $(BUILD_ROOT),$(MODULES))

INCLUDES  := $(addprefix -I,$(SRC_DIR)) -Isrc -I/usr/include/libxml2

vpath %.cpp $(SRC_DIR)

define make-goal
$1/%.o: %.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $$< -o $$@
endef

LIBS += -lalljoyn_gwconnector -lalljoyn_config -lalljoyn_notification -lalljoyn_about -lalljoyn_services_common -lalljoyn -lstrophe -lxml2 -lssl -lcrypto -lresolv -lstdc++ -lpthread -lrt -lz -lgcc_s -lc -luuid

ifeq	($(CPU),arm)
    LIBS += -llzma
endif

ifdef RAPIDJSON_PATH
    INCLUDES += -I$(RAPIDJSON_PATH)
endif

ifdef ALLJOYN_DISTDIR
  INCLUDES += -I$(ALLJOYN_DISTDIR)/cpp/inc -I$(ALLJOYN_DISTDIR)/about/inc -I$(ALLJOYN_DISTDIR)/notification/inc -I$(ALLJOYN_DISTDIR)/services_common/inc -I$(ALLJOYN_DISTDIR)/gatewayConnector/inc -I$(ALLJOYN_DISTDIR)/gatewayMgmtApp/inc -I$(ALLJOYN_DISTDIR)/config/inc
  LDFLAGS += -L$(ALLJOYN_DISTDIR)/cpp/lib -L$(ALLJOYN_DISTDIR)/about/lib -L$(ALLJOYN_DISTDIR)/notification/lib -L$(ALLJOYN_DISTDIR)/services_common/lib -L$(ALLJOYN_DISTDIR)/gatewayConnector/lib -L$(ALLJOYN_DISTDIR)/gatewayMgmtApp/lib -L$(ALLJOYN_DISTDIR)/config/lib
endif

.PHONY: all checkdirs clean

all: checkdirs $(BUILD_ROOT) $(BUILD_ROOT)$(TARGET)

$(BUILD_ROOT)$(TARGET): $(OBJ)
	$(LD) $^ $(LDFLAGS) -L$(BUILD_ROOT) $(LIBS) -o $@


checkdirs: $(BUILD_DIR)

$(BUILD_DIR):
	@mkdir -p $@

clean:
	@rm -rf $(BUILD_DIR) $(BUILD_ROOT)/*.so $(BUILD_ROOT)/clconnector

$(foreach bdir,$(BUILD_DIR),$(eval $(call make-goal,$(bdir))))
