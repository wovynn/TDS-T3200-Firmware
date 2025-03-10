#if defined(CONFIG_BCM_KF_MIPS_BCM963XX) && defined(CONFIG_MIPS_BCM963XX)

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
/*
 * Generic setup routines for Broadcom 963xx MIPS boards
 */

//#include <linux/config.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/console.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/bootmem.h>

#include <asm/addrspace.h>
#include <asm/bcache.h>
#include <asm/irq.h>
#include <asm/time.h>
#include <asm/reboot.h>
//#include <asm/gdb-stub.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>

#include <linux/platform_device.h>

#include <boardparms.h>

extern unsigned long getMemorySize(void);
extern irqreturn_t brcm_timer_interrupt(int irq, void *dev_id);

#include <bcm_map_part.h>
#include <bcm_cpu.h>
#include <bcm_intr.h>
#include <board.h>
#include <boardparms.h>

#if defined(CONFIG_PCI)
#include <linux/pci.h>
#include <bcmpci.h>
#endif

#if IS_ENABLED(CONFIG_BCM_ADSL)
#include "softdsl/AdslCoreDefs.h"
#endif

#if defined(CONFIG_BCM_ENDPOINT_MODULE)
#include <dsp_mod_size.h>
#endif


#include "shared_utils.h"
#include <pmc_usb.h>

#if 1

/***************************************************************************
 * C++ New and delete operator functions
 ***************************************************************************/

/* void *operator new(unsigned int sz) */
void *_Znwj(unsigned int sz)
{
    return( kmalloc(sz, GFP_KERNEL) );
}

/* void *operator new[](unsigned int sz)*/
void *_Znaj(unsigned int sz)
{
    return( kmalloc(sz, GFP_KERNEL) );
}

/* placement new operator */
/* void *operator new (unsigned int size, void *ptr) */
void *ZnwjPv(unsigned int size, void *ptr)
{
    return ptr;
}

/* void operator delete(void *m) */
void _ZdlPv(void *m)
{
    kfree(m);
}

/* void operator delete[](void *m) */
void _ZdaPv(void *m)
{
    kfree(m);
}

EXPORT_SYMBOL(_Znwj);
EXPORT_SYMBOL(_Znaj);
EXPORT_SYMBOL(ZnwjPv);
EXPORT_SYMBOL(_ZdlPv);
EXPORT_SYMBOL(_ZdaPv);

#endif

#if IS_ENABLED(CONFIG_BCM_ADSL)
/***************************************************************************
 * Function Name: kerSysGetDslPhyMemory
 * Description  : return the start address of the reserved DSL SDRAM. The memory
 *                is reserved in the arch dependent setup.c
 * Returns      : physical address of the reserved DSL SDRAM
 ***************************************************************************/
void* kerSysGetDslPhyMemory(void)
{
    return (void*)((getMemorySize() - ADSL_SDRAM_IMAGE_SIZE));
}

EXPORT_SYMBOL(kerSysGetDslPhyMemory);

#endif

bool kerSysIsRootfsSet(void)
{
	char *cmd_ptr;

	cmd_ptr = strstr(arcs_cmdline, "root=");
	if (cmd_ptr != NULL)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(kerSysIsRootfsSet);

#if defined(CONFIG_BCM96838) || defined(CONFIG_BCM96848)
struct boot_mem_name 
{
	char name[32];
} map_name[BOOT_MEM_MAP_MAX];


static unsigned long get_usable_mem_size(void)
{
	int i;
	
	for(i = 0; i < boot_mem_map.nr_map; i++)
	{
		if( boot_mem_map.map[i].type == BOOT_MEM_RAM )
			return boot_mem_map.map[i].size;
	}
	
	return 0;
}
static void BcmMemReserveInit(void)
{
	unsigned long 		memsize = getMemorySize();
	unsigned long		size = 0;
	NVRAM_DATA NvramData;

	if (boot_mem_map.nr_map != 0)
	{
		unsigned long usr_usable		= get_usable_mem_size();
		unsigned long usr_reserved	= memsize - usr_usable;
		
		if( usr_usable == 0 )
		{
			printk("Error: No usable memory detected\n");
			BUG_ON(1);
		}
		
		if( memsize < usr_usable )
		{
			printk("Error: Detected usable memory greater than physical memory\n");
			BUG_ON(1);
		}

		boot_mem_map.nr_map = 0;
		if(usr_reserved)
		{
			memsize -= usr_reserved;
			strcpy(map_name[boot_mem_map.nr_map].name, "mem_user_reserved");
			add_memory_region(memsize, usr_reserved, BOOT_MEM_RESERVED);
		}
	}

	kerSysNvRamGet((char *)&NvramData, sizeof(NVRAM_DATA), 0);

	size = NvramData.allocs.alloc_rdp.tmsize;
	if (size == 0xff) {
		/* Erased NVRAM should be alloc TM_ERASED_NVRAM_DEF_DDR_SIZE to be
         * backward compatible */
		size = TM_ERASED_NVRAM_DEF_DDR_SIZE;
	}
	size = size * 0x100000;

	
	if(size < TM_DEF_DDR_SIZE)
		size = TM_DEF_DDR_SIZE;

    /* TM_BASE_ADDR_STR must be 2MB aligned, reserve unaligned block to heap */
    if ((memsize - size) % (2 * 1024 * 1024) != 0)
    {
        int tempsize = (memsize - size) % (2 * 1024 * 1024);

        memsize -= tempsize;
        strcpy(map_name[boot_mem_map.nr_map].name, "BOOT_MEM_RAM");	
        add_memory_region(memsize, tempsize, BOOT_MEM_RAM);
    }

	memsize -= size;
	strcpy(map_name[boot_mem_map.nr_map].name, TM_BASE_ADDR_STR);	
	add_memory_region(memsize, size, BOOT_MEM_RESERVED);

	size = NvramData.allocs.alloc_rdp.mcsize;
	if (size == 0xff) {
		/* Erased NVRAM should be treated as zero */
		size = 0;
	}
	size = size * 0x100000;
	
	if(size < TM_MC_DEF_DDR_SIZE)
		size = TM_MC_DEF_DDR_SIZE;
		
	memsize -= size;
	strcpy(map_name[boot_mem_map.nr_map].name, TM_MC_BASE_ADDR_STR);	
	add_memory_region(memsize, size, BOOT_MEM_RESERVED);

//#ifdef CONFIG_DHD_RUNNER
    /* Add memory for DHD offload */
    /* XXX: Temporary solution, should be under dedicated #ifdef */

    size = NvramData.alloc_dhd.dhd_size[0];
    if (size != 0xff && size != 0) {
        size = size * 0x100000;
        memsize -= size;
        strcpy(map_name[boot_mem_map.nr_map].name, DHD_BASE_ADDR_STR); 
        add_memory_region(memsize, size, BOOT_MEM_RESERVED);
    }
    size = NvramData.alloc_dhd.dhd_size[1];
    if (size != 0xff && size != 0) {
        size = size * 0x100000;
        memsize -= size;
        strcpy(map_name[boot_mem_map.nr_map].name, DHD_BASE_ADDR_STR_1); 
        add_memory_region(memsize, size, BOOT_MEM_RESERVED);
    }
    size = NvramData.alloc_dhd.dhd_size[2];
    if (size != 0xff && size != 0) {
        size = size * 0x100000;
        memsize -= size;
        strcpy(map_name[boot_mem_map.nr_map].name, DHD_BASE_ADDR_STR_2); 
        add_memory_region(memsize, size, BOOT_MEM_RESERVED);
    }

//#endif

	/* add the linux usable region */
	add_memory_region(0, memsize, BOOT_MEM_RAM);
}

int BcmMemReserveGetByName(char *name, void **addr, uint32_t *size)
{
	int i;
	*addr = NULL;
	*size = 0;

	/* the data in index i of boot_mem_map refers to the name in map_name with the same index i */
	for (i=0; i<boot_mem_map.nr_map; i++)
	{
		if ( strcmp(name, map_name[i].name) == 0 )
		{
			*addr = (void*)(((unsigned int)boot_mem_map.map[i].addr) | 0xA0000000);
			*size = boot_mem_map.map[i].size;
			return 0;
		}
	}
	return -1;
}
EXPORT_SYMBOL(BcmMemReserveGetByName);
#endif

void __init plat_mem_setup(void)
{
#if defined(CONFIG_BCM96838) || defined(CONFIG_BCM96848)
	BcmMemReserveInit();
#elif !IS_ENABLED(CONFIG_BCM_ADSL)
    add_memory_region(0, (getMemorySize()), BOOT_MEM_RAM);
#else
    printk("DSL SDRAM reserved: 0x%x\n", ADSL_SDRAM_IMAGE_SIZE);
    add_memory_region(0, (unsigned long)(kerSysGetDslPhyMemory()), BOOT_MEM_RAM);
#endif

    {
        volatile unsigned long *cr;
        uint32 mipsBaseAddr = MIPS_BASE;

        cr = (void *)(mipsBaseAddr + MIPS_RAC_CR0);
        *cr = *cr | RAC_D;

#if defined(MIPS_RAC_CR1)
        cr = (void *)(mipsBaseAddr + MIPS_RAC_CR1);
        *cr = *cr | RAC_D;
#endif        
    }
}


extern UINT32 __init calculateCpuSpeed(void);
#ifdef CONFIG_BCM_HOSTMIPS_PWRSAVE_TIMERS
extern void BcmPwrMngtInitC0Speed (void);
#endif


void __init plat_time_init(void)
{
    mips_hpt_frequency = calculateCpuSpeed() / 2;
#ifdef CONFIG_BCM_HOSTMIPS_PWRSAVE_TIMERS
    BcmPwrMngtInitC0Speed();
#else
    // Enable cp0 counter/compare interrupt when
    // not using workaround for clock divide
    write_c0_status(IE_IRQ5 | read_c0_status());
#endif
}


static void brcm_machine_restart(char *command)
{
    kerSysMipsSoftReset();
}

extern void stop_other_cpu(void);  // in arch/mips/kernel/smp.c

static void brcm_machine_halt(void)
{
    /*
     * we don't support power off yet.  This halt will cause both CPU's to
     * spin in a while(1) loop with interrupts disabled.  (Used for gathering
     * wlan debug dump via JTAG)
     */
#if defined(CONFIG_SMP)
    stop_other_cpu();
#endif
    printk("System halted\n");
    local_irq_disable();
    while (1);
}

#if defined(CONFIG_BCM96318) /* For 6318 for now but can be used for other chip if need */
/*  *********************************************************************
    *  setPinMuxGpio()
    *
    *  Set pin mux to GPIO function
    *
    *  Input parameters:
    *      unsigned short gpio pin number
    *
    *  Return value:
    *      nothing
    ********************************************************************* */
static void setPinMuxGpio(unsigned short gpio)
{
	uint32 reg, shift;
	volatile uint32* pinMuxSel;

        /* on 6318 device, gpio 13 to 41 pin is not default to gpio function. must change
        * the pinmux to gpio function
        */

        if( (gpio&BP_GPIO_NUM_MASK) == BP_GPIO_NONE )
                return;

	/* not for any serial LED gpio as they don't take any gpio pins */
	if( (gpio&BP_GPIO_SERIAL) == BP_GPIO_SERIAL )
		return;

	gpio = gpio&BP_GPIO_NUM_MASK;
	if( gpio >= 13 && gpio <= 41 )
	{
        /* set the pad control to gpio */
		reg = GPIO->PadControl[gpio>>3];
		shift = (gpio&0x7)<<2;
		reg &= ~(PAD_CTRL_GPIO0_MASK<<shift);
		reg |= (PAD_CTRL_GPIO<<shift);
		GPIO->PadControl[gpio>>3] = reg;

		/* set pin mux to gpio */
		pinMuxSel = &GPIO->PinMuxSel0;
		pinMuxSel += gpio>>4;
		reg = *pinMuxSel;
		shift = (gpio&0xf)<<1;
		reg &= ~(PINMUX_SEL_GPIO0_MASK<<shift);
		reg |= (PINMUX_SEL_GPIO<<shift);
		*pinMuxSel = reg;
	}

	return;
}

/*  *********************************************************************
    *  initGpioPinMux()
    *
    *  Initialize the gpio pin mux register setting. On some chip like 6318, Certain
    *  gpio pin are muxed with other function and  are not default to gpio. so init
    *  code needs to set the mux to gpio if they are used by led or gpio boardparm
    *
    *
    *  Input parameters: none
    *
    *  Return value:
    *      nothing
    ********************************************************************* */
static void initGpioPinMux(void)
{
    int i = 0, token = 0, rc;
    unsigned short gpio;

    /* walk through all the led bp */
    for(;;)
    {
        rc = BpGetLedGpio(i, &token, &gpio);
       	if( rc == BP_MAX_ITEM_EXCEEDED )
       	    break;
       	else if( rc == BP_SUCCESS )
       	    setPinMuxGpio(gpio);
        else
	{
	    token = 0;
       	    i++;
	}
    }

    /* walk through all the gpio bp */
    i = 0;
    token = 0;
    for(;;)
    {
        rc = BpGetGpioGpio(i, &token, &gpio);
       	if( rc == BP_MAX_ITEM_EXCEEDED )
       	    break;
       	else if( rc == BP_SUCCESS )
       	    setPinMuxGpio(gpio);
        else
	{
	    token = 0;
       	    i++;
	}
    }

    return;
}
#endif

#if   defined(CONFIG_BCM96362)

static int __init bcm6362_hw_init(void)
{
    unsigned long GPIOOverlays, DeviceOptions = 0;
    unsigned short gpio;

    if( BpGetDeviceOptions(&DeviceOptions) == BP_SUCCESS ) {
        if(DeviceOptions&BP_DEVICE_OPTION_DISABLE_LED_INVERSION)
            MISC->miscLed_inv = 0;
    }
    
    /* Set LED blink rate for activity LEDs to 80mS */
    LED->ledInit &= ~LED_FAST_INTV_MASK;
    LED->ledInit |= (LED_INTERVAL_20MS * 4) << LED_FAST_INTV_SHIFT;

    if( BpGetGPIOverlays(&GPIOOverlays) == BP_SUCCESS ) {
        /* Start with all HW LEDs disabled */
        LED->ledHWDis |= 0xFFFFFF;
        if (GPIOOverlays & BP_OVERLAY_SERIAL_LEDS) {
            GPIO->GPIOMode |= (GPIO_MODE_SERIAL_LED_CLK | GPIO_MODE_SERIAL_LED_DATA);
            LED->ledInit |= LED_SERIAL_LED_EN;
        }

        if (GPIOOverlays & BP_OVERLAY_SPI_SSB2_EXT_CS) {           
            /* Enable Overlay for SPI SS2 Pin */            
             GPIO->GPIOMode |= GPIO_MODE_LS_SPIM_SSB2;                    
        }

        if (GPIOOverlays & BP_OVERLAY_SPI_SSB3_EXT_CS) {           
            /* Enable Overlay for SPI SS3 Pin */            
             GPIO->GPIOMode |= GPIO_MODE_LS_SPIM_SSB3;                    
        }

        /* Map HW LEDs to LED controller inputs and enable LED controller to drive GPIO */
        if (GPIOOverlays & BP_OVERLAY_USB_LED) {
            LED->ledLinkActSelLow |= ((1 << LED_USB_ACT) << LED_0_ACT_SHIFT);
            LED->ledLinkActSelLow |= ((1 << LED_USB_ACT) << LED_0_LINK_SHIFT);
            GPIO->LEDCtrl |= (1 << LED_USB_ACT);
            LED->ledHWDis &= ~(1 << LED_USB_ACT);
        }
        if ( BpGetWanDataLedGpio(&gpio) == BP_SUCCESS ) {
            if ((gpio & BP_GPIO_NUM_MASK) == LED_INET_ACT) {
                /* WAN Data LED must be LED 1 */
                LED->ledLinkActSelLow |= ((1 << LED_INET_ACT) << LED_1_ACT_SHIFT);
                GPIO->LEDCtrl |= GPIO_NUM_TO_MASK(gpio);
            }
        }
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_0) {
            LED->ledLinkActSelHigh |= ((1 << (LED_ENET0 - 4)) << LED_4_LINK_SHIFT);
            GPIO->LEDCtrl |= (1 << LED_ENET0);
            LED->ledHWDis &= ~(1 << LED_ENET0);
        }
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_1) {
            LED->ledLinkActSelHigh |= ((1 << (LED_ENET1 - 4)) << LED_5_LINK_SHIFT);
            GPIO->LEDCtrl |= (1 << LED_ENET1);
            LED->ledHWDis &= ~(1 << LED_ENET1);
        }
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_2) {
            LED->ledLinkActSelHigh |= ((1 << (LED_ENET2 - 4)) << LED_6_LINK_SHIFT);
            GPIO->LEDCtrl |= (1 << LED_ENET2);
            LED->ledHWDis &= ~(1 << LED_ENET2);
        }
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_3) {
            LED->ledLinkActSelHigh |= ((1 << (LED_ENET3 - 4)) << LED_7_LINK_SHIFT);
            GPIO->LEDCtrl |= (1 << LED_ENET3);
            LED->ledHWDis &= ~(1 << LED_ENET3);
        }
    }

#if defined(CONFIG_USB)
    PERF->blkEnables |= USBH_CLK_EN;
    mdelay(100);
    USBH->SwapControl = EHCI_ENDIAN_SWAP | OHCI_ENDIAN_SWAP;
    USBH->Setup |= USBH_IOC;
#else
    MISC->miscIddqCtrl |= MISC_IDDQ_CTRL_USBH;
    PERF->blkEnables &= ~USBH_CLK_EN;
#endif

#if defined(CONFIG_BCM_KF_FAP) && (defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE))
#else
    PERF->blkEnables &= ~FAP_CLK_EN;
#endif

#if defined(CONFIG_PCI)
    /* Enable WOC */  
    PERF->blkEnables |=WLAN_OCP_CLK_EN;
    mdelay(10);
    PERF->softResetB &= ~(SOFT_RST_WLAN_SHIM_UBUS | SOFT_RST_WLAN_SHIM);
    mdelay(1);
    PERF->softResetB |= (SOFT_RST_WLAN_SHIM_UBUS | SOFT_RST_WLAN_SHIM);
    mdelay(1);

    WLAN_SHIM->ShimMisc = (WLAN_SHIM_FORCE_CLOCKS_ON|WLAN_SHIM_MACRO_SOFT_RESET);
    mdelay(1);
    WLAN_SHIM->MacControl = (SICF_FGC|SICF_CLOCK_EN);
    WLAN_SHIM->ShimMisc = WLAN_SHIM_FORCE_CLOCKS_ON;
    WLAN_SHIM->ShimMisc = 0;
    WLAN_SHIM->MacControl = SICF_CLOCK_EN;        	

#endif    

#if defined(CONFIG_BCM_ETH_PWRSAVE)
    // Turn off pll_use_lock to allow watchdog timer to reset the chip when
    // ephy_pwr_down_dll is set in ethernet sleep mode
    MISC->miscStrapBus &= ~MISC_STRAP_BUS_PLL_USE_LOCK;
    MISC->miscStrapOverride |= 1;
#endif

#if defined(CONFIG_BCM_KF_POWER_SAVE) && defined(CONFIG_BCM_DDR_SELF_REFRESH_PWRSAVE)
    /* Enable power savings from DDR pads on this chip when DDR goes in Self-Refresh mode */
    DDR->PhyControl.IDLE_PAD_CONTROL = 0x00000172;
    DDR->PhyByteLane0Control.IDLE_PAD_CONTROL = 0x000fffff;
    DDR->PhyByteLane1Control.IDLE_PAD_CONTROL = 0x000fffff;
#endif

    if ( BpGetFemtoResetGpio(&gpio) == BP_SUCCESS ) {
        kerSysSetGpioState(gpio, kGpioActive);
    }

    return 0;
}
#define bcm63xx_specific_hw_init() bcm6362_hw_init()

#elif defined(CONFIG_BCM96328)

static int __init bcm6328_hw_init(void)
{
    unsigned long GPIOOverlays, DeviceOptions = 0;
    unsigned short gpio;
    
    if( BpGetDeviceOptions(&DeviceOptions) == BP_SUCCESS ) {
        if(DeviceOptions&BP_DEVICE_OPTION_DISABLE_LED_INVERSION)
            MISC->miscLedXorReg = 0;
    }

    /* Set LED blink rate for activity LEDs to 80mS */
    LED->ledInit &= ~LED_FAST_INTV_MASK;
    LED->ledInit |= (LED_INTERVAL_20MS * 4) << LED_FAST_INTV_SHIFT;

    if( BpGetGPIOverlays(&GPIOOverlays) == BP_SUCCESS ) {
        /* Start with all HW LEDs disabled */
        LED->ledHWDis |= 0xFFFFFF;
        if (GPIOOverlays & BP_OVERLAY_SERIAL_LEDS) {
            GPIO->PinMuxSel |= PINMUX_SERIAL_LED_DATA;
            GPIO->PinMuxSel |= PINMUX_SERIAL_LED_CLK;
            LED->ledInit |= LED_SERIAL_LED_EN;
        }
        if ( BpGetWanDataLedGpio(&gpio) == BP_SUCCESS ) {
            if ((gpio & BP_GPIO_NUM_MASK) < 4) {
                /* WAN Data LED must be LED 0-3 */
                LED->ledLinkActSelLow |= ((1 << LED_INET_ACT) << ((gpio & BP_GPIO_NUM_MASK) * 4));
                LED->ledLinkActSelLow |= ((1 << LED_INET_ACT) << (((gpio & BP_GPIO_NUM_MASK) * 4) + LED_0_LINK_SHIFT));
                GPIO->GPIOMode |= GPIO_NUM_TO_MASK(gpio);

                /* The following two checks are for legacy schematics */
                if (gpio & BP_GPIO_SERIAL) {
                    /* If WAN Data LED is serial, then configure serial controller to shift it out */
                    LED->ledSerialMuxSelect |= GPIO_NUM_TO_MASK(gpio);
                }
                if ((gpio & BP_GPIO_NUM_MASK) == 0) {
                    /* In case INET_ACT LED is connected to GPIO_11 */
                    GPIO->PinMuxSel |= PINMUX_INET_ACT_LED;
                }
            }
        }
        /* Enable LED controller to drive GPIO */
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_0) {
            GPIO->PinMuxSel |= PINMUX_EPHY0_ACT_LED;
            GPIO->GPIOMode |= (1 << EPHY0_SPD_LED);
            LED->ledHWDis &= ~(1 << EPHY0_SPD_LED);
        }
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_1) {
            GPIO->PinMuxSel |= PINMUX_EPHY1_ACT_LED;
            GPIO->GPIOMode |= (1 << EPHY1_SPD_LED);
            LED->ledHWDis &= ~(1 << EPHY1_SPD_LED);
        }
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_2) {
            GPIO->PinMuxSel |= PINMUX_EPHY2_ACT_LED;
            GPIO->GPIOMode |= (1 << EPHY2_SPD_LED);
            LED->ledHWDis &= ~(1 << EPHY2_SPD_LED);
        }
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_3) {
            GPIO->PinMuxSel |= PINMUX_EPHY3_ACT_LED;
            GPIO->GPIOMode |= (1 << EPHY3_SPD_LED);
            LED->ledHWDis &= ~(1 << EPHY3_SPD_LED);
        }
        if (GPIOOverlays & BP_OVERLAY_SPI_EXT_CS) { 
            GPIO->PinMuxSelOther &= ~PINMUX_SEL_SPI2_MASK;
            GPIO->PinMuxSelOther |= PINMUX_SEL_SPI2;                            
        }

        /* Enable PCIe CLKREQ signal */
        if (GPIOOverlays & BP_OVERLAY_PCIE_CLKREQ) {
            GPIO->PinMuxSel |= PINMUX_PCIE_CLKREQ;
        }
    }

#if defined(CONFIG_USB)
    PERF->blkEnables |= USBH_CLK_EN;
    mdelay(100);
    USBH->SwapControl = EHCI_ENDIAN_SWAP | OHCI_ENDIAN_SWAP;
    USBH->Setup |= USBH_IOC;
#else
    MISC->miscIddqCtrl |= MISC_IDDQ_CTRL_USBH;
    PERF->blkEnables &= ~USBH_CLK_EN;
#endif

#if !(defined(CONFIG_BCM_ADSL) || defined(CONFIG_BCM_ADSL_MODULE))
    MISC->miscIddqCtrl |= MISC_IDDQ_CTRL_SAR | MISC_IDDQ_CTRL_ADSL2_AFE | MISC_IDDQ_CTRL_ADSL2_PHY | MISC_IDDQ_CTRL_ADSL2_MIPS;
    PERF->blkEnables &= ~(SAR_CLK_EN | ADSL_CLK_EN | ADSL_AFE_EN | ADSL_QPROC_EN | PHYMIPS_CLK_EN);
    MISC->miscPllCtrlSysPll2 |= (1<<6); // Channel 5
    MISC->miscPllCtrlDdrPll |= 1; // Channel 5
#endif

#if defined(CONFIG_BCM_KF_POWER_SAVE) && defined(CONFIG_BCM_DDR_SELF_REFRESH_PWRSAVE)
    /* Enable power savings from DDR pads on this chip when DDR goes in Self-Refresh mode */
    DDR->PhyControl.IDLE_PAD_CONTROL = 0x00000172;
    DDR->PhyByteLane0Control.IDLE_PAD_CONTROL = 0x000fffff;
    DDR->PhyByteLane1Control.IDLE_PAD_CONTROL = 0x000fffff;
#endif
    return 0;
}

#define bcm63xx_specific_hw_init() bcm6328_hw_init()

#elif defined(CONFIG_BCM963268)

int map_63268_vdsl_override(int val) {
    switch (val & ~BP_ACTIVE_MASK) {
        case (BP_GPIO_10_AH & BP_GPIO_NUM_MASK):
        case (BP_GPIO_11_AH & BP_GPIO_NUM_MASK):
	    return(GPIO_BASE_VDSL_PHY_OVERRIDE_0);
        case (BP_GPIO_12_AH & BP_GPIO_NUM_MASK):
        case (BP_GPIO_13_AH & BP_GPIO_NUM_MASK):
	    return(GPIO_BASE_VDSL_PHY_OVERRIDE_1);
        case (BP_GPIO_24_AH & BP_GPIO_NUM_MASK):
        case (BP_GPIO_25_AH & BP_GPIO_NUM_MASK):
	    return(GPIO_BASE_VDSL_PHY_OVERRIDE_2);
        case (BP_GPIO_26_AH & BP_GPIO_NUM_MASK):
        case (BP_GPIO_27_AH & BP_GPIO_NUM_MASK):
	    return(GPIO_BASE_VDSL_PHY_OVERRIDE_3);
        default:
            return(0);
    }
}

int map_63268_misc_misc_override(int val) {
    switch (val & ~BP_ACTIVE_MASK) {
        case (BP_GPIO_8_AH & BP_GPIO_NUM_MASK):
	    return(MISC_MISC_DSL_GPIO_8_OVERRIDE);
        case (BP_GPIO_9_AH & BP_GPIO_NUM_MASK):
	    return(MISC_MISC_DSL_GPIO_9_OVERRIDE);
        default:
            return(0);
    }
}

static int __init bcm63268_hw_init(void)
{
    unsigned long GPIOOverlays, DeviceOptions = 0;
    unsigned short gpio;
    const ETHERNET_MAC_INFO *EnetInfo;
    unsigned char vreg1p8;
#if defined(CONFIG_BCM_1V2REG_AUTO_SHUTDOWN)
    uint32 startCount, endCount;
    int diff; 
#endif
    
    /* Turn off test bus */
    PERF->blkEnables &= ~TBUS_CLK_EN;


#if !(defined(CONFIG_BCM_XTMRT) || defined(CONFIG_BCM_XTMRT_MODULE))
    // Disable SAR if unused
    PERF->blkEnables &= ~( SAR_CLK_EN );
    MISC->miscIddqCtrl |= MISC_IDDQ_CTRL_SAR;
#endif

#if defined(CONFIG_BCM_XTMRT) || defined(CONFIG_BCM_XTMRT_MODULE)
    // Phy should always be powered down if XTM is deselected
    if (kerSysGetDslPhyEnable()) {
#else
    if (0) {
#endif
        MISC->miscIddqCtrl &= ~(MISC_IDDQ_CTRL_VDSL_PHY 
				| MISC_IDDQ_CTRL_VDSL_MIPS
				| MISC_IDDQ_CTRL_SAR);
    } 
    else 
    {
        /* If there is no phy support, shut off power */
        PERF->blkEnables &= ~( PHYMIPS_CLK_EN
				| VDSL_CLK_EN 
				| VDSL_AFE_EN | VDSL_QPROC_EN );
        MISC->miscIddqCtrl |= (MISC_IDDQ_CTRL_VDSL_PHY 
				| MISC_IDDQ_CTRL_VDSL_MIPS);
    }

    if( BpGetDeviceOptions(&DeviceOptions) == BP_SUCCESS ) {
        if(DeviceOptions&BP_DEVICE_OPTION_DISABLE_LED_INVERSION)
            MISC->miscLed_inv = 0;
    }

    /* Set LED blink rate for activity LEDs to 80mS */
    LED->ledInit &= ~LED_FAST_INTV_MASK;
    LED->ledInit |= (LED_INTERVAL_20MS * 4) << LED_FAST_INTV_SHIFT;

    /* Start with all HW LEDs disabled */
    LED->ledHWDis |= 0xFFFFFF;


    EnetInfo = BpGetEthernetMacInfoArrayPtr();

    /* Enable HW to drive LEDs for Ethernet ports in use */
    if (EnetInfo[0].sw.port_map & (1 << 0)) {
        LED->ledHWDis &= ~(1 << LED_EPHY0_ACT);
        LED->ledHWDis &= ~(1 << LED_EPHY0_SPD);
    }
    if (EnetInfo[0].sw.port_map & (1 << 1)) {
        LED->ledHWDis &= ~(1 << LED_EPHY1_ACT);
        LED->ledHWDis &= ~(1 << LED_EPHY1_SPD);
    }
    if (EnetInfo[0].sw.port_map & (1 << 2)) {
        LED->ledHWDis &= ~(1 << LED_EPHY2_ACT);
        LED->ledHWDis &= ~(1 << LED_EPHY2_SPD);
    }
    if (EnetInfo[0].sw.port_map & (1 << 3)) {
        LED->ledHWDis &= ~(1 << LED_GPHY0_ACT);
/*In Actiontec 63268 Chip, GPIO 0/1 is used for Internet LED , not for GPHY speed LED*/
#ifndef AEI_63168_CHIP
        LED->ledHWDis &= ~(1 << LED_GPHY0_SPD0);
        LED->ledHWDis &= ~(1 << LED_GPHY0_SPD1);
#endif
        LED->ledLinkActSelLow |= ((1 << LED_GPHY0_SPD0) << LED_0_LINK_SHIFT);
        LED->ledLinkActSelLow |= ((1 << LED_GPHY0_SPD1) << LED_1_LINK_SHIFT);
        GPIO->RoboSWLEDControl |= LED_BICOLOR_SPD;
    }

    /* UART2 - SDIN and SDOUT are separate for flexibility */
    {
        unsigned short Uart2Sdin;
        unsigned short Uart2Sdout;
        if (BpGetUart2SdinGpio(&Uart2Sdin) == BP_SUCCESS) {
            switch (Uart2Sdin & BP_GPIO_NUM_MASK) {
            case (BP_GPIO_12_AH & BP_GPIO_NUM_MASK):
                GPIO->GPIOMode |= (GPIO_MODE_UART2_SDIN);
                break;
            case (BP_GPIO_26_AH & BP_GPIO_NUM_MASK):
                GPIO->GPIOMode |= (GPIO_MODE_UART2_SDIN2);
                break;
            }
        }
        if (BpGetUart2SdoutGpio(&Uart2Sdout) == BP_SUCCESS) {
            switch (Uart2Sdout & BP_GPIO_NUM_MASK) {
            case (BP_GPIO_13_AH & BP_GPIO_NUM_MASK):
                GPIO->GPIOMode |= (GPIO_MODE_UART2_SDOUT);
                break;
            case (BP_GPIO_27_AH & BP_GPIO_NUM_MASK):
                GPIO->GPIOMode |= (GPIO_MODE_UART2_SDOUT2);
                break;
            }
        }
    }


    if( BpGetGPIOverlays(&GPIOOverlays) == BP_SUCCESS ) {
        if (GPIOOverlays & BP_OVERLAY_SERIAL_LEDS) {
            GPIO->GPIOMode |= (GPIO_MODE_SERIAL_LED_CLK | GPIO_MODE_SERIAL_LED_DATA);
            LED->ledInit |= LED_SERIAL_LED_EN;
        }
        if ( BpGetWanDataLedGpio(&gpio) == BP_SUCCESS ) {
            if ((gpio & BP_GPIO_NUM_MASK) == LED_INET_ACT) {
                /* WAN Data LED must be LED 8 */
                if (!(gpio & BP_GPIO_SERIAL)) {
                    /* If LED is not serial, enable corresponding GPIO */
                    GPIO->LEDCtrl |= GPIO_NUM_TO_MASK(gpio);
                }
            }
        }
        /* Enable LED controller to drive GPIO when LEDs are connected to GPIO pins */
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_0) {
            GPIO->LEDCtrl |= (1 << LED_EPHY0_ACT);
            GPIO->LEDCtrl |= (1 << LED_EPHY0_SPD);
        }
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_1) {
            GPIO->LEDCtrl |= (1 << LED_EPHY1_ACT);
            GPIO->LEDCtrl |= (1 << LED_EPHY1_SPD);
        }
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_2) {
            GPIO->LEDCtrl |= (1 << LED_EPHY2_ACT);
            GPIO->LEDCtrl |= (1 << LED_EPHY2_SPD);
        }
        if (GPIOOverlays & BP_OVERLAY_GPHY_LED_0) {
            GPIO->LEDCtrl |= (1 << LED_GPHY0_ACT);
            GPIO->LEDCtrl |= (1 << LED_GPHY0_SPD0);
            GPIO->LEDCtrl |= (1 << LED_GPHY0_SPD1);
        }
        if (GPIOOverlays & BP_OVERLAY_PHY) {
            unsigned short IntLdMode = 0xffff;
            unsigned short IntLdPwr = 0xffff;
            unsigned short ExtLdMode = 0xffff;
            unsigned short ExtLdPwr = 0xffff;
            unsigned short ExtLdClk = 0xffff;
            unsigned short ExtLdData = 0xffff;
            unsigned long ul;
            int ExplicitLdControl ;
            ExplicitLdControl = (BpGetIntAFELDModeGpio(&IntLdMode) == BP_SUCCESS) ? 1 : 0;
            ExplicitLdControl = ExplicitLdControl + ((BpGetIntAFELDPwrGpio(&IntLdPwr) == BP_SUCCESS) ? 1 : 0);
            ExplicitLdControl = ExplicitLdControl + ((BpGetExtAFELDModeGpio(&ExtLdMode) == BP_SUCCESS) ? 1 : 0);
            ExplicitLdControl = ExplicitLdControl + ((BpGetExtAFELDPwrGpio(&ExtLdPwr) == BP_SUCCESS) ? 1 : 0);
            ExplicitLdControl = ExplicitLdControl + ((BpGetExtAFELDClkGpio(&ExtLdClk) == BP_SUCCESS) ? 1 : 0);
            ExplicitLdControl = ExplicitLdControl + ((BpGetExtAFELDDataGpio(&ExtLdData) == BP_SUCCESS) ? 1 : 0);
            if (ExplicitLdControl == 0) {
                /* default if boardparms doesn't specify a subset */
                GPIO->GPIOBaseMode |= GPIO_BASE_VDSL_PHY_OVERRIDE_0  | GPIO_BASE_VDSL_PHY_OVERRIDE_1;
            } else {
                GPIO->GPIOBaseMode |= map_63268_vdsl_override(IntLdMode) 
                    |  map_63268_vdsl_override(IntLdPwr) 
                    |  map_63268_vdsl_override(ExtLdMode)
                    |  map_63268_vdsl_override(ExtLdPwr)
                    |  map_63268_vdsl_override(ExtLdClk)
                    |  map_63268_vdsl_override(ExtLdData) ;
                ul = map_63268_misc_misc_override(IntLdMode) 
                    |  map_63268_misc_misc_override(IntLdPwr) 
                    |  map_63268_misc_misc_override(ExtLdMode)
                    |  map_63268_misc_misc_override(ExtLdPwr)
                    |  map_63268_misc_misc_override(ExtLdClk)
                    |  map_63268_misc_misc_override(ExtLdData) ;
		if (ul != 0) {
			MISC->miscMisc_ctrl |= ul;
  		}
            } 
        }

        /* Enable PCIe CLKREQ signal */
        if (GPIOOverlays & BP_OVERLAY_PCIE_CLKREQ) {
            GPIO->GPIOMode |= GPIO_MODE_PCIE_CLKREQ_B;
        }

        if (GPIOOverlays & BP_OVERLAY_USB_LED) {
            LED->ledHWDis &= ~(1 << LED_USB_ACT);
        }
        /* Enable HS SPI SS Pins */
        if (GPIOOverlays & BP_OVERLAY_HS_SPI_SSB4_EXT_CS) {
             GPIO->GPIOMode |= GPIO_MODE_HS_SPI_SS_4;
        }
        if (GPIOOverlays & BP_OVERLAY_HS_SPI_SSB5_EXT_CS) {
             GPIO->GPIOMode |= GPIO_MODE_HS_SPI_SS_5;
        }
        if (GPIOOverlays & BP_OVERLAY_HS_SPI_SSB6_EXT_CS) {
             GPIO->GPIOMode |= GPIO_MODE_HS_SPI_SS_6;
        }
        if (GPIOOverlays & BP_OVERLAY_HS_SPI_SSB7_EXT_CS) {
             GPIO->GPIOMode |= GPIO_MODE_HS_SPI_SS_7;
        }
    }

    {
        unsigned short PhyBaseAddr;
        /* clear the base address first. hw does not clear upon soft reset*/
        GPIO->RoboswEphyCtrl &= ~EPHY_PHYAD_BASE_ADDR_MASK;
        if( BpGetEphyBaseAddress(&PhyBaseAddr) == BP_SUCCESS ) {
            GPIO->RoboswEphyCtrl |= ((PhyBaseAddr >>3) & 0x3) << EPHY_PHYAD_BASE_ADDR_SHIFT;
        }

        /* clear the base address first. hw does not clear upon soft reset*/
        GPIO->RoboswGphyCtrl &= ~GPHY_PHYAD_BASE_ADDR_MASK;
        if( BpGetGphyBaseAddress(&PhyBaseAddr) == BP_SUCCESS ) {
            GPIO->RoboswGphyCtrl |= ((PhyBaseAddr >>3) & 0x3) << GPHY_PHYAD_BASE_ADDR_SHIFT;
        }
    }


#if defined(CONFIG_USB)
    PERF->blkEnables |= USBH_CLK_EN;
    PERF->softResetB |= SOFT_RST_USBH;
    TIMER->ClkRstCtl |= USB_REF_CLKEN;
    MISC->miscIddqCtrl &= ~MISC_IDDQ_CTRL_USBH;
    mdelay(100);
    USBH->SwapControl = EHCI_ENDIAN_SWAP | OHCI_ENDIAN_SWAP;
    USBH->Setup |= USBH_IOC;
    USBH->Setup &= ~USBH_IPP;
    USBH->PllControl1 &= ~(PLLC_PLL_IDDQ_PWRDN | PLLC_PLL_PWRDN_DELAY);
#else
    MISC->miscIddqCtrl |= MISC_IDDQ_CTRL_USBH;
    PERF->blkEnables &= ~USBH_CLK_EN;
#endif

#if defined(CONFIG_BCM_KF_FAP) && (defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE))
#else
    PERF->blkEnables &= ~FAP0_CLK_EN;
    PERF->blkEnables &= ~FAP1_CLK_EN;
#endif

#if defined(CONFIG_PCI)
    /* Enable WOC */  
    PERF->blkEnables |=WLAN_OCP_CLK_EN;
    mdelay(10);
    PERF->softResetB &= ~(SOFT_RST_WLAN_SHIM_UBUS | SOFT_RST_WLAN_SHIM);
    mdelay(1);
    PERF->softResetB |= (SOFT_RST_WLAN_SHIM_UBUS | SOFT_RST_WLAN_SHIM);
    mdelay(1);
 
    WLAN_SHIM->ShimMisc = (WLAN_SHIM_FORCE_CLOCKS_ON|WLAN_SHIM_MACRO_SOFT_RESET);
    mdelay(1);
    WLAN_SHIM->MacControl = (SICF_FGC|SICF_CLOCK_EN);
    WLAN_SHIM->ShimMisc = WLAN_SHIM_FORCE_CLOCKS_ON;
    WLAN_SHIM->ShimMisc = 0;
    WLAN_SHIM->MacControl = SICF_CLOCK_EN;
#endif    

#if defined(CONFIG_BCM_KF_POWER_SAVE) && defined(CONFIG_BCM_DDR_SELF_REFRESH_PWRSAVE)
    /* Enable power savings from DDR pads on this chip when DDR goes in Self-Refresh mode */
    MEMC->PhyControl.IDLE_PAD_CONTROL = 0x00000172;
    MEMC->PhyByteLane0Control.IDLE_PAD_CONTROL = 0x000fffff;
    MEMC->PhyByteLane1Control.IDLE_PAD_CONTROL = 0x000fffff;
#endif

#if defined(CONFIG_BCM_1V2REG_AUTO_SHUTDOWN)
    /*
     * Determine if internal VREG is used.
     * If not, disable it to improve WLAN performance at 5GHz
     * The ring oscillators are affected when varying the 1V2 voltage
     * So take a measure of the ring osc count, then raise the internal regulator voltage and remeasure
     * If the ring osc count changed as expected than internal regulators are used
     */
    printk("Internal 1P2 VREG will be shutdown if unused...");

    /* Take the first ring osc measurement */
    GPIO->RingOscCtrl1 = RING_OSC_ENABLE_MASK | RING_OSC_COUNT_RESET | RING_OSC_IRQ;
    GPIO->RingOscCtrl1 = RING_OSC_ENABLE_MASK | (2 << RING_OSC_SELECT_SHIFT);
    GPIO->RingOscCtrl0 = RING_OSC_512_CYCLES;
    while (!(GPIO->RingOscCtrl1 & RING_OSC_IRQ));
    startCount = GPIO->RingOscCtrl1 & RING_OSC_COUNT_MASK;

    /* Increase internal 1V2 slightly and see if the ring osc is speeding up */
    MISC->miscVregCtrl1 += 8;
    MISC->miscVregCtrl0 |= MISC_VREG_CONTROL0_REG_RESET_B;

    /* Take the second ring osc measurement */
    GPIO->RingOscCtrl1 = RING_OSC_ENABLE_MASK | RING_OSC_COUNT_RESET | RING_OSC_IRQ;
    GPIO->RingOscCtrl1 = RING_OSC_ENABLE_MASK | (2 << RING_OSC_SELECT_SHIFT);
    GPIO->RingOscCtrl0 = RING_OSC_512_CYCLES;
    while (!(GPIO->RingOscCtrl1 & RING_OSC_IRQ));
    endCount = GPIO->RingOscCtrl1 & RING_OSC_COUNT_MASK;

    /* Reset the internal 1V2 to its original value */
    MISC->miscVregCtrl1 -= 8;

    /*
     * A negative difference or a small positive difference indicates that an external regulator is used
     * This code was calibrated by repeating the measurements thousands of times and looking for a safe value
     * Safe means avoiding at all costs being wrong by shutting down the internal regulator when it is in use
     * It is better to be wrong by leaving the internal regulator running when an external regulator is used
     */
    diff = startCount - endCount;
    if (diff < 300) {
        printk("Unused, turn it off (%08lx-%08lx=%d<300)\n", startCount, endCount,diff);
        /* Turn off internal 1P2 regulator */
        MISC->miscVregCtrl0 |= MISC_VREG_CONTROL0_REG_RESET_B | MISC_VREG_CONTROL0_POWER_DOWN_1;
    } else {
        printk("Used, leave it on (%08lx-%08lx=%d>=300)\n", startCount, endCount, diff);
    }
#elif defined(CONFIG_BCM_1V2REG_ALWAYS_SHUTDOWN)
    printk("Internal 1P2 VREG is forced to be shutdown\n");
    MISC->miscVregCtrl0 |= MISC_VREG_CONTROL0_REG_RESET_B | MISC_VREG_CONTROL0_POWER_DOWN_1;
#elif defined(CONFIG_BCM_1V2REG_NEVER_SHUTDOWN)
    printk("Internal 1P2 VREG is forced to remain enabled\n");
#endif

    if ( BpGetVreg1P8(&vreg1p8) == BP_SUCCESS ) {
        if (vreg1p8 == BP_VREG_EXTERNAL) {
            printk("Internal 1P8 VREG is forced by boardparms to be shutdown\n");
            MISC->miscVregCtrl0 |= MISC_VREG_CONTROL0_REG_RESET_B | MISC_VREG_CONTROL0_POWER_DOWN_2;
        }
    }	

    if ( BpGetFemtoResetGpio(&gpio) == BP_SUCCESS ) {
        kerSysSetGpioState(gpio, kGpioActive);
    }
    return 0;
}

#define bcm63xx_specific_hw_init() bcm63268_hw_init()

#elif defined(CONFIG_BCM96318)

static int __init bcm6318_hw_init(void)
{
    const ETHERNET_MAC_INFO *EnetInfo;
    unsigned long GPIOOverlays;
    unsigned short gpio;
    unsigned short SerialMuxSel;
    unsigned int chipRev = UtilGetChipRev();

    /* Set LED blink rate for activity LEDs to 80mS */
    LED->ledInit &= ~LED_FAST_INTV_MASK;
    LED->ledInit |= (LED_INTERVAL_20MS * 4) << LED_FAST_INTV_SHIFT;


    EnetInfo = BpGetEthernetMacInfoArrayPtr();


#if defined(CONFIG_USB)
    PERF->blkEnables |= USBH_CLK_EN;
    mdelay(100);
    USBH->PllControl1 |= PLLC_PLL_SUSPEND_EN;  
    USBH->SwapControl = EHCI_ENDIAN_SWAP | OHCI_ENDIAN_SWAP;
    USBH->Setup |= USBH_IOC;
    USBH->Setup &= ~USBH_IPP;
    USBH->USBSimControl |= USBH_LADDR_SEL; /*choose A1 implemenatation mode for Last ADDR calculation*/
    USBH->PllControl1 &= ~(PLLC_PLL_IDDQ_PWRDN);
    GPIO->PinMuxSel0 &= ~(PINMUX_SEL_GPIO13_MASK << PINMUX_SEL_GPIO13_SHIFT);
    GPIO->PinMuxSel0 |= (PINMUX_SEL_USB_PWRON << PINMUX_SEL_GPIO13_SHIFT);  
#endif

#if !defined(CONFIG_BCM_USB) && !defined(CONFIG_USB)
    PERF->blkEnables &= ~USBH_CLK_EN;
    PERF->blkEnablesUbus &= ~USB_UBUS_CLK_EN;
    PERF->softResetB &= ~(SOFT_RST_USBH | SOFT_RST_USBD);
    PLL_PWR->PllPwrControlIddqCtrl |= IDDQ_USB;
    PLL_PWR->PllPwrControlPsmVddCtrl |= PSM_VDD_USBH | PSM_VDD_USBD;
#endif

#if !defined(CONFIG_BCM_ADSL) && !defined(CONFIG_BCM_ADSL_MODULE)
    PLL_PWR->PllPwrControlPsmVddCtrl |= PSM_VDD_ADSL | PSM_VDD_PHY_MIPS | PSM_VDD_SAR | PSM_VDD_PHY_MIPS_CACHE;
    PLL_PWR->PllPwrControlIddqCtrl |= IDDQ_LDO2P9;
    PERF->blkEnables &= ~(SAR_CLK_EN | ADSL_CLK_EN | ADSL_AFE_EN | ADSL_QPROC_EN | PHYMIPS_CLK_EN);
    PERF->blkEnablesUbus &= ~(ADSL_UBUS_CLK_EN | PHYMIPS_UBUS_CLK_EN | SAR_UBUS_CLK_EN);
    PERF->softResetB &= ~(SOFT_RST_SAR | SOFT_RST_ADSL | SOFT_RST_PHYMIPS);
#endif

    /* set any in use led/gpio pin mux to gpio function */
    initGpioPinMux();

    /* Start with all HW LEDs disabled */
    LED->ledHWDis |= 0xFFFFFF;
    LED->ledMode = 0;

    /* Enable HW to drive LEDs for Ethernet ports in use */
    if (EnetInfo[0].sw.port_map & (1 << 0)) {
        LED->ledHWDis &= ~(1 << EPHY0_SPD_LED);
        LED->ledHWDis &= ~(1 << EPHY0_ACT_LED);
        /* set up link and speed mapping */
        LED->ledLinkActSelLow |= ((1<<(EPHY0_ACT_LED-4))<<LED_0_LINK_SHIFT);
        LED->ledLinkActSelHigh |= ((1<<(EPHY0_ACT_LED-4))<<LED_0_LINK_SHIFT);
        /* workaround for hw which invert the active low to active high */
        if(chipRev == 0xa0)
        {
            LED->ledXorReg |= (1 << EPHY0_SPD_LED);
            LED->ledXorReg |= (1 << EPHY0_ACT_LED);
        }
    }
    if (EnetInfo[0].sw.port_map & (1 << 1)) {
        LED->ledHWDis &= ~(1 << EPHY1_SPD_LED);
        LED->ledHWDis &= ~(1 << EPHY1_ACT_LED);
        LED->ledLinkActSelLow |= ((1<<(EPHY1_ACT_LED-4))<<LED_1_LINK_SHIFT);
        LED->ledLinkActSelHigh |= ((1<<(EPHY1_ACT_LED-4))<<LED_1_LINK_SHIFT);
        if(chipRev == 0xa0)
        {
            LED->ledXorReg |= (1 << EPHY1_SPD_LED);
            LED->ledXorReg |= (1 << EPHY1_ACT_LED);
        }
    }
    if (EnetInfo[0].sw.port_map & (1 << 2)) {
        LED->ledHWDis &= ~(1 << EPHY2_SPD_LED);
        LED->ledHWDis &= ~(1 << EPHY2_ACT_LED);
        LED->ledLinkActSelLow |= ((1<<(EPHY2_ACT_LED-4))<<LED_2_LINK_SHIFT);
        LED->ledLinkActSelHigh |= ((1<<(EPHY2_ACT_LED-4))<<LED_2_LINK_SHIFT);
        if(chipRev == 0xa0)
        {
            LED->ledXorReg |= (1 << EPHY2_SPD_LED);
            LED->ledXorReg |= (1 << EPHY2_ACT_LED);
        }
    }
    if (EnetInfo[0].sw.port_map & (1 << 3)) {
        LED->ledHWDis &= ~(1 << EPHY3_SPD_LED);
        LED->ledHWDis &= ~(1 << EPHY3_ACT_LED);
        LED->ledLinkActSelLow |= ((1<<(EPHY3_ACT_LED-4))<<LED_3_LINK_SHIFT);
        LED->ledLinkActSelHigh |= ((1<<(EPHY3_ACT_LED-4))<<LED_3_LINK_SHIFT);
        if(chipRev == 0xa0)
        {
            LED->ledXorReg |= (1 << EPHY3_SPD_LED);
            LED->ledXorReg |= (1 << EPHY3_ACT_LED);
        }
    }

    if( BpGetGPIOverlays(&GPIOOverlays) == BP_SUCCESS ) 
    {      
        if (GPIOOverlays & BP_OVERLAY_SERIAL_LEDS) {
            GPIO->GPIOMode |= (1 << SERIAL_LED_DATA);
            GPIO->GPIOMode |= (1 << SERIAL_LED_CLK);
            /* enable shift only on led status change mode to solve the dim led issue. only available in b0 or newer chip */
            if(chipRev != 0xa0)
            {
            	LED->ledInit &= ~LED_SERIAL_SHIFT_MODE_MASK;
            	LED->ledInit |= LED_SERIAL_SHIFT_MODE_CHANGE;
            }

            LED->ledSerialMuxSelect = 0x0;
            if( BpGetSerialLEDMuxSel(&SerialMuxSel) == BP_SUCCESS )
            {
                if( SerialMuxSel == (BP_SERIAL_MUX_SEL_GROUP0|BP_SERIAL_MUX_SEL_GROUP2) )
                     LED->ledSerialMuxSelect = 0xff;
        	    /* otherwise either non supported combination or default 8 to 23 LED*/
            }


            /* For default Serial MUX selection, XOR workaround is not needed for EPHY 3 SPD and ACT
        	 * as EPHY 3 LED function is not available anyway. Otherwise, serial data/clk will be inverted too.
        	 * But for non default Serial MUX selection, we need it to make all EPHY LINK/SPD LED work.
        	 * However LED 16 to 23 are inverted too. Will fix in next hw revision */
            if(chipRev == 0xa0)
            {
                if( LED->ledSerialMuxSelect == 0x0 )
                {
                    LED->ledXorReg &= ~(1 << SERIAL_LED_DATA);
                    LED->ledXorReg &= ~(1 << SERIAL_LED_CLK);
                    LED->ledInit |= (LED_SERIAL_LED_EN|LED_SERIAL_LED_MUX_SEL);
        	    }
                else
                    LED->ledInit |= (LED_SERIAL_LED_EN|LED_SERIAL_LED_MUX_SEL|LED_SERIAL_LED_CLK_NPOL);
            }
            else
            	LED->ledInit |= (LED_SERIAL_LED_EN|LED_SERIAL_LED_MUX_SEL);
        }

        /* Enable LED controller to drive GPIO */
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_0) {
            GPIO->GPIOMode |= (1 << EPHY0_SPD_LED);
            GPIO->GPIOMode |= (1 << EPHY0_ACT_LED);
        }
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_1) {
            GPIO->GPIOMode |= (1 << EPHY1_SPD_LED);
            GPIO->GPIOMode |= (1 << EPHY1_ACT_LED);
		}
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_2) {
            GPIO->GPIOMode |= (1 << EPHY2_SPD_LED);
            GPIO->GPIOMode |= (1 << EPHY2_ACT_LED);
        }
        if (GPIOOverlays & BP_OVERLAY_EPHY_LED_3) {
            GPIO->GPIOMode |= (1 << EPHY3_SPD_LED);
            GPIO->GPIOMode |= (1 << EPHY3_ACT_LED);
        }

        if (GPIOOverlays & BP_OVERLAY_USB_DEVICE) {
            LED->ledHWDis &= ~(1 << USB_ACT_LED);
        }
        if (GPIOOverlays & BP_OVERLAY_USB_LED) {
            GPIO->GPIOMode |= (1 << USB_ACT_LED);
        }

        if ( BpGetWanDataLedGpio(&gpio) == BP_SUCCESS ) {
            if ((gpio & BP_GPIO_NUM_MASK) == INET_ACT_LED) {
            	/* WAN Data LED must be LED 8 */
                if (!(gpio & BP_GPIO_SERIAL)) {
                    /* If LED is not serial, enable corresponding GPIO */
                    GPIO->GPIOMode |= GPIO_NUM_TO_MASK(gpio);
                }
            }
        }

#if defined(CONFIG_PCI) && defined(PCIEH)
        /* enable PCIE_REQ if boardparam says so */
        if (GPIOOverlays & BP_OVERLAY_PCIE_CLKREQ)
        {
            /* PCIE CLK req use GPIO 10 input so make sure GPIO 10 is not used when this flag is set.*/
            if( !BpIsGpioInUse(BP_GPIO_10_AL&BP_GPIO_NUM_MASK) )
            {
            	kerSysSetGpioDirInput(BP_GPIO_10_AL&BP_GPIO_NUM_MASK);
            	MISC->miscPeriph_ECO_Register |= MISC_PCIE_CLKREQ_EN;
            	PCIEH_MISC_HARD_REGS->hard_pcie_hard_debug |= PCIE_MISC_HARD_PCIE_HARD_DEBUG_CLKREQ_DEBUG_ENABLE;
            }
        }
#endif
    }
    
    return 0;
}

#define bcm63xx_specific_hw_init() bcm6318_hw_init()

#elif defined(CONFIG_BCM960333)

static int __init bcm60333_hw_init(void)
{
    /* We can add a minimum GPIO MUX setup here to enable UART TxRx*/
    return 0;
}

#define bcm63xx_specific_hw_init() bcm60333_hw_init()

#elif defined(CONFIG_BCM96838)

static int __init bcm6838_hw_init(void)
{
    unsigned short irq, gpio;
	
    if( BpGetResetToDefaultExtIntr(&irq) == BP_SUCCESS )
    {
        if(BpGetResetToDefaultExtIntrGpio(&gpio) == BP_SUCCESS)
        {
            int gpio_polarity = gpio & BP_ACTIVE_MASK;
            gpio &= BP_GPIO_NUM_MASK;
            PERF->ext_irq_muxsel0 |= ( (gpio&EXT_IRQ_MASK_LOW) << (irq*EXT_IRQ_OFF_LOW) );
            DBGPERF->Dbg_extirqmuxsel0_1 |= ( ((gpio&EXT_IRQ_MASK_HIGH)>>EXT_IRQ_OFF_LOW) << (irq*EXT_IRQ_OFF_HIGH) );
            if (gpio_polarity == BP_ACTIVE_HIGH)
                PERF->ExtIrqCfg |= (1<<irq);
        }
    }

    if( BpGetWirelessSesExtIntr(&irq) == BP_SUCCESS )
    {
        if(BpGetWirelessSesExtIntrGpio(&gpio) == BP_SUCCESS)
        {
            int gpio_polarity = gpio & BP_ACTIVE_MASK;
            gpio &= BP_GPIO_NUM_MASK;
            PERF->ext_irq_muxsel0 |= ( (gpio&EXT_IRQ_MASK_LOW) << (irq*EXT_IRQ_OFF_LOW) );
            DBGPERF->Dbg_extirqmuxsel0_1 |= ( ((gpio&EXT_IRQ_MASK_HIGH)>>EXT_IRQ_OFF_LOW) << (irq*EXT_IRQ_OFF_HIGH) );
            if (gpio_polarity == BP_ACTIVE_HIGH)
                PERF->ExtIrqCfg |= (1<<irq);
        }
    }

    if( BpGetPmdAlarmExtIntr(&irq) == BP_SUCCESS )
    {
        if(BpGetPmdAlarmExtIntrGpio(&gpio) == BP_SUCCESS)
        {
            int gpio_polarity = gpio & BP_ACTIVE_MASK;
            gpio &= BP_GPIO_NUM_MASK;
            PERF->ext_irq_muxsel0 |= ( (gpio&EXT_IRQ_MASK_LOW) << (irq*EXT_IRQ_OFF_LOW) );
            DBGPERF->Dbg_extirqmuxsel0_1 |= ( ((gpio&EXT_IRQ_MASK_HIGH)>>EXT_IRQ_OFF_LOW) << (irq*EXT_IRQ_OFF_HIGH) );
            if (gpio_polarity == BP_ACTIVE_HIGH)
			{
                PERF->ExtIrqCfg |= (1<<irq);
                PERF->ExtIrqCfg |= (1<<26);
            }
        }
    }

    if( BpGetPmdSDExtIntr(&irq) == BP_SUCCESS )
    {
        if(BpGetPmdSDExtIntrGpio(&gpio) == BP_SUCCESS)
        {
            int gpio_polarity = gpio & BP_ACTIVE_MASK;
            gpio &= BP_GPIO_NUM_MASK;
            PERF->ext_irq_muxsel0 |= ( (gpio&EXT_IRQ_MASK_LOW) << (irq*EXT_IRQ_OFF_LOW) );
            DBGPERF->Dbg_extirqmuxsel0_1 |= ( ((gpio&EXT_IRQ_MASK_HIGH)>>EXT_IRQ_OFF_LOW) << (irq*EXT_IRQ_OFF_HIGH) );
            if (gpio_polarity == BP_ACTIVE_HIGH)
            {
                PERF->ExtIrqCfg |= (1<<irq);
                /* PERF->ExtIrqCfg |= (1<<27); change interrupt to level */
            }
        }
    }

    if( BpGetTrplxrTxFailExtIntr(&irq) == BP_SUCCESS )
    {
        if(BpGetTrplxrTxFailExtIntrGpio(&gpio) == BP_SUCCESS)
        {
            int gpio_polarity = gpio & BP_ACTIVE_MASK;
            gpio &= BP_GPIO_NUM_MASK;
            PERF->ext_irq_muxsel0 |= ( (gpio&EXT_IRQ_MASK_LOW) << (irq*EXT_IRQ_OFF_LOW) );
            DBGPERF->Dbg_extirqmuxsel0_1 |= ( ((gpio&EXT_IRQ_MASK_HIGH)>>EXT_IRQ_OFF_LOW) << (irq*EXT_IRQ_OFF_HIGH) );
            if (gpio_polarity == BP_ACTIVE_HIGH)
			{
                PERF->ExtIrqCfg |= (1<<irq);
            }
        }
    }

    if( BpGetTrplxrSdExtIntr(&irq) == BP_SUCCESS )
    {
        if(BpGetTrplxrSdExtIntrGpio(&gpio) == BP_SUCCESS)
        {
            int gpio_polarity = gpio & BP_ACTIVE_MASK;
            gpio &= BP_GPIO_NUM_MASK;
            PERF->ext_irq_muxsel0 |= ( (gpio&EXT_IRQ_MASK_LOW) << (irq*EXT_IRQ_OFF_LOW) );
            DBGPERF->Dbg_extirqmuxsel0_1 |= ( ((gpio&EXT_IRQ_MASK_HIGH)>>EXT_IRQ_OFF_LOW) << (irq*EXT_IRQ_OFF_HIGH) );
            if (gpio_polarity == BP_ACTIVE_HIGH)
			{
                PERF->ExtIrqCfg |= (1<<irq);
            }
        }
    }

    if( BpGetWifiOnOffExtIntr(&irq) == BP_SUCCESS )
    {
        if(BpGetWifiOnOffExtIntrGpio(&gpio) == BP_SUCCESS)
        {
            int gpio_polarity = gpio & BP_ACTIVE_MASK;
            gpio &= BP_GPIO_NUM_MASK;
            PERF->ext_irq_muxsel0 |= ( (gpio&EXT_IRQ_MASK_LOW) << (irq*EXT_IRQ_OFF_LOW) );
            DBGPERF->Dbg_extirqmuxsel0_1 |= ( ((gpio&EXT_IRQ_MASK_HIGH)>>EXT_IRQ_OFF_LOW) << (irq*EXT_IRQ_OFF_HIGH) );
            if (gpio_polarity == BP_ACTIVE_HIGH)
			{
                PERF->ExtIrqCfg |= (1<<irq);
            }
        }
    }

    if( BpGetLteExtIntr(&irq) == BP_SUCCESS )
    {
        if(BpGetLteExtIntrGpio(&gpio) == BP_SUCCESS)
        {
            int gpio_polarity = gpio & BP_ACTIVE_MASK;
            gpio &= BP_GPIO_NUM_MASK;
            PERF->ext_irq_muxsel0 |= ( (gpio&EXT_IRQ_MASK_LOW) << (irq*EXT_IRQ_OFF_LOW) );
            DBGPERF->Dbg_extirqmuxsel0_1 |= ( ((gpio&EXT_IRQ_MASK_HIGH)>>EXT_IRQ_OFF_LOW) << (irq*EXT_IRQ_OFF_HIGH) );
            if (gpio_polarity == BP_ACTIVE_HIGH)
			{
                PERF->ExtIrqCfg |= (1<<irq);
            }
        }
    }


    if (BpGetUart2SdinGpio(&gpio) == BP_SUCCESS)
    {
        gpio &= BP_GPIO_NUM_MASK;
        set_pinmux(gpio, 1);
    }
    if (BpGetUart2SdoutGpio(&gpio) == BP_SUCCESS)
    {
        gpio &= BP_GPIO_NUM_MASK;
        set_pinmux(gpio, 1);
    }

#if defined(CONFIG_USB)
    if(kerSysGetUsbHostPortEnable(0) || kerSysGetUsbHostPortEnable(1))
    {
        /* enable power to USB ports */
        GPIO->port_block_data1 = 0x0;
        if(kerSysGetUsbHostPortEnable(0))
        {
            GPIO->port_block_data2 = 0x1045; /*USB0_PWRFLT */
            GPIO->port_command = 0x21;
            GPIO->port_block_data2 = 0x1046; /*USB0_PWRON */
            GPIO->port_command = 0x21;
        }
        if(kerSysGetUsbHostPortEnable(1))
        {
            GPIO->port_block_data2 = 0x5047; /*USB1_PWRFLT */
            GPIO->port_command = 0x21;
            GPIO->port_block_data2 = 0x5048; /*USB1_PWRON */
            GPIO->port_command = 0x21;
        }
        mdelay(100);
        USBH->SwapControl = EHCI_ENDIAN_SWAP | OHCI_ENDIAN_SWAP;
        USBH->Setup |= USB_IOC;
        USBH->Setup &= ~USB_IPP;
        USBH->PllControl1 &= ~(PLL_IDDQ_PWRDN);
    }
    else
    { /* no USB HOST */
        /*
         * Power to USB Host controller is on by default,
         * shutdown power to USB Host controller
         */
        kerSysSetUsbPower(0, USB_HOST_FUNC);
    }

    if(!kerSysGetUsbDeviceEnable())
    {
        /* USB device not supported shutdown power to USB device */
        kerSysSetUsbPower(0, USB_DEVICE_FUNC);
    }
#endif
	return 0;
}

#define bcm63xx_specific_hw_init() bcm6838_hw_init()

#elif defined(CONFIG_BCM963381)

extern void bcm_set_pinmux(unsigned int pin_num, unsigned int mux_num);

#if defined(CONFIG_USB)

#define CAP_TYPE_EHCI       0x00 
#define CAP_TYPE_OHCI       0x01 
#define CAP_TYPE_XHCI       0x02 

static struct platform_device *xhci_dev;

static void bcm63381_manual_usb_ldo_start(void)
{
    USBH_CTRL->pll_ctl &= ~(1 << 30); /*pll_resetb=0*/
    USBH_CTRL->utmi_ctl_1 = 0; 
    USBH_CTRL->pll_ldo_ctl = 4; /*ldo_ctl=core_rdy */
    USBH_CTRL->pll_ctl |= ( 1 << 31); /*pll_iddq=1*/
    mdelay(10);
    USBH_CTRL->pll_ctl &= ~( 1 << 31); /*pll_iddq=0*/
    USBH_CTRL->pll_ldo_ctl |= 1; /*ldo_ctl.AFE_LDO_PWRDWNB=1*/
    USBH_CTRL->pll_ldo_ctl |= 2; /*ldo_ctl.AFE_BG_PWRDWNB=1*/
    mdelay(1);
    USBH_CTRL->utmi_ctl_1 = 0x00020002;/* utmi_resetb &ref_clk_sel=0; */ 
    USBH_CTRL->pll_ctl |= ( 1 << 30); /*pll_resetb=1*/
    mdelay(10);
}    

#define MDIO_USB2   0
#define MDIO_USB3   (1 << 31)
static void usb_mdio_write(volatile u32 *mdio, u32 reg, u32 val, int mode)
{
    uint32_t data;
    data = (reg << 16) | val | mode;
    *mdio = data;
    data |= (1 << 25);
    *mdio = data;
    mdelay(1);
    data &= ~(1 << 25);
    *mdio = data;
}

static void usb2_eye_fix(void)
{
    /* Updating USB 2.0 PHY registers */
    usb_mdio_write((void *)&USBH_CTRL->mdio, 0x1f, 0x80a0, MDIO_USB2);
    usb_mdio_write((void *)&USBH_CTRL->mdio, 0x0a, 0xc6a0, MDIO_USB2);
}

static void usb3_pll_fix(void)
{
    /* Updating USB 3.0 PHY registers */
    usb_mdio_write((void *)&USBH_CTRL->mdio, 0x1f, 0x8000, MDIO_USB3);
    usb_mdio_write((void *)&USBH_CTRL->mdio, 0x07, 0x1503, MDIO_USB3);
}


static __init struct platform_device *bcm_add_usb_host(int type, int id,
                            uint32_t mem_base, uint32_t mem_size, int irq,
                            const char *devname, void *private_data)
{
    struct resource res[2];
    struct platform_device *pdev;
    //static const u64 usb_dmamask = ~(u32)0;
    static const u64 usb_dmamask = 0xffffffff;

    memset(&res, 0, sizeof(res));
    res[0].start = mem_base;
    res[0].end   = mem_base + (mem_size -1);
    res[0].flags = IORESOURCE_MEM;

    res[1].flags = IORESOURCE_IRQ;
    res[1].start = res[1].end = irq;

    pdev = platform_device_alloc(devname, id);
    if(!pdev)
    {
        printk("Error Failed to allocate platform device for devname=%s id=%d\n",
                devname, id);
    }

    platform_device_add_resources(pdev, res, 2);

    pdev->dev.dma_mask = (u64 *)&usb_dmamask;
    pdev->dev.coherent_dma_mask = 0xffffffff;
    
    if(private_data)
    {
        pdev->dev.platform_data = private_data;
    } 

    if( platform_device_add(pdev))
    {
        printk(KERN_ERR "Error Failed to add platform device for devname=%s id=%d\n",
                devname, id);
    }

    return pdev;
}

static void bcm63381_usb30_init(void)
{

    /*initialize XHCI settings*/
    //USB30H_CTRL->setup |= (USBH_IPP);

    USB30H_CTRL->usb30_ctl2 = 0x1; /*swap data & control */

    USB30H_CTRL->usb30_ctl1 |= (1<<30); /*disable over current*/
    USB30H_CTRL->usb30_ctl1 |= USB3_IOC;
    //USB30H_CTRL->usb30_ctl1 |= USB3_IPP;
    USB30H_CTRL->usb30_ctl1 |= XHC_SOFT_RESETB;
    USB30H_CTRL->usb30_ctl1 |= PHY3_PLL_SEQ_START;

    /* work around to avoid USB3.0 issue of contoller being reset when UBUS is loaded */ 
    USB30H_CTRL->bridge_ctl = (USBH_CTRL->bridge_ctl & 0xFFFF00FF) | (0x1000);
    
    usb3_pll_fix();

    xhci_dev = bcm_add_usb_host(CAP_TYPE_XHCI, 0, USB_XHCI_BASE, 0x1000,
                                INTERRUPT_ID_USBH30, "xhci-hcd", NULL); 
}

#endif

static int __init bcm63381_hw_init(void)
{ 
#if defined(CONFIG_USB)
    short usb_gpio;
    unsigned int chipRev = UtilGetChipRev();
    if(pmc_usb_power_up(PMC_USB_HOST_20))
    {
        printk(KERN_ERR "+++ Failed to Power Up USB20 Host\n");
        return -1;
    }
    
    bcm63381_manual_usb_ldo_start();

    USBH_CTRL->setup |= (USBH_IOC);
    if(BpGetUsbPwrFlt0(&usb_gpio) == BP_SUCCESS)
    {
       if((usb_gpio & BP_ACTIVE_MASK) != BP_ACTIVE_LOW)
       {
          USBH_CTRL->setup &= ~(USBH_IOC);
       }
    }
    if(BpGetUsbPwrOn0(&usb_gpio) == BP_SUCCESS)
    {
       if((usb_gpio & BP_ACTIVE_MASK) != BP_ACTIVE_LOW)
       {
          USBH_CTRL->setup &= ~(USBH_IPP);
       }
       else
       {
            USBH_CTRL->setup |= (USBH_IPP);
       }
    }

    if ((chipRev & 0xf0) == 0xa0)
    {
        USBH_CTRL->bridge_ctl |= (EHCI_ENDIAN_SWAP | OHCI_ENDIAN_SWAP);
    } else {
        USBH_CTRL->bridge_ctl = (USBH_CTRL->bridge_ctl & ~EHCI_SWAP_MODE_MASK & ~OHCI_SWAP_MODE_MASK) 
                                | ((EHCI_SWAP_MODE_BOTH << EHCI_SWAP_MODE_SHIFT) | (OHCI_SWAP_MODE_BOTH << OHCI_SWAP_MODE_SHIFT));
    }

    usb2_eye_fix();

    if(kerSysGetUsb30HostEnable())
    {
        if(pmc_usb_power_up(PMC_USB_HOST_30))
        {
            printk(KERN_ERR "+++ Failed to Power Up USB30 Host\n");
            return -1;
        }
        mdelay(10);
        bcm63381_usb30_init();
    }
#endif
	return 0;
}

#define bcm63xx_specific_hw_init() bcm63381_hw_init()

#elif defined(CONFIG_BCM96848)

static int __init bcm6848_hw_init(void)
{ 
#if defined(CONFIG_USB)
    short usb_gpio;

    if (!kerSysGetUsbHostPortEnable(0))
        return 0;

    if(pmc_usb_power_up(PMC_USB_HOST_20))
    {
        printk(KERN_ERR "+++ Failed to Power Up USB20 Host\n");
        return -1;
    }

    USBH_CTRL->setup |= (USBH_IOC);
    if(BpGetUsbPwrFlt0(&usb_gpio) == BP_SUCCESS)
    {
       if((usb_gpio & BP_ACTIVE_MASK) != BP_ACTIVE_LOW)
       {
          USBH_CTRL->setup &= ~(USBH_IOC);
       }
    }
    if(BpGetUsbPwrOn0(&usb_gpio) == BP_SUCCESS)
    {
       if((usb_gpio & BP_ACTIVE_MASK) != BP_ACTIVE_LOW)
       {
          USBH_CTRL->setup &= ~(USBH_IPP);
       }
       else
       {
            USBH_CTRL->setup |= (USBH_IPP);
       }
    }

    USBH_CTRL->bridge_ctl = (USBH_CTRL->bridge_ctl & ~EHCI_SWAP_MODE_MASK & ~OHCI_SWAP_MODE_MASK) 
        | ((EHCI_SWAP_MODE_BOTH << EHCI_SWAP_MODE_SHIFT) | (OHCI_SWAP_MODE_BOTH << OHCI_SWAP_MODE_SHIFT));
#endif
	return 0;
}

#define bcm63xx_specific_hw_init() bcm6848_hw_init()

#endif

static int __init bcm63xx_hw_init(void)
{
#if !defined(CONFIG_BRCM_IKOS)
    kerSysFlashInit();
#endif

    return bcm63xx_specific_hw_init();
}
arch_initcall(bcm63xx_hw_init);


static int __init brcm63xx_setup(void)
{
    extern int panic_timeout;

    _machine_restart = brcm_machine_restart;
    _machine_halt = brcm_machine_halt;
    pm_power_off = brcm_machine_halt;

    panic_timeout = 1;

    return 0;
}

arch_initcall(brcm63xx_setup);


unsigned long getMemorySize(void)
{
#if defined(CONFIG_BRCM_IKOS)
    return(31 * 1024 * 1024); /* voice DSP is loaded after this amount */
#elif defined(CONFIG_BRCM_MEMORY_RESTRICTION_16M)
    return(16 * 1024 * 1024); 
#elif defined(CONFIG_BRCM_MEMORY_RESTRICTION_32M)
    return(32 * 1024 * 1024); 
#elif defined(CONFIG_BRCM_MEMORY_RESTRICTION_64M)
    return(64 * 1024 * 1024); 
#elif defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) 
    return (DDR->CSEND << 24);
#elif defined(CONFIG_BCM96318) || defined(CONFIG_BCM960333) || defined(CONFIG_BCM963381)
    uint32 memCfg;

#if defined (CONFIG_BCM963381)
    memCfg = MEMC->SDR_CFG.SDR_CFG;
#else
    memCfg = MEMC->SDR_CFG;
#endif
    memCfg = (memCfg&MEMC_SDRAM_SPACE_MASK)>>MEMC_SDRAM_SPACE_SHIFT;

    return 1<<(memCfg+20);
#elif defined(CONFIG_BCM96848)
    return 1<<(((MEMC->GLB_GCFG&MEMC_GLB_GCFG_SIZE1_MASK)>>MEMC_GLB_GCFG_SIZE1_SHIFT)+20);
#else
    return (((MEMC->CSEND > 16) ? 16 : MEMC->CSEND) << 24);
#endif
}


/* Pointers to memory buffers allocated for the DSP module */
void *dsp_core;
void *dsp_init;
EXPORT_SYMBOL(dsp_core);
EXPORT_SYMBOL(dsp_init);
void __init allocDspModBuffers(void);
/*
*****************************************************************************
** FUNCTION:   allocDspModBuffers
**
** PURPOSE:    Allocates buffers for the init and core sections of the DSP
**             module. This module is special since it has to be allocated
**             in the 0x800.. memory range which is not mapped by the TLB.
**
** PARAMETERS: None
** RETURNS:    Nothing
*****************************************************************************
*/
void __init allocDspModBuffers(void)
{
#if defined(CONFIG_BCM_ENDPOINT_MODULE)
    printk("Allocating memory for DSP module core and initialization code\n");

  dsp_core = (void*)((DSP_CORE_SIZE > 0) ? alloc_bootmem((unsigned long)DSP_CORE_SIZE) : NULL);
  dsp_init = (void*)((DSP_INIT_SIZE > 0) ? alloc_bootmem((unsigned long)DSP_INIT_SIZE) : NULL);

  printk("Allocated DSP module memory - CORE=0x%x SIZE=%d, INIT=0x%x SIZE=%d\n",
         (unsigned int)dsp_core, DSP_CORE_SIZE, (unsigned int)dsp_init , DSP_INIT_SIZE);
#endif
}

#endif
