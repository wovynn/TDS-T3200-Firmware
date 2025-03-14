/*
* <:copyright-BRCM:2013-2015:GPL/GPL:standard
* 
*    Copyright (c) 2013-2015 Broadcom Corporation
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
#ifndef _BDMF_SYSTEM_H_
#define _BDMF_SYSTEM_H_


#include "bdmf_system_common.h"
#include <linux/nbuff.h>
#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
#include <linux/gbpm.h>
#endif
#ifdef CONFIG_MIPS
#include <asm/r4kcache.h>
#endif
#include "rdp_drv_bpm.h"


/** Invalidate dcache range
 * \param[in]   addr    start address
 * \param[in]   size    size
 */
static inline void bdmf_dcache_inv(unsigned long addr, unsigned long size)
{
    //blast_inv_dcache_range(addr, addr+size);
    cache_invalidate_len((void*)addr, size);
}

/** Flush dcache range
 * \param[in]   addr    start address
 * \param[in]   size    size
 */
static inline void bdmf_dcache_flush(unsigned long addr, unsigned long size)
{
    //blast_dcache_range(addr, addr+size);
    cache_flush_len((void*)addr, size);
}

static inline bdmf_sysb_type bdmf_sysb_typeof(bdmf_sysb sysb)
{
    if ( IS_FKBUFF_PTR(sysb) )
    {
        return bdmf_sysb_fkb;
    }
    else
    {
        return bdmf_sysb_skb;
    }
}

/** Set headroom size for system buffer
 * \param[in]   sysb_type   System buffer type
 * \param[in]   headroom    Headroom size
 */
void bdmf_sysb_headroom_size_set(bdmf_sysb_type sysb_type, uint32_t headroom);

/** convert sysb to skb or fkb
 * \param[in]   sysb        System buffer
 * \return skb or fkb
 */
static inline void *bdmf_sysb_2_fkb_or_skb( bdmf_sysb sysb )
{
    return PNBUFF_2_PBUF(sysb);
}

extern struct sk_buff * skb_header_alloc(void);

/** Allocate system buffer header i.e skb or fkb structure
 *  and initilize it with the provided len & data buffer
 * \param[in]   sysb_type   System buffer type
 * \param[in]   len         Data length
 * \return system buffer pointer.
 * If the function returns NULL, caller is responsible for "data" deallocation
 */
static inline bdmf_sysb bdmf_sysb_header_alloc(bdmf_sysb_type sysb_type, void* datap, uint32_t len, uint32_t context)
{
    if( sysb_type == bdmf_sysb_fkb )
    {
        FkBuff_t *fkb;

        fkb = fkb_init(datap, BCM_PKT_HEADROOM, datap, len);

        /*set the recyle hook */
        fkb->recycle_hook = bdmf_sysb_recycle;
        fkb->recycle_context = context;

        return (bdmf_sysb)FKBUFF_2_PNBUFF(fkb);
    }
    else if (sysb_type == bdmf_sysb_skb)
    {
        struct sk_buff *skb;
        /* allocate skb structure*/
        skb = skb_header_alloc();
        if(!skb)
        {
            return NULL;
        }

        /* initialize the skb */
        /*skb_headerinit(BCM_PKT_HEADROOM, len, skb, datap, bdmf_sysb_recycle,
                   context, NULL);*/

    skb_headerinit(BCM_PKT_HEADROOM,
#if defined(ENET_CACHE_SMARTFLUSH)
            SKB_DATA_ALIGN(len+BCM_SKB_TAILROOM),
#else
            BCM_MAX_PKT_LEN,
#endif
            skb, datap, bdmf_sysb_recycle,
            context, NULL);

        skb_trim(skb, len);
        skb->recycle_flags &= SKB_NO_RECYCLE;/* no skb recycle,just do data recyle */

        return (bdmf_sysb)SKBUFF_2_PNBUFF(skb);
    }
    printk("%s: sysbtype=%d not supported\n", __FUNCTION__, sysb_type);
    return NULL;
}

/** Allocate data buffers.
 * \param[out]  bufp        Array to hold allocated data buffers
 * \param[in]   num_buffs   number of buffers to allocate
 * \param[in]   context     currently unused
 * \returns     number of buffers allocated.
 */
static inline uint32_t bdmf_sysb_databuf_alloc(uint32_t *bufp, uint32_t num_buffs, uint32_t context)
{
#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
    int i;
    if(gbpm_alloc_mult_buf(num_buffs, bufp) == GBPM_ERROR)
    {
        /* BPM returns either all the buffers requested or none */
        return 0;
    }

    /* no cache invalidation of buffers is needed for buffers coming from BPM */

    /*reserve space for headroom & FKB */
    for(i=0; i < num_buffs; i++ )
    {
        bufp[i]= (uint32_t)PFKBUFF_TO_PDATA((void *)(bufp[i]),BCM_PKT_HEADROOM);
    }

    return num_buffs;
#else
    uint32_t *datap;
       /* allocate from kernel directly */
    datap = kmalloc(BCM_PKTBUF_SIZE, GFP_ATOMIC);

   if(!datap)
   {
        return 0;
   }
    /* do a cache invalidate of the buffer */
    bdmf_dcache_inv((unsigned long)datap, BCM_PKTBUF_SIZE );

    /*reserve space for headroom & FKB */
    bufp[0] = (uint32_t)PFKBUFF_TO_PDATA((void *)(datap),BCM_PKT_HEADROOM);

    /* always return only one buffer when BPM is not enabled */
    return 1;
#endif
}


/** Recycle system buffer.
 * \param[in]   sysb        System buffer
 * \param[in]   context     unused
 */
static inline void __bdmf_sysb_databuf_recycle(void *datap, uint32_t context)
{
#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
    gbpm_free_buf(datap);
#else
    kfree(datap);
#endif
}

/** Free the datap poniter actual pointer allocated(before headroom) and
 then recyle
 * \param[in]   sysb        System buffer
 * \param[in]   context     unused
 */
static inline void bdmf_sysb_databuf_free(void *datap, uint32_t context)
{
     /*do cache invalidate */
      bdmf_dcache_inv((unsigned long)datap, BCM_MAX_PKT_LEN);
    __bdmf_sysb_databuf_recycle((void *)PDATA_TO_PFKBUFF(datap, BCM_PKT_HEADROOM), context);
}

/** Release system buffer.
 * \param[in]   sysb        System buffer
 */
static inline void bdmf_sysb_free(bdmf_sysb sysb)
{
        nbuff_free(sysb);
}

/** Release pbuf
 * \param[in]   pbuf        Platform buffer
 * \return 0=OK, <0-error (sysb doesn't contain pbuf info)
 */
static inline void bdmf_pbuf_free(bdmf_pbuf_t *pbuf)
{
    if (pbuf->bpm_bn == INVALID_BPM_BUFFER)
        bdmf_sysb_free((bdmf_sysb)pbuf->data);
    else
        fi_bl_drv_bpm_free_buffer(pbuf->source, pbuf->bpm_bn);
}

/** Get sysb data pointer
 * \param[in]   sysb        System buffer
 * \return data pointer
 */
static inline void *bdmf_sysb_data(const bdmf_sysb sysb)
{
    if(IS_FKBUFF_PTR(sysb))
    {
        return (PNBUFF_2_FKBUFF(sysb))->data;
    }
    else
    {
        return (PNBUFF_2_SKBUFF(sysb))->data;
    }
}

/** Flush sysb data
 * \param[in]   sysb        System buffer
 * \return data pointer
 */
static inline void bdmf_sysb_data_flush(const bdmf_sysb sysb, void *data, uint32_t len)
{
    nbuff_flush(sysb, data, len);
}


/** Get sysb data length
 * \param[in]   sysb        System buffer
 * \return data length
 */
static inline uint32_t bdmf_sysb_length(const bdmf_sysb sysb)
{
    if(IS_FKBUFF_PTR(sysb))
    {
        return (PNBUFF_2_FKBUFF(sysb))->len;
    }
    else
    {
        return (PNBUFF_2_SKBUFF(sysb))->len;
    }
}

/** Get sysb linear data length
 * \param[in]   sysb        System buffer
 * \return data length
 */
static inline uint32_t bdmf_sysb_data_length(const bdmf_sysb sysb)
{
    if(IS_FKBUFF_PTR(sysb))
    {
        return (PNBUFF_2_FKBUFF(sysb))->len;
    }
    else
    {
        /*length of skb->data only, does not include data in skb->frags or fraglist*/
        return skb_headlen(PNBUFF_2_SKBUFF(sysb));
    }
}


#if defined(CONFIG_BCM_PKTRUNNER_GSO)

#define RUNNER_MAX_GSO_DESC 512

/*
*** RUNNER_MAX_GSO_FRAGS should match RDD_GSO_DESC_ENTRY_FRAG_DATA_NUMBER
*/
#define RUNNER_MAX_GSO_FRAGS 18


extern void* bdmf_sysb_data_to_gsodesc(const bdmf_sysb sysb, uint32_t *is_gso_pkt_p);
extern int bdmf_gso_desc_pool_create( uint32_t num_desc);
extern void bdmf_gso_desc_pool_destroy(void);


#ifdef _BYTE_ORDER_LITTLE_ENDIAN_
typedef struct {
    union{
        uint32_t word0;
        uint32_t data;
    };
    union{
        uint32_t word1;
        struct{
            uint16_t len;
            uint16_t linear_len; /*TODO change to total_len */
        };
    };

    union{
        uint32_t word2;
        struct{
            uint16_t mss;
            struct {
                uint8_t isAllocated:1;
                uint8_t reserved0:7;
            };
            uint8_t nr_frags;
        };
    };

    union{
        uint32_t word3;
        uint32_t reserved1;
    };

    uint32_t frag_data[RUNNER_MAX_GSO_FRAGS];
    uint16_t frag_len[RUNNER_MAX_GSO_FRAGS];
    uint32_t reserved2;
}runner_gso_desc_t;

#else
typedef struct {
    union{
        uint32_t word0;
        uint32_t data;
    };

    union{
        uint32_t word1;
        struct{
            uint16_t linear_len;
            uint16_t len; /*TODO change to total_len */
        };
    };

    union{
        uint32_t word2;
        struct{
            uint8_t nr_frags;
            struct {
                uint8_t isAllocated:1;
                uint8_t reserved0:7;
            };
            uint16_t mss;
        };
    };

    union{
        uint32_t word3;
        uint32_t reserved1;
    };
    uint32_t frag_data[RUNNER_MAX_GSO_FRAGS];
    uint16_t frag_len[RUNNER_MAX_GSO_FRAGS];
    uint32_t reserved2;
}runner_gso_desc_t;

#endif

/** Checks if a packet needs GSO processing and convert skb to GSO Descriptor
 * \param[in]   sysb  system buffer
 * \param[out]  is_gso_pkt_p indicates to caller if sysb is a GSO packet
 * \returns for Non-GSO: sysb->data GSO: GSO Desciptor or NULL
 */
static inline void *bdmf_sysb_gso_data(const bdmf_sysb sysb, uint32_t *is_gso_pkt_p)
{
    if(IS_FKBUFF_PTR(sysb))
    {
        *is_gso_pkt_p = 0;
        return (PNBUFF_2_FKBUFF(sysb))->data;
    }
    else
    {
        return bdmf_sysb_data_to_gsodesc(sysb, is_gso_pkt_p);
    }
}

#endif

#if defined(CONFIG_BCM_SPDSVC_SUPPORT)
/** Perform any pending GSO and checksum computations on the given System Buffer
 * \param[in]   sysb  system buffer
 * \returns BDMF_ERR_OK on success. On error, the system buffer is freed.
 */
static inline int bdmf_sysb_complete_packet(bdmf_sysb sysb)
{
    if (IS_FKBUFF_PTR(sysb))
    {
        bdmf_sysb_free(sysb);

        return BDMF_ERR_NOT_SUPPORTED;
    }
    else
    {
        if (skb_bypass_hw_features(PNBUFF_2_SKBUFF(sysb)))
        {
            return BDMF_ERR_INTERNAL;
        }

        if ((PNBUFF_2_SKBUFF(sysb))->next)
        {
            /* Multiple SKBs generated */

            bdmf_sysb_free(sysb);

            return BDMF_ERR_NOT_SUPPORTED;
        }
    }

    return BDMF_ERR_OK;
}
#endif

/*we put the common include at last line of this file*/


#endif
