# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
include $(top_builddir)/version.mk

LANG = C
LC_ALL = C

EXTRA_DIST = $(BUILT_SOURCES) $(TESTS) $(XFAIL_TESTS)
TESTS =
XFAIL_TESTS =
TEST_EXTENSIONS =

TEST_EXTENSIONS += .clit
CLIT_LOG_COMPILER = $(top_builddir)/src/clitoris
AM_CLIT_LOG_FLAGS = --builddir $(top_builddir)/src

TESTS += $(XFAIL_TESTS)

## the actual tests here
TESTS += ret_01.clit
TESTS += empty_01.clit
TESTS += empty_02.clit
TESTS += empty_03.clit

TESTS += diff_01.clit
XFAIL_TESTS += diff_02.clit

XFAIL_TESTS += early_01.clit
XFAIL_TESTS += early_02.clit

## Makefile.am ends here
