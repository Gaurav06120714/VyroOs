# vyro-apt-source — meta-package that enables the Vyro APT repo
# Installation enables the disabled-by-default sources entry shipped on
# the ISO and pulls in the GPG keyring.

PREFIX  ?= /usr

.PHONY: all install clean

all:
	@true

install:
	install -D -m 0644 ../../live-build/config/includes.chroot/etc/apt/sources.list.d/vyro.sources \
		$(DESTDIR)/etc/apt/sources.list.d/vyro.sources
	@# Flip Enabled: no → Enabled: yes in the installed copy
	sed -i 's/^Enabled: no$$/Enabled: yes/' $(DESTDIR)/etc/apt/sources.list.d/vyro.sources

clean:
	@true
