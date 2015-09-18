-include ../../Makefile.include
DEPS= ../../Makefile.include Makefile

# lib name, version
LIBNAME=ulxmlrpcpp
VERSION=1.7.5
SOURCE=$(LIBNAME)-$(VERSION)-src
ARCHIVE=$(SOURCE).tar.bz2

# configuration settings
CONFIGURE=cp -f $(CONFIG_SUB) $(CONFIG_GUESS) build-aux/; \
    ./configure --prefix=$(PREFIX) \
    CFLAGS="-D__unix__" LDFLAGS="-lc++" CXXFLAGS="-D__unix__ -Wno-error=c++11-narrowing"

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
	cd $(PLATFORM); $(AUTORECONF) -vif
	cd $(PLATFORM); patch -p1 < ../tvos-bitcode-fix.patch
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
