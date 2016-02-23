TOPDIR ?= .
INTERFACES_DIR ?= xbmc/interfaces

GENERATED_JSON = $(INTERFACES_DIR)/json-rpc/ServiceDescription.h addons/xbmc.json/addon.xml
ifeq ($(wildcard $(JSON_BUILDER)),)
  JSON_BUILDER = $(shell which JsonSchemaBuilder)
ifeq ($(JSON_BUILDER),)
  JSON_BUILDER = tools/depends/native/JsonSchemaBuilder/bin/JsonSchemaBuilder
endif
endif

codegenerated: $(GENERATED_JSON) $(GENERATED_ADDON_JSON)

$(GENERATED_JSON): $(JSON_BUILDER)
	@echo Jsonbuilder: $(JSON_BUILDER)
	$(MAKE) -C $(INTERFACES_DIR)/json-rpc $(notdir $@)

$(JSON_BUILDER):
ifeq ($(BOOTSTRAP_FROM_DEPENDS), yes)
	@echo JsonSchemaBuilder not found. You didn\'t build depends. Check docs/README.\<yourplatform\>
	@false
else
#build json builder - ".." because makefile is in the parent dir of "bin"
	$(MAKE) -C $(abspath $(dir $@)..)
endif
