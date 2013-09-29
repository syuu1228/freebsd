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

#ifndef __IXGBEIOCTL_H__
#define __IXGBEIOCTL_H__

enum {
	IX_ADD_SIGFILTER = 0x0,
	IX_CLR_SIGFILTER
};

enum {
	IXGBE_FILTER_PROTO_TCPV4,
	IXGBE_FILTER_PROTO_UDPV4
};

struct ix_filter {
	int proto;
	struct in_addr src_ip;
	int src_port;
	struct in_addr dst_ip;
	int dst_port;
	int que_index;
};

#define IXGBE_ADD_SIGFILTER	_IOW('i', IX_ADD_SIGFILTER, struct ix_filter)
#define IXGBE_CLR_SIGFILTER	_IOW('i', IX_CLR_SIGFILTER, struct ix_filter)

#endif

