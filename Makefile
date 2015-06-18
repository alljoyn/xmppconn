CC        := g++
LD        := g++
CFLAGS    := -DQCC_OS_GROUP_POSIX -DQCC_OS_LINUX

TARGET    := xmppconn

MODULES   := common app transport
SRC_DIR   := $(addprefix src/,$(MODULES))
BUILD_ROOT:= build/

SRC       := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.cpp))
OBJ       := $(patsubst src/%.cpp,build/%.o,$(SRC))

BUILD_DIR := $(addprefix $(BUILD_ROOT),$(MODULES))

INCLUDES  := $(addprefix -I,$(SRC_DIR)) -Isrc

vpath %.cpp $(SRC_DIR)

define make-goal
$1/%.o: %.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $$< -o $$@
endef

LIBS += -lalljoyn_gwconnector -lalljoyn_config -lalljoyn_notification -lalljoyn_about -lalljoyn_services_common -lalljoyn -lstrophe -lxml2 -lssl -lresolv -lstdc++ -lpthread -lrt -lz

ifdef ALLJOYN_DISTDIR
  INCLUDES += -I$(ALLJOYN_DISTDIR)/cpp/inc -I$(ALLJOYN_DISTDIR)/about/inc -I$(ALLJOYN_DISTDIR)/notification/inc -I$(ALLJOYN_DISTDIR)/services_common/inc -I$(ALLJOYN_DISTDIR)/gatewayConnector/inc -I$(ALLJOYN_DISTDIR)/gatewayMgmtApp/inc -I$(ALLJOYN_DISTDIR)/config/inc
  LDFLAGS += -L$(ALLJOYN_DISTDIR)/cpp/lib -L$(ALLJOYN_DISTDIR)/about/lib -L$(ALLJOYN_DISTDIR)/notification/lib -L$(ALLJOYN_DISTDIR)/services_common/lib -L$(ALLJOYN_DISTDIR)/gatewayConnector/lib -L$(ALLJOYN_DISTDIR)/gatewayMgmtApp/lib -L$(ALLJOYN_DISTDIR)/config/lib
else
  $(error ALLJOYN_DISTDIR is not set. Please see README.md for more information)
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
