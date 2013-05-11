/*-
 * Copyright (c) 2010 David Malone <dwmalone@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <netinet/in_softrss.h>
#include <netinet/toeplitz.h>

#ifdef _KERNEL
#include <sys/systm.h>
#else
#include <err.h>
#include <stdio.h>
#endif

#ifndef _KERNEL
static uint8_t	tkey8[RSS_KEYSIZE] = {
	0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
	0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
	0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
	0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
	0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa
};

#define	min(x, y)	((x) < (y) ? (x) : (y))
#endif

uint32_t
toeplitz_hash(u_int keylen, const uint8_t *key, u_int datalen,
    const uint8_t *data)
{
	uint32_t hash = 0, v;
	u_int i, b;

	/* XXXRW: Perhaps an assertion about key length vs. data length? */

	v = (key[0]<<24) + (key[1]<<16) + (key[2] <<8) + key[3];
	for (i = 0; i < datalen; i++) {
		for (b = 0; b < 8; b++) {
			if (data[i] & (1<<(7-b)))
				hash ^= v;
			v <<= 1;
			if ((i + 4) < RSS_KEYSIZE &&
			    (key[i+4] & (1<<(7-b))))
				v |= 1;
		}
	}
	return (hash);
}
