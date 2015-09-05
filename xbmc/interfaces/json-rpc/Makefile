SRCS  = AddonsOperations.cpp
SRCS += ApplicationOperations.cpp
SRCS += AudioLibrary.cpp
SRCS += FavouritesOperations.cpp
SRCS += FileItemHandler.cpp
SRCS += FileOperations.cpp
SRCS += GUIOperations.cpp
SRCS += InputOperations.cpp
SRCS += JSONRPC.cpp
SRCS += JSONServiceDescription.cpp
SRCS += PlayerOperations.cpp
SRCS += PlaylistOperations.cpp
SRCS += ProfilesOperations.cpp
SRCS += PVROperations.cpp
SRCS += SettingsOperations.cpp
SRCS += SystemOperations.cpp
SRCS += TextureOperations.cpp
SRCS += VideoLibrary.cpp
SRCS += XBMCOperations.cpp

LIB   = json-rpc.a

GENERATED_JSON = ServiceDescription.h
GENERATED_ADDON_JSON = ../../../addons/xbmc.json/addon.xml
JSON_VERSION_SRC = schema/version.txt
JSON_SRC = $(JSON_VERSION_SRC) schema/license.txt schema/methods.json schema/types.json schema/notifications.json

all: $(GENERATED_JSON) $(GENERATED_ADDON_JSON) $(LIB)

-include ../../../Makefile.include
ifeq ($(wildcard $(JSON_BUILDER)),)
  JSON_BUILDER = $(shell which JsonSchemaBuilder)
ifeq ($(JSON_BUILDER),)
  JSON_BUILDER = ../../../tools/depends/native/JsonSchemaBuilder/bin/JsonSchemaBuilder
endif
endif

$(GENERATED_JSON): $(JSON_SRC)
	$(JSON_BUILDER) $(JSON_SRC)

.PHONY: $(notdir $(GENERATED_ADDON_JSON))
$(notdir $(GENERATED_ADDON_JSON)): $(GENERATED_ADDON_JSON)

$(GENERATED_ADDON_JSON): $(JSON_VERSION_SRC)
	sed s/@jsonrpc_version@/`cat $(JSON_VERSION_SRC)`/g $@.in > $@

-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
