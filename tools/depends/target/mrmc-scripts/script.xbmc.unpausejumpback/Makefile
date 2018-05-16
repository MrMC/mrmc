-include ../../../Makefile.include
DEPS= ../../../Makefile.include Makefile

# lib name
LIBNAME=script.xbmc.unpausejumpback
VERSION=2.3.6
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz
BASE_URL=https://github.com/MrMC/repo-scripts/branches/gotham

CLEAN_FILES=$(ARCHIVE) $(PLATFORM)

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	svn export $(BASE_URL)/$(LIBNAME) $(SOURCE)
	$(ARCHIVE_TOOL) -cvzf $(TARBALLS_LOCATION)/$(ARCHIVE) $(SOURCE)
	rm -rf $(SOURCE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)

.installed-$(PLATFORM): $(PLATFORM)
	rm -rf $(PREFIX)/mrmc/addons/$(LIBNAME)
	mkdir -p $(PREFIX)/mrmc/addons/$(LIBNAME)
	cp -rf $(PLATFORM)/* $(PREFIX)/mrmc/addons/$(LIBNAME)
	rm -f $(PREFIX)/mrmc/addons/$(LIBNAME)/README
	rm -f $(PREFIX)/mrmc/addons/$(LIBNAME)/README.md
	rm -f $(PREFIX)/mrmc/addons/$(LIBNAME)/LICENSE.txt
	rm -f $(PREFIX)/mrmc/addons/$(LIBNAME)/changelog.txt
	touch $@

clean:
	rm -rf .installed-$(PLATFORM) $(PREFIX)/mrmc/addons/$(LIBNAME)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM) $(PREFIX)/mrmc/addons/$(LIBNAME)

distcleanfiles::
	rm -rf $(PLATFORM) .installed-$(PLATFORM) $(PREFIX)/mrmc/addons/$(LIBNAME) $(TARBALLS_LOCATION)/$(ARCHIVE)
