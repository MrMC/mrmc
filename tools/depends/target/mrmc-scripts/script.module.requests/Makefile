-include ../../../Makefile.include
DEPS= ../../../Makefile.include Makefile

# lib name
LIBNAME=script.module.requests
VERSION=1.0.0
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz
REALARCHIVE=v2.12.4.tar.gz
BASE_URL=https://github.com/MrMC/script.module.requests/archive

CLEAN_FILES=$(ARCHIVE) $(PLATFORM)

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	$(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(REALARCHIVE)
	mv -f $(REALARCHIVE) $(TARBALLS_LOCATION)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)

.installed-$(PLATFORM): $(PLATFORM)
	rm -rf $(PREFIX)/mrmc/addons/$(LIBNAME)
	mkdir -p $(PREFIX)/mrmc/addons/$(LIBNAME)
	cp -rf $(PLATFORM)/* $(PREFIX)/mrmc/addons/$(LIBNAME)
	rm -f $(PREFIX)/mrmc/addons/$(LIBNAME)/README.md
	touch $@

clean:
	rm -rf .installed-$(PLATFORM) $(PREFIX)/mrmc/addons/$(LIBNAME)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM) $(PREFIX)/mrmc/addons/$(LIBNAME)

distcleanfiles::
	rm -rf $(PLATFORM) .installed-$(PLATFORM) $(PREFIX)/mrmc/addons/$(LIBNAME) $(TARBALLS_LOCATION)/$(ARCHIVE)
