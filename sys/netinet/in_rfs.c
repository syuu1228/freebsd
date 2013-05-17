#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#include "opt_rfs.h"


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/refcount.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/smp.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include <vm/uma.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_rfs.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#endif /* INET6 */


 
struct netisr_flow{
	uint16_t cpu;
	unsigned qlen;
};

static struct netisr_flow *netisr_flow_table;
static unsigned *socket_flow_table;


MALLOC_DEFINE(M_RFS_FLOWS, "rfs", "rfs flow entrys");
SYSCTL_NODE(_net_inet, OID_AUTO, rfs, CTLFLAG_RW, 0, "Receive flow steering");

static int	rfs_maxflows = 128;
TUNABLE_INT("net.inet.rfs.maxflows", &rfs_maxflows);
SYSCTL_INT(_net_inet_rfs, OID_AUTO, maxflows, CTLFLAG_RDTUN,
    &rfs_maxflows, 0, "Flow entrys using RFS.");

static void
socket_flow_table_init(void)
{
	int i;

	socket_flow_table = (unsigned *)malloc(sizeof(unsigned) * rfs_maxflows, 
					       M_RFS_FLOWS, M_NOWAIT);
	
	if (socket_flow_table == NULL)
		panic("not allocate memory for soft rss");

	for (i = 0; i < rfs_maxflows; i++) 
		socket_flow_table[i] = NO_CURR_CPU;
}
SYSINIT(scoket_flow_table_init, SI_SUB_CLOCKS, SI_ORDER_MIDDLE, socket_flow_table_init, NULL);

static void
netisr_flow_table_init(void)
{
	int i;

	netisr_flow_table = (struct netisr_flow *)malloc(sizeof(struct netisr_flow) * rfs_maxflows, 
							 M_RFS_FLOWS, M_NOWAIT);

	if (netisr_flow_table == NULL)
		panic("not allocate memory for RFS");

	for (i = 0; i < rfs_maxflows; i++) {
		netisr_flow_table[i].cpu = 0;
		netisr_flow_table[i].qlen = 0;
	}
}
SYSINIT(netisr_flow_table_init, SI_SUB_CLOCKS, SI_ORDER_MIDDLE, netisr_flow_table_init, NULL);

void
rfs_record_curcpu(struct sockbuf *sb)
{
	if (sb->flowid)
		atomic_store_rel_int(&socket_flow_table[sb->flowid % rfs_maxflows], curcpu);
}

static inline int
rfs_get_curcpu(int index)
{
	return(atomic_load_acq_int(&socket_flow_table[index]));
}

static inline void
rfs_record_dstcpu(int index, uint16_t cpu)
{
	atomic_store_rel_16(&netisr_flow_table[index].cpu, cpu);
}

static inline uint16_t
rfs_get_dstcpu(int index)
{
	return(atomic_load_acq_16(&netisr_flow_table[index].cpu));
}


static inline void
rfs_inc_flow_qlen(int index)
{
	atomic_add_acq_int(&netisr_flow_table[index].qlen, 1);
}

void
rfs_dec_flow_qlen(unsigned flowid)
{
	atomic_subtract_acq_int(&netisr_flow_table[flowid % rfs_maxflows].qlen, 1);
}	

static inline int
rfs_get_flow_qlen(int index)
{
	return(atomic_load_acq_int(&netisr_flow_table[index].qlen));
}

static u_int
rfs_getcpu(u_int flowid)
{
        int index;
	u_int cur, dst, qlen;

	index = flowid % rfs_maxflows;
	cur = rfs_get_curcpu(index);
	dst = rfs_get_dstcpu(index);
	qlen = rfs_get_flow_qlen(index);

	if (cur == NO_CURR_CPU){
		cur = netisr_default_flow2cpu(flowid);
		rfs_record_dstcpu(index, (uint16_t)cur);
	}
	else if (cur != dst){
		if (qlen == 0)
			rfs_record_dstcpu(index, (uint16_t)cur);
		else
			cur = dst;
	}
	rfs_inc_flow_qlen(index);
	return ((u_int)cur);
}

/*
 * netisr CPU affinity lookup routine for use by flowid.
 */
struct mbuf *
rfs_m2cpuid(struct mbuf *m, uintptr_t source, u_int *cpuid)
{
	*cpuid = rfs_getcpu(m->m_pkthdr.flowid);
	return (m);
}
