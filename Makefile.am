ACLOCAL_AMFLAGS = -I m4
if USE_SCANBUTTOND
SUBDIRS = src/scanbuttond src/scanbd conf 
else
SUBDIRS = src/scanbd
endif
SUBDIRS += conf integration scripts contrib doc

DISTCHECK_CONFIGURE_FLAGS = \
	--with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)

EXTRA_DIST = \
	Makefile.simple \
	Makefile.conf.in \
	Makefile.conf \
	Makefile.include.in \
	Makefile.include

noinst_DATA = \
	Makefile.conf \
	Makefile.include 

Makefile.conf: Makefile.conf.in configure
	echo "Generating $@...";
	sed  -e 's|@PACKAGEVERSION@|$(PACKAGE_VERSION)|g' \
	     -e 's|@PACKAGENAME@|$(PACKAGE)|g' $@.in  >$@

Makefile.include: Makefile.include.in configure
	echo "Generating $@...";
	sed  -e 's|@PACKAGEVERSION@|$(PACKAGE_VERSION)|g' \
	     -e 's|@PACKAGENAME@|$(PACKAGE)|g' $@.in  >$@


