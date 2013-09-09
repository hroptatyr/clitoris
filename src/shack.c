/*** shack.c -- sha check
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
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include "boobs.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif	/* !LIKELY */
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* UNLIKELY */

#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

#define _paste(x, y)	x ## y
#define paste(x, y)	_paste(x, y)

#if !defined with
# define with(args...)							\
	for (args, *paste(__ep, __LINE__) = (void*)1;			\
	     paste(__ep, __LINE__); paste(__ep, __LINE__)= 0)
#endif	/* !with */

/* operator by value */
typedef struct {
	uint32_t v[5U];
} sha_t;


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


static inline __attribute__((const, pure)) uint32_t
rotl(uint32_t x, unsigned int n)
{
	n %= 32U;
	return (x << n) | (x >> (32 - n));
}

static sha_t
sha_chunk(const uint32_t b32[static 16], sha_t old)
{
	uint32_t s32[80U];
	sha_t res = old;
#define a	res.v[0U]
#define b	res.v[1U]
#define c	res.v[2U]
#define d	res.v[3U]
#define e	res.v[4U]

	/* extend the sixteen 32-bit words that make up BUF into 80 words */
	for (size_t i = 0; i < 16U; i++) {
		s32[i] = be32toh(b32[i]);
	}
	for (size_t i = 16U; i < 32U; i++) {
		s32[i] = s32[i - 3] ^ s32[i - 8] ^ s32[i - 14] ^ s32[i - 16];
		s32[i] = rotl(s32[i], 1U);
	}
	for (size_t i = 32U; i < 80U; i++) {
		s32[i] = s32[i - 6] ^ s32[i - 16] ^ s32[i - 28] ^ s32[i - 32];
		s32[i] = rotl(s32[i], 2U);
	}

	/* main loop */
	for (size_t i = 0; i < 80U; i++) {
		uint32_t f, k;

		switch (i / 20U) {
		case 0U/*0U ... 19U*/:
			f = d ^ (b & (c ^ d));
			k = 0x5a827999u;
			break;
		case 1u/*20u ... 39u*/:
			f = b ^ c ^ d;
			k = 0x6ed9eba1u;
			break;
		case 2u/*40u ... 59u*/:
			f = (b & c) | (b & d) | (c & d);
			k = 0x8f1bbcdcu;
			break;
		case 3u/*60u ... 79u*/:
			f = b ^ c ^ d;
			k = 0xca62c1d6u;
			break;
		}

		with (uint32_t tmp = rotl(a, 5U) + f + e + k + s32[i]) {
			e = d;
			d = c;
			c = rotl(b, 30U);
			b = a;
			a = tmp;
		}
	}

	/* add to old values */
	for (size_t i = 0U; i < 5U; i++) {
		res.v[i] += old.v[i];
	}
#undef a
#undef b
#undef c
#undef d
#undef e
	return res;
}

static sha_t
sha_fin(const uint32_t b32[static 1], sha_t old, size_t fz/*in bytes*/)
{
	uint32_t l[16U];
	size_t fz512 = fz % 64U;

	/* copy the first chunk */
	memcpy(l, b32, sizeof(l));
	with (uint8_t *restrict l8 = (void*)l) {
		/* append sep char 0x80 */
		l8[fz512] = 0x80U;
	}
	/* last 2 l entries are big-endian fz */
	with (uint64_t *restrict l64 = (void*)l) {
		l64[7U] = htobe64(fz * 8U/*CHAR_BIT*/);
	}
	return sha_chunk(l, old);
}

static sha_t
shaf(const char *fn)
{
	static const sha_t null_sha = {};
	struct stat st;
	int fd;
	size_t fz;
	sha_t h = {
		0x67452301U,
		0xEFCDAB89U,
		0x98BADCFEU,
		0x10325476U,
		0xC3D2E1F0U,
	};

	if ((fd = open(fn, O_RDONLY)) < 0) {
		error(0, "Error: cannot open file `%s'", fn);
		goto clo;
	} else if (fstat(fd, &st) < 0) {
		error(0, "Error: cannot stat file `%s'", fn);
		goto clo;
	}

	/* get the file size */
	if ((fz = st.st_size) == 0U) {
		h = sha_fin(NULL, h, 0U);
		goto out;
	}
	/* now map considerable portions of the file and process */
	for (size_t i = 0; i < fz / 4096U; i++) {
		off_t o = i * 4096U;
		void *p = mmap(NULL, 4096U, PROT_READ, MAP_SHARED, fd, o);

		if (UNLIKELY(p == MAP_FAILED)) {
			goto clo;
		}
		for (size_t j = 0; j < 4096U; j += 64U) {
			const void *b32 = (const uint8_t*)p + j;
			h = sha_chunk(b32, h);
		}
		if (munmap(p, 4096U) < 0) {
			goto clo;
		}
	}
	with (size_t rest = fz % 4096U) {
		off_t o = fz - rest;
		void *p = mmap(NULL, rest, PROT_READ, MAP_SHARED, fd, o);
		size_t j;

		if (UNLIKELY(p == MAP_FAILED)) {
			goto clo;
		}
		for (j = 0; j < rest / 64U; j++) {
			const void *b32 = (const uint8_t*)p + j * 64U;
			h = sha_chunk(b32, h);
		}
		/* and the very last block */
		with (const void *b32 = (const uint8_t*)p + j * 64U) {
			h = sha_fin(b32, h, fz);
		}
		/* and now munmap() for good */
		if (munmap(p, rest) < 0) {
			goto clo;
		}
	}
out:
	/* convert to big endian */
	for (size_t i = 0U; i < 5U; i++) {
		h.v[i] = htobe32(h.v[i]);
	}
	close(fd);
	return h;
clo:
	close(fd);
	return null_sha;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "shack.h"
#include "shack.x"
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
	} else if (argi->inputs_num < 1) {
		print_help_common();
		goto out;
	}

	with (const char *fn = argi->inputs[0U]) {
		/* compute the sha of FN */
		sha_t sum = shaf(fn);

		/* hex it */
		for (const uint8_t *sp = (const void*)sum.v,
			     *const ep = sp + sizeof(sum); sp < ep; sp++) {
			static char hx[] = "0123456789abcdef";
			putchar(hx[(*sp & 0xf0U) >> 4U]);
			putchar(hx[(*sp & 0x0fU) >> 0U]);
		}
		putchar('\n');
	}

out:
	cmdline_parser_free(argi);
	/* never succeed */
	return rc;
}

/* shack.c ends here */
