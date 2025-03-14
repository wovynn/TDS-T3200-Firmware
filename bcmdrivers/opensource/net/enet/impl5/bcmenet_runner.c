/*
<:copyright-BRCM:2010:DUAL/GPL:standard

   Copyright (c) 2010 Broadcom Corporation
   All Rights Reserved

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2, as published by
the Free Software Foundation (the "GPL").

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.


A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.

:>
*/


//**************************************************************************
// File Name  : bcmeapi_runner.c
//
// Description: This is Linux network driver for Broadcom Ethernet controller
//
//**************************************************************************
#define VERSION     "0.1"
#define VER_STR     "v" VERSION " " __DATE__ " " __TIME__

#define _BCMENET_LOCAL_
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/init.h>
#include <asm/io.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/mii.h>
#include <linux/skbuff.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/kmod.h>
#include <linux/rtnetlink.h>
#include <net/arp.h>
#include <board.h>
#include <spidevices.h>
#include <bcmnetlink.h>
#include <bcm_map_part.h>
#include <bcm_intr.h>
#include "linux/bcm_assert_locks.h"
#include <linux/stddef.h>
#include <asm/atomic.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/nbuff.h>
#include <linux/kernel_stat.h>
#include "boardparms.h"
#include "bcmenet_common.h"
#include "bcmenet.h"
#include "bcmenet_runner.h"
#if defined(CONFIG_BCM96838) || defined(CONFIG_BCM96848)
#include "phys_common_drv.h"
#endif
/*must be after bcmenet_runner.h */
#include "rdp_cpu_ring_defs.h"
#include "rdp_mm.h"
#include "rdpa_cpu_basic.h"
ENET_RING_S enet_ring[2]  = {};

int g_rxq_stats_received[2]  = {}; /* for every queue */
int g_rxq_stats_dropped[2]  = {};  /* for every queue */
int g_rxq_reason_stats[2][rdpa_cpu_reason__num_of]  = {}; /* reason statistics for US/DS */

extern void * _databuf_alloc(ENET_RING_S *pDescriptor);
extern int rdpa_cpu_ring_not_empty(void*);
extern void rdpa_cpu_ring_rest_desc(volatile void *raw_desc, void *data);
#include "bcmmii.h"
#include "ethsw.h"
#include "ethsw_phy.h"
#include "bcmsw.h"
#include <rdpa_api.h>
#include "ethswdefs.h"
#include "eth_pwrmngt.h"
#include <bcm_map_part.h>
#include "bcm_ethsw.h"

/* Init sequence parameters */
#define INIT_QUEUE_THRESHOLD_VAL 128
#define INIT_GBE_QUEUES_VAL 8
#define INIT_LAN_QUEUES_VAL 4

/* Extern data */
extern enet_global_var_t global;
bdmf_object_handle rdpa_cpu_obj;
rdpa_system_init_cfg_t init_cfg = {};
rdpa_wan_type wan_type = rdpa_wan_none;
int wan_port_id = -1; /* initialize as invalid WAN port */
extern extsw_info_t extSwInfo;
extern int vport_cnt;  /* number of vports: bitcount of Enetinfo.sw.port_map */
typedef unsigned char (*EponGetOamSelFunc)(void);
void* get_oam_sel_func = NULL;
EXPORT_SYMBOL(get_oam_sel_func);

#if defined(CONFIG_BCM96838) || defined(CONFIG_BCM96848)
static int use_cpu_meter = 0;
#else
#define use_cpu_meter 0
#endif

#if !defined(CONFIG_BCM963138) && !defined(CONFIG_BCM963148)
static int bcme_rdpa_tm_queues_cfg(bdmf_object_handle tm, int q_num)
{
    rdpa_tm_queue_cfg_t queue_cfg = {};
    int qid;
    bdmf_error_t rc = BDMF_ERR_OK;

    /* Egress tm queues configuration */
    queue_cfg.drop_alg = rdpa_tm_drop_alg_dt;
    queue_cfg.drop_threshold = INIT_QUEUE_THRESHOLD_VAL;

    /* configure all queues */
    for (qid = 0; qid < q_num; ++qid)
    {
        queue_cfg.queue_id = qid;
        rc = rdpa_egress_tm_queue_cfg_set(tm, qid, &queue_cfg);
        if (rc)
            printk("%s %s Failed to create tm queue id - %d rc(%d)\n", __FILE__, __FUNCTION__, qid, rc);
    }

    return rc;
}

static int bcme_rdpa_init_tm_and_queues_cfg(rdpa_traffic_dir dir,
    int logical_port, int q_num, bdmf_object_handle port)
{
    BDMF_MATTR(tm_attr, rdpa_egress_tm_drv());
    bdmf_object_handle tm;
    rdpa_port_tm_cfg_t port_tm_cfg = {NULL, NULL, NULL};
    bdmf_error_t rc;

    /* Create a scheduler, connect to port and configure a queues */
    rdpa_egress_tm_dir_set(tm_attr, dir);
    rdpa_egress_tm_index_set(tm_attr, logical_port);
    rdpa_egress_tm_level_set(tm_attr, rdpa_tm_level_queue);
    rdpa_egress_tm_mode_set(tm_attr, rdpa_tm_sched_sp);
    rc = bdmf_new_and_set(rdpa_egress_tm_drv(), port, tm_attr, &tm);
    if (rc)
    {
        printk("%s %s Failed to create rdpa egress tm object rc(%d)\n", __FILE__, __FUNCTION__, rc);
        goto Exit;
    }

    /* Configure tm queues*/
    rc = bcme_rdpa_tm_queues_cfg(tm, q_num);
    if (rc)
        goto Exit;

    /* Connect scheduler to port */
    port_tm_cfg.sched = tm;
    rc = rdpa_port_tm_cfg_set(port, &port_tm_cfg);
    if (rc)
        printk("%s %s Failed to configure egress tm to port rc(%d)\n", __FILE__, __FUNCTION__, rc);

Exit:
    if (rc && tm)
        bdmf_put(tm);
    return rc;
}

static int bcme_create_rdpa_wan_gbe_port(void)
{
    BDMF_MATTR(rdpa_port_attrs, rdpa_port_drv());
    bdmf_object_handle rdpa_port_obj;
    rdpa_port_dp_cfg_t port_cfg = {};
    bdmf_error_t rc = BDMF_ERR_OK;

    rdpa_port_index_set(rdpa_port_attrs, rdpa_if_wan0);
    rdpa_port_wan_type_set(rdpa_port_attrs, rdpa_wan_gbe);
    port_cfg.emac = init_cfg.gbe_wan_emac;
    rdpa_port_cfg_set(rdpa_port_attrs, &port_cfg);
    rc = bdmf_new_and_set(rdpa_port_drv(), NULL, rdpa_port_attrs, &rdpa_port_obj);
    if (rc)
    {
        printk("%s %s Failed to create rdpa wan port rc(%d)\n", __FILE__, __FUNCTION__, rc);
        goto exit;
    }

    /* Create a scheduler, connect to port and configure a queues */
    rc = bcme_rdpa_init_tm_and_queues_cfg(rdpa_dir_us, 0, INIT_GBE_QUEUES_VAL, rdpa_port_obj);
    if (rc)
        goto exit;

exit:
    if (rc && rdpa_port_obj)
        bdmf_put(rdpa_port_obj);
    return rc;
}
#endif

static int bcme_create_rdpa_lan_port(struct net_device *dev, rdpa_if *rdpa_port)
{
    BcmEnet_devctrl *pDevCtrl = netdev_priv(dev);
    BDMF_MATTR(rdpa_port_attrs, rdpa_port_drv());
    bdmf_object_handle rdpa_port_obj = NULL;
    bdmf_object_handle switch_port_obj = NULL;
    rdpa_port_dp_cfg_t port_cfg = {};
    bdmf_error_t rc = BDMF_ERR_OK;
    bdmf_boolean is_ext_sw = init_cfg.runner_ext_sw_cfg.enabled;
    int logical_port = ((BcmEnet_devctrl *)netdev_priv(dev))->sw_port_id;

#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
    /* XXX: FIXME */
    /* Runner based device with external switch supports LAN port only on external switch */
    if (is_ext_sw && LOGICAL_PORT_TO_UNIT_NUMBER(logical_port) == 0) 
        goto Exit;
#endif

    *rdpa_port = rdpa_if_lan0 + LOGICAL_PORT_TO_PHYSICAL_PORT(logical_port);
#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
    if (*rdpa_port == rdpa_if_lan7)
        *rdpa_port = rdpa_if_lan6;
#endif
#ifndef BRCM_FTTDP
    rdpa_port_index_set(rdpa_port_attrs, *rdpa_port);
#else
    /* XXX: FW workaround, debug port cpu interface always expected to be on index lan21,
     * also assume fttdp board will never have external switch. */
    if (LOGICAL_PORT_TO_PHYSICAL_PORT(logical_port) == g9991_bp_debug_port)
    {
        *rdpa_port = G9991_DEBUG_RDPA_PORT;
        /* XXX FIXME */
        is_ext_sw = 0;
        rdpa_port_index_set(rdpa_port_attrs, *rdpa_port);
    }
    else
    {
        *rdpa_port = rdpa_if_lan0 + netdev_path_get_hw_port(dev);
        rdpa_port_index_set(rdpa_port_attrs, *rdpa_port); /* Must be before cfg */
        port_cfg.physical_port = pDevCtrl->physical_inx; /* ??? */
        port_cfg.emac = rdpa_emac_none;
        rdpa_port_cfg_set(rdpa_port_attrs, &port_cfg);
    }
#endif

    if (!is_ext_sw)
    {
        if (init_cfg.runner_ext_sw_cfg.type != rdpa_brcm_fttdp)
        {
            port_cfg.sal_enable = 1;
            port_cfg.dal_enable = 1;
            port_cfg.sal_miss_action = rdpa_forward_action_host;
            port_cfg.dal_miss_action = rdpa_forward_action_host;
        }

        port_cfg.emac = (rdpa_emac)logical_port;
        rdpa_port_cfg_set(rdpa_port_attrs, &port_cfg);

    }
    else
    {
        /* get the rdpa switch port in order to configure as owner to extswitch lan ports */
        rc = rdpa_port_get(rdpa_if_switch, &switch_port_obj);
        if (rc)
        {
            printk("%s %s Failed to get rdpa switch port rc(%d)\n", __FILE__, __FUNCTION__, rc);
            goto Exit;
        }
    }

    rc = bdmf_new_and_set(rdpa_port_drv(), switch_port_obj, rdpa_port_attrs, &rdpa_port_obj);
    if (rc)
    {
        printk("%s %s Failed to create rdpa lan port rc(%d)\n", __FILE__, __FUNCTION__, rc);
        goto Exit;
    }

    /* Save the port if in dev */
    pDevCtrl->rdpa_port = *rdpa_port;

#if !defined(CONFIG_BCM963138) && !defined(CONFIG_BCM963148)
    rc = bcme_rdpa_init_tm_and_queues_cfg(rdpa_dir_ds, (*rdpa_port-rdpa_if_lan0), INIT_LAN_QUEUES_VAL, rdpa_port_obj);
#endif

Exit:
    if (rc && rdpa_port_obj)
        bdmf_put(rdpa_port_obj);
    if (switch_port_obj)
        bdmf_put(switch_port_obj);
    return rc;
}

int bcmeapi_should_create_vport(int logical_port)
{
    int ret_val = 1;

#if defined(ENET_GPON_CONFIG)
    /* Skip creating Liux interface for GPON port during create_vport()*/
    ret_val &= (logical_port == GPON_PORT_ID) ? 0 : 1;
#endif /* ENET_GPON_CONFIG */
#if defined(ENET_EPON_CONFIG)
    /* Skip creating Linux interface for EPON port during create_vport()*/
    ret_val &= (logical_port == EPON_PORT_ID) ? 0 : 1;
#endif /* ENET_EPON_CONFIG */
    return ret_val;
}

int bcmeapi_create_vport(struct net_device *dev)
{
    BcmEnet_devctrl *pDevCtrl = netdev_priv(dev);
    rdpa_if          rdpa_port;
    bdmf_error_t     rc;
#if defined(STAR_FIGHTER2)
    int              logical_port;
    int              unit;
    int              physical_port;
    int              phy_conn;
    BcmEnet_devctrl *pVnetDev0 = (BcmEnet_devctrl *)netdev_priv(vnet_dev[0]);
#endif

    pDevCtrl->default_txq = 0; /* BCM_GMP_MW_UNCLASSIFIED_TRAFFIC_RC; TBD */
#if !defined(CONFIG_BCM963138) && !defined(CONFIG_BCM963148)
    if (dev->base_addr == GPON_PORT_ID || dev->base_addr == EPON_PORT_ID)
        return 0;

    /* Check and configure if this is GBE port */
    if (init_cfg.gbe_wan_emac != rdpa_emac_none && dev->base_addr == init_cfg.gbe_wan_emac)
    {
        rc = bcme_create_rdpa_wan_gbe_port();
        if (rc)
        {
            printk("%s %s Failed to configure rdpa wan port rc(%d)\n", __FILE__, __FUNCTION__, rc);
            goto exit;
        }
        pDevCtrl->wanPort |= (1 << dev->base_addr);
        dev->priv_flags |= IFF_WANDEV;
        pDevCtrl->rdpa_port = rdpa_if_wan0; 
        return 0;
    }

    if ((rdpa_emac)dev->base_addr == rdpa_emac5) /* EMAC5 will always be created from ioctl */
        return 0;
#endif

    rc = bcme_create_rdpa_lan_port(dev, &rdpa_port);
    if (rc)
    {
        printk("%s %s Failed to configure rdpa lan port rc(%d)\n", __FILE__, __FUNCTION__, rc);
        goto exit;
    }        

#if defined(STAR_FIGHTER2)
    logical_port = pDevCtrl->sw_port_id;
    physical_port = LOGICAL_PORT_TO_PHYSICAL_PORT(logical_port);
    unit = LOGICAL_PORT_TO_UNIT_NUMBER(logical_port);
    phy_conn = pVnetDev0->EnetInfo[unit].sw.phyconn[physical_port];
    if ( PHY_CONN_TYPE_PLC == phy_conn )
    {
        pVnetDev0->learningDisabledPortMap |= (1<<logical_port);
        ethsw_set_rx_tx_flow_control(logical_port, 1, 0);
    }
#endif

exit:
    return rc;
}

#if defined(CONFIG_BCM96838) || defined(CONFIG_BCM96848)
/* This is a hack for emac5 wan, which will be known if it is applicable only after wanconf loads:
 * device interface will always be created, but rdpa object will be only created if this is triggered */
char *rdpa_init_wan_gbe_emac(void)
{
    int port;

    /* init_cfg.gbe_wan_emac is expected to be none if gbe emac5 is used -
     * will be set to emac5 later when rdpa object port/index=wan0 with wan_type gbe is created */
    if (init_cfg.gbe_wan_emac != rdpa_emac_none)
        return NULL;

    wan_port_id = init_cfg.gbe_wan_emac = rdpa_emac5;
    wan_type = rdpa_wan_gbe;

    /* Find net_device with base addr of emac5 in order to create rdpa port wan0 for it */
    for (port = 1; port <= vport_cnt; port++)
    {
        if (!vnet_dev[port])
            continue;

        if (vnet_dev[port]->base_addr == wan_port_id)
        {
            bcmeapi_create_vport(vnet_dev[port]);

            return vnet_dev[port]->name;
        }
    }

    return NULL;
}
#endif

static int cpu_meter_idx[2] = { BDMF_INDEX_UNASSIGNED, BDMF_INDEX_UNASSIGNED };
static void cpu_meter_idx_init(void)
{
    /* XXX: Scan and find available meters */
    cpu_meter_idx[rdpa_dir_ds] = 1;
    cpu_meter_idx[rdpa_dir_us] = 1;
}

#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
static int __is_gbe_mode(void)
{
    return (wan_type == rdpa_wan_gbe);
}
#endif

static int reason2queue_cfg_skip(rdpa_cpu_reason_index_t *reason_cfg_idx, rdpa_cpu_reason_cfg_t *reason_cfg)
{
#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
    if (!__is_gbe_mode() && reason_cfg_idx->dir == rdpa_dir_ds)
    {
        return 1;
    }
#endif        

    /* Any third party already configured the reason? */
    if (reason_cfg &&
        reason_cfg->queue != NETDEV_CPU_RX_QUEUE_ID &&
        reason_cfg->queue != NETDEV_CPU_HI_RX_QUEUE_ID
       )
    {
        return 1;
    }

    /* One of the reasons which we defenitely don't handle? */
    if ((reason_cfg_idx->reason == rdpa_cpu_rx_reason_oam)||
        (reason_cfg_idx->reason == rdpa_cpu_rx_reason_omci)||
        (reason_cfg_idx->reason >= rdpa_cpu_rx_reason_direct_queue_0 &&
        reason_cfg_idx->reason <= rdpa_cpu_rx_reason_direct_queue_7)
#if defined(ENET_EPON_CONFIG)
        || (reason_cfg_idx->reason == rdpa_cpu_rx_reason_etype_udef_2) ||
        (reason_cfg_idx->reason == rdpa_cpu_rx_reason_etype_udef_3)
#endif
       )
    {
        return 1;
    }
    return 0;
}

static int reason2meter_cfg_skip(rdpa_cpu_reason_index_t *reason_cfg_idx, rdpa_cpu_reason_cfg_t *reason_cfg)
{
    if (reason2queue_cfg_skip(reason_cfg_idx, reason_cfg))
        return 1; /* Reason is not trapped to NETDEV_CPU_RX_QUEUE_ID */

    switch (reason_cfg_idx->reason)
    {
        /* Traffic we do want to skip from being metered */
    case rdpa_cpu_rx_reason_local_ip:
    case rdpa_cpu_rx_reason_unknown_sa:
    case rdpa_cpu_rx_reason_unknown_da:
    case rdpa_cpu_rx_reason_ip_frag:
    case rdpa_cpu_rx_reason_non_tcp_udp:
    case rdpa_cpu_rx_reason_ip_flow_miss:
        return 0;
    case rdpa_cpu_rx_reason_bcast:
#if defined(ENET_EPON_CONFIG)
#define OUI_DPOE 4
        if (get_oam_sel_func != NULL)
        {
            if ((*(EponGetOamSelFunc)get_oam_sel_func)() == (1<<OUI_DPOE))
                return 1; /*In this case, DPoE module will take care of meter*/
        }
#endif
        return 0;
    default:
        break;
    }

    return 1;
}

static void _rdpa_cpu_set_reasons_to_meter(int cpu_meter_idx_ds, int cpu_meter_idx_us)
{
    rdpa_cpu_reason_index_t reason_cfg_idx = {BDMF_INDEX_UNASSIGNED, BDMF_INDEX_UNASSIGNED};
    rdpa_cpu_reason_cfg_t reason_cfg = {};
    int rc;

    while (!rdpa_cpu_reason_cfg_get_next(rdpa_cpu_obj, &reason_cfg_idx))
    {
        rdpa_cpu_reason_cfg_get(rdpa_cpu_obj, &reason_cfg_idx, &reason_cfg);

        /* for US: if meter_ports mask=0, apply to ALL LAN PORTS */
        if ((!reason_cfg.meter_ports) && (reason_cfg_idx.dir == rdpa_dir_us) )
        {
            if (rdpa_cpu_is_per_port_metering_supported(reason_cfg_idx.reason))
                reason_cfg.meter_ports = rdpa_ports_all_lan(); 
        }

        if (reason2meter_cfg_skip(&reason_cfg_idx, &reason_cfg))
            continue;
        reason_cfg.meter = reason_cfg_idx.dir == rdpa_dir_ds ? cpu_meter_idx_ds : cpu_meter_idx_us;
#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
        reason_cfg_idx.table_index = (reason_cfg_idx.dir == rdpa_dir_us)
            ? CPU_REASON_LAN_TABLE_INDEX : CPU_REASON_WAN1_TABLE_INDEX;
#endif
        rc = rdpa_cpu_reason_cfg_set(rdpa_cpu_obj, &reason_cfg_idx, &reason_cfg);
        if (rc < 0)
            printk(KERN_ERR CARDNAME ": Error (%d) configuring CPU reason to meter\n", rc);
    }
}

/* This is the fibunachi series of rates. */
#define NUM_OF_SUGGESTED_RATES 9
static int recommended_rates[NUM_OF_SUGGESTED_RATES] = { 1000, 2000, 3000, 5000, 8000, 13000, 21000, 34000, 40000 };
static inline void _rdpa_cfg_cpu_meter(int increase_rate)
{
    int i, prev_rate_idx;
    rdpa_cpu_meter_cfg_t meter_cfg;
    rdpa_dir_index_t dir_idx;
    static int rate_idx = 1; /* For the first time, need 1 in order to be configured, in order to start from rate 0 */

    prev_rate_idx = rate_idx;
    if (increase_rate)
    {
        if (rate_idx == NUM_OF_SUGGESTED_RATES)
            return; /* skip configuration */
        rate_idx++;
    }
    else
    {
        if (!rate_idx)
            return; /* skip configuration */
        rate_idx--;
    }

    if (rate_idx == NUM_OF_SUGGESTED_RATES)
    {
        /* We reached the maximal meter and want to increase - remove the rate limiter */
        _rdpa_cpu_set_reasons_to_meter(BDMF_INDEX_UNASSIGNED, BDMF_INDEX_UNASSIGNED);
        return;
    }
    else if (prev_rate_idx == NUM_OF_SUGGESTED_RATES)
    {
        /* The only case when prev_rate_idx can be NUM_OF_SUGGESTED_RATES is when we didn't have a meter, and got a call
         * to decrease the rate. In this case, we want to put the meter back. */
        _rdpa_cpu_set_reasons_to_meter(cpu_meter_idx[rdpa_dir_ds], cpu_meter_idx[rdpa_dir_us]);
        return;
    }

    for (i = rdpa_dir_ds; i <= rdpa_dir_us; i++)
    {
        dir_idx.index = cpu_meter_idx[i];
        dir_idx.dir = i;
        meter_cfg.sir = recommended_rates[rate_idx];
        rdpa_cpu_meter_cfg_set(rdpa_cpu_obj, &dir_idx , &meter_cfg);
    } 
}

int bcmeapi_module_init(void)
{
    struct net_device *management_eth_dev;
    bdmf_object_handle system_obj = NULL;
    int rc;

    if (rdpa_cpu_get(rdpa_cpu_host, &rdpa_cpu_obj))
        return -ESRCH;

    rc = rdpa_system_get(&system_obj);
    if (rc)
    {
        printk("Failed to getting RDPA System object\n");
        return rc;
    }
    
    rdpa_system_init_cfg_get(system_obj, &init_cfg);
    bdmf_put(system_obj);

    /* Unload kernel's initial driver for eth0. */
    management_eth_dev = dev_get_by_name(&init_net, "eth0");
    if (management_eth_dev)
    {
        management_eth_dev->netdev_ops->ndo_stop(management_eth_dev);
        dev_put(management_eth_dev);
        unregister_netdev(management_eth_dev);
        free_netdev(management_eth_dev); /* this will wait until refcnt is zero */
    }

    if (init_cfg.gbe_wan_emac != rdpa_emac_none)
    {
        wan_type = rdpa_wan_gbe;
#if defined(CONFIG_BCM96838) || defined(CONFIG_BCM96848)
        wan_port_id = init_cfg.gbe_wan_emac; /* gbe wan port will always be emac number */
#else
        /*
         * In DSL mode the WAN is either Ethernet or XTM. There is no need to detect the 
         * Ethernet WAN type, because there is only one type (DSL packets are received
         * by the XTM driver). Hence, initializing wan_port_id to emac0 as default is sufficient
         */
        wan_port_id = rdpa_emac0;
#endif
    }

    if (use_cpu_meter)
    {
        cpu_meter_idx_init();
        _rdpa_cfg_cpu_meter(0);
    }

    return 0;
}

#if defined(STAR_FIGHTER2)

/* TBD - These crossbar changes need to be moved to separate file(s) to avoid clutter */
/*
 * ext_port is one of BCMENET_CROSSBAR_MAX_EXT_PORTS phys/ports available on Crossbar
 * int_port is one of the BCMENET_CROSSBAR_MAX_INT_PORTS ports available on Crossbas (1 port for Runner P_WAN and others for switch) 
 */

/* External Crossbar Ports Numbers
 *  00 - SERDES                              = P9
 *  01 - Single GPHY4                        = P10
 *  02 - RGMII3                              = P11
 *  03 - MII/TMII/RvMII/RGMII                = P12
 *  04 - GPHY3 (Only applicable to 63138B0)  = P13
 *  
 * 4x2 Crossbar 
 *  Bit 01:00 -- Switch Port P4 -- Internal Crossbar Port#0
 *  Bit 03:02 -- RDP WAN Port -- Internal Crossbar Port#1
 *  
 * 5x3 Crossbar 
 *  Bit 02:00 -- Switch Port P3 -- Internal Crossbar Port#0
 *  Bit 05:03 -- Switch Port P4 -- Internal Crossbar Port#1
 *  Bit 08:06 -- RDP WAN Port   -- Internal Crossbar Port#2
 *  Bit    09 -- WAN Port Link Status as programmed by Software 
 *  Bit    10 -- WAN Port Link Status Source (Software or Switch); THIS IS USED FOR WAN LED STATUS ONLY
 *                  ;HW/Switch can get the link status for Internal GPHY and SERDES but not for RGMII or external PHYs;
 *                  ;So this should be updated for RGMIIs or external PHYs i.e. set bit:10 and update the link status in bit:09
 */ 
static int crossbar_select(int ext_port, int int_port)
{
    u32 val32 = 0;
    volatile u32 *cb_mux_reg = (void *)(SWITCH_BASE + CROSSBAR_SWITCH_REG);
    if ( !SF2_VALID_CB_INT_PORT(int_port) || !SF2_VALID_CB_EXT_PORT(ext_port) ) {
        return -BCM_E_PARAM;
    }
    val32 =  *cb_mux_reg; /* Locally store current register config */
    val32 &= ~(CB_PHY_PORT_MASK << (int_port * CB_PHY_PORT_SHIFT)); /* Reset config for the port */
    *cb_mux_reg = val32 | (ext_port & CB_PHY_PORT_MASK) << (int_port * CB_PHY_PORT_SHIFT); 

    printk("Cross Bar MUX Config : Internal Port %02d maps to External Port %02d <reg_val : 0x%08x>\n",
        int_port,ext_port,*cb_mux_reg);

    return 0;
}

/*
   Crossbar Internal Port to Switch/Runner Logic Description
   Convert from [SW][Port] to Crossbar Int port
   */
typedef struct enetSwPort_s {
    int unit;
    int port;
} enetSwPort_t;

/* Below are fixed mapping based on HW/chip
   i.e. which SF2/Runner port connected to which internal crossbar port */
#if defined(CONFIG_5x3_CROSSBAR_SUPPORT) /* 5x3 crossbar */
static const enetSwPort_t muxInt2SwPort[BCMENET_CROSSBAR_MAX_INT_PORTS] = {{1,3}, {1,4}, {0, 0}};
#else
static const enetSwPort_t muxInt2SwPort[BCMENET_CROSSBAR_MAX_INT_PORTS] = {{1,4}, {0, 0}}; 
#endif
/* Below array has the reverse mapping of muxInt2SwPort
 * This array is populated once at init time */
static int swPort2MuxInt[BP_MAX_ENET_MACS][BP_MAX_SWITCH_PORTS] = 
    {[0 ... (BP_MAX_ENET_MACS-1)][0 ... (BP_MAX_SWITCH_PORTS-1)] = -1};

/* Below array stores the dynamic/run-time mapping of
   Crossbar Internal <-> External Port */
static int muxInt2Ext[BCMENET_CROSSBAR_MAX_INT_PORTS] = {[0 ... (BCMENET_CROSSBAR_MAX_INT_PORTS-1)] = BP_CROSSBAR_NOT_DEFINED};

static void crossbar_update_wan_link_status(int unit, int physical_port, int cb_port, int lnk_status)
{
/* 63138B0 onwards 5x3 crossbar supports WAN LED and need link status for RGMII ports */
/* 63148 4x2 crossbar supports WAN LED */
#if defined(CONFIG_5x3_CROSSBAR_SUPPORT) || defined(CONFIG_BCM963148)
    u32 val32 = 0;
    volatile u32 *cb_mux_reg = (void *)(SWITCH_BASE + CROSSBAR_SWITCH_REG);
#if defined(SUPPORT_GPL_1)
    ETHERNET_MAC_INFO *info = EnetGetEthernetMacInfo();
    unsigned long phy_id;
    int software_src = 0;
    /* Get phy_id of the crossbar port currently connected to the WAN port */
    phy_id = info[unit].sw.crossbar[cb_port].phy_id;

    if (IsRGMII(phy_id) || ((phy_id & MAC_IFACE) == MAC_IF_SERDES))
    {
        software_src = 1; /* Source of the LED is software when RGMII or SERDES is connected */
        /*
         * Workaround to to not blink link LEDs corresponding to
         * WAN Ethernet when WAN MoCA or WAN SFP is Active. The workaround
         * is to force link status to DOWN in Crossbar Switch register.
         */
        lnk_status = 0;
    }
#endif

    val32 =  *cb_mux_reg; /* Locally store the crossbar control register */
    val32 &= ~(CB_WAN_LNK_STATUS_MASK | CB_WAN_LNK_STATUS_SRC_MASK); /* Clear both Link Status and Source */
#if defined(SUPPORT_GPL_1)
    val32 |= ( (lnk_status&1) << CB_WAN_LNK_STATUS_SHIFT ) | ( software_src << CB_WAN_LNK_STATUS_SRC_SHIFT );
#else
    val32 |= (lnk_status&1) << CB_WAN_LNK_STATUS_SHIFT;
#endif

    /* Source of the LED is software when RGMII is connected */
#if !defined(CONFIG_BCM963148)
    if ( IsRGMII(EnetGetEthernetMacInfo()[unit].sw.crossbar[cb_port].phy_id))
    {
        val32 |= CB_WAN_LNK_STATUS_SRC_MASK;
    }
#endif
    *cb_mux_reg = val32 ; 

#if defined(SUPPORT_GPL_1)
    printk("Cross Bar MUX Config : LED reg change ; software driven <%d> link <%d> <reg_val : 0x%08x>\n",
           software_src,lnk_status,val32);
#else
    printk("Cross Bar MUX Config : LED reg change ; software driven <%d> link <%d> <reg_val : 0x%08x>\n",
        val32&CB_WAN_LNK_STATUS_SRC_MASK, lnk_status, val32);
#endif

#endif
}

/*
    static void serdes_work_around(int phy_id)
    Serdes work around during 100FX for dependency on Serdes status
    phy_id:
        -1: Initialize work around.
        Others: current link down PHY ID.
*/
static void serdes_work_around(int phy_id)
{
    static int serdesMuxExtPort, serdesMuxIntPort;
    int phyId, unit, port, logPort, cb_port;
    ETHERNET_MAC_INFO *info;

    if(phy_id == -1)    /* Init work around */
    {
        info = EnetGetEthernetMacInfo();

        /* Search and record Serdes port for hardware work around */
        for (unit=0; unit < BP_MAX_ENET_MACS; unit++)
        {
            for (port = 0; (port < BP_MAX_SWITCH_PORTS); port++)
            {
                logPort = PHYSICAL_PORT_TO_LOGICAL_PORT(port,unit);
                for(cb_port = enet_get_first_crossbar_port(logPort);
                        cb_port != BP_CROSSBAR_NOT_DEFINED;
                        cb_port = enet_get_next_crossbar_port(logPort, cb_port)) 
                {
                    phyId = info[unit].sw.phy_id[port];
                    if ((phyId & MAC_IFACE) == MAC_IF_SERDES)
                    {
                        /* Found Serdes port, go to end */
                        serdesMuxExtPort = cb_port;
                        serdesMuxIntPort = swPort2MuxInt[unit][port];
                        goto end;
                    }
                }
            }
        }
        /* No Serdes found */
        serdesMuxExtPort = -1;
        return;
    }
    
    if (serdesMuxExtPort == -1 || (ethsw_sfp_get_speed(6) & BMCR_SPEED100) == 0 || 
        (phy_id & MAC_IFACE) == MAC_IF_SERDES)
    {
        /* If no Serdes in layout, no 100FX configured or Link down is Serdes, no action is taken */
        return;
    }
end:
    crossbar_select(serdesMuxExtPort, serdesMuxIntPort);
}

static void config_crossbar_init(void)
{
    ETHERNET_MAC_INFO *info = EnetGetEthernetMacInfo();
    int sw_port, sw_unit, muxExtPort, muxIntPort;
    int muxExt2Int[BCMENET_CROSSBAR_MAX_EXT_PORTS] = {[0 ... (BCMENET_CROSSBAR_MAX_EXT_PORTS-1)] = BP_CROSSBAR_NOT_DEFINED};

    for (muxIntPort = 0; muxIntPort < BCMENET_CROSSBAR_MAX_INT_PORTS; muxIntPort++)
    {
        /* Populate the Switch/Runner port to Crossbar/Mux Internal Port Mapping
         * This is the only time we store this mapping and it never changes */
        swPort2MuxInt[muxInt2SwPort[muxIntPort].unit][muxInt2SwPort[muxIntPort].port] = muxIntPort;
    }

    for (sw_unit = 0; sw_unit < BP_MAX_ENET_MACS; sw_unit++)
    {
        for (muxExtPort = 0; muxExtPort < BCMENET_CROSSBAR_MAX_EXT_PORTS; muxExtPort++) {
            sw_port = info[sw_unit].sw.crossbar[muxExtPort].switch_port;
            if (sw_port == BP_CROSSBAR_NOT_DEFINED)
            {
                continue;
            }
            /* Now we have the mapping from <unit,port> to <muxExtPort> : based on boardparms;
             * Find out which internal crossbar port this <unit,port> maps to
             * There should never be invalid internal port if boardparms are correct */
            muxIntPort = swPort2MuxInt[sw_unit][sw_port];
            if (muxIntPort == -1)
            {
                printk(" Error: Invalid cross port definition found: unit %d, port %d\n", sw_unit, sw_port);
                continue;
            }

            /* Check if this external crossbar port is already mapped to any internal crossbar port */
            if (muxExt2Int[muxExtPort] != BP_CROSSBAR_NOT_DEFINED)
            {
                printk("\nERROR : sw:ext_cb <%d:%d> : Duplicate MUX mapping <%d:%d> and <%d:%d> \n\n",
                        sw_unit, muxExtPort, muxExt2Int[muxExtPort], muxExtPort, muxIntPort, muxExtPort);
                continue;
            }

            muxExt2Int[muxExtPort] = muxIntPort;
            if(muxInt2Ext[muxIntPort] == -1) /* Skip already configured for 12N dyanmic mapping */
            {
                muxInt2Ext[muxIntPort] = muxExtPort;
            }

            printk("%s Port#%d (Internal MUX Port#%d) connects to Crossbar Port#%d\n",
                    sw_unit? "Switch": "Runner", sw_port, muxIntPort, muxExtPort);
        }
    }

    /* Now assign unique ports to Internal Mux ports that are not configured through board params */
    for (muxIntPort = 0; muxIntPort < BCMENET_CROSSBAR_MAX_INT_PORTS; muxIntPort++) {
        if (muxInt2Ext[muxIntPort] == -1) { /* Not configured through Board params */
            /* Find unused port */
            for (muxExtPort = 0; muxExtPort < BCMENET_CROSSBAR_MAX_EXT_PORTS; muxExtPort++) {
                if (muxExt2Int[muxExtPort] == -1) { /* This external port is not mapped yet */
                    muxInt2Ext[muxIntPort] = muxExtPort;
                    printk("Unused MUX Internal Port#%d connects to Crossbar External Port#%d\n",
                        muxIntPort, muxExtPort);
                    break; /* Move on to the next Internal Mux Port */
                }
            }
        }
    }

    /* Configure the MUX based on mapping */
    for (muxIntPort = 0; muxIntPort < BCMENET_CROSSBAR_MAX_INT_PORTS; muxIntPort++) {
        sw_unit = muxInt2SwPort[muxIntPort].unit;
        sw_port = muxInt2SwPort[muxIntPort].port;
        muxExtPort = muxInt2Ext[muxIntPort];
        info[sw_unit].sw.phy_id[sw_port] = info[sw_unit].sw.crossbar[muxExtPort].phy_id;
#if defined(SUPPORT_GPL_1)
        info[sw_unit].sw.port_flags[sw_port] = info[sw_unit].sw.crossbar[muxExtPort].port_flags;
#endif
        crossbar_select(muxInt2Ext[muxIntPort], muxIntPort); 
    }

    serdes_work_around(-1);
}
#endif

static int bcme_rdpa_create_rdpa_switch_port(rdpa_emac emac_id)
{
    int rc;
    BDMF_MATTR(rdpa_port_attrs, rdpa_port_drv());
    bdmf_object_handle rdpa_port_obj;
    bdmf_object_handle rdpa_filter_obj = NULL;
    rdpa_port_dp_cfg_t port_cfg = {};

    rdpa_port_index_set(rdpa_port_attrs, rdpa_if_switch);
    port_cfg.emac = emac_id;
    rdpa_port_cfg_set(rdpa_port_attrs, &port_cfg);


    rc = bdmf_new_and_set(rdpa_port_drv(), NULL, rdpa_port_attrs, &rdpa_port_obj);
    if (rc)
    {
        printk("%s %s Failed to create port object rc(%d)\n", __FILE__, __FUNCTION__, rc);
        goto Exit;
    }

Exit:
    if (rdpa_filter_obj)
        bdmf_put(rdpa_filter_obj);
    return rc;
}

int bcmeapi_ethsw_init()
{
    int rc = 0;

    if (init_cfg.runner_ext_sw_cfg.enabled)
    {
        rc = bcme_rdpa_create_rdpa_switch_port(init_cfg.runner_ext_sw_cfg.emac_id);
        if (rc) {
            printk("%s %s Failed to create rdpa switch port rc(%d)\n", __FILE__, __FUNCTION__, rc);
            goto Exit;
        }
    }

#if defined(STAR_FIGHTER2)
    bcm_ethsw_init();
    config_crossbar_init();
#if defined(SWITCH_REG_SINGLE_SERDES_CNTRL)
    ethsw_init_serdes();
#endif
#endif

Exit:
    return rc;
}

void bcmeapi_module_init2(void)
{
}

void bcmeapi_enet_module_cleanup(void)
{
    bdmf_put(rdpa_cpu_obj);
}

static void bcmeapi_enet_isr(long queue_id)
{
    BcmEnet_devctrl *pDevCtrl = netdev_priv(vnet_dev[0]);

    rdpa_cpu_int_disable(rdpa_cpu_host, NETDEV_CPU_HI_RX_QUEUE_ID);
    rdpa_cpu_int_clear(rdpa_cpu_host, NETDEV_CPU_HI_RX_QUEUE_ID);
    rdpa_cpu_int_disable(rdpa_cpu_host, NETDEV_CPU_RX_QUEUE_ID);
    rdpa_cpu_int_clear(rdpa_cpu_host, NETDEV_CPU_RX_QUEUE_ID);
    BCMENET_WAKEUP_RXWORKER(pDevCtrl);
}

void bcmeapi_buf_alloc(BcmEnet_devctrl *pDevCtrl) 
{ 
}

int bcmeapi_queue_select(EnetXmitParams *pParam)
{
    return BCMEAPI_CTRL_CONTINUE;
}

void bcmeapi_napi_post(struct BcmEnet_devctrl *pDevCtrl)
{
    rdpa_cpu_int_enable(rdpa_cpu_host, NETDEV_CPU_HI_RX_QUEUE_ID);
    rdpa_cpu_int_enable(rdpa_cpu_host, NETDEV_CPU_RX_QUEUE_ID);
    /* If the queue got full while the network driver was handling previous
     * packets, then new packets will not cause interrupt (they will be
     * simply dropped by Runner without interrupt). In this case, no one
     * will wake up the network driver again, and traffic will stop. So, the
     * solution is to schedule another NAPI round that will flush the queue. */
    /*just check if ring is full*/

    if (rdpa_cpu_ring_not_empty(enet_ring[NETDEV_CPU_RX_QUEUE_ID - NETDEV_CPU_RX_QUEUE_ID_BASE].head) ||
        rdpa_cpu_ring_not_empty(enet_ring[NETDEV_CPU_HI_RX_QUEUE_ID - NETDEV_CPU_RX_QUEUE_ID_BASE].head))
    {
        rdpa_cpu_int_disable(rdpa_cpu_host, NETDEV_CPU_HI_RX_QUEUE_ID);
        rdpa_cpu_int_clear(rdpa_cpu_host, NETDEV_CPU_HI_RX_QUEUE_ID);
        rdpa_cpu_int_disable(rdpa_cpu_host, NETDEV_CPU_RX_QUEUE_ID);
        rdpa_cpu_int_clear(rdpa_cpu_host, NETDEV_CPU_RX_QUEUE_ID);
        BCMENET_WAKEUP_RXWORKER(pDevCtrl);
    }
}

void bcmeapi_get_tx_queue(EnetXmitParams *pParam)
{
}

void bcmeapi_add_dev_queue(struct net_device *dev)
{
#if !defined(CONFIG_BCM963138) && !defined(CONFIG_BCM963148)
    int lan_port;
    BcmEnet_devctrl *pDevCtrl = netdev_priv(dev);
    if (pDevCtrl->sw_port_id != GPON_PORT_ID && pDevCtrl->sw_port_id !=
        SID_PORT_ID)
    {
        lan_port = rdpa_emac0 + pDevCtrl->sw_port_id;
        pDevCtrl->phy_addr = BpGetPhyAddr(0,lan_port);
#if defined(CONFIG_BCM96838) || defined(CONFIG_BCM96848)
        phy_auto_enable(lan_port);
#if defined(CONFIG_BCM_ETH_HWAPD_PWRSAVE)
        ethsw_setup_hw_apd(1);
#endif
        ethsw_eee_enable_phy(lan_port);
#endif
    }
#endif
}

void _rdp_databuff_free(void* databuff)
{
    cache_invalidate_len(databuff, BCM_MAX_PKT_LEN);
#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
    gbpm_free_buf(databuff);
#else
    kfree(databuff);
#endif
}

int bcmenet_delete_runner_ring(uint32_t ring_id)
{
    ENET_RING_S* p_ring;
    uint32_t entry;
    volatile CPU_RX_DESCRIPTOR* p_desc;

    p_ring = &enet_ring[ring_id];
    if (!p_ring->ring_size)
    {
        printk("ERROR:deleting ring_id %d which does not exists!", ring_id);
        return -1;
    }

    /*free the data buffers in ring */
    for (p_desc = (volatile CPU_RX_DESCRIPTOR*) p_ring->base, entry = 0; entry < p_ring->ring_size;
                    p_desc++, entry++)
    {
        if (p_desc->word2)
        {
            p_desc->ownership = OWNERSHIP_HOST;
            _rdp_databuff_free((void *) PHYS_TO_CACHED(p_desc->word2));
            p_desc->word2 = 0;
        }
    }

    /* free any buffers in buff_cache */
    while (p_ring->buff_cache_cnt)
    {
        void * freePtr = (void *) p_ring->buff_cache[--p_ring->buff_cache_cnt];
        if (freePtr)
            _rdp_databuff_free(freePtr);
    }

    /*free buff_cache */
    if (p_ring->buff_cache)
        CACHED_FREE(p_ring->buff_cache);

    /*delete the ring of descriptors*/
    if (p_ring->base)
        rdp_mm_aligned_free((void*) NONCACHE_TO_CACHE(p_ring->base),
                        p_ring->ring_size * sizeof(CPU_RX_DESCRIPTOR));

    p_ring->ring_size = 0;

    return 0;
}

static int bcmenet_create_runner_ring(int ring_id, uint32_t size,uint32_t **ring_base)
{
    ENET_RING_S*    p_ring;
    volatile CPU_RX_DESCRIPTOR*  p_desc;
    uint32_t        entry;
    void*           dataPtr = 0;
    uint32_t        phy_addr;

    if( ring_id >= ARRAY_SIZE(enet_ring) || ring_id < 0)
    {
        printk("ERROR: ring_id %d out of range(%d)\n",ring_id,
                        sizeof(enet_ring)/sizeof(ENET_RING_S));
        return -1;
    }

    p_ring = &enet_ring[ ring_id ];
    if(p_ring->ring_size)
    {
        printk("ERROR: ring_id %d already exists!\n",ring_id);
        return -1;
    }

    /*set ring parameters*/
    p_ring->ring_size     = size;
    p_ring->descriptor_size = sizeof(CPU_RX_DESCRIPTOR);
    p_ring->buff_cache_cnt = 0;


    /*TODO:update the comment  allocate buff_cache which helps to reduce the overhead of when
     * allocating data buffers to ring descriptor */
    p_ring->buff_cache = (uint32_t *)(CACHED_MALLOC(sizeof(uint32_t) * MAX_BUFFERS_IN_RING_CACHE));
    if( p_ring->buff_cache == NULL )
    {
        printk("failed to allocate memory for cache of data buffers \n");
        return -1;
    }

    /*allocate ring descriptors - must be non-cacheable memory*/
    p_ring->base = (CPU_RX_DESCRIPTOR*)rdp_mm_aligned_alloc(sizeof(CPU_RX_DESCRIPTOR) * size, &phy_addr);
    if( p_ring->base == NULL)
    {
        printk("failed to allocate memory for ring descriptor\n");
        bcmenet_delete_runner_ring(ring_id);
        return -1;
    }


    /*initialize descriptors*/
    for (p_desc = p_ring->base, entry = 0 ; entry < size; p_desc++ ,entry++ )
    {
        memset((void*)p_desc,0,sizeof(*p_desc));

        /*allocate actual packet in DDR*/
        dataPtr = _databuf_alloc(p_ring);
        if(!dataPtr)
        {
            printk("failed to allocate packet map entry=%d\n",entry);
            bcmenet_delete_runner_ring(ring_id);
            return -1;
        }
        rdpa_cpu_ring_rest_desc(p_desc,dataPtr);
    }

    /*set the ring header to the first entry*/
    p_ring->head = p_ring->base;

    /*using pointer arithmetics calculate the end of the ring*/
    p_ring->end  = p_ring->base + size;

    *ring_base = (uint32_t*)phy_addr;

    printk("Creating Enet CPU ring for queue number %d with %d packets,Descriptor base=%pK\n ",ring_id,size,p_ring->base);

    return 0;
}

static void dump_data_cb(bdmf_index queue, bdmf_boolean enabled)
{
    if (queue == NETDEV_CPU_HI_RX_QUEUE_ID || queue == NETDEV_CPU_RX_QUEUE_ID)
        global.dump_enable= enabled;
}

static int bcmenet_rxq_queued_get(int qid)
{
    CPU_RX_DESCRIPTOR *p;
    int count;

    //walk over the ring and check the onwership flag.
    for (p = enet_ring[qid].base, count = 0; p != enet_ring[qid].end; p++)
        count += (swap4bytes(p->word2) & 0x80000000) ? 1 : 0;

    return count;
} 

static void bcmenet_rxq_stat_cb(int qid, extern_rxq_stat_t *stat, bdmf_boolean clear)
{
    int queue_idx = qid -  NETDEV_CPU_RX_QUEUE_ID_BASE;

    if (!stat)
        return;

    stat->received = g_rxq_stats_received[queue_idx];
    stat->dropped = g_rxq_stats_dropped[queue_idx];
    stat->queued  = bcmenet_rxq_queued_get(queue_idx);

    if (clear)
        g_rxq_stats_received[queue_idx] = g_rxq_stats_dropped[queue_idx] = 0;
}

static void bcmenet_reason_stat_cb(uint32_t *stat, rdpa_cpu_reason_index_t *rindex)
{
    BCM_ASSERT(stat && rindex);

    *stat = g_rxq_reason_stats[rindex->dir][rindex->reason];
    g_rxq_reason_stats[rindex->dir][rindex->reason] = 0;
}

static int _rdpa_cfg_cpu_rx_queue(int queue_id, uint32_t queue_size,
    rdpa_cpu_rxq_rx_isr_cb_t rx_isr)
{
    rdpa_cpu_rxq_cfg_t rxq_cfg;
    uint32_t *ring_base = NULL;
    int rc;

    /* Read current configuration, set new drop threshold and ISR and write
     * back. */
    bdmf_lock();
    rc = rdpa_cpu_rxq_cfg_get(rdpa_cpu_obj, queue_id, &rxq_cfg);
    if (rc < 0)
        goto unlock_exit;
    rxq_cfg.size = queue_size;
    rxq_cfg.isr_priv = queue_id;
    rxq_cfg.rx_isr = rx_isr;

    rc = bcmenet_create_runner_ring(queue_id - NETDEV_CPU_RX_QUEUE_ID_BASE,queue_size,&ring_base);
    if (rc < 0)
       goto unlock_exit;

    rxq_cfg.ring_head = ring_base;
    /* XXX: Should de-register on uninit */
    rxq_cfg.rx_dump_data_cb = dump_data_cb;
    rxq_cfg.rxq_stat = bcmenet_rxq_stat_cb;
#ifdef ENET_INT_COALESCING_ENABLE
    rxq_cfg.ic_cfg.ic_enable = true;
    rxq_cfg.ic_cfg.ic_timeout_us = ENET_INTERRUPT_COALESCING_TIMEOUT_US;
    rxq_cfg.ic_cfg.ic_max_pktcnt = ENET_INTERRUPT_COALESCING_MAX_PKT_CNT;
#endif
    rc = rdpa_cpu_rxq_cfg_set(rdpa_cpu_obj, queue_id, &rxq_cfg);

unlock_exit:
    bdmf_unlock();
    return rc;
}


/* ---------------------------------------------------
NOTE: runner ring memory is not currently freed!
      This WILL cause a memory leak when the module
      is unloaded!!!  
*/
      
void bcmeapi_del_dev_intr(BcmEnet_devctrl *pDevCtrl)
{
    rdpa_cpu_int_disable(rdpa_cpu_host, NETDEV_CPU_HI_RX_QUEUE_ID);
    rdpa_cpu_int_clear(rdpa_cpu_host, NETDEV_CPU_HI_RX_QUEUE_ID);

    rdpa_cpu_int_disable(rdpa_cpu_host, NETDEV_CPU_RX_QUEUE_ID);
    rdpa_cpu_int_clear(rdpa_cpu_host, NETDEV_CPU_RX_QUEUE_ID);
}

#define SKB_POOL_SIZE 1024
int bcmeapi_init_queue(BcmEnet_devctrl *pDevCtrl)
{
    unsigned char *pSkbuff;
    int i;

    pDevCtrl->default_txq = 0; /* BCM_GMP_MW_UNCLASSIFIED_TRAFFIC_RC; TBD */ 

    if (!pDevCtrl->skbs_p)
    { /* CAUTION!!! DONOT reallocate SKB pool */
        /*
         * Dynamic allocation of skb logic assumes that all the skb-buffers
         * in 'freeSkbList' belong to the same contiguous address range. So if you do any change
         * to the allocation method below, make sure to rework the dynamic allocation of skb
         * logic. look for kmem_cache_create, kmem_cache_alloc and kmem_cache_free functions 
         * in this file 
         */
        if( (pDevCtrl->skbs_p = kmalloc((SKB_POOL_SIZE * BCM_SKB_ALIGNED_SIZE) + 0x10,
            GFP_ATOMIC)) == NULL )
            return -ENOMEM;

        memset(pDevCtrl->skbs_p, 0, (SKB_POOL_SIZE * BCM_SKB_ALIGNED_SIZE) + 0x10);

        pDevCtrl->freeSkbList = NULL;

        /* Chain socket skbs */
        for(i = 0, pSkbuff = (unsigned char *)
            (((unsigned long) pDevCtrl->skbs_p + 0x0f) & ~0x0f);
            i < SKB_POOL_SIZE; i++, pSkbuff += BCM_SKB_ALIGNED_SIZE)
        {
            ((struct sk_buff *) pSkbuff)->next_free = pDevCtrl->freeSkbList;
            pDevCtrl->freeSkbList = (struct sk_buff *) pSkbuff;
        }
        pDevCtrl->end_skbs_p = pDevCtrl->skbs_p + (SKB_POOL_SIZE * BCM_SKB_ALIGNED_SIZE) + 0x10;
    }
    return 0;
}

void bcmeapi_get_chip_idrev(unsigned int *chipid, unsigned int *chiprev)
{
}

static int _rdpa_cfg_cpu_reason_to_queue_init(void)
{
    int rc;
    rdpa_cpu_reason_index_t reason_cfg_idx = {BDMF_INDEX_UNASSIGNED, BDMF_INDEX_UNASSIGNED};
    rdpa_cpu_reason_cfg_t reason_cfg = {};

    while (!rdpa_cpu_reason_cfg_get_next(rdpa_cpu_obj, &reason_cfg_idx))
    {
        if (reason2queue_cfg_skip(&reason_cfg_idx, NULL))
            continue;

        if (reason_cfg_idx.reason == rdpa_cpu_rx_reason_etype_pppoe_d ||
            reason_cfg_idx.reason == rdpa_cpu_rx_reason_etype_pppoe_s ||
            reason_cfg_idx.reason == rdpa_cpu_rx_reason_etype_arp ||
            reason_cfg_idx.reason == rdpa_cpu_rx_reason_etype_801_1ag_cfm ||
            reason_cfg_idx.reason == rdpa_cpu_rx_reason_l4_icmp ||
            reason_cfg_idx.reason == rdpa_cpu_rx_reason_icmpv6 ||
            reason_cfg_idx.reason == rdpa_cpu_rx_reason_igmp ||
            reason_cfg_idx.reason == rdpa_cpu_rx_reason_dhcp ||
            reason_cfg_idx.reason == rdpa_cpu_rx_reason_l4_udef_0)
        {
            //            if (reason_cfg_idx.dir == rdpa_dir_ds)
            reason_cfg.queue = NETDEV_CPU_HI_RX_QUEUE_ID;
            //            else
            //               reason_cfg.queue = NETDEV_CPU_RX_QUEUE_ID;
        }
        else
        {
            reason_cfg.queue = NETDEV_CPU_RX_QUEUE_ID;
        }

#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
        reason_cfg_idx.table_index = (reason_cfg_idx.dir == rdpa_dir_us)
            ? CPU_REASON_LAN_TABLE_INDEX : CPU_REASON_WAN1_TABLE_INDEX;
#endif
        reason_cfg.meter = BDMF_INDEX_UNASSIGNED;
        rc = rdpa_cpu_reason_cfg_set(rdpa_cpu_obj, &reason_cfg_idx, &reason_cfg);
        if (rc < 0)
        {
            printk(KERN_ERR CARDNAME ": Error (%d) configuraing CPU reason to queue \n", rc );
            return rc;
        }
    }
    return 0;
}

int bcmeapi_open_dev(BcmEnet_devctrl *pDevCtrl, struct net_device *dev)
{
    int rc;

    rc = _rdpa_cfg_cpu_rx_queue(NETDEV_CPU_HI_RX_QUEUE_ID, NETDEV_CPU_RX_QUEUE_SIZE, bcmeapi_enet_isr);
    if (rc < 0)
    {
        printk(KERN_ERR CARDNAME ": Cannot configure CPU Rx queue (%d)\n", rc);
        return -EINVAL;
    }

    rdpa_cpu_int_clear(rdpa_cpu_host, NETDEV_CPU_HI_RX_QUEUE_ID);
    rdpa_cpu_int_enable(rdpa_cpu_host, NETDEV_CPU_HI_RX_QUEUE_ID);

    rc = _rdpa_cfg_cpu_rx_queue(NETDEV_CPU_RX_QUEUE_ID, NETDEV_CPU_RX_QUEUE_SIZE, bcmeapi_enet_isr);
    if (rc < 0)
    {
        printk(KERN_ERR CARDNAME ": Cannot configure CPU Rx queue (%d)\n", rc);
        return -EINVAL;
    }

    rc = rdpa_cpu_reason_stat_external_cb_set(rdpa_cpu_obj, bcmenet_reason_stat_cb);
    if (rc < 0)
        printk(KERN_ERR CARDNAME ": Cannot configure CPU external reason statistics callback (%d)\n", rc);

    rdpa_cpu_int_clear(rdpa_cpu_host, NETDEV_CPU_RX_QUEUE_ID);
    rdpa_cpu_int_enable(rdpa_cpu_host, NETDEV_CPU_RX_QUEUE_ID);

    rc = _rdpa_cfg_cpu_reason_to_queue_init();
    if (rc < 0)
        return -EINVAL;
    if (use_cpu_meter)
        _rdpa_cpu_set_reasons_to_meter(cpu_meter_idx[rdpa_dir_ds], cpu_meter_idx[rdpa_dir_us]);

    /*    rc = rdpa_cpu_rxq_flush_set(rdpa_cpu_obj, NETDEV_CPU_RX_QUEUE_ID, TRUE);
          if (rc < 0)
          printk(KERN_ERR CARDNAME ": Failed to flush cpu queue %d, rc %d\n", NETDEV_CPU_RX_QUEUE_ID, rc);*/

    return 0;
}

void bcmeapi_reset_mib_cnt(uint32_t sw_port)
{
    bdmf_object_handle port_obj = NULL;
    rdpa_port_stat_t stat;
    int rc = 0;
    
    memset(&stat, 0, sizeof(stat));

    if ((rdpa_emac0 + sw_port) >= rdpa_emac__num_of)
        printk("bcmeapi_reset_mib_cnt: invalid lan port id %d \n", sw_port);

    bdmf_lock();
    
    rc = rdpa_port_get(rdpa_if_lan0 + sw_port, &port_obj);
    if (rc)
        goto unlock_exit;
    
    /* Reset RDPA statistic */
    rdpa_port_stat_set(port_obj, &stat);
    
unlock_exit:
    if (port_obj)
        bdmf_put(port_obj);    
    bdmf_unlock();
}

int bcmeapi_ioctl_kernel_poll(struct ethswctl_data *e)
{
    static int mdk_init_done = 0;

    /* MDK will calls this function for the first time after it completes initialization */
    if (!mdk_init_done) 
    {
        mdk_init_done = 1;
#if !defined(STAR_FIGHTER2)  /* FIXME - we shouldn't use chip specific compilation */
        /* Disable HW switching by default for Runner based platforms */
        ethsw_set_hw_switching(HW_SWITCHING_DISABLED);
#endif
#if defined(CONFIG_BCM_ETH_HWAPD_PWRSAVE)
        ethsw_setup_hw_apd(1);
#endif
        ethsw_eee_init();
    }
    ethsw_eee_process_delayed_enable_requests();

    return 0;
}

#if defined(STAR_FIGHTER2)
static inline void link_update_exception_handling(BcmEnet_devctrl *priv, 
    unsigned long *p_port_map,
    unsigned long *p_force_lnk_dn_port)
{
    /* FIXME - this logic should be separated out along with WAN-capable port(s) logic we plan to do for WebGUI/CMS */
    if ( (priv->linkState & (1<<(PHYSICAL_PORT_TO_LOGICAL_PORT(0,0))))) /* ETHWAN Link is UP */
    {   /* Check for transition i.e. Port was UP and configured as WAN --> Still up but not configured as WAN anymore */
        if (!(priv->wanPort & (1 << PHYSICAL_PORT_TO_LOGICAL_PORT(0,0)))) /* Port is not configured as ETHWAN */
        {
            *p_force_lnk_dn_port |= (1<<0);
        }
    }
    else  /* ETHWAN link is DOWN */
    {
        if (!(priv->wanPort & (1 << PHYSICAL_PORT_TO_LOGICAL_PORT(0,0)))) /* Port is not configured as ETHWAN */
        {
            *p_port_map &= (~(1<<0)); /* Remove EMAC_0 port - it is not configured as ETHWAN */
        }
    }
}

#if defined(AEI_VDSL_MUTIL_WAN_ETHERNET_PHY)
void bcmeapi_ioctl_ethsw_crossbar_config(struct net_device *dev)
{
#define LOGICAL_WAN_RUNNER_PORT  0 /* This is fixed for 63138/63148 platform */
    int vport;
    int physical_port = LOGICAL_PORT_TO_PHYSICAL_PORT(LOGICAL_WAN_RUNNER_PORT);
    int sw_unit = LOGICAL_PORT_TO_UNIT_NUMBER(LOGICAL_WAN_RUNNER_PORT);
    BcmEnet_devctrl *vdev_priv = (BcmEnet_devctrl *)netdev_priv(dev);
    BcmEnet_devctrl *bcmsw_priv = (BcmEnet_devctrl *)netdev_priv(vnet_dev[0]);
    ETHERNET_MAC_INFO *info = &bcmsw_priv->EnetInfo[sw_unit];
    int muxIntPort = swPort2MuxInt[sw_unit][physical_port];
    int muxExtPort = vdev_priv->cb_ext_port;
    PHY_STAT phys;

    for (vport = 1; vport <= vport_cnt && (vnet_dev[vport] != dev); vport++);
    if (vport > vport_cnt)
       return;

    /* Internal switch i.e. Runner port */
    if (LOGICAL_PORT_TO_UNIT_NUMBER(vdev_priv->sw_port_id) != 0) // The sw_port_id is 0 while the dev is internal switch.
       return;

    if (muxInt2Ext[muxIntPort] != muxExtPort)
    {
       crossbar_select(muxExtPort, muxIntPort);

       muxInt2Ext[muxIntPort] = muxExtPort;

       info->sw.phy_id[physical_port] = info->sw.crossbar[muxExtPort].phy_id;
       info->sw.port_flags[physical_port] = info->sw.crossbar[muxExtPort].port_flags;
    }

    /* 
     * If crossbar ports change we need to swap 'vport' corresponding 
     * to Runner WAN 'logical port' also
     */
    logicalport_to_vport[LOGICAL_WAN_RUNNER_PORT] = vport;

    phys = ethsw_phyid_stat(info->sw.crossbar[muxExtPort].phy_id);
    ethsw_set_mac(physical_port, phys);
#if defined(SUPPORT_GPL_1)
    crossbar_update_wan_link_status(sw_unit, physical_port, muxInt2Ext[muxIntPort], phys.lnk);
#endif
    printk("Runner WAN port 0 connected to %s via crossbar port %d\n", dev->name, muxExtPort);
}
#else
static PHY_STAT enet_connect_crossPort(int logPort, int *new_cb_port)
{
    int phsclPort = LOGICAL_PORT_TO_PHYSICAL_PORT(logPort);
    int sw_unit = LOGICAL_PORT_TO_UNIT_NUMBER(logPort);
    ETHERNET_MAC_INFO *info = EnetGetEthernetMacInfo();
    int muxIntPort = swPort2MuxInt[sw_unit][phsclPort];
    int muxExtPort;
    int phyId;
    PHY_STAT phys, oldPhys;
    *new_cb_port = BP_CROSSBAR_NOT_DEFINED; /* Initialize */

    for(muxExtPort = 0; muxExtPort < BCMENET_CROSSBAR_MAX_EXT_PORTS; muxExtPort++)
    {
        /* Skip no member crossbar ports */
        if (info[sw_unit].sw.crossbar[muxExtPort].switch_port != phsclPort) continue;

        phyId = enet_cb_port_to_phyid(sw_unit, muxExtPort);
        phys = ethsw_phy_stat(sw_unit, phsclPort, muxExtPort);

        /* Save the existing PHY status for no change */
        if (muxInt2Ext[muxIntPort] == muxExtPort)
        {
            oldPhys = phys;
            *new_cb_port = muxExtPort; /* Return old/current value */
        }

        if(phys.lnk)
        {
            crossbar_select(muxExtPort, muxIntPort);
            muxInt2Ext[muxIntPort] = muxExtPort;
            info[sw_unit].sw.phy_id[phsclPort] = phyId;
            *new_cb_port = muxExtPort; /* Return new value */
            return phys;
        }

    }
    return oldPhys;
}
#endif
#endif

int enet_get_current_cb_port(int logPort)
{
#if defined(STAR_FIGHTER2)
    int phsclPort = LOGICAL_PORT_TO_PHYSICAL_PORT(logPort);
    int sw_unit = LOGICAL_PORT_TO_UNIT_NUMBER(logPort);
    int muxIntPort = swPort2MuxInt[sw_unit][phsclPort];
    if (muxIntPort != BP_CROSSBAR_NOT_DEFINED)
    {
        return muxInt2Ext[muxIntPort];
    }
#endif
    return BP_CROSSBAR_NOT_DEFINED;
}

#if defined(AEI_VDSL_MUTIL_WAN_ETHERNET_PHY)
/*
 * This function is very specific to 963138/963148 BHR platform.
 * It does the needed crossbar port configuration to enable/disable
 * the WAN MoCA and WAN Ethernet interfaces connected to the 
 * Runner WAN Port 0 via the Crossbar Switch.
 */
void bcmeapi_update_link_status(void)
{
#define RUNNER_UNIT_NUM 0
    int vport, runner_wan_port = -1;
    unsigned long force_lnk_dn_port = 0;
    unsigned int wan_sw_port_link_state = 0;
    struct net_device *bcwmsw_dev = vnet_dev[0];
    BcmEnet_devctrl *bcmsw_priv = (BcmEnet_devctrl *)netdev_priv(bcwmsw_dev);
    ETHERNET_MAC_INFO *emac_info = &bcmsw_priv->EnetInfo[0]; /* Unit is 0 since we use it only for Runner */
    unsigned long port_map = emac_info->sw.port_map;

#if defined(CONFIG_BCM_EXT_SWITCH)
    port_map &= (~(1<<extSwInfo.connected_to_internalPort)); /* Remove internal port connected to external switch */
#endif

#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
    link_update_exception_handling(bcmsw_priv, &port_map, &force_lnk_dn_port);
#endif

    if (!port_map) 
        return;

    for (vport = 1; vport <= vport_cnt; vport++)
    {
        int logical_port, physical_port, phy_id;
        unsigned int soft_link;
        struct net_device *vdev = vnet_dev[vport];
        BcmEnet_devctrl *vdev_priv = (BcmEnet_devctrl *)netdev_priv(vdev);
        PHY_STAT phys;

        logical_port = VPORT_TO_LOGICAL_PORT(vport);

        /* No WAN ports have been configured */
        if (!port_map)
            continue;

        /* Internal switch i.e. Runner port */
        if (LOGICAL_PORT_TO_UNIT_NUMBER(logical_port) != 0) 
            continue;

        physical_port = LOGICAL_PORT_TO_PHYSICAL_PORT(logical_port); 
        if (vdev_priv->cb_ext_port == BP_CROSSBAR_NOT_DEFINED)
        {
            phy_id = enet_sw_port_to_phyid(0, physical_port);
            soft_link =  (bcmsw_priv->linkState & (1<<logical_port)) > 0;
        }
        else
        {
            phy_id = emac_info->sw.crossbar[vdev_priv->cb_ext_port].phy_id;
            soft_link = vdev_priv->linkState > 0;
        }

        if (!IsPhyConnected(phy_id))
            continue;

        if (force_lnk_dn_port & (1 << physical_port))
            memset(&phys, 0, sizeof(phys)); /* Force link down */
        else
            phys = ethsw_phyid_stat(phy_id);

        if (soft_link != phys.lnk) /* Did link state change */
        {
            int muxIntPort = swPort2MuxInt[RUNNER_UNIT_NUM][physical_port];
            int muxExtPort = vdev_priv->cb_ext_port;

            if (muxInt2Ext[muxIntPort] == muxExtPort)
	    {
               ethsw_set_mac(physical_port, phys);

#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
               crossbar_update_wan_link_status(RUNNER_UNIT_NUM, physical_port, muxInt2Ext[muxIntPort], phys.lnk);
#endif
	    }

            link_change_handler(logical_port, vdev_priv->cb_ext_port, 
                    phys.lnk, phys.spd1000?1000 : (phys.spd100?100 : 10), phys.fdx);

            if (vdev_priv->cb_ext_port != BP_CROSSBAR_NOT_DEFINED)
            {
                runner_wan_port = logical_port;

                if (phys.lnk)
                    wan_sw_port_link_state |= (1<<vdev_priv->cb_ext_port);
                else
                    wan_sw_port_link_state &= ~(1<<vdev_priv->cb_ext_port);
            }
        }                   
    }

    /* Update Runner WAN port link status based on the crossbar port link status */
    if (runner_wan_port >= 0)
    {
       if (wan_sw_port_link_state > 0)
          bcmsw_priv->linkState |= (0x1 << runner_wan_port);
       else
          bcmsw_priv->linkState &= ~(0x1 << runner_wan_port);
    }
}
#else

/* Ideally this function should be common for both Runner and DMA based chips
 * Link polling through Ethernet driver is not commonly used for DMA platforms
 * and performed by SWMDK. This logic should be clubbed with link_poll function 
 * later. */ 
void bcmeapi_update_link_status(void)
{
    struct net_device *dev = vnet_dev[0];
    BcmEnet_devctrl *priv = (BcmEnet_devctrl *)netdev_priv(dev);
    unsigned long port_map = priv->EnetInfo[0].sw.port_map; /* Note : Internal switch/Runner Port Map; Used only for optimization */
    int phy_id, softLink, logPort, cb_port = BP_CROSSBAR_NOT_DEFINED;
    unsigned long force_lnk_dn_port = 0;

#if defined(CONFIG_BCM_EXT_SWITCH)
    port_map &= (~(1<<extSwInfo.connected_to_internalPort)); /* Remove internal port connected to external switch */
#endif

#if defined(STAR_FIGHTER2)
    link_update_exception_handling(priv, &port_map, &force_lnk_dn_port);
#endif

    if (port_map) 
    {
        int vport, physical_port, unit;
        PHY_STAT phys;

        for (vport = vport_cnt; port_map && vport >= 1; vport--) /* Starting backward - internal switch port are towards end */
        {
            logPort = VPORT_TO_LOGICAL_PORT(vport);
            softLink =  (priv->linkState & (1<<logPort)) > 0;
            unit = LOGICAL_PORT_TO_UNIT_NUMBER(logPort);
            if (unit == 0) /* Internal switch i.e. Runner port */
            {
                physical_port = LOGICAL_PORT_TO_PHYSICAL_PORT(logPort); /* Get physical port number on Runner */
                phy_id = enet_sw_port_to_phyid(0, physical_port);

                port_map &= (~(1<<physical_port)); /* Remove the port from port_map */
                cb_port = enet_get_current_cb_port(logPort);
#if defined(ENET_GPON_CONFIG)
                /* Skip GPON interface */
                if(physical_port == GPON_PORT_ID) {
                    continue;
                }
#endif
#if defined(ENET_EPON_CONFIG)
                /* Skip EPON interface */
                if(physical_port == EPON_PORT_ID) {
                    continue;
                }
#endif                
                if (force_lnk_dn_port & (1<<physical_port))
                {
                    /* Force link down */
                    memset(&phys,0,sizeof(phys));
                }
                else
                {
                    /* Get the status of Phy connected to physical port */
#if defined(STAR_FIGHTER2)
                    if(softLink == 0)
                    {
                        phys = enet_connect_crossPort(logPort, &cb_port);
                    }
                    else
#endif
                        phys = ethsw_phy_stat(unit, physical_port, cb_port);
                }

                if (softLink != phys.lnk) /* Did link state change */
                {
                    ethsw_set_mac(logPort, phys);
#if defined(STAR_FIGHTER2)
                    crossbar_update_wan_link_status(0/*unit*/,physical_port, cb_port, phys.lnk);
                    if (phys.lnk == 0)
                    {
                        serdes_work_around(phy_id);    
                    }
#endif
                    link_change_handler(PHYSICAL_PORT_TO_LOGICAL_PORT(physical_port,0), cb_port, 
                        phys.lnk, phys.spd1000?1000 : (phys.spd100?100 : 10), phys.fdx);

                    /* link up to down, put Crossbar back to Serdes for hardware work around */
                }
            }
        }
    }
}
#endif

/* we assume that if RX stime per test period > speficied value, that we decrease the rate. Taking in account that
   period is about 1 sec (which is 1000 jiffies), we don't allow value > 1000. Initially we set it to default of 800,
   and allow to user to reconfigure it.
   If jiffies is changed to something else than 1000, this code needs to be enhanced! */
static unsigned long rx_stime_ps_thresh = 800;

void bcmeapi_enet_poll_timer(void)
{
    static cputime_t last_rx_stime = 0;
    unsigned long delta_stime = 0;
    static int first_time = 1; 
    BcmEnet_devctrl *pDevCtrl = netdev_priv(vnet_dev[0]);

#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
    rdpa_cpu_tx_reclaim();
#endif

    if (!use_cpu_meter)
        return;

    if (first_time)
    {
        last_rx_stime = pDevCtrl->rx_thread->stime;
        first_time = 0;
        return;
    }

    delta_stime = cputime_to_jiffies(pDevCtrl->rx_thread->stime - last_rx_stime);
    last_rx_stime = pDevCtrl->rx_thread->stime;

    if (delta_stime < rx_stime_ps_thresh)
        _rdpa_cfg_cpu_meter(1);
    else 
        _rdpa_cfg_cpu_meter(0);
}

static int proc_get_rx_stime_thresh(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
    int r;

    *eof = 1;
    r = sprintf(page, "%lu\n", rx_stime_ps_thresh);
    return r < cnt ? r : 0;
}

static int proc_set_rx_stime_thresh(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    char input[32] = {};

    if (copy_from_user(input, buf, cnt) != 0)
        return -EFAULT;
    rx_stime_ps_thresh = strtoul(input, NULL, 10);
    if (rx_stime_ps_thresh > 1000)
    {
        printk("Bad value for RX stime threshold\n");
        return -EFAULT; 
    }
    return cnt;
}

void bcmeapi_add_proc_files(struct net_device *dev, BcmEnet_devctrl *pDevCtrl)
{
    struct proc_dir_entry *p;

    if (!use_cpu_meter)
        return;

    p = create_proc_entry("rx_stime_ps_thresh", 0644, NULL);

    if (p == NULL)
        return;

    p->data = dev;
    p->read_proc = proc_get_rx_stime_thresh;
    p->write_proc = proc_set_rx_stime_thresh;
}

void bcmeapi_free_queue(BcmEnet_devctrl *pDevCtrl)
{
    if (!use_cpu_meter)
        return;

    remove_proc_entry("rx_stime_ps_thresh", NULL);
}

#ifdef BRCM_FTTDP
int bcmeapi_fttdp_init_cfg(int physical_port)
{
    BDMF_MATTR(rdpa_lag_port_attrs, rdpa_port_drv());
    bdmf_object_handle switch_port_obj = NULL;
    bdmf_object_handle lag_port_obj = NULL;
    rdpa_port_dp_cfg_t lag_port_cfg = {};
    bdmf_error_t rc;

    rc = rdpa_port_get(rdpa_if_switch, &switch_port_obj);
    if (rc)
    {
        printk("%s %s missing switch port! rc(%d)\n", __FILE__, __FUNCTION__, rc);
        goto Exit;
    }

    printk("Create rdpa lag%d port \n", physical_port);
    rdpa_port_index_set(rdpa_lag_port_attrs, rdpa_if_lag0 + physical_port);
    lag_port_cfg.emac = (rdpa_emac)physical_port;
    rdpa_port_cfg_set(rdpa_lag_port_attrs, &lag_port_cfg);
    rc = bdmf_new_and_set(rdpa_port_drv(), NULL, rdpa_lag_port_attrs, &lag_port_obj);
    if (rc)
    {
        printk("%s %s Failed to create rdpa lag%d port rc(%d)\n", __FILE__, __FUNCTION__, physical_port, rc);
        goto Exit;
    }

    /*FIXME: add uinimac configuration here */


    bdmf_link(switch_port_obj, lag_port_obj, NULL);

Exit:
    if (switch_port_obj)
        bdmf_put(switch_port_obj);
    if (rc && lag_port_obj)
        bdmf_put(lag_port_obj);
    return rc;
}
#endif

