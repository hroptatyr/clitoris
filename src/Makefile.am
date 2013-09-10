# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
include $(top_builddir)/version.mk

LANG = C
LC_ALL = C

AM_CFLAGS = $(EXTRA_CFLAGS)
AM_CPPFLAGS = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -D_BSD_SOURCE

bin_PROGRAMS =
noinst_PROGRAMS =
pkglib_LIBRARIES =
noinst_LIBRARIES =
BUILT_SOURCES =
EXTRA_DIST = $(BUILT_SOURCES)
CLEANFILES =

CLEANFILES += version.c
EXTRA_DIST += version.c.in

noinst_LIBRARIES += libclit.a
libclit_a_SOURCES = version.c version.h


bin_PROGRAMS += clitoris
clitoris_CPPFLAGS = $(AM_CPPFLAGS)
clitoris_CPPFLAGS += -D_GNU_SOURCE
clitoris_LDFLAGS = $(AM_LDFLAGS)
clitoris_LDADD = libclit.a
clitoris_LDADD += -lutil
BUILT_SOURCES += clitoris.x clitoris.h

bin_PROGRAMS += shack
shack_SOURCES = shack.c
shack_SOURCES += boobs.h
shack_LDADD = libclit.a
BUILT_SOURCES += shack.x shack.h


## generate version.c from .c.in
version.c: version.c.in $(top_builddir)/version.mk
	sed -e 's,[@]VERSION[@],$(VERSION),g' <$(srcdir)/$@.in >$@

## ggo rule
%.x %.h: %.ggo
	$(AM_V_GEN) gengetopt -l -c x -i $< -F $* \
	&& sed \
	  -e 's,config.h,version.h,g' \
	  -e 's,\(.*\)_PARSER_VERSION VERSION,\1_PARSER_VERSION $(PACKAGE)_version_string,g' \
	  $*.h >$*.h-t \
	&& mv $*.h-t $*.h


## for dist or dist check we need binaries, so help2man can work
dist-hook: $(bin_PROGRAMS)

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<

## Makefile.am ends here
