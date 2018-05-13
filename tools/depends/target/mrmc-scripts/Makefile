-include ../../Makefile.include
DEPS= ../../Makefile.include Makefile


SUBDIRS  = script-plex
SUBDIRS += script.advanced.wol
SUBDIRS += script.keymap
SUBDIRS += script.module.requests
SUBDIRS += script.xbmc.unpausejumpback
SUBDIRS += service.libraryautoupdate

all: scripts

scripts:
	for d in $(SUBDIRS); do if test -f $$d/Makefile; then $(MAKE) -C $$d; fi; done

clean:
	for d in $(SUBDIRS); do if test -f $$d/Makefile; then $(MAKE) -C $$d clean; fi; done

distclean::
	for d in $(SUBDIRS); do if test -f $$d/Makefile; then $(MAKE) -C $$d distclean; fi; done

distcleanfiles::
	for d in $(SUBDIRS); do if test -f $$d/Makefile; then $(MAKE) -C $$d distcleanfiles; fi; done
