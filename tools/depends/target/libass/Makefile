include ../../Makefile.include
DEPS= ../../Makefile.include Makefile

# lib name, version
LIBNAME=libass
VERSION=0.12.1
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz

ifeq ($(OS),ios)
# silly configure, if we pass this down from the main LDFLAGS, we get bcsymbolmaps 
# for all the configure tests while we only care about the one from the actual lib.
BCSYMMAPBITS=-Wl,-bitcode_verify -Wl,-bitcode_hide_symbols -Wl,-bitcode_symbol_map,$(PREFIX)/bcsymbolmaps/$(LIBNAME)
endif

# configuration settings
CONFIGURE=cp -f $(CONFIG_SUB) $(CONFIG_GUESS) .; \
          ./configure --prefix=$(PREFIX) --disable-enca \
	  LDFLAGS="$(BCSYMMAPBITS)"

LIBDYLIB=$(PLATFORM)/$(LIBNAME)/.libs/$(LIBNAME).a

CLEAN_FILES=$(ARCHIVE) $(PLATFORM)

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	cd $(TARBALLS_LOCATION); $(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM)/*; mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	cd $(PLATFORM); $(CONFIGURE)

$(LIBDYLIB): $(PLATFORM)
ifeq ($(OS),ios)
	rm -rf $(PREFIX)/bcsymbolmaps/$(LIBNAME); mkdir -p $(PREFIX)/bcsymbolmaps/$(LIBNAME)
endif
	$(MAKE) -C $(PLATFORM)

.installed-$(PLATFORM): $(LIBDYLIB)
	$(MAKE) -C $(PLATFORM) install
ifeq ($(OS),android)
	rm -f $(PREFIX)/lib/libass.la $(PREFIX)/lib/libass.so $(PREFIX)/lib/libass.so.5
	mv -f $(PREFIX)/lib/libass.so.5.1.0 $(PREFIX)/lib/libass.so
	$(RPL) -e "libass.so.5" "libass.so\x00\x00" $(PREFIX)/lib/libass.so
	-$(READELF) --dynamic $(PREFIX)/lib/libass.so | grep ibrary
endif
	touch $@

clean:
	$(MAKE) -C $(PLATFORM) clean
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PREFIX)/bcsymbolmaps/$(LIBNAME)
	rm -rf $(PLATFORM) .installed-$(PLATFORM)

