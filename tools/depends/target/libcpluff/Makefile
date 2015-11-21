include ../../Makefile.include
DEPS= ../../Makefile.include Makefile

# lib name, version
LIBNAME=cpluff
VERSION=3.0.1
SOURCE=$(LIBNAME)-$(VERSION)

ifeq ($(OS),ios)
# silly configure, if we pass this down from the main LDFLAGS, we get bcsymbolmaps 
# for all the configure tests while we only care about the one from the actual lib.
BCSYMMAPBITS=-Wl,-bitcode_verify -Wl,-bitcode_hide_symbols -Wl,-bitcode_symbol_map,$(PREFIX)/bcsymbolmaps/lib$(LIBNAME)
endif

# configuration settings
CONFIGURE=cp -f $(CONFIG_SUB) $(CONFIG_GUESS) build-aux/; \
    ./configure --prefix=$(PREFIX) --disable-nls \
    LDFLAGS="$(BCSYMMAPBITS)"

LIBDYLIB=$(PLATFORM)/libcpluff/.libs/libcpluff.a

CLEAN_FILES=$(PLATFORM)

all: .installed-$(PLATFORM)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM)/*; mkdir -p $(PLATFORM)
	cp -R cpluff/* $(PLATFORM)/
	cd $(PLATFORM); autoreconf -vif
	cd $(PLATFORM); $(CONFIGURE)

$(LIBDYLIB): $(PLATFORM)
ifeq ($(OS),ios)
	rm -rf $(PREFIX)/bcsymbolmaps/lib$(LIBNAME); mkdir -p $(PREFIX)/bcsymbolmaps/lib$(LIBNAME)
endif
	$(MAKE) -C $(PLATFORM)/libcpluff

.installed-$(PLATFORM): $(LIBDYLIB)
	$(MAKE) -C $(PLATFORM)/libcpluff install
	touch $@

clean:
	$(MAKE) -C $(PLATFORM) clean
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PREFIX)/bcsymbolmaps/lib$(LIBNAME)
	rm -rf $(PLATFORM) .installed-$(PLATFORM)

