-include ../../Makefile.include
DEPS= ../../Makefile.include Makefile

ifneq ($(findstring tvos, $(TARGET_PLATFORM)), tvos)
	SUBDIRS += skin.ariana.touch
endif
ifneq ($(findstring ios, $(TARGET_PLATFORM)), ios)
	SUBDIRS += skin.opacity
	SUBDIRS += skin.blackglassnova
	SUBDIRS += skin.mrmc
	SUBDIRS += skin.pm3.hd
	SUBDIRS += skin.amber
	SUBDIRS += skin.ariana
endif

all: skins

skins:
	for d in $(SUBDIRS); do if test -f $$d/Makefile; then $(MAKE) -C $$d; fi; done

clean:
	for d in $(SUBDIRS); do if test -f $$d/Makefile; then $(MAKE) -C $$d clean; fi; done

distclean::
	for d in $(SUBDIRS); do if test -f $$d/Makefile; then $(MAKE) -C $$d distclean; fi; done

distcleanfiles::
	for d in $(SUBDIRS); do if test -f $$d/Makefile; then $(MAKE) -C $$d distcleanfiles; fi; done
