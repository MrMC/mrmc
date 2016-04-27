-include ../../Makefile.include
DEPS= ../../Makefile.include Makefile

# lib name, version
LIBNAME=ulxmlrpcpp
VERSION=1.7.5
SOURCE=$(LIBNAME)-$(VERSION)-src
ARCHIVE=$(SOURCE).tar.bz2

CXXFLAGS_EXTRA = -D__unix__
ifeq ($(OS),android)
CONFIG_EXTRA = ac_cv_func_realloc_0_nonnull=yes
else ifeq ($(OS),linux)
CONFIG_EXTRA = LDFLAGS="-lz -ldl"
endif
ifeq ($(NATIVE_OS),osx)
CXXFLAGS_EXTRA += -Wno-error=c++11-narrowing
CONFIG_EXTRA = LDFLAGS="-lc++"
endif

# configuration settings
CONFIGURE=cp -f $(CONFIG_SUB) $(CONFIG_GUESS) build-aux/; \
    ./configure --prefix=$(PREFIX) \
    CFLAGS="$(CFLAGS) -D__unix__"  \
    CXXFLAGS="$(CFLAGS) $(CXXFLAGS_EXTRA)" \
    $(CONFIG_EXTRA)

LIBDYLIB=$(PLATFORM)/ulxmlrpcpp/.libs/libulxmlrpcpp.a

CLEAN_FILES=$(ARCHIVE) $(PLATFORM)

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	cd $(TARBALLS_LOCATION); $(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM)/*; mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	sed -ie "s|contrib docs tests||" "$(PLATFORM)/Makefile.am"
	sed -ie "s|contrib docs tests||" "$(PLATFORM)/ulxmlrpcpp/Makefile.am"
	sed -ie "s|fork()|-1|" "$(PLATFORM)/ulxmlrpcpp/contrib/mprocess_rpc_server.cpp"
ifeq ($(OS),android)
	sed -ie "s|#include <sys/timeb.h>||" "$(PLATFORM)/ulxmlrpcpp/ulxr_log4j.cpp"
	cd $(PLATFORM); patch -p1 < ../01-droid-no-domainname.patch
	cd $(PLATFORM); patch -p1 < ../02-droid-missing-libc-bits.patch
	cd $(PLATFORM); patch -p0 < ../03-const_correct.patch
endif
	cd $(PLATFORM); $(AUTORECONF) -vif
	cd $(PLATFORM); $(CONFIGURE)

$(LIBDYLIB): $(PLATFORM)
	$(MAKE) -C $(PLATFORM)

.installed-$(PLATFORM): $(LIBDYLIB)
	$(MAKE) -C $(PLATFORM) install
	rm -rf $(PREFIX)/lib/libulxmlrpcpp*.dylib $(PREFIX)/lib/libulxmlrpcpp*.so $(PREFIX)/lib/libulxmlrpcpp.la
	touch $@

clean:
	$(MAKE) -C $(PLATFORM) clean
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)
