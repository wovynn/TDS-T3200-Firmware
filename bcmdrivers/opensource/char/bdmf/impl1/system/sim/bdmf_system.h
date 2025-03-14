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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define BDMFSYS_STDIO
#include <assert.h>
#include <stdint.h>
#include <ctype.h>
#include <stddef.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h> /* to get htonl, ntohl */
#include <arpa/inet.h>  /* to get inet_aton and friends */
#include <pthread.h>
#include <semaphore.h>

#include <bdmf_errno.h>
#include <bdmf_queue.h>
#include <bdmf_buf.h>

#define BUG()           assert(0)
#define BUG_ON(_f)      assert(!(_f))

#ifndef __PACKING_ATTRIBUTE_STRUCT_END__
#define __PACKING_ATTRIBUTE_STRUCT_END__        __attribute__ ((packed))
#endif
#ifndef __PACKING_ATTRIBUTE_FIELD_LEVEL__
#define __PACKING_ATTRIBUTE_FIELD_LEVEL__
#endif

extern uint32_t jiffies;

/* Allocate/release memory */
static inline void *bdmf_alloc(size_t size)
{
    return malloc(size);
}

static inline void *bdmf_calloc(size_t size)
{
    void *p = bdmf_alloc(size);
    if (p)
        memset(p, 0, size);
    return p;
}

static inline void bdmf_free(void *p)
{
    free(p);
}

static inline void *bdmf_alloc_uncached(int size, uint32_t *phys_addr_p)
{
    *phys_addr_p = (uint32_t)(bdmf_alloc(size));

    return (void *)(*phys_addr_p);
}

#define bdmf_free_uncached(_virt_p, _phys_addr, _size)   bdmf_free(_virt_p)

/* Input/Output */
#define bdmf_print(format,args...)             printf(format, ## args)
#define bdmf_vprint(format,ap)                 vprintf(format, ap)
#define bdmf_print_error(format,args...)       bdmf_print("***Error in %s:%d>"format, __FUNCTION__, __LINE__, ## args)

/* Binary mutex
 * Task that attempts to take mutex that is already taken blocks,
 * regardless on who holds the mutex.
 */
typedef sem_t bdmf_mutex;
#define bdmf_mutex_init(pmutex)        sem_init(pmutex, 0, 1)
#define bdmf_mutex_lock(pmutex)        ((sem_wait(pmutex) == -1) ? BDMF_ERR_INTR : 0)
#define bdmf_mutex_unlock(pmutex)      sem_post(pmutex)
#define bdmf_mutex_delete(pmutex)


/* Task-aware (recursive) mutex
 * The same task can take the same mutex multiple times.
 * However, if task B attempts to take mutex that is already taken by task A,
 * it will block
 */
typedef struct { int initialized; char b[128]; } bdmf_ta_mutex;
void bdmf_ta_mutex_init(bdmf_ta_mutex *pmutex);
int  bdmf_ta_mutex_lock(bdmf_ta_mutex *pmutex);
void bdmf_ta_mutex_unlock(bdmf_ta_mutex *pmutex);
void bdmf_ta_mutex_delete(bdmf_ta_mutex *pmutex);

#define __BDMF_TA_MUTEX_INITIALIZER(lock) {.initialized = 0}

typedef bdmf_ta_mutex bdmf_reent_fastlock;
#define bdmf_reent_fastlock_init(plock)  bdmf_ta_mutex_init(plock)
#define bdmf_reent_fastlock_lock(plock)  bdmf_ta_mutex_lock(plock)
#define bdmf_reent_fastlock_unlock(plock) bdmf_ta_mutex_unlock(plock)

typedef struct { int initialized; int locked; } bdmf_simple_mutex;
static inline void bdmf_simple_mutex_init(bdmf_simple_mutex *pmutex)
{
    pmutex->locked = 0;
    pmutex->initialized = 1;
}

static inline int bdmf_simple_mutex_lock(const char *func, int line, bdmf_simple_mutex *pmutex)
{
    if (!pmutex->initialized)
        bdmf_simple_mutex_init(pmutex);
    if (pmutex->locked)
        bdmf_print("%s:%d> LOCK APPLIED WHEN INTERRUPTS LOCKED (expect re-entrant?)!!!\n", func, line);

    pmutex->locked = 1;

    return 0;
}

static inline void bdmf_simple_mutex_unlock(const char *func, int line, bdmf_simple_mutex *pmutex)
{
    if (!pmutex->initialized)
        bdmf_simple_mutex_init(pmutex);

    if (!pmutex->locked)
        bdmf_print("%s:%d> UNLOCK APPLIED WHEN INTERRUPTS UNLOCKED!!!\n", func, line);

    pmutex->locked = 0;
}

static inline void bdmf_simple_mutex_delete(bdmf_simple_mutex *pmutex)
{
    pmutex->locked = 0;
    pmutex->initialized = 0;
}

#define __BDMF_SIMPLE_MUTEX_INITIALIZER(lock) {.initialized = 0}

/* Fast lock/unlock */
typedef bdmf_simple_mutex bdmf_fastlock;
#define DEFINE_BDMF_FASTLOCK(lock) bdmf_fastlock lock = __BDMF_SIMPLE_MUTEX_INITIALIZER(lock)
#define bdmf_fastlock_init(plock)  bdmf_simple_mutex_init(plock)
#define bdmf_fastlock_lock(plock)  bdmf_simple_mutex_lock(__FUNCTION__, __LINE__, plock)
#define bdmf_fastlock_unlock(plock) bdmf_simple_mutex_unlock(__FUNCTION__, __LINE__, plock)
#define bdmf_fastlock_lock_irq(plock, flags) \
    do { \
        bdmf_simple_mutex_lock(__FUNCTION__, __LINE__, plock); \
        flags = 0; \
    } while (flags)
#define bdmf_fastlock_unlock_irq(plock, flags)  bdmf_simple_mutex_unlock(__FUNCTION__, __LINE__, plock)

/* Tasks */
typedef pthread_t bdmf_task;
int bdmf_task_create(const char *name, int priority, int stack,
                int (*handler)(void *arg), void *arg, bdmf_task *ptask);
#define BDMFSYS_DEFAULT_TASK_PRIORITY     (-1)
#define BDMFSYS_DEFAULT_TASK_STACK        (-1)
int bdmf_task_destroy(bdmf_task task);
#define bdmf_task_wait(kick)   bdmf_mutex_lock(kick)
#define bdmf_task_kick(kick)   bdmf_mutex_unlock(kick)
#define bdmf_usleep(_us)       usleep(_us)
#define likely(x) (x)
#define unlikely(x) (x)
#define in_irq() (0)

/* File IO */
typedef int bdmf_file;
#define BDMF_FMODE_RDONLY   (O_RDONLY)
#define BDMF_FMODE_WRONLY   (O_WRONLY)
#define BDMF_FMODE_RDWR     (O_RDWR)
#define BDMF_FMODE_CREATE   (O_CREAT)
#define BDMF_FMODE_APPEND   (O_APPEND)
#define BDMF_FMODE_TRUNCATE (O_TRUNC)
#define BDMF_FMODE_SYNC     (O_SYNC)
static inline bdmf_file bdmf_file_open(const char *fname, int flags)
{
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int fd = open(fname, flags, mode);
    return (fd <= 0) ? 0 : fd;
}
static inline void bdmf_file_close(bdmf_file fd)
{
    close(fd);
}
static inline int bdmf_file_read(bdmf_file fd, void *buf, uint32_t size)
{
    int rc = read(fd, buf, size);
    return (rc >= 0) ? rc : BDMF_ERR_IO;
}
static inline int bdmf_file_write(bdmf_file fd, const void *buf, uint32_t size)
{
    int rc = write(fd, buf, size);
    return (rc >= 0) ? rc : BDMF_ERR_IO;
}

/* mmap shared area */
void *bdmf_mmap(const char *fname, uint32_t size);

/* IRQ handling */
typedef int (*f_bdmf_irq_cb)(int irq, void *data);
#define BDMFSYS_IRQ__NUM_OF   64
int bdmf_irq_connect(int irq, f_bdmf_irq_cb cb, void *data);
int bdmf_irq_free(int irq, f_bdmf_irq_cb cb, void *data);
void bdmf_irq_raise(int irq);

/*
 * dcache
 */
#define bdmf_dcache_inv(addr, size)
#define bdmf_dcache_flush(addr, size)


/*
 * System buffer support
 */

/** System buffer type */
typedef enum
{
    bdmf_sysb_skb,          /**< sk_buff */
    bdmf_sysb_fkb,          /**< fkbuff */

    bdmf_sysb_type__num_of
} bdmf_sysb_type;

/** System buffer */
typedef void *bdmf_sysb;

/** Get sysb type
 * \param[in]   sysb        System buffer
 * \return system buffer type
 */
static inline bdmf_sysb_type bdmf_sysb_typeof(bdmf_sysb sysb)
{
    return bdmf_sysb_skb;
}

/** Set headroom size for system buffer
 * \param[in]   sysb_type   System buffer type
 * \param[in]   headroom    Headroom size
 */
void bdmf_sysb_headroom_size_set(bdmf_sysb_type sysb_type, uint32_t headroom);

/** Allocate system buffer.
 * \param[in]   sysb_type   System buffer type
 * \param[in]   length      Data length
 * \return system buffer pointer.
 * If the function returns NULL, caller is responsible for "data" deallocation
 */
bdmf_sysb bdmf_sysb_alloc(bdmf_sysb_type sysb_type, uint32_t length);

/** Release system buffer.
 * \param[in]   sysb        System buffer
 */
static inline void bdmf_sysb_free(bdmf_sysb sysb)
{
    BUG_ON(bdmf_sysb_typeof(sysb) != bdmf_sysb_skb);
    dev_kfree_skb((struct sk_buff *)sysb);
}

/** Get sysb data pointer
 * \param[in]   sysb        System buffer
 * \return data pointer
 */
static inline void *bdmf_sysb_data(const bdmf_sysb sysb)
{
    BUG_ON(bdmf_sysb_typeof(sysb) != bdmf_sysb_skb);
    return ((struct sk_buff *)sysb)->data;
}

/** Get sysb data length
 * \param[in]   sysb        System buffer
 * \return data length
 */
static inline uint32_t bdmf_sysb_length(const bdmf_sysb sysb)
{
    BUG_ON(bdmf_sysb_typeof(sysb) != bdmf_sysb_skb);
    return ((struct sk_buff *)sysb)->len;
}


/** Get sysb data length
 * \param[in]   sysb        System buffer
 * \return data length
 */
static inline uint32_t bdmf_sysb_data_length(const bdmf_sysb sysb)
{
    BUG_ON(bdmf_sysb_typeof(sysb) != bdmf_sysb_skb);
    return ((struct sk_buff *)sysb)->len;
}
static inline void bdmf_sysb_data_flush(const bdmf_sysb sysb, void *data, uint32_t len)
{
    BUG_ON(bdmf_sysb_typeof(sysb) != bdmf_sysb_skb);
    bdmf_dcache_flush(data, len);
}

/** convert sysb to skb or fkb
 * \param[in]   sysb        System buffer
 * \return skb or fkb
 */
static inline void *bdmf_sysb_2_fkb_or_skb( bdmf_sysb sysb )
{
    /*currently only skb is supported*/
    BUG_ON(bdmf_sysb_typeof(sysb) != bdmf_sysb_skb);
    return ((struct sk_buff *)sysb);
}

/** Reserve headroom
 *
 * The function will assert if called for sysb containing data
 *
 * \param[in]   sysb        System buffer
 * \param[in]   bytes       Bytes to reserve
 */
static inline void bdmf_sysb_reserve(const bdmf_sysb sysb, uint32_t bytes)
{
    BUG_ON(bdmf_sysb_typeof(sysb) != bdmf_sysb_skb);
    skb_reserve((struct sk_buff *)sysb, bytes);
}

/** Allocate system buffer header i.e skb or fkb structure
 *  and initilize it with the provided len & data buffer
 * \param[in]   sysb_type   System buffer type
 * \param[in]   len         Data length
 * \return system buffer pointer.
 * If the function returns NULL, caller is responsible for "data" deallocation
 */
static inline bdmf_sysb bdmf_sysb_header_alloc(bdmf_sysb_type sysb_type, void* datap, uint32_t len, uint32_t context)
{
    BUG_ON(sysb_type != bdmf_sysb_skb);

    /* skb is part of data pointer */
    struct sk_buff *skb = datap - (sizeof(struct sk_buff) + SKB_RESERVE);
    memset(skb, 0, sizeof(struct sk_buff));

    skb->magic = SKB_MAGIC;
    skb->data = (uint8_t *)skb + sizeof(struct sk_buff) + SKB_RESERVE;
    skb->tail = skb->data;
    skb->len = len;
    skb->end = (uint8_t *)skb + SKB_ALLOC_LEN;
    return skb;
}


/** Allocate data buffers.
 * \param[out]  bufp        Array to hold allocated data buffers
 * \param[in]   num_buffs   number of buffers to allocate
 * \param[in]   context     currently unused
 * \returns     number of buffers allocated.
 */
static inline uint32_t bdmf_sysb_databuf_alloc(uint32_t *bufp, uint32_t num_buffs, uint32_t context)
{
#if 0
    void *datap;

    datap = bdmf_mem_alloc(NULL, BDMF_MEM_CACHE, SKB_ALLOC_LEN, SKB_ALLOC_ALIGN);

    if(!datap)
    {
        return 0;
    }
    /* do a cache invalidate of the buffer */
    bdmf_dcache_inv((unsigned long)datap, SKB_ALLOC_LEN);

    /*reserve space for headroom & SKB */
    bufp[0] = (uint32_t)(datap + sizeof(struct sk_buff) + SKB_RESERVE);
    /* always return only one buffer */
    return 1;
#else
    /*TODO fix this with proper len */
    struct sk_buff *skb = dev_alloc_skb(SKB_RESERVE);
    if (!skb)
        return 0;

    bufp[0] = (uint32_t)skb->data;
    return 1;
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
    dev_kfree_skb_data(datap);
}

/** Add data to sysb
 *
 * The function will is similar to skb_put()
 *
 * \param[in]   sysb        System buffer
 * \param[in]   bytes       Bytes to add
 * \returns added block pointer
 */
static inline void *bdmf_sysb_put(const bdmf_sysb sysb, uint32_t bytes)
{
    BUG_ON(bdmf_sysb_typeof(sysb) != bdmf_sysb_skb);
    return skb_put((struct sk_buff *)sysb, bytes);
}

/*
 * Platform buffer support
 */

/** Platform buffer */
typedef struct
{
    void *data;             /**< Buffer pointer */
    uint16_t bpm_bn;        /**< Buffer number */
    uint16_t source;        /**< Buffer source */
    uint16_t offset;        /**< Buffer offset */
    uint16_t length;        /**< Buffer length */
} bdmf_pbuf_t;

void bdmf_pbuf_init(uint32_t size, uint32_t offset);

int bdmf_pbuf_alloc(void *data, uint32_t length, uint16_t source, bdmf_pbuf_t *pbuf);

void bdmf_pbuf_free(bdmf_pbuf_t *pbuf);

int bdmf_pbuf_from_sysb(const bdmf_sysb sysb, uint16_t pbuf_source, bdmf_pbuf_t *pbuf);

bdmf_sysb bdmf_pbuf_to_sysb(bdmf_sysb_type sysb_type, bdmf_pbuf_t *pbuf);

int bdmf_sysb_is_pbuf(bdmf_sysb sysb);

static inline int bdmf_sysb_complete_packet(bdmf_sysb sysb)
{
    BUG_ON(bdmf_sysb_typeof(sysb) != bdmf_sysb_skb);
    return BDMF_ERR_OK;
}

/*-----------------------------------------------------------------------
 * Timers
 * todo: add "real" timer mapping
 *----------------------------------------------------------------------*/

/** timer handle */
typedef struct bdmf_timer bdmf_timer_t;

/** timer callback function
 * \param[in]   timer   timer that has expired
 * \param[in]   priv    private cookie passed in bdmf_timer_init()
 */
typedef void (*bdmf_timer_cb_t)(bdmf_timer_t *timer, unsigned long priv);

struct bdmf_timer
{
    unsigned long priv;
    bdmf_timer_cb_t cb;
};

/** Initialize timer
 * \param[in]   timer   timer handle
 * \param[in]   cb      callback to be called upon expiration
 * \param[in]   priv    private cooke to be passed to cb()
 */
void bdmf_timer_init(bdmf_timer_t *timer, bdmf_timer_cb_t cb, unsigned long priv);

/** Start timer
 * \param[in]   timer   timer handle that has been initialized using bdmf_timer_init()
 * \param[in]   ticks   number of ticks from now to expiration
 * \returns 0=OK or error code <0
 */
int bdmf_timer_start(bdmf_timer_t *timer, uint32_t ticks);

/** stop timer
 * \param[in]   timer   timer to be stopped
 * The function is safe to call even if timer is not running
 */
void bdmf_timer_stop(bdmf_timer_t *timer);

/** Delete timer
 * \param[in]   timer   timer to be deleted
 * The timer is stopped if running
 */
void bdmf_timer_delete(bdmf_timer_t *timer);

/** Convert ms to ticks
 * \param[in]   ms  ms
 * \returns timer ticks
 */
uint32_t bdmf_ms_to_ticks(uint32_t ms);



#define BDMF_IRQ_NONE       0           /**< IRQ is not from this device */
#define BDMF_IRQ_HANDLED    1           /**< IRQ has been handled */

#define BDMF_IRQF_DISABLED  1           /**< Interrupt is disabled after connect */

/** Connect system interrupt
 * \param[in]   irq     IRQ number
 * \param[in]   cpu     CPU number (for SMP)
 * \param[in]   flags   IRQ flags
 * \param[in]   isr     ISR
 * \param[in]   name    device name
 * \param[in]   priv    Private cookie
 * \returns 0=OK, <0- error
 */
static inline int bdmf_int_connect(int irq, int cpu, int flags,
    int (*isr)(int irq, void *priv), const char *name, void *priv)
{
    return 0;
}

/** Disconnect system interrupt
 * \param[in]   irq     IRQ number
 * \param[in]   priv    Private cookie passed in bdmf_int_connect()
 * \returns 0=OK, <0- error
 */
static inline void bdmf_int_disconnect(int irq, void *priv)
{
}

/** Unmask IRQ
 * \param[in]   irq IRQ
 */
static inline void bdmf_int_enable(int irq)
{
}

/** Mask IRQ
 * \param[in]   irq IRQ
 */
static inline void bdmf_int_disable(int irq)
{
}

/*-----------------------------------------------------------------------
 * Endian-related macros and constants.
 *
 *  __BYTE_ORDER define is set by GCC compilation environment.
 *  No need to do anything here.
 *  The following must be defined:
 *  __BYTE_ORDER
 *  __LITTLE_ENDIAN
 *  __BIG_ENDIAN
 *  __bswap_16(x)
 *  __bswap_32(x)
 *
 *----------------------------------------------------------------------*/

/*-----------------------------------------------------------------------
 * A few defines to make porting of linux drivers into BDMF smoother
 *----------------------------------------------------------------------*/
struct module;
#define __user
#define __init
#define __exit
#define EXPORT_SYMBOL(x)
#define MODULE_AUTHOR(x)
#define MODULE_DESCRIPTION(x)
#define MODULE_LICENSE(x)
#define module_init(x)
#define module_exit(x)

#endif /* _BDMF_SYSTEM_H_ */
