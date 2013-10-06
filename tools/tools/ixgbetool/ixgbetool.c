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
#include <sys/sysctl.h>
#include <ixgbe_ufilter.h>

static void
usage(void)
{
	fprintf(stderr, "Usage: ixgbetool <ifname> [operation]\n");
	fprintf(stderr, "\tadd_sig_filter <proto> <src_ip> <src_port> <dst_ip> <dst_port> <que_index>\n");
	fprintf(stderr, "\tshow_sig_filter\n");
	fprintf(stderr, "\tdel_sig_filter <id>\n");
}

static int 
add_sig_filter(int fd, int argc, char *argv[])
{
	struct ufilter_entry filter;
	int error;

	if (argc != 9) 
		return -1;

	if (!strcmp(argv[3], "tcpv4"))
		filter.proto = UFILTER_PROTO_TCPV4;
	else if (!strcmp(argv[3], "udpv4"))
		filter.proto = UFILTER_PROTO_UDPV4;
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

	error = ioctl(fd, IXGBE_ADD_SIGFILTER, &filter);
	if (error) {
		perror("ixgbetool");
		close(fd);
		exit(1);
	}
	return 0;
}

static inline const char *
filter_proto_str(int proto)
{
	const char *str;

	switch (proto) {
	case UFILTER_PROTO_TCPV4:
		str = "tcpv4";
		break;
	case UFILTER_PROTO_UDPV4:
		str = "udpv4";
		break;
	default:
		str = "(inval)";
	}
	return str;
}

static int 
show_sig_filter(int fd, int argc, char *argv[])
{
	unsigned i;
	unsigned len;
	int error;

	if (argc != 3) 
		return -1;

	error = ioctl(fd, IXGBE_GET_SIGFILTER_LEN, &len);
	if (error) {
		perror("ixgbetool");
		close(fd);
		exit(1);
	}

	for (i = 0; i < len; i++) {
		struct ufilter_entry filter;

		filter.id = i;
		error = ioctl(fd, IXGBE_GET_SIGFILTER, &filter);
		if (error)
			continue;
		printf("id: %u\n", filter.id);
		printf("proto: %s\n", filter_proto_str(filter.proto));
		printf("src_ip: %s\n", inet_ntoa(filter.src_ip));
		printf("src_port: %d\n", filter.src_port);
		printf("dst_ip: %s\n", inet_ntoa(filter.dst_ip));
		printf("dst_port: %d\n", filter.dst_port);
		printf("que_index: %d\n", filter.que_index);
		printf("\n");
	}
	return 0;
}

static int 
del_sig_filter(int fd, int argc, char *argv[])
{
	unsigned id;
	int error;

	if (argc != 4) 
		return -1;

	errno = 0;
	id = strtoul(argv[3], NULL, 0);
	if (errno)
		return errno;

	error = ioctl(fd, IXGBE_CLR_SIGFILTER, &id);
	if (error) {
		perror("ixgbetool");
		close(fd);
		exit(1);
	}
	return 0;
}

int 
main(int argc, char *argv[])
{
	int ret;
	char buf[64];
	int fd;
	int ifno;
	int coop_atr;
	int atr;
	size_t coop_atr_size = sizeof(coop_atr);
	size_t atr_size = sizeof(atr);

	if (argc < 3) {
		usage();
		exit(1);
	}
	snprintf(buf, sizeof(buf), "/dev/%s", argv[1]);
	if ((fd = open(buf, O_RDWR)) < 0) {
		perror("ixgbetool");
		exit(1);
	}
	sscanf(argv[1], "ix%d", &ifno);
	snprintf(buf, sizeof(buf), "dev.ix.%d.cooperative_atr", ifno);
	ret = sysctlbyname(buf, &coop_atr, &coop_atr_size, NULL, 0);
	if (ret) {
		perror("ixgbetool");
		exit(1);
	}
	snprintf(buf, sizeof(buf), "dev.ix.%d.atr_sample_rate", ifno);
	ret = sysctlbyname(buf, &atr, &atr_size, NULL, 0);
	if (ret) {
		perror("ixgbetool");
		exit(1);
	}
	if (!coop_atr && atr) {
		printf("To use user defined filter, you need to add 'hw.ixgbe.cooperative_atr=1' on /boot/loader.conf and reboot\n");
		close(fd);
		exit(1);
	}
	if (!strcmp(argv[2], "add_sig_filter"))
		ret = add_sig_filter(fd, argc, argv);
	else if (!strcmp(argv[2], "show_sig_filter"))
		ret = show_sig_filter(fd, argc, argv);
	else if (!strcmp(argv[2], "del_sig_filter"))
		ret = del_sig_filter(fd, argc, argv);
	else 
		ret = -1;

	if (ret)
		usage();

	close(fd);

	return (ret);
}

