include $(top_builddir)/version.mk

# Help the Developers and yourself.  Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG=C
LC_ALL=C

BUILT_SOURCES =
BUILT_MANS =
EXTRA_DIST = $(BUILT_SOURCES) $(BUILT_MANS)

man1_MANS =
man1_MANS += $(BUILT_MANS)

BUILT_MANS += clitoris.man
BUILT_MANS += hxdiff.man
BUILT_MANS += shack.man

## help2man helpers
%.man: $(top_srcdir)/src/%.yuck $(top_srcdir)/configure $(top_builddir)/version.mk
	$(AM_V_GEN) PATH="$(top_builddir)/build-aux:$${PATH}" \
		yuck genman --version=$(VERSION) -o $@ $<

## Makefile.am ends here
