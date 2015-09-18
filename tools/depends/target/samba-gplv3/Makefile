include ../../Makefile.include
DEPS= ../../Makefile.include Makefile samba_android.patch

# lib name, version
LIBNAME=samba
VERSION=3.6.12
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz

# clang issue with optimizing out functions
CONFIGURE_HAMMER=samba_cv_optimize_out_funcation_calls=no

CONFIGURE= cp -f $(CONFIG_SUB) $(CONFIG_GUESS) .; \
          ./configure --prefix=$(PREFIX) --disable-shared --enable-static \
          --with-fhs \
          --disable-fam \
          --disable-cups \
          --disable-swat \
          --disable-avahi \
          --disable-iprint \
          --without-ads \
          --without-pam \
          --without-pam_smbpass \
          --without-ldap \
          --without-winbind \
          --without-libaddns \
          --without-libnetapi \
          --without-dnsupdate \
          --without-cluster-support \
          --without-libsmbsharemodes \
          --with-libiconv=$(STAGING_DIR) \
          $(CONFIGURE_HAMMER)

# configuration settings
# android does not really support LFS but we can force it
# in libsmbclient by defining off_t to off64_t and changing
# libsmbclient.h to match. Defining _OFF_T_DEFINED_ allows
# us to change the typedef of off_t in android's sys/type.h.
# Then xbmc will correctly access smb shares with LFS enabled
# using the altered libsmbclient.h.
ifeq ($(OS),android)
export CFLAGS+=-D_FILE_OFFSET_BITS=64 -D_OFF_T_DEFINED_ -Doff_t=off64_t -Dlseek=lseek64
endif

LIBDYLIB=$(PLATFORM)/source3/bin/libsmbclient.a

CLEAN_FILES=$(ARCHIVE) $(PLATFORM)

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	cd $(TARBALLS_LOCATION); $(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
ifeq ($(OS),android)
	cd $(PLATFORM); patch -p0 < ../samba_android.patch
endif
	cd $(PLATFORM)/source3; $(CONFIGURE)

$(LIBDYLIB): $(PLATFORM)
	$(MAKE) -C $(PLATFORM)/source3 libsmbclient
	$(MAKE) -C $(PLATFORM)/source3 libwbclient
	$(MAKE) -C $(PLATFORM)/source3 libtalloc
	$(MAKE) -C $(PLATFORM)/source3 libtdb

.installed-$(PLATFORM): $(LIBDYLIB)
	$(MAKE) -C $(PLATFORM)/source3 installlibsmbclient
	$(MAKE) -C $(PLATFORM)/source3 installlibwbclient
	$(MAKE) -C $(PLATFORM)/source3 installlibtalloc
	$(MAKE) -C $(PLATFORM)/source3 installlibtdb
	# internal collides with _MD5xxx in libmicrohttpd.a
	$(RPL) -e "_MD5Init" "_MDxInit" $(PREFIX)/lib/libsmbclient.a
	$(RPL) -e "_MD5Final" "_MDxFinal" $(PREFIX)/lib/libsmbclient.a
	$(RPL) -e "_MD5Update" "_MDxUpdate" $(PREFIX)/lib/libsmbclient.a
	# internal collides with _SHA256_xxx in libcrypto.a(sha256.o)
	$(RPL) -e "_SHA256_Init" "_SHA256xInit" $(PREFIX)/lib/libsmbclient.a
	$(RPL) -e "_SHA256_Final" "_SHA256xFinal" $(PREFIX)/lib/libsmbclient.a
	$(RPL) -e "_SHA256_Update" "_SHA256xUpdate" $(PREFIX)/lib/libsmbclient.a
	# internal collides with _packet_send libssh.a(packet.o)
	$(RPL) -e "_packet_send" "_SMBpkt_send" $(PREFIX)/lib/libsmbclient.a
	touch $@

clean:
	$(MAKE) -C $(PLATFORM)/source3 clean
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)

