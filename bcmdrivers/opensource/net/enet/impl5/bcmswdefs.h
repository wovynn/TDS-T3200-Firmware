/*
<:copyright-BRCM:2004:DUAL/GPL:standard

   Copyright (c) 2004 Broadcom Corporation
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

#ifndef _BCMSWDEFS_H_
#define _BCMSWDEFS_H_

#define NUM_ARL_ENTRIES 2048
#define MAX_VLANS 4096

#define DMA_CFG ((volatile uint32 * const) SWITCH_DMA_CONFIG)
#define DMA_STATE ((volatile uint32 * const) SWITCH_DMA_STATE)
/* Advertise 100BaseTxFD/HD and 10BaseTFD/HD */
#define AN_ADV_ALL 0x1E1
/* Advertise 1000BaseTFD/HD */
#define AN_1000BASET_CTRL_ADV_ALL 0x300

/* For USB loopback, enable rx and tx of swpktbus and set the rx_id different
   from tx_id */
#define USB_SWPKTBUS_LOOPBACK_VAL 0x70031
#define LINKDOWN_OVERRIDE_VAL 0x4B

#if defined(BCM_SWITCH_SCHED_WRR)
#define DEFAULT_HQ_PREEMPT_EN    0 
#else //if defined(BCM_SWITCH_SCHED_SP)
#define DEFAULT_HQ_PREEMPT_EN    1
#endif

/* 6829 Queue Thresholds */
#define BCM6829_PRIQ_HYST        0x220
#define BCM6829_PRIQ_PAUSE       0x2E0
#define BCM6829_PRIQ_DROP        0x2F0
#define BCM6829_PRIQ_LOWDROP     0x40
#define BCM6829_TOTAL_HYST       0x230
#define BCM6829_TOTAL_PAUSE      0x2F0
#define BCM6829_TOTAL_DROP       0x300  

/*
 * NOTE : These default buffer thresholds are duplicated in SWMDK as well. Check files bcm6xxx_a0_bmd_init.c 
*/
#if !defined(SUPPORT_SWMDK)
#if defined(CONFIG_BCM963268)
/* These FC thresholds are based on 0x200 buffers available in the switch */
#define DEFAULT_TOTAL_DROP_THRESHOLD           0x1FF
#define DEFAULT_TOTAL_PAUSE_THRESHOLD          0x1FF
#define DEFAULT_TOTAL_HYSTERESIS_THRESHOLD     0x1F0 
#define DEFAULT_TXQHI_DROP_THRESHOLD           0x78
#define DEFAULT_TXQHI_PAUSE_THRESHOLD          0x70
#define DEFAULT_TXQHI_HYSTERESIS_THRESHOLD     0x68

#else /* 6328 and 6318 */

/* These FC thresholds are based on 0x100 buffers available in the switch */
#define DEFAULT_TOTAL_DROP_THRESHOLD           0xFF
#define DEFAULT_TOTAL_PAUSE_THRESHOLD          0xD0
#define DEFAULT_TOTAL_HYSTERESIS_THRESHOLD     0xA0
#define DEFAULT_TXQHI_DROP_THRESHOLD           0x3D
#define DEFAULT_TXQHI_PAUSE_THRESHOLD          0x2D
#define DEFAULT_TXQHI_HYSTERESIS_THRESHOLD     0x1D
#endif
#endif /* !SUPPORT_SWMDK */

#endif /* _BCMSWDEFS_H_ */
