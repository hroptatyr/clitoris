include $(top_builddir)/version.mk

LANG = C
LC_ALL = C

ACLOCAL_AMFLAGS = -I m4

SUBDIRS =
EXTRA_DIST = $(DISTCLEANFILES)
DISTCLEANFILES =
CLEANFILES =

SUBDIRS += build-aux
SUBDIRS += src
SUBDIRS += info
SUBDIRS += test

DISTCLEANFILES += version.mk
DISTCLEANFILES += .version
EXTRA_DIST += version.mk.in

## make sure .version is read-only in the dist
dist-hook:
	chmod ugo-w $(distdir)/.version
