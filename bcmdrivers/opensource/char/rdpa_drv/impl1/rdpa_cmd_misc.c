
/*
* <:copyright-BRCM:2015:DUAL/GPL:standard
* 
*    Copyright (c) 2015 Broadcom Corporation
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

/*
 *******************************************************************************
 * File Name  : rdpa_cmd_misc.c
 *
 * Description: This file contains the RDPA MISCELLANEOUS API.
 *
 *******************************************************************************
 */

#include <linux/module.h>
#include <linux/bcm_log.h>
#include "bcmtypes.h"
#include "rdpa_types.h"
#include "rdpa_api.h"
#include "rdpa_drv.h"
#include "rdpa_cmd_misc.h"
#include "rdpa_mw_qos.h"


#define __BDMF_LOG__

#define CMD_MISC_LOG_ID_RDPA_CMD_DRV BCM_LOG_ID_RDPA_CMD_DRV

#if defined(__BDMF_LOG__)
#define CMD_MISC_LOG_ERROR(fmt, args...) 										\
    do {                                                            				\
        if (bdmf_global_trace_level >= bdmf_trace_level_error)      				\
            bdmf_trace("ERR: %s#%d: " fmt "\n", __FUNCTION__, __LINE__, ## args);	\
    } while(0)
#define CMD_MISC_LOG_INFO(fmt, args...) 										\
    do {                                                            				\
        if (bdmf_global_trace_level >= bdmf_trace_level_info)      					\
            bdmf_trace("INF: %s#%d: " fmt "\n", __FUNCTION__, __LINE__, ## args);	\
    } while(0)
#define CMD_MISC_LOG_DEBUG(fmt, args...) 										\
    do {                                                            				\
        if (bdmf_global_trace_level >= bdmf_trace_level_debug)      					\
            bdmf_trace("DBG: %s#%d: " fmt "\n", __FUNCTION__, __LINE__, ## args);	\
    } while(0)
#else
#define CMD_MISC_LOG_ERROR(fmt, arg...) BCM_LOG_ERROR(fmt, arg...)
#define CMD_MISC_LOG_INFO(fmt, arg...) BCM_LOG_INFO(fmt, arg...)
#define CMD_MISC_LOG_DEBUG(fmt, arg...) BCM_LOG_DEBUG(fmt, arg...)
#endif


/*******************************************************************************/
/* global routines                                                             */
/*******************************************************************************/


/*******************************************************************************
 *
 * Function: rdpa_cmd_misc_ioctl
 *
 * IOCTL interface to the RDPA MISCELLANEOUS API.
 *
 *******************************************************************************/
int rdpa_cmd_misc_ioctl(unsigned long arg)
{
    rdpa_drv_ioctl_misc_t *userMisc_p = (rdpa_drv_ioctl_misc_t *)arg;
    rdpa_drv_ioctl_misc_t misc_cfg;
    int ret = 0;
    int rc = BDMF_ERR_OK;
    
    copy_from_user(&misc_cfg, userMisc_p, sizeof(rdpa_drv_ioctl_misc_t));

    CMD_MISC_LOG_DEBUG("RDPA MISC CMD(%d)", misc_cfg.cmd);

    bdmf_lock();

    switch(misc_cfg.cmd)
    {
        case RDPA_IOCTL_MISC_CMD_PKT_BASED_QOS_GET:
        {
            BOOL enable;
            
            CMD_MISC_LOG_DEBUG(
                "RDPA_IOCTL_MISC_CMD_PKT_BASED_QOS_GET dir=%d type=%d", \
                misc_cfg.dir, misc_cfg.type);

            rc = rdpa_mw_pkt_based_qos_get(
                misc_cfg.dir, misc_cfg.type, &enable);

            if (rc != 0)
            {
                CMD_MISC_LOG_ERROR("rdpa_mw_pkt_based_qos_get() failed:" \
                    " dir(%u) type(%d) rc(%d)", \
                    misc_cfg.dir, misc_cfg.type, rc);
                ret = RDPA_DRV_PKT_BASED_QOS_GET;
            }
            else
            {
                misc_cfg.enable = enable;
                copy_to_user(
                    userMisc_p, &misc_cfg, sizeof(rdpa_drv_ioctl_misc_t));
            }

            break;
        }
        
        case RDPA_IOCTL_MISC_CMD_PKT_BASED_QOS_SET:
        {
            CMD_MISC_LOG_DEBUG(
                "RDPA_IOCTL_MISC_CMD_PKT_BASED_QOS_SET dir=%d type=%d en=%d", \
                misc_cfg.dir, misc_cfg.type, misc_cfg.enable);
            
            rc = rdpa_mw_pkt_based_qos_set(
                misc_cfg.dir, misc_cfg.type, &misc_cfg.enable);
            
            if (rc != 0)
            {
                CMD_MISC_LOG_ERROR("rdpa_mw_pkt_based_qos_set() failed:" \
                    " dir(%u) type(%d) en(%d) rc(%d)", \
                    misc_cfg.dir, misc_cfg.type, misc_cfg.enable, rc);
                ret = RDPA_DRV_PKT_BASED_QOS_SET;
            }
            
            break;
        }
        
        default:
            CMD_MISC_LOG_ERROR("Invalid IOCTL cmd %d", misc_cfg.cmd);
            rc = RDPA_DRV_ERROR;
    }
    
    if (ret) 
    {
        CMD_MISC_LOG_ERROR(
            "rdpa_cmd_misc_ioctl() OUT: FAILED: cmd(%u) ret(%d)", \
            misc_cfg.cmd, ret);
    }

    bdmf_unlock();
    return ret;
}

/*******************************************************************************
 *
 * Function: rdpa_cmd_misc_init
 *
 * Initializes the RDPA MISCELLANEOUS API.
 *
 *******************************************************************************/
void rdpa_cmd_misc_init(void)
{
    CMD_MISC_LOG_DEBUG("RDPA MISC INIT");
}

EXPORT_SYMBOL(rdpa_cmd_misc_ioctl);
EXPORT_SYMBOL(rdpa_cmd_misc_init);

