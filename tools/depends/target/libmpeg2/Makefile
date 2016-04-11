include ../../Makefile.include
DEPS= ../../Makefile.include Makefile

# lib name, version
LIBNAME=libmpeg2
VERSION=0.5.1
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz

# configuration settings
CONFIGURE=cp -f $(CONFIG_SUB) $(CONFIG_GUESS) .auto/ ; \
          ./configure --prefix=$(PREFIX) --disable-sdl

LIBDYLIB=$(PLATFORM)/$(LIBNAME)/.libs/$(LIBNAME).a

CLEAN_FILES=$(ARCHIVE) $(PLATFORM)

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	cd $(TARBALLS_LOCATION); $(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM)/*; mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
ifeq ($(OS),ios)
	cd $(PLATFORM); patch -p1 < ../01-libmpeg2-add-asm-leading-underscores.patch
ifeq ($(CPU),arm64)
	cd $(PLATFORM); patch -p1 < ../02-neon-arm64.patch
else
	cd $(PLATFORM); patch -p0 < ../02-neon.patch
endif
	cd $(PLATFORM); patch -p1 < ../03-config-fix.patch
endif
	cd $(PLATFORM); patch -p1 < ../04-clang-fix.patch
	cd $(PLATFORM); patch -p0 < ../05-upstream-motion_comp_arm_s.S-is-not-PIC-enough.patch
ifeq ($(OS),ios)
	case $(XCODE_VERSION) in \
	4.* | 4.*.*) \
	;;\
	*) \
	cd $(PLATFORM); patch -p0 < ../06-motion_comp_arm_s.S-even_more_pic.patch \
	;;\
	esac
endif
	cd $(PLATFORM); $(AUTORECONF) -vif
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

