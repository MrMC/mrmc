include ../../Makefile.include
DEPS= ../../Makefile.include Makefile

# lib name, version
LIBNAME=mDNSResponder
VERSION=379.37-android
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz
REALARCHIVE=master.tar.gz
BASE_URL=https://github.com/MrMC/mDNSResponder/archive

LIBDYLIB=$(PLATFORM)/$(OUTPUTNAME).a

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	$(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(REALARCHIVE)
	mv -f $(REALARCHIVE) $(TARBALLS_LOCATION)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM)/*; mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	cd $(PLATFORM); cp ../makefile.internal Makefile

$(LIBDYLIB): $(PLATFORM)
	cd $(PLATFORM); CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS)" $(MAKE)

.installed-$(PLATFORM): $(LIBDYLIB)
	cd $(PLATFORM); PREFIX="$(PREFIX)" $(MAKE) install
	touch $@

clean:
	make -C $(PLATFORM) clean 
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)
