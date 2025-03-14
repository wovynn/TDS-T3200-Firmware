/*
 Copyright 2002-2010 Broadcom Corp. All Rights Reserved.

 <:label-BRCM:2011:DUAL/GPL:standard    
 
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
#ifndef _BCMENET_RUNNER_H_
#define _BCMENET_RUNNER_H_

#include "bcmenet.h"
#include "rdpa_mw_cpu_queue_ids.h"
#include<linux/skbuff.h>

#define ENET_CACHE_SMARTFLUSH

#define NULL_FUNC_NULL_STMT 
#define NULL_FUNC_RET_VOID {}
#define NULL_FUNC_RET_INT (0)

struct BcmEnet_devctrl {
    BcmEnetDevctrlBaseClass_s;

    int             phy_addr;           /* PHY address */
    struct sk_buff *freeSkbList;
    unsigned char *skbs_p;
    unsigned char *end_skbs_p;
    int            rdpa_port;
    int physical_inx;
#if defined(AEI_VDSL_TOOLBOX)
    UINT16 usMirrorInFlags;
    UINT16 usMirrorOutFlags;
#endif
};

struct enet_xmit_params {
    enet_xmit_params_base;
    int channel;
    FkBuff_t *pFkb;
    struct sk_buff *skb;
};

#if defined(_CONFIG_BCM_ENDPOINT)
#define NETDEV_WEIGHT  16 // lower weight for less voice latency
#else
#define NETDEV_WEIGHT  32
#endif

#define ENET_INT_COALESCING_ENABLE 

#define ENET_INTERRUPT_COALESCING_TIMEOUT_US 500
#define ENET_INTERRUPT_COALESCING_MAX_PKT_CNT 32

#include "rdp_cpu_ring_defs.h"
#define MAX_BUFFERS_IN_RING_CACHE   32
typedef struct
{
    uint32_t ring_size;
    uint32_t descriptor_size;
    CPU_RX_DESCRIPTOR* head;
    CPU_RX_DESCRIPTOR* base;
    CPU_RX_DESCRIPTOR* end;
    uint32_t buff_cache_cnt;
    uint32_t* buff_cache;
}ENET_RING_S;



#define bcmeapi_EthGetStats(log_port, rxDropped, txDropped) NULL_FUNC_RET_VOID
#define bcmeapi_init_dev(dev) NULL_FUNC_RET_INT
#define bcmeapi_EthSetPhyRate(port, enable, bps, isWanPort) NULL_FUNC_RET_VOID
#define bcmeapi_set_mac_speed(port, speed) NULL_FUNC_RET_VOID
#define bcmeapiPhyIntEnable(enable) NULL_FUNC_RET_VOID
#define bcmeapi_aelink_handler(linkstatus) NULL_FUNC_RET_VOID
#define bcmeapi_select_tx_def_queue(param) NULL_FUNC_RET_VOID
#define bcmeapi_select_tx_nodef_queue(param) NULL_FUNC_RET_VOID
#define bcmeapi_map_interrupt(pDevCtrl) 0
#define bcmeapi_link_might_changed() 0
#define bcmeapi_que_select() BCMEAPI_CTRL_TRUE 
#define bcmbcmeapiPhyIntEnable(enable) NULL_FUNC_RET_VOID
#define bcmeapi_buf_reclaim(pParam) NULL_FUNC_RET_VOID
#define bcmeapi_config_tx_queue(pParam) NULL_FUNC_RET_VOID
#define bcmeapi_napi_leave(pDevCtrl) NULL_FUNC_RET_VOID
#define bcmeapi_xmit_unlock_exit_post(pXmitParams) NULL_FUNC_RET_VOID
#define bcmeapi_xmit_unlock_drop_exit_post(pXmitParams) NULL_FUNC_RET_VOID
#define bcmeapi_update_rx_queue(pDevCtrl) NULL_FUNC_RET_VOID
#define bcmeapi_prepare_rx() NULL_FUNC_NULL_STMT
#define bcmeapi_prepare_next_rx(rxpktgood) BCMEAPI_CTRL_CONTINUE
#define bcmeapi_rx_post(rxpktgood) NULL_FUNC_RET_VOID
#define bcmeapi_free_irq(pDevCtrl)  NULL_FUNC_RET_VOID
#define bcmeapi_config_queue(e)  NULL_FUNC_RET_VOID
#define bcmeapi_repare_next_queue() NULL_FUNC_RET_VOID
#define bcmeapi_ioctl_ethsw_rxscheduling(e) NULL_FUNC_RET_INT
#define bcmeapi_ioctl_ethsw_wrrparam(e) NULL_FUNC_RET_INT
#define bcmeapi_ioctl_rx_pkt_rate_config(e) NULL_FUNC_RET_INT
#define bcmeapi_ioctl_rx_pkt_rate_limit_config(e) NULL_FUNC_RET_INT
#define bcmeapi_ioctl_test_config(e) NULL_FUNC_RET_INT
#define bcmeapi_dump_queue(e, pDevCtrl) NULL_FUNC_RET_VOID
#define bcmeapi_get_num_txques(ethctl) NULL_FUNC_RET_INT
#define bcmeapi_set_num_txques(ethctl) NULL_FUNC_RET_INT
#define bcmeapi_get_num_rxques(ethctl) NULL_FUNC_RET_INT
#define bcmeapi_set_num_rxques(ethctl) NULL_FUNC_RET_INT
int bcmeapi_ethsw_init(void);
#define bcmeapi_ethsw_init_ports()

#ifndef CARDNAME
#define CARDNAME    "BCM_RUNNER_ENET"
#endif

/* Need to keep this number of port #defines same as robo_reg.h
 * Unless we can make the LOGICAL/PHYSICAL macros bcmenet.h independent
 * of the assumption that internal and external switch have 8 ports */
#define MAX_SWITCH_PORTS    8
#if defined(CONFIG_BCM_EXT_SWITCH)
#define MAX_TOTAL_SWITCH_PORTS (MAX_SWITCH_PORTS+MAX_SWITCH_PORTS)
#else
#define MAX_TOTAL_SWITCH_PORTS (MAX_SWITCH_PORTS)
#endif
#define NETDEV_CPU_RX_QUEUE_SIZE 512 /* Leave 32 packets for ploams/mpcp and 64 for omci/oam */

int bcmeapi_ioctl_link_change(BcmEnet_devctrl *priv, struct ethswctl_data *e);
int bcmeapi_module_init(void);
void bcmeapi_add_dev_queue(struct net_device *dev);
int bcmeapi_init_queue(BcmEnet_devctrl *pDevCtrl);
void bcmeapi_del_dev_intr(BcmEnet_devctrl *pDevCtrl);
int bcmeapi_open_dev(BcmEnet_devctrl *pDevCtrl, struct net_device *dev);
void bcmeapi_get_chip_idrev(unsigned int *chipid, unsigned int *chiprev);
int bcmeapi_queue_select(EnetXmitParams *pParam);
void bcmeapi_buf_alloc(BcmEnet_devctrl *pDevCtrl);
void bcmeapi_get_tx_queue(EnetXmitParams *pParam);
int bcmeapi_ioctl_kernel_poll(struct ethswctl_data *e);
void bcmeapi_update_link_status(void);
void bcmeapi_enet_poll_timer(void);
void bcmeapi_add_proc_files(struct net_device *dev, BcmEnet_devctrl *pDevCtrl);
void bcmeapi_free_queue(BcmEnet_devctrl *pDevCtrl);
int enet_get_current_cb_port(int logPort);


#ifdef BRCM_FTTDP
extern int g9991_bp_debug_port;
#define G9991_DEBUG_RDPA_PORT rdpa_if_lan29
int bcmeapi_fttdp_init_cfg(int phisical_port);
#endif
int bcmeapi_should_create_vport(int logical_port);

#if defined(AEI_VDSL_MUTIL_WAN_ETHERNET_PHY)
void bcmeapi_ioctl_ethsw_crossbar_config(struct net_device *dev);
#endif

#endif /* _BCMENET_RUNNER_H_ */
