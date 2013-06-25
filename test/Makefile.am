# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
include $(top_builddir)/version.mk

LANG = C
LC_ALL = C

AM_CFLAGS = $(EXTRA_CFLAGS)
AM_CPPFLAGS = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -D_BSD_SOURCE
AM_CPPFLAGS += -DTEST

EXTRA_DIST = $(BUILT_SOURCES) $(TESTS) $(XFAIL_TESTS)
TESTS =
XFAIL_TESTS =
TEST_EXTENSIONS =
BUILT_SOURCES =
CLEANFILES = $(check_PROGRAMS)

TEST_EXTENSIONS += .clit
CLIT_LOG_COMPILER = $(top_builddir)/src/clitoris
AM_CLIT_LOG_FLAGS = --builddir $(top_builddir)/src

## the actual tests here
TESTS += ret_01.clit
XFAIL_TESTS += empty_01.clit
XFAIL_TESTS += empty_02.clit
XFAIL_TESTS += empty_03.clit

## Makefile.am ends here
