-include ../../Makefile.include

VERSION.TXT := $(XBMCROOT)/version.txt
#APP_NAME=$(shell awk '/APP_NAME/ {print tolower($$2)}' $(VERSION.TXT))
APP_NAME=$(shell awk '/APP_NAME/ {print $$2}' $(VERSION.TXT))

SOURCE=../../../../

export CXXFLAGS+=-O3
export CFLAGS+=-O3

# configuration settings
CONFIGURE = cp -f $(CONFIG_SUB) $(CONFIG_GUESS) build-aux/ ;\
  ./configure --prefix=$(PREFIX) $(DEBUG)

ifeq ($(OS),android)
  CONFIGURE += --disable-libcec
  ifeq ($(lite),yes)
    CONFIGURE += --with-bundle_extension=lite
  else ifeq ($(dev),yes)
    CONFIGURE += --with-bundle_extension=dev
  endif
else
  ifeq ($(lite),yes)
    CONFIGURE += --with-bundle_extension=lite
  else ifeq ($(OS),osx)
    CONFIGURE += --with-bundle_extension=osx
    ifeq ($(USE_SIMULATOR),yes)
      CONFIGURE += --enable-gles
    endif
  else ifeq ($(OS),ios)
    ifeq ($(TARGET_PLATFORM),tvos)
      CONFIGURE += --with-bundle_extension=tvos
    else
      CONFIGURE += --with-bundle_extension=ios
    endif
  endif
endif

CONFIGURE += $(CONFIG_EXTRA)

all: $(SOURCE)/lib$(APP_NAME).so

Release: DEBUG=--enable-debug=no
Release: $(SOURCE)/lib$(APP_NAME).so

Debug: DEBUG=--enable-debug=yes
Debug: $(SOURCE)/lib$(APP_NAME).so

$(SOURCE)/lib$(APP_NAME).so:
	cd $(SOURCE); SWIG_LIB=$(NATIVEPREFIX)/share/swig/3.0.10 BOOTSTRAP_FROM_DEPENDS=yes ./bootstrap
	cd $(SOURCE); echo $(EXPORTS) echo $(CONFIGURE)

../../Makefile.include:
	$(error Please run configure)

clean:
	cd $(SOURCE); $(MAKE) clean

distclean:
	cd $(SOURCE); $(MAKE) clean
