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

EXTRA_CONFIG = --with-libdvdcss
EXTRA_CFLAGS = -D_XBMC -DHAVE_DVDCSS_DVDCSS_H
ifeq ($(NATIVE_OS),osx)
  EXTRA_CFLAGS +=  -D__DARWIN__
endif

# configuration settings
CONFIGURE= cp -f $(CONFIG_SUB) $(CONFIG_GUESS) .; \
	./configure --prefix=$(PREFIX) \
	--disable-shared --enable-static --with-pic \
	$(EXTRA_CONFIG) \
	CFLAGS="$(CFLAGS) $(EXTRA_CFLAGS)"

all: .installed-$(PLATFORM)

download: $(TARBALLS_LOCATION)/$(ARCHIVE)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	cd $(TARBALLS_LOCATION); $(RETRIEVE_TOOL) -o $(TARBALLS_LOCATION)/$(ARCHIVE) $(BASE_URL)/archive/$(VERSION).tar.gz

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(TARBALLS_LOCATION)/$(ARCHIVE)
	cd $(PLATFORM); patch -p1 < ../silence-stderr-spew.patch
	cd $(PLATFORM); patch -p1 < ../silence-gcc_struc-spew.patch
	cd $(PLATFORM); patch -p1 < ../fallback2iso9660.patch
	cd $(PLATFORM); patch -p1 < ../betterUnicode.patch
	cd $(PLATFORM); $(AUTORECONF) -vif
	cd $(PLATFORM); $(CONFIGURE)

$(PLATFORM)/.libs/$(LIBNAME).a: $(PLATFORM)
	$(MAKE) -C $(PLATFORM)

.installed-$(PLATFORM): $(PLATFORM)/.libs/$(LIBNAME).a
	$(MAKE) -C $(PLATFORM) install
	touch $@

clean:
	rm -rf $(PLATFORM) .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)

