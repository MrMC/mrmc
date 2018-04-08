-include ../../../Makefile.include
DEPS= ../../../Makefile.include Makefile

# lib name
SKINNAME=skin.mrmc
VERSION=1.0.0
SOURCE=$(SKINNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz
REALARCHIVE=master.tar.gz
BASE_URL=https://github.com/MrMC/skin.mrmc/archive

RSYNC_SKIN =rsync -av
RSYNC_SKIN +=--exclude .DS_Store* --exclude LICENSE.txt --exclude README.md --exclude changelog.txt
RSYNC_SKIN +=--include /media/Textures.xbt
RSYNC_SKIN +=--exclude /media/*

SKIN_FILES = $(shell find $(PLATFORM) -type f -name "*" | sed 's/ /\\ /g')
SKIN_MEDIA  = $(shell find $(PLATFORM)/media -type f -name "*jpg" | sed 's/ /\\ /g')
SKIN_MEDIA += $(shell find $(PLATFORM)/media -type f -name "*png | sed 's/ /\\ /g'")
SKIN_MEDIA += $(shell find $(PLATFORM)/media -type f -name "*gif | sed 's/ /\\ /g'")

TARGET = $(PLATFORM)/media/Textures.xbt

CLEAN_FILES=$(ARCHIVE) $(PLATFORM)

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	$(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(REALARCHIVE)
	mv -f $(REALARCHIVE) $(TARBALLS_LOCATION)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)

$(TARGET): $(PLATFORM) $(SKIN_MEDIA)
ifeq ($(findstring arm, $(CPU)), arm)
	$(NATIVEPREFIX)/bin/TexturePacker -dupecheck -use_none -input $(PLATFORM)/media -output $(TARGET)
else
	$(NATIVEPREFIX)/bin/TexturePacker -dupecheck -input $(PLATFORM)/media -output $(TARGET)
endif

.installed-$(PLATFORM): $(TARGET) $(SKIN_FILES)
	rm -rf $(PREFIX)/mrmc/addons/$(SKINNAME)
	mkdir -p $(PREFIX)/mrmc/addons/$(SKINNAME)
	$(RSYNC_SKIN) $(PLATFORM)/* $(PREFIX)/mrmc/addons/$(SKINNAME)
	touch $@

clean:
	rm -rf .installed-$(PLATFORM) $(PREFIX)/mrmc/addons/$(SKINNAME)
	rm -f $(TARGET)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM) $(PREFIX)/mrmc/addons/$(SKINNAME)

distcleanfiles::
	rm -rf $(PLATFORM) .installed-$(PLATFORM) $(PREFIX)/mrmc/addons/$(SKINNAME) $(TARBALLS_LOCATION)/$(ARCHIVE)