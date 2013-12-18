# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
include $(top_builddir)/version.mk

LANG = C
LC_ALL = C

AM_CFLAGS = $(EXTRA_CFLAGS)
AM_CPPFLAGS = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE
AM_CPPFLAGS += -D_ALL_SOURCE -D_NETBSD_SOURCE
AM_CPPFLAGS += -DHAVE_VERSION_H

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
if HAVE_PTY_H
clitoris_LDADD += -lutil
endif  HAVE_PTY_H
BUILT_SOURCES += clitoris.yucc

bin_PROGRAMS += shack
shack_LDADD = libclit.a
BUILT_SOURCES += shack.yucc

bin_PROGRAMS += hxdiff
hxdiff_LDADD = libclit.a
BUILT_SOURCES += hxdiff.yucc


SUFFIXES = .yuck .yucc .sa

analysis: $(bin_PROGRAMS:%=%.sa)

## generate version.c from .c.in
version.c: version.c.in $(top_builddir)/version.mk
	sed -e 's,[@]VERSION[@],$(VERSION),g' <$(srcdir)/$@.in >$@

.yuck.yucc:
	$(AM_V_GEN) PATH=$(top_builddir)/build-aux:"$${PATH}" \
		yuck$(EXEEXT) gen $< > $@ || { rm -f -- $@; false; }

## for dist or dist check we need binaries, so help2man can work
dist-hook: $(bin_PROGRAMS)

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<

## Create assembler output (debugging purposes only)
.c.sa:
	$(AM_V_GEN) clang -std=gnu99 --analyze \
		$(AM_CFLAGS) $(AM_CPPFLAGS) -D_GNU_SOURCE \
		-I$(builddir) -I$(srcdir) -DHAVE_CONFIG_H $< -o $@

## Makefile.am ends here
