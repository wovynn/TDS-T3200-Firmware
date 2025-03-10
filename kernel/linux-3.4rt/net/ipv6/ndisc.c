/*
 *	Neighbour Discovery for IPv6
 *	Linux INET6 implementation
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>
 *	Mike Shaver		<shaver@ingenia.com>
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

/*
 *	Changes:
 *
 *	Pierre Ynard			:	export userland ND options
 *						through netlink (RDNSS support)
 *	Lars Fenneberg			:	fixed MTU setting on receipt
 *						of an RA.
 *	Janos Farkas			:	kmalloc failure checks
 *	Alexey Kuznetsov		:	state machine reworked
 *						and moved to net/core.
 *	Pekka Savola			:	RFC2461 validation
 *	YOSHIFUJI Hideaki @USAGI	:	Verify ND options properly
 */

/* Set to 3 to get tracing... */
#define ND_DEBUG 1

#define ND_PRINTK(fmt, args...) do { if (net_ratelimit()) { printk(fmt, ## args); } } while(0)
#define ND_NOPRINTK(x...) do { ; } while(0)
#define ND_PRINTK0 ND_PRINTK
#define ND_PRINTK1 ND_NOPRINTK
#define ND_PRINTK2 ND_NOPRINTK
#define ND_PRINTK3 ND_NOPRINTK
#if ND_DEBUG >= 1
#undef ND_PRINTK1
#define ND_PRINTK1 ND_PRINTK
#endif
#if ND_DEBUG >= 2
#undef ND_PRINTK2
#define ND_PRINTK2 ND_PRINTK
#endif
#if ND_DEBUG >= 3
#undef ND_PRINTK3
#define ND_PRINTK3 ND_PRINTK
#endif

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/sched.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/route.h>
#include <linux/init.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif

#include <linux/if_addr.h>
#include <linux/if_arp.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>
#include <linux/jhash.h>

#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/ndisc.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/icmp.h>

#include <net/netlink.h>
#include <linux/rtnetlink.h>

#include <net/flow.h>
#include <net/ip6_checksum.h>
#include <net/inet_common.h>
#include <linux/proc_fs.h>

#include <linux/netfilter.h>
#include <linux/netfilter_ipv6.h>

static u32 ndisc_hash(const void *pkey,
		      const struct net_device *dev,
		      __u32 *hash_rnd);
static int ndisc_constructor(struct neighbour *neigh);
static void ndisc_solicit(struct neighbour *neigh, struct sk_buff *skb);
static void ndisc_error_report(struct neighbour *neigh, struct sk_buff *skb);
static int pndisc_constructor(struct pneigh_entry *n);
static void pndisc_destructor(struct pneigh_entry *n);
static void pndisc_redo(struct sk_buff *skb);

static const struct neigh_ops ndisc_generic_ops = {
	.family =		AF_INET6,
	.solicit =		ndisc_solicit,
	.error_report =		ndisc_error_report,
	.output =		neigh_resolve_output,
	.connected_output =	neigh_connected_output,
};

static const struct neigh_ops ndisc_hh_ops = {
	.family =		AF_INET6,
	.solicit =		ndisc_solicit,
	.error_report =		ndisc_error_report,
	.output =		neigh_resolve_output,
	.connected_output =	neigh_resolve_output,
};


static const struct neigh_ops ndisc_direct_ops = {
	.family =		AF_INET6,
	.output =		neigh_direct_output,
	.connected_output =	neigh_direct_output,
};

struct neigh_table nd_tbl = {
	.family =	AF_INET6,
	.key_len =	sizeof(struct in6_addr),
	.hash =		ndisc_hash,
	.constructor =	ndisc_constructor,
	.pconstructor =	pndisc_constructor,
	.pdestructor =	pndisc_destructor,
	.proxy_redo =	pndisc_redo,
	.id =		"ndisc_cache",
	.parms = {
		.tbl			= &nd_tbl,
		.base_reachable_time	= ND_REACHABLE_TIME,
		.retrans_time		= ND_RETRANS_TIMER,
		.gc_staletime		= 60 * HZ,
		.reachable_time		= ND_REACHABLE_TIME,
		.delay_probe_time	= 5 * HZ,
		.queue_len_bytes	= 64*1024,
		.ucast_probes		= 3,
		.mcast_probes		= 3,
		.anycast_delay		= 1 * HZ,
		.proxy_delay		= (8 * HZ) / 10,
		.proxy_qlen		= 64,
	},
	.gc_interval =	  30 * HZ,
	.gc_thresh1 =	 128,
	.gc_thresh2 =	 512,
	.gc_thresh3 =	1024,
};

#ifdef SUPPORT_GPL_9
/* RDNSS option */
struct rdnss {
    __u8            type;
    __u8            length;
    __u16           reserved;
    __be32          lifetime;
    struct in6_addr prefix[0];  /* (length-1)/2 */
};

struct rdnss_msg {
        u8 flag;
        u8 length;
        struct in6_addr dnsServer[2];
};
static struct rdnss_msg message_dns;
#define SEND_RDNSS_COUNT  3
static int resend_count;
#endif

/* ND options */
struct ndisc_options {
#ifdef SUPPORT_GPL_9
	struct nd_opt_hdr *nd_opt_array[__ND_OPT_MAX];
#else
	struct nd_opt_hdr *nd_opt_array[__ND_OPT_ARRAY_MAX];
#endif
#ifdef CONFIG_IPV6_ROUTE_INFO
	struct nd_opt_hdr *nd_opts_ri;
	struct nd_opt_hdr *nd_opts_ri_end;
#endif
#ifdef SUPPORT_GPL_9
    struct nd_opt_hdr *nd_opts_rd;
    struct nd_opt_hdr *nd_opts_rd_end;
#endif
	struct nd_opt_hdr *nd_useropts;
	struct nd_opt_hdr *nd_useropts_end;
};

#define nd_opts_src_lladdr	nd_opt_array[ND_OPT_SOURCE_LL_ADDR]
#define nd_opts_tgt_lladdr	nd_opt_array[ND_OPT_TARGET_LL_ADDR]
#define nd_opts_pi		nd_opt_array[ND_OPT_PREFIX_INFO]
#define nd_opts_pi_end		nd_opt_array[__ND_OPT_PREFIX_INFO_END]
#define nd_opts_rh		nd_opt_array[ND_OPT_REDIRECT_HDR]
#define nd_opts_mtu		nd_opt_array[ND_OPT_MTU]

#define NDISC_OPT_SPACE(len) (((len)+2+7)&~7)
#ifdef  ACTION_TEC_IPV6_CODE_FOR_IOT

#define MAX_PAYLOAD 1024

static struct sock *ndisc_nl_sk = NULL;
#ifdef SUPPORT_GPL_9
static struct sock *ndisc_nl_sk_rdnss = NULL;
#endif


#define RESEND_TIMES	(200)
#define RESEND_DURA		(HZ/2)

static void broadcast_resend(unsigned long);
static int broadcast_resend_times=RESEND_TIMES;
static DEFINE_TIMER(resend_timer, broadcast_resend, 0, 0);
struct ra_info_msg {
        u32 u_ra;
        struct in6_addr addr;
        char name[IFNAMSIZ];
};
static struct ra_info_msg message_val;


static void skb_ndisc_rcv(struct sk_buff *skb)
{
	printk(KERN_DEBUG "received sth\n");
	return;
}


#ifdef SUPPORT_GPL_9
static int send_dnsinfo_message_to_userspace(struct rdnss_msg *flag) {
    struct nlmsghdr *nlh;
	int ret=0;
	struct sk_buff *skb = NULL;
	struct rdnss_msg *intp;

	if (!ndisc_nl_sk_rdnss) {
		ndisc_nl_sk_rdnss = netlink_kernel_create(&init_net,NETLINK_RAD_RDNSS, 0, skb_ndisc_rcv,NULL,THIS_MODULE);
		if (!ndisc_nl_sk_rdnss) {
			printk(KERN_ERR "Create netlink error\n");
			return -1;
		}
	}

    if (!netlink_has_listeners(ndisc_nl_sk_rdnss, 1))
        return -ESRCH;

	skb=alloc_skb(NLMSG_SPACE(sizeof(struct rdnss_msg)),GFP_ATOMIC);
	if (!skb) {
		printk(KERN_ERR "alloc skb error\n");
		return -1;
	}

	nlh = NLMSG_PUT(skb, 0, 0, NLMSG_DONE, sizeof(struct rdnss_msg));
	intp = (struct rdnss_msg *)NLMSG_DATA(nlh);
    if(flag)
	    memcpy((void *)intp,(void *)flag, sizeof(struct rdnss_msg));

    nlh->nlmsg_pid = 0;  /* from kernel */
    nlh->nlmsg_flags = 0;

	/* sender is in group 1<<0 */

    NETLINK_CB(skb).pid = 0;  		/* from kernel */
    //NETLINK_CB(skb).dst_pid = 0;  	/* multicast */
    /* to mcast group 1<<0 */
    NETLINK_CB(skb).dst_group = 1;

    /*multicast the message to all listening processes*/
    ret = netlink_broadcast(ndisc_nl_sk_rdnss, skb, 0, 1, GFP_ATOMIC);
    //printk(KERN_DEBUG "### %d:%s, send message to userspace, %d ###\n", __LINE__, __FUNCTION__, ret);
    if (0 == ret)
        resend_count = 0;
	return ret;
nlmsg_failure:
	kfree_skb(skb);
	return -EINVAL;
}
#endif
static int send_netlink_message_to_userspace(struct ra_info_msg *flag) {
    struct nlmsghdr *nlh;
	int ret=0;
	struct sk_buff *skb = NULL;
	struct ra_info_msg *intp;

	if (!ndisc_nl_sk) {
		ndisc_nl_sk = netlink_kernel_create(&init_net,NETLINK_RAD, 0, skb_ndisc_rcv,NULL,THIS_MODULE);
		if (!ndisc_nl_sk) {
			printk(KERN_ERR "Create netlink error\n");
			return -1;
		}
	}

	skb=alloc_skb(NLMSG_SPACE(sizeof(struct ra_info_msg)),GFP_ATOMIC);
	if (!skb) {
		printk(KERN_ERR "alloc skb error\n");
		return -1;
	}

	nlh = NLMSG_PUT(skb, 0, 0, NLMSG_DONE, sizeof(struct ra_info_msg));
	intp = (struct ra_info_msg *)NLMSG_DATA(nlh);
    if(flag)
	    memcpy((void *)intp,(void *)flag, sizeof(struct ra_info_msg));

    nlh->nlmsg_pid = 0;  /* from kernel */
    nlh->nlmsg_flags = 0;

	/* sender is in group 1<<0 */

    NETLINK_CB(skb).pid = 0;  		/* from kernel */
    //NETLINK_CB(skb).dst_pid = 0;  	/* multicast */
    /* to mcast group 1<<0 */
    NETLINK_CB(skb).dst_group = 1;

    /*multicast the message to all listening processes*/
    ret = netlink_broadcast(ndisc_nl_sk, skb, 0, 1, GFP_ATOMIC);
	if (ret < 0) {
		if (broadcast_resend_times>0) {
			//printk(KERN_DEBUG "Need redo broadcast start a timer\n");

			resend_timer.expires = jiffies + RESEND_DURA;
			resend_timer.data = (unsigned long)flag;

			mod_timer(&resend_timer,resend_timer.expires);
			broadcast_resend_times--;
		}
	}
	else if( timer_pending(&resend_timer)) {
		del_timer(&resend_timer);
		//printk(KERN_DEBUG "Sent succ,need delete the timer in pending");
	}

	//printk(KERN_DEBUG "Sending msg ,ret is %d,"
    //         "flag is 0x%08x\n",ret,*flag);

	return ret;
nlmsg_failure:
	kfree_skb(skb);
	return -EINVAL;
}
static void broadcast_resend(unsigned long param)
{
	struct ra_info_msg *flag;
	if (!param) {
		printk(KERN_NOTICE "Param in broadcast time func is null,should be a bug\n");
		return;
	}
	flag=(struct ra_info_msg *)param;
	send_netlink_message_to_userspace(flag);
	return;
}

#endif

/*
 * Return the padding between the option length and the start of the
 * link addr.  Currently only IP-over-InfiniBand needs this, although
 * if RFC 3831 IPv6-over-Fibre Channel is ever implemented it may
 * also need a pad of 2.
 */
static int ndisc_addr_option_pad(unsigned short type)
{
	switch (type) {
	case ARPHRD_INFINIBAND: return 2;
	default:                return 0;
	}
}

static inline int ndisc_opt_addr_space(struct net_device *dev)
{
	return NDISC_OPT_SPACE(dev->addr_len + ndisc_addr_option_pad(dev->type));
}

static u8 *ndisc_fill_addr_option(u8 *opt, int type, void *data, int data_len,
				  unsigned short addr_type)
{
	int space = NDISC_OPT_SPACE(data_len);
	int pad   = ndisc_addr_option_pad(addr_type);

	opt[0] = type;
	opt[1] = space>>3;

	memset(opt + 2, 0, pad);
	opt   += pad;
	space -= pad;

	memcpy(opt+2, data, data_len);
	data_len += 2;
	opt += data_len;
	if ((space -= data_len) > 0)
		memset(opt, 0, space);
	return opt + space;
}

static struct nd_opt_hdr *ndisc_next_option(struct nd_opt_hdr *cur,
					    struct nd_opt_hdr *end)
{
	int type;
	if (!cur || !end || cur >= end)
		return NULL;
	type = cur->nd_opt_type;
	do {
		cur = ((void *)cur) + (cur->nd_opt_len << 3);
	} while(cur < end && cur->nd_opt_type != type);
	return cur <= end && cur->nd_opt_type == type ? cur : NULL;
}

static inline int ndisc_is_useropt(struct nd_opt_hdr *opt)
{
	return opt->nd_opt_type == ND_OPT_RDNSS;
}

static struct nd_opt_hdr *ndisc_next_useropt(struct nd_opt_hdr *cur,
					     struct nd_opt_hdr *end)
{
	if (!cur || !end || cur >= end)
		return NULL;
	do {
		cur = ((void *)cur) + (cur->nd_opt_len << 3);
	} while(cur < end && !ndisc_is_useropt(cur));
	return cur <= end && ndisc_is_useropt(cur) ? cur : NULL;
}

static struct ndisc_options *ndisc_parse_options(u8 *opt, int opt_len,
						 struct ndisc_options *ndopts)
{
	struct nd_opt_hdr *nd_opt = (struct nd_opt_hdr *)opt;

	if (!nd_opt || opt_len < 0 || !ndopts)
		return NULL;
	memset(ndopts, 0, sizeof(*ndopts));
	while (opt_len) {
		int l;
		if (opt_len < sizeof(struct nd_opt_hdr))
			return NULL;
		l = nd_opt->nd_opt_len << 3;
		if (opt_len < l || l == 0)
			return NULL;
		switch (nd_opt->nd_opt_type) {
		case ND_OPT_SOURCE_LL_ADDR:
		case ND_OPT_TARGET_LL_ADDR:
		case ND_OPT_MTU:
		case ND_OPT_REDIRECT_HDR:
			if (ndopts->nd_opt_array[nd_opt->nd_opt_type]) {
				ND_PRINTK2(KERN_WARNING
					   "%s(): duplicated ND6 option found: type=%d\n",
					   __func__,
					   nd_opt->nd_opt_type);
			} else {
				ndopts->nd_opt_array[nd_opt->nd_opt_type] = nd_opt;
			}
			break;
		case ND_OPT_PREFIX_INFO:
			ndopts->nd_opts_pi_end = nd_opt;
			if (!ndopts->nd_opt_array[nd_opt->nd_opt_type])
				ndopts->nd_opt_array[nd_opt->nd_opt_type] = nd_opt;
			break;
#ifdef CONFIG_IPV6_ROUTE_INFO
		case ND_OPT_ROUTE_INFO:
			ndopts->nd_opts_ri_end = nd_opt;
			if (!ndopts->nd_opts_ri)
				ndopts->nd_opts_ri = nd_opt;
			break;
#endif
#ifdef SUPPORT_GPL_9
        case ND_OPT_RDNSS:
            ndopts->nd_opts_rd_end = nd_opt;
            if (!ndopts->nd_opts_rd)
            {
				ndopts->nd_opts_rd = nd_opt;
            }
            break;
#endif
		default:
			if (ndisc_is_useropt(nd_opt)) {
				ndopts->nd_useropts_end = nd_opt;
				if (!ndopts->nd_useropts)
					ndopts->nd_useropts = nd_opt;
			} else {
				/*
				 * Unknown options must be silently ignored,
				 * to accommodate future extension to the
				 * protocol.
				 */
				ND_PRINTK2(KERN_NOTICE
					   "%s(): ignored unsupported option; type=%d, len=%d\n",
					   __func__,
					   nd_opt->nd_opt_type, nd_opt->nd_opt_len);
			}
		}
		opt_len -= l;
		nd_opt = ((void *)nd_opt) + l;
	}
	return ndopts;
}

static inline u8 *ndisc_opt_addr_data(struct nd_opt_hdr *p,
				      struct net_device *dev)
{
	u8 *lladdr = (u8 *)(p + 1);
	int lladdrlen = p->nd_opt_len << 3;
	int prepad = ndisc_addr_option_pad(dev->type);
	if (lladdrlen != NDISC_OPT_SPACE(dev->addr_len + prepad))
		return NULL;
	return lladdr + prepad;
}

int ndisc_mc_map(const struct in6_addr *addr, char *buf, struct net_device *dev, int dir)
{
	switch (dev->type) {
	case ARPHRD_ETHER:
	case ARPHRD_IEEE802:	/* Not sure. Check it later. --ANK */
	case ARPHRD_FDDI:
		ipv6_eth_mc_map(addr, buf);
		return 0;
	case ARPHRD_IEEE802_TR:
		ipv6_tr_mc_map(addr,buf);
		return 0;
	case ARPHRD_ARCNET:
		ipv6_arcnet_mc_map(addr, buf);
		return 0;
	case ARPHRD_INFINIBAND:
		ipv6_ib_mc_map(addr, dev->broadcast, buf);
		return 0;
	case ARPHRD_IPGRE:
		return ipv6_ipgre_mc_map(addr, dev->broadcast, buf);
	default:
		if (dir) {
			memcpy(buf, dev->broadcast, dev->addr_len);
			return 0;
		}
	}
	return -EINVAL;
}

EXPORT_SYMBOL(ndisc_mc_map);

static u32 ndisc_hash(const void *pkey,
		      const struct net_device *dev,
		      __u32 *hash_rnd)
{
	return ndisc_hashfn(pkey, dev, hash_rnd);
}

static int ndisc_constructor(struct neighbour *neigh)
{
	struct in6_addr *addr = (struct in6_addr*)&neigh->primary_key;
	struct net_device *dev = neigh->dev;
	struct inet6_dev *in6_dev;
	struct neigh_parms *parms;
	int is_multicast = ipv6_addr_is_multicast(addr);

	in6_dev = in6_dev_get(dev);
	if (in6_dev == NULL) {
		return -EINVAL;
	}

	parms = in6_dev->nd_parms;
	__neigh_parms_put(neigh->parms);
	neigh->parms = neigh_parms_clone(parms);

	neigh->type = is_multicast ? RTN_MULTICAST : RTN_UNICAST;
	if (!dev->header_ops) {
		neigh->nud_state = NUD_NOARP;
		neigh->ops = &ndisc_direct_ops;
		neigh->output = neigh_direct_output;
	} else {
		if (is_multicast) {
			neigh->nud_state = NUD_NOARP;
			ndisc_mc_map(addr, neigh->ha, dev, 1);
		} else if (dev->flags&(IFF_NOARP|IFF_LOOPBACK)) {
			neigh->nud_state = NUD_NOARP;
			memcpy(neigh->ha, dev->dev_addr, dev->addr_len);
			if (dev->flags&IFF_LOOPBACK)
				neigh->type = RTN_LOCAL;
		} else if (dev->flags&IFF_POINTOPOINT) {
			neigh->nud_state = NUD_NOARP;
			memcpy(neigh->ha, dev->broadcast, dev->addr_len);
		}
		if (dev->header_ops->cache)
			neigh->ops = &ndisc_hh_ops;
		else
			neigh->ops = &ndisc_generic_ops;
		if (neigh->nud_state&NUD_VALID)
			neigh->output = neigh->ops->connected_output;
		else
			neigh->output = neigh->ops->output;
	}
	in6_dev_put(in6_dev);
	return 0;
}

static int pndisc_constructor(struct pneigh_entry *n)
{
	struct in6_addr *addr = (struct in6_addr*)&n->key;
	struct in6_addr maddr;
	struct net_device *dev = n->dev;

	if (dev == NULL || __in6_dev_get(dev) == NULL)
		return -EINVAL;
	addrconf_addr_solict_mult(addr, &maddr);
	ipv6_dev_mc_inc(dev, &maddr);
	return 0;
}

static void pndisc_destructor(struct pneigh_entry *n)
{
	struct in6_addr *addr = (struct in6_addr*)&n->key;
	struct in6_addr maddr;
	struct net_device *dev = n->dev;

	if (dev == NULL || __in6_dev_get(dev) == NULL)
		return;
	addrconf_addr_solict_mult(addr, &maddr);
	ipv6_dev_mc_dec(dev, &maddr);
}

struct sk_buff *ndisc_build_skb(struct net_device *dev,
				const struct in6_addr *daddr,
				const struct in6_addr *saddr,
				struct icmp6hdr *icmp6h,
				const struct in6_addr *target,
				int llinfo)
{
	struct net *net = dev_net(dev);
	struct sock *sk = net->ipv6.ndisc_sk;
	struct sk_buff *skb;
	struct icmp6hdr *hdr;
	int hlen = LL_RESERVED_SPACE(dev);
	int tlen = dev->needed_tailroom;
	int len;
	int err;
	u8 *opt;

	if (!dev->addr_len)
		llinfo = 0;

	len = sizeof(struct icmp6hdr) + (target ? sizeof(*target) : 0);
	if (llinfo)
		len += ndisc_opt_addr_space(dev);

	skb = sock_alloc_send_skb(sk,
				  (MAX_HEADER + sizeof(struct ipv6hdr) +
				   len + hlen + tlen),
				  1, &err);
	if (!skb) {
		ND_PRINTK0(KERN_ERR
			   "ICMPv6 ND: %s() failed to allocate an skb, err=%d.\n",
			   __func__, err);
		return NULL;
	}

	skb_reserve(skb, hlen);
	ip6_nd_hdr(sk, skb, dev, saddr, daddr, IPPROTO_ICMPV6, len);

	skb->transport_header = skb->tail;
	skb_put(skb, len);

	hdr = (struct icmp6hdr *)skb_transport_header(skb);
	memcpy(hdr, icmp6h, sizeof(*hdr));

	opt = skb_transport_header(skb) + sizeof(struct icmp6hdr);
	if (target) {
		*(struct in6_addr *)opt = *target;
		opt += sizeof(*target);
	}

	if (llinfo)
		ndisc_fill_addr_option(opt, llinfo, dev->dev_addr,
				       dev->addr_len, dev->type);

	hdr->icmp6_cksum = csum_ipv6_magic(saddr, daddr, len,
					   IPPROTO_ICMPV6,
					   csum_partial(hdr,
							len, 0));

	return skb;
}

EXPORT_SYMBOL(ndisc_build_skb);

void ndisc_send_skb(struct sk_buff *skb,
		    struct net_device *dev,
		    struct neighbour *neigh,
		    const struct in6_addr *daddr,
		    const struct in6_addr *saddr,
		    struct icmp6hdr *icmp6h)
{
	struct flowi6 fl6;
	struct dst_entry *dst;
	struct net *net = dev_net(dev);
	struct sock *sk = net->ipv6.ndisc_sk;
	struct inet6_dev *idev;
	int err;
	u8 type;

	type = icmp6h->icmp6_type;

	icmpv6_flow_init(sk, &fl6, type, saddr, daddr, dev->ifindex);
	dst = icmp6_dst_alloc(dev, neigh, &fl6);
	if (IS_ERR(dst)) {
		kfree_skb(skb);
		return;
	}

	skb_dst_set(skb, dst);

	rcu_read_lock();
	idev = __in6_dev_get(dst->dev);
	IP6_UPD_PO_STATS(net, idev, IPSTATS_MIB_OUT, skb->len);

	err = NF_HOOK(NFPROTO_IPV6, NF_INET_LOCAL_OUT, skb, NULL, dst->dev,
		      dst_output);
	if (!err) {
		ICMP6MSGOUT_INC_STATS(net, idev, type);
		ICMP6_INC_STATS(net, idev, ICMP6_MIB_OUTMSGS);
	}

	rcu_read_unlock();
}

EXPORT_SYMBOL(ndisc_send_skb);

/*
 *	Send a Neighbour Discover packet
 */
static void __ndisc_send(struct net_device *dev,
			 struct neighbour *neigh,
			 const struct in6_addr *daddr,
			 const struct in6_addr *saddr,
			 struct icmp6hdr *icmp6h, const struct in6_addr *target,
			 int llinfo)
{
	struct sk_buff *skb;

	skb = ndisc_build_skb(dev, daddr, saddr, icmp6h, target, llinfo);
	if (!skb)
		return;

	ndisc_send_skb(skb, dev, neigh, daddr, saddr, icmp6h);
}

static void ndisc_send_na(struct net_device *dev, struct neighbour *neigh,
			  const struct in6_addr *daddr,
			  const struct in6_addr *solicited_addr,
			  int router, int solicited, int override, int inc_opt)
{
	struct in6_addr tmpaddr;
	struct inet6_ifaddr *ifp;
	const struct in6_addr *src_addr;
	struct icmp6hdr icmp6h = {
		.icmp6_type = NDISC_NEIGHBOUR_ADVERTISEMENT,
	};

	/* for anycast or proxy, solicited_addr != src_addr */
	ifp = ipv6_get_ifaddr(dev_net(dev), solicited_addr, dev, 1);
	if (ifp) {
		src_addr = solicited_addr;
		if (ifp->flags & IFA_F_OPTIMISTIC)
			override = 0;
		inc_opt |= ifp->idev->cnf.force_tllao;
		in6_ifa_put(ifp);
	} else {
		if (ipv6_dev_get_saddr(dev_net(dev), dev, daddr,
				       inet6_sk(dev_net(dev)->ipv6.ndisc_sk)->srcprefs,
				       &tmpaddr))
			return;
		src_addr = &tmpaddr;
	}

	icmp6h.icmp6_router = router;
	icmp6h.icmp6_solicited = solicited;
	icmp6h.icmp6_override = override;

	__ndisc_send(dev, neigh, daddr, src_addr,
		     &icmp6h, solicited_addr,
		     inc_opt ? ND_OPT_TARGET_LL_ADDR : 0);
}

static void ndisc_send_unsol_na(struct net_device *dev)
{
	struct inet6_dev *idev;
	struct inet6_ifaddr *ifa;
#if !defined(CONFIG_BCM_KF_ANDROID) || !defined(CONFIG_BCM_ANDROID)
	struct in6_addr mcaddr;
#else
	struct in6_addr mcaddr = IN6ADDR_LINKLOCAL_ALLNODES_INIT;
#endif

	idev = in6_dev_get(dev);
	if (!idev)
		return;

	read_lock_bh(&idev->lock);
	list_for_each_entry(ifa, &idev->addr_list, if_list) {
#if !defined(CONFIG_BCM_KF_ANDROID) || !defined(CONFIG_BCM_ANDROID)
		addrconf_addr_solict_mult(&ifa->addr, &mcaddr);
#endif
		ndisc_send_na(dev, NULL, &mcaddr, &ifa->addr,
			      /*router=*/ !!idev->cnf.forwarding,
			      /*solicited=*/ false, /*override=*/ true,
			      /*inc_opt=*/ true);
	}
	read_unlock_bh(&idev->lock);

	in6_dev_put(idev);
}

void ndisc_send_ns(struct net_device *dev, struct neighbour *neigh,
		   const struct in6_addr *solicit,
		   const struct in6_addr *daddr, const struct in6_addr *saddr)
{
	struct in6_addr addr_buf;
	struct icmp6hdr icmp6h = {
		.icmp6_type = NDISC_NEIGHBOUR_SOLICITATION,
	};

	if (saddr == NULL) {
		if (ipv6_get_lladdr(dev, &addr_buf,
				   (IFA_F_TENTATIVE|IFA_F_OPTIMISTIC)))
			return;
		saddr = &addr_buf;
	}

	__ndisc_send(dev, neigh, daddr, saddr,
		     &icmp6h, solicit,
		     !ipv6_addr_any(saddr) ? ND_OPT_SOURCE_LL_ADDR : 0);
}

void ndisc_send_rs(struct net_device *dev, const struct in6_addr *saddr,
		   const struct in6_addr *daddr)
{
	struct icmp6hdr icmp6h = {
		.icmp6_type = NDISC_ROUTER_SOLICITATION,
	};
	int send_sllao = dev->addr_len;

#ifdef CONFIG_IPV6_OPTIMISTIC_DAD
	/*
	 * According to section 2.2 of RFC 4429, we must not
	 * send router solicitations with a sllao from
	 * optimistic addresses, but we may send the solicitation
	 * if we don't include the sllao.  So here we check
	 * if our address is optimistic, and if so, we
	 * suppress the inclusion of the sllao.
	 */
	if (send_sllao) {
		struct inet6_ifaddr *ifp = ipv6_get_ifaddr(dev_net(dev), saddr,
							   dev, 1);
		if (ifp) {
			if (ifp->flags & IFA_F_OPTIMISTIC)  {
				send_sllao = 0;
			}
			in6_ifa_put(ifp);
		} else {
			send_sllao = 0;
		}
	}
#endif
	__ndisc_send(dev, NULL, daddr, saddr,
		     &icmp6h, NULL,
		     send_sllao ? ND_OPT_SOURCE_LL_ADDR : 0);
}


static void ndisc_error_report(struct neighbour *neigh, struct sk_buff *skb)
{
	/*
	 *	"The sender MUST return an ICMP
	 *	 destination unreachable"
	 */
	dst_link_failure(skb);
	kfree_skb(skb);
}

/* Called with locked neigh: either read or both */

static void ndisc_solicit(struct neighbour *neigh, struct sk_buff *skb)
{
	struct in6_addr *saddr = NULL;
	struct in6_addr mcaddr;
	struct net_device *dev = neigh->dev;
	struct in6_addr *target = (struct in6_addr *)&neigh->primary_key;
	int probes = atomic_read(&neigh->probes);

	if (skb && ipv6_chk_addr(dev_net(dev), &ipv6_hdr(skb)->saddr, dev, 1))
		saddr = &ipv6_hdr(skb)->saddr;

	if ((probes -= neigh->parms->ucast_probes) < 0) {
		if (!(neigh->nud_state & NUD_VALID)) {
			ND_PRINTK1(KERN_DEBUG "%s(): trying to ucast probe in NUD_INVALID: %pI6\n",
				   __func__, target);
		}
		ndisc_send_ns(dev, neigh, target, target, saddr);
	} else if ((probes -= neigh->parms->app_probes) < 0) {
#ifdef CONFIG_ARPD
		neigh_app_ns(neigh);
#endif
	} else {
		addrconf_addr_solict_mult(target, &mcaddr);
		ndisc_send_ns(dev, NULL, target, &mcaddr, saddr);
	}
}

static int pndisc_is_router(const void *pkey,
			    struct net_device *dev)
{
	struct pneigh_entry *n;
	int ret = -1;

	read_lock_bh(&nd_tbl.lock);
	n = __pneigh_lookup(&nd_tbl, dev_net(dev), pkey, dev);
	if (n)
		ret = !!(n->flags & NTF_ROUTER);
	read_unlock_bh(&nd_tbl.lock);

	return ret;
}

static void ndisc_recv_ns(struct sk_buff *skb)
{
	struct nd_msg *msg = (struct nd_msg *)skb_transport_header(skb);
	const struct in6_addr *saddr = &ipv6_hdr(skb)->saddr;
	const struct in6_addr *daddr = &ipv6_hdr(skb)->daddr;
	u8 *lladdr = NULL;
	u32 ndoptlen = skb->tail - (skb->transport_header +
				    offsetof(struct nd_msg, opt));
	struct ndisc_options ndopts;
	struct net_device *dev = skb->dev;
	struct inet6_ifaddr *ifp;
	struct inet6_dev *idev = NULL;
	struct neighbour *neigh;
	int dad = ipv6_addr_any(saddr);
	int inc;
	int is_router = -1;

	if (ipv6_addr_is_multicast(&msg->target)) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 NS: multicast target address");
		return;
	}

	/*
	 * RFC2461 7.1.1:
	 * DAD has to be destined for solicited node multicast address.
	 */
	if (dad &&
	    !(daddr->s6_addr32[0] == htonl(0xff020000) &&
	      daddr->s6_addr32[1] == htonl(0x00000000) &&
	      daddr->s6_addr32[2] == htonl(0x00000001) &&
	      daddr->s6_addr [12] == 0xff )) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 NS: bad DAD packet (wrong destination)\n");
		return;
	}

	if (!ndisc_parse_options(msg->opt, ndoptlen, &ndopts)) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 NS: invalid ND options\n");
		return;
	}

	if (ndopts.nd_opts_src_lladdr) {
		lladdr = ndisc_opt_addr_data(ndopts.nd_opts_src_lladdr, dev);
		if (!lladdr) {
			ND_PRINTK2(KERN_WARNING
				   "ICMPv6 NS: invalid link-layer address length\n");
			return;
		}

		/* RFC2461 7.1.1:
		 *	If the IP source address is the unspecified address,
		 *	there MUST NOT be source link-layer address option
		 *	in the message.
		 */
		if (dad) {
			ND_PRINTK2(KERN_WARNING
				   "ICMPv6 NS: bad DAD packet (link-layer address option)\n");
			return;
		}
	}

	inc = ipv6_addr_is_multicast(daddr);

	ifp = ipv6_get_ifaddr(dev_net(dev), &msg->target, dev, 1);
	if (ifp) {

		if (ifp->flags & (IFA_F_TENTATIVE|IFA_F_OPTIMISTIC)) {
			if (dad) {
				if (dev->type == ARPHRD_IEEE802_TR) {
					const unsigned char *sadr;
					sadr = skb_mac_header(skb);
					if (((sadr[8] ^ dev->dev_addr[0]) & 0x7f) == 0 &&
					    sadr[9] == dev->dev_addr[1] &&
					    sadr[10] == dev->dev_addr[2] &&
					    sadr[11] == dev->dev_addr[3] &&
					    sadr[12] == dev->dev_addr[4] &&
					    sadr[13] == dev->dev_addr[5]) {
						/* looped-back to us */
						goto out;
					}
				}

				/*
				 * We are colliding with another node
				 * who is doing DAD
				 * so fail our DAD process
				 */
				addrconf_dad_failure(ifp);
				return;
			} else {
				/*
				 * This is not a dad solicitation.
				 * If we are an optimistic node,
				 * we should respond.
				 * Otherwise, we should ignore it.
				 */
				if (!(ifp->flags & IFA_F_OPTIMISTIC))
					goto out;
			}
		}

		idev = ifp->idev;
	} else {
		struct net *net = dev_net(dev);

		idev = in6_dev_get(dev);
		if (!idev) {
			/* XXX: count this drop? */
			return;
		}

		if (ipv6_chk_acast_addr(net, dev, &msg->target) ||
		    (idev->cnf.forwarding &&
		     (net->ipv6.devconf_all->proxy_ndp || idev->cnf.proxy_ndp) &&
		     (is_router = pndisc_is_router(&msg->target, dev)) >= 0)) {
			if (!(NEIGH_CB(skb)->flags & LOCALLY_ENQUEUED) &&
			    skb->pkt_type != PACKET_HOST &&
			    inc != 0 &&
			    idev->nd_parms->proxy_delay != 0) {
				/*
				 * for anycast or proxy,
				 * sender should delay its response
				 * by a random time between 0 and
				 * MAX_ANYCAST_DELAY_TIME seconds.
				 * (RFC2461) -- yoshfuji
				 */
				struct sk_buff *n = skb_clone(skb, GFP_ATOMIC);
				if (n)
					pneigh_enqueue(&nd_tbl, idev->nd_parms, n);
				goto out;
			}
		} else
			goto out;
	}

	if (is_router < 0)
		is_router = !!idev->cnf.forwarding;

	if (dad) {
		ndisc_send_na(dev, NULL, &in6addr_linklocal_allnodes, &msg->target,
			      is_router, 0, (ifp != NULL), 1);
		goto out;
	}

	if (inc)
		NEIGH_CACHE_STAT_INC(&nd_tbl, rcv_probes_mcast);
	else
		NEIGH_CACHE_STAT_INC(&nd_tbl, rcv_probes_ucast);

	/*
	 *	update / create cache entry
	 *	for the source address
	 */
	neigh = __neigh_lookup(&nd_tbl, saddr, dev,
			       !inc || lladdr || !dev->addr_len);
	if (neigh)
		neigh_update(neigh, lladdr, NUD_STALE,
			     NEIGH_UPDATE_F_WEAK_OVERRIDE|
			     NEIGH_UPDATE_F_OVERRIDE);
	if (neigh || !dev->header_ops) {
		ndisc_send_na(dev, neigh, saddr, &msg->target,
			      is_router,
			      1, (ifp != NULL && inc), inc);
		if (neigh)
			neigh_release(neigh);
	}

out:
	if (ifp)
		in6_ifa_put(ifp);
	else
		in6_dev_put(idev);
}

static void ndisc_recv_na(struct sk_buff *skb)
{
	struct nd_msg *msg = (struct nd_msg *)skb_transport_header(skb);
	const struct in6_addr *saddr = &ipv6_hdr(skb)->saddr;
	const struct in6_addr *daddr = &ipv6_hdr(skb)->daddr;
	u8 *lladdr = NULL;
	u32 ndoptlen = skb->tail - (skb->transport_header +
				    offsetof(struct nd_msg, opt));
	struct ndisc_options ndopts;
	struct net_device *dev = skb->dev;
	struct inet6_ifaddr *ifp;
	struct neighbour *neigh;

	if (skb->len < sizeof(struct nd_msg)) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 NA: packet too short\n");
		return;
	}

	if (ipv6_addr_is_multicast(&msg->target)) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 NA: target address is multicast.\n");
		return;
	}

	if (ipv6_addr_is_multicast(daddr) &&
	    msg->icmph.icmp6_solicited) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 NA: solicited NA is multicasted.\n");
		return;
	}

	if (!ndisc_parse_options(msg->opt, ndoptlen, &ndopts)) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 NS: invalid ND option\n");
		return;
	}
	if (ndopts.nd_opts_tgt_lladdr) {
		lladdr = ndisc_opt_addr_data(ndopts.nd_opts_tgt_lladdr, dev);
		if (!lladdr) {
			ND_PRINTK2(KERN_WARNING
				   "ICMPv6 NA: invalid link-layer address length\n");
			return;
		}
	}
	ifp = ipv6_get_ifaddr(dev_net(dev), &msg->target, dev, 1);
	if (ifp) {
		if (skb->pkt_type != PACKET_LOOPBACK
		    && (ifp->flags & IFA_F_TENTATIVE)) {
				addrconf_dad_failure(ifp);
				return;
		}
		/* What should we make now? The advertisement
		   is invalid, but ndisc specs say nothing
		   about it. It could be misconfiguration, or
		   an smart proxy agent tries to help us :-)

		   We should not print the error if NA has been
		   received from loopback - it is just our own
		   unsolicited advertisement.
		 */
		if (skb->pkt_type != PACKET_LOOPBACK)
			ND_PRINTK1(KERN_WARNING
			   "ICMPv6 NA: someone advertises our address %pI6 on %s!\n",
			   &ifp->addr, ifp->idev->dev->name);
		in6_ifa_put(ifp);
		return;
	}
	neigh = neigh_lookup(&nd_tbl, &msg->target, dev);

	if (neigh) {
		u8 old_flags = neigh->flags;
		struct net *net = dev_net(dev);

		if (neigh->nud_state & NUD_FAILED)
			goto out;

		/*
		 * Don't update the neighbor cache entry on a proxy NA from
		 * ourselves because either the proxied node is off link or it
		 * has already sent a NA to us.
		 */
		if (lladdr && !memcmp(lladdr, dev->dev_addr, dev->addr_len) &&
		    net->ipv6.devconf_all->forwarding && net->ipv6.devconf_all->proxy_ndp &&
		    pneigh_lookup(&nd_tbl, net, &msg->target, dev, 0)) {
			/* XXX: idev->cnf.prixy_ndp */
			goto out;
		}

		neigh_update(neigh, lladdr,
			     msg->icmph.icmp6_solicited ? NUD_REACHABLE : NUD_STALE,
			     NEIGH_UPDATE_F_WEAK_OVERRIDE|
			     (msg->icmph.icmp6_override ? NEIGH_UPDATE_F_OVERRIDE : 0)|
			     NEIGH_UPDATE_F_OVERRIDE_ISROUTER|
			     (msg->icmph.icmp6_router ? NEIGH_UPDATE_F_ISROUTER : 0));

		if ((old_flags & ~neigh->flags) & NTF_ROUTER) {
			/*
			 * Change: router to host
			 */
			struct rt6_info *rt;
			rt = rt6_get_dflt_router(saddr, dev);
			if (rt)
				ip6_del_rt(rt);
		}

out:
		neigh_release(neigh);
	}
}

static void ndisc_recv_rs(struct sk_buff *skb)
{
	struct rs_msg *rs_msg = (struct rs_msg *)skb_transport_header(skb);
	unsigned long ndoptlen = skb->len - sizeof(*rs_msg);
	struct neighbour *neigh;
	struct inet6_dev *idev;
	const struct in6_addr *saddr = &ipv6_hdr(skb)->saddr;
	struct ndisc_options ndopts;
	u8 *lladdr = NULL;

	if (skb->len < sizeof(*rs_msg))
		return;

	idev = __in6_dev_get(skb->dev);
	if (!idev) {
		if (net_ratelimit())
			ND_PRINTK1("ICMP6 RS: can't find in6 device\n");
		return;
	}

#if defined(CONFIG_BCM_KF_IP)
	if (!idev->cnf.forwarding  || (idev->dev->priv_flags & IFF_WANDEV))
#else
	/* Don't accept RS if we're not in router mode */
	if (!idev->cnf.forwarding)
#endif
		goto out;

	/*
	 * Don't update NCE if src = ::;
	 * this implies that the source node has no ip address assigned yet.
	 */
	if (ipv6_addr_any(saddr))
		goto out;

	/* Parse ND options */
	if (!ndisc_parse_options(rs_msg->opt, ndoptlen, &ndopts)) {
		if (net_ratelimit())
			ND_PRINTK2("ICMP6 NS: invalid ND option, ignored\n");
		goto out;
	}

	if (ndopts.nd_opts_src_lladdr) {
		lladdr = ndisc_opt_addr_data(ndopts.nd_opts_src_lladdr,
					     skb->dev);
		if (!lladdr)
			goto out;
	}

	neigh = __neigh_lookup(&nd_tbl, saddr, skb->dev, 1);
	if (neigh) {
		neigh_update(neigh, lladdr, NUD_STALE,
			     NEIGH_UPDATE_F_WEAK_OVERRIDE|
			     NEIGH_UPDATE_F_OVERRIDE|
			     NEIGH_UPDATE_F_OVERRIDE_ISROUTER);
		neigh_release(neigh);
	}
out:
	return;
}

static void ndisc_ra_useropt(struct sk_buff *ra, struct nd_opt_hdr *opt)
{
	struct icmp6hdr *icmp6h = (struct icmp6hdr *)skb_transport_header(ra);
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	struct nduseroptmsg *ndmsg;
	struct net *net = dev_net(ra->dev);
	int err;
	int base_size = NLMSG_ALIGN(sizeof(struct nduseroptmsg)
				    + (opt->nd_opt_len << 3));
	size_t msg_size = base_size + nla_total_size(sizeof(struct in6_addr));

	skb = nlmsg_new(msg_size, GFP_ATOMIC);
	if (skb == NULL) {
		err = -ENOBUFS;
		goto errout;
	}

	nlh = nlmsg_put(skb, 0, 0, RTM_NEWNDUSEROPT, base_size, 0);
	if (nlh == NULL) {
		goto nla_put_failure;
	}

	ndmsg = nlmsg_data(nlh);
	ndmsg->nduseropt_family = AF_INET6;
	ndmsg->nduseropt_ifindex = ra->dev->ifindex;
	ndmsg->nduseropt_icmp_type = icmp6h->icmp6_type;
	ndmsg->nduseropt_icmp_code = icmp6h->icmp6_code;
	ndmsg->nduseropt_opts_len = opt->nd_opt_len << 3;

	memcpy(ndmsg + 1, opt, opt->nd_opt_len << 3);

	NLA_PUT(skb, NDUSEROPT_SRCADDR, sizeof(struct in6_addr),
		&ipv6_hdr(ra)->saddr);
	nlmsg_end(skb, nlh);

	rtnl_notify(skb, net, 0, RTNLGRP_ND_USEROPT, NULL, GFP_ATOMIC);
	return;

nla_put_failure:
	nlmsg_free(skb);
	err = -EMSGSIZE;
errout:
	rtnl_set_sk_err(net, RTNLGRP_ND_USEROPT, err);
}

static inline int accept_ra(struct inet6_dev *in6_dev)
{
#if defined(CONFIG_BCM_KF_IP)
	/* WAN interface needs to act like a host. */
	if (((in6_dev->cnf.forwarding) && 
		(!(in6_dev->dev->priv_flags & IFF_WANDEV) || 
		((in6_dev->dev->priv_flags & IFF_WANDEV) && 
		netdev_path_is_root(in6_dev->dev))))
		&& (in6_dev->cnf.accept_ra < 2))
#else
	/*
	 * If forwarding is enabled, RA are not accepted unless the special
	 * hybrid mode (accept_ra=2) is enabled.
	 */
	if (in6_dev->cnf.forwarding && in6_dev->cnf.accept_ra < 2)
#endif
		return 0;

	return in6_dev->cnf.accept_ra;
}

static void ndisc_router_discovery(struct sk_buff *skb)
{
	struct ra_msg *ra_msg = (struct ra_msg *)skb_transport_header(skb);
	struct neighbour *neigh = NULL;
	struct inet6_dev *in6_dev;
	struct rt6_info *rt = NULL;
	int lifetime;
	struct ndisc_options ndopts;
	int optlen;
	unsigned int pref = 0;
#ifdef SUPPORT_GPL_9
    unsigned int otherFlag = 1;
#endif

	__u8 * opt = (__u8 *)(ra_msg + 1);

	optlen = (skb->tail - skb->transport_header) - sizeof(struct ra_msg);

	if (!(ipv6_addr_type(&ipv6_hdr(skb)->saddr) & IPV6_ADDR_LINKLOCAL)) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 RA: source address is not link-local.\n");
		return;
	}
	if (optlen < 0) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 RA: packet too short\n");
		return;
	}

#ifdef CONFIG_IPV6_NDISC_NODETYPE
	if (skb->ndisc_nodetype == NDISC_NODETYPE_HOST) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 RA: from host or unauthorized router\n");
		return;
	}
#endif

	/*
	 *	set the RA_RECV flag in the interface
	 */

	in6_dev = __in6_dev_get(skb->dev);
	if (in6_dev == NULL) {
		ND_PRINTK0(KERN_ERR
			   "ICMPv6 RA: can't find inet6 device for %s.\n",
			   skb->dev->name);
		return;
	}

	if (!ndisc_parse_options(opt, optlen, &ndopts)) {
		ND_PRINTK2(KERN_WARNING
			   "ICMP6 RA: invalid ND options\n");
		return;
	}

	if (!accept_ra(in6_dev))
		goto skip_linkparms;

#ifdef CONFIG_IPV6_NDISC_NODETYPE
	/* skip link-specific parameters from interior routers */
	if (skb->ndisc_nodetype == NDISC_NODETYPE_NODEFAULT)
		goto skip_linkparms;
#endif

	if (in6_dev->if_flags & IF_RS_SENT) {
		/*
		 *	flag that an RA was received after an RS was sent
		 *	out on this interface.
		 */
		in6_dev->if_flags |= IF_RA_RCVD;
	}

	/*
	 * Remember the managed/otherconf flags from most recently
	 * received RA message (RFC 2462) -- yoshfuji
	 */
	in6_dev->if_flags = (in6_dev->if_flags & ~(IF_RA_MANAGED |
				IF_RA_OTHERCONF)) |
				(ra_msg->icmph.icmp6_addrconf_managed ?
					IF_RA_MANAGED : 0) |
				(ra_msg->icmph.icmp6_addrconf_other ?
					IF_RA_OTHERCONF : 0);
#ifdef SUPPORT_GPL_9
    otherFlag = ra_msg->icmph.icmp6_addrconf_other ? 1 : 0;
#endif

	if (!in6_dev->cnf.accept_ra_defrtr)
		goto skip_defrtr;

	if (ipv6_chk_addr(dev_net(in6_dev->dev), &ipv6_hdr(skb)->saddr, NULL, 0))
		goto skip_defrtr;

	lifetime = ntohs(ra_msg->icmph.icmp6_rt_lifetime);

#ifdef CONFIG_IPV6_ROUTER_PREF
	pref = ra_msg->icmph.icmp6_router_pref;
	/* 10b is handled as if it were 00b (medium) */
	if (pref == ICMPV6_ROUTER_PREF_INVALID ||
	    !in6_dev->cnf.accept_ra_rtr_pref)
		pref = ICMPV6_ROUTER_PREF_MEDIUM;
#endif

	rt = rt6_get_dflt_router(&ipv6_hdr(skb)->saddr, skb->dev);

	if (rt) {
		neigh = dst_neigh_lookup(&rt->dst, &ipv6_hdr(skb)->saddr);
		if (!neigh) {
			ND_PRINTK0(KERN_ERR
				   "ICMPv6 RA: %s() got default router without neighbour.\n",
				   __func__);
			dst_release(&rt->dst);
			return;
		}
	}
	if (rt && lifetime == 0) {
		ip6_del_rt(rt);
		rt = NULL;
	}

	if (rt == NULL && lifetime) {
		ND_PRINTK3(KERN_DEBUG
			   "ICMPv6 RA: adding default router.\n");

		rt = rt6_add_dflt_router(&ipv6_hdr(skb)->saddr, skb->dev, pref);
		if (rt == NULL) {
			ND_PRINTK0(KERN_ERR
				   "ICMPv6 RA: %s() failed to add default route.\n",
				   __func__);
			return;
		}

		neigh = dst_neigh_lookup(&rt->dst, &ipv6_hdr(skb)->saddr);
		if (neigh == NULL) {
			ND_PRINTK0(KERN_ERR
				   "ICMPv6 RA: %s() got default router without neighbour.\n",
				   __func__);
			dst_release(&rt->dst);
			return;
		}
		neigh->flags |= NTF_ROUTER;
	} else if (rt) {
		rt->rt6i_flags = (rt->rt6i_flags & ~RTF_PREF_MASK) | RTF_PREF(pref);
	}

	if (rt)
		rt6_set_expires(rt, jiffies + (HZ * lifetime));
	if (ra_msg->icmph.icmp6_hop_limit) {
		in6_dev->cnf.hop_limit = ra_msg->icmph.icmp6_hop_limit;
		if (rt)
			dst_metric_set(&rt->dst, RTAX_HOPLIMIT,
				       ra_msg->icmph.icmp6_hop_limit);
	}

#ifdef ACTION_TEC_IPV6_CODE_FOR_IOT
	if (rt) // send default route info to userspace
    {
        memcpy(&message_val.u_ra,(const void*)&(ra_msg->icmph.icmp6_dataun.u_nd_ra),
                sizeof(struct icmpv6_nd_ra));
        strncpy(message_val.name,skb->dev->name,IFNAMSIZ-1);
        memcpy((void *)&message_val.addr,(void *)&ipv6_hdr(skb)->saddr,sizeof(struct in6_addr));
        broadcast_resend_times = RESEND_TIMES;
        //printk(KERN_ERR "==========send ra info to userspace========\n");
        send_netlink_message_to_userspace(&message_val);
    }
#endif
skip_defrtr:

	/*
	 *	Update Reachable Time and Retrans Timer
	 */

	if (in6_dev->nd_parms) {
		unsigned long rtime = ntohl(ra_msg->retrans_timer);

		if (rtime && rtime/1000 < MAX_SCHEDULE_TIMEOUT/HZ) {
			rtime = (rtime*HZ)/1000;
			if (rtime < HZ/10)
				rtime = HZ/10;
			in6_dev->nd_parms->retrans_time = rtime;
			in6_dev->tstamp = jiffies;
			inet6_ifinfo_notify(RTM_NEWLINK, in6_dev);
		}

		rtime = ntohl(ra_msg->reachable_time);
		if (rtime && rtime/1000 < MAX_SCHEDULE_TIMEOUT/(3*HZ)) {
			rtime = (rtime*HZ)/1000;

			if (rtime < HZ/10)
				rtime = HZ/10;

			if (rtime != in6_dev->nd_parms->base_reachable_time) {
				in6_dev->nd_parms->base_reachable_time = rtime;
				in6_dev->nd_parms->gc_staletime = 3 * rtime;
				in6_dev->nd_parms->reachable_time = neigh_rand_reach_time(rtime);
				in6_dev->tstamp = jiffies;
				inet6_ifinfo_notify(RTM_NEWLINK, in6_dev);
			}
		}
	}

skip_linkparms:

	/*
	 *	Process options.
	 */

	if (!neigh)
		neigh = __neigh_lookup(&nd_tbl, &ipv6_hdr(skb)->saddr,
				       skb->dev, 1);
	if (neigh) {
		u8 *lladdr = NULL;
		if (ndopts.nd_opts_src_lladdr) {
			lladdr = ndisc_opt_addr_data(ndopts.nd_opts_src_lladdr,
						     skb->dev);
			if (!lladdr) {
				ND_PRINTK2(KERN_WARNING
					   "ICMPv6 RA: invalid link-layer address length\n");
				goto out;
			}
		}
		neigh_update(neigh, lladdr, NUD_STALE,
			     NEIGH_UPDATE_F_WEAK_OVERRIDE|
			     NEIGH_UPDATE_F_OVERRIDE|
			     NEIGH_UPDATE_F_OVERRIDE_ISROUTER|
			     NEIGH_UPDATE_F_ISROUTER);
	}

	if (!accept_ra(in6_dev))
		goto out;

#ifdef CONFIG_IPV6_ROUTE_INFO
	if (ipv6_chk_addr(dev_net(in6_dev->dev), &ipv6_hdr(skb)->saddr, NULL, 0))
		goto skip_routeinfo;

	if (in6_dev->cnf.accept_ra_rtr_pref && ndopts.nd_opts_ri) {
		struct nd_opt_hdr *p;
		for (p = ndopts.nd_opts_ri;
		     p;
		     p = ndisc_next_option(p, ndopts.nd_opts_ri_end)) {
			struct route_info *ri = (struct route_info *)p;
#ifdef CONFIG_IPV6_NDISC_NODETYPE
			if (skb->ndisc_nodetype == NDISC_NODETYPE_NODEFAULT &&
			    ri->prefix_len == 0)
				continue;
#endif
			if (ri->prefix_len > in6_dev->cnf.accept_ra_rt_info_max_plen)
				continue;
			rt6_route_rcv(skb->dev, (u8*)p, (p->nd_opt_len) << 3,
				      &ipv6_hdr(skb)->saddr);
		}
	}

skip_routeinfo:
#endif

#ifdef CONFIG_IPV6_NDISC_NODETYPE
	/* skip link-specific ndopts from interior routers */
	if (skb->ndisc_nodetype == NDISC_NODETYPE_NODEFAULT)
		goto out;
#endif

	if (in6_dev->cnf.accept_ra_pinfo && ndopts.nd_opts_pi) {
		struct nd_opt_hdr *p;
		for (p = ndopts.nd_opts_pi;
		     p;
		     p = ndisc_next_option(p, ndopts.nd_opts_pi_end)) {
			addrconf_prefix_rcv(skb->dev, (u8 *)p,
					    (p->nd_opt_len) << 3,
					    ndopts.nd_opts_src_lladdr != NULL);
		}
	}

	if (ndopts.nd_opts_mtu) {
		__be32 n;
		u32 mtu;

		memcpy(&n, ((u8*)(ndopts.nd_opts_mtu+1))+2, sizeof(mtu));
		mtu = ntohl(n);

		if (mtu < IPV6_MIN_MTU || mtu > skb->dev->mtu) {
			ND_PRINTK2(KERN_WARNING
				   "ICMPv6 RA: invalid mtu: %d\n",
				   mtu);
		} else if (in6_dev->cnf.mtu6 != mtu) {
			in6_dev->cnf.mtu6 = mtu;

			if (rt)
				dst_metric_set(&rt->dst, RTAX_MTU, mtu);

			rt6_mtu_change(skb->dev, mtu);
		}
	}

#ifdef SUPPORT_GPL_9
    if (ndopts.nd_opts_rd && (otherFlag == 0))  // process the dns info recv from RA
    {
        int i, index;
        int skip_flag, cmp_flag;
        struct nd_opt_hdr *p;
        //struct in6_addr *rdns_server;
		for (p = ndopts.nd_opts_rd;
		     p;
		     p = ndisc_next_option(p, ndopts.nd_opts_rd_end)) {
            struct rdnss *rs = (struct rdnss *)p;
            index = (rs->length-1)/2;
            index = (index>2) ? 2 : index;
            if (index > 0)
            {
                cmp_flag = 0;
                skip_flag = 0;
                for (i = 0; i < index; i++)
                {
                    cmp_flag = ipv6_addr_cmp(&message_dns.dnsServer[i], &rs->prefix[i]);
                    if (cmp_flag != 0)
                    {
                        ipv6_addr_copy(&message_dns.dnsServer[i], &rs->prefix[i]);
                        if (0 == skip_flag)
                            resend_count = SEND_RDNSS_COUNT;
                        skip_flag = 1;
                    }
                }
                if (resend_count > 0)
                {
                    resend_count--;
                    message_dns.length = index;
                    message_dns.flag = 1;
                    send_dnsinfo_message_to_userspace(&message_dns);
                }
            }
        }
    }
    else
    {
        if (message_dns.flag != 0)
        {
            message_dns.flag = 0;
            send_dnsinfo_message_to_userspace(&message_dns);
        }
    }
#endif

    if (ndopts.nd_useropts) {
		struct nd_opt_hdr *p;
		for (p = ndopts.nd_useropts;
		     p;
		     p = ndisc_next_useropt(p, ndopts.nd_useropts_end)) {
			ndisc_ra_useropt(skb, p);
		}
	}

	if (ndopts.nd_opts_tgt_lladdr || ndopts.nd_opts_rh) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 RA: invalid RA options");
	}
out:
	if (rt)
		dst_release(&rt->dst);
	if (neigh)
		neigh_release(neigh);
}

static void ndisc_redirect_rcv(struct sk_buff *skb)
{
	struct inet6_dev *in6_dev;
	struct icmp6hdr *icmph;
	const struct in6_addr *dest;
	const struct in6_addr *target;	/* new first hop to destination */
	struct neighbour *neigh;
	int on_link = 0;
	struct ndisc_options ndopts;
	int optlen;
	u8 *lladdr = NULL;

#ifdef CONFIG_IPV6_NDISC_NODETYPE
	switch (skb->ndisc_nodetype) {
	case NDISC_NODETYPE_HOST:
	case NDISC_NODETYPE_NODEFAULT:
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 Redirect: from host or unauthorized router\n");
		return;
	}
#endif

	if (!(ipv6_addr_type(&ipv6_hdr(skb)->saddr) & IPV6_ADDR_LINKLOCAL)) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 Redirect: source address is not link-local.\n");
		return;
	}

	optlen = skb->tail - skb->transport_header;
	optlen -= sizeof(struct icmp6hdr) + 2 * sizeof(struct in6_addr);

	if (optlen < 0) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 Redirect: packet too short\n");
		return;
	}

	icmph = icmp6_hdr(skb);
	target = (const struct in6_addr *) (icmph + 1);
	dest = target + 1;

	if (ipv6_addr_is_multicast(dest)) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 Redirect: destination address is multicast.\n");
		return;
	}

	if (ipv6_addr_equal(dest, target)) {
		on_link = 1;
	} else if (ipv6_addr_type(target) !=
		   (IPV6_ADDR_UNICAST|IPV6_ADDR_LINKLOCAL)) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 Redirect: target address is not link-local unicast.\n");
		return;
	}

	in6_dev = __in6_dev_get(skb->dev);
	if (!in6_dev)
		return;
#if defined(CONFIG_BCM_KF_IP)
	/* WAN interface needs to act like a host. */
	if (((in6_dev->cnf.forwarding) && !(in6_dev->dev->priv_flags & IFF_WANDEV))
		|| (!in6_dev->cnf.accept_redirects))
#else
	if (in6_dev->cnf.forwarding || !in6_dev->cnf.accept_redirects)
#endif
		return;

	/* RFC2461 8.1:
	 *	The IP source address of the Redirect MUST be the same as the current
	 *	first-hop router for the specified ICMP Destination Address.
	 */

	if (!ndisc_parse_options((u8*)(dest + 1), optlen, &ndopts)) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 Redirect: invalid ND options\n");
		return;
	}
	if (ndopts.nd_opts_tgt_lladdr) {
		lladdr = ndisc_opt_addr_data(ndopts.nd_opts_tgt_lladdr,
					     skb->dev);
		if (!lladdr) {
			ND_PRINTK2(KERN_WARNING
				   "ICMPv6 Redirect: invalid link-layer address length\n");
			return;
		}
	}

	neigh = __neigh_lookup(&nd_tbl, target, skb->dev, 1);
	if (neigh) {
		rt6_redirect(dest, &ipv6_hdr(skb)->daddr,
			     &ipv6_hdr(skb)->saddr, neigh, lladdr,
			     on_link);
		neigh_release(neigh);
	}
}

void ndisc_send_redirect(struct sk_buff *skb, const struct in6_addr *target)
{
	struct net_device *dev = skb->dev;
	struct net *net = dev_net(dev);
	struct sock *sk = net->ipv6.ndisc_sk;
	int len = sizeof(struct icmp6hdr) + 2 * sizeof(struct in6_addr);
	struct sk_buff *buff;
	struct icmp6hdr *icmph;
	struct in6_addr saddr_buf;
	struct in6_addr *addrp;
	struct rt6_info *rt;
	struct dst_entry *dst;
	struct inet6_dev *idev;
	struct flowi6 fl6;
	u8 *opt;
	int hlen, tlen;
	int rd_len;
	int err;
	u8 ha_buf[MAX_ADDR_LEN], *ha = NULL;

	if (ipv6_get_lladdr(dev, &saddr_buf, IFA_F_TENTATIVE)) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 Redirect: no link-local address on %s\n",
			   dev->name);
		return;
	}

	if (!ipv6_addr_equal(&ipv6_hdr(skb)->daddr, target) &&
	    ipv6_addr_type(target) != (IPV6_ADDR_UNICAST|IPV6_ADDR_LINKLOCAL)) {
		ND_PRINTK2(KERN_WARNING
			"ICMPv6 Redirect: target address is not link-local unicast.\n");
		return;
	}

	icmpv6_flow_init(sk, &fl6, NDISC_REDIRECT,
			 &saddr_buf, &ipv6_hdr(skb)->saddr, dev->ifindex);

	dst = ip6_route_output(net, NULL, &fl6);
	if (dst->error) {
		dst_release(dst);
		return;
	}
	dst = xfrm_lookup(net, dst, flowi6_to_flowi(&fl6), NULL, 0);
	if (IS_ERR(dst))
		return;

	rt = (struct rt6_info *) dst;

	if (rt->rt6i_flags & RTF_GATEWAY) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 Redirect: destination is not a neighbour.\n");
		goto release;
	}
	if (!rt->rt6i_peer)
		rt6_bind_peer(rt, 1);
	if (!inet_peer_xrlim_allow(rt->rt6i_peer, 1*HZ))
		goto release;

	if (dev->addr_len) {
		struct neighbour *neigh = dst_neigh_lookup(skb_dst(skb), target);
		if (!neigh) {
			ND_PRINTK2(KERN_WARNING
				   "ICMPv6 Redirect: no neigh for target address\n");
			goto release;
		}

		read_lock_bh(&neigh->lock);
		if (neigh->nud_state & NUD_VALID) {
			memcpy(ha_buf, neigh->ha, dev->addr_len);
			read_unlock_bh(&neigh->lock);
			ha = ha_buf;
			len += ndisc_opt_addr_space(dev);
		} else
			read_unlock_bh(&neigh->lock);

		neigh_release(neigh);
	}

	rd_len = min_t(unsigned int,
		     IPV6_MIN_MTU-sizeof(struct ipv6hdr)-len, skb->len + 8);
	rd_len &= ~0x7;
	len += rd_len;

	hlen = LL_RESERVED_SPACE(dev);
	tlen = dev->needed_tailroom;
	buff = sock_alloc_send_skb(sk,
				   (MAX_HEADER + sizeof(struct ipv6hdr) +
				    len + hlen + tlen),
				   1, &err);
	if (buff == NULL) {
		ND_PRINTK0(KERN_ERR
			   "ICMPv6 Redirect: %s() failed to allocate an skb, err=%d.\n",
			   __func__, err);
		goto release;
	}

	skb_reserve(buff, hlen);
	ip6_nd_hdr(sk, buff, dev, &saddr_buf, &ipv6_hdr(skb)->saddr,
		   IPPROTO_ICMPV6, len);

	skb_set_transport_header(buff, skb_tail_pointer(buff) - buff->data);
	skb_put(buff, len);
	icmph = icmp6_hdr(buff);

	memset(icmph, 0, sizeof(struct icmp6hdr));
	icmph->icmp6_type = NDISC_REDIRECT;

	/*
	 *	copy target and destination addresses
	 */

	addrp = (struct in6_addr *)(icmph + 1);
	*addrp = *target;
	addrp++;
	*addrp = ipv6_hdr(skb)->daddr;

	opt = (u8*) (addrp + 1);

	/*
	 *	include target_address option
	 */

	if (ha)
		opt = ndisc_fill_addr_option(opt, ND_OPT_TARGET_LL_ADDR, ha,
					     dev->addr_len, dev->type);

	/*
	 *	build redirect option and copy skb over to the new packet.
	 */

	memset(opt, 0, 8);
	*(opt++) = ND_OPT_REDIRECT_HDR;
	*(opt++) = (rd_len >> 3);
	opt += 6;

	memcpy(opt, ipv6_hdr(skb), rd_len - 8);

	icmph->icmp6_cksum = csum_ipv6_magic(&saddr_buf, &ipv6_hdr(skb)->saddr,
					     len, IPPROTO_ICMPV6,
					     csum_partial(icmph, len, 0));

	skb_dst_set(buff, dst);
	rcu_read_lock();
	idev = __in6_dev_get(dst->dev);
	IP6_UPD_PO_STATS(net, idev, IPSTATS_MIB_OUT, skb->len);
	err = NF_HOOK(NFPROTO_IPV6, NF_INET_LOCAL_OUT, buff, NULL, dst->dev,
		      dst_output);
	if (!err) {
		ICMP6MSGOUT_INC_STATS(net, idev, NDISC_REDIRECT);
		ICMP6_INC_STATS(net, idev, ICMP6_MIB_OUTMSGS);
	}

	rcu_read_unlock();
	return;

release:
	dst_release(dst);
}

static void pndisc_redo(struct sk_buff *skb)
{
	ndisc_recv_ns(skb);
	kfree_skb(skb);
}

int ndisc_rcv(struct sk_buff *skb)
{
	struct nd_msg *msg;

	if (!pskb_may_pull(skb, skb->len))
		return 0;

	msg = (struct nd_msg *)skb_transport_header(skb);

	__skb_push(skb, skb->data - skb_transport_header(skb));

	if (ipv6_hdr(skb)->hop_limit != 255) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 NDISC: invalid hop-limit: %d\n",
			   ipv6_hdr(skb)->hop_limit);
		return 0;
	}

	if (msg->icmph.icmp6_code != 0) {
		ND_PRINTK2(KERN_WARNING
			   "ICMPv6 NDISC: invalid ICMPv6 code: %d\n",
			   msg->icmph.icmp6_code);
		return 0;
	}

	memset(NEIGH_CB(skb), 0, sizeof(struct neighbour_cb));

	switch (msg->icmph.icmp6_type) {
	case NDISC_NEIGHBOUR_SOLICITATION:
		ndisc_recv_ns(skb);
		break;

	case NDISC_NEIGHBOUR_ADVERTISEMENT:
		ndisc_recv_na(skb);
		break;

	case NDISC_ROUTER_SOLICITATION:
		ndisc_recv_rs(skb);
		break;

	case NDISC_ROUTER_ADVERTISEMENT:
		ndisc_router_discovery(skb);
		break;

	case NDISC_REDIRECT:
		ndisc_redirect_rcv(skb);
		break;
	}

	return 0;
}

static int ndisc_netdev_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;
	struct net *net = dev_net(dev);

	switch (event) {
	case NETDEV_CHANGEADDR:
		neigh_changeaddr(&nd_tbl, dev);
		fib6_run_gc(~0UL, net);
		break;
	case NETDEV_DOWN:
		neigh_ifdown(&nd_tbl, dev);
		fib6_run_gc(~0UL, net);
		break;
	case NETDEV_NOTIFY_PEERS:
		ndisc_send_unsol_na(dev);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block ndisc_netdev_notifier = {
	.notifier_call = ndisc_netdev_event,
};

#ifdef CONFIG_SYSCTL
static void ndisc_warn_deprecated_sysctl(struct ctl_table *ctl,
					 const char *func, const char *dev_name)
{
	static char warncomm[TASK_COMM_LEN];
	static int warned;
	if (strcmp(warncomm, current->comm) && warned < 5) {
		strcpy(warncomm, current->comm);
		printk(KERN_WARNING
			"process `%s' is using deprecated sysctl (%s) "
			"net.ipv6.neigh.%s.%s; "
			"Use net.ipv6.neigh.%s.%s_ms "
			"instead.\n",
			warncomm, func,
			dev_name, ctl->procname,
			dev_name, ctl->procname);
		warned++;
	}
}

int ndisc_ifinfo_sysctl_change(struct ctl_table *ctl, int write, void __user *buffer, size_t *lenp, loff_t *ppos)
{
	struct net_device *dev = ctl->extra1;
	struct inet6_dev *idev;
	int ret;

	if ((strcmp(ctl->procname, "retrans_time") == 0) ||
	    (strcmp(ctl->procname, "base_reachable_time") == 0))
		ndisc_warn_deprecated_sysctl(ctl, "syscall", dev ? dev->name : "default");

	if (strcmp(ctl->procname, "retrans_time") == 0)
		ret = proc_dointvec(ctl, write, buffer, lenp, ppos);

	else if (strcmp(ctl->procname, "base_reachable_time") == 0)
		ret = proc_dointvec_jiffies(ctl, write,
					    buffer, lenp, ppos);

	else if ((strcmp(ctl->procname, "retrans_time_ms") == 0) ||
		 (strcmp(ctl->procname, "base_reachable_time_ms") == 0))
		ret = proc_dointvec_ms_jiffies(ctl, write,
					       buffer, lenp, ppos);
	else
		ret = -1;

	if (write && ret == 0 && dev && (idev = in6_dev_get(dev)) != NULL) {
		if (ctl->data == &idev->nd_parms->base_reachable_time)
			idev->nd_parms->reachable_time = neigh_rand_reach_time(idev->nd_parms->base_reachable_time);
		idev->tstamp = jiffies;
		inet6_ifinfo_notify(RTM_NEWLINK, idev);
		in6_dev_put(idev);
	}
	return ret;
}


#endif

static int __net_init ndisc_net_init(struct net *net)
{
	struct ipv6_pinfo *np;
	struct sock *sk;
	int err;

	err = inet_ctl_sock_create(&sk, PF_INET6,
				   SOCK_RAW, IPPROTO_ICMPV6, net);
	if (err < 0) {
		ND_PRINTK0(KERN_ERR
			   "ICMPv6 NDISC: Failed to initialize the control socket (err %d).\n",
			   err);
		return err;
	}

	net->ipv6.ndisc_sk = sk;

	np = inet6_sk(sk);
	np->hop_limit = 255;
	/* Do not loopback ndisc messages */
	np->mc_loop = 0;

	return 0;
}

static void __net_exit ndisc_net_exit(struct net *net)
{
	inet_ctl_sock_destroy(net->ipv6.ndisc_sk);
}

static struct pernet_operations ndisc_net_ops = {
	.init = ndisc_net_init,
	.exit = ndisc_net_exit,
};

int __init ndisc_init(void)
{
	int err;

	err = register_pernet_subsys(&ndisc_net_ops);
	if (err)
		return err;
	/*
	 * Initialize the neighbour table
	 */
	neigh_table_init(&nd_tbl);

#ifdef CONFIG_SYSCTL
	err = neigh_sysctl_register(NULL, &nd_tbl.parms, "ipv6",
				    &ndisc_ifinfo_sysctl_change);
	if (err)
		goto out_unregister_pernet;
#endif
	err = register_netdevice_notifier(&ndisc_netdev_notifier);
	if (err)
		goto out_unregister_sysctl;
#ifdef ACTION_TEC_IPV6_CODE_FOR_IOT
    ndisc_nl_sk = netlink_kernel_create(&init_net,NETLINK_RAD, 0, skb_ndisc_rcv,NULL,THIS_MODULE);
    if (!ndisc_nl_sk) {
        printk(KERN_ERR "Create netlink error\n");
    }
#endif
#ifdef SUPPORT_GPL_9
    ndisc_nl_sk_rdnss = netlink_kernel_create(&init_net,NETLINK_RAD_RDNSS, 0, skb_ndisc_rcv,NULL,THIS_MODULE);
    if (!ndisc_nl_sk_rdnss) {
        printk(KERN_ERR "Create netlink error\n");
    }
#endif
out:
	return err;

out_unregister_sysctl:
#ifdef CONFIG_SYSCTL
	neigh_sysctl_unregister(&nd_tbl.parms);
out_unregister_pernet:
#endif
	unregister_pernet_subsys(&ndisc_net_ops);
	goto out;
}

void ndisc_cleanup(void)
{
	unregister_netdevice_notifier(&ndisc_netdev_notifier);
#ifdef CONFIG_SYSCTL
	neigh_sysctl_unregister(&nd_tbl.parms);
#endif
	neigh_table_clear(&nd_tbl);
	unregister_pernet_subsys(&ndisc_net_ops);
}
