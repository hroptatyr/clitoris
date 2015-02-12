/*** hxdiff.c -- diffing hexdumps
 *
 * Copyright (C) 2013-2015 Sebastian Freundt
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
#include <poll.h>

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
		uintptr_t _a_ = a;		\
		uintptr_t _b_ = b;		\
		_a_ < _b_ ? _a_ : _b_;		\
	})

typedef struct clit_buf_s clit_buf_t;

struct clit_buf_s {
	size_t z;
	const char *d;
};

struct clit_chld_s {
	const char *fn1;
	const char *fn2;

	int fd1;
	int fd2;
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

static void
setsock_nonblock(int sock)
{
	int opts;

	/* get former options */
	opts = fcntl(sock, F_GETFL);
	if (opts < 0) {
		return;
	}
	opts |= O_NONBLOCK;
	(void)fcntl(sock, F_SETFL, opts);
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

static void
mkfifofn(char *restrict buf, size_t bsz, const char *fn)
{
	static char i = '1';
	const char *tmp;

	if ((tmp = strrchr(fn, '/')) != NULL) {
		fn = tmp + 1U;
	}
	snprintf(buf, bsz, "hex output for FILE%c (%s)", i++, fn);
	return;
}

static int
init_diff(struct clit_chld_s ctx[static 1])
{
	static char expfn[PATH_MAX];
	static char actfn[PATH_MAX];
	pid_t diff = -1;

	if (mkfifofn(expfn, sizeof(expfn), ctx->fn1),
	    mkfifo(expfn, 0666) < 0) {
		error("cannot create fifo `%s'", expfn);
		goto out;
	} else if (mkfifofn(actfn, sizeof(actfn), ctx->fn2),
		   mkfifo(actfn, 0666) < 0) {
		error("cannot create fifo `%s'", actfn);
		goto out;
	}

	block_sigs();
	switch ((diff = vfork())) {
	case -1:
		/* i am an error */
		unblock_sigs();
		error("vfork failed");
		break;

	case 0:;
		/* i am the child */
		static char *const diff_opt[] = {
			"diff",
			"-u",
			expfn, actfn, NULL,
		};

		unblock_sigs();

		execvp("diff", diff_opt);
		error("execlp failed");
		_exit(EXIT_FAILURE);

	default:;
		/* i am the parent */
		int expfd;
		int actfd;

		expfd = open(expfn, O_WRONLY);
		actfd = open(actfn, O_WRONLY);

		/* and put result descriptors in output args */
		ctx->fd1 = expfd;
		ctx->fd2 = actfd;
		ctx->chld = diff;

		/* make out descriptors non-blocking */
		setsock_nonblock(ctx->fd1);
		setsock_nonblock(ctx->fd2);

		/* diff's stdout can just go straight there */
		break;
	}
out:
	/* clean up our fifo files, let's hope there's no other files
	 * with that name, innit? */
	if (*expfn) {
		(void)unlink(expfn);
	}
	if (*actfn) {
		(void)unlink(actfn);
	}
	return diff;
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

static unsigned int
pollpair(int fds[static 2U])
{
/* find the pairings of fd1 in/out, return bitmask
 * iohhee meaning input possible, output possible, hup, hup, err, err */
#define PIO	(POLLIN | POLLOUT)
	struct pollfd pfds[] = {
		{
			.fd = fds[0],
			.events = POLLIN,
		}, {
			.fd = fds[1],
			.events = POLLOUT,
		}
	};
	int npl = 0;

	if (poll(pfds, countof(pfds), 1) <= 0) {
		return 0;
	}
	if ((pfds[0].revents | pfds[1].revents) & POLLIN) {
		npl |= 1;
	}
	if ((pfds[0].revents | pfds[1].revents) & POLLOUT) {
		npl |= 2;
	}
	if (pfds[0].revents & POLLHUP) {
		npl |= 1 << 2U;
	}
	if (pfds[1].revents & POLLHUP) {
		npl |= 2 << 2U;
	}
	if (pfds[0].revents & (POLLERR | POLLNVAL)) {
		npl |= 1 << 4U;
	}
	if (pfds[1].revents & (POLLERR | POLLNVAL)) {
		npl |= 2 << 4U;
	}
	return npl;
}

struct fan_s {
	char buf[4096U];
	size_t tot;
	ssize_t nrd;
	ssize_t nwr;
	int f;
	int of;
};

static size_t
fanout1(struct fan_s f[static 1U])
{
	unsigned int npl = pollpair((int[]){!f->nrd ? f->f : -1, f->of});
	size_t res = 0U;

	if (!f->nrd && npl & 1U) {
		/* reader coru*/
		if ((f->nrd = read(f->f, f->buf, sizeof(f->buf))) > 0) {
			goto wr;
		} else if (f->nrd == 0) {
			f->f = -1;
		}
	}
	if (f->nwr > 0 && (npl & 2U)) {
		/* writer coru */
		clit_buf_t hx;
		ssize_t nwr;

	wr:
		hx = hexlify(f->buf, f->nrd, f->tot);
		if (UNLIKELY(!f->nwr)) {
			f->nwr = hx.z;
		} else {
			hx.d += (hx.z - f->nwr);
		}
		if ((nwr = write(f->of, hx.d, f->nwr)) > 0 &&
		    (f->nwr -= nwr) == 0U) {
			/* ah finished writing, innit? */
			f->tot += (res = f->nrd);
			f->nrd = 0U;
		}
	}
	return res;
}

static int
fanout(struct clit_chld_s ctx[static 1], int f1, size_t z1, int f2, size_t z2)
{
	struct fan_s fans[] = {
		{
			.f = f1,
			.of = ctx->fd1,
		}, {
			.f = f2,
			.of = ctx->fd2,
		}
	};
	size_t tot1 = 0U;
	size_t tot2 = 0U;

	while (tot1 < z1 || tot2 < z2) {
		if (LIKELY(tot1 < z1)) {
			if (UNLIKELY((tot1 += fanout1(fans + 0U)) >= z1)) {
				close(fans[0U].of);
				fans[0U].of = -1;
			}
		}

		if (LIKELY(tot2 < z2)) {
			if (UNLIKELY((tot2 += fanout1(fans + 1U)) >= z2)) {
				close(fans[1U].of);
				fans[1U].of = -1;
			}
		}
	}
	return 0;
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
	}

	if (fstat(fd1, &st) < 0) {
		error("Error: cannot stat file `%s'", file1);
		goto clo;
	}
	fz1 = st.st_size;

	if (fstat(fd2, &st) < 0) {
		error("Error: cannot stat file `%s'", file2);
		goto clo;
	}
	fz2 = st.st_size;

	/* assign file names (for init_diff) */
	ctx->fn1 = file1;
	ctx->fn2 = file2;
	if (UNLIKELY(init_chld(ctx) < 0)) {
		goto clo;
	} else if (UNLIKELY(init_diff(ctx) < 0)) {
		goto clo;
	}

	/* now, we read a bit of fd1, hexdump it, feed it to diff's fd1
	 * then read a bit of fd2, hexdump it, feed it to diff's fd2 */
	fanout(ctx, fd1, fz1, fd2, fz2);

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


#include "hxdiff.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int rc = 99;

	if (yuck_parse(argi, argc, argv)) {
		goto out;
	} else if (argi->nargs != 2U) {
		yuck_auto_help(argi);
		goto out;
	}

	with (const char *f1 = argi->args[0U], *f2 = argi->args[1U]) {
		if ((rc = hxdiff(f1, f2)) < 0) {
			rc = 99;
		}
	}

out:
	yuck_free(argi);
	return rc;
}

/* hxdiff.c ends here */
