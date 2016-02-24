-include ../../Makefile.include
DEPS = Makefile ../../Makefile.include

LIBNAME=libdvdread
BASE_URL=https://github.com/mrmc/libdvdread
VERSION=master

RETRIEVE_TOOL := curl -Ls --create-dirs --retry 10 --retry-delay 3
ARCHIVE_TOOL := tar --strip-components=1 -xf

GITREV=$(shell git ls-remote $(BASE_URL) $(VERSION) | awk '{print $$1}')
ifeq ($(GITREV),)
  ARCHIVE=$(LIBNAME)-$(VERSION).tar.gz
else
  ARCHIVE=$(LIBNAME)-$(GITREV).tar.gz
endif

EXTRA_CFLAGS = -D_XBMC
ifeq ($(OS),android)
  EXTRA_CFLAGS +=  -D_FILE_OFFSET_BITS=64 -D_OFF_T_DEFINED_ -Doff_t=off64_t -Dlseek=lseek64
endif
ifeq ($(OS),osx)
  EXTRA_CFLAGS +=  -D__DARWIN__
endif
ifeq ($(OS),ios)
  EXTRA_CFLAGS +=  -D__DARWIN__ -UHAVE_DVDCSS_DVDCSS_H
else
  config += --with-libdvdcss
  EXTRA_CFLAGS += -DHAVE_DVDCSS_DVDCSS_H
endif

# configuration settings
CONFIGURE= cp -f $(CONFIG_SUB) $(CONFIG_GUESS) .; \
	./configure --prefix=$(PREFIX) \
	--disable-shared --enable-static --with-pic \
	CFLAGS="$(CFLAGS) $(EXTRA_CFLAGS)"

CLEAN_FILES=$(ARCHIVE) $(PLATFORM)

all: .installed-$(PLATFORM)

download: $(TARBALLS_LOCATION)/$(ARCHIVE)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	cd $(TARBALLS_LOCATION); $(RETRIEVE_TOOL) -o $(TARBALLS_LOCATION)/$(ARCHIVE) $(BASE_URL)/archive/$(VERSION).tar.gz

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(TARBALLS_LOCATION)/$(ARCHIVE)
ifeq ($(NATIVE_OS),osx)
	# shut up clang warnings about gcc_struct
	sed -ie "s|,gcc_struct||" "$(PLATFORM)/src/dvdread/ifo_types.h"
endif
	cd $(PLATFORM); $(AUTORECONF) -vif
	cd $(PLATFORM); $(CONFIGURE)

$(PLATFORM)/.libs/$(LIBNAME).a: $(PLATFORM)
	[ -d $(PLATFORM)/.libs ] && [ ! -f $@ ] && $(MAKE) -C $(PLATFORM) clean || :
	$(MAKE) -C $(PLATFORM)

.installed-$(PLATFORM): $(PLATFORM)/.libs/$(LIBNAME).a
	$(MAKE) -C $(PLATFORM) install
	touch $@

clean:
	$(MAKE) -C $(PLATFORM) clean
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)

