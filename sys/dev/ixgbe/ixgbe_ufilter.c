
#include "ixgbe.h"

#ifdef IXGBE_FDIR
static d_open_t ixgbe_ufilter_open;
static d_close_t ixgbe_ufilter_close;
static d_ioctl_t ixgbe_ufilter_ioctl;
static struct cdevsw cdevsw = {
       .d_version =    D_VERSION,
       .d_flags =      0,
       .d_open =       ixgbe_ufilter_open,
       .d_close =      ixgbe_ufilter_close,
       .d_ioctl =      ixgbe_ufilter_ioctl,
       .d_name =       "ix",
};

static int ufilter_hash_size = 65536;
TUNABLE_INT("hw.ixgbe.ufilter_hash_size", &ufilter_hash_size);
int ixgbe_cooperative_atr = 0;
TUNABLE_INT("hw.ixgbe.cooperative_atr", &ixgbe_cooperative_atr);

static inline void
list_insert(struct ixgbe_ufilter *ufilter, struct ufilter_kentry *ke)
{
	TAILQ_INSERT_TAIL(&ufilter->list, ke, tq_link);
}

static inline void
list_remove(struct ixgbe_ufilter *ufilter, struct ufilter_kentry *ke)
{
	TAILQ_REMOVE(&ufilter->list, ke, tq_link);
}

static inline struct ufilter_kentry *
list_find(struct ixgbe_ufilter *ufilter, unsigned id)
{
	struct ufilter_kentry *ke;

	TAILQ_FOREACH(ke, &ufilter->list, tq_link)
		if (ke->e.id == id)
			return (ke);
	return (NULL);
}

static inline void
list_freeall(struct ixgbe_ufilter *ufilter)
{
	struct ufilter_kentry *ke, *t;

	TAILQ_FOREACH_SAFE(ke, &ufilter->list, tq_link, t)
		free(ke, M_DEVBUF);
}

static inline void
hash_insert(struct ixgbe_ufilter *ufilter, struct ufilter_kentry *ke)
{
	int bucket;
	
	bucket = ke->hash & ufilter->hashmask;
	rm_wlock(&ufilter->hashlock);
	LIST_INSERT_HEAD(&ufilter->hash[bucket], ke, li_link);
	rm_wunlock(&ufilter->hashlock);
}

static inline void
hash_remove(struct ixgbe_ufilter *ufilter, struct ufilter_kentry *ke)
{
	int bucket;

	bucket = ke->hash & ufilter->hashmask;
	rm_wlock(&ufilter->hashlock);
	LIST_REMOVE(ke, li_link);
	free(ke, M_DEVBUF);
	rm_wunlock(&ufilter->hashlock);
}

int
ixgbe_ufilter_exists(struct adapter *adapter, u32 hash)
{
	struct ixgbe_ufilter *ufilter = &adapter->ufilter;
	int bucket;
	struct rm_priotracker tracker;
	struct ufilter_kentry *ke;
	int found = 0;

	bucket = hash & ufilter->hashmask;
	rm_rlock(&ufilter->hashlock, &tracker);
	LIST_FOREACH(ke, &ufilter->hash[bucket], li_link) {
		if (ke->hash == hash) {
			found++;
			break;
		}
	}
	rm_runlock(&ufilter->hashlock, &tracker);

	return (found);
}

int
ixgbe_ufilter_attach(struct adapter *adapter)
{
	adapter->ufilter.cdev = make_dev(&cdevsw, adapter->ifp->if_dunit,
	    UID_ROOT, GID_WHEEL, 0600, "%s", if_name(adapter->ifp));
	if (adapter->ufilter.cdev == NULL)
		return (ENOMEM);

	adapter->ufilter.cdev->si_drv1 = (void *)adapter;
		
	mtx_init(&adapter->ufilter.mtx, "ufilter_mtx", NULL, MTX_DEF);
	TAILQ_INIT(&adapter->ufilter.list);
	if (ixgbe_cooperative_atr) {
		adapter->ufilter.hash = hashinit(ufilter_hash_size, M_DEVBUF,
			&adapter->ufilter.hashmask);
		rm_init_flags(&adapter->ufilter.hashlock, "ufilter_hashlock", 
			RM_NOWITNESS);
	}
        SYSCTL_ADD_INT(device_get_sysctl_ctx(adapter->dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(adapter->dev)),
			OID_AUTO, "ufilter_hash_size", CTLTYPE_INT|CTLFLAG_RD,
			&ufilter_hash_size, 20, "ufilter hashtable size");
        SYSCTL_ADD_INT(device_get_sysctl_ctx(adapter->dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(adapter->dev)),
			OID_AUTO, "cooperative_atr", CTLTYPE_INT|CTLFLAG_RD,
			&ixgbe_cooperative_atr, 20, "cooperative ATR");

	return (0);
}

int
ixgbe_ufilter_detach(struct adapter *adapter)
{
	list_freeall(&adapter->ufilter);
	if (ixgbe_cooperative_atr) {
		rm_destroy(&adapter->ufilter.hashlock);
		hashdestroy(adapter->ufilter.hash, M_DEVBUF, 
			adapter->ufilter.hashmask);
	}
	mtx_destroy(&adapter->ufilter.mtx);
	if (adapter->ufilter.cdev)
		destroy_dev(adapter->ufilter.cdev);
	return (0);
}

static int
ixgbe_ufilter_open(struct cdev *dev, int flags, int fmp, struct thread *td)
{
       return (0);
}

static int
ixgbe_ufilter_close(struct cdev *dev, int flags, int fmt, struct thread *td)
{
       return (0);
}

static int
ixgbe_ufilter_ioctl(struct cdev *dev, unsigned long cmd, caddr_t data,
    int fflag, struct thread *td)
{
	struct adapter *adapter = (struct adapter *)dev->si_drv1;
	struct ixgbe_ufilter *ufilter = &adapter->ufilter;
	int error = 0;

	if (priv_check(td, PRIV_DRIVER)) {
		return (EPERM);
	}

	if (!ixgbe_cooperative_atr && ixgbe_atr_sample_rate)
		return (ENODEV);

	mtx_lock(&ufilter->mtx);
	switch (cmd) {
	case IXGBE_ADD_SIGFILTER: {
		struct ufilter_entry *e = (struct ufilter_entry *)data;
		struct ufilter_kentry *ke;
		union ixgbe_atr_hash_dword input = {.dword = 0};
		union ixgbe_atr_hash_dword common = {.dword = 0};

		switch (e->proto) {
		case UFILTER_PROTO_TCPV4:
			input.formatted.flow_type ^= IXGBE_ATR_FLOW_TYPE_TCPV4;
			break;
		case UFILTER_PROTO_UDPV4:
			input.formatted.flow_type ^= IXGBE_ATR_FLOW_TYPE_UDPV4;
			break;
		default:
			error = EINVAL;
			goto out;
		}
		common.port.src ^= htons(e->src_port);
		common.port.dst ^= htons(e->dst_port);
		common.flex_bytes ^= htons(ETHERTYPE_IP);
		common.ip ^= e->src_ip.s_addr ^ e->dst_ip.s_addr;

		ke = malloc(sizeof(*ke), M_DEVBUF, M_NOWAIT | M_ZERO);
		if (!ke) {
			error = ENOMEM;
			goto out;
		}
		memcpy(&ke->e, e, sizeof(ke->e));
		ke->e.id = ufilter->next_id++;
		ke->input = input;
		ke->common = common;
		if (ixgbe_cooperative_atr) {
			ke->hash = ixgbe_atr_compute_sig_hash_82599(input, 
				common);
			hash_insert(ufilter, ke);
		}
		list_insert(ufilter, ke);
		ixgbe_fdir_add_signature_filter_82599(&adapter->hw,
			input, common, e->que_index);
		break;
	}
	case IXGBE_GET_SIGFILTER: {
		struct ufilter_entry *e = (struct ufilter_entry *)data;
		struct ufilter_kentry *ke;

		ke = list_find(ufilter, e->id);
		if (ke)
			memcpy(e, &ke->e, sizeof(*e));
		else
			error = ENOENT;
		break;
	};
	case IXGBE_CLR_SIGFILTER: {
		unsigned *id = (unsigned *)data;
		struct ufilter_kentry *ke;

		ke = list_find(ufilter, *id);
		if (!ke) {
			error = ENOENT;
			goto out;
		}

		if (ixgbe_cooperative_atr)
			hash_remove(ufilter, ke);
		list_remove(ufilter, ke);

		ixgbe_fdir_erase_signature_filter_82599(&adapter->hw,
			ke->input, ke->common);
		break;
	}
	case IXGBE_GET_SIGFILTER_LEN: {
		unsigned *id = (unsigned *)data;
		
		*id = ufilter->next_id;
		break;
	}
	default:
		error = EOPNOTSUPP;
		break;
	}

out:
	mtx_unlock(&ufilter->mtx);
	return (error);
}
#endif /* IXGBE_FDIR */
