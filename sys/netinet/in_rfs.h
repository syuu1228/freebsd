#ifndef _NETINET_IN_RFS_H_
#define _NETINET_IN_RFS_H_

#include <netinet/in.h>         /* in_addr_t */
#include <sys/sockbuf.h>        /* struct sockbuf */

#define NO_CURR_CPU 0xffffffff
#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_RFS_FLOWS);
#endif

void rfs_record_curcpu(struct sockbuf *sb);
void rfs_dec_flow_qlen(u_int flowid);
struct mbuf *rfs_m2cpuid(struct mbuf *m, uintptr_t source, u_int *cpuid);

#endif /* !_NETINET_IN_RFS_H_ */
