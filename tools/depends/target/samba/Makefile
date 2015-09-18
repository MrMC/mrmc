include ../../Makefile.include
DEPS= ../../Makefile.include Makefile \
      samba30-3.0.37-configure.in.patch \
      samba30-3.0.37-CVE-2010-2063.patch \
      samba30-3.0.37-silence-receive-warning.patch \
      samba30-3.0.37-vfs_default.c.patch \
      samba30-3.0.37-wle-fix.patch \
      samba30-3.0.37-android.patch \
      samba30-3.0.37_darwin-iface-getifaddrs.patch

# lib name, version
LIBNAME=samba
VERSION=3.0.37
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz

# configuration settings
# android does not really support LFS but we can force it
# in libsmbclient by defining off_t to off64_t and changing
# libsmbclient.h to match. Defining _OFF_T_DEFINED_ allows
# us to change the typedef of off_t in android's sys/type.h.
# Then xbmc will correctly access smb shares with LFS enabled
# using the altered libsmbclient.h.
ifeq ($(OS),android)
export CFLAGS+=-D_FILE_OFFSET_BITS=64 -D_OFF_T_DEFINED_ -Doff_t=off64_t -Dlseek=lseek64
ANDROID_OFF_T_FIX=sed -ie "s|off_t|off64_t|g" "$(PLATFORM)/source/include/libsmbclient.h"
endif

# clang issue with optimizing out functions
CONFIGURE_HAMMER=samba_cv_optimize_out_funcation_calls=no

CONFIGURE= cp -f $(CONFIG_SUB) $(CONFIG_GUESS) .; \
          ./configure --prefix=$(PREFIX) \
        --disable-cups \
        --enable-static \
        --disable-shared \
        --disable-pie \
        --disable-iprint \
        --disable-fam \
        --without-cluster-support \
        --without-cifsupcall \
        --without-ads \
        --without-ldap \
        --with-included-popt \
        --with-included-iniparser \
        --without-sys-quotas \
        --without-krb5 \
        --without-automount \
        --without-sendfile-support \
        --with-libiconv=$(STAGING_DIR) \
        --without-cifsmount \
        --without-winbind \
	 --without-libmsrpc \
        $(CONFIGURE_HAMMER)

LIBDYLIB=$(PLATFORM)/source/bin/libsmbclient.a

CLEAN_FILES=$(ARCHIVE) $(PLATFORM)

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	cd $(TARBALLS_LOCATION); $(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	cd $(PLATFORM); patch -p1 < ../samba30-3.0.37-configure.in.patch
	cd $(PLATFORM); patch -p1 < ../samba30-3.0.37-CVE-2010-2063.patch
	cd $(PLATFORM); patch -p1 < ../samba30-3.0.37-silence-receive-warning.patch
	cd $(PLATFORM); patch -p1 < ../samba30-3.0.37-vfs_default.c.patch
	cd $(PLATFORM); patch -p1 < ../samba30-3.0.37-wle-fix.patch
	cd $(PLATFORM); patch -p1 < ../samba30-3.0.37-android.patch
	cd $(PLATFORM); patch -p1 < ../samba30-3.0.37_darwin-iface-getifaddrs.patch
	$(ANDROID_OFF_T_FIX)
	# remove fork/execX/system, it is not used for client
	sed -ie "s|forkret = fork()|forkret = (pid_t)-1|" "$(PLATFORM)/source/lib/system.c"
	sed -ie "s|execv|//execv|" "$(PLATFORM)/source/lib/system.c"
	sed -ie "s|execl(|(|" "$(PLATFORM)/source/lib/smbrun.c"
	sed -ie "s|system(cmd)|-1|" "$(PLATFORM)/source/lib/util.c"
	sed -ie "s|fork()|-1|" "$(PLATFORM)/source/lib/sock_exec.c"
	sed -ie "s|system(prog)|-1|" "$(PLATFORM)/source/lib/sock_exec.c"
	cd $(PLATFORM)/source && ./autogen.sh
	cd $(PLATFORM)/source; $(CONFIGURE)

$(LIBDYLIB): $(PLATFORM)
	$(MAKE) -C $(PLATFORM)/source headers
	mkdir -p $(PLATFORM)/source/bin
	$(MAKE) -C $(PLATFORM)/source libsmbclient

.installed-$(PLATFORM): $(LIBDYLIB)
	$(MAKE) -C $(PLATFORM)/source installclientlib
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
	$(MAKE) -C $(PLATFORM)/source clean
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)

