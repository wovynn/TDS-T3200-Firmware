/*
<:copyright-BRCM:2002:GPL/GPL:standard

   Copyright (c) 2002 Broadcom Corporation
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
/***************************************************************************
* File Name  : bcm63xx_gpio.c
*
* Description: This file contains functions related to GPIO access.
*     See GPIO register defs in shared/broadcom/include/bcm963xx/xxx_common.h
*
*
***************************************************************************/

/*
 * Access to _SHARED_ GPIO registers should go through common functions
 * defined in board.h.  These common functions will use a spinlock with
 * irq's disabled to prevent concurrent access.
 * Functions which don't want to call the common gpio access functions must
 * acquire the bcm_gpio_spinlock with irq's disabled before accessing the
 * shared GPIO registers.
 * The GPIO registers that must be protected are:
 * GPIO->GPIODir
 * GPIO->GPIOio
 * GPIO->GPIOMode
 *
 * Note that many GPIO registers are dedicated to some driver or sub-system.
 * In those cases, the driver/sub-system can use its own locking scheme to
 * ensure serial access to its GPIO registers.
 *
 * DEFINE_SPINLOCK is the new, recommended way to declaring a spinlock.
 * See spinlock_types.h
 *
 * I am using a spin_lock_irqsave/spin_lock_irqrestore to lock and unlock
 * so that GPIO's can be accessed in interrupt context.
 */
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/bcm_assert_locks.h>
#include <bcm_map_part.h>
#include "board.h"
#include "shared_utils.h"
#include "bcm_gpio.h"
#include <boardparms.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
#include <linux/module.h>
#include <linux/cache.h>
#endif



DEFINE_SPINLOCK(bcm_gpio_spinlock);
EXPORT_SYMBOL(bcm_gpio_spinlock);


void kerSysSetGpioStateLocked(unsigned short bpGpio, GPIO_STATE_t state)
{
    BCM_ASSERT_V(bpGpio != BP_NOT_DEFINED);
    BCM_ASSERT_V((bpGpio & BP_GPIO_NUM_MASK) < GPIO_NUM_MAX);
    BCM_ASSERT_HAS_SPINLOCK_V(&bcm_gpio_spinlock);

    kerSysSetGpioDirLocked(bpGpio);

#if defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) 
    /* Take over high GPIOs from WLAN block */
    if ((bpGpio & BP_GPIO_NUM_MASK) > 31)
        GPIO->GPIOCtrl |= GPIO_NUM_TO_MASK(bpGpio - 32);
#endif

    if((state == kGpioActive && !(bpGpio & BP_ACTIVE_LOW)) ||
        (state == kGpioInactive && (bpGpio & BP_ACTIVE_LOW)))
#if defined(CONFIG_BCM96838) 
        gpio_set_data((bpGpio & BP_GPIO_NUM_MASK), 1);
#elif defined(CONFIG_BCM963138) || defined(CONFIG_BCM963381) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM960333) || defined(CONFIG_BCM96848)
        bcm_gpio_set_data((bpGpio & BP_GPIO_NUM_MASK), 1);
#else
        GPIO->GPIOio |= GPIO_NUM_TO_MASK(bpGpio);
#endif
    else
#if defined(CONFIG_BCM96838) 
        gpio_set_data((bpGpio & BP_GPIO_NUM_MASK), 0);
#elif defined(CONFIG_BCM963138) || defined(CONFIG_BCM963381) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM960333) || defined(CONFIG_BCM96848)
        bcm_gpio_set_data((bpGpio & BP_GPIO_NUM_MASK), 0);
#else
        GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(bpGpio);
#endif
}

void kerSysSetGpioState(unsigned short bpGpio, GPIO_STATE_t state)
{
    unsigned long flags;

    spin_lock_irqsave(&bcm_gpio_spinlock, flags);
    kerSysSetGpioStateLocked(bpGpio, state);
    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
}



void kerSysSetGpioDirLocked(unsigned short bpGpio)
{
    BCM_ASSERT_V(bpGpio != BP_NOT_DEFINED);
    BCM_ASSERT_V((bpGpio & BP_GPIO_NUM_MASK) < GPIO_NUM_MAX);
    BCM_ASSERT_HAS_SPINLOCK_V(&bcm_gpio_spinlock);

#if   defined(CONFIG_BCM96838) 
    gpio_set_dir(bpGpio & BP_GPIO_NUM_MASK, 1);
#elif defined(CONFIG_BCM963138) || defined(CONFIG_BCM963381) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM960333) || defined(CONFIG_BCM96848)
    bcm_gpio_set_dir(bpGpio, 1);
#else
    GPIO->GPIODir |= GPIO_NUM_TO_MASK(bpGpio);
#endif
}

void kerSysSetGpioDir(unsigned short bpGpio)
{
    unsigned long flags;

    spin_lock_irqsave(&bcm_gpio_spinlock, flags);
    kerSysSetGpioDirLocked(bpGpio);
    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
}


/* Set gpio direction to input. Parameter gpio is in boardparms.h format. */
int kerSysSetGpioDirInput(unsigned short bpGpio)
{
    unsigned long flags;

    spin_lock_irqsave(&bcm_gpio_spinlock, flags);
  //  GPIO_TAKE_CONTROL(bpGpio & BP_GPIO_NUM_MASK);

#if defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) 
    /* Take over high GPIOs from WLAN block */
    if ((bpGpio & BP_GPIO_NUM_MASK) > 31)
        GPIO->GPIOCtrl |= GPIO_NUM_TO_MASK(bpGpio - 32);
#endif

#if defined(CONFIG_BCM96838) 
    gpio_set_dir(bpGpio & BP_GPIO_NUM_MASK, 0);
#elif defined(CONFIG_BCM963138) || defined(CONFIG_BCM963381) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM960333) || defined(CONFIG_BCM96848)
    bcm_gpio_set_dir(bpGpio, 0);
#else
    GPIO->GPIODir &= ~GPIO_NUM_TO_MASK(bpGpio);
#endif
    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);

    return(0);
}

/* Return a gpio's value, 0 or 1. Parameter gpio is in boardparms.h format. */
int kerSysGetGpioValue(unsigned short bpGpio)
{
#if defined(CONFIG_BCM96838) 
    return gpio_get_data(bpGpio & BP_GPIO_NUM_MASK);
#elif defined(CONFIG_BCM963138) || defined(CONFIG_BCM963381) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM960333) || defined(CONFIG_BCM96848)
    return bcm_gpio_get_data(bpGpio);
#else
    return((int) ((GPIO->GPIOio & GPIO_NUM_TO_MASK(bpGpio)) >>
        (bpGpio & BP_GPIO_NUM_MASK)));
#endif
}

#if defined(CONFIG_I2C_GPIO) && !defined(CONFIG_GPIOLIB)
/* GPIO bit functions to support drivers/i2c/busses/i2c-gpio.c */

#define GPIO_TAKE_CONTROL(G) 

/* noop */
int gpio_request(unsigned bpGpio, const char *label)
{
    return(0); /* success */
}

/* noop */
void gpio_free(unsigned bpGpio)
{
}

/* Assign a gpio's value. Parameter gpio is in boardparms.h format. */
void gpio_set_value(unsigned bpGpio, int value)
{
    unsigned long flags;

    /* value should be either 0 or 1 */
    spin_lock_irqsave(&bcm_gpio_spinlock, flags);

#if   defined(CONFIG_BCM96838) 
    gpio_set_data(bpGpio & BP_GPIO_NUM_MASK, value);
#elif defined(CONFIG_BCM963138) || defined(CONFIG_BCM963381) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM960333) || defined(CONFIG_BCM96848)
    bcm_gpio_set_data(bpGpio, value);
#else
    GPIO_TAKE_CONTROL(bpGpio & BP_GPIO_NUM_MASK);
    if( value == 0 )
        GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(bpGpio);
    else
        if( value == 1 )
            GPIO->GPIOio |= GPIO_NUM_TO_MASK(bpGpio);
#endif

    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
}

/* Set gpio direction to input. Parameter gpio is in boardparms.h format. */
int gpio_direction_input(unsigned bpGpio)
{
    unsigned long flags;

    spin_lock_irqsave(&bcm_gpio_spinlock, flags);

#if   defined(CONFIG_BCM96838) 
    gpio_set_dir(bpGpio & BP_GPIO_NUM_MASK, 0);
#elif defined(CONFIG_BCM963138) || defined(CONFIG_BCM963381) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM960333) || defined(CONFIG_BCM96848)
    bcm_gpio_set_dir(bpGpio, 0);
#else
    GPIO_TAKE_CONTROL(bpGpio & BP_GPIO_NUM_MASK);
    GPIO->GPIODir &= ~GPIO_NUM_TO_MASK(bpGpio);
#endif

    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
    return(0);
}

/* Set gpio direction to output. Parameter gpio is in boardparms.h format. */
int gpio_direction_output(unsigned bpGpio, int value)
{
    unsigned long flags;

    spin_lock_irqsave(&bcm_gpio_spinlock, flags);

#if   defined(CONFIG_BCM96838) 
    gpio_set_dir(bpGpio & BP_GPIO_NUM_MASK, 1);
#elif defined(CONFIG_BCM963138) || defined(CONFIG_BCM963381) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM960333) || defined(CONFIG_BCM96848)
    bcm_gpio_set_dir(bpGpio, 1);
#else
    GPIO_TAKE_CONTROL(bpGpio & BP_GPIO_NUM_MASK);
    GPIO->GPIODir |= GPIO_NUM_TO_MASK(bpGpio);
#endif

    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);

    gpio_set_value(bpGpio, value);

    return(0);
}

/* Return a gpio's value, 0 or 1. Parameter gpio is in boardparms.h format. */
int gpio_get_value(unsigned bpGpio)
{
#if   !defined(CONFIG_BCM96838) && !defined(CONFIG_BCM963138) && !defined(CONFIG_BCM963381) &&!defined(CONFIG_BCM963148) && !defined(CONFIG_BCM960333) &&!defined(CONFIG_BCM96848)
    unsigned long flags;

    spin_lock_irqsave(&bcm_gpio_spinlock, flags);
    GPIO_TAKE_CONTROL(bpGpio & BP_GPIO_NUM_MASK);
    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
#endif

#if   defined(CONFIG_BCM96838) 
    return gpio_get_data(bpGpio & BP_GPIO_NUM_MASK);
#elif defined(CONFIG_BCM963138) || defined(CONFIG_BCM963381) || defined(CONFIG_BCM963148) || defined(CONFIG_BCM960333) || defined(CONFIG_BCM96848)
    return(bcm_gpio_get_data(bpGpio));
#else
    return((int) ((GPIO->GPIOio & GPIO_NUM_TO_MASK(bpGpio)) >>
        (bpGpio & BP_GPIO_NUM_MASK)));
#endif

}
#endif

EXPORT_SYMBOL(kerSysSetGpioState);
EXPORT_SYMBOL(kerSysSetGpioStateLocked);
EXPORT_SYMBOL(kerSysSetGpioDir);
EXPORT_SYMBOL(kerSysSetGpioDirLocked);
EXPORT_SYMBOL(kerSysSetGpioDirInput);
EXPORT_SYMBOL(kerSysGetGpioValue);

