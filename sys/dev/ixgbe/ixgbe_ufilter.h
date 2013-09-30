/**************************************************************************

Copyright (c) 2013 Takuya ASADA
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

$FreeBSD$

***************************************************************************/

#ifndef __IXGBEUFILTER_H__
#define __IXGBEUFILTER_H__

enum {
	UFILTER_PROTO_TCPV4,
	UFILTER_PROTO_UDPV4
};

struct ufilter_entry {
	unsigned id;
	int proto;
	struct in_addr src_ip;
	int src_port;
	struct in_addr dst_ip;
	int dst_port;
	int que_index;
};

#define IXGBE_ADD_SIGFILTER	_IOW('i', 0x0, struct ufilter_entry)
#define IXGBE_GET_SIGFILTER	_IOWR('i', 0x1, struct ufilter_entry)
#define IXGBE_CLR_SIGFILTER	_IOW('i', 0x2, unsigned)
#define IXGBE_GET_SIGFILTER_LEN _IOR('i', 0x3, unsigned)

#ifdef _KERNEL
#ifdef IXGBE_FDIR
#include <sys/rmlock.h>

struct ufilter_kentry {
	struct ufilter_entry e;
	TAILQ_ENTRY(ufilter_kentry) tq_link;
	LIST_ENTRY(ufilter_kentry) li_link;
	union ixgbe_atr_hash_dword input;
	union ixgbe_atr_hash_dword common;
	u32 hash;
};

struct ixgbe_ufilter {
	struct cdev		*cdev;
	TAILQ_HEAD(, ufilter_kentry) list;
	struct mtx		mtx;
	unsigned		next_id;
	LIST_HEAD(, ufilter_kentry) *hash;
	u_long			hashmask;
	struct rmlock		hashlock;
};

struct adapter;
int ixgbe_ufilter_attach(struct adapter *adapter);
int ixgbe_ufilter_detach(struct adapter *adapter);
extern int ixgbe_cooperative_atr;
int ixgbe_ufilter_exists(struct adapter *adapter, u32 hash);
#endif /* IXGBE_FDIR */
#endif /* _KERNEL */
#endif /* __IXGBEUFILTER_H__ */

