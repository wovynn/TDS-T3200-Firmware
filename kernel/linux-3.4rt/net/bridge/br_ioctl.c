/*
 *	Ioctl handler
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/capability.h>
#include <linux/kernel.h>
#include <linux/if_bridge.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/times.h>
#include <net/net_namespace.h>
#include <asm/uaccess.h>
#include "br_private.h"
#if defined(CONFIG_BCM_KF_IGMP) && defined(CONFIG_BR_IGMP_SNOOP)
#include "br_igmp.h"
#endif
#if defined(CONFIG_BCM_KF_MLD) && defined(CONFIG_BR_MLD_SNOOP)
#include "br_mld.h"
#endif
#if defined(CONFIG_BCM_KF_NETFILTER)
#include <linux/module.h> 
#include "br_flows.h"
#endif

#if defined(CONFIG_BCM_KF_NETFILTER)
#include <linux/bcm_log.h>
#endif


/* called with RTNL */
static int get_bridge_ifindices(struct net *net, int *indices, int num)
{
	struct net_device *dev;
	int i = 0;

	for_each_netdev(net, dev) {
		if (i >= num)
			break;
		if (dev->priv_flags & IFF_EBRIDGE)
			indices[i++] = dev->ifindex;
	}

	return i;
}

/* called with RTNL */
static void get_port_ifindices(struct net_bridge *br, int *ifindices, int num)
{
	struct net_bridge_port *p;

	list_for_each_entry(p, &br->port_list, list) {
		if (p->port_no < num)
			ifindices[p->port_no] = p->dev->ifindex;
	}
}

/*
 * Format up to a page worth of forwarding table entries
 * userbuf -- where to copy result
 * maxnum  -- maximum number of entries desired
 *            (limited to a page for sanity)
 * offset  -- number of records to skip
 */
static int get_fdb_entries(struct net_bridge *br, void __user *userbuf,
			   unsigned long maxnum, unsigned long offset)
{
	int num;
	void *buf;
	size_t size;

	/* Clamp size to PAGE_SIZE, test maxnum to avoid overflow */
	if (maxnum > PAGE_SIZE/sizeof(struct __fdb_entry))
		maxnum = PAGE_SIZE/sizeof(struct __fdb_entry);

	size = maxnum * sizeof(struct __fdb_entry);

	buf = kmalloc(size, GFP_USER);
	if (!buf)
		return -ENOMEM;

	num = br_fdb_fillbuf(br, buf, maxnum, offset);
	if (num > 0) {
		if (copy_to_user(userbuf, buf, num*sizeof(struct __fdb_entry)))
			num = -EFAULT;
	}
	kfree(buf);

	return num;
}

#if defined(CONFIG_BCM_KF_BRIDGE_STATIC_FDB) || defined(CONFIG_BCM_KF_BRIDGE_DYNAMIC_FDB)
/* called with RTNL */
static int add_fdb_entries(struct net_bridge *br, void __user *userbuf,
			   unsigned long maxnum, int ifindex, int isStatic)
{
	struct net_device *dev;
	unsigned char     *pMacAddr = NULL;
	unsigned char     *pMac = NULL;
	int                size;
	int                i;
	int                ret = 0;
	struct net_bridge_port *port;

	if (!capable(CAP_NET_ADMIN))
	{
		return -EPERM;
	}

	dev = dev_get_by_index(dev_net(br->dev), ifindex);
	if (!dev)
	{
		return -EINVAL;
	}

	port = br_port_get_rtnl(dev);
	if (!port)
	{
		dev_put(dev);
		return -EINVAL;
	}

	size     = maxnum * ETH_ALEN;
	pMacAddr = kmalloc(size, GFP_KERNEL);
	if (!pMacAddr)
	{
		dev_put(dev);
		return -ENOMEM;
	}

	copy_from_user(pMacAddr, userbuf, size);

	pMac = pMacAddr;
	for ( i = 0; i < maxnum; i++ )
	{
#if defined(CONFIG_BCM_KF_BRIDGE_STATIC_FDB)
		if(isStatic)
		{   
			ret = br_fdb_adddel_static(br, port, (const unsigned char *)pMac, 1);
		}
		else
#endif
		{        
#if defined(CONFIG_BCM_KF_VLAN_AGGREGATION) && defined(CONFIG_BCM_VLAN_AGGREGATION)
			br_fdb_update(br, port, (const unsigned char *)pMac, VLAN_N_VID);
#else
			br_fdb_update(br, port, (const unsigned char *)pMac);
#endif
			ret = 0;
		}
		pMac += ETH_ALEN;
	}

	kfree(pMacAddr);

	dev_put(dev);
   
	return ret;
}
#endif

#if defined(CONFIG_BCM_KF_BRIDGE_DYNAMIC_FDB)
/* called with RTNL */
static int del_dyn_fdb_entries(struct net_bridge *br, int ifindex)
{
	struct net_device *dev;
	struct net_bridge_port *port;

	if (!capable(CAP_NET_ADMIN))
	{
		return -EPERM;
	}

	dev = dev_get_by_index(dev_net(br->dev), ifindex);
	if (!dev)
	{
		return -EINVAL;
	}

	port = br_port_get_rtnl(dev);
	if (!port)
	{
		dev_put(dev);
		return -EINVAL;
	}

	br_fdb_delete_by_port(br, port, 0);
	dev_put(dev);
	return 0;
}
#endif

#if defined(CONFIG_BCM_KF_STP_LOOP)
/* called with RTNL */
static int block_stp_loop(struct net_bridge *br, int ifindex)
{
    struct net_device *dev;
    struct net_bridge_port *port;

    dev = dev_get_by_index(dev_net(br->dev), ifindex);
    if (!dev)
    {  
        return -EINVAL;
    }
    
    port = br_port_get_rtnl(dev);
    if (!port)
    {
        dev_put(dev);
        return -EINVAL;
    }
    port->is_bpdu_blocked = !(port->is_bpdu_blocked);
    dev_put(dev);
    printk("%s: port bpdus are %s\n", dev->name, port->is_bpdu_blocked ? "BLOCKED":"unblocked");
    return 0;
}

static int mark_dedicated_stp(struct net_bridge *br, int ifindex, int isDedicated)
{
    struct net_device *dev;
    struct net_bridge_port *port;
    
    dev = dev_get_by_index(dev_net(br->dev), ifindex);
    if (!dev)
    {  
        return -EINVAL;
    }

    port = br_port_get_rtnl(dev);
    
    if (!port)
    {
        dev_put(dev);
        return -EINVAL;
    }
    
    port->is_dedicated_stp_port = !!isDedicated;
    dev_put(dev);
    printk("[%s.%d] stp port %s is %s\n", __func__, __LINE__, dev->name,
           port->is_dedicated_stp_port ? "dedicated":"undedicated");
    return 0;
    
}

#endif

#if defined(CONFIG_BCM_KF_BRIDGE_STATIC_FDB)
/* called with RTNL */
static int delete_fdb_entries(struct net_bridge *br, void __user *userbuf,
			unsigned long maxnum, int ifindex)
{
	struct net_device *dev;
	unsigned char     *pMacAddr = NULL;
	unsigned char     *pMac = NULL;
	int                size;
	int                i;
	int                ret = 0;
	struct net_bridge_port *port;

	if (!capable(CAP_NET_ADMIN))
	{
		return -EPERM;
	}

	dev = dev_get_by_index(dev_net(br->dev), ifindex);
	if (!dev)
	{
		return -EINVAL;
	}

	port = br_port_get_rtnl(dev);
	if (!port)
	{
		dev_put(dev);
		return -EINVAL;
	}

	size     = maxnum * ETH_ALEN;
	pMacAddr = kmalloc(size, GFP_KERNEL);
	if (!pMacAddr)
	{
		dev_put(dev);
		return -ENOMEM;
	}

	copy_from_user(pMacAddr, userbuf, size);

	pMac = pMacAddr;
	for ( i = 0; i < maxnum; i++ )
	{
		ret = br_fdb_adddel_static(br, port, (const unsigned char *)pMac, 0);
		pMac += ETH_ALEN;
	}

	kfree(pMacAddr);

	dev_put(dev);

	return ret;
}

#endif

#if defined(CONFIG_BCM_KF_BRIDGE_DYNAMIC_FDB)
/* called with RTNL */
static int add_fdb_dynamic_entries(struct net_bridge *br, void __user *userbuf,
			   unsigned long maxnum, int ifindex)
{
	return add_fdb_entries(br, userbuf, maxnum, ifindex, 0);
}
#endif

#if defined(CONFIG_BCM_KF_BRIDGE_STATIC_FDB)
/* called with RTNL */
static int add_fdb_static_entries(struct net_bridge *br, void __user *userbuf,
			   unsigned long maxnum, int ifindex)
{
	return add_fdb_entries(br, userbuf, maxnum, ifindex, 1);
}

/* called with RTNL */
static int del_static_fdb_entries(struct net_bridge *br, int ifindex)
{
	struct net_device *dev;
	struct net_bridge_port *port;

	if (!capable(CAP_NET_ADMIN))
	{
		return -EPERM;
	}

	dev = dev_get_by_index(dev_net(br->dev), ifindex);
	if (!dev)
	{
		return -EINVAL;
	}

	port = br_port_get_rtnl(dev);
	if (!port)
	{
		dev_put(dev);
		return -EINVAL;
	}

	br_fdb_delete_by_port(br, port, 2);
	dev_put(dev);
	return 0;
}

#endif

#if defined(CONFIG_BCM_KF_BRIDGE_MAC_FDB_LIMIT) && defined(CONFIG_BCM_BRIDGE_MAC_FDB_LIMIT)
static int get_dev_and_port(struct net_bridge *br, int ifindex,
								struct net_device **dev, struct net_bridge_port **port)
{
	if (!capable(CAP_NET_ADMIN))
	{
		return -EPERM;
	}

	*dev = dev_get_by_index(dev_net(br->dev), ifindex);
	if (!*dev)
	{
		return -EINVAL;
	}

	if (port != NULL)
	{    		
		*port = br_port_get_rtnl(*dev);
		if (!*port)
		{
			dev_put(*dev);
			return -EINVAL;
		}
	}
	return 0;
}

/* called with RTNL */
/* Get FDB limit
 *    lmtType 0: Bridge limit
 *                1: Port limit
 */
static int get_fdb_mac_limit(struct net_bridge *br, int lmt_type,
								 int ifindex, int is_min)
{
	struct net_device *dev = NULL;
	struct net_bridge_port *port = NULL;
	int fdb_limit = 0;
	
	if (lmt_type == 1)
	{    
		if (get_dev_and_port(br, ifindex, &dev, &port) != 0)
		{
			return -EINVAL;
		}
		fdb_limit = br_get_fdb_limit(NULL, port, is_min);           
	}
	else
	{
		fdb_limit = br_get_fdb_limit(br, NULL, is_min);   
	}


	if(dev)
		dev_put(dev);
	return fdb_limit;
}

static int set_fdb_mac_limit(struct net_bridge *br, int fdb_limit,
								int lmt_type, int ifindex, int is_min)
{
	struct net_device *dev = NULL;
	struct net_bridge_port *port = NULL;
	int ret = 0;
	
	if (lmt_type == 1)
	{    
		if (get_dev_and_port(br, ifindex, &dev, &port) != 0)
		{
			return -EINVAL;
		}		
	}
	ret = br_set_fdb_limit(br, port, lmt_type, is_min, fdb_limit);
	if(dev)
		dev_put(dev);
	return ret;
}
#endif

#if defined(CONFIG_BCM_KF_NETFILTER)
static int set_flows(struct net_bridge *br, int rxifindex, int txifindex)
{
	struct net_device *rxdev, *txdev;
	int                ret = 0;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	rxdev = dev_get_by_index(dev_net(br->dev), rxifindex);
	if (rxdev == NULL)
		return -EINVAL;

	txdev = dev_get_by_index(dev_net(br->dev), txifindex);
	if (txdev == NULL)
		return -EINVAL;

   br_flow_blog_rules(br, rxdev, txdev);

   dev_put(rxdev);
   dev_put(txdev);

	return ret;
}
#endif

/* called with RTNL */
static int add_del_if(struct net_bridge *br, int ifindex, int isadd)
{
	struct net_device *dev;
	int ret;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	dev = __dev_get_by_index(dev_net(br->dev), ifindex);
	if (dev == NULL)
		return -EINVAL;

	if (isadd)
		ret = br_add_if(br, dev);
	else
		ret = br_del_if(br, dev);

#if defined(CONFIG_BCM_KF_BRIDGE_PORT_ISOLATION)
	rcu_read_lock();
	br_dev_notify_if_change(&br->dev->name[0]);
	rcu_read_unlock();
#endif
	return ret;
}

/*
 * Legacy ioctl's through SIOCDEVPRIVATE
 * This interface is deprecated because it was too difficult to
 * to do the translation for 32/64bit ioctl compatibility.
 */
static int old_dev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct net_bridge *br = netdev_priv(dev);
#if defined(CONFIG_BCM_KF_BRIDGE_MAC_FDB_LIMIT) && defined(CONFIG_BCM_BRIDGE_MAC_FDB_LIMIT)
	unsigned long args[5];
#else
	unsigned long args[4];
#endif

	if (copy_from_user(args, rq->ifr_data, sizeof(args)))
		return -EFAULT;

	switch (args[0]) {
	case BRCTL_ADD_IF:
	case BRCTL_DEL_IF:
		return add_del_if(br, args[1], args[0] == BRCTL_ADD_IF);

	case BRCTL_GET_BRIDGE_INFO:
	{
		struct __bridge_info b;

		memset(&b, 0, sizeof(struct __bridge_info));
		rcu_read_lock();
		memcpy(&b.designated_root, &br->designated_root, 8);
		memcpy(&b.bridge_id, &br->bridge_id, 8);
		b.root_path_cost = br->root_path_cost;
		b.max_age = jiffies_to_clock_t(br->max_age);
		b.hello_time = jiffies_to_clock_t(br->hello_time);
		b.forward_delay = br->forward_delay;
		b.bridge_max_age = br->bridge_max_age;
		b.bridge_hello_time = br->bridge_hello_time;
		b.bridge_forward_delay = jiffies_to_clock_t(br->bridge_forward_delay);
		b.topology_change = br->topology_change;
		b.topology_change_detected = br->topology_change_detected;
		b.root_port = br->root_port;

		b.stp_enabled = (br->stp_enabled != BR_NO_STP);
		b.ageing_time = jiffies_to_clock_t(br->ageing_time);
		b.hello_timer_value = br_timer_value(&br->hello_timer);
		b.tcn_timer_value = br_timer_value(&br->tcn_timer);
		b.topology_change_timer_value = br_timer_value(&br->topology_change_timer);
		b.gc_timer_value = br_timer_value(&br->gc_timer);
		rcu_read_unlock();

		if (copy_to_user((void __user *)args[1], &b, sizeof(b)))
			return -EFAULT;

		return 0;
	}

	case BRCTL_GET_PORT_LIST:
	{
		int num, *indices;

		num = args[2];
		if (num < 0)
			return -EINVAL;
		if (num == 0)
			num = 256;
		if (num > BR_MAX_PORTS)
			num = BR_MAX_PORTS;

		indices = kcalloc(num, sizeof(int), GFP_KERNEL);
		if (indices == NULL)
			return -ENOMEM;

		get_port_ifindices(br, indices, num);
		if (copy_to_user((void __user *)args[1], indices, num*sizeof(int)))
			num =  -EFAULT;
		kfree(indices);
		return num;
	}

	case BRCTL_SET_BRIDGE_FORWARD_DELAY:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		return br_set_forward_delay(br, args[1]);

	case BRCTL_SET_BRIDGE_HELLO_TIME:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		return br_set_hello_time(br, args[1]);

	case BRCTL_SET_BRIDGE_MAX_AGE:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		return br_set_max_age(br, args[1]);

	case BRCTL_SET_AGEING_TIME:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		br->ageing_time = clock_t_to_jiffies(args[1]);
		return 0;

	case BRCTL_GET_PORT_INFO:
	{
		struct __port_info p;
		struct net_bridge_port *pt;

		rcu_read_lock();
		if ((pt = br_get_port(br, args[2])) == NULL) {
			rcu_read_unlock();
			return -EINVAL;
		}

		memset(&p, 0, sizeof(struct __port_info));
		memcpy(&p.designated_root, &pt->designated_root, 8);
		memcpy(&p.designated_bridge, &pt->designated_bridge, 8);
		p.port_id = pt->port_id;
		p.designated_port = pt->designated_port;
		p.path_cost = pt->path_cost;
		p.designated_cost = pt->designated_cost;
		p.state = pt->state;
		p.top_change_ack = pt->topology_change_ack;
		p.config_pending = pt->config_pending;
		p.message_age_timer_value = br_timer_value(&pt->message_age_timer);
		p.forward_delay_timer_value = br_timer_value(&pt->forward_delay_timer);
		p.hold_timer_value = br_timer_value(&pt->hold_timer);

		rcu_read_unlock();

		if (copy_to_user((void __user *)args[1], &p, sizeof(p)))
			return -EFAULT;

		return 0;
	}

	case BRCTL_SET_BRIDGE_STP_STATE:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		br_stp_set_enabled(br, args[1]);
		return 0;

	case BRCTL_SET_BRIDGE_PRIORITY:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_bh(&br->lock);
		br_stp_set_bridge_priority(br, args[1]);
		spin_unlock_bh(&br->lock);
		return 0;

	case BRCTL_SET_PORT_PRIORITY:
	{
		struct net_bridge_port *p;
		int ret;

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_bh(&br->lock);
		if ((p = br_get_port(br, args[1])) == NULL)
			ret = -EINVAL;
		else
			ret = br_stp_set_port_priority(p, args[2]);
		spin_unlock_bh(&br->lock);
		return ret;
	}

	case BRCTL_SET_PATH_COST:
	{
		struct net_bridge_port *p;
		int ret;

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_bh(&br->lock);
		if ((p = br_get_port(br, args[1])) == NULL)
			ret = -EINVAL;
		else
			ret = br_stp_set_path_cost(p, args[2]);
		spin_unlock_bh(&br->lock);

		return ret;
	}

	case BRCTL_GET_FDB_ENTRIES:
		return get_fdb_entries(br, (void __user *)args[1],
					   args[2], args[3]);

#if defined(CONFIG_BCM_KF_BRIDGE_STATIC_FDB)
	case BRCTL_ADD_FDB_ENTRIES:
		return add_fdb_static_entries(br, (void __user *)args[1],
					   args[2], args[3]);

	case BRCTL_DEL_FDB_ENTRIES:
		return delete_fdb_entries(br, (void __user *)args[1],
					   args[2], args[3]);

	case BRCTL_DEL_STATIC_FDB_ENTRIES:
		return del_static_fdb_entries(br, args[1]);
#endif

#if defined(CONFIG_BCM_KF_BRIDGE_DYNAMIC_FDB)
	case BRCTL_DEL_DYN_FDB_ENTRIES:
		return del_dyn_fdb_entries(br, args[1]);

	case BRCTL_ADD_FDB_DYNAMIC_ENTRIES:
		return add_fdb_dynamic_entries(br, (void __user *)args[1],
					   args[2], args[3]); 
#endif

#if defined(CONFIG_BCM_KF_STP_LOOP)
	case BRCTL_MARK_DEDICATED_STP:
		return mark_dedicated_stp(br, args[1], args[2]);

	case BRCTL_BLOCK_STP:
		return block_stp_loop(br, args[1]);
#endif

#if defined(CONFIG_BCM_KF_NETFILTER)
	case BRCTL_SET_FLOWS:
		return set_flows(br, args[1], args[2]);
#endif

#if defined(CONFIG_BCM_KF_BRIDGE_MAC_FDB_LIMIT) && defined(CONFIG_BCM_BRIDGE_MAC_FDB_LIMIT)
	case BRCTL_GET_BR_FDB_LIMIT:
	{
		int fdb_limit;
		
		fdb_limit = get_fdb_mac_limit(br, args[2], args[3], args[4]);
		if(fdb_limit < 0)
			fdb_limit = 0;
		if (copy_to_user((void __user *)args[1], &fdb_limit, sizeof(fdb_limit)))
			return -EFAULT;
		return 0;
	}

	case BRCTL_SET_BR_FDB_LIMIT:
		return set_fdb_mac_limit(br, args[1], args[2], args[3], args[4]);        
#endif
	}

	return -EOPNOTSUPP;
}

#if (defined(CONFIG_BCM_KF_IGMP) && defined(CONFIG_BR_IGMP_SNOOP)) || (defined(CONFIG_BCM_KF_MLD) && defined(CONFIG_BR_MLD_SNOOP)) || defined(CONFIG_BCM_KF_IGMP_RATE_LIMIT)
static int getDevice(struct net_device **dev, unsigned long arg1)
{
	char buf[IFNAMSIZ];

	if (!capable(CAP_NET_ADMIN))
	{
		return -EPERM;
	}
	if (copy_from_user(buf, (void __user *) arg1, IFNAMSIZ))
	{
		return -EFAULT;
	}
	buf[IFNAMSIZ-1] = 0;
	*dev = dev_get_by_name(&init_net, buf);
	if (*dev == NULL)
	{
		return  -ENXIO; 	/* Could not find device */
	}

	return 0;
}
#endif

static int old_deviceless(struct net *net, void __user *uarg)
{
	unsigned long args[3];

	if (copy_from_user(args, uarg, sizeof(args)))
		return -EFAULT;

	switch (args[0]) {
	case BRCTL_GET_VERSION:
		return BRCTL_VERSION;

	case BRCTL_GET_BRIDGES:
	{
		int *indices;
		int ret = 0;

		if (args[2] >= 2048)
			return -ENOMEM;
		indices = kcalloc(args[2], sizeof(int), GFP_KERNEL);
		if (indices == NULL)
			return -ENOMEM;

		args[2] = get_bridge_ifindices(net, indices, args[2]);

		ret = copy_to_user((void __user *)args[1], indices, args[2]*sizeof(int))
			? -EFAULT : args[2];

		kfree(indices);
		return ret;
	}

	case BRCTL_ADD_BRIDGE:
	case BRCTL_DEL_BRIDGE:
	{
		char buf[IFNAMSIZ];

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (copy_from_user(buf, (void __user *)args[1], IFNAMSIZ))
			return -EFAULT;

		buf[IFNAMSIZ-1] = 0;

		if (args[0] == BRCTL_ADD_BRIDGE)
			return br_add_bridge(net, buf);

		return br_del_bridge(net, buf);
	}
#if defined(CONFIG_BCM_KF_IGMP) && defined(CONFIG_BR_IGMP_SNOOP)
	case BRCTL_ENABLE_SNOOPING:
	{
		struct net_device *dev = NULL;
		struct net_bridge *br;
		int error = getDevice(&dev, args[1]);
		if (error)
		{
			return error;
		}
		
		br = netdev_priv(dev);
		br->igmp_snooping = args[2];
#ifdef SUPPORT_GPL_1
                /*flush igmp fdb table if snooping disable*/
                if (br->igmp_snooping == 0)
                {
                        br_igmp_mc_fdb_cleanup(br);
                }
#endif
		dev_put(dev);

		return 0;
	}

	case BRCTL_ENABLE_IGMP_LAN2LAN_MC:
	{
		struct net_device *dev = NULL;
		struct net_bridge *br;
		int error = getDevice(&dev, args[1]);
		if (error)
		{
			return error;
		}
		
		br = netdev_priv(dev);
		br->igmp_lan2lan_mc_enable= args[2];

		dev_put(dev);

		return 0;
	}

	case BRCTL_GET_IGMP_LAN_TO_LAN_MCAST_ENABLED:
	{
		struct net_device *dev = NULL;
		struct net_bridge *br;
		int enable;
	  
		int error = getDevice(&dev, args[1]);
		if (error)
		{
			return error;
		}

		br = netdev_priv(dev);
		enable = br_mcast_get_lan2lan_snooping (BR_MCAST_PROTO_IGMP, br);
		if (copy_to_user((void __user *)args[2], &enable, sizeof(int)))
		{
			dev_put(dev);
			return -EFAULT;
		}
		dev_put(dev);
		return 0;
	}
#endif
#if defined(CONFIG_BCM_KF_MLD) && defined(CONFIG_BR_MLD_SNOOP)
	case BRCTL_MLD_ENABLE_SNOOPING:
	{
		struct net_device *dev = NULL;
		struct net_bridge *br;
		int error = getDevice(&dev, args[1]);
		if (error)
		{
			return error;
		}
		
		br = netdev_priv(dev);
		br->mld_snooping = args[2];
		if(br->mld_snooping==SNOOPING_DISABLED_MODE) 
			br_mcast_wl_flush(br) ;
		
		dev_put(dev);

		return 0;
	}

	case BRCTL_ENABLE_MLD_LAN2LAN_MC:
	{
		struct net_device *dev = NULL;
		struct net_bridge *br;
		int error = getDevice(&dev, args[1]);
		if (error)
		{
			return error;
		}
		
		br = netdev_priv(dev);
		br->mld_lan2lan_mc_enable= args[2];
		
		dev_put(dev);

		return 0;

	}

	case BRCTL_GET_MLD_LAN_TO_LAN_MCAST_ENABLED:
	{
		struct net_device *dev = NULL;
		struct net_bridge *br;
		int enable;
		int error = getDevice(&dev, args[1]);
		if (error)
		{
			return error;
		}

		br = netdev_priv(dev);
		enable = br_mcast_get_lan2lan_snooping (BR_MCAST_PROTO_MLD, br);
		if (copy_to_user((void __user *)args[2], &enable, sizeof(int)))
		{
			dev_put(dev);
			return -EFAULT;
		}
		dev_put(dev);
		return 0;
	}   
#endif

#if defined(CONFIG_BCM_KF_IGMP_RATE_LIMIT)
	case BRCTL_ENABLE_IGMP_RATE_LIMIT:
	{
		struct net_device *dev = NULL;
		struct net_bridge *br;
		int error = getDevice(&dev, args[1]);
		if (error)
		{
			return error;
		}

		br = netdev_priv(dev);

		if (args[2] > 500)
		{
			dev_put(dev);
			return  -EINVAL; 	/* Could not find device */
		}

		br = netdev_priv(dev);
		br->igmp_rate_limit       = args[2];
		br->igmp_rate_last_packet = ktime_set(0,0);
		br->igmp_rate_bucket      = 0;
		br->igmp_rate_rem_time    = 0;

		dev_put(dev);

		return 0;
	}
#endif

	}

	return -EOPNOTSUPP;
}

int br_ioctl_deviceless_stub(struct net *net, unsigned int cmd, void __user *uarg)
{
	switch (cmd) {
	case SIOCGIFBR:
	case SIOCSIFBR:
		return old_deviceless(net, uarg);

	case SIOCBRADDBR:
	case SIOCBRDELBR:
	{
		char buf[IFNAMSIZ];

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (copy_from_user(buf, uarg, IFNAMSIZ))
			return -EFAULT;

		buf[IFNAMSIZ-1] = 0;
		if (cmd == SIOCBRADDBR)
			return br_add_bridge(net, buf);

		return br_del_bridge(net, buf);
	}
	}
	return -EOPNOTSUPP;
}

int br_dev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct net_bridge *br = netdev_priv(dev);

	switch(cmd) {
	case SIOCDEVPRIVATE:
		return old_dev_ioctl(dev, rq, cmd);

	case SIOCBRADDIF:
	case SIOCBRDELIF:
		return add_del_if(br, rq->ifr_ifindex, cmd == SIOCBRADDIF);

	}

	br_debug(br, "Bridge does not support ioctl 0x%x\n", cmd);
	return -EOPNOTSUPP;
}
