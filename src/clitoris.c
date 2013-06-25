/*** clitoris.c -- command line testing oris
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
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <string.h>
#include <errno.h>
#include <pty.h>

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif	/* !LIKELY */
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* UNLIKELY */

#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

#if !defined with
# define with(args...)	for (args, *__ep__ = (void*)1; __ep__; __ep__ = 0)
#endif	/* !with */


typedef struct clitf_s clitf_t;
typedef struct clit_bit_s clit_bit_t;
typedef struct clit_tst_s *clit_tst_t;

struct clitf_s {
	size_t z;
	void *d;
};

struct clit_bit_s {
	size_t z;
	const char *d;
};

struct clit_chld_s {
	int pin;
	int pou;
	int pll;
	pid_t chld;
};

/* a test is the command (inlcuding stdin), stdout result, and stderr result */
struct clit_tst_s {
	clit_bit_t cmd;
	clit_bit_t out;
	clit_bit_t err;
	clit_bit_t rest;
};


static void
__attribute__((format(printf, 2, 3)))
error(int eno, const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (eno || errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(eno ?: errno), stderr);
	}
	fputc('\n', stderr);
	return;
}


static clitf_t
mmap_fd(int fd, size_t fz)
{
	void *p;

	if ((p = mmap(NULL, fz, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		return (clitf_t){.z = 0U, .d = NULL};
	}
	return (clitf_t){.z = fz, .d = p};
}

static int
munmap_fd(clitf_t map)
{
	return munmap(map.d, map.z);
}


static const char *
find_shtok(const char *bp, size_t bz)
{
/* finds a (lone) occurrence of $ at the beginning of a line */
	for (const char *res;
	     (res = memchr(bp, '$', bz)) != NULL;
	     bz -= (res + 1 - bp), bp = res + 1) {
		/* we're actually after a "\n$" */
		if (res == bp || res[-1] == '\n') {
			return res;
		}
	}
	return NULL;
}

static clit_bit_t
find_cmd(const char *bp, size_t bz)
{
	clit_bit_t resbit = {0U};
	clit_bit_t tok;

	/* find the bit where it says '$ ' */
	with (const char *res) {
		if (UNLIKELY((res = find_shtok(bp, bz)) == NULL)) {
			return (clit_bit_t){0U};
		} else if (UNLIKELY(res[1] != ' ')) {
			return (clit_bit_t){0U};
		}
		/* otherwise */
		resbit.d = res += 2U;
		bz -= res - bp;
		bp = res;
	}

	/* find the new line bit */
	for (const char *res;
	     (res = memchr(bp, '\n', bz)) != NULL;
	     bz -= (res + 1U - bp), bp = res + 1U) {
		size_t lz = (res + 1U - bp);

		/* check for trailing \ or <<EOF (in that line) */
		if (UNLIKELY((tok.d = memmem(bp, lz, "<<", 2)) != NULL)) {
			tok.d += 2U;
			tok.z = res - tok.d;
			/* analyse this eof token */
			bp = res + 1U;
			goto eof;
		} else if (res == bp || res[-1] != '\\') {
			resbit.z = res + 1 - resbit.d;
			break;
		}
	}
	return resbit;

eof:
	/* massage tok so that it starts on a non-space and ends on one */
	for (; *tok.d == ' ' || *tok.d == '\t'; tok.d++, tok.z--);
	for (;
	     tok.z && (tok.d[tok.z - 1] == ' ' || tok.d[tok.z - 1] == '\t');
	     tok.z--);
	if ((*tok.d == '\'' || *tok.d == '"') && tok.d[tok.z - 1] == *tok.d) {
		tok.d++;
		tok.z -= 2U;
	}
	/* now find the opposite EOF token */
	for (const char *eotok;
	     (eotok = memmem(bp, bz, tok.d, tok.z)) != NULL;
	     bz -= eotok + 1U - bp, bp = eotok + 1U) {
		if (LIKELY(eotok[-1] == '\n' && eotok[tok.z] == '\n')) {
			resbit.z = eotok + tok.z + 1U - resbit.d;
			break;
		}
	}
	return resbit;
}

static int
find_tst(struct clit_tst_s tst[static 1], const char *bp, size_t bz)
{
	if (UNLIKELY(!(tst->cmd = find_cmd(bp, bz)).z)) {
		goto fail;
	}
	/* reset bp and bz */
	bz = bz - (tst->cmd.d + tst->cmd.z - bp);
	bp = tst->cmd.d + tst->cmd.z;
	if (UNLIKELY((tst->rest.d = find_shtok(bp, bz)) == NULL)) {
		goto fail;
	}
	/* otherwise set the rest bit already */
	tst->rest.z = bz - (tst->rest.d - bp);

	/* now the stdout and stderr bits must be in between (or 0) */
	tst->out = (clit_bit_t){.z = tst->rest.d - bp, bp};
	tst->err = (clit_bit_t){0U};
	return 0;
fail:
	memset(tst, 0, sizeof(*tst));
	return -1;
}

static int
init_chld(struct clit_chld_s ctx[static 1])
{
/* set up a connection with /bin/sh to pipe to and read from */
	int pty;
	int pin[2];
	int pou[2];

	if (0) {
		;
	} else if (UNLIKELY(pipe(pin) < 0)) {
		ctx->chld = -1;
		return -1;
	} else if (UNLIKELY(pipe(pou) < 0)) {
		ctx->chld = -1;
		return -1;
	}

	/* allow both vfork() and forkpty() if requested */
	switch ((ctx->chld = 1 ? vfork() : forkpty(&pty, NULL, NULL, NULL))) {
	case -1:
		/* i am an error */
		return -1;

	case 0:;
		/* i am the child, read from pin and write to pou */
		if (1) {
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			/* pin[0] ->stdin */
			dup2(pin[0], STDIN_FILENO);
			close(pin[1]);
		}
		/* stdout -> pou[1] */
		dup2(pou[1], STDOUT_FILENO);
		close(pou[0]);
		execl("/bin/sh", "sh", NULL);

	default:
		/* i am the parent, clean up descriptors */
		if (1) {
			close(pin[0]);
		}
		close(pou[1]);

		/* assign desc, write end of pin */
		if (1) {
			ctx->pin = pin[1];
		} else {
			ctx->pin = pty;
		}
		/* ... and read end of pou */
		ctx->pou = pou[0];

		if ((ctx->pll = epoll_create1(EPOLL_CLOEXEC)) >= 0) {
			struct epoll_event ev = {
				EPOLLIN | EPOLLRDHUP | EPOLLHUP,
			};

			epoll_ctl(ctx->pll, EPOLL_CTL_ADD, ctx->pou, &ev);
		}

		/* just to be on the safe side, send a false */
		write(ctx->pin, "false\n", sizeof("false\n") - 1U);
		break;
	}
	return 0;
}

static int
fini_chld(struct clit_chld_s ctx[static 1])
{
	int st;

	if (UNLIKELY(ctx->chld == -1)) {
		return -1;
	}

	/* end of epoll monitoring */
	epoll_ctl(ctx->pll, EPOLL_CTL_DEL, ctx->pou, NULL);
	close(ctx->pll);

	/* and indicate end of pipes */
	close(ctx->pin);
	close(ctx->pou);

	while (waitpid(ctx->chld, &st, 0) != -1);
	if (WIFEXITED(st)) {
		return WEXITSTATUS(st);
	} else if (WIFSIGNALED(st)) {
		return 0;
	}
	return -1;
}

static int
diff_out(struct clit_chld_s ctx[static 1], clit_bit_t exp)
{
	static char *buf;
	static size_t bsz;
	int rc = 0;

	/* check and maybe realloc read buffer */
	if (exp.z > bsz) {
		bsz = ((exp.z / 4096U) + 1U) * 4096U;
		buf = realloc(buf, bsz);
	}

	if (read(ctx->pou, buf, bsz) != exp.z || memcmp(buf, exp.d, exp.z)) {
		/* also check for equality */
		puts("output differs");
		rc = 1;
	}
	return rc;
}

static int
run_tst(struct clit_chld_s ctx[static 1], struct clit_tst_s tst[static 1])
{
	static struct epoll_event ev[1];
	int rc = 0;

	write(ctx->pin, tst->cmd.d, tst->cmd.z);
	if (tst->out.z > 0U) {
		if (epoll_wait(ctx->pll, ev, countof(ev), 2000/*ms*/) <= 0) {
			/* indicate timeout */
			puts("timeout");
			return -1;
		}
		rc = diff_out(ctx, tst->out);
	} else {
		/* we expect no output, check if there is some anyway */
		if (epoll_wait(ctx->pll, ev, countof(ev), 10/*ms*/) > 0) {
			puts("output present but not expected");
			rc = -1;
		}
	}
	return rc;
}

static int
test_f(clitf_t tf)
{
	static struct clit_chld_s ctx[1];
	static struct clit_tst_s tst[1];
	const char *bp = tf.d;
	size_t bz = tf.z;
	int rc = -1;

	if (UNLIKELY(init_chld(ctx) < 0)) {
		return -1;
	}
	for (; find_tst(tst, bp, bz) == 0; bp = tst->rest.d, bz = tst->rest.z) {
		if ((rc = run_tst(ctx, tst)) < 0) {
			break;
		}
	}
	with (int fin_rc) {
		if ((fin_rc = fini_chld(ctx))) {
			rc = fin_rc;
		}
	}
	return rc;
}

static int
test(const char *testfile)
{
	int fd;
	struct stat st;
	clitf_t tf;

	if ((fd = open(testfile, O_RDONLY)) < 0) {
		error(0, "Error: cannot open file `%s'", testfile);
	} else if (fstat(fd, &st) < 0) {
		error(0, "Error: cannot stat file `%s'", testfile);
	} else if ((tf = mmap_fd(fd, st.st_size)).d == NULL) {
		error(0, "Error: cannot map file `%s'", testfile);
	} else {
		/* yaay */
		int rc;

		rc = test_f(tf);
		munmap_fd(tf);
		return rc;
	}
	return -1;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "clitoris.h"
#include "clitoris.x"
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
	} else if (argi->inputs_num != 1) {
		print_help_common();
		goto out;
	}

	if (argi->builddir_given) {
		setenv("builddir", argi->builddir_arg, 1);
	}
	if (argi->srcdir_given) {
		setenv("srcdir", argi->srcdir_arg, 1);
	}
	if (argi->hash_given) {
		setenv("hash", argi->hash_arg, 1);
	}
	if (argi->husk_given) {
		setenv("husk", argi->husk_arg, 1);
	}

	/* also bang builddir to path */
	with (char *blddir = getenv("builddir")) {
		if (blddir != NULL) {
			size_t blddiz = strlen(blddir);
			char *path = getenv("PATH");
			size_t patz = strlen(path);
			char *newp;

			newp = malloc(patz + blddiz + 1U/*:*/ + 1U/*\nul*/);
			memcpy(newp, blddir, blddiz);
			newp[blddiz] = ':';
			memcpy(newp + blddiz + 1U, path, patz + 1U);
			setenv("PATH", newp, 1);
			free(newp);
		}
	}
	/* just to be clear about this */
#if defined WORDS_BIGENDIAN
	setenv("endian", "big", 1);
#else  /* !WORDS_BIGENDIAN */
	setenv("endian", "little", 1);
#endif	/* WORDS_BIGENDIAN */

	if ((rc = test(argi->inputs[0])) < 0) {
		rc = 99;
	}

out:
	cmdline_parser_free(argi);
	/* never succeed */
	return rc;
}

/* clitoris.c ends here */
