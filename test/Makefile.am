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
check_PROGRAMS =

TEST_EXTENSIONS += .clit
CLIT_LOG_COMPILER = $(top_builddir)/src/clitoris
AM_CLIT_LOG_FLAGS = --builddir $(builddir) --verbose

TESTS += $(XFAIL_TESTS)

## the actual tests here
TESTS += ret_01.clit
TESTS += empty_01.clit
TESTS += empty_02.clit
TESTS += empty_03.clit

TESTS += diff_01.clit
XFAIL_TESTS += diff_02.clit
TESTS += diff_03.clit
TESTS += diff_04.clit

XFAIL_TESTS += early_01.clit
XFAIL_TESTS += early_02.clit

check_PROGRAMS += tty_01
TESTS += tty_01.clit

XFAIL_TESTS += tty_02.clit
XFAIL_TESTS += tty_03.clit

TESTS += out-in-file_01.clit
TESTS += out-in-file_02.clit
out-in-file_02.log: out-in-file_01.log

TESTS += redir_01.clit
TESTS += redir_02.clit
TESTS += redir_03.clit

TESTS += vars_01.clit
TESTS += vars_02.clit
#TESTS += vars_03.clit
#TESTS += vars_04.clit

CLIT_LOG_COMPILER += --timeout 2
TESTS += timeo_01.clit
XFAIL_TESTS += timeo_02.clit
XFAIL_TESTS += timeo_03.clit
TESTS += timeo_04.clit

TESTS += shack_01.clit
XFAIL_TESTS += shack_02.clit

TESTS += hxdiff_01.clit
XFAIL_TESTS += hxdiff_02.clit

TESTS += ignore_01.clit
TESTS += ignore_02.clit
TESTS += ignore_03.clit
TESTS += ignore_04.clit
TESTS += ignore_05.clit
TESTS += ignore_06.clit

XFAIL_TESTS += keep-going_01.clit
XFAIL_TESTS += keep-going_02.clit

TESTS += negexp_01.clit
XFAIL_TESTS += negexp_02.clit
TESTS += negexp_03.clit
TESTS += negexp_04.clit
XFAIL_TESTS += negexp_05.clit
XFAIL_TESTS += negexp_06.clit

## Makefile.am ends here
