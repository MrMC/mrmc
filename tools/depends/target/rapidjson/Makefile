include ../../Makefile.include
DEPS= ../../Makefile.include Makefile 0001-remove_custom_cxx_flags.patch

# lib name, version
LIBNAME=rapidjson
VERSION=1.0.2
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	cd $(TARBALLS_LOCATION); $(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	cd $(PLATFORM); rm -rf build; mkdir -p build
	cd $(PLATFORM); patch -p1 < ../0001-remove_custom_cxx_flags.patch
	cd $(PLATFORM)/build; $(CMAKE) -DRAPIDJSON_HAS_STDSTRING=ON -DRAPIDJSON_BUILD_DOC=OFF -DRAPIDJSON_BUILD_EXAMPLES=OFF -DRAPIDJSON_BUILD_TESTS=OFF -DRAPIDJSON_BUILD_THIRDPARTY_GTEST=OFF VERBOSE=1 ..

.installed-$(PLATFORM): $(PLATFORM)
	make -C $(PLATFORM)/build
	make -C $(PLATFORM)/build install
	rm -rf $(PREFIX)/share/doc/RapidJSON
	touch $@

clean:
	rm -rf $(PLATFORM) .installed-$(PLATFORM)

distclean:
	rm -rf $(PLATFORM) .installed-$(PLATFORM)
