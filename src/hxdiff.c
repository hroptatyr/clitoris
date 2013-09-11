/*** hxdiff.c -- diffing hexdumps
 *
 * Copyright (C) 2013 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of clitoris.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif	/* !LIKELY */
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* UNLIKELY */

#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

#if !defined UNUSED
# define UNUSED(x)	__attribute__((unused)) x
#endif	/* !UNUSED */

#if !defined with
# define with(args...)	for (args, *__ep__ = (void*)1; __ep__; __ep__ = 0)
#endif	/* !with */

#define xmin(a, b)				\
	({					\
		typeof(a) _a_ = a;		\
		typeof(b) _b_ = b;		\
		_a_ < _b_ ? _a_ : _b_;		\
	})

typedef struct clit_buf_s clit_buf_t;

struct clit_buf_s {
	size_t z;
	const char *d;
};

struct clit_chld_s {
	int fd_df1;
	int fd_df2;
	pid_t chld;

	unsigned int verbosep:1;
};

sigset_t fatal_signal_set[1];
sigset_t empty_signal_set[1];


static void
__attribute__((format(printf, 1, 2)))
error(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}


static void
block_sigs(void)
{
	(void)sigprocmask(SIG_BLOCK, fatal_signal_set, (sigset_t*)NULL);
	return;
}

static void
unblock_sigs(void)
{
	sigprocmask(SIG_SETMASK, empty_signal_set, (sigset_t*)NULL);
	return;
}


static int
init_chld(struct clit_chld_s UNUSED(ctx)[static 1])
{
	/* set up the set of fatal signals */
	sigemptyset(fatal_signal_set);
	sigaddset(fatal_signal_set, SIGHUP);
	sigaddset(fatal_signal_set, SIGQUIT);
	sigaddset(fatal_signal_set, SIGINT);
	sigaddset(fatal_signal_set, SIGTERM);
	sigaddset(fatal_signal_set, SIGXCPU);
	sigaddset(fatal_signal_set, SIGXFSZ);
	/* also the empty set */
	sigemptyset(empty_signal_set);
	return 0;
}

static int
fini_chld(struct clit_chld_s UNUSED(ctx)[static 1])
{
	return 0;
}

static int
init_diff(struct clit_chld_s ctx[static 1])
{
	int p_exp[2];
	int p_is[2];
	pid_t diff;

	/* expected and is pipes for diff */
	if (pipe(p_exp) < 0) {
		return -1;
	} else if (pipe(p_is) < 0) {
		goto clo_exp;
	}

	block_sigs();
	switch ((diff = vfork())) {
	case -1:
		/* i am an error */
		unblock_sigs();
		goto clo;

	case 0:;
		/* i am the child */
		static char fa[64];
		static char fb[64];
		static char *const diff_opt[] = {
			"diff",
			"-u", "--label=expected", "--label=actual",
			fa, fb, NULL,
		};

		unblock_sigs();

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		/* kick the write ends of our pipes */
		close(p_exp[1]);
		close(p_is[1]);

		/* stdout -> stderr */
		dup2(STDERR_FILENO, STDOUT_FILENO);

		snprintf(fa, sizeof(fa), "/dev/fd/%d", *p_exp);
		snprintf(fb, sizeof(fb), "/dev/fd/%d", *p_is);

		execvp("diff", diff_opt);
		error("execlp failed");
		_exit(EXIT_FAILURE);

	default:;
		/* i am the parent */

		/* clean up descriptors */
		close(*p_exp);
		close(*p_is);

		/* and put result descriptors in output args */
		ctx->fd_df1 = p_exp[1];
		ctx->fd_df2 = p_is[1];
		ctx->chld = diff;

		/* diff's stdout can just go straight there */
		break;
	}
	return 0;
clo:
	close(p_is[0]);
	close(p_is[1]);
clo_exp:
	close(p_exp[0]);
	close(p_exp[1]);
	return -1;
}

static int
fini_diff(struct clit_chld_s ctx[static 1])
{
	int st;
	int rc;

	unblock_sigs();

	while (waitpid(ctx->chld, &st, 0) != ctx->chld);
	if (LIKELY(WIFEXITED(st))) {
		rc = WEXITSTATUS(st);
	} else {
		rc = 1;
	}
	return rc;
}

static size_t
hexlify_addr(char *restrict buf, off_t a)
{
	static char hx[] = "0123456789abcdef";

	for (size_t j = 8U; j > 0; j--, a >>= 4) {
		buf[j - 1U] = hx[a & 0x0fU];
	}
	return 8U;
}

static size_t
hexlify_beef(char *restrict buf, const uint8_t *src, size_t ssz)
{
	static char hx[] = "0123456789abcdef";
	char *restrict bp = buf;
	const uint8_t *sp = src;
	size_t j;

#define HX(c)						\
	({						\
		uint8_t x = (c);			\
		*bp++ = hx[(x >> 4U) & 0x0fU];		\
		*bp++ = hx[(x >> 0U) & 0x0fU];		\
	})

	for (j = 0U; j < xmin(ssz, 8U); j++) {
		HX(*sp++);
		*bp++ = ' ';
	}
	*bp++ = ' ';
	for (; j < xmin(ssz, 16U); j++) {
		HX(*sp++);
		*bp++ = ' ';
	}
	/* pad with spaces */
	for (; j < 16U; j++) {
		*bp++ = ' ';
		*bp++ = ' ';
		*bp++ = ' ';
	}
	return bp - buf;
}

static size_t
hexlify_pudd(char *restrict buf, const uint8_t *src, size_t ssz)
{
	char *restrict bp = buf;
	const uint8_t *sp = src;

#define AX(c)								\
	({								\
		uint8_t x = (c);					\
		(char)((x >= 0x20U && x < 0x7fU) ? (char)x : '.');	\
	})

	for (size_t j = 0U; j < xmin(ssz, 16U); j++) {
		*bp++ = AX(*sp++);
	}
	return bp - buf;
}

static size_t
hexlify_line(char *restrict buf, const uint8_t *src, size_t ssz, off_t off)
{
	char *restrict bp = buf;

	/* print addr */
	bp += hexlify_addr(bp, off);
	*bp++ = ' ';
	*bp++ = ' ';

	/* the actual hex contents */
	bp += hexlify_beef(bp, src, ssz);

	/* print the ascii portion */
	*bp++ = ' ';
	*bp++ = ' ';
	*bp++ = '|';
	bp += hexlify_pudd(bp, src, ssz);
	*bp++ = '|';
	*bp++ = '\n';
	return bp - buf;
}

static clit_buf_t
hexlify(const char *src, size_t ssz, off_t off)
{
	/* size for 4k of binary data */
	static char buf[5U * 4096U];
	const uint8_t *sp = (const void*)src;
	char *bp = buf;

	for (size_t i = 0U; i < ssz / 16U; i++, sp += 16U, off += 16U) {
		bp += hexlify_line(bp, sp, 16U, off);
	}
	/* last one is special */
	if (ssz % 16U) {
		bp += hexlify_line(bp, sp, ssz % 16U, off);
	}
	return (clit_buf_t){.z = bp - buf, .d = buf};
}


static int
hxdiff(const char *file1, const char *file2)
{
	static struct clit_chld_s ctx[1];
	int fd1;
	int fd2;
	struct stat st;
	size_t fz1;
	size_t fz2;
	int rc = -1;

	if ((fd1 = open(file1, O_RDONLY)) < 0) {
		error("Error: cannot open file `%s'", file1);
		goto out;
	} else if ((fd2 = open(file2, O_RDONLY)) < 0) {
		error("Error: cannot open file `%s'", file2);
		goto clo;
	} else if (fstat(fd1, &st) < 0) {
		error("Error: cannot stat file `%s'", file1);
		goto clo;
	} else if ((fz1 = st.st_size, 0)) {
		/* optimised out */
	} else if (fstat(fd2, &st) < 0) {
		error("Error: cannot stat file `%s'", file2);
		goto clo;
	} else if ((fz2 = st.st_size, 0)) {
		/* optimised out */
	}

	if (UNLIKELY(init_chld(ctx) < 0)) {
		goto clo;
	} else if (UNLIKELY(init_diff(ctx) < 0)) {
		goto clo;
	}

	/* now, we read a bit of fd1, hexdump it, feed it to diff's fd1
	 * then read a bit of fd2, hexdump it, feed it to diff's fd2 */
	{
		char buf[4096U];
		size_t tot1 = 0U;
		size_t tot2 = 0U;
		ssize_t nrd;

		do {
			if (UNLIKELY(tot1 >= fz1)) {
				/* nothing coming from fd1 anymore */
				;
			} else if ((nrd = read(fd1, buf, sizeof(buf))) > 0) {
				clit_buf_t hx = hexlify(buf, nrd, tot1);

				write(ctx->fd_df1, hx.d, hx.z);
				if (UNLIKELY((tot1 += nrd) >= fz1)) {
					close(ctx->fd_df1);
				}
			}
			if (UNLIKELY(tot2 >= fz2)) {
				/* nothing coming from fd2 anymore */
				;
			} else if ((nrd = read(fd2, buf, sizeof(buf))) > 0) {
				clit_buf_t hx = hexlify(buf, nrd, tot2);

				write(ctx->fd_df2, hx.d, hx.z);
				if (UNLIKELY((tot2 += nrd) >= fz2)) {
					close(ctx->fd_df2);
				}
			}

			printf("%zu/%zu  %zu/%zu\n", tot1, fz1, tot2, fz2);
		} while (tot1 < fz1 || tot2 < fz2);
	}

	/* get the diff tool's exit status et al */
	rc = fini_diff(ctx);

	if (UNLIKELY(fini_chld(ctx) < 0)) {
		rc = -1;
	}

clo:
	close(fd1);
	close(fd2);
out:
	return rc;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "hxdiff.h"
#include "hxdiff.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct gengetopt_args_info argi[1];
	int rc = 99;

	if (cmdline_parser(argc, argv, argi)) {
		goto out;
	} else if (argi->inputs_num != 2) {
		print_help_common();
		goto out;
	}

	with (const char *f1 = argi->inputs[0U], *f2 = argi->inputs[1U]) {
		if ((rc = hxdiff(f1, f2)) < 0) {
			rc = 99;
		}
	}

out:
	cmdline_parser_free(argi);
	return rc;
}

/* hxdiff.c ends here */
