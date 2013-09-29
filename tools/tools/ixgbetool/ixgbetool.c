/**************************************************************************

Copyright (c) 2013, Takuya ASADA.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Chelsio Corporation nor the names of its
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


***************************************************************************/
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <ixgbe_ioctl.h>

static void
usage(void)
{
	fprintf(stderr, "Usage: ixgbetool <ifname> [operation]\n");
	fprintf(stderr, "\tadd_sig_filter <proto> <src_ip> <src_port> <dst_ip> <dst_port> <que_index>\n");
	fprintf(stderr, "\tdel_sig_filter <proto> <src_ip> <src_port> <dst_ip> <dst_port>\n");
}

static int
doit(const char *iff_name, unsigned long cmd, void *data)
{
	int fd = 0;
	int err;
	char buf[64];

	snprintf(buf, 64, "/dev/%s", iff_name);
	printf("iff_name:%s buf:%s\n", iff_name, buf);

	if ((fd = open(buf, O_RDWR)) < 0)
		return -1;
	
	err = ioctl(fd, cmd, data) < 0 ? -1 : 0;
	close(fd);
	return err;
}

static int 
add_sig_filter(int argc, char *argv[], char *ifname)
{
	struct ix_filter filter;
	int error;

	if (argc != 9) 
		return -1;

	if (!strcmp(argv[3], "tcpv4"))
		filter.proto = IXGBE_FILTER_PROTO_TCPV4;
	else if (!strcmp(argv[3], "udpv4"))
		filter.proto = IXGBE_FILTER_PROTO_UDPV4;
	else
		return -1;
	error = inet_aton(argv[4], &filter.src_ip);
	if (error != 1)
		return error;
	errno = 0;
	filter.src_port = strtol(argv[5], NULL, 0);
	if (errno)
		return errno;
	error = inet_aton(argv[6], &filter.dst_ip);
	if (error != 1)
		return error;
	errno = 0;
	filter.dst_port = strtol(argv[7], NULL, 0);
	if (errno)
		return errno;
	errno = 0;
	filter.que_index = strtol(argv[8], NULL, 0);
	if (errno)
		return errno;

	error = doit(ifname, IXGBE_ADD_SIGFILTER, (void *)&filter);
	if (error)
		perror("ioctl");
	return 0;
}

static int 
del_sig_filter(int argc, char *argv[], char *ifname)
{
	struct ix_filter filter;
	int error;

	if (argc != 8) 
		return -1;

	if (!strcmp(argv[3], "tcpv4"))
		filter.proto = IXGBE_FILTER_PROTO_TCPV4;
	else if (!strcmp(argv[3], "udpv4"))
		filter.proto = IXGBE_FILTER_PROTO_UDPV4;
	else
		return -1;
	error = inet_aton(argv[4], &filter.src_ip);
	if (error != 1)
		return error;
	errno = 0;
	filter.src_port = strtol(argv[5], NULL, 0);
	if (errno)
		return errno;
	error = inet_aton(argv[6], &filter.dst_ip);
	if (error != 1)
		return error;
	errno = 0;
	filter.dst_port = strtol(argv[7], NULL, 0);
	if (errno)
		return errno;

	error = doit(ifname, IXGBE_CLR_SIGFILTER, (void *)&filter);
	if (error)
		perror("ioctl");
	return 0;
}

int 
main(int argc, char *argv[])
{
	int ret;
	char *ifname;

	if (argc < 3) {
		usage();
		exit(1);
	}
	ifname = argv[1];
	if (!strcmp(argv[2], "add_sig_filter"))
		ret = add_sig_filter(argc, argv, ifname);
	else if (!strcmp(argv[2], "del_sig_filter"))
		ret = del_sig_filter(argc, argv, ifname);
	else 
		ret = -1;

	if (ret)
		usage();

	return (ret);
}

