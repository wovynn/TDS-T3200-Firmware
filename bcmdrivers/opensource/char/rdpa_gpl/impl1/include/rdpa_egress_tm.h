/*
* <:copyright-BRCM:2013:DUAL/GPL:standard
* 
*    Copyright (c) 2013 Broadcom Corporation
*    All Rights Reserved
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License, version 2, as published by
* the Free Software Foundation (the "GPL").
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* 
* A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
* writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
* Boston, MA 02111-1307, USA.
* 
* :> 
*/


#ifndef _RDPA_EGRESS_TM_H_
#define _RDPA_EGRESS_TM_H_

/** \defgroup egress_tm Egress Traffic Manager
 * \ingroup tm
 * The RDPA egress traffic manager controls a number of egress-TM elements, whereas
 * egress TM element is either a transmit queue or a next-level egress-TM. Then total 
 * number of supported egress-TM elements and transmit queues is platform dependent retrieved via system object. 
 * The RDPA egress traffic manager has the following capabilites:
 *  - Egress TM element can be configured to support either rate limiter or scheduler or both.
 *  - Up to 3-level hierarchical egress-TM.
 *  - The transmit queues support different drop policies ::rdpa_tm_drop_alg.
 * All egress_tm are created with rdpa_tm_drop_alg_dt meaning no special drop algorithm
 * @{
 */
/* Max ds service queue shapers */
#define RDPA_MAX_SERVICE_QUEUE        8
#define RDPA_MAX_DS_TM_QUEUE        128
#define RDPA_TM_MAX_US_SCHED 128 
#define RDPA_TM_MAX_DS_SCHED (RDPA_MAX_DS_TM_QUEUE + RDPA_MAX_SERVICE_QUEUE)
#define RDPA_TM_MAX_SCHED_ELEMENTS 32 /**< Max number of subsidiary elements in egress-TM group */
#define RDPA_WEIGHT_UNASSIGNED 0
#define RDPA_MAX_EGRESS_QUEUES 8    /**< Max number of egress queues per egress-TM element */
#define RDPA_MAX_WEIGHT 63
#define RDPA_MIN_WEIGHT 1
#define RDPA_MAX_WRED_PROFILE_PER_DIRECTION 8  /**< Max number of wred profilers */
#define RDPA_WRED_MAX_DROP_PROBABILITY     100
#define RDPA_ETH_TX_PRIORITY_QUEUE_THRESHOLD  1024

#include "rdpa_egress_tm_basic.h"

/** Egress-TM next level type */
typedef enum {
    rdpa_tm_level_queue,      /**< Next level type is queue */
    rdpa_tm_level_egress_tm   /**< Next level type is Egress-TM */
} rdpa_tm_level_type;

/** Egress-TM rate limiter mode */
typedef enum {
    rdpa_tm_rl_single_rate,     /**< Single rate (default mode) */
    rdpa_tm_rl_dual_rate        /**< AF and BE, avialible only for SP scheduling (rdpa_tm_sched_sp) */
} rdpa_tm_rl_rate_mode;

/** Scheduler operating modes */
typedef enum {
    rdpa_tm_sched_disabled,     /**< Scheduling disabled */
    rdpa_tm_sched_sp,           /**< SP scheduler */
    rdpa_tm_sched_wrr,          /**< WRR scheduler */

    rdpa_tm_sched__num_of, 
} rdpa_tm_sched_mode;

/** Egress-TM Rate limiter configuration.\n
 */
typedef struct {
    uint32_t af_rate;                   /**< AF (assured forwarding) rate */
    uint32_t be_rate;                   /**< BE (best effort) rate, available only for rdpa_tm_rl_dual_rate */
    uint32_t burst_size;                /**< Burst size, available for LLID or upstream pir bucket  */
} rdpa_tm_rl_cfg_t;

/** Drop policy algorithm type
*/
typedef enum {
    rdpa_tm_drop_alg_dt,                /**< Drop tail */
    rdpa_tm_drop_alg_red,               /**< RED (random early detection)*/
    rdpa_tm_drop_alg_wred,              /**< WRED (weighted RED) */
    rdpa_tm_drop_alg_reserved,          /**< queues with this profile work as flow control */
    rdpa_tm_drop_alg__num_of            /* Number of drop algorithms */
} rdpa_tm_drop_alg;

/** Egress queue parameters configuration.\n
 */
typedef struct {
    uint32_t queue_id;           /**< queue_id. Assigned by management application */
    uint32_t drop_threshold;     /**< Drop threshold (queue size) */
    uint32_t weight;             /**< Weight in WFQ/WRR/DWRR Egress-TM group */
    rdpa_tm_drop_alg drop_alg;   /**< Drop algorithm */
    uint32_t red_high_threshold; /**< Relevant for ::rdpa_tm_drop_alg_red/::rdpa_tm_drop_alg_wred :high red packet threshold */
    uint32_t red_low_threshold;  /**< Relevant for ::rdpa_tm_drop_alg_red/::rdpa_tm_drop_alg_wred :low red packet threshold */
} rdpa_tm_queue_cfg_t;

/** Queue index for flush[] and queue_stat[] attributes
 */
typedef struct {
    bdmf_index channel;         /**< Channel selector. -1=all channels */
    uint32_t queue_id;          /**< Queue id */
} rdpa_tm_queue_index_t;

/** Service queue
 */
typedef struct {
    bdmf_boolean enable; /**< Egress_tm is of type service queue */
} rdpa_tm_service_queue_t;

#ifdef BDMF_DRIVER
extern bdmf_attr_enum_table_t orl_prty_enum_table;
#endif

/*call back pointer to register epon stack gobal rate limter funtion */
typedef unsigned char (*epon_global_shaper_cb_t)(uint32_t);
extern epon_global_shaper_cb_t global_shaper_cb;

/*call back pointer to register epon stack link rate limter funtion */
typedef unsigned char (*epon_link_shaper_cb_t)(uint8_t, uint32_t, uint16_t);
extern epon_link_shaper_cb_t epon_link_shaper_cb;

/** @} end of sched Doxygen group */

#endif /* _RDPA_EGRESS_TM_H_ */
