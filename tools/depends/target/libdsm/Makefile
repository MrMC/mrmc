include ../../Makefile.include
DEPS= ../../Makefile.include Makefile \
	lib-includepath.patch

# lib name, version
LIBNAME=dsm
VERSION=0.2.2_4
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz
REALARCHIVE=mrmc-jarvis-16.0-4.tar.gz
BASE_URL=https://github.com/MrMC/libdsm/archive

CONFIGURE= cp -f $(CONFIG_SUB) $(CONFIG_GUESS) .; \
          ./configure --prefix=$(PREFIX) \
        --enable-shared \
        --disable-static \
        --disable-programs \
        --disable-debug

LIBDYLIB=$(PLATFORM)/.libs/libdsm.dylib

CLEAN_FILES=$(ARCHIVE) $(PLATFORM)

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	$(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(REALARCHIVE)
	mv -f $(REALARCHIVE) $(TARBALLS_LOCATION)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	cd $(PLATFORM); ./bootstrap
	cd $(PLATFORM); patch -p1 < ../lib-includepath.patch
ifeq ($(OS),android)
	sed -ie "s|HAVE_LANGINFO_H|HAVE_BALONEY|" "$(PLATFORM)/src/smb_utils.c"
endif
	cd $(PLATFORM); $(CONFIGURE)

$(LIBDYLIB): $(PLATFORM)
	$(MAKE) -C $(PLATFORM)

.installed-$(PLATFORM): $(LIBDYLIB)
	$(MAKE) -C $(PLATFORM) install
	touch $@

clean:
	$(MAKE) -C $(PLATFORM) clean
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)

