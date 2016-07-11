-include ../../Makefile.include
DEPS= ../../Makefile.include Makefile

# lib name, version
LIBNAME=repo-resources
VERSION=2.0.0-16.2
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz
BASE_URL=https://github.com/MrMC/repo-resources/archive

CLEAN_FILES=$(ARCHIVE) $(PLATFORM)

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	cd $(TARBALLS_LOCATION); $(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	rm -f $(PLATFORM)/readme.txt

.installed-$(PLATFORM): $(PLATFORM)
	rm -rf $(PREFIX)/mrmc/repo-resources
	mkdir -p $(PREFIX)/mrmc/repo-resources
	cp -rf $(PLATFORM)/* $(PREFIX)/mrmc/repo-resources
	touch $@

clean:
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM) $(PREFIX)/mrmc/repo-resources
