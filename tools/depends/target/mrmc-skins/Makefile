-include ../../Makefile.include
DEPS= ../../Makefile.include Makefile


SUBDIRS  = skin.blackglassnova
SUBDIRS += skin.sio2

all: skins

skins:
	for d in $(SUBDIRS); do if test -f $$d/Makefile; then $(MAKE) -C $$d; fi; done

clean:
	for d in $(SUBDIRS); do if test -f $$d/Makefile; then $(MAKE) -C $$d clean; fi; done

distclean::
	for d in $(SUBDIRS); do if test -f $$d/Makefile; then $(MAKE) -C $$d distclean; fi; done

distcleanfiles::
	for d in $(SUBDIRS); do if test -f $$d/Makefile; then $(MAKE) -C $$d distcleanfiles; fi; done
