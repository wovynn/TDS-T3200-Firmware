
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
 * File Name  : rdpa_cmd_pbit_to_q.c
 *
 * Description: This file contains the RDPA PBIT TO Q API.
 *
 *******************************************************************************
 */

#include <linux/module.h>
#include <linux/bcm_log.h>
#include "bcmtypes.h"
#include "rdpa_types.h"
#include "rdpa_api.h"
#include "rdpa_drv.h"
#include "rdpa_cmd_pbit_to_q.h"

#define __BDMF_LOG__

#define CMD_P_TO_Q_LOG_ID_RDPA_CMD_DRV BCM_LOG_ID_RDPA_CMD_DRV

#if defined(__BDMF_LOG__)
#define CMD_P_TO_Q_LOG_ERROR(fmt, args...) 										\
    do {                                                            				\
        if (bdmf_global_trace_level >= bdmf_trace_level_error)      				\
            bdmf_trace("ERR: %s#%d: " fmt "\n", __FUNCTION__, __LINE__, ## args);	\
    } while(0)
#define CMD_P_TO_Q_LOG_INFO(fmt, args...) 										\
    do {                                                            				\
        if (bdmf_global_trace_level >= bdmf_trace_level_info)      					\
            bdmf_trace("INF: %s#%d: " fmt "\n", __FUNCTION__, __LINE__, ## args);	\
    } while(0)
#define CMD_P_TO_Q_LOG_DEBUG(fmt, args...) 										\
    do {                                                            				\
        if (bdmf_global_trace_level >= bdmf_trace_level_debug)      					\
            bdmf_trace("DBG: %s#%d: " fmt "\n", __FUNCTION__, __LINE__, ## args);	\
    } while(0)
#else
#define CMD_P_TO_Q_LOG_ERROR(fmt, arg...) BCM_LOG_ERROR(fmt, arg...)
#define CMD_P_TO_Q_LOG_INFO(fmt, arg...) BCM_LOG_INFO(fmt, arg...)
#define CMD_P_TO_Q_LOG_DEBUG(fmt, arg...) BCM_LOG_DEBUG(fmt, arg...)
#endif


/*******************************************************************************/
/* static routines Functions                                                   */
/*******************************************************************************/

/*******************************************************************************
 *
 * Function: dev_link_pbit_to_q_check
 *
 *   check if dev is linked to pbit to queue 
 *
 ******************************************************************************/
static BOOL dev_link_pbit_to_q_check(bdmf_object_handle dev_obj, 
                               bdmf_object_handle p_to_q_obj)
{
    bdmf_link_handle obj_link = NULL;
    
    obj_link = bdmf_get_next_us_link(p_to_q_obj, NULL);
    if (obj_link)
    {
        do {
            if (bdmf_us_link_to_object(obj_link) == dev_obj)
            {
                return TRUE;
            }
            obj_link = bdmf_get_next_us_link(p_to_q_obj, obj_link);
        } while(obj_link);
    }

    /* compare DS links obj*/
    obj_link = bdmf_get_next_ds_link(p_to_q_obj, NULL);
    if (obj_link)
    {
        do {
            if (bdmf_us_link_to_object(obj_link) == dev_obj)
            {
                return true;
            }
            obj_link = bdmf_get_next_ds_link(p_to_q_obj, obj_link);
        } while(obj_link);
    }

    return FALSE;
}


static int pbit_to_q_link_dev_num_get(bdmf_object_handle p_to_q_obj)
{
    int link_num = 0;
    bdmf_link_handle obj_link = NULL;
    
    obj_link = bdmf_get_next_us_link(p_to_q_obj, NULL);
    if (obj_link)
    {
        do {
            link_num++;
            obj_link = bdmf_get_next_us_link(p_to_q_obj, obj_link);
        } while(obj_link);
    }

    obj_link = bdmf_get_next_ds_link(p_to_q_obj, NULL);
    if (obj_link)
    {
        do {
            link_num++;
            obj_link = bdmf_get_next_ds_link(p_to_q_obj, obj_link);
        } while(obj_link);
    }

    return link_num;
}



/*******************************************************************************/
/* global routines                                                             */
/*******************************************************************************/


/*******************************************************************************
 *
 * Function: rdpa_cmd_pbit_to_q_ioctl
 *
 * IOCTL interface to the RDPA PBIT TO Q API.
 *
 *******************************************************************************/
int rdpa_cmd_pbit_to_q_ioctl(unsigned long arg)
{
    rdpa_drv_ioctl_pbit_to_q_t *userPtoQ_p = (rdpa_drv_ioctl_pbit_to_q_t *)arg;
    rdpa_drv_ioctl_pbit_to_q_t pbit_to_q;
    int ret = 0;
    int rc = BDMF_ERR_OK;
    bdmf_object_handle p_to_q_obj = NULL;
    bdmf_object_handle dev_obj = NULL;
    
    copy_from_user(&pbit_to_q, userPtoQ_p, sizeof(rdpa_drv_ioctl_pbit_to_q_t));

    CMD_P_TO_Q_LOG_DEBUG("RDPA PBIT TO Q CMD(%d)", pbit_to_q.cmd);

    bdmf_lock();

    switch (pbit_to_q.dev_type)
    {
        case RDPA_IOCTL_DEV_PORT:
            rc = rdpa_port_get(pbit_to_q.dev_id, &dev_obj);
            if (0 != rc)
            {
                CMD_P_TO_Q_LOG_DEBUG("rdpa_port_get() failed:" \
                    " port(%u) rc(%d)", pbit_to_q.dev_id, rc);
                ret = RDPA_DRV_PORT_GET;
                goto p_to_q_ioctl_exit;
            }
            break;
            
        case RDPA_IOCTL_DEV_TCONT:
            rc = rdpa_tcont_get(pbit_to_q.dev_id, &dev_obj);
            if (0 != rc)
            {
                CMD_P_TO_Q_LOG_DEBUG("rdpa_tcont_get() failed:" \
                    " tcont(%u) rc(%d)", pbit_to_q.dev_id, rc);
                ret = RDPA_DRV_TCONT_GET;
                goto p_to_q_ioctl_exit;
            }
            break;
            
        case RDPA_IOCTL_DEV_LLID:
            rc = rdpa_llid_get(pbit_to_q.dev_id, &dev_obj);
            if (0 != rc)
            {
                CMD_P_TO_Q_LOG_DEBUG("rdpa_llid_get() failed:" \
                    " llid(%u) rc(%d)", pbit_to_q.dev_id, rc);
                ret = RDPA_DRV_LLID_GET;
                goto p_to_q_ioctl_exit;
            }
            break;
            
        default:
            CMD_P_TO_Q_LOG_DEBUG(
                "Invalid IOCTL dev type %d", pbit_to_q.dev_type);
            ret = RDPA_DRV_ERROR;
            goto p_to_q_ioctl_exit;
    }

    switch (pbit_to_q.cmd)
    {
        case RDPA_IOCTL_P_TO_Q_CMD_GET:
        {
            int i;
            int tbl_idx;
            bdmf_number pbit_qid;
 
            CMD_P_TO_Q_LOG_DEBUG("RDPA_IOCTL_P_TO_Q_CMD_GET dev=%d if=%d", \
                pbit_to_q.dev_type, pbit_to_q.dev_id);

            pbit_to_q.found = FALSE;
            
            for (tbl_idx = 0; tbl_idx < RDPA_PBIT_TO_PRTY_MAX_TABLES; tbl_idx++)
            {
                p_to_q_obj = NULL;
                rc = rdpa_pbit_to_queue_get(tbl_idx, &p_to_q_obj);
                if (0 != rc)
                    continue;

                if (dev_link_pbit_to_q_check(dev_obj, p_to_q_obj))
                {
                    pbit_to_q.found = TRUE;
                    break;
                }
                else
                {
                    bdmf_put(p_to_q_obj);
                }
            }

            if (!pbit_to_q.found)
                goto p_to_q_ioctl_exit;
           
            for (i = 0; i < 8; i++)
            {
                rc = rdpa_pbit_to_queue_pbit_map_get(p_to_q_obj, i, &pbit_qid);
                if (0 != rc)
                {
                    CMD_P_TO_Q_LOG_DEBUG(
                        "rdpa_pbit_to_queue_pbit_map_get() failed:" \
                        " table(%u) pbit(%u) rc(%d)", tbl_idx, i, rc);
                    ret = RDPA_DRV_P_TO_Q_QOS_MAP_GET;
                    break;
                }
                pbit_to_q.pbit_q_map[i] = pbit_qid;
            }
            
            bdmf_put(p_to_q_obj);
            break;
        }

        case RDPA_IOCTL_P_TO_Q_CMD_SET:
        {
            int i;
            int tbl_idx;
            BOOL has_same_cfg = FALSE;
            BOOL dev_has_table = FALSE;
            bdmf_number pbit_qid;

            /* dev's current linked table obj */
            bdmf_object_handle cur_p_to_q_obj = NULL;
            
            BDMF_MATTR(pbit_to_q_attrs, rdpa_pbit_to_queue_drv());

            CMD_P_TO_Q_LOG_DEBUG(
                            "RDPA_IOCTL_P_TO_Q_CMD_SET");

            /* check if the dev itself has table linked already*/
            for (tbl_idx = 0; tbl_idx < RDPA_PBIT_TO_PRTY_MAX_TABLES; tbl_idx++)
            {
                cur_p_to_q_obj = NULL;
                rc = rdpa_pbit_to_queue_get(tbl_idx, &cur_p_to_q_obj);
                if (0 != rc)
                    continue;

                if (dev_link_pbit_to_q_check(dev_obj, cur_p_to_q_obj))
                {
                    dev_has_table = TRUE;
                    break;
                }
                bdmf_put(cur_p_to_q_obj);
            }
            
            /* check if there's existing table the matches the new cfg */    
            for (tbl_idx = 0; tbl_idx < RDPA_PBIT_TO_PRTY_MAX_TABLES; tbl_idx++)
            {
                p_to_q_obj = NULL;
                rc = rdpa_pbit_to_queue_get(tbl_idx, &p_to_q_obj);
                if (0 != rc)
                    continue;

                for (i = 0; i < 8; i++)
                {
                    rc = rdpa_pbit_to_queue_pbit_map_get(p_to_q_obj, i, &pbit_qid);
                    if (0 != rc)
                    {
                        CMD_P_TO_Q_LOG_DEBUG(
                            "rdpa_pbit_to_queue_pbit_map_get() failed:" \
                            " table(%u) pbit(%u) rc(%d)", tbl_idx, i, rc);
                        ret = RDPA_DRV_P_TO_Q_QOS_MAP_GET;
                        bdmf_put(p_to_q_obj);
                        goto p_to_q_ioctl_exit;
                    }

                    if (pbit_qid != pbit_to_q.pbit_q_map[i])
                        break;
                }

                if (i == 8)
                {
                    has_same_cfg = TRUE;
                    break;
                }
                
                bdmf_put(p_to_q_obj);
            }

            /* start the process 
                       when there's existing table that matches the new config.
                       Just link this table to the dev. */
            if (has_same_cfg)
            {
                /* check if this table has linked to the dev already.
                              if yes, do nothing.
                              if no, link it to the dev. */
                if (!dev_link_pbit_to_q_check(dev_obj, p_to_q_obj))
                {
                    /* before link the new table, 
                                    unlink the current table of the dev first. */
                    if (dev_has_table)
                        bdmf_unlink(cur_p_to_q_obj, dev_obj);

                    /* link the table */
                    bdmf_link(p_to_q_obj, dev_obj, NULL);
                }

                if (dev_has_table)
                    bdmf_put(cur_p_to_q_obj);
                bdmf_put(p_to_q_obj);

                goto p_to_q_ioctl_exit;
            }

            /* start the process 
                       when there's no existing table that matches the new config */
            if (dev_has_table && 
                (1 == pbit_to_q_link_dev_num_get(cur_p_to_q_obj)))
            {
                /* if the dev has table already and the table is only used by the dev,
                              we just use this table to do the new cfg later. */
                p_to_q_obj = cur_p_to_q_obj;
            }
            else
            {
                /* we create a new table to do the new cfg. */

                /* if the dev has table already, but the table is used by other dev also,
                              we unlink the current table from the dev here. */
                if (dev_has_table)
                {
                    bdmf_unlink(cur_p_to_q_obj, dev_obj);
                    bdmf_put(cur_p_to_q_obj);
                }

                /* create a new table */
                p_to_q_obj = NULL;
                rc = bdmf_new_and_set(
                    rdpa_pbit_to_queue_drv(), NULL, pbit_to_q_attrs, &p_to_q_obj);
                if (rc || (p_to_q_obj == NULL))
                {
                    CMD_P_TO_Q_LOG_ERROR(
                        "bdmf_new_and_set() failed: pbit_to_q rc(%d)", rc);
                    ret = RDPA_DRV_NEW_D_TO_P_ALLOC;
                    goto p_to_q_ioctl_exit;
                }
                
                bdmf_link(p_to_q_obj, dev_obj, NULL);
            }

            /* modify the table with the new cfg*/
            for (i = 0; i < 8; i++)
            {
                rc = rdpa_pbit_to_queue_pbit_map_set(
                    p_to_q_obj, i, pbit_to_q.pbit_q_map[i]);
                if (0 != rc)
                {
                    CMD_P_TO_Q_LOG_DEBUG(
                        "rdpa_pbit_to_queue_pbit_map_set() failed:" \
                        "pbit(%u) qid(%u) rc(%d)", \
                        i, pbit_to_q.pbit_q_map[i], rc);
                    ret = RDPA_DRV_P_TO_Q_QOS_MAP_SET;
                    bdmf_put(p_to_q_obj);
                    goto p_to_q_ioctl_exit;
                }
            }

            bdmf_put(p_to_q_obj);
            
            break;
        }

        default:
            CMD_P_TO_Q_LOG_ERROR("Invalid IOCTL cmd %d", pbit_to_q.cmd);
            rc = RDPA_DRV_ERROR;
    }

    if (rc) {
        CMD_P_TO_Q_LOG_ERROR(
            "rdpa_cmd_pbit_to_q_ioctl() OUT: FAILED: rc(%d)", rc);
    }

p_to_q_ioctl_exit:

    copy_to_user(
        userPtoQ_p, &pbit_to_q, sizeof(rdpa_drv_ioctl_pbit_to_q_t));
    
    if (dev_obj)
        bdmf_put(dev_obj);
    
    if (ret) 
    {
        CMD_P_TO_Q_LOG_ERROR(
            "rdpa_cmd_pbit_to_q_ioctl() OUT: FAILED: cmd(%u) ret(%d)", \
            pbit_to_q.cmd, ret);
    }

    bdmf_unlock();
    return ret;
}

/*******************************************************************************
 *
 * Function: rdpa_cmd_pbit_to_q_init
 *
 * Initializes the RDPA PBIT TO Q API.
 *
 *******************************************************************************/
void rdpa_cmd_pbit_to_q_init(void)
{
    CMD_P_TO_Q_LOG_DEBUG("RDPA PBIT TO Q INIT");
}

EXPORT_SYMBOL(rdpa_cmd_pbit_to_q_ioctl);
EXPORT_SYMBOL(rdpa_cmd_pbit_to_q_init);

