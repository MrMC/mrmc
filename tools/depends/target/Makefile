include ../Makefile.include

ifneq ($(shell test -f $(PREFIX)/share/config.site && echo 1),1)
  $(error Error: $(PREFIX)/share/config.site  is missing. Please reconfigure depends to generate it)
endif

DEPENDS  = pcre
DEPENDS += expat
DEPENDS += gettext
DEPENDS += sqlite3
DEPENDS += libgpg-error
DEPENDS += libgcrypt
DEPENDS += bzip2
DEPENDS += liblzo2
DEPENDS += libzip
DEPENDS += freetype2
DEPENDS += fontconfig
DEPENDS += openssl
DEPENDS += gmp
DEPENDS += nettle
DEPENDS += gnutls
DEPENDS += libssh2
DEPENDS += curl
DEPENDS += libjpeg-turbo
DEPENDS += libpng
DEPENDS += libgif
DEPENDS += fribidi
DEPENDS += libass
DEPENDS += libsquish
DEPENDS += libxml2
DEPENDS += yajl
DEPENDS += libmicrohttpd
DEPENDS += mysql
DEPENDS += libshairplay
DEPENDS += libplist
DEPENDS += libcec
DEPENDS += tinyxml
DEPENDS += dummy-libxbmc
DEPENDS += libamplayer
DEPENDS += libssh
DEPENDS += taglib
DEPENDS += libusb
DEPENDS += libnfs
DEPENDS += libxmlrpc
DEPENDS += libxslt
DEPENDS += libdcadec
DEPENDS += ffmpeg
DEPENDS += platform
DEPENDS += crossguid
DEPENDS += libcpluff

ifeq ($(ENABLE_GPLV3),yes)
  DEPENDS+=samba-gplv3
else
  DEPENDS+=samba
endif

EXCLUDED_DEPENDS =
ifeq ($(OS),ios)
  EXCLUDED_DEPENDS += libcec
  EXCLUDED_DEPENDS += libusb
endif

ifeq ($(OS),osx)
  ifneq ($(NEVER_YOU_MIND),yes)
    DEPENDS += libGLEW
  endif
  EXCLUDED_DEPENDS += libcec
  EXCLUDED_DEPENDS += libusb
endif

ifeq ($(OS),android)
  DEPENDS += mdnsresponder android-sources-ics
endif

DEPENDS := $(filter-out $(EXCLUDED_DEPENDS),$(DEPENDS))

ZLIB =
ifneq ($(HAS_ZLIB),1)
  DEPENDS += zlib
  ZLIB = zlib
endif

ICONV =
ifeq ($(NEED_LIBICONV),1)
  DEPENDS += libiconv
  ICONV = libiconv
endif

ALSA_LIB =
LINUX_SYSTEM_LIBS=
ifeq ($(OS),linux)
  DEPENDS += libuuid
  #not for raspberry pi
  ifneq ($(TARGET_PLATFORM),raspberry-pi)
    DEPENDS += libsdl2 linux-system-libs
    LINUX_SYSTEM_LIBS = linux-system-libs
  endif
  DEPENDS += alsa-lib
  ALSA_LIB = alsa-lib
endif

.PHONY: $(DEPENDS)

all: .installed-$(PLATFORM)

gettext: $(ICONV)
libgcrypt: libgpg-error
fontconfig: freetype2 expat $(ICONV)
libssh2: libgcrypt openssl
curl: openssl libssh2
libass: fontconfig libpng freetype2 expat $(ICONV)
libmicrohttpd: gnutls libgcrypt libgpg-error
libplist: libxml2 $(ZLIB)
libssh: openssl $(ZLIB)
mysql: openssl
libzip: $(ZLIB)
libpng: $(ZLIB)
openssl: $(ZLIB)
samba: libssh2
samba-gplv3: libssh2
gnutls: nettle $(ZLIB)
nettle: gmp
libsdl2: $(LINUX_SYSTEM_LIBS)
libxslt: libgcrypt
ffmpeg: $(ICONV) $(ZLIB) bzip2 gnutls libdcadec
libcec: platform
ifeq ($(OS),linux)
crossguid: libuuid
endif
libcpluff: expat

.installed-$(PLATFORM): $(DEPENDS)
	touch $@
	@echo "Dependencies built successfully."

$(DEPENDS):
	$(MAKE) -C $@

clean:
	for d in $(DEPENDS); do $(MAKE) -C $$d clean; done

# Debug target, this will DELETE all data in staging!
test-dependencies:
	( for d in $(DEPENDS); do \
	rm -rf $(PREFIX); \
	mkdir -p $(PREFIX)/include $(PREFIX)/share $(PREFIX)/bin; \
	cp -f config.site Toolchain.cmake $(PREFIX)/share/; \
	$(MAKE) distclean; \
	$(MAKE) $$d; done ) && echo "$@ built successfully"


distclean::
	for d in $(DEPENDS); do $(MAKE) -C $$d distclean; done

ifeq ($(OS),linux)
linux-system-libs:
	[ -f $(PREFIX)/lib/pkgconfig/x11.pc ] || ln -s /usr/lib/$(HOST)/pkgconfig/x11.pc $(PREFIX)/lib/pkgconfig/x11.pc
	[ -f $(PREFIX)/lib/pkgconfig/xproto.pc ] || ln -s /usr/share/pkgconfig/xproto.pc $(PREFIX)/lib/pkgconfig/xproto.pc
	[ -f $(PREFIX)/lib/pkgconfig/kbproto.pc ] || ln -s /usr/share/pkgconfig/kbproto.pc $(PREFIX)/lib/pkgconfig/kbproto.pc
	[ -f $(PREFIX)/lib/pkgconfig/xcb.pc ] || ln -s /usr/lib/$(HOST)/pkgconfig/xcb.pc $(PREFIX)/lib/pkgconfig/xcb.pc
	[ -f $(PREFIX)/lib/pkgconfig/pthread-stubs.pc ] || ln -s /usr/lib/$(HOST)/pkgconfig/pthread-stubs.pc $(PREFIX)/lib/pkgconfig/pthread-stubs.pc
	[ -f $(PREFIX)/lib/pkgconfig/xau.pc ] || ln -s /usr/lib/$(HOST)/pkgconfig/xau.pc $(PREFIX)/lib/pkgconfig/xau.pc
	[ -f $(PREFIX)/lib/pkgconfig/xdmcp.pc ] || ln -s /usr/lib/$(HOST)/pkgconfig/xdmcp.pc $(PREFIX)/lib/pkgconfig/xdmcp.pc
	[ -f $(PREFIX)/lib/pkgconfig/xext.pc ] || ln -s /usr/lib/$(HOST)/pkgconfig/xext.pc $(PREFIX)/lib/pkgconfig/xext.pc
	[ -f $(PREFIX)/lib/pkgconfig/xextproto.pc ] || ln -s /usr/share/pkgconfig/xextproto.pc $(PREFIX)/lib/pkgconfig/xextproto.pc
	[ -f $(PREFIX)/lib/pkgconfig/xrandr.pc ] || ln -s /usr/lib/$(HOST)/pkgconfig/xrandr.pc $(PREFIX)/lib/pkgconfig/xrandr.pc
	[ -f $(PREFIX)/lib/pkgconfig/xrender.pc ] || ln -s /usr/lib/$(HOST)/pkgconfig/xrender.pc $(PREFIX)/lib/pkgconfig/xrender.pc
	[ -f $(PREFIX)/lib/pkgconfig/randrproto.pc ] || ln -s /usr/share/pkgconfig/randrproto.pc $(PREFIX)/lib/pkgconfig/randrproto.pc
	[ -f $(PREFIX)/lib/pkgconfig/renderproto.pc ] || ln -s /usr/share/pkgconfig/renderproto.pc $(PREFIX)/lib/pkgconfig/renderproto.pc
	[ -f $(PREFIX)/lib/pkgconfig/xt.pc ] || ln -s /usr/lib/$(HOST)/pkgconfig/xt.pc $(PREFIX)/lib/pkgconfig/xt.pc
	[ -f $(PREFIX)/lib/pkgconfig/ice.pc ] || ln -s /usr/lib/$(HOST)/pkgconfig/ice.pc $(PREFIX)/lib/pkgconfig/ice.pc
	[ -f $(PREFIX)/lib/pkgconfig/sm.pc ] || ln -s /usr/lib/$(HOST)/pkgconfig/sm.pc $(PREFIX)/lib/pkgconfig/sm.pc
	[ -f $(PREFIX)/lib/pkgconfig/xmu.pc ] || ln -s /usr/lib/$(HOST)/pkgconfig/xmu.pc $(PREFIX)/lib/pkgconfig/xmu.pc
	[ -f $(PREFIX)/lib/pkgconfig/libdrm.pc ] || ln -s /usr/lib/$(HOST)/pkgconfig/libdrm.pc $(PREFIX)/lib/pkgconfig/libdrm.pc
endif
