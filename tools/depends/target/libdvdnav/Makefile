-include ../../Makefile.include
DEPS = Makefile ../../Makefile.include

LIBNAME=libdvdnav
BASE_URL=https://github.com/mrmc/libdvdnav
VERSION=master

RETRIEVE_TOOL := curl -Ls --create-dirs --retry 10 --retry-delay 3
ARCHIVE_TOOL := tar --strip-components=1 -xf

GITREV=$(shell git ls-remote $(BASE_URL) $(VERSION) | awk '{print $$1}')
ifeq ($(GITREV),)
  ARCHIVE=$(LIBNAME)-$(VERSION).tar.gz
else
  ARCHIVE=$(LIBNAME)-$(GITREV).tar.gz
endif

EXTRA_CFLAGS = -g -D_XBMC -DHAVE_DVDCSS_DVDCSS_H
ifeq ($(NATIVE_OS),osx)
  EXTRA_CFLAGS += -D__DARWIN__
endif
ifeq ($(OS),osx)
  # since we removed undefined dynamic_lookup in libtool,
  # need to actually link to required frameworks
  EXTRA_LDFLAGS += -framework IOKit -framework CoreFoundation
endif



# configuration settings
CONFIGURE= cp -f $(CONFIG_SUB) $(CONFIG_GUESS) .; \
	./configure --prefix=$(PREFIX) \
	--enable-shared --disable-static --with-pic \
	CFLAGS="$(CFLAGS) $(EXTRA_CFLAGS)" \
	LDFLAGS="$(LDFLAGS) $(EXTRA_LDFLAGS)"


CLEAN_FILES=$(ARCHIVE) $(PLATFORM)

all: .installed-$(PLATFORM)

download: $(TARBALLS_LOCATION)/$(ARCHIVE)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	cd $(TARBALLS_LOCATION); $(RETRIEVE_TOOL) -o $(TARBALLS_LOCATION)/$(ARCHIVE) $(BASE_URL)/archive/$(VERSION).tar.gz

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(TARBALLS_LOCATION)/$(ARCHIVE)
	cd $(PLATFORM); patch -p1 < ../expose_dvd_defs.patch
	cd $(PLATFORM); $(AUTORECONF) -vif
	cd $(PLATFORM); $(CONFIGURE)

$(PLATFORM)/.libs/$(LIBNAME).a: $(PLATFORM)
	[ -d $(PLATFORM)/.libs ] && [ ! -f $@ ] && $(MAKE) -C $(PLATFORM) clean || :
	$(MAKE) -C $(PLATFORM)

.installed-$(PLATFORM): $(PLATFORM)/.libs/$(LIBNAME).a
	# change *this to *self or die under c++
	sed -ie "s|*this|*self|" "$(PLATFORM)/src/dvdnav/dvdnav.h"
	sed -ie "s|*this|*self|" "$(PLATFORM)/src/dvdnav_internal.h"
	# now we can do the install
	$(MAKE) -C $(PLATFORM) install
	# nasty but we access dvdnav internals and need the private headers
	cp $(PLATFORM)/src/vm/vm.h $(PREFIX)/include/dvdnav/vm.h
	cp $(PLATFORM)/src/vm/decoder.h $(PREFIX)/include/dvdnav/decoder.h
	cp $(PLATFORM)/src/dvdnav_internal.h $(PREFIX)/include/dvdnav/dvdnav_internal.h
	touch $@

clean:
	rm -rf $(PLATFORM) .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)

