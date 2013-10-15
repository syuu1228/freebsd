/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/linker_set.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "bhyverun.h"
#include "inout.h"

SET_DECLARE(inout_port_set, struct inout_port);

#define	MAX_IOPORTS	(1 << 16)

#define	VERIFY_IOPORT(port, size) \
	assert((port) >= 0 && (size) > 0 && ((port) + (size)) <= MAX_IOPORTS)

static struct {
	const char	*name;
	int		flags;
	inout_func_t	handler;
	void		*arg;
} inout_handlers[MAX_IOPORTS];

static int
default_inout(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
              uint32_t *eax, void *arg)
{
        if (in) {
                switch (bytes) {
                case 4:
                        *eax = 0xffffffff;
                        break;
                case 2:
                        *eax = 0xffff;
                        break;
                case 1:
                        *eax = 0xff;
                        break;
                }
        }
        
        return (0);
}

static void 
register_default_iohandler(int start, int size)
{
	struct inout_port iop;
	
	VERIFY_IOPORT(start, size);

	bzero(&iop, sizeof(iop));
	iop.name = "default";
	iop.port = start;
	iop.size = size;
	iop.flags = IOPORT_F_INOUT;
	iop.handler = default_inout;

	register_inout(&iop);
}

int
emulate_inout(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
	      uint32_t *eax, int strict)
{
	int flags;
	uint32_t mask;
	inout_func_t handler;
	void *arg;

	assert(port < MAX_IOPORTS);

	handler = inout_handlers[port].handler;

	if (strict && handler == default_inout) {
		fprintf(stderr, "%s vcpu:%d in:%d port:%x bytes:%d\n",
			__func__, vcpu, in, port, bytes);
		return (-1);
	}

	if (!in) {
		switch (bytes) {
		case 1:
			mask = 0xff;
			break;
		case 2:
			mask = 0xffff;
			break;
		default:
			mask = 0xffffffff;
			break;
		}
		*eax = *eax & mask;
	}

	flags = inout_handlers[port].flags;
	arg = inout_handlers[port].arg;

	if ((in && (flags & IOPORT_F_IN)) || (!in && (flags & IOPORT_F_OUT)))
		return ((*handler)(ctx, vcpu, in, port, bytes, eax, arg));
	else {
		fprintf(stderr, "%s vcpu:%d in:%d port:%x bytes:%d\n",
			__func__, vcpu, in, port, bytes);
		return (-1);
	}
}

#define REP_OUTS_FUNC(type, bytes) \
do { \
	type *addr = paddr_guest2host(ctx, (uint32_t)*rsi, *rcx); \
	int i, error; \
	for (i = 0; i < *rcx / bytes; i++) { \
		uint32_t eax = addr[i]; \
		error = emulate_inout(ctx, vcpu, 0, port, bytes, &eax, \
			strict); \
		if (error) \
			return (error); \
	} \
	return (0); \
} while(0)

static int
emulate_rep_outsb(struct vmctx *ctx, int vcpu, int port, uint64_t *rcx,
		  uint64_t *rsi, int strict)
{
	static const int bytes = 1;
	int len = *rcx;
	uintptr_t gaddr = *rsi;
	uint8_t *haddr;
	int i, error;

	if ((gaddr & 0xffffffff00000000) == 0xffffffff00000000)
		gaddr &= 0x00000000ffffffff; 
	fprintf(stderr, "%s:%d gaddr=0x%lx\n", __func__, __LINE__, gaddr);
	haddr = paddr_guest2host(ctx, gaddr, len);
	if (!haddr)
		fprintf(stderr, "%s:%d !haddr\n", __func__, __LINE__);
	if (!haddr)
		return (-1);
	for (i = 0; i < len / 1; i++) {
		uint32_t eax = haddr[i];
		error = emulate_inout(ctx, vcpu, 0, port, bytes, &eax,
			strict);
		if (error)
			fprintf(stderr, "%s:%d emulate_inout i=%d port=%x failed\n",
				__func__, __LINE__, i, port);
		if (error)
			return (error);
	}
	return (0);
//	REP_OUTS_FUNC(uint8_t, 1);
}

static int
emulate_rep_outsw(struct vmctx *ctx, int vcpu, int port, uint64_t *rcx,
		  uint64_t *rsi, int strict)
{
	REP_OUTS_FUNC(uint16_t, 2);
}
static int
emulate_rep_outsd(struct vmctx *ctx, int vcpu, int port, uint64_t *rcx,
		  uint64_t *rsi, int strict)
{
	REP_OUTS_FUNC(uint32_t, 4);
}

int
emulate_rep_inouts(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		 uint64_t *rcx, uint64_t *rxi, int strict)
{
	int out = !in;

	if (in)
		return (-1);
	else if (out && bytes == 1)
		return emulate_rep_outsb(ctx, vcpu, port, rcx, rxi, strict);
	else if (out && bytes == 2)
		return emulate_rep_outsw(ctx, vcpu, port, rcx, rxi, strict);
	else if (out && bytes == 4)
		return emulate_rep_outsd(ctx, vcpu, port, rcx, rxi, strict);
	else
		return (-1);
}

void
init_inout(void)
{
	struct inout_port **iopp, *iop;

	/*
	 * Set up the default handler for all ports
	 */
	register_default_iohandler(0, MAX_IOPORTS);

	/*
	 * Overwrite with specified handlers
	 */
	SET_FOREACH(iopp, inout_port_set) {
		iop = *iopp;
		assert(iop->port < MAX_IOPORTS);
		inout_handlers[iop->port].name = iop->name;
		inout_handlers[iop->port].flags = iop->flags;
		inout_handlers[iop->port].handler = iop->handler;
		inout_handlers[iop->port].arg = NULL;
	}
}

int
register_inout(struct inout_port *iop)
{
	int i;

	VERIFY_IOPORT(iop->port, iop->size);
	
	for (i = iop->port; i < iop->port + iop->size; i++) {
		inout_handlers[i].name = iop->name;
		inout_handlers[i].flags = iop->flags;
		inout_handlers[i].handler = iop->handler;
		inout_handlers[i].arg = iop->arg;
	}

	return (0);
}

int
unregister_inout(struct inout_port *iop)
{

	VERIFY_IOPORT(iop->port, iop->size);
	assert(inout_handlers[iop->port].name == iop->name);

	register_default_iohandler(iop->port, iop->size);

	return (0);
}
