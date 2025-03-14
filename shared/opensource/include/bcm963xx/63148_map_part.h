/*
<:copyright-BRCM:2013:DUAL/GPL:standard

   Copyright (c) 2013 Broadcom Corporation
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

#ifndef __BCM63148_MAP_PART_H
#define __BCM63148_MAP_PART_H

#ifdef __cplusplus
extern "C" {
#endif

#define CHIP_FAMILY_ID_HEX	0x63148

/* Physical address for all the registers */
#define SPIFLASH_PHYS_BASE	0xffd00000	/* spi flash direct access address */
#define NANDFLASH_PHYS_BASE	0xffe00000	/* nand flash direct access address */
#define PERF_PHYS_BASE		0xfffe8000	/* chip control */
#define TIMR_PHYS_BASE		0xfffe8080	/* timer registers */
#define NAND_INTR_PHYS_BASE	0xfffe80f0	/* NAND int register */
#define GPIO_PHYS_BASE		0xfffe8100	/* gpio registers */
#define I2C_BASE            0xfffebe00  /* I2C regsiters */
#define MISC_PHYS_BASE		0xfffe8180	/* Miscellaneous Registers */
#define SOTP_PHYS_BASE		0xfffe8200	/* SOTP register */
#define PKA_PHYS_BASE		0xfffe8280
#define RNG_PHYS_BASE		0xfffe8300
#define UART0_PHYS_BASE		0xfffe8600	/* uart registers */
#define UART_PHYS_BASE		UART0_PHYS_BASE
#define UART1_PHYS_BASE		0xfffe8620	/* uart registers */
#define LED_PHYS_BASE		0xfffe8700	/* LED control registers */
#define I2S_PHYS_BASE		0xfffe8900
#define AES0_PHYS_BASE		0xfffe8980
#define AES1_PHYS_BASE		0xfffe8a00
#define HSSPIM_PHYS_BASE	0xfffe9000	/* High-Speed SPI registers */
#define NAND_REG_PHYS_BASE	0xfffea000	/* nand interrupt control */
#define NAND_CACHE_PHYS_BASE	0xfffea400
#define BROM_SEC_PHYS_BASE	0xfffeb600
#define SRAM_SEC_PHYS_BASE	0xfffeb800
#define PER_SEC_PHYS_BASE	0xfffeb820
#define JTAG_OTP_PHYS_BASE	0xfffebb00
#define JTAG_IOTP_PHYS_BASE	0xfffebd00
#define BOOTLUT_PHYS_BASE	0xffff0000

/*TODO change the naming of usbdevice regs to USB20D */
#define USB_CTL_PHYS_BASE	0x80001000	/* USB 2.0 device control */
#define USB_DMA_PHYS_BASE	0x80001800	/* USB 2.0 device DMA registers */
#define MEMC_PHYS_BASE		0x80002000	/* DDR IO Buf Control */
#define DDRPHY_PHYS_BASE	0x80003000
#define SAR_PHYS_BASE		0x80004000
#define SAR_DMA_PHYS_BASE	0x80007800	/* ATM SAR DMA control */
#define SATA_PHYS_BASE		0x80008000
#define USBH_PHYS_BASE		0x8000c000
#define USBH_CFG_PHYS_BASE	0x8000c200
#define USB_EHCI_PHYS_BASE	0x8000c300	/* USB host registers */
#define USB_OHCI_PHYS_BASE	0x8000c400	/* USB host registers */
#define USB_XHCI_PHYS_BASE	0x8000d000	/* USB host registers */
#define USB_XHCI1_PHYS_BASE	0x8000e000	/* USB host registers */
#define ERROR_PORT_PHYS_BASE	0x80010000

#define AIP_PHYS_BASE		0x80018000
#if 0	/* may be needed */
#define ETB_PHYS_BASE		0x8001a000
#define CSCTI_PHYS_BASE		0x8001b000
#endif

#define B15_CTRL_PHYS_BASE	0x80020000
#define B15_PHYS_BASE		0x80030000

#define DECT_PHYS_BASE		0x80040000
#define DECT_AHB_REG_PHYS_BASE	DECT_PHYS_BASE
#define DECT_SHIM_CTRL_PHYS_BASE	0x80050000
#define DECT_SHIM_DMA_CTRL_PHYS_BASE	0x80050050
#define DECT_APB_REG_PHYS_BASE	0x80050800
#define PCIE_0_PHYS_BASE		0x80060000
#define PCIE_PHYS_BASE		PCIE_0_PHYS_BASE
#define PCIE_1_PHYS_BASE		0x80070000

#define SF2_PHYS_BASE		0x80080000
#define SWITCH_PHYS_BASE	SF2_PHYS_BASE
#define APM_PHYS_BASE		0x80100000
#define RDP_PHYS_BASE		0x80200000
#define PMC_PHYS_BASE		0x80400000
#define PROC_MON_PHYS_BASE	0x80480000
#define DSLPHY_PHYS_BASE	0x80600000
#define DSLPHY_AFE_PHYS_BASE	0x80657300
#define DSLLMEM_PHYS_BASE	0x80700000

#define PCIE0_MEM_PHYS_BASE	0x90000000
#define PCIE1_MEM_PHYS_BASE	0xa0000000


#ifndef __BCM63148_MAP_H
#define __BCM63148_MAP_H

#include "bcmtypes.h"

/*
 * note: in CFE, MMU is initialized in order to use data cache.  However,
 * the address translation here is virtual address = physical address.
 * Therefore, CFE can use physical address to access registers, since
 * the translation is transparent for it.  CFE will disable / de-initialize
 * MMU before it hands over to kernel, 
 *
 * As when Kernel just takes over the control, MMU is not initialized,
 * the register access will be based on physical address.  Once MMU is
 * set up and initialized, then it will use virtual address. Most of
 * the register accesses by module should happen after MMU is initialized.
 *
 * In case that if one tries to access register before MMU is initialized
 * and not in CFE, one should take care of assigning register data
 * structure to a physical address on his own, i.e., UART driver in Lnux
 * might be used before and after MMU is initialized, so the driver
 * should take care of using physical or virtual address.
 */
#ifdef _CFE_
#define BCM_IO_ADDR(x) (x)
#elif defined(__KERNEL__) 
#include <mach/hardware.h>
#define BCM_IO_ADDR(x)	IO_ADDRESS(x)
#else /* Userspace - #define BCM_IO_ADDR to allow compiling when bcm_map.h is not included first */
#define BCM_IO_ADDR(x) (x)
#endif

/* Physical and access(could be virtual or physical) bases address for
 * all the registers */
#define SPIFLASH_BASE		BCM_IO_ADDR(SPIFLASH_PHYS_BASE)
#define NANDFLASH_BASE		BCM_IO_ADDR(NANDFLASH_PHYS_BASE)
#define PERF_BASE		BCM_IO_ADDR(PERF_PHYS_BASE)
#define TIMR_BASE		BCM_IO_ADDR(TIMR_PHYS_BASE)
#define NAND_INTR_BASE		BCM_IO_ADDR(NAND_INTR_PHYS_BASE)
#define GPIO_BASE		BCM_IO_ADDR(GPIO_PHYS_BASE)
#define MISC_BASE		BCM_IO_ADDR(MISC_PHYS_BASE)
#define SOTP_BASE		BCM_IO_ADDR(SOTP_PHYS_BASE)
#define PKA_BASE		BCM_IO_ADDR(PKA_PHYS_BASE)
#define RNG_BASE		BCM_IO_ADDR(RNG_PHYS_BASE)
#define UART0_BASE		BCM_IO_ADDR(UART0_PHYS_BASE)
#define UART_BASE		UART0_BASE
#define UART1_BASE		BCM_IO_ADDR(UART1_PHYS_BASE)
#define LED_BASE		BCM_IO_ADDR(LED_PHYS_BASE)
#define I2S_BASE		BCM_IO_ADDR(I2S_PHYS_BASE)
#define AES0_BASE		BCM_IO_ADDR(AES0_PHYS_BASE)
#define AES1_BASE		BCM_IO_ADDR(AES1_PHYS_BASE)
#define HSSPIM_BASE		BCM_IO_ADDR(HSSPIM_PHYS_BASE)
#define NAND_REG_BASE		BCM_IO_ADDR(NAND_REG_PHYS_BASE)
#define NAND_CACHE_BASE		BCM_IO_ADDR(NAND_CACHE_PHYS_BASE)
#define BROM_SEC_BASE		BCM_IO_ADDR(BROM_SEC_PHYS_BASE)
#define SRAM_SEC_BASE		BCM_IO_ADDR(SRAM_SEC_PHYS_BASE)
#define PER_SEC_BASE		BCM_IO_ADDR(PER_SEC_PHYS_BASE)
#define JTAG_OTP_BASE		BCM_IO_ADDR(JTAG_OTP_PHYS_BASE)
#define JTAG_IOTP_BASE		BCM_IO_ADDR(JTAG_IOTP_PHYS_BASE)
#define BOOTLUT_BASE		BCM_IO_ADDR(BOOTLUT_PHYS_BASE)

/*TODO keep the naming convention same as RDB */
#define USB_CTL_BASE		BCM_IO_ADDR(USB_CTL_PHYS_BASE)
#define USB_DMA_BASE		BCM_IO_ADDR(USB_DMA_PHYS_BASE)
#define MEMC_BASE		BCM_IO_ADDR(MEMC_PHYS_BASE)
#define DDRPHY_BASE		BCM_IO_ADDR(DDRPHY_PHYS_BASE)
#define SAR_BASE		BCM_IO_ADDR(SAR_PHYS_BASE)
#define SAR_DMA_BASE		BCM_IO_ADDR(SAR_DMA_PHYS_BASE)
#define SATA_BASE		BCM_IO_ADDR(SATA_PHYS_BASE)
#define USBH_BASE		BCM_IO_ADDR(USBH_PHYS_BASE)
#define USBH_CFG_BASE		BCM_IO_ADDR(USBH_CFG_PHYS_BASE)
#define USB_EHCI_BASE		BCM_IO_ADDR(USB_EHCI_PHYS_BASE)
#define USB_OHCI_BASE		BCM_IO_ADDR(USB_OHCI_PHYS_BASE)
#define USB_XHCI_BASE		BCM_IO_ADDR(USB_XHCI_PHYS_BASE)
#define USB_XHCI1_BASE		BCM_IO_ADDR(USB_XHCI1_PHYS_BASE)
#define ERROR_PORT_BASE		BCM_IO_ADDR(ERROR_PORT_PHYS_BASE)

#define B15_CTRL_BASE		BCM_IO_ADDR(B15_CTRL_PHYS_BASE)
#define B15_BASE		BCM_IO_ADDR(B15_PHYS_BASE)

#define DECT_BASE		BCM_IO_ADDR(DECT_PHYS_BASE)
#define DECT_AHB_REG_BASE	DECT_BASE
#define DECT_SHIM_CTRL_BASE		BCM_IO_ADDR(DECT_SHIM_CTRL_PHYS_BASE)
#define DECT_SHIM_DMA_CTRL_BASE		BCM_IO_ADDR(DECT_SHIM_DMA_CTRL_PHYS_BASE)
#define DECT_APB_REG_BASE	BCM_IO_ADDR(DECT_APB_REG_PHYS_BASE)
#define PCIE_0_BASE		BCM_IO_ADDR(PCIE_0_PHYS_BASE)
#define PCIE_BASE		PCIE_0_BASE
#define PCIE_1_BASE		BCM_IO_ADDR(PCIE_1_PHYS_BASE)

#define SF2_BASE		BCM_IO_ADDR(SF2_PHYS_BASE)
#define SWITCH_BASE		SF2_BASE
#define APM_BASE		BCM_IO_ADDR(APM_PHYS_BASE)
#define RDP_BASE		BCM_IO_ADDR(RDP_PHYS_BASE)
#define PMC_BASE		BCM_IO_ADDR(PMC_PHYS_BASE)
#define PROC_MON_BASE		BCM_IO_ADDR(PROC_MON_PHYS_BASE)
#define DSLPHY_BASE		BCM_IO_ADDR(DSLPHY_PHYS_BASE)
#define DSLPHY_AFE_BASE		BCM_IO_ADDR(DSLPHY_AFE_PHYS_BASE)
#define DSLLMEM_BASE		BCM_IO_ADDR(DSLLMEM_PHYS_BASE)

/* PCIE MEM does not get MMAP to MMU by default */
#define PCIE0_MEM_BASE		PCIE0_MEM_PHYS_BASE
#define PCIE_MEM_BASE		PCIE0_MEM_BASE
#define PCIE1_MEM_BASE		PCIE1_MEM_PHYS_BASE

#if 0	// FIXME later!
#define IPSEC_BASE		BCM_IO_ADDR(0xb0002800)
#define IPSEC_DMA_BASE		BCM_IO_ADDR(0xb000d000)
#define SWITCH_DMA_BASE		BCM_IO_ADDR(0xb000d800)
#define GMAC_BASE		BCM_IO_ADDR(0xb000e000)
#define GMAC_DMA_BASE		BCM_IO_ADDR(0xb000e000)

#endif

#define DECT_AHB_SHARED_RAM_BASE    DECT_AHB_REG_BASE
#define DECT_STARTCTL      (DECT_AHB_SHARED_RAM_BASE + 0x0818)
#define PCM_BUFF_CTL3      (DECT_AHB_SHARED_RAM_BASE + 0x082c)
#define PCM_BUFF_CTL7      (DECT_AHB_SHARED_RAM_BASE + 0x083c)
#define DECT_AHB_CHAN0_RX  (DECT_AHB_REG_PHYS_BASE + 0x0a20)
#define DECT_AHB_CHAN1_RX  (DECT_AHB_REG_PHYS_BASE + 0x0de0)
#define DECT_AHB_CHAN2_RX  (DECT_AHB_REG_PHYS_BASE + 0x11a0)
#define DECT_AHB_CHAN3_RX  (DECT_AHB_REG_PHYS_BASE + 0x1560)
#define DECT_AHB_CHAN0_TX  (DECT_AHB_REG_PHYS_BASE + 0x0840)
#define DECT_AHB_CHAN1_TX  (DECT_AHB_REG_PHYS_BASE + 0x0c00)
#define DECT_AHB_CHAN2_TX  (DECT_AHB_REG_PHYS_BASE + 0x0fc0)
#define DECT_AHB_CHAN3_TX  (DECT_AHB_REG_PHYS_BASE + 0x1380)
#define DECT_CLKEN         0x00000040 

#endif   /* ifndef __BCM63148_MAP_H */


/*
** DECT IP Device Registers
*/
typedef enum  DECT_SHM_ENABLE_BITS
{
   DECT_SHM_IRQ_DSP_INT,
   DECT_SHM_IRQ_DSP_IRQ_OUT,
   DECT_SHM_IRQ_DIP_INT,
   DECT_SHM_H2D_BUS_ERR,
   DECT_SHM_IRQ_TX_DMA_DONE,                 
   DECT_SHM_IRQ_RX_DMA_DONE,
   DECT_SHM_IRQ_PLL_PHASE_LOCK = DECT_SHM_IRQ_RX_DMA_DONE + 2, /* Skip reserved bit */
   DECT_SHM_REG_DSP_BREAK,
   DECT_SHM_REG_DIP_BREAK,
   DECT_SHM_REG_IRQ_TO_IP = DECT_SHM_REG_DIP_BREAK + 2, /* Skip reserved bit */
   DECT_SHM_TX_DMA_DONE_TO_IP,
   DECT_SHM_RX_DMA_DONE_TO_IP, 
} DECT_SHM_ENABLE_BITS;

typedef struct DECTShimControl 
{
   uint32 dect_shm_ctrl;                     /*  0x80050000  DECT shim control registers                    */
#define APB_SWAP_MASK             0x0000C000
#define APB_SWAP_16_BIT           0x00000000
#define APB_SWAP_8_BIT            0x00004000
#define AHB_SWAP_MASK             0x00003000
#define AHB_SWAP_16_BIT           0x00003000
#define AHB_SWAP_8_BIT            0x00002000
#define AHB_SWAP_ACCESS           0x00001000
#define AHB_SWAP_NONE             0x00000000
#define UBUS3_SWAP                0x00000400
#define DECT_PULSE_COUNT_ENABLE   0x00000200
#define PCM_PULSE_COUNT_ENABLE    0x00000100
#define DECT_SOFT_RESET           0x00000010
#define PHCNT_CLK_SRC_PLL         0x00000008
#define PHCNT_CLK_SRC_XTAL        0x00000000
#define DECT_CLK_OUT_PLL          0x00000004
#define DECT_CLK_OUT_XTAL         0x00000000
#define DECT_CLK_CORE_PCM         0x00000002
#define DECT_CLK_CORE_DECT        0x00000000
#define DECT_PLL_REF_PCM          0x00000001
#define DECT_PLL_REF_DECT         0x00000000

   uint32 dect_shm_pcm_clk_cntr;             /*  0x80050004  PCM clock counter                              */
   uint32 dect_shm_dect_clk_cntr;            /*  0x80050008  DECT clock counter                             */
   uint32 dect_shm_dect_clk_cntr_sh;         /*  0x8005000c  DECT clock counter snapshot                    */
   uint32 dect_shm_irq_enable;               /*  0x80050010  DECT interrupt enable register                 */
   uint32 dect_shm_irq_status;               /*  0x80050014  DECT Interrupt status register                 */
   uint32 dect_shm_irq_trig;                 /*  0x80050018  DECT DSP ext IRQ trigger and IRQ test register */
   uint32 dect_shm_dma_status;               /*  0x8005001c  DECT DMA STATUS register                       */
   //uint32 dect_shm_xtal_ctrl;                /*  0xb000b020  DECT analog tunable XTAL control register      */
   //uint32 dect_shm_bandgap_ctrl;             /*  0xb000b024  DECT analog bandgap control register           */
   //uint32 dect_shm_afe_tx_ctrl;              /*  0xb000b028  DECT analog TX DAC control register            */
   //uint32 dect_shm_afe_test_ctrl;            /*  0xb000b02c  DECT analog test control register              */
   uint32 dect_shim_dcxo_ctrl0;              /*  0x80050020 DECT analog turnable XTAL control register0     */
   uint32 dect_shim_dcxo_ctrl1;              /*  0x80050024 DECT analog turnable XTAL control register1     */
   uint32 dect_shim_pcm_ctrl;                /*  0x80050028 PCM CLK control register                        */
#define  XTAL_DIV_EN                         0x00001000	
   uint32 dect_shim_unused0;                 /*  0x8005002C unused                                          */
   uint32 dect_shm_pll_reg_0;                /*  0x80050030  DECT PLL configuration register 0,   DECT_SHIM_PLL_CTRL0 in RDB  */
   uint32 dect_shm_pll_reg_1;                /*  0x80050034  DECT PLL configuration register 1              */
#define  PLL_RESETB                          0x80000000
#define  PLL_P_RESETB                        0x40000000
#define  PLL_PWRON                           0x08000000
#define  PLL_PWRDWN_CH1                      0x04000000
#define  PLL_PWRDWN_CH2                      0x02000000
#define  PLL_PWRDWN_CH3                      0x01000000

   uint32 dect_shm_pll_reg_2;                /*  0x80050038  DECT PLL Ndiv configuration register           */
   uint32 dect_shm_pll_reg_3;                /*  0x8005003c  DECT PLL Pdiv and Mdiv configuration register  */
} DECTShimControl;

#define DECT_CTRL ((volatile DECTShimControl * const) DECT_SHIM_CTRL_BASE)

typedef struct DECTShimDmaControl 
{
   uint32 dect_shm_dma_ctrl;                 /*  0x80050050  DECT DMA control register                      */
#define  DMA_CLEAR                           0x80000000
#define  DMA_SWAP_16_BIT                     0x03000000
#define  DMA_SWAP_8_BIT                      0x02000000
#define  DMA_SWAP_NONE                       0x01000000
#define  DMA_SUBWORD_SWAP_MASK               0x03000000
#define  DMA_TRIG_CNT_CLK_SEL_PCM                0x00800000
#define  DMA_TRIG_CNT_IRQ_EN                     0x00400000
#define  DMA_RX_CNT_TRIG_EN                  0x00200000   
#define  DMA_TX_CNT_TRIG_EN                  0x00100000 
#define  DMA_RX_INT_TRIG_EN                  0x00080000 	
#define  DMA_TX_INT_TRIG_EN                  0x00040000 	
#define  DMA_RX_REG_TRIG_EN                  0x00020000 	
#define  DMA_TX_REG_TRIG_EN                  0x00010000 
#define  DMA_RX_TRIG_FIRST                   0x00008000
#define  MAX_BURST_CYCLE_MASK                0x00001F00
#define  MAX_BURST_CYCLE_SHIFT               8
#define  RX_EN_3                             0x00000080   
#define  RX_EN_2                             0x00000040   
#define  RX_EN_1                             0x00000020   
#define  RX_EN_0                             0x00000010   
#define  TX_EN_3                             0x00000008   
#define  TX_EN_2                             0x00000004   
#define  TX_EN_1                             0x00000002   
#define  TX_EN_0                             0x00000001   
 
   uint32 dect_shm_dma_trig_cnt_preset;      /*  0x80050054  DECT DMA trigger counter preset value                */
   uint32 dect_shm_dma_ddr_saddr_tx_s0;      /*  0x80050058  DECT DMA DDR buffer starting address for TX slot 0   */
   uint32 dect_shm_dma_ddr_saddr_tx_s1;      /*  0x8005005c  DECT DMA DDR buffer starting address for TX slot 1   */
   uint32 dect_shm_dma_ddr_saddr_tx_s2;      /*  0x80050060  DECT DMA DDR buffer starting address for TX slot 2   */
   uint32 dect_shm_dma_ddr_saddr_tx_s3;      /*  0x80050064  DECT DMA DDR buffer starting address for TX slot 3   */
   uint32 dect_shm_dma_ddr_saddr_rx_s0;      /*  0x80050068  DECT DMA DDR buffer starting address for RX slot 0   */
   uint32 dect_shm_dma_ddr_saddr_rx_s1;      /*  0x8005006c  DECT DMA DDR buffer starting address for RX slot 1   */
   uint32 dect_shm_dma_ddr_saddr_rx_s2;      /*  0x80050070  DECT DMA DDR buffer starting address for RX slot 2   */
   uint32 dect_shm_dma_ddr_saddr_rx_s3;      /*  0x80050074  DECT DMA DDR buffer starting address for RX slot 3   */
   uint32 dect_shm_dma_ahb_saddr_tx_s01;     /*  0x80050078  DECT DMA AHB shared memory buffer starting address for TX slots 0 and 1  */
   uint32 dect_shm_dma_ahb_saddr_tx_s23;     /*  0x8005007c  DECT DMA AHB shared memory buffer starting address for TX slots 2 and 3  */
   uint32 dect_shm_dma_ahb_saddr_rx_s01;     /*  0x80050080  DECT DMA AHB shared memory buffer starting address for RX slots 0 and 1  */
   uint32 dect_shm_dma_ahb_saddr_rx_s23;     /*  0x80050084  DECT DMA AHB shared memory buffer starting address for RX slots 2 and 3  */
   uint32 dect_shm_dma_xfer_size_tx;         /*  0x80050088  DECT DMA TX slots transfer size of each trigger      */
   uint32 dect_shm_dma_xfer_size_rx;         /*  0x8005008c  DECT DMA RX slots transfer size of each trigger      */
   uint32 dect_shm_dma_buf_size_tx;          /*  0x80050090  DECT DMA TX slots memory buffer size                 */
   uint32 dect_shm_dma_buf_size_rx;          /*  0x80050094  DECT DMA RX slots memory buffer size                 */
   uint32 dect_shm_dma_offset_addr_tx_s01;   /*  0x80050098  DECT DMA access offset address for TX slots 0 and 1  */
   uint32 dect_shm_dma_offset_addr_tx_s23;   /*  0x8005009c  DECT DMA access offset address for TX slots 2 and 3  */
   uint32 dect_shm_dma_offset_addr_rx_s01;   /*  0x800500a0  DECT DMA access offset address for RX slots 0 and 1  */
   uint32 dect_shm_dma_offset_addr_rx_s23;   /*  0x800500a4  DECT DMA access offset address for RX slots 2 and 3  */
   uint32 dect_shm_dma_xfer_cntr_tx;         /*  0x800500a8  DECT DMA transfer count per slot in number of DMA transfer size */
   uint32 dect_shm_dma_xfer_cntr_rx;         /*  0x800500a8  DECT DMA transfer count per slot in number of DMA transfer size */   
} DECTShimDmaControl;

#define DECT_DMA_CTRL ((volatile DECTShimDmaControl * const) DECT_SHIM_DMA_CTRL_BASE)


typedef struct UBUSInterface {
	uint32 CFG;			/* 0x00 */
#define UBUSIF_CFG_WRITE_REPLY_MODE_SHIFT   0x0  
#define UBUSIF_CFG_WRITE_REPLY_MODE_MASK    (0x1<< UBUSIF_CFG_WRITE_REPLY_MODE_SHIFT)
#define UBUSIF_CFG_WRITE_BURST_MODE_SHIFT   0x1  
#define UBUSIF_CFG_WRITE_BURST_MODE_MASK    (0x1<< UBUSIF_CFG_WRITE_BURST_MODE_SHIFT)
#define UBUSIF_CFG_INBAND_ERR_MASK_SHIFT    0x2
#define UBUSIF_CFG_INBAND_ERR_MASK_MASK     (0x1<< UBUSIF_CFG_INBAND_ERR_MASK_SHIFT)
#define UBUSIF_CFG_OOB_ERR_MASK_SHIFT       0x3
#define UBUSIF_CFG_OOB_ERR_MASK_MASK        (0x1<< UBUSIF_CFG_OOB_ERR_MASK_SHIFT)
	uint32 SRC_QUEUE_CTRL_0;	/* 0x04 */
	uint32 SRC_QUEUE_CTRL_1;	/* 0x08 */
	uint32 SRC_QUEUE_CTRL_2;	/* 0x0c */
	uint32 SRC_QUEUE_CTRL_3;	/* 0x10 */
	uint32 REP_ARB_MODE;		/* 0x14 */
#define UBUSIF_REP_ARB_MODE_FIFO_MODE_SHIFT 0x0
#define UBUSIF_REP_ARB_MODE_FIFO_MODE_MASK  (0x1<<UBUSIF_REP_ARB_MODE_FIFO_MODE_SHIFT)
	uint32 SCRATCH;			/* 0x18 */
	uint32 DEBUG_R0;		/* 0x1c */
	uint32 unused[8];		/* 0x20-0x3f */
} UBUSInterface;

typedef struct EDISEngine {
	uint32 REV_ID;			/* 0x00 */
	uint32 CTRL_TRIG;		/* 0x04 */
	uint32 CTRL_MODE;		/* 0x08 */
	uint32 CTRL_SIZE;		/* 0x0c */
	uint32 CTRL_ADDR_START;		/* 0x10 */
	uint32 CTRL_ADDR_START_EXT;	/* 0x14 */
	uint32 CTRL_ADDR_END;		/* 0x18 */
	uint32 CTRL_ADDR_END_EXT;	/* 0x1c */
	uint32 CTRL_WRITE_MASKS;	/* 0x20 */
	uint32 unused0[7];		/* 0x24-0x3f */
	uint32 STAT_MAIN;		/* 0x40 */
	uint32 STAT_WORDS_WRITTEN;	/* 0x44 */
	uint32 STAT_WORDS_READ;		/* 0x48 */
	uint32 STAT_ERROR_COUNT;	/* 0x4c */
	uint32 STAT_ERROR_BITS;		/* 0x50 */
	uint32 STAT_ADDR_LAST;		/* 0x54 */
	uint32 STAT_ADDR_LAST_EXT;	/* 0x58 */
	uint32 unused1[8];		/* 0x5c-0x7b */
	uint32 STAT_DEBUG;		/* 0x7c */
	uint32 STAT_DATA_PORT[8];	/* 0x80-0x9c */
	uint32 GEN_LFSR_STATE[4];	/* 0xa0-0xac */
	uint32 GEN_CLOCK;		/* 0xb0 */
	uint32 GEN_PATTERN;		/* 0xb4 */
	uint32 unused2[2];		/* 0xb8-0xbf */
	uint32 BYTELANE_0_CTRL_LO;	/* 0xc0 */
	uint32 BYTELANE_0_CTRL_HI;	/* 0xc4 */
	uint32 BYTELANE_1_CTRL_LO;	/* 0xc8 */
	uint32 BYTELANE_1_CTRL_HI;	/* 0xcc */
	uint32 BYTELANE_2_CTRL_LO;	/* 0xd0 */
	uint32 BYTELANE_2_CTRL_HI;	/* 0xd4 */
	uint32 BYTELANE_3_CTRL_LO;	/* 0xd8 */
	uint32 BYTELANE_3_CTRL_HI;	/* 0xdc */
	uint32 BYTELANE_0_STAT_LO;	/* 0xe0 */
	uint32 BYTELANE_0_STAT_HI;	/* 0xe4 */
	uint32 BYTELANE_1_STAT_LO;	/* 0xe8 */
	uint32 BYTELANE_1_STAT_HI;	/* 0xec */
	uint32 BYTELANE_2_STAT_LO;	/* 0xf0 */
	uint32 BYTELANE_2_STAT_HI;	/* 0xf4 */
	uint32 BYTELANE_3_STAT_LO;	/* 0xf8 */
	uint32 BYTELANE_3_STAT_HI;	/* 0xfc */
} EDISEngine;

typedef struct SecureRangeCheckers {
	uint32 LOCK;			/* 0x00 */
	uint32 LOG_INFO_0;		/* 0x04 */
	uint32 LOG_INFO_1;		/* 0x08 */
	uint32 CTRL_0;			/* 0x0c */
	uint32 UBUS0_PORT_0;		/* 0x10 */
	uint32 BASE_0;			/* 0x14 */
	uint32 CTRL_1;			/* 0x18 */
	uint32 UBUS0_PORT_1;		/* 0x1c */
	uint32 BASE_1;			/* 0x20 */
	uint32 CTRL_2;			/* 0x24 */
	uint32 UBUS0_PORT_2;		/* 0x28 */
	uint32 BASE_2;			/* 0x2c */
	uint32 CTRL_3;			/* 0x30 */
	uint32 UBUS0_PORT_3;		/* 0x34 */
	uint32 BASE_3;			/* 0x38 */
	uint32 CTRL_4;			/* 0x3c */
	uint32 UBUS0_PORT_4;		/* 0x40 */
	uint32 BASE_4;			/* 0x44 */
	uint32 CTRL_5;			/* 0x48 */
	uint32 UBUS0_PORT_5;		/* 0x4c */
	uint32 BASE_5;			/* 0x50 */
	uint32 CTRL_6;			/* 0x54 */
	uint32 UBUS0_PORT_6;		/* 0x58 */
	uint32 BASE_6;			/* 0x5c */
	uint32 CTRL_7;			/* 0x60 */
	uint32 UBUS0_PORT_7;		/* 0x64 */
	uint32 BASE_7;			/* 0x68 */
	uint32 unused[37];		/* 0x6c-0xff */
} SecureRangeCheckers;

typedef struct DDRPhyControl {
	uint32 REVISION;		/* 0x00 */
	uint32 PLL_STATUS;		/* 0x04 */
	uint32 PLL_CONFIG;		/* 0x08 */
	uint32 PLL_CONTROL1;		/* 0x0c */
	uint32 PLL_CONTROL2;		/* 0x10 */
	uint32 PLL_CONTROL3;		/* 0x14 */
	uint32 PLL_DIVIDER;		/* 0x18 */
	uint32 PLL_PRE_DIVIDER;		/* 0x1c */
	uint32 PLL_SS_EN;		/* 0x20 */
	uint32 PLL_SS_CFG;		/* 0x24 */
	uint32 AUX_CONTROL;		/* 0x28 */
	uint32 IDLE_PAD_CONTROL;	/* 0x2c */
	uint32 IDLE_PAD_EN0;		/* 0x30 */
	uint32 IDLE_PAD_EN1;		/* 0x34 */
	uint32 DRIVE_PAD_CTL;		/* 0x38 */
	uint32 STATIC_PAD_CTL;		/* 0x3c */
	uint32 DRAM_CFG;		/* 0x40 */
	uint32 DRAM_TIMING1;		/* 0x44 */
	uint32 DRAM_TIMING2;		/* 0x48 */
	uint32 DRAM_TIMING3;		/* 0x4c */
	uint32 DRAM_TIMING4;		/* 0x50 */
	uint32 unused0[3];		/* 0x54-0x5f */

	uint32 VDL_REGS[47];		/* 0x60-0x118 */
	uint32 AC_SPARE;		/* 0x11c */
	uint32 unused1[4];		/* 0x120-0x12f */

	uint32 REFRESH;			/* 0x130 */
	uint32 UPDATE_VDL;		/* 0x134 */
	uint32 UPDATE_VDL_SNOOP1;	/* 0x138 */
	uint32 UPDATE_VDL_SNOOP2;	/* 0x13c */
	uint32 CMND_REG1;		/* 0x140 */
	uint32 CMND_AUX_REG1;		/* 0x144 */
	uint32 CMND_REG2;		/* 0x148 */
	uint32 CMND_AUX_REG2;		/* 0x14c */
	uint32 CMND_REG3;		/* 0x150 */
	uint32 CMND_AUX_REG3;		/* 0x154 */
	uint32 CMND_REG4;		/* 0x158 */
	uint32 CMND_AUX_REG4;		/* 0x15c */
	uint32 CMND_REG_TIMER;		/* 0x160 */
	uint32 MODE_REG[9];		/* 0x164-184 */
#define PHY_CONTROL_MODE_REG_VALID_SHIFT       16
#define PHY_CONTROL_MODE_REG_VALID_MASK        (0x1<<PHY_CONTROL_MODE_REG_VALID_SHIFT)
	uint32 MODE_REG15;		/* 0x188 */
	uint32 MODE_REG63;		/* 0x18c */
	uint32 ALERT_CLEAR;		/* 0x190 */
	uint32 ALERT_STATUS;		/* 0x194 */
	uint32 CA_PARITY;		/* 0x198 */
	uint32 CA_PLAYBACK_CTRL;	/* 0x19c */
	uint32 CA_PLAYBACK_STATUS0;	/* 0x1a0 */
	uint32 CA_PLAYBACK_STATUS1;	/* 0x1a4 */
	uint32 unused2;			/* 0x1a8 */
	uint32 WRITE_LEVEL_CTRL;	/* 0x1ac */
	uint32 WRITE_LEVEL_STATUS;	/* 0x1b0 */
	uint32 READ_EN_CTRL;		/* 0x1b4 */
	uint32 READ_EN_STATUS;		/* 0x1b8 */
	uint32 unused3;			/* 0x1bc */
	uint32 TRAFFIC_GEN[12];		/* 0x1c0-0x1ec */
	uint32 VIRT_VTT_CTRL;		/* 0x1f0 */
	uint32 VIRT_VTT_STATUS;		/* 0x1f4 */
	uint32 VIRT_VTT_CONNECTION;	/* 0x1f8 */
	uint32 VIRT_VTT_OVERRIDE;	/* 0x1fc */
	uint32 VREF_DAC_CTRL;		/* 0x200 */
	uint32 PHYBIST[9];		/* 0x204-0x224 */
	uint32 unused4[2];		/* 0x228-0x22f */
	uint32 STANDBY_CTRL;		/* 0x230 */
	uint32 DEBUG_FREEZE_EN;		/* 0x234 */
	uint32 DEBUG_MUX_CTRL;		/* 0x238 */
	uint32 DFI_CTRL;		/* 0x23c */
#define PHY_CONTROL_DFI_CNTRL_DFI_CS1_SHIFT    7
#define PHY_CONTROL_DFI_CNTRL_DFI_CS1_MASK     (0x1<<PHY_CONTROL_DFI_CNTRL_DFI_CS1_SHIFT)
#define PHY_CONTROL_DFI_CNTRL_DFI_CS0_SHIFT    6
#define PHY_CONTROL_DFI_CNTRL_DFI_CS0_MASK     (0x1<<PHY_CONTROL_DFI_CNTRL_DFI_CS0_SHIFT)
#define PHY_CONTROL_DFI_CNTRL_DFI_RSTN_SHIFT   5
#define PHY_CONTROL_DFI_CNTRL_DFI_RSTN_MASK    (0x1<<PHY_CONTROL_DFI_CNTRL_DFI_RSTN_SHIFT)
#define PHY_CONTROL_DFI_CNTRL_DFI_CKE1_SHIFT   4
#define PHY_CONTROL_DFI_CNTRL_DFI_CKE1_MASK    (0x1<<PHY_CONTROL_DFI_CNTRL_DFI_CKE1_SHIFT)
#define PHY_CONTROL_DFI_CNTRL_DFI_CKE0_SHIFT   3
#define PHY_CONTROL_DFI_CNTRL_DFI_CKE0_MASK    (0x1<<PHY_CONTROL_DFI_CNTRL_DFI_CKE0_SHIFT)
#define PHY_CONTROL_DFI_CNTRL_ASSERT_REQ_SHIFT 0
#define PHY_CONTROL_DFI_CNTRL_ASSERT_REQ_MASK  (0x1<<PHY_CONTROL_DFI_CNTRL_ASSERT_REQ_SHIFT)
	uint32 WRITE_ODT_CTRL;		/* 0x240 */
	uint32 ABI_PAR_CTRL;		/* 0x244 */
	uint32 ZQ_CAL;			/* 0x248 */
	uint32 unused5[45];		/* 0x24c-0x2ff */
} DDRPhyControl;

typedef struct DDRPhyByteLaneControl {
	uint32 VDL_CTRL_WR[12];		/* 0x00 - 0x2c */
	uint32 VDL_CTRL_RD[24];		/* 0x30 - 0x8c */
	uint32 VDL_CLK_CTRL;		/* 0x90 */
	uint32 VDL_LDE_CTRL;		/* 0x94 */
	uint32 unused0[2];		/* 0x98-0x9f */
	uint32 RD_EN_DLY_CYC;		/* 0xa0 */
	uint32 WR_CHAN_DLY_CYC;		/* 0xa4 */
	uint32 unused1[2];		/* 0xa8-0xaf */
	uint32 RD_CTRL;			/* 0xb0 */
	uint32 RD_FIFO_ADDR;		/* 0xb4 */
	uint32 RD_FIFO_DATA;		/* 0xb8 */
	uint32 RD_FIFO_DM_DBI;		/* 0xbc */
	uint32 RD_FIFO_STATUS;		/* 0xc0 */
	uint32 RD_FIFO_CLR;		/* 0xc4 */
	uint32 IDLE_PAD_CTRL;		/* 0xc8 */
	uint32 DRIVE_PAD_CTRL;		/* 0xcc */
	uint32 RD_EN_DRIVE_PAD_CTRL;	/* 0xd0 */
	uint32 STATIC_PAD_CTRL;		/* 0xd4 */
	uint32 WR_PREAMBLE_MODE;	/* 0xd8 */
	uint32 unused2;			/* 0xdc */
	uint32 ODT_CTRL;		/* 0xe0 */
	uint32 unused3[3];		/* 0xe4-0xef */
	uint32 EDC_DPD_CTRL;		/* 0xf0 */
	uint32 EDC_DPD_STATUS;		/* 0xf4 */
	uint32 EDC_DPD_OUT_CTRL;	/* 0xf8 */
	uint32 EDC_DPD_OUT_STATUS;	/* 0xfc */
	uint32 EDC_DPD_OUT_STATUS_CLEAR;/* 0x100 */
	uint32 EDC_CRC_CTRL;		/* 0x104 */
	uint32 EDC_CRC_STATUS;		/* 0x108 */
	uint32 EDC_CRC_COUNT;		/* 0x10c */
	uint32 EDC_CRC_STATUS_CLEAR;	/* 0x110 */
	uint32 BL_SPARE_REG;		/* 0x114 */
	uint32 unused4[58];
} DDRPhyByteLaneControl;

/*
 * I2C Controller.
 */

typedef struct I2CControl {
  uint32        ChipAddress;            /* 0x0 */
#define I2C_CHIP_ADDRESS_MASK           0x000000f7
#define I2C_CHIP_ADDRESS_SHIFT          0x1
  uint32        DataIn0;                /* 0x4 */
  uint32        DataIn1;                /* 0x8 */
  uint32        DataIn2;                /* 0xc */

  uint32        DataIn3;                /* 0x10 */
  uint32        DataIn4;                /* 0x14 */
  uint32        DataIn5;                /* 0x18 */
  uint32        DataIn6;                /* 0x1c */

  uint32        DataIn7;                /* 0x20 */
  uint32        CntReg;                 /* 0x24 */
#define I2C_CNT_REG1_SHIFT              0x0
#define I2C_CNT_REG2_SHIFT              0x6
  uint32        CtlReg;                 /* 0x28 */
#define I2C_CTL_REG_DTF_MASK            0x00000003
#define I2C_CTL_REG_DTF_WRITE           0x0
#define I2C_CTL_REG_DTF_READ            0x1
#define I2C_CTL_REG_DTF_READ_AND_WRITE  0x2
#define I2C_CTL_REG_DTF_WRITE_AND_READ  0x3
#define I2C_CTL_REG_DEGLITCH_DISABLE    0x00000004
#define I2C_CTL_REG_DELAY_DISABLE       0x00000008
#define I2C_CTL_REG_SCL_SEL_MASK        0x00000030
#define I2C_CTL_REG_SCL_CLK_375KHZ      0x00000000
#define I2C_CTL_REG_SCL_CLK_390KHZ      0x00000010
#define I2C_CTL_REG_SCL_CLK_187_5KHZ    0x00000020
#define I2C_CTL_REG_SCL_CLK_200KHZ      0x00000030
#define I2C_CTL_REG_INT_ENABLE          0x00000040
#define I2C_CTL_REG_DIV_CLK             0x00000080
  uint32        IICEnable;              /* 0x2c */

#define I2C_IIC_ENABLE                  0x00000001
#define I2C_IIC_INTRP                   0x00000002
#define I2C_IIC_NO_ACK                  0x00000004
#define I2C_IIC_NO_STOP                 0x00000010
#define I2C_IIC_NO_START                0x00000020
  uint32        DataOut0;               /* 0x30 */
  uint32        DataOut1;               /* 0x34 */
  uint32        DataOut2;               /* 0x38 */
  uint32        DataOut3;               /* 0x3c */

  uint32        DataOut4;               /* 0x40 */
  uint32        DataOut5;               /* 0x44 */
  uint32        DataOut6;               /* 0x48 */
  uint32        DataOut7;               /* 0x4c */

  uint32        CtlHiReg;               /* 0x50 */
#define I2C_CTLHI_REG_WAIT_DISABLE      0x00000001
#define I2C_CTLHI_REG_IGNORE_ACK        0x00000002
#define I2C_CTLHI_REG_DATA_REG_SIZE     0x00000040
  uint32        SclParam;               /* 0x54 */
} I2CControl;

#define I2C ((volatile I2CControl * const) BCM_IO_ADDR(I2C_BASE))

typedef struct MEMCControl {
	uint32 GLB_VERS;		/* 0x000 */
	uint32 GLB_GCFG;		/* 0x004 */
#define MEMC_GLB_GCFG_DRAM_EN_SHIFT        31
#define MEMC_GLB_GCFG_DRAM_EN_MASK         (0x1<<MEMC_GLB_GCFG_DRAM_EN_SHIFT)
#define MEMC_GLB_GCFG_PHY_4X_SHIFT         24
#define MEMC_GLB_GCFG_PHY_4X_MASK          (0x1<<MEMC_GLB_GCFG_PHY_4X_SHIFT)
#define MEMC_GLB_GCFG_MCLKSRC_SHIFT        9
#define MEMC_GLB_GCFG_MCLKSRC_MASK         (0x1<<MEMC_GLB_GCFG_MCLKSRC_SHIFT)
#define MEMC_GLB_GCFG_MEMINITDONE_SHIFT    8
#define MEMC_GLB_GCFG_MEMINITDONE_MASK     (0x1<<MEMC_GLB_GCFG_MEMINITDONE_SHIFT)
#define MEMC_GLB_GCFG_SIZE1_SHIFT          0
#define MEMC_GLB_GCFG_SIZE1_MASK           (0xf<<MEMC_GLB_GCFG_SIZE1_SHIFT)

	uint32 unused0[2];		/* 0x008-0x00f */  
	uint32 GLB_CFG;			/* 0x010 */
#define MEMC_GLB_CFG_RR_MODE_SHIFT      0x0
#define MEMC_GLB_CFG_RR_MODE_MASK       (0x1<<MEMC_GLB_CFG_RR_MODE_SHIFT)
#define MEMC_GLB_CFG_BURST_MODE_SHIFT   0x1
#define MEMC_GLB_CFG_BURST_MODE_MASK    (0x1<<MEMC_GLB_CFG_BURST_MODE_SHIFT)
	uint32 GLB_QUE_DIS;		/* 0x014 */
	uint32 GLB_SP_SEL;		/* 0x018 */
#define MEMC_GLB_SP_SEL_SELECT_MASK     0x001fffff
	uint32 GLB_SP_PRI_0;		/* 0x01c */
	uint32 GLB_SP_PRI_1;		/* 0x020 */
	uint32 GLB_SP_PRI_2;		/* 0x024 */
	uint32 GLB_SP_PRI_3;		/* 0x028 */
	uint32 GLB_SP_PRI_4;		/* 0x02c */
	uint32 GLB_SP_PRI_5;		/* 0x030 */
	uint32 unused1[3];		/* 0x034-0x3f */
	uint32 GLB_RR_QUANTUM[12];	/* 0x040-0x06c */
	uint32 unused2[4];		/* 0x070-0x07f */

	uint32 INTR2_CPU_STATUS;	/* 0x080 */
	uint32 INTR2_CPU_SET;		/* 0x084 */
	uint32 INTR2_CPU_CLEAR;		/* 0x088 */
	uint32 INTR2_CPU_MASK_STATUS;	/* 0x08c */
	uint32 INTR2_CPU_MASK_SET;	/* 0x090 */
	uint32 INTR2_CPU_MASK_CLEAR;	/* 0x094 */
	uint32 unused3[10];		/* 0x098-0x0bf */

	uint32 SRAM_REMAP_CTRL;		/* 0x0c0 */
	uint32 SRAM_REMAP_INIT;		/* 0x0c4 */
	uint32 SRAM_REMAP_LOG_INFO_0;	/* 0x0c8 */
	uint32 SRAM_REMAP_LOG_INFO_1;	/* 0x0cc */
	uint32 unused4[12];		/* 0x0d0-0x0ff */

	uint32 CHN_CFG_CNFG;		/* 0x100 */
	uint32 CHN_CFG_CSST;		/* 0x104 */
	uint32 CHN_CFG_CSEND;		/* 0x108 */
	uint32 unused5;			/* 0x10c */
	uint32 CHN_CFG_ROW00_0;		/* 0x110 */
	uint32 CHN_CFG_ROW00_1;		/* 0x114 */
	uint32 CHN_CFG_ROW01_0;		/* 0x118 */
	uint32 CHN_CFG_ROW01_1;		/* 0x11c */
	uint32 unused6[4];		/* 0x120-0x12f */
	uint32 CHN_CFG_ROW20_0;		/* 0x130 */
	uint32 CHN_CFG_ROW20_1;		/* 0x134 */
	uint32 CHN_CFG_ROW21_0;		/* 0x138 */
	uint32 CHN_CFG_ROW21_1;		/* 0x13c */
	uint32 unused7[4];		/* 0x140-0x14f */
	uint32 CHN_CFG_COL00_0;		/* 0x150 */
	uint32 CHN_CFG_COL00_1;		/* 0x154 */
	uint32 CHN_CFG_COL01_0;		/* 0x158 */
	uint32 CHN_CFG_COL01_1;		/* 0x15c */
	uint32 unused8[4];		/* 0x160-0x16f */
	uint32 CHN_CFG_COL20_0;		/* 0x170 */
	uint32 CHN_CFG_COL20_1;		/* 0x174 */
	uint32 CHN_CFG_COL21_0;		/* 0x178 */
	uint32 CHN_CFG_COL21_1;		/* 0x17c */
	uint32 unused9[4];		/* 0x180-0x18f */
	uint32 CHN_CFG_BNK10;		/* 0x190 */
	uint32 CHN_CFG_BNK32;		/* 0x194 */
	uint32 unused10[26];		/* 0x198-0x1ff */

	uint32 CHN_TIM_DCMD;		/* 0x200 */
	uint32 CHN_TIM_DMODE_0;		/* 0x204 */
	uint32 CHN_TIM_DMODE_2;		/* 0x208 */
	uint32 CHN_TIM_CLKS;		/* 0x20c */
	uint32 CHN_TIM_ODT;		/* 0x210 */
	uint32 CHN_TIM_TIM1_0;		/* 0x214 */
	uint32 CHN_TIM_TIM1_1;		/* 0x218 */
	uint32 CHN_TIM_TIM2;		/* 0x21c */
	uint32 CHN_TIM_CTL_CRC;		/* 0x220 */
	uint32 CHN_TIM_DOUT_CRC;	/* 0x224 */
	uint32 CHN_TIM_DIN_CRC;		/* 0x228 */
	uint32 CHN_TIM_CRC_CTRL;	/* 0x22c */
	uint32 CHN_TIM_PHY_ST;		/* 0x230 */
#define MEMC_CHN_TIM_PHY_ST_PHY_ST_POWER_UP       0x1
#define MEMC_CHN_TIM_PHY_ST_PHY_ST_HW_RESET       0x2
#define MEMC_CHN_TIM_PHY_ST_PHY_ST_SW_RESET       0x4
#define MEMC_CHN_TIM_PHY_ST_PHY_ST_READY          0x10
	uint32 CHN_TIM_DRAM_CFG;	/* 0x234 */
#define MEMC_CHN_TIM_DRAM_CFG_DRAM_CFG_DRAMTYPE_SHIFT 0x0
#define MEMC_CHN_TIM_DRAM_CFG_DRAM_CFG_DRAMTYPE_MASK  (0xf<<MEMC_CHN_TIM_DRAM_CFG_DRAM_CFG_DRAMTYPE_SHIFT)
	uint32 CHN_TIM_STAT;		/* 0x238 */
	uint32 unused11[49];		/* 0x23c-0x2ff */

	UBUSInterface UBUSIF0;		/* 0x300-0x33f */
	UBUSInterface UBUSIF1;		/* 0x340-0x37f */

	uint32 AXIRIF_0_CFG;		/* 0x380 */
	uint32 AXIRIF_0_REP_ARB_MODE;	/* 0x384 */
#define MEMC_AXIRIF_0_REP_ARB_MODE_FIFO_MODE_SHIFT    0x0
#define MEMC_AXIRIF_0_REP_ARB_MODE_FIFO_MODE_MASK     (0x3<<MEMC_AXIRIF_0_REP_ARB_MODE_FIFO_MODE_SHIFT)
	uint32 AXIRIF_0_SCRATCH;	/* 0x388 */
	uint32 unused12[29];		/* 0x38c-0x3ff */

	uint32 AXIWIF_0_CFG;		/* 0x400 */
#define MEMC_AXIWIF_0_CFG_WRITE_REPLY_MODE_SHIFT      0x0
#define MEMC_AXIWIF_0_CFG_WRITE_REPLY_MODE_MASK       (0x1<<MEMC_AXIWIF_0_CFG_WRITE_REPLY_MODE_SHIFT)
#define MEMC_AXIWIF_0_CFG_WRITE_BURST_MODE_SHIFT      0x1
#define MEMC_AXIWIF_0_CFG_WRITE_BURST_MODE_MASK       (0x1<<MEMC_AXIWIF_0_CFG_WRITE_BURST_MODE_SHIFT)
	uint32 AXIWIF_0_REP_ARB_MODE;	/* 0x404 */
#define MEMC_AXIWIF_0_REP_ARB_MODE_FIFO_MODE_SHIFT    0x0
#define MEMC_AXIWIF_0_REP_ARB_MODE_FIFO_MODE_MASK     (0x1<<MEMC_AXIRIF_0_REP_ARB_MODE_FIFO_MODE_SHIFT)

	uint32 AXIWIF_0_SCRATCH;	/* 0x408 */
	uint32 unused13[61];		/* 0x40c-0x4ff */

	EDISEngine EDIS_0;		/* 0x500 */
	EDISEngine EDIS_1;		/* 0x600 */

	uint32 STATS_CTRL;		/* 0x700 */
	uint32 STATS_TIMER_CFG;		/* 0x704 */
	uint32 STATS_TIMER_COUNT;	/* 0x708 */
	uint32 STATS_TOTAL_SLICE;	/* 0x70c */
	uint32 STATS_TOTAL_PACKET;	/* 0x710 */
	uint32 STATS_SLICE_REORDER;	/* 0x714 */
	uint32 STATS_IDLE_DDR_CYCLE;	/* 0x718 */
	uint32 STATS_ARB_GRANT;		/* 0x71c */
	uint32 STATS_PROG_0;		/* 0x720 */
	uint32 STATS_PROG_1;		/* 0x724 */
	uint32 STATS_ARB_GRANT_MATCH;	/* 0x728 */
	uint32 STATS_CFG_0;		/* 0x72c */
	uint32 STATS_CFG_1;		/* 0x730 */
	uint32 unused14[19];		/* 0x734-0x77f */

	uint32 CAP_CAPTURE_CFG;		/* 0x780 */
	uint32 CAP_TRIGGER_ADDR;	/* 0x784 */
	uint32 CAP_READ_CTRL;		/* 0x788 */
	uint32 unused15;		/* 0x78c */
	uint32 CAP_CAPTURE_MATCH0;	/* 0x790 */
	uint32 CAP_CAPTURE_MATCH1;	/* 0x794 */
	uint32 CAP_CAPTURE_MATCH2;	/* 0x798 */
	uint32 unused16;		/* 0x79c */
	uint32 CAP_CAPTURE_MASK0;	/* 0x7a0 */
	uint32 CAP_CAPTURE_MASK1;	/* 0x7a4 */
	uint32 CAP_CAPTURE_MASK2;	/* 0x7a8 */
	uint32 unused17;		/* 0x7ac */
	uint32 CAP_TRIGGER_MATCH0;	/* 0x7b0 */
	uint32 CAP_TRIGGER_MATCH1;	/* 0x7b4 */
	uint32 CAP_TRIGGER_MATCH2;	/* 0x7b8 */
	uint32 unused18;		/* 0x7bc */
	uint32 CAP_TRIGGER_MASK0;	/* 0x7c0 */
	uint32 CAP_TRIGGER_MASK1;	/* 0x7c4 */
	uint32 CAP_TRIGGER_MASK2;	/* 0x7c8 */
	uint32 unused19;		/* 0x7cc */
	uint32 CAP_READ_DATA[4];	/* 0x7d0-0x7dc */
	uint32 unused20[8];		/* 0x7e0-0x7ff */

	SecureRangeCheckers SEC_RANGE_CHK;	/* 0x800 */

	uint32 SEC_INTR2_CPU_STATUS;	/* 0x900 */
	uint32 SEC_INTR2_CPU_SET;	/* 0x904 */
	uint32 SEC_INTR2_CPU_CLEAR;	/* 0x908 */
	uint32 SEC_INTR2_CPU_MASK_STATUS;	/* 0x90c */
	uint32 SEC_INTR2_CPU_MASK_SET;	/* 0x910 */
	uint32 SEC_INTR2_CPU_MASK_CLEAR;	/* 0x914 */
	uint32 unused21[10];		/* 0x918-0x93f */

	uint32 SEC_SRAM_REMAP_CTRL;	/* 0x940 */
	uint32 SEC_SRAM_REMAP_INIT;	/* 0x944 */
	uint32 SEC_SRAM_REMAP_LOG_0;	/* 0x948 */
	uint32 SEC_SRAM_REMAP_LOG_1;	/* 0x94c */
	uint32 unused22[428];		/* 0x950-0xfff */

	DDRPhyControl PhyControl;	/* 0x1000 */
	DDRPhyByteLaneControl PhyByteLane0Control;	/* 0x1400 */
	DDRPhyByteLaneControl PhyByteLane1Control;	/* 0x1600 */
} MEMCControl;

#define MEMC ((volatile MEMCControl * const) MEMC_BASE)


/*
 * Peripheral Controller
 */
typedef struct PerfControl {
	uint32 RevID;		/* 0x00 */
#define CHIP_ID_SHIFT	12
#define CHIP_ID_MASK	(0xfffff << CHIP_ID_SHIFT)
#define REV_ID_MASK	0xff

	uint32 DiagCtrl;	/* 0x04 */
#define DIAG_SW_EN	(1 << 23)
#define DIAG_CTRL_MASK	(0x001fffff)

#if 0	/* FIXME! do the following reflect DiagCtrl register? */
	uint32 blkEnables;	/* (04) word 1 */
#define ROBOSW250_CLK_EN	(1 << 31)
#define TBUS_CLK_EN		(1 << 27)
#define NAND_CLK_EN		(1 << 20)
//#define SECMIPS_CLK_EN	(1 << 19)
#define GMAC_CLK_EN		(1 << 19)
#define PHYMIPS_CLK_EN		(1 << 18)
#define PCIE_CLK_EN		(1 << 17)
#define HS_SPI_CLK_EN		(1 << 16)
#define SPI_CLK_EN		(1 << 15)
#define IPSEC_CLK_EN		(1 << 14)
#define USBH_CLK_EN		(1 << 13)
#define USBD_CLK_EN		(1 << 12)
#define APM_CLK_EN		(1 << 11)
#define ROBOSW_CLK_EN		(1 << 10)
#define SAR_CLK_EN		(1 << 9)
#define FAP1_CLK_EN		(1 << 8)
#define FAP0_CLK_EN		(1 << 7)
#define DECT_CLK_EN		(1 << 6)
#define WLAN_OCP_CLK_EN		(1 << 5)
#define MIPS_CLK_EN		(1 << 4)
#define VDSL_CLK_EN		(1 << 3)
#define VDSL_AFE_EN		(1 << 2)
#define VDSL_QPROC_EN		(1 << 1)
#define DISABLE_GLESS		(1 << 0)
#endif

	uint32 ExtIrqCtrl;	/* 0x08 */
#define EI_CLEAR_SHFT   0
#define EI_SENSE_SHFT   8
#define EI_INSENS_SHFT  16
#define EI_LEVEL_SHFT   24
	uint32 ExtIrqStatus;	/* 0x0c */
#define EI_MASK_SHFT 16
#define EI_STATUS_SHFT 0
	uint32 IrqStatus[4];	/* 0x10 - 0x1c */

	uint32 IntMaskARM0[4];	/* 0x20 */
	uint32 IntMaskARM1[4];	/* 0x30 */
	uint32 IntMaskPCIE[4];	/* 0x40 */
	uint32 IntMaskPMC[4];	/* 0x50 */
} PerfControl;

#define PERF ((volatile PerfControl * const) PERF_BASE)


/*
 * Timer
 */
typedef struct Timer {
	uint32 TimerCtl0;	/* 0x00 */
	uint32 TimerCtl1;	/* 0x04 */
	uint32 TimerCtl2;	/* 0x08 */
	uint32 TimerCtl3;	/* 0x0c */
#define TIMERENABLE		(1 << 31)
#define RSTCNTCLR		(1 << 30)

	uint32 TimerCnt0;	/* 0x10 */
	uint32 TimerCnt1;	/* 0x14 */
	uint32 TimerCnt2;	/* 0x18 */
	uint32 TimerCnt3;	/* 0x1c */
#define TIMER_COUNT_MASK	0x3FFFFFFF

	uint32 TimerMask;	/* 0x20 */
#define TIMER0EN		(1 << 0)
#define TIMER1EN		(1 << 1)
#define TIMER2EN		(1 << 2)
#define TIMER3EN		(1 << 3)

	uint32 TimerInts;	/* 0x24 */
#define TIMER0			(1 << 0)
#define TIMER1			(1 << 1)
#define TIMER2			(1 << 2)
#define TIMER3			(1 << 3)
#define WATCHDOG		(1 << 4)

	uint32 WatchDogDefCount;	/* 0x28 */

	/* Write 0xff00 0x00ff to Start timer
	 * Write 0xee00 0x00ee to Stop and re-load default count
	 * Read from this register returns current watch dog count
	 */
	uint32 WatchDogCtl;	/* 0x2c */

	/* Number of 50-MHz ticks for WD Reset pulse to last */
	uint32 WDResetCount;	/* 0x30 */
	uint32 SoftRst;	        /* 0x34 */
#define SOFT_RESET              (1 << 0)
	uint32 TimerResetStatus;	/* 0x38 */
} Timer;

#define TIMER ((volatile Timer * const) TIMR_BASE)


/*
 * Gpio Controller
 */
typedef struct GpioControl {
	uint32 GPIODir[5];	/* 0x00-0x10 */
	uint32 GPIOio[5];	/* 0x14-0x24 */
	uint32 SpiSlaveCfg;	/* 0x28 */
	uint32 unused;		/* 0x2c */
	uint32 TestControl;		/* 0x30 */
	uint32 TestPortBlockEnMSB;	/* 0x34 */
	uint32 TestPortBlockEnLSB;	/* 0x38 */
	uint32 TestPortBlockDataMSB;	/* 0x3c */
	uint32 TestPortBlockDataLSB;	/* 0x40 */
#define PINMUX_DATA_SHIFT       12
#define PINMUX_0                0
#define PINMUX_1                1
#define PINMUX_2                2
#define PINMUX_3                3
#define PINMUX_4                4
#define PINMUX_5                5
#define PINMUX_6                6
#define PINMUX_MSPI             PINMUX_0
#define PINMUX_MSPI_SS          PINMUX_1
#define PINMUX_PCM              PINMUX_1
#define PINMUX_GPIO             PINMUX_5
	uint32 TestPortCmd;		/* 0x44 */
#define LOAD_MUX_REG_CMD        0x21
	uint32 GPIOBaseMode;		/* 0x48 */
#define GPIO_BASE_USB_LED_OVERRIDE	(1<<11)
#define GPIO_BASE_VDSL_LED_OVERRIDE	(1<<10)
	uint32 DiagReadBack;		/* 0x4c */
	uint32 DiagReadBackHi;		/* 0x50 */
	uint32 GeneralPurpose;		/* 0x54 */
} GpioControl;

#define GPIO ((volatile GpioControl * const) GPIO_BASE)

/* Number to mask conversion macro used for GPIODir and GPIOio */
#define GPIO_NUM_MAX			160	// FIXME!!
#define GPIO_NUM_TO_ARRAY_IDX(X)	((((X) & BP_GPIO_NUM_MASK) < GPIO_NUM_MAX) ? ((((X) & BP_GPIO_NUM_MASK) >> 5) & 0x07) : (0))
#define GPIO_NUM_TO_MASK(X)		((((X) & BP_GPIO_NUM_MASK) < GPIO_NUM_MAX) ? (1 << (((X) & BP_GPIO_NUM_MASK) & 0x1f)) : (0))
#define GPIO_NUM_TO_ARRAY_SHIFT(X) 	(((X) & BP_GPIO_NUM_MASK) & 0x1f)

/*
 * Misc Register Set Definitions.
 */
typedef struct Misc {
	uint32 miscPCIECtrl;	/* 0x00 */
	uint32 miscStrapBus;	/* 0x04 */
#define MISC_STRAP_BUS_SW_RESERVE_1		(0x3 << 24)
#define MISC_STRAP_BUS_BISR_MEM_REPAIR		(1 << 23)
#define MISC_STRAP_BUS_RESET_OUT_DELAY_SHIFT	22
#define MISC_STRAP_BUS_RESET_OUT_DELAY_MASK	(1 << MISC_STRAP_BUS_RESET_OUT_DELAY_SHIFT)
#define MISC_STRAP_BUS_RESET_OUT_DELAY_100MS	(1 << MISC_STRAP_BUS_RESET_OUT_DELAY_SHIFT)
#define MISC_STRAP_BUS_RESET_OUT_DELAY_50MS	(0x0 << MISC_STRAP_BUS_RESET_OUT_DELAY_SHIFT)
#define MISC_STRAP_BUS_SYS_BUS_FREQ		(0x3 << 20)
#define MISC_STRAP_BUS_A9_CORE0_BOOT		(1 << 19)
#define MISC_STRAP_BUS_PMC_BOOT_FLASH_N		(1 << 18)
#define MISC_STRAP_BUS_PMC_BOOT_AVS		(1 << 17)
#define MISC_STRAP_BUS_HS_SPIM_24B_N_32B_ADDR   (1 << 16)
#define MISC_STRAP_BUS_HS_SPIM_CLK_SLOW_N_FAST  (1 << 15)
#define MISC_STRAP_BUS_SW_RESERVE_0		(0x7 << 12)
#define MISC_STRAP_BUS_B15_START_SLOW_FREQ	(1 << 11)
#define MISC_STRAP_BUS_PMC_ROM_BOOT_SHIFT       10
#define MISC_STRAP_BUS_PMC_ROM_BOOT             (1<<MISC_STRAP_BUS_PMC_ROM_BOOT_SHIFT) /* pmc rom boot enable */
#define MISC_STRAP_BUS_PICO_ROM_BOOT		(1 << 9)
#define MISC_STRAP_BUS_BOOT_SEL_SHIFT		4
#define MISC_STRAP_BUS_BOOT_SEL_MASK		(0x1f << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_OPT_MASK            (0x18 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_SPI_NOR		(0x18 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_SPI_NOR_1_24MHZ	(0x0 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_SPI_NOR_2_54MHZ	(0x1 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_SPI_NOR_2_81MHZ	(0x2 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_SPI_NOR_4_81MHZ	(0x3 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_EMMC		(0x1e << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_NAND_2K_PAGE	(0x00 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_NAND_4K_PAGE	(0x08 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_NAND_8K_PAGE	(0x10 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_SEL_ECC_MASK	(0x7 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_NAND_ECC_DISABLE	(0x0 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_NAND_ECC_1_BIT	(0x1 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_NAND_ECC_4_BIT	(0x2 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_NAND_ECC_8_BIT	(0x3 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_NAND_ECC_12_BIT	(0x4 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_NAND_ECC_24_BIT	(0x5 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_NAND_ECC_40_BIT	(0x6 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_BOOT_NAND_ECC_60_BIT	(0x7 << MISC_STRAP_BUS_BOOT_SEL_SHIFT)
#define MISC_STRAP_BUS_LS_SPI_SLAVE_DISABLE	(1 << 3)
#define MISC_STRAP_BUS_PCIE1_RC_MODE		(1 << 2)
#define MISC_STRAP_BUS_PCIE0_RC_MODE		(1 << 1)
#define MISC_STRAP_BUS_TBUS_DISABLE		(1 << 0)

	uint32 miscStrapOverride;	/* 0x08 */
	uint32 miscAdsl_clock_sample;	/* 0x0c */
	uint32 miscRngCtrl;		/* 0x10 */
	uint32 miscMbox0_data;		/* 0x14 */
	uint32 miscMbox1_data;		/* 0x18 */
	uint32 miscMbox2_data;		/* 0x1c */
	uint32 miscMbox3_data;		/* 0x20 */
	uint32 miscMbox_ctrl;		/* 0x24 */
	uint32 miscxMIIPadCtrl[4];		/* 0x28 */
#define MISC_XMII_PAD_MODEHV                    (1 << 6)
#define MISC_XMII_PAD_SEL_GMII                  (1 << 4)
	uint32 miscxMIIPullCtrl[4];		/* 0x38 */
	uint32 miscRBUSBridgeCtrl;	/* 0x48 */
    uint32 miscSGMIIFiberDetect;    /* 0x4c */
#define MISC_SGMII_FIBER_GPIO36     (1<<0)
} Misc;

#define MISC ((volatile Misc * const) MISC_BASE)


typedef struct Rng {
	uint32 ctrl0;			/* 0x00 */
	uint32 rngSoftReset;		/* 0x04 */
	uint32 rbgSoftReset;		/* 0x08 */
	uint32 totalBitCnt;		/* 0x0c */
	uint32 totalBitCntThreshold;	/* 0x10 */
	uint32 revId;			/* 0x14 */
	uint32 intStatus;		/* 0x18 */
	uint32 intEn;			/* 0x1c */
	uint32 rngFifoData;		/* 0x20 */
	uint32 fifoCnt;			/* 0x24 */
	uint32 perm;			/* 0x28 */
} Rng;

#define RNG ((volatile Rng * const) RNG_BASE)

/*
 * UART Peripheral
 */
typedef struct UartChannel {
	uint32 fifoctl	: 8,	/* 0x00 */
#define RSTTXFIFOS	0x80
#define RSTRXFIFOS	0x40
	/* 5-bit TimeoutCnt is in low bits of this register.
	 *  This count represents the number of characters
	 *  idle times before setting receive Irq when below threshold
	 */
	       config	: 8,
#define XMITBREAK	0x40
#define BITS5SYM	0x00
#define BITS6SYM	0x10
#define BITS7SYM	0x20
#define BITS8SYM	0x30
#define ONESTOP		0x07
#define TWOSTOP		0x0f
	/* 4-LSBS represent STOP bits/char
	 * in 1/8 bit-time intervals.  Zero
	 * represents 1/8 stop bit interval.
	 * Fifteen represents 2 stop bits.
	 */
	       control	: 8,
#define BRGEN		0x80	/* Control register bit defs */
#define TXEN		0x40
#define RXEN		0x20
#define LOOPBK		0x10
#define TXPARITYEN	0x08
#define TXPARITYEVEN	0x04
#define RXPARITYEN	0x02
#define RXPARITYEVEN	0x01
	       unused0	: 8;
	       
	uint32 baudword;	/* 0x04 */
	/* When divide SysClk/2/(1+baudword) we should get 32*bit-rate
	 */

	/* 0x08 */
	uint32 prog_out		: 8,	/* Set value of DTR (Bit0), RTS (Bit1)
					 *  if these bits are also enabled to
					 *  GPIO_o
					 */
#define ARMUARTEN	0x04
#define RTSEN		0x02
#define DTREN		0x01
	       fifocfg		: 8,	/* Upper 4-bits are TxThresh, Lower are
					 * RxThreshold.  Irq can be asserted
					 * when rx fifo> thresh, txfifo<thresh
					 */

	       rxf_levl		: 8,	/* Read-only fifo depth */
	       txf_levl		: 8;	/* Read-only fifo depth */

	/* 0x0c */
	uint32 DeltaIP_SyncIP		: 8,	/* Upper 4 bits show which bits
						 *  have changed (may set IRQ).
						 *  read automatically clears
						 *  bit.
						 *  Lower 4 bits are actual
						 *  status
						 */
	       DeltaIPConfig_Mask	: 8,	/* Upper 4 bits: 1 for posedge
						 * sense 0 for negedge sense if
						 * not configured for edge
						 * insensitive (see above)
						 * Lower 4 bits: Mask to enable
						 * change detection IRQ for
						 * corresponding GPIO_i
						 */
	       DeltaIPEdgeNoSense	: 8,	/* Low 4-bits, set corr bit to
						 * 1 to detect irq on rising
						 * AND falling edges for
						 * corresponding GPIO_i
						 * if enabled (edge insensitive)
						 */
	       unused1		: 8;

	uint32 intStatus	: 16,	/* 0x10 */
#define DELTAIP         0x0001
#define TXUNDERR        0x0002
#define TXOVFERR        0x0004
#define TXFIFOTHOLD     0x0008
#define TXREADLATCH     0x0010
#define TXFIFOEMT       0x0020
#define RXUNDERR        0x0040
#define RXOVFERR        0x0080
#define RXTIMEOUT       0x0100
#define RXFIFOFULL      0x0200
#define RXFIFOTHOLD     0x0400
#define RXFIFONE        0x0800
#define RXFRAMERR       0x1000
#define RXPARERR        0x2000
#define RXBRK           0x4000
	       intMask		: 16;

	uint32 Data	: 16,		/* 0x14  Write to TX, Read from RX */
                                        /* bits 10:8 are BRK,PAR,FRM errors */
	       unused2	: 16;
} Uart;

#define UART ((volatile Uart * const) UART_BASE)
#define UART1 ((volatile Uart *const) UART1_BASE)

/*
 * B15 CFG
 */
typedef struct B15ArchRegion {
	uint32 addr_ulimit;
	uint32 addr_llimit;
	uint32 permission;
	uint32 access_right_ctrl;
} B15ArchRegion;

typedef struct B15Arch {
	B15ArchRegion region[8];
	uint32 unused[95];
	uint32 scratch;
} B15Arch;

typedef struct B15CpuBusRange {
#define ULIMIT_SHIFT 4
#define BUSNUM_MASK 0x0000000FU

#define BUSNUM_UBUS 1
#define BUSNUM_RBUS 2
#define BUSNUM_RSVD 3
#define BUSNUM_MCP0 4
#define BUSNUM_MCP1 5
#define BUSNUM_MCP2 6

	uint32 ulimit;
	uint32 llimit;
} B15CpuBusRange;

typedef struct B15CpuAccessRightViol {
	uint32 addr;
	uint32 upper_addr;
	uint32 detail_addr;
} B15CpuAccessRightViol;

typedef struct B15CpuBPCMAVS {
	uint32 bpcm_id;
	uint32 bpcm_capability;
	uint32 bpcm_ctrl;
	uint32 bpcm_status;
	uint32 avs_rosc_ctrl;
	uint32 avs_rosc_threshold;
	uint32 avs_rosc_cnt;
	uint32 avs_pwd_ctrl;
} B15CpuBPCMAVS;

typedef struct B15CpuCtrl {
	B15CpuBusRange bus_range[11];	/* 0x0 */
	uint32 secure_reset_hndshake;
	uint32 secure_soft_reset;
	B15CpuAccessRightViol access_right_viol[2];	/* 0x60 */
	uint32 rac_cfg0;
	uint32 rac_cfg1;
	uint32 rac_flush;		/* 0x80 */
	uint32 cpu_power_cfg;
	uint32 cpu0_pwr_zone_ctrl;
	uint32 cpu1_pwr_zone_ctrl;
	uint32 cpu2_pwr_zone_ctrl;	/* 0x90 */
	uint32 cpu3_pwr_zone_ctrl;
	uint32 l2biu_pwr_zone_ctrl;
	uint32 cpu0_pwr_zone_cfg1;
	uint32 cpu0_pwr_zone_cfg2;	/* 0xa0 */
	uint32 cpu1_pwr_zone_cfg1;
	uint32 cpu1_pwr_zone_cfg2;
	uint32 cpu2_pwr_zone_cfg1;
	uint32 cpu2_pwr_zone_cfg2;	/* 0xb0 */
	uint32 cpu3_pwr_zone_cfg1;
	uint32 cpu3_pwr_zone_cfg2;
	uint32 l2biu_pwr_zone_cfg1;
	uint32 l2biu_pwr_zone_cfg2;	/* 0xc0 */
	uint32 cpu0_pwr_freq_scalar_ctrl;
	uint32 cpu1_pwr_freq_scalar_ctrl;
	uint32 cpu2_pwr_freq_scalar_ctrl;
	uint32 cpu3_pwr_freq_scalar_ctrl;	/* 0xd0 */
	uint32 l2biu_pwr_freq_scalar_ctrl;
	B15CpuBPCMAVS cpu_bpcm_avs[4];	/* 0xd8 */
	B15CpuBPCMAVS l2biu_bpcm_avs;	/* 0x158 */
	uint32 reset_cfg;		/* 0x178 */
	uint32 clock_cfg;
	uint32 misc_cfg;		/* 0x180 */
	uint32 credit;
	uint32 therm_throttle_temp;
	uint32 term_throttle_irq_cfg;
	uint32 therm_irq_high;		/* 0x190 */
	uint32 therm_irq_low;
	uint32 therm_misc_threshold;
	uint32 therm_irq_misc;
	uint32 defeature;		/* 0x1a0 */
	uint32 defeature_key;
	uint32 debug_rom_addr;
	uint32 debug_self_addr;
	uint32 debug_tracectrl;		/* 0x1b0 */
	uint32 axi_cfg;
	uint32 revision;
	uint32 ubus_cfg_window[8];	/* 0x1bc */
	uint32 ubus_cfg;		/* 0x1dc */
	uint32 unused[135];
	uint32 scratch;			/* 0x3fc */
} B15CpuCtrl;

typedef struct B15Ctrl {
	uint32 unused0[1024];
	B15Arch arch;			/* 0x1000 */
	uint32 unused1[896];
	B15CpuCtrl cpu_ctrl;		/* 0x2000 */
} B15Ctrl;

#define B15CTRL ((volatile B15Ctrl *const) B15_CTRL_BASE)

/*
 * LedControl Register Set Definitions.
 */
typedef struct LedControl {
	uint32 glbCtrl;		/* 0x00 */
	uint32 mask;		/* 0x04 */
	uint32 hWLedEn;		/* 0x08 */
	uint32 serialLedShiftSel; /* 0x0c */
	uint32 flashRateCtrl[4];	/* 0x10-0x1c */
	uint32 brightCtrl[4];	/* 0x20-0x2c */
	uint32 powerLedCfg;	/* 0x30 */
	uint32 pledLut[2][16];	/* 0x34-0x70, 0x74-0xb0 */
        uint32 HwPolarity;      /* 0xb4 */
	uint32 SwData;          /* 0xb8 */
	uint32 SwPolarity;      /* 0xbc */
	uint32 ParallelLedPolarity;    /* 0xc0 */
	uint32 SerialLedPolarity;
	uint32 HwLedStatus;
} LedControl;

#define LED_NUM_LEDS              32
#define LED_NUM_PWM_LEDS              2
#define LED ((volatile LedControl * const) LED_BASE)
#define LED_NUM_TO_MASK(X)	 (1 << ((X) & (LED_NUM_LEDS-1)))


typedef struct I2s {
   uint32 cfg;              /* 0x00 Config Register    */
#define I2S_ENABLE               (1 << 31)    
#define I2S_MCLK_RATE_SHIFT      20 
#define I2S_OUT_R                (1 << 19)    
#define I2S_OUT_L                (1 << 18)    
#define I2S_CLKSEL_SHIFT         16 
#define I2S_CLK_100MHZ           0
#define I2S_CLK_50MHZ            1
#define I2S_CLK_25MHZ            2
#define I2S_MCLK_CLKSEL_CLR_MASK 0xFF0CFFFF
#define I2S_BITS_PER_SAMPLE_SHIFT 10 
#define I2S_BITS_PER_SAMPLE_32   0 
#define I2S_BITS_PER_SAMPLE_24   24
#define I2S_BITS_PER_SAMPLE_20   20
#define I2S_BITS_PER_SAMPLE_18   18
#define I2S_BITS_PER_SAMPLE_16   16
#define I2S_SCLK_POLARITY        (1 << 9)    
#define I2S_LRCK_POLARITY        (1 << 8)    
#define I2S_SCLKS_PER_1FS_DIV32_SHIFT  4 
#define I2S_DATA_JUSTIFICATION   (1 << 3)    
#define I2S_DATA_ALIGNMENT       (1 << 2)    
#define I2S_DATA_ENABLE          (1 << 1)    
#define I2S_CLOCK_ENABLE         (1 << 0)
   
   uint32 unused[3];        /* 0x04 - 0x0c */  
   uint32 intr;             /* 0x10 Interrupt Register */
#define I2S_DESC_OFF_LEVEL_SHIFT    12
#define I2S_DESC_IFF_LEVEL_SHIFT    8    
#define I2S_DESC_LEVEL_MASK         0x0F
#define I2S_DESC_OFF_OVERRUN_INTR   (1 << 3)
#define I2S_DESC_IFF_UNDERRUN_INTR  (1 << 2)
#define I2S_DESC_OFF_INTR           (1 << 1)
#define I2S_DESC_IFF_INTR           (1 << 0)
#define I2S_INTR_MASK               0x0F
   
   uint32 intr_en;          /* 0x14 Interrupt Enables Register */
#define I2S_DESC_INTR_TYPE_SEL        (1 << 4)
#define I2S_DESC_OFF_OVERRUN_INTR_EN  (1 << 3)
#define I2S_DESC_IFF_UNDERRUN_INTR_EN (1 << 2)
#define I2S_DESC_OFF_INTR_EN          (1 << 1)
#define I2S_DESC_IFF_INTR_EN          (1 << 0)
   
   uint32 intr_iff_thld;    /* 0x18 Descriptor Input FIFO Interrupt Threshold Register  */
   uint32 intr_off_thld;    /* 0x1c Descriptor Output FIFO Interrupt Threshold Register */
#define I2S_DESC_IFF_INTR_THLD_MASK    0x07
   
   uint32 desc_iff_addr;    /* 0x20 Descriptor Input FIFO Address  */
   uint32 desc_iff_len;     /* 0x24 Descriptor Input FIFO Length   */
   uint32 desc_off_addr;    /* 0x28 Descriptor Output FIFO Address */
   uint32 desc_off_len;     /* 0x2c Descriptor Output FIFO Length  */
#define I2S_DESC_EOP             (1 << 31)                                 
#define I2S_DESC_FIFO_DEPTH      8
#define I2S_DMA_BUFF_MAX_LEN     0xFFFF
#define I2S_DESC_LEN_MASK        I2S_DMA_BUFF_MAX_LEN
   
} I2s;

#define I2S ((volatile I2s * const) I2S_BASE)


/*
** High-Speed SPI Controller
*/
#define __mask(end, start)      (((1 << ((end - start) + 1)) - 1) << start)
typedef struct HsSpiControl {
	uint32 hs_spiGlobalCtrl;	/* 0x00 */
#define HS_SPI_MOSI_IDLE	(1 << 18)
#define HS_SPI_CLK_POLARITY	(1 << 17)
#define HS_SPI_CLK_GATE_SSOFF	(1 << 16)
#define HS_SPI_PLL_CLK_CTRL	(8)
#define HS_SPI_PLL_CLK_CTRL_MASK	__mask(15, HS_SPI_PLL_CLK_CTRL)
#define HS_SPI_SS_POLARITY	(0)
#define HS_SPI_SS_POLARITY_MASK		__mask(7, HS_SPI_SS_POLARITY)

	uint32 hs_spiExtTrigCtrl;	/* 0x04 */
#define HS_SPI_TRIG_RAW_STATE	(24)
#define HS_SPI_TRIG_RAW_STATE_MASK	__mask(31, HS_SPI_TRIG_RAW_STATE)
#define HS_SPI_TRIG_LATCHED	(16)
#define HS_SPI_TRIG_LATCHED_MASK	__mask(23, HS_SPI_TRIG_LATCHED)
#define HS_SPI_TRIG_SENSE	(8)
#define HS_SPI_TRIG_SENSE_MASK		__mask(15, HS_SPI_TRIG_SENSE)
#define HS_SPI_TRIG_TYPE	(0)
#define HS_SPI_TRIG_TYPE_MASK		__mask(7, HS_SPI_TRIG_TYPE)
#define HS_SPI_TRIG_TYPE_EDGE	(0)
#define HS_SPI_TRIG_TYPE_LEVEL	(1)

	uint32 hs_spiIntStatus;		/* 0x08 */
#define HS_SPI_IRQ_PING1_USER	(28)
#define HS_SPI_IRQ_PING1_USER_MASK	__mask(31, HS_SPI_IRQ_PING1_USER)
#define HS_SPI_IRQ_PING0_USER	(24)
#define HS_SPI_IRQ_PING0_USER_MASK	__mask(27, HS_SPI_IRQ_PING0_USER)

#define HS_SPI_IRQ_PING1_CTRL_INV	(1 << 12)
#define HS_SPI_IRQ_PING1_POLL_TOUT	(1 << 11)
#define HS_SPI_IRQ_PING1_TX_UNDER	(1 << 10)
#define HS_SPI_IRQ_PING1_RX_OVER	(1 << 9)
#define HS_SPI_IRQ_PING1_CMD_DONE	(1 << 8)

#define HS_SPI_IRQ_PING0_CTRL_INV	(1 << 4)
#define HS_SPI_IRQ_PING0_POLL_TOUT	(1 << 3)
#define HS_SPI_IRQ_PING0_TX_UNDER	(1 << 2)
#define HS_SPI_IRQ_PING0_RX_OVER	(1 << 1)
#define HS_SPI_IRQ_PING0_CMD_DONE	(1 << 0)

	uint32 hs_spiIntStatusMasked;	/* 0x0C */
#define HS_SPI_IRQSM__PING1_USER	(28)
#define HS_SPI_IRQSM__PING1_USER_MASK	__mask(31, HS_SPI_IRQSM__PING1_USER)
#define HS_SPI_IRQSM__PING0_USER	(24)
#define HS_SPI_IRQSM__PING0_USER_MASK	__mask(27, HS_SPI_IRQSM__PING0_USER)

#define HS_SPI_IRQSM__PING1_CTRL_INV	(1 << 12)
#define HS_SPI_IRQSM__PING1_POLL_TOUT	(1 << 11)
#define HS_SPI_IRQSM__PING1_TX_UNDER	(1 << 10)
#define HS_SPI_IRQSM__PING1_RX_OVER	(1 << 9)
#define HS_SPI_IRQSM__PING1_CMD_DONE	(1 << 8)

#define HS_SPI_IRQSM__PING0_CTRL_INV	(1 << 4)
#define HS_SPI_IRQSM__PING0_POLL_TOUT	(1 << 3)
#define HS_SPI_IRQSM__PING0_TX_UNDER	(1 << 2)
#define HS_SPI_IRQSM__PING0_RX_OVER	(1 << 1)
#define HS_SPI_IRQSM__PING0_CMD_DONE	(1 << 0)

	uint32 hs_spiIntMask;		/* 0x10 */
#define HS_SPI_IRQM_PING1_USER		(28)
#define HS_SPI_IRQM_PING1_USER_MASK	__mask(31, HS_SPI_IRQM_PING1_USER)
#define HS_SPI_IRQM_PING0_USER		(24)
#define HS_SPI_IRQM_PING0_USER_MASK	__mask(27, HS_SPI_IRQM_PING0_USER)

#define HS_SPI_IRQM_PING1_CTRL_INV	(1 << 12)
#define HS_SPI_IRQM_PING1_POLL_TOUT	(1 << 11)
#define HS_SPI_IRQM_PING1_TX_UNDER	(1 << 10)
#define HS_SPI_IRQM_PING1_RX_OVER	(1 << 9)
#define HS_SPI_IRQM_PING1_CMD_DONE	(1 << 8)

#define HS_SPI_IRQM_PING0_CTRL_INV	(1 << 4)
#define HS_SPI_IRQM_PING0_POLL_TOUT	(1 << 3)
#define HS_SPI_IRQM_PING0_TX_UNDER	(1 << 2)
#define HS_SPI_IRQM_PING0_RX_OVER	(1 << 1)
#define HS_SPI_IRQM_PING0_CMD_DONE	(1 << 0)

#define HS_SPI_INTR_CLEAR_ALL		(0xFF001F1F)

	uint32 hs_spiFlashCtrl;		/* 0x14 */
#define HS_SPI_FCTRL_MB_ENABLE		(23)
#define HS_SPI_FCTRL_SS_NUM		(20)
#define HS_SPI_FCTRL_SS_NUM_MASK	__mask(22, HS_SPI_FCTRL_SS_NUM)
#define HS_SPI_FCTRL_PROFILE_NUM	(16)
#define HS_SPI_FCTRL_PROFILE_NUM_MASK	__mask(18, HS_SPI_FCTRL_PROFILE_NUM)
#define HS_SPI_FCTRL_DUMMY_BYTES	(10)
#define HS_SPI_FCTRL_DUMMY_BYTES_MASK	__mask(11, HS_SPI_FCTRL_DUMMY_BYTES)
#define HS_SPI_FCTRL_ADDR_BYTES		(8)
#define HS_SPI_FCTRL_ADDR_BYTES_MASK	__mask(9, HS_SPI_FCTRL_ADDR_BYTES)
#define HS_SPI_FCTRL_ADDR_BYTES_2	(0)
#define HS_SPI_FCTRL_ADDR_BYTES_3	(1)
#define HS_SPI_FCTRL_ADDR_BYTES_4	(2)
#define HS_SPI_FCTRL_READ_OPCODE	(0)
#define HS_SPI_FCTRL_READ_OPCODE_MASK	__mask(7, HS_SPI_FCTRL_READ_OPCODE)

	uint32 hs_spiFlashAddrBase;	/* 0x18 */
} HsSpiControl;

typedef struct HsSpiPingPong {
	uint32 command;		/* 0x00 */
#define HS_SPI_SS_NUM		(12)
#define ZSI_SPI_DEV_ID     7     // SS_N[7] connected to APM/PCM block for use by MSIF/ZDS interfaces
#define HS_SPI_PROFILE_NUM	(8)
#define HS_SPI_TRIGGER_NUM	(4)
#define HS_SPI_COMMAND_VALUE	(0)
    #define HS_SPI_COMMAND_NOOP		(0)
    #define HS_SPI_COMMAND_START_NOW	(1)
    #define HS_SPI_COMMAND_START_TRIGGER (2)
    #define HS_SPI_COMMAND_HALT		(3)
    #define HS_SPI_COMMAND_FLUSH	(4)

	uint32 status;		/* 0x04 */
#define HS_SPI_ERROR_BYTE_OFFSET	(16)
#define HS_SPI_WAIT_FOR_TRIGGER		(2)
#define HS_SPI_SOURCE_BUSY		(1)
#define HS_SPI_SOURCE_GNT		(0)

	uint32 fifo_status;	/* 0x08 */
	uint32 control;		/* 0x0c */
} HsSpiPingPong;

typedef struct HsSpiProfile {
	uint32 clk_ctrl;	/* 0x00 */
#define HS_SPI_ACCUM_RST_ON_LOOP	(15)
#define HS_SPI_SPI_CLK_2X_SEL		(14)
#define HS_SPI_FREQ_CTRL_WORD		(0)

	uint32 signal_ctrl;	/* 0x04 */
#define	HS_SPI_ASYNC_INPUT_PATH	(1 << 16)
#define	HS_SPI_LAUNCH_RISING	(1 << 13)
#define	HS_SPI_LATCH_RISING	(1 << 12)

	uint32 mode_ctrl;	/* 0x08 */
#define HS_SPI_PREPENDBYTE_CNT		(24)
#define HS_SPI_MODE_ONE_WIRE		(20)
#define HS_SPI_MULTIDATA_WR_SIZE	(18)
#define HS_SPI_MULTIDATA_RD_SIZE	(16)
#define HS_SPI_MULTIDATA_WR_STRT	(12)
#define HS_SPI_MULTIDATA_RD_STRT	(8)
#define HS_SPI_FILLBYTE			(0)

	uint32 polling_config;	/* 0x0c */
	uint32 polling_and_mask;	/* 0x10 */
	uint32 polling_compare;	/* 0x14 */
	uint32 polling_timeout;	/* 0x18 */
	uint32 reserved;	/* 0x1c */

} HsSpiProfile;

#define HS_SPI_OP_CODE 13
    #define HS_SPI_OP_SLEEP (0)
    #define HS_SPI_OP_READ_WRITE (1)
    #define HS_SPI_OP_WRITE (2)
    #define HS_SPI_OP_READ (3)
    #define HS_SPI_OP_SETIRQ (4)

#define HS_SPI ((volatile HsSpiControl * const) HSSPIM_BASE)
#define HS_SPI_PINGPONG0 ((volatile HsSpiPingPong * const) (HSSPIM_BASE + 0x80))
#define HS_SPI_PINGPONG1 ((volatile HsSpiPingPong * const) (HSSPIM_BASE + 0xc0))
#define HS_SPI_PROFILES ((volatile HsSpiProfile * const) (HSSPIM_BASE + 0x100))
#define HS_SPI_FIFO0 ((volatile uint8 * const) (HSSPIM_BASE + 0x200))
#define HS_SPI_FIFO1 ((volatile uint8 * const) (HSSPIM_BASE + 0x400))


/*
** NAND Controller Registers
*/
typedef struct NandCtrlRegs {
	uint32 NandRevision;	/* 0x00 */
	uint32 NandCmdStart;	/* 0x04 */
#define NCMD_MASK		0x0000001f
#define NCMD_BLOCK_ERASE_MULTI	0x15
#define NCMD_PROGRAM_PAGE_MULTI	0x13
#define NCMD_STS_READ_MULTI	0x12
#define NCMD_PAGE_READ_MULTI	0x11
#define NCMD_LOW_LEVEL_OP	0x10
#define NCMD_PARAM_CHG_COL	0x0f
#define NCMD_PARAM_READ		0x0e
#define NCMD_BLK_LOCK_STS	0x0d
#define NCMD_BLK_UNLOCK		0x0c
#define NCMD_BLK_LOCK_DOWN	0x0b
#define NCMD_BLK_LOCK		0x0a
#define NCMD_FLASH_RESET	0x09
#define NCMD_BLOCK_ERASE	0x08
#define NCMD_DEV_ID_READ	0x07
#define NCMD_COPY_BACK		0x06
#define NCMD_PROGRAM_SPARE	0x05
#define NCMD_PROGRAM_PAGE	0x04
#define NCMD_STS_READ		0x03
#define NCMD_SPARE_READ		0x02
#define NCMD_PAGE_READ		0x01

	uint32 NandCmdExtAddr;	/* 0x08 */
	uint32 NandCmdAddr;	/* 0x0c */
	uint32 NandCmdEndAddr;	/* 0x10 */
	uint32 NandIntfcStatus;	/* 0x14 */
#define NIS_CTLR_READY		(1 << 31)
#define NIS_FLASH_READY		(1 << 30)
#define NIS_CACHE_VALID		(1 << 29)
#define NIS_SPARE_VALID		(1 << 28)
#define NIS_FLASH_STS_MASK	0x000000ff
#define NIS_WRITE_PROTECT	0x00000080
#define NIS_DEV_READY		0x00000040
#define NIS_PGM_ERASE_ERROR	0x00000001


	uint32 NandNandBootConfig;	/* 0x18 */
#define NBC_CS_LOCK		(1 << 31)
#define NBC_AUTO_DEV_ID_CFG	(1 << 30)
#define NBC_WR_PROT_BLK0	(1 << 28)
#define NBC_EBI_CS7_USES_NAND	(1<<15)
#define NBC_EBI_CS6_USES_NAND	(1<<14)
#define NBC_EBI_CS5_USES_NAND	(1<<13)
#define NBC_EBI_CS4_USES_NAND	(1<<12)
#define NBC_EBI_CS3_USES_NAND	(1<<11)
#define NBC_EBI_CS2_USES_NAND	(1<<10)
#define NBC_EBI_CS1_USES_NAND	(1<< 9)
#define NBC_EBI_CS0_USES_NAND	(1<< 8)
#define NBC_EBC_CS7_SEL		(1<< 7)
#define NBC_EBC_CS6_SEL		(1<< 6)
#define NBC_EBC_CS5_SEL		(1<< 5)
#define NBC_EBC_CS4_SEL		(1<< 4)
#define NBC_EBC_CS3_SEL		(1<< 3)
#define NBC_EBC_CS2_SEL		(1<< 2)
#define NBC_EBC_CS1_SEL		(1<< 1)
#define NBC_EBC_CS0_SEL		(1<< 0)

	uint32 NandCsNandXor;		/* 0x1c */
	uint32 NandLlOpNand;            /* 0x20 */
	uint32 NandMplaneBaseExtAddr;	/* 0x24 */
	uint32 NandMplaneBaseAddr;	/* 0x28 */
	uint32 NandReserved1[9];	/* 0x2c-0x4f */
	uint32 NandAccControl;		/* 0x50 */
#define NAC_RD_ECC_EN		(1 << 31)
#define NAC_WR_ECC_EN		(1 << 30)
#define NAC_CE_CARE_EN          (1 << 28)
#define NAC_RD_ERASED_ECC_EN	(1 << 27)
#define NAC_PARTIAL_PAGE_EN	(1 << 26)
#define NAC_WR_PREEMPT_EN	(1 << 25)
#define NAC_PAGE_HIT_EN		(1 << 24)
#define NAC_PREFETCH_EN 	(1 << 23)
#define NAC_CACHE_MODE_EN	(1 << 22)
#define NAC_ECC_LVL_SHIFT	16
#define NAC_ECC_LVL_MASK	0x001f0000
#define NAC_ECC_LVL_DISABLE 0
#define NAC_ECC_LVL_BCH_1   1
#define NAC_ECC_LVL_BCH_2   2
#define NAC_ECC_LVL_BCH_3   3
#define NAC_ECC_LVL_BCH_4   4
#define NAC_ECC_LVL_BCH_5   5
#define NAC_ECC_LVL_BCH_6   6
#define NAC_ECC_LVL_BCH_7   7
#define NAC_ECC_LVL_BCH_8   8
#define NAC_ECC_LVL_BCH_9   9
#define NAC_ECC_LVL_BCH_10  10
#define NAC_ECC_LVL_BCH_11  11
#define NAC_ECC_LVL_BCH_12  12
#define NAC_ECC_LVL_BCH_13  13
#define NAC_ECC_LVL_BCH_14  14
#define NAC_ECC_LVL_HAMMING 15  /* Hamming if spare are size = 16, BCH15 otherwise */
#define NAC_ECC_LVL_BCH15   15    
#define NAC_ECC_LVL_BCH_16  16
#define NAC_ECC_LVL_BCH_17  17
/* BCH18 to 30 for sector size = 1K. To be added when we need it */
#define NAC_SECTOR_SIZE_1K	(1 << 7)
#define NAC_SPARE_SZ_SHIFT	0
#define NAC_SPARE_SZ_MASK	0x0000007f

	uint32 NandConfigExt;		/* 0x54 */ /* Nand Flash Config Ext*/
#define NC_BLK_SIZE_MASK	(0xff << 4)
#define NC_BLK_SIZE_8192K	(0xa << 4)
#define NC_BLK_SIZE_4096K	(0x9 << 4)
#define NC_BLK_SIZE_2048K	(0x8 << 4)
#define NC_BLK_SIZE_1024K	(0x7 << 4)
#define NC_BLK_SIZE_512K	(0x6 << 4)
#define NC_BLK_SIZE_256K	(0x5 << 4)
#define NC_BLK_SIZE_128K	(0x4 << 4)
#define NC_BLK_SIZE_64K	        (0x3 << 4)
#define NC_BLK_SIZE_32K	        (0x2 << 4)
#define NC_BLK_SIZE_16K		(0x1 << 4)
#define NC_BLK_SIZE_8K		(0x0 << 4)
#define NC_PG_SIZE_MASK		(0xf << 0)
#define NC_PG_SIZE_16K		(0x5 << 0)
#define NC_PG_SIZE_8K		(0x4 << 0)
#define NC_PG_SIZE_4K		(0x3 << 0)
#define NC_PG_SIZE_2K		(0x2 << 0)
#define NC_PG_SIZE_1K		(0x1 << 0)
#define NC_PG_SIZE_512B		(0x0 << 0)

	uint32 NandConfig;		/* 0x58 */ /* Nand Flash Config */
#define NC_CONFIG_LOCK		(1 << 31)
#define NC_DEV_SIZE_SHIFT	24
#define NC_DEV_SIZE_MASK	(0x0f << NC_DEV_SIZE_SHIFT)
#define NC_DEV_WIDTH_MASK	(1 << 23)
#define NC_DEV_WIDTH_16		(1 << 23)
#define NC_DEV_WIDTH_8		(0 << 23)
#define NC_FUL_ADDR_SHIFT	16
#define NC_FUL_ADDR_MASK	(0x7 << NC_FUL_ADDR_SHIFT)
#define NC_BLK_ADDR_SHIFT	8
#define NC_BLK_ADDR_MASK	(0x07 << NC_BLK_ADDR_SHIFT)

	uint32 NandTiming1;	/* 0x5c */ /* Nand Flash Timing Parameters 1 */
	uint32 NandTiming2;	/* 0x60 */ /* Nand Flash Timing Parameters 2 */
	/* 0x64 */
	uint32 NandAccControlCs1;	/* Nand Flash Access Control */
	uint32 NandConfigExtCs1;	/* Nand Flash Config Ext*/
	uint32 NandConfigCs1;		/* Nand Flash Config */
	uint32 NandTiming1Cs1;		/* Nand Flash Timing Parameters 1 */
	uint32 NandTiming2Cs1;		/* Nand Flash Timing Parameters 2 */
	/* 0x78 */
	uint32 NandAccControlCs2;	/* Nand Flash Access Control */
	uint32 NandConfigExtCs2;	/* Nand Flash Config Ext*/
	uint32 NandConfigCs2;		/* Nand Flash Config */
	uint32 NandTiming1Cs2;		/* Nand Flash Timing Parameters 1 */
	uint32 NandTiming2Cs2;		/* Nand Flash Timing Parameters 2 */
	/* 0x8c */
	uint32 NandAccControlCs3;	/* Nand Flash Access Control */
	uint32 NandConfigExtCs3;	/* Nand Flash Config Ext*/
	uint32 NandConfigCs3;		/* Nand Flash Config */
	uint32 NandTiming1Cs3;		/* Nand Flash Timing Parameters 1 */
	uint32 NandTiming2Cs3;		/* Nand Flash Timing Parameters 2 */
	/* 0xa0 */
	uint32 NandAccControlCs4;	/* Nand Flash Access Control */
	uint32 NandConfigExtCs4;	/* Nand Flash Config Ext*/
	uint32 NandConfigCs4;		/* Nand Flash Config */
	uint32 NandTiming1Cs4;		/* Nand Flash Timing Parameters 1 */
	uint32 NandTiming2Cs4;		/* Nand Flash Timing Parameters 2 */
	/* 0xb4 */
	uint32 NandAccControlCs5;	/* Nand Flash Access Control */
	uint32 NandConfigExtCs5;	/* Nand Flash Config Ext*/
	uint32 NandConfigCs5;		/* Nand Flash Config */
	uint32 NandTiming1Cs5;		/* Nand Flash Timing Parameters 1 */
	uint32 NandTiming2Cs5;		/* Nand Flash Timing Parameters 2 */
	/* 0xc8 */
	uint32 NandAccControlCs6;	/* Nand Flash Access Control */
	uint32 NandConfigExtCs6;	/* Nand Flash Config Ext*/
	uint32 NandConfigCs6;		/* Nand Flash Config */
	uint32 NandTiming1Cs6;		/* Nand Flash Timing Parameters 1 */
	uint32 NandTiming2Cs6;		/* Nand Flash Timing Parameters 2 */

	/* 0xdc */
	uint32 NandCorrStatThreshold;	/* Correctable Error Reporting Threshold */
	uint32 NandCorrStatThresholdExt;	/* Correctable Error Reporting
						 * Threshold */
	uint32 NandBlkWrProtect;	/* Block Write Protect Enable and Size */
					/*   for EBI_CS0b */
	uint32 NandMplaneOpcode1;

	/* 0xec */
	uint32 NandMplaneOpcode2;
	uint32 NandMplaneCtrl;
	uint32 NandReserved2[2];	/* 0xf4-0xfb */
	uint32 NandUncorrErrorCount;	/* 0xfc */

	/* 0x100 */
	uint32 NandCorrErrorCount;
	uint32 NandReadErrorCount;	/* Read Error Count */
	uint32 NandBlockLockStatus;	/* Nand Flash Block Lock Status */
	uint32 NandEccCorrExtAddr;	/* ECC Correctable Error Extended Address*/
	/* 0x110 */
	uint32 NandEccCorrAddr;		/* ECC Correctable Error Address */
	uint32 NandEccUncExtAddr;	/* ECC Uncorrectable Error Extended Addr */
	uint32 NandEccUncAddr;		/* ECC Uncorrectable Error Address */
	uint32 NandFlashReadExtAddr;	/* Flash Read Data Extended Address */
	/* 0x120 */
	uint32 NandFlashReadAddr;	/* Flash Read Data Address */
	uint32 NandProgramPageExtAddr;	/* Page Program Extended Address */
	uint32 NandProgramPageAddr;	/* Page Program Address */
	uint32 NandCopyBackExtAddr;	/* Copy Back Extended Address */
	/* 0x130 */
	uint32 NandCopyBackAddr;	/* Copy Back Address */
	uint32 NandBlockEraseExtAddr;	/* Block Erase Extended Address */
	uint32 NandBlockEraseAddr;	/* Block Erase Address */
	uint32 NandInvReadExtAddr;	/* Flash Invalid Data Extended Address */
	/* 0x140 */
	uint32 NandInvReadAddr;		/* Flash Invalid Data Address */
	uint32 NandInitStatus;
	uint32 NandOnfiStatus;		/* ONFI Status */
	uint32 NandOnfiDebugData;	/* ONFI Debug Data */

	uint32 NandSemaphore;		/* 0x150 */ /* Semaphore */
	uint32 NandReserved3[16];	/* 0x154-0x193 */

	/* 0x194 */
	uint32 NandFlashDeviceId;	/* Nand Flash Device ID */
	uint32 NandFlashDeviceIdExt;	/* Nand Flash Extended Device ID */
	uint32 NandLlRdData;		/* Nand Flash Low Level Read Data */

	uint32 NandReserved4[24];	/* 0x1a0 - 0x1ff */

	/* 0x200 */
	uint32 NandSpareAreaReadOfs0;	/* Nand Flash Spare Area Read Bytes 0-3 */
	uint32 NandSpareAreaReadOfs4;	/* Nand Flash Spare Area Read Bytes 4-7 */
	uint32 NandSpareAreaReadOfs8;	/* Nand Flash Spare Area Read Bytes 8-11 */
	uint32 NandSpareAreaReadOfsC;	/* Nand Flash Spare Area Read Bytes 12-15*/
	/* 0x210 */
	uint32 NandSpareAreaReadOfs10;	/* Nand Flash Spare Area Read Bytes 16-19 */
	uint32 NandSpareAreaReadOfs14;	/* Nand Flash Spare Area Read Bytes 20-23 */
	uint32 NandSpareAreaReadOfs18;	/* Nand Flash Spare Area Read Bytes 24-27 */
	uint32 NandSpareAreaReadOfs1C;	/* Nand Flash Spare Area Read Bytes 28-31*/
	/* 0x220 */
	uint32 NandSpareAreaReadOfs20;	/* Nand Flash Spare Area Read Bytes 32-35 */
	uint32 NandSpareAreaReadOfs24;	/* Nand Flash Spare Area Read Bytes 36-39 */
	uint32 NandSpareAreaReadOfs28;	/* Nand Flash Spare Area Read Bytes 40-43 */
	uint32 NandSpareAreaReadOfs2C;	/* Nand Flash Spare Area Read Bytes 44-47*/
	/* 0x230 */
	uint32 NandSpareAreaReadOfs30;	/* Nand Flash Spare Area Read Bytes 48-51 */
	uint32 NandSpareAreaReadOfs34;	/* Nand Flash Spare Area Read Bytes 52-55 */
	uint32 NandSpareAreaReadOfs38;	/* Nand Flash Spare Area Read Bytes 56-59 */
	uint32 NandSpareAreaReadOfs3C;	/* Nand Flash Spare Area Read Bytes 60-63*/

	uint32 NandReserved5[16];	/* 0x240-0x27f */

	/* 0x280 */
	uint32 NandSpareAreaWriteOfs0;	/* Nand Flash Spare Area Write Bytes 0-3 */
	uint32 NandSpareAreaWriteOfs4;	/* Nand Flash Spare Area Write Bytes 4-7 */
	uint32 NandSpareAreaWriteOfs8;	/* Nand Flash Spare Area Write Bytes 8-11 */
	uint32 NandSpareAreaWriteOfsC;	/* Nand Flash Spare Area Write Bytes 12-15 */
	/* 0x290 */
	uint32 NandSpareAreaWriteOfs10;	/* Nand Flash Spare Area Write Bytes 16-19 */
	uint32 NandSpareAreaWriteOfs14;	/* Nand Flash Spare Area Write Bytes 20-23 */
	uint32 NandSpareAreaWriteOfs18;	/* Nand Flash Spare Area Write Bytes 24-27 */
	uint32 NandSpareAreaWriteOfs1C;	/* Nand Flash Spare Area Write Bytes 28-31 */
	/* 0x2a0 */
	uint32 NandSpareAreaWriteOfs20;	/* Nand Flash Spare Area Write Bytes 32-35 */
	uint32 NandSpareAreaWriteOfs24;	/* Nand Flash Spare Area Write Bytes 36-39 */
	uint32 NandSpareAreaWriteOfs28;	/* Nand Flash Spare Area Write Bytes 40-43 */
	uint32 NandSpareAreaWriteOfs2C;	/* Nand Flash Spare Area Write Bytes 44-47 */
	/* 0x2b0 */
	uint32 NandSpareAreaWriteOfs30;	/* Nand Flash Spare Area Write Bytes 48-51 */
	uint32 NandSpareAreaWriteOfs34;	/* Nand Flash Spare Area Write Bytes 52-55 */
	uint32 NandSpareAreaWriteOfs38;	/* Nand Flash Spare Area Write Bytes 56-59 */
	uint32 NandSpareAreaWriteOfs3C;	/* Nand Flash Spare Area Write Bytes 60-63 */
	/* 0x2c0 */
	uint32 NandDdrTiming;
	uint32 NandDdrNcdlCalibCtl;
	uint32 NandDdrNcdlCalibPeriod;
	uint32 NandDdrNcdlCalibStat;
	/* 0x2d0 */
	uint32 NandDdrNcdlMode;
	uint32 NandDdrNcdlOffset;
	uint32 NandDdrPhyCtl;
	uint32 NandDdrPhyBistCtl;
	/* 0x2e0 */
	uint32 NandDdrPhyBistStat;
	uint32 NandDdrDiagStat0;
	uint32 NandDdrDiagStat1;
	uint32 NandReserved6[69];	/* 0x2ec-0x3ff */

	/* 0x400 */
	uint32 NandFlashCache[128];	/* 0x400-0x5ff */
} NandCtrlRegs;

#define NAND ((volatile NandCtrlRegs * const) NAND_REG_BASE)


/*
 * Power Management Control
 */
typedef struct PmcCtrlReg {
	/* 0x00 */
	uint32 l1Irq4keMask;
	uint32 l1Irq4keStatus;
	uint32 l1IrqMipsMask;
	uint32 l1IrqMipsStatus;
	/* 0x10 */
	uint32 l2IrqGpMask;
	uint32 l2IrqGpStatus;
	uint32 gpTmr0Ctl;
	uint32 gpTmr0Cnt;
	/* 0x20 */
	uint32 gpTmr1Ctl;
	uint32 gpTmr1Cnt;
	uint32 hostMboxIn;
	uint32 hostMboxOut;
	/* 0x30 */
#define PMC_CTRL_GP_FLASH_BOOT_STALL                  0x00000080
	uint32 gpOut;
	uint32 gpIn;
	uint32 gpInIrqMask;
	uint32 gpInIrqStatus;
	/* 0x40 */
	uint32 dmaCtrl;
	uint32 dmaStatus;
	uint32 dma0_3FifoStatus;
	uint32 unused0[3];	/* 0x4c-0x57 */
	/* 0x58 */
	uint32 l1IrqMips1Mask;
	uint32 diagControl;
	/* 0x60 */
	uint32 diagHigh;
	uint32 diagLow;
	uint32 badAddr;
	uint32 addr1WndwMask;
	/* 0x70 */
	uint32 addr1WndwBaseIn;
	uint32 addr1WndwBaseOut;
	uint32 addr2WndwMask;
	uint32 addr2WndwBaseIn;
	/* 0x80 */
	uint32 addr2WndwBaseOut;
	uint32 scratch;
	uint32 tm;
	uint32 softResets;
	/* 0x90 */
	uint32 eb2ubusTimeout;
	uint32 m4keCoreStatus;
	uint32 gpInIrqSense;
	uint32 ubSlaveTimeout;
	/* 0xa0 */
	uint32 diagEn;
	uint32 devTimeout;
	uint32 ubusErrorOutMask;
	uint32 diagCaptStopMask;
	/* 0xb0 */
	uint32 revId;
	uint32 gpTmr2Ctl;
	uint32 gpTmr2Cnt;
	uint32 legacyMode;
	/* 0xc0 */
	uint32 smisbMonitor;
	uint32 diagCtrl;
	uint32 diagStat;
	uint32 diagMask;
	/* 0xd0 */
	uint32 diagRslt;
	uint32 diagCmp;
	uint32 diagCapt;
	uint32 diagCnt;
	/* 0xe0 */
	uint32 diagEdgeCnt;
	uint32 unused1[4];	/* 0xe4-0xf3 */
	/* 0xf4 */
	uint32 iopPeriphBaseAddr;
	uint32 lfsr;
	uint32 unused2;		/* 0xfc-0xff */
} PmcCtrlReg;

typedef struct PmcOutFifoReg {
	uint32 msgCtrl;		/* 0x00 */
	uint32 msgSts;		/* 0x04 */
	uint32 unused[14];	/* 0x08-0x3f */
	uint32 msgData[16];	/* 0x40-0x7c */
} PmcOutFifoReg;

typedef struct PmcInFifoReg {
	uint32 msgCtrl;		/* 0x00 */
	uint32 msgSts;		/* 0x04 */
	uint32 unused[13];	/* 0x08-0x3b */
	uint32 msgLast;		/* 0x3c */
	uint32 msgData[16];	/* 0x40-0x7c */
} PmcInFifoReg;

typedef struct PmcDmaReg {
	/* 0x00 */
	uint32 src;
	uint32 dest;
	uint32 cmdList;
	uint32 lenCtl;
	/* 0x10 */
	uint32 rsltSrc;
	uint32 rsltDest;
	uint32 rsltHcs;
	uint32 rsltLenStat;
} PmcDmaReg;

typedef struct PmcTokenReg {
	/* 0x00 */
	uint32 bufSize;
	uint32 bufBase;
	uint32 idx2ptrIdx;
	uint32 idx2ptrPtr;
	/* 0x10 */
	uint32 unused[2];
	uint32 bufSize2;
} PmcTokenReg;

typedef struct PmcPerfPowReg {
	/* 0x00 */
	uint32 dcacheHit;
	uint32 dcacheMiss;
	uint32 icacheHit;
	uint32 icacheMiss;
	/* 0x10 */
	uint32 instnComplete;
	uint32 wtbMerge;
	uint32 wtbNoMerge;
	uint32 itlbHit;
	/* 0x20 */
	uint32 itlbMiss;
	uint32 dtlbHit;
	uint32 dtlbMiss;
	uint32 jtlbHit;
	/* 0x30 */
	uint32 jtlbMiss;
	uint32 powerSubZone;
	uint32 powerMemPda;
	uint32 freqScalarCtrl;
	/* 0x40 */
	uint32 freqScalarMask;
} PmcPerfPowReg;

typedef struct PmcDQMReg {
	/* 0x00 */
	uint32 cfg;
	uint32 _4keLowWtmkIrqMask;
	uint32 mipsLowWtmkIrqMask;
	uint32 lowWtmkIrqMask;
	/* 0x10 */
	uint32 _4keNotEmptyIrqMask;
	uint32 mipsNotEmptyIrqMask;
	uint32 notEmptyIrqSts;
	uint32 queueRst;
	/* 0x20 */
	uint32 notEmptySts;
	uint32 nextAvailMask;
	uint32 nextAvailQueue;
	uint32 mips1LowWtmkIrqMask;
	/* 0x30 */
	uint32 mips1NotEmptyIrqMask;
	uint32 autoSrcPidInsert;
} PmcDQMReg;

typedef struct PmcCntReg {
	uint32 cntr[10];
	uint32 unused[6];	/* 0x28-0x3f */
	uint32 cntrIrqMask;
	uint32 cntrIrqSts;
} PmcCntReg;

typedef struct PmcDqmQCtrlReg {
	uint32 size;
	uint32 cfga;
	uint32 cfgb;
	uint32 cfgc;
} PmcDqmQCtrlReg;

typedef struct PmcDqmQDataReg {
	uint32 word[4];
} PmcDqmQDataReg;

typedef struct PmcDqmQMibReg {
	uint32 qNumFull[32];
	uint32 qNumEmpty[32];
	uint32 qNumPushed[32];
} PmcDqmQMibReg;

typedef struct Pmc {
	uint32 baseReserved;		/* 0x0000 */
	uint32 unused0[1023];
	PmcCtrlReg ctrl;		/* 0x1000 */

	PmcOutFifoReg outFifo;		/* 0x1100 */
	uint32 unused1[32];		/* 0x1180-0x11ff */
	PmcInFifoReg inFifo;		/* 0x1200 */
	uint32 unused2[32];		/* 0x1280-0x12ff */

	PmcDmaReg dma[2];		/* 0x1300 */
	uint32 unused3[48];		/* 0x1340-0x13ff */

	PmcTokenReg token;		/* 0x1400 */
	uint32 unused4[121];		/* 0x141c-0x15ff */

	PmcPerfPowReg perfPower;	/* 0x1600 */
	uint32 unused5[47];		/* 0x1644-0x16ff */

	uint32 msgId[32];		/* 0x1700 */
	uint32 unused6[32];		/* 0x1780-0x17ff */

	PmcDQMReg dqm;			/* 0x1800 */
	uint32 unused7[50];		/* 0x1838-0x18ff */

	PmcCntReg hwCounter;		/* 0x1900 */
	uint32 unused8[46];		/* 0x1948-0x19ff */

	PmcDqmQCtrlReg dqmQCtrl[32];	/* 0x1a00 */
	PmcDqmQDataReg dqmQData[32];	/* 0x1c00 */
	uint32 unused9[64];		/* 0x1e00-0x1eff */

	uint32 qStatus[32];		/* 0x1f00 */
	uint32 unused10[32];		/* 0x1f80-0x1fff */

	PmcDqmQMibReg qMib;		/* 0x2000 */
	uint32 unused11[1952];		/* 0x2180-0x3ffff */

	uint32 sharedMem[8192];		/* 0x4000-0xbffc */
} Pmc;

#define PMC ((volatile Pmc * const) PMC_BASE)

/*
 * Process Monitor Module
 */
typedef struct PMRingOscillatorControl {
	uint32 control;
	uint32 en_lo;
	uint32 en_mid;
	uint32 en_hi;
	uint32 idle_lo;
	uint32 idle_mid;
	uint32 idle_hi;
} PMRingOscillatorControl;

#define RCAL_0P25UM_HORZ          0
#define RCAL_0P25UM_VERT          1
#define RCAL_0P5UM_HORZ           2
#define RCAL_0P5UM_VERT           3
#define RCAL_1UM_HORZ             4
#define RCAL_1UM_VERT             5
#define PMMISC_RMON_EXT_REG       ((RCAL_1UM_VERT + 1)/2)
#define PMMISC_RMON_VALID_MASK    (0x1<<16)
typedef struct PMMiscControl {
	uint32 gp_out;
	uint32 clock_select;
	uint32 unused[2];
	uint32 misc[4];
} PMMiscControl;

typedef struct PMSSBMasterControl {
	uint32 control;
#define PMC_SSBM_CONTROL_SSB_START	(1<<15)
#define PMC_SSBM_CONTROL_SSB_ADPRE	(1<<13)
#define PMC_SSBM_CONTROL_SSB_EN		(1<<12)
#define PMC_SSBM_CONTROL_SSB_CMD_SHIFT	(10)
#define PMC_SSBM_CONTROL_SSB_CMD_MASK	(0x3 << PMC_SSBM_CONTROL_SSB_CMD_SHIFT)
#define PMC_SSBM_CONTROL_SSB_CMD_READ	(2)
#define PMC_SSBM_CONTROL_SSB_CMD_WRITE	(1)
#define PMC_SSBM_CONTROL_SSB_ADDR_SHIFT	(0)
#define PMC_SSBM_CONTROL_SSB_ADDR_MASK	(0x3ff << PMC_SSBM_CONTROL_SSB_ADDR_SHIFT)
	uint32 wr_data;
	uint32 rd_data;
} PMSSBMasterControl;

typedef struct PMEctrControl {
	uint32 control;
	uint32 interval;
	uint32 thresh_lo;
	uint32 thresh_hi;
	uint32 count;
} PMEctrControl;

typedef struct PMBMaster {
	uint32 ctrl;
#define PMC_PMBM_START		(1 << 31)
#define PMC_PMBM_TIMEOUT	(1 << 30)
#define PMC_PMBM_SLAVE_ERR	(1 << 29)
#define PMC_PMBM_BUSY		(1 << 28)
#define PMC_PMBM_Read		(0 << 20)
#define PMC_PMBM_Write		(1 << 20)
	uint32 wr_data;
	uint32 timeout;
	uint32 rd_data;
	uint32 unused[4];
} PMBMaster;

typedef struct PMAPVTMONControl {
	uint32 control;
	uint32 reserved;
	uint32 cfg_lo;
	uint32 cfg_hi;
	uint32 data;
	uint32 vref_data;
	uint32 unused[2];
	uint32 ascan_cfg;
	uint32 warn_temp;
	uint32 reset_temp;
	uint32 temp_value;
	uint32 data1_value;
	uint32 data2_value;
	uint32 data3_value;
} PMAPVTMONControl;

typedef struct PMUBUSCfg {
	uint32 window[8];
	uint32 control;
} PMUBUSCfg;

typedef struct ProcessMonitorRegs {
	uint32 MonitorCtrl;		/* 0x00 */
	uint32 unused0[7];
	PMRingOscillatorControl ROSC;	/* 0x20 */
	uint32 unused1;
	PMMiscControl Misc;		/* 0x40 */
	PMSSBMasterControl SSBMaster;	/* 0x60 */
	uint32 unused2[5];
	PMEctrControl Ectr;		/* 0x80 */
	uint32 unused3[11];
	PMBMaster PMBM[2];		/* 0xc0 */
	PMAPVTMONControl APvtmonCtrl;	/* 0x100 */
	uint32 unused4[9];
	PMUBUSCfg UBUSCfg;		/* 0x160 */
} ProcessMonitorRegs;

#define PROCMON ((volatile ProcessMonitorRegs * const) PROC_MON_BASE)

#if 0
/*
** Spi Controller
*/

typedef struct SpiControl {
  uint16        spiMsgCtl;              /* (0x0) control byte */
#define FULL_DUPLEX_RW                  0
#define HALF_DUPLEX_W                   1
#define HALF_DUPLEX_R                   2
#define SPI_MSG_TYPE_SHIFT              14
#define SPI_BYTE_CNT_SHIFT              0
  byte          spiMsgData[0x21e];      /* (0x02 - 0x21f) msg data */
  byte          unused0[0x1e0];
  byte          spiRxDataFifo[0x220];   /* (0x400 - 0x61f) rx data */
  byte          unused1[0xe0];

  uint16        spiCmd;                 /* (0x700): SPI command */
#define SPI_CMD_NOOP                    0
#define SPI_CMD_SOFT_RESET              1
#define SPI_CMD_HARD_RESET              2
#define SPI_CMD_START_IMMEDIATE         3

#define SPI_CMD_COMMAND_SHIFT           0
#define SPI_CMD_COMMAND_MASK            0x000f

#define SPI_CMD_DEVICE_ID_SHIFT         4
#define SPI_CMD_PREPEND_BYTE_CNT_SHIFT  8
#define SPI_CMD_ONE_BYTE_SHIFT          11
#define SPI_CMD_ONE_WIRE_SHIFT          12
#define SPI_DEV_ID_0                    0
#define SPI_DEV_ID_1                    1
#define SPI_DEV_ID_2                    2
#define SPI_DEV_ID_3                    3
#define ZSI_SPI_DEV_ID                  3

  byte          spiIntStatus;           /* (0x702): SPI interrupt status */
  byte          spiMaskIntStatus;       /* (0x703): SPI masked interrupt status */

  byte          spiIntMask;             /* (0x704): SPI interrupt mask */
#define SPI_INTR_CMD_DONE               0x01
#define SPI_INTR_RX_OVERFLOW            0x02
#define SPI_INTR_INTR_TX_UNDERFLOW      0x04
#define SPI_INTR_TX_OVERFLOW            0x08
#define SPI_INTR_RX_UNDERFLOW           0x10
#define SPI_INTR_CLEAR_ALL              0x1f

  byte          spiStatus;              /* (0x705): SPI status */
#define SPI_RX_EMPTY                    0x02
#define SPI_CMD_BUSY                    0x04
#define SPI_SERIAL_BUSY                 0x08

  byte          spiClkCfg;              /* (0x706): SPI clock configuration */
#define SPI_CLK_0_391MHZ                1
#define SPI_CLK_0_781MHZ                2 /* default */
#define SPI_CLK_1_563MHZ                3
#define SPI_CLK_3_125MHZ                4
#define SPI_CLK_6_250MHZ                5
#define SPI_CLK_12_50MHZ                6
#define SPI_CLK_MASK                    0x07
#define SPI_SSOFFTIME_MASK              0x38
#define SPI_SSOFFTIME_SHIFT             3
#define SPI_BYTE_SWAP                   0x80

  byte          spiFillByte;            /* (0x707): SPI fill byte */
  byte          unused2;
  byte          spiMsgTail;             /* (0x709): msgtail */
  byte          unused3;
  byte          spiRxTail;              /* (0x70B): rxtail */
} SpiControl;

#define SPI ((volatile SpiControl * const) SPI_BASE)
#endif


#if defined(SUPPORT_631XX_TX_RX_IUDMA)
#define IUDMA_MAX_CHANNELS          32

/*
** DMA Channel Configuration (1 .. 32)
*/
typedef struct DmaChannelCfg {
  uint32        cfg;                    /* (00) assorted configuration */
#define         DMA_ENABLE      0x00000001  /* set to enable channel */
#define         DMA_PKT_HALT    0x00000002  /* idle after an EOP flag is detected */
#define         DMA_BURST_HALT  0x00000004  /* idle after finish current memory burst */
  uint32        intStat;                /* (04) interrupts control and status */
  uint32        intMask;                /* (08) interrupts mask */
#define         DMA_BUFF_DONE   0x00000001  /* buffer done */
#define         DMA_DONE        0x00000002  /* packet xfer complete */
#define         DMA_NO_DESC     0x00000004  /* no valid descriptors */
#define         DMA_RX_ERROR    0x00000008  /* rxdma detect client protocol error */
  uint32        maxBurst;               /* (0C) max burst length permitted */
#define         DMA_DESCSIZE_SEL 0x00040000  /* DMA Descriptor Size Selection */
} DmaChannelCfg;

/*
** DMA State RAM (1 .. 16)
*/
typedef struct DmaStateRam {
  uint32        baseDescPtr;            /* (00) descriptor ring start address */
  uint32        state_data;             /* (04) state/bytes done/ring offset */
  uint32        desc_len_status;        /* (08) buffer descriptor status and len */
  uint32        desc_base_bufptr;       /* (0C) buffer descrpitor current processing */
} DmaStateRam;


/*
** DMA Registers
*/
typedef struct DmaRegs {
    uint32 controller_cfg;              /* (00) controller configuration */
#define DMA_MASTER_EN           0x00000001
#define DMA_FLOWC_CH1_EN        0x00000002
#define DMA_FLOWC_CH3_EN        0x00000004

    // Flow control Ch1
    uint32 flowctl_ch1_thresh_lo;           /* 004 */
    uint32 flowctl_ch1_thresh_hi;           /* 008 */
    uint32 flowctl_ch1_alloc;               /* 00c */
#define DMA_BUF_ALLOC_FORCE     0x80000000

    // Flow control Ch3
    uint32 flowctl_ch3_thresh_lo;           /* 010 */
    uint32 flowctl_ch3_thresh_hi;           /* 014 */
    uint32 flowctl_ch3_alloc;               /* 018 */

    // Flow control Ch5
    uint32 flowctl_ch5_thresh_lo;           /* 01C */
    uint32 flowctl_ch5_thresh_hi;           /* 020 */
    uint32 flowctl_ch5_alloc;               /* 024 */

    // Flow control Ch7
    uint32 flowctl_ch7_thresh_lo;           /* 028 */
    uint32 flowctl_ch7_thresh_hi;           /* 02C */
    uint32 flowctl_ch7_alloc;               /* 030 */

    uint32 ctrl_channel_reset;              /* 034 */
    uint32 ctrl_channel_debug;              /* 038 */
    uint32 reserved1;                       /* 03C */
    uint32 ctrl_global_interrupt_status;    /* 040 */
    uint32 ctrl_global_interrupt_mask;      /* 044 */

    // Unused words
    uint8 reserved2[0x200-0x48];

    // Per channel registers/state ram
    DmaChannelCfg chcfg[IUDMA_MAX_CHANNELS];/* (200-3FF) Channel configuration */
    union {
        DmaStateRam     s[IUDMA_MAX_CHANNELS];
        uint32          u32[4 * IUDMA_MAX_CHANNELS];
    } stram;                                /* (400-5FF) state ram */
} DmaRegs;

#define SAR_DMA ((volatile DmaRegs * const) SAR_DMA_BASE)
#define SW_DMA ((volatile DmaRegs * const) SWITCH_DMA_BASE)
#define GMAC_DMA ((volatile DmaRegs * const) GMAC_DMA_BASE)
#endif

#if defined(SUPPORT_631XX_TX_RX_IUDMA)
/*
** DMA Buffer
*/
typedef struct DmaDesc {
#if !defined(CONFIG_ARM)
  union {
    struct {
        uint16        length;                   /* in bytes of data in buffer */
#define          DMA_DESC_USEFPM    0x8000
#define          DMA_DESC_MULTICAST 0x4000
#define          DMA_DESC_BUFLENGTH 0x0fff
        uint16        status;                   /* buffer status */
#define          DMA_OWN                0x8000  /* cleared by DMA, set by SW */
#define          DMA_EOP                0x4000  /* last buffer in packet */
#define          DMA_SOP                0x2000  /* first buffer in packet */
#define          DMA_WRAP               0x1000  /* */
#define          DMA_PRIO               0x0C00  /* Prio for Tx */
#define          DMA_APPEND_BRCM_TAG    0x0200
#define          DMA_APPEND_CRC         0x0100
#define          USB_ZERO_PKT           (1<< 0) // Set to send zero length packet
    };
    uint32      word0;
  };
#else
  union {
    struct {
        uint16        status;                   /* buffer status */
#define          DMA_OWN                0x8000  /* cleared by DMA, set by SW */
#define          DMA_EOP                0x4000  /* last buffer in packet */
#define          DMA_SOP                0x2000  /* first buffer in packet */
#define          DMA_WRAP               0x1000  /* */
#define          DMA_PRIO               0x0C00  /* Prio for Tx */
#define          DMA_APPEND_BRCM_TAG    0x0200
#define          DMA_APPEND_CRC         0x0100
#define          USB_ZERO_PKT           (1<< 0) // Set to send zero length packet
        uint16        length;                   /* in bytes of data in buffer */
#define          DMA_DESC_USEFPM    0x8000
#define          DMA_DESC_MULTICAST 0x4000
#define          DMA_DESC_BUFLENGTH 0x0fff
    };
    uint32      word0;
  };
#endif

  uint32        address;                /* address of data */
} DmaDesc;

/*
** 16 Byte DMA Buffer
*/
typedef struct {
  union {
    struct {
        uint16        length;                   /* in bytes of data in buffer */
#define          DMA_DESC_USEFPM        0x8000
#define          DMA_DESC_MULTICAST     0x4000
#define          DMA_DESC_BUFLENGTH     0x0fff
        uint16        status;                   /* buffer status */
#define          DMA_OWN                0x8000  /* cleared by DMA, set by SW */
#define          DMA_EOP                0x4000  /* last buffer in packet */
#define          DMA_SOP                0x2000  /* first buffer in packet */
#define          DMA_WRAP               0x1000  /* */
#define          DMA_PRIO               0x0C00  /* Prio for Tx */
#define          DMA_APPEND_BRCM_TAG    0x0200
#define          DMA_APPEND_CRC         0x0100
#define          USB_ZERO_PKT           (1<< 0) // Set to send zero length packet
    };
    uint32      word0;
  };

  uint32        address;                 /* address of data */
  uint32        control;
#define         GEM_ID_MASK             0x001F
  uint32        reserved;
} DmaDesc16;
#endif

#define SWITCH_CNTRL_REG    BCM_IO_ADDR(0x800c0000)

#define SWITCH_SINGLE_SERDES_STAT (SWITCH_CNTRL_REG + 0x198)
    #define SWITCH_REG_SSER_LINK_STAT   (1<<0)
    #define SWITCH_REG_SSER_RXSIG_DET   (1<<1)
    #define SWITCH_REG_SSER_RXSIG_1G    (1<<2)
    #define SWITCH_REG_SSER_SGMII       (1<<3)
    #define SWITCH_REG_SSER_SYNC_STAT   (1<<4)
    #define SWITCH_REG_SSER_POLL_LOCK   (1<<5)
    #define SWITCH_REG_SSER_EXTFB_DET   (1<<6)

#define SWITCH_REG_SINGLE_SERDES_CNTRL (SWITCH_CNTRL_REG + 0x194)

#define SWITCH_REG_SERDES_IQQD       (1<<0)
#define SWITCH_REG_SERDES_PWRDWN     (1<<1)
#define SWITCH_REG_SERDES_RESETPLL   (1<<3)
#define SWITCH_REG_SERDES_RESETMDIO  (1<<4)
#define SWITCH_REG_SERDES_RESET      (1<<5)

#if 0
typedef struct USBControl {
    uint32 BrtControl1;
    uint32 BrtControl2;
    uint32 BrtStatus1;
    uint32 BrtStatus2;
    uint32 UTMIControl1;
    uint32 TestPortControl;
    uint32 PllControl1;
#define PLLC_REFCLKSEL_MASK     0x00000003
#define PLLC_REFCLKSEL_SHIFT    0
#define PLLC_CLKSEL_MASK        0x0000000c
#define PLLC_CLKSEL_SHIFT       2
#define PLLC_XTAL_PWRDWNB       0x00000010
#define PLLC_PLL_PWRDWNB        0x00000020
#define PLLC_PLL_CALEN          0x00000040
#define PLLC_PHYPLL_BYP         0x00000080
#define PLLC_PLL_RESET          0x00000100
#define PLLC_PLL_IDDQ_PWRDN     0x00000200
#define PLLC_PLL_PWRDN_DELAY    0x00000400
    uint32 SwapControl;
#define USB_DEVICE_SEL          (1<<6)
#define EHCI_LOGICAL_ADDRESS_EN (1<<5)
#define EHCI_ENDIAN_SWAP        (1<<4)
#define EHCI_DATA_SWAP          (1<<3)
#define OHCI_LOGICAL_ADDRESS_EN (1<<2)
#define OHCI_ENDIAN_SWAP        (1<<1)
#define OHCI_DATA_SWAP          (1<<0)
    uint32 GenericControl;
#define GC_PLL_SUSPEND_EN       (1<<1)
    uint32 FrameAdjustValue;
    uint32 Setup;
#define USBH_IPP                (1<<5)
#define USBH_IOC                (1<<4)
    uint32 MDIO;
    uint32 MDIO32;
    uint32 USBSimControl;
} USBControl;

#define USBH ((volatile USBControl * const) USBH_CFG_BASE)
#endif

#if 0
typedef struct EthSwAvTableRegs {
    uint8  arlTableControl;
#define ARL_CTRL_START_DONE 0x80
#define ARL_CTRL_READ       0x01
#define ARL_CTRL_WRITE      0x00
    uint16 macAddrIndexLow;
    uint32 macAddrIndexHigh;
    uint16 vlanIdIndex;
#define ARL_VID_INDEX_MASK 0x0FFF
    uint32 reserved0;
    uint32 arlMacVidEntryLow;
    uint32 arlMacVidEntryHigh;
#define ARL_MAC_VID_ENTRY_HIGH_VID_MASK 0x0FFF0000
#define ARL_MAC_VID_ENTRY_HIGH_VID_SHFT 16
#define ARL_MAC_VID_ENTRY_HIGH_MAC_MASK 0x0000FFFF
    uint16 arlDataEntry;
#define ARL_DATA_ENTRY_VALID     0x8000
#define ARL_DATA_ENTRY_STATIC    0x4000
#define ARL_DATA_ENTRY_AGE       0x2000
#define ARL_DATA_ENTRY_PRIO_MASK 0x1C00
#define ARL_DATA_ENTRY_PRIO_SHFT 10
#define ARL_DATA_ENTRY_PORT_MASK 0x01FF
    uint32 reserved1[5];
    uint16 arlSearchControl;
#define ARL_SEARCH_CTRL_START_DONE 0x0080
#define ARL_SEARCH_CTRL_VALID      0x0001
    uint16 arlSearchAddr;
    uint32 arlSearchMacVidResultLow;
    uint32 arlSearchMacVidResultHigh;
#define ARL_SEARCH_MAC_VID_RESULT_VID_MASK 0x0FFF0000
#define ARL_SEARCH_MAC_VID_RESULT_VID_SHFT 16
#define ARL_SEARCH_MAC_VID_RESULT_MAC_MASK 0x0000FFFF
    uint16 arlSearchDataResult;
#define ARL_SEARCH_DATA_RESULT_STATIC    0x8000
#define ARL_SEARCH_DATA_RESULT_AGE       0x4000
#define ARL_SEARCH_DATA_RESULT_PRIO_MASK 0x3800
#define ARL_SEARCH_DATA_RESULT_PRIO_SHFT 11
#define ARL_SEARCH_DATA_RESULT_PORT_MASK 0x03FE
#define ARL_SEARCH_DATA_RESULT_PORT_SHFT 1
    uint32 reserved2[8];
    uint8  vlanTableControl;
    uint16 vlanTableAddrIndex;
    uint32 vlanTableEntry;
} EthSwAvTableRegs;

#define ETHSW_AVTABLE_REG ((volatile EthSwAvTableRegs * const) (SWITCH_BASE + 0x500))

typedef struct EthSwMIBRegs {
    unsigned int TxOctetsLo;               // 0
    unsigned int TxOctetsHi;
    unsigned int TxDropPkts;               // 8
    unsigned int TxQoSPkts;                // Tx Q0 pkts
    unsigned int TxBroadcastPkts;          // 0x10
    unsigned int TxMulticastPkts;
    unsigned int TxUnicastPkts;
    unsigned int TxCol;
    unsigned int TxSingleCol;              // 0x20
    unsigned int TxMultipleCol;
    unsigned int TxDeferredTx;
    unsigned int TxLateCol;
    unsigned int TxExcessiveCol;           // 0x30
    unsigned int TxFrameInDisc;
    unsigned int TxPausePkts;
    // SF2 Enhancements
    unsigned int TxQ1Pkts;                 // Tx Q1 pkts
    unsigned int TxQ2Pkts;                 // 0x40 Tx Q2 pkts
    unsigned int TxQ3Pkts;                 // Tx Q3 pkts
    unsigned int TxQ4Pkts;                 // Tx Q4 pkts
    unsigned int TxQ5Pkts;                 // Tx Q5 pkts
    //unsigned int TxQoSOctetsLo;         
    //unsigned int TxQoSOctetsHi;
    // SF2 Done
    unsigned int RxOctetsLo;               // 0x50
    unsigned int RxOctetsHi;
    unsigned int RxUndersizePkts;
    unsigned int RxPausePkts;
    unsigned int Pkts64Octets;             // 0x60
    unsigned int Pkts65to127Octets;
    unsigned int Pkts128to255Octets;
    unsigned int Pkts256to511Octets;
    unsigned int Pkts512to1023Octets;      // 0x70
    unsigned int Pkts1024to1522Octets;
    unsigned int RxOversizePkts;
    unsigned int RxJabbers;
    unsigned int RxAlignErrs;              // 0x80
    unsigned int RxFCSErrs;
    unsigned int RxGoodOctetsLo;
    unsigned int RxGoodOctetsHi;
    unsigned int RxDropPkts;               // 0x90
    unsigned int RxUnicastPkts;
    unsigned int RxMulticastPkts;
    unsigned int RxBroadcastPkts;
    unsigned int RxSAChanges;              // 0xa0
    unsigned int RxFragments;
    unsigned int RxExcessSizeDisc;
    unsigned int RxSymbolError;
    unsigned int InRangeErr;               // 0xb0
    unsigned int outRangeErr;
    unsigned int EEELpiEvevt;
    unsigned int EEELpiDuration;
    unsigned int RxDiscard;                // 0xc0
    unsigned int reserved1;
    unsigned int TxQ6Pkts;                 // Tx Q6 pkts
    unsigned int TxQ7Pkts;                 // Tx Q7 pkts
    unsigned int TxPkts64Octets;           // 0xD0
    unsigned int TxPkts65to127Octets;      // 
    unsigned int TxPkts128to255Octets;     // 
    unsigned int TxPkts256to511Octets;     // 
    unsigned int TxPkts512to1023Octets;    // 0xE0
    unsigned int TxPktsPkts1024toMaxOctets; 
    
#if 0
    unsigned int RxQoSPkts;
    unsigned int RxQoSOctetsLo;
    unsigned int RxQoSOctetsHi;
    unsigned int Pkts1523to2047;
    unsigned int Pkts2048to4095;
    unsigned int Pkts4096to8191;
    unsigned int Pkts8192to9728;
#endif
} EthSwMIBRegs;

#define ETHSWMIBREG ((volatile EthSwMIBRegs * const) (SWITCH_BASE + 0x2000))

/* Enet registers controlling rx iuDMA channel */
typedef struct EthSwQosIngressPortPriRegs{
    unsigned short pri_id_map[9];
} EthSwQosIngressPortPriRegs;

#define ETHSWQOSREG ((volatile EthSwQosIngressPortPriRegs * const) (SWITCH_BASE + 0x3050))
#endif

/********************** UNIMAC Block *************************/
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_HD_BKP_CNTL BCM_IO_ADDR(0x802d4004) /* UniMAC Half Duplex Backpressure Control Register */
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD         BCM_IO_ADDR(0x802d4008) /* UniMAC Command Register */
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_MAC0        BCM_IO_ADDR(0x802d400c) /*Unimac MAC address first 4 bytes*/
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_MAC1        BCM_IO_ADDR(0x802d4010) /*Unimac MAC address 2 last bytes*/
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_FRM_LEN     BCM_IO_ADDR(0x802d4014) /* UniMAC Frame Length */
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_PAUSE_QUNAT BCM_IO_ADDR(0x802d4018) /* UniMAC Pause Quanta */
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_MODE        BCM_IO_ADDR(0x802d4044) /* UniMAC Mode */
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_TX_IPG_LEN  BCM_IO_ADDR(0x802d405c) /* UniMAC Inter Packet Gap */
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_EEE_CTRL    BCM_IO_ADDR(0x802d4064) /* UniMAC Energy Efficient Ethernet Control */
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_EEE_REF_COUNT   BCM_IO_ADDR(0x802d4070) /* UniMAC Energy Efficient Ethernet Ref Clock Speed */
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_EEE_REF_COUNT_VAL 350               /* Ref clock is half speed of Runner clock */
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_PAUSE_CNTRL BCM_IO_ADDR(0x802d4330) /* UniMAC Repetitive Pause Control in TX direction */
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_RX_MAX_PKT_SIZE BCM_IO_ADDR(0x802d4608) /* UniMAC RX MAX packet Size Register */

#define UNIMAC_TOP_UNIMAC_MISC_UNIMAC_CFG      BCM_IO_ADDR(0x802db800) /* configuration register */
#define UNIMAC_TOP_UNIMAC_MISC_UNIMAC_EXT_CFG1 BCM_IO_ADDR(0x802db804)
#define UNIMAC_TOP_UNIMAC_MISC_UNIMAC_EXT_CFG2 BCM_IO_ADDR(0x802db808)

#define UNIMAC_CONF_EMAC_OFFSET                     0x1000
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_HD_BKP_CNTL_ipg_config_rx_MASK 0x0000007c
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_HD_BKP_CNTL_ipg_config_rx_SHIFT 2
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_HD_BKP_CNTL_hd_fc_ena_MASK 0x00000001
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_HD_BKP_CNTL_hd_fc_ena_SHIFT 0
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_txrx_en_config_MASK    0x20000000
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_txrx_en_config_SHIFT   29
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_tx_pause_ignore_MASK   0x10000000
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_tx_pause_ignore_SHIFT  28
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_rmt_loop_ena_MASK      0x02000000
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_rmt_loop_ena_SHIFT     25
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_cntl_frm_ena_MASK      0x00800000
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_cntl_frm_ena_SHIFT     23
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_lcl_loop_ena_MASK      0x00008000
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_lcl_loop_ena_SHIFT     15
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_sw_reset_MASK          0x00002000
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_sw_reset_SHIFT         13
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_hd_ena_MASK            0x00000400
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_hd_ena_SHIFT           10
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_rx_pause_ignore_MASK   0x00000100
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_rx_pause_ignore_SHIFT  8
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_pause_fwd_MASK         0x00000080
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_pause_fwd_SHIFT        7
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_crc_fwd_MASK           0x00000040
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_crc_fwd_SHIFT          6
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_pad_en_MASK            0x00000020
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_pad_en_SHIFT           5
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_eth_speed_MASK         0x0000000c
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_eth_speed_SHIFT        2
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_rx_ena_MASK            0x00000002
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_rx_ena_SHIFT           1
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_tx_ena_MASK            0x00000001
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_CMD_tx_ena_SHIFT           0
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_FRM_LEN_frame_length_MASK  0x00003fff
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_FRM_LEN_frame_length_SHIFT 0
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_TX_IPG_LEN_tx_ipg_len_MASK 0x0000007f
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_TX_IPG_LEN_tx_ipg_len_SHIFT 0
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_RX_MAX_PKT_SIZE_max_pkt_size_MASK 0x00003fff
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_RX_MAX_PKT_SIZE_max_pkt_size_SHIFT 0
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_PAUSE_CNTRL_pause_control_en_MASK 0x00020000
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_PAUSE_CNTRL_pause_control_en_SHIFT 17
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_PAUSE_CNTRL_pause_timer_MASK 0x0001ffff
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_PAUSE_CNTRL_pause_timer_SHIFT 0
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_PAUSE_QUNAT_pause_quant_MASK 0x0000ffff
#define UNIMAC_CONFIGURATION_UMAC_0_RDP_PAUSE_QUNAT_pause_quant_SHIFT 0

#define UNIMAC_MISC_EMAC_OFFSET 0x400
#define UNIMAC_TOP_UNIMAC_MISC_UNIMAC_EXT_CFG2_BACKPRESSURE_ENABLE_INT 1 << 16


#define UNIMAC_MIB_EMAC_OFFSET 0x400
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_MIB_CNTRL BCM_IO_ADDR(0x802da180) /* MIB Control Register */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GR64 BCM_IO_ADDR(0x802da000) /* Receive 64B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GR127 BCM_IO_ADDR(0x802da004) /* Receive 65B to 127B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GR255 BCM_IO_ADDR(0x802da008) /* Receive 128B to 255B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GR511 BCM_IO_ADDR(0x802da00c) /* Receive 256B to 511B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GR1023 BCM_IO_ADDR(0x802da010) /* Receive 512B to 1023B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GR1518 BCM_IO_ADDR(0x802da014) /* Receive 1024B to 1518B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRMGV BCM_IO_ADDR(0x802da018) /* Receive 1519B to 1522B Good VLAN Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GR2047 BCM_IO_ADDR(0x802da01c) /* Receive 1519B to 2047B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GR4095 BCM_IO_ADDR(0x802da020) /* Receive 2048B to 4095B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GR9216 BCM_IO_ADDR(0x802da024) /* Receive 4096B to 9216B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRPKT BCM_IO_ADDR(0x802da028) /* Receive Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRBYT BCM_IO_ADDR(0x802da02c) /* Receive Byte Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRMCA BCM_IO_ADDR(0x802da030) /* Receive Multicast Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRBCA BCM_IO_ADDR(0x802da034) /* Receive Broadcast Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRFCS BCM_IO_ADDR(0x802da038) /* Receive FCS Error Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRXCF BCM_IO_ADDR(0x802da03c) /* Receive Control Frame Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRXPF BCM_IO_ADDR(0x802da040) /* Receive Pause Frame Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRXUO BCM_IO_ADDR(0x802da044) /* Receive Unknown OP Code Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRALN BCM_IO_ADDR(0x802da048) /* Receive Alignmenet Error Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRFLR BCM_IO_ADDR(0x802da04c) /* Receive Frame Length Out Of Range Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRCDE BCM_IO_ADDR(0x802da050) /* Receive Code Error Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRFCR BCM_IO_ADDR(0x802da054) /* Receive Carrier Sense Error Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GROVR BCM_IO_ADDR(0x802da058) /* Receive Oversize Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRJBR BCM_IO_ADDR(0x802da05c) /* Receive Jabber Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRMTUE BCM_IO_ADDR(0x802da060) /* Receive MTU Error Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRPOK BCM_IO_ADDR(0x802da064) /* Receive Good Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRUC BCM_IO_ADDR(0x802da068) /* Receive Unicast Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRPPP BCM_IO_ADDR(0x802da06c) /* Receive PPP Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GRCRC BCM_IO_ADDR(0x802da070) /* Receive CRC Match Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_RRPKT BCM_IO_ADDR(0x802da100) /* Receive RUNT Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_RRUND BCM_IO_ADDR(0x802da104) /* Receive RUNT Packet And Contain A Valid FCS */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_RRFRG BCM_IO_ADDR(0x802da108) /* Receive RUNT Packet And Contain Invalid FCS or Alignment Error */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_RRBYT BCM_IO_ADDR(0x802da10c) /* Receive RUNT Packet Byte Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_TR64 BCM_IO_ADDR(0x802da080) /* Transmit 64B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_TR127 BCM_IO_ADDR(0x802da084) /* Transmit 65B to 127B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_TR255 BCM_IO_ADDR(0x802da088) /* Transmit 128B to 255B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_TR511 BCM_IO_ADDR(0x802da08c) /* Transmit 256B to 511B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_TR1023 BCM_IO_ADDR(0x802da090) /* Transmit 512B to 1023B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_TR1518 BCM_IO_ADDR(0x802da094) /* Transmit 1024B to 1518B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_TRMGV BCM_IO_ADDR(0x802da098) /* Transmit 1519B to 1522B Good VLAN Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_TR2047 BCM_IO_ADDR(0x802da09c) /* Transmit 1519B to 2047B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_TR4095 BCM_IO_ADDR(0x802da0a0) /* Transmit 2048B to 4095B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_TR9216 BCM_IO_ADDR(0x802da0a4) /* Transmit 4096B to 9216B Frame Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTPKT BCM_IO_ADDR(0x802da0a8) /* Transmit Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTMCA BCM_IO_ADDR(0x802da0ac) /* Transmit Multicast Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTBCA BCM_IO_ADDR(0x802da0b0) /* Transmit Broadcast Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTXPF BCM_IO_ADDR(0x802da0b4) /* Transmit Pause Frame Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTXCF BCM_IO_ADDR(0x802da0b8) /* Transmit Control Frame Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTFCS BCM_IO_ADDR(0x802da0bc) /* Transmit FCS Error Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTOVR BCM_IO_ADDR(0x802da0c0) /* Transmit Oversize Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTDRF BCM_IO_ADDR(0x802da0c4) /* Transmit Deferral Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTEDF BCM_IO_ADDR(0x802da0c8) /* Transmit Excessive Deferral Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTSCL BCM_IO_ADDR(0x802da0cc) /* Transmit Single Collision Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTMCL BCM_IO_ADDR(0x802da0d0) /* Transmit Multiple Collision Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTLCL BCM_IO_ADDR(0x802da0d4) /* Transmit Late Collision Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTXCL BCM_IO_ADDR(0x802da0d8) /* Transmit Excessive Collision Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTFRG BCM_IO_ADDR(0x802da0dc) /* Transmit Fragments Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTNCL BCM_IO_ADDR(0x802da0e0) /* Transmit Total Collision Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTJBR BCM_IO_ADDR(0x802da0e4) /* Transmit Jabber Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTBYT BCM_IO_ADDR(0x802da0e8) /* Transmit Byte Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTPOK BCM_IO_ADDR(0x802da0ec) /* Transmit Good Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_GTUC BCM_IO_ADDR(0x802da0f0) /* Transmit Unicast Packet Counter */
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_MIB_CNTRL_rx_cnt_st_MASK 0x00000001
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_MIB_CNTRL_rx_cnt_st_SHIFT 0
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_MIB_CNTRL_tx_cnt_rst_MASK 0x00000004
#define UNIMAC_CONFIGURATION_UMAC_MIB_0Module_RDP_MIB_CNTRL_tx_cnt_rst_SHIFT 2


/*
** SAR Registers
*/

#define SAR_TX_CTL_REGS (SAR_BASE + 0x00000060) /* SAR Tx Control Registers */
#define SAR_TX_CTL_REGS_SZ  0x00000020
#define SAR_RX_CTL_REGS (SAR_BASE + 0x00000080) /* SAR Rx Control Registers */
#define SAR_RX_CTL_REGS_SZ  0x00000030
#define SAR_RX_VCAM_REGS (SAR_BASE + 0x00000140) /* SAR  Rx ATM VPI_VCI CAM Table Reg Registers */
#define SAR_RX_VCAM_REGS_SZ  0x00000080
#define SAR_RX_PBUF_REGS (SAR_BASE + 0x00000300) /* SAR Rx Packet Buffer Control Registers */
#define SAR_RX_PBUF_REGS_SZ  0x00000060
#define SAR_MIB_REGS (SAR_BASE + 0x00000600) /* SAR  Atm MIB Counters Registers */
#define SAR_MIB_REGS_SZ  0x000000C0
#define SAR_RX_PAF_REGS (SAR_BASE + 0x00000800) /* SAR RxPaf Top Registers */
#define SAR_RX_PAF_REGS_SZ  0x00000100
#define SAR_RX_BOND_REGS (SAR_BASE + 0x00000900) /* SAR RxPaf Bonding Registers */
#define SAR_RX_BOND_REGS_SZ  0x000000C0

/* Specific SAR Rx Control Registers - LED Configuration Register */
#define SAR_RX_CTL_LED_CFG (SAR_BASE + 0x000000AC) /* LED Configuration Register */
#define SARRXCTLLEDCFG ((volatile uint32 * const) (SAR_RX_CTL_LED_CFG))

#define SARLEDCFG_TEST                  0x000000100     /* LED Test bit. */
#define SARLEDCFG_BLINK_30MS            0x000000000     /* LED blink speed: 00 = 30 ms */
#define SARLEDCFG_BLINK_50MS            0x000000020     /* LED blink speed: 01 = 50 ms */
#define SARLEDCFG_BLINK_125MS           0x000000040     /* LED blink speed: 10 = 125 ms */
#define SARLEDCFG_BLINK_250MS           0x000000060     /* LED blink speed: 11 = 250 ms */
#define SARLEDCFG_LNK                   0x000000010     /* Link is established - set by software when ADSL link is established. */
#define SARLEDCFG_INT_LED               0x000000008     /* Set to enable using internal LED logic to drive INET_LED, otherwise use Periph LED logic control. */
#define SARLEDCFG_MODE_LINK             0x000000000     /* INET_LED Mode: 00 = Assert on ADSL Link Only (by setting LINK bit) */
#define SARLEDCFG_MODE_TRAFFIC          0x000000002     /* INET_LED Mode: 01 = Assert on Cell Activity */
#define SARLEDCFG_MODE_MELODYLINK       0x000000004     /* INET_LED Mode: 10 = ADSL Melody/Link Mode - blink slowly during training and solid on link */
#define SARLEDCFG_MODE_TRAFFICLINK      0x000000006     /* INET_LED Mode: 11 = Assert on ADSL Link, blink on ATM TX and RX cells traffic */
#define SARLEDCFG_LED_EN                0x000000001     /* ADSL LED Enable */



#if defined(SUPPORT_631XX_TX_RX_IUDMA)
/* SAR registers controlling rx iuDMA channel */
typedef struct SarRxMuxRegs{
    unsigned int vcid0_qid;
    unsigned int vcid1_qid;
} SarRxMuxRegs;

#define SARRXMUXREG ((volatile SarRxMuxRegs * const) (SAR_BASE + 0x0400))
#endif

/*
** PCI-E
*/
#define UBUS2_PCIE
#define PCIE3_CORE
typedef struct PcieRegs{
  uint32 devVenID;
  uint16 command;
  uint16 status;
  uint32 revIdClassCode;
  uint32 headerTypeLatCacheLineSize;
  uint32 bar1;
  uint32 bar2;
  uint32 priSecBusNo;
#define PCIE_CFG_TYPE1_PRI_SEC_BUS_NO_SUB_BUS_NO_MASK              0x00ff0000
#define PCIE_CFG_TYPE1_PRI_SEC_BUS_NO_SUB_BUS_NO_SHIFT             16
#define PCIE_CFG_TYPE1_PRI_SEC_BUS_NO_SEC_BUS_NO_MASK              0x0000ff00
#define PCIE_CFG_TYPE1_PRI_SEC_BUS_NO_SEC_BUS_NO_SHIFT             8
#define PCIE_CFG_TYPE1_PRI_SEC_BUS_NO_PRI_BUS_NO_MASK              0x000000ff

  uint32 secStatusioBaseLimit;
  uint32 rcMemBaseLimit;
  uint32 rcPrefBaseLimit;
  uint32 rcPrefBaseHi;
  uint32 rcPrefLimitHi;
  uint32 rcIoBaseLimit;
  uint32 capPointer;
  uint32 expRomBase;
  uint32 bridgeCtrl;
  uint32 unused1[2];
  uint32 pm_cap;				/* 0x048 */
  uint32 pm_csr;				/* 0x04c */
  uint32 unused2[23];	
  /* PcieExpressCtrlRegs */
  uint16 pciExpressCap;			/* 0x0ac */		
  uint16 pcieCapabilitiy;
  uint32 deviceCapability;
  uint16 deviceControl;
  uint16 deviceStatus;
  uint32 linkCapability;
#define PCIE_RC_CFG_PCIE_LINK_CAPBILITY_MAX_LINK_SPEED_MASK        0xf
#define PCIE_RC_CFG_PCIE_LINK_CAPBILITY_MAX_LINK_SPEED_2500MBPS    1
#define PCIE_RC_CFG_PCIE_LINK_CAPBILITY_MAX_LINK_SPEED_5000MBPS    2
  uint16 linkControl;
  uint16 linkStatus;
  uint32 slotCapability;
  uint16 slotControl;
  uint16 slotStatus;
  uint16 rootControl;
  uint16 rootCap;
  uint32 rootStatus;
  uint32 deviceCapability2;
  uint16 deviceControl2;
  uint16 deviceStatus2;
  uint32 linkCapability2;
  uint16 linkControl2;
  uint16 linkStatus2;
  uint32 slotCapability2;
  uint16 slotControl2;
  uint16 slotStatus2;
  uint32 unused3[6];		/* 0x0e8 */

  /* PcieErrorRegs */
  uint16 advErrCapId;		/* 0x100 */
  uint16 advErrCapOff;
  uint32 ucErrStatus;
  uint32 ucorrErrMask;
  uint32 ucorrErrSevr;
  uint32 corrErrStatus;
  uint32 corrErrMask;
  uint32 advErrCapControl;
  uint32 headerLog1;
  uint32 headerLog2;
  uint32 headerLog3;
  uint32 headerLog4;
  uint32 rootErrorCommand;
  uint32 rootErrorStatus;
  uint16 rcCorrId;
  uint16 rcFatalNonfatalId;
  uint32 unused4[10];		/* 0x138 */

  /* PcieVcRegs */
  uint16 vcCapId;
  uint16 vcCapOffset;
  uint32 prtVcCapability;
  uint32 portVcCapability2;
  uint16 portVcControl;
  uint16 portVcCtatus;
  uint32 portArbStatus;
  uint32 vcRsrcControl;
  uint32 vcRsrcStatus;
  uint32 unused5[1];		/* 0x17c */
} PcieRegs;

typedef struct PcieRcCfgVendorRegs{
	uint32 vendorCap;
	uint32 specificHeader;
	uint32 specificReg1;
#define PCIE_RC_CFG_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_BAR1_SHIFT    0	
#define PCIE_RC_CFG_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_BAR2_SHIFT    2
#define PCIE_RC_CFG_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_BAR3_SHIFT    4
#define PCIE_RC_CFG_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_WORD_ALIGN    0x0	
#define PCIE_RC_CFG_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_HWORD_ALIGN   0x1	
#define PCIE_RC_CFG_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_BYTE_ALIGN    0x2
}PcieRcCfgVendorRegs;

typedef struct PcieBlk404Regs{
  uint32 unused;          /* 0x404 */
  uint32 config2;         /* 0x408 */
#define PCIE_IP_BLK404_CONFIG_2_BAR1_SIZE_MASK         0x0000000f
#define PCIE_IP_BLK404_CONFIG_2_BAR1_DISABLE           0
  uint32 config3;         /* 0x40c */
  uint32 pmDataA;         /* 0x410 */
  uint32 pmDataB;         /* 0x414 */
} PcieBlk404Regs;

typedef struct PcieBlk428Regs{
  uint32 vpdIntf;        /* 0x428 */
  uint16 unused_g;       /* 0x42c */
  uint16 vpdAddrFlag;    /* 0x42e */
  uint32 vpdData;        /* 0x430 */
  uint32 idVal1;         /* 0x434 */
  uint32 idVal2;         /* 0x438 */
  uint32 idVal3;         /* 0x43c */
#define PCIE_IP_BLK428_ID_VAL3_REVISION_ID_MASK                    0xff000000
#define PCIE_IP_BLK428_ID_VAL3_REVISION_ID_SHIFT                   24
#define PCIE_IP_BLK428_ID_VAL3_CLASS_CODE_MASK                     0x00ffffff
#define PCIE_IP_BLK428_ID_VAL3_CLASS_CODE_SHIFT                    16
#define PCIE_IP_BLK428_ID_VAL3_SUB_CLASS_CODE_SHIFT                 8

  uint32 idVal4;
  uint32 idVal5;
  uint32 unused_h;
  uint32 idVal6;
  uint32 msiData;
  uint32 msiAddr_h;
  uint32 msiAddr_l;
  uint32 unused_1[2];
  uint32 msiMask;
  uint32 msiPend;
  uint32 pmData_c;
  uint32 unused_2[20];
  uint32 msixControl;
  uint32 msixTblOffBir;
  uint32 msixPbaOffBit;
  uint32 unused_k;
  uint32 pcieCapability;
  uint32 deviceCapability;
  uint32 deviceControl;
  uint32 linkCapability;
#define PCIE_IP_BLK428_LINK_CAPBILITY_MAX_LINK_SPEED_MASK          0xf
#define PCIE_IP_BLK428_LINK_CAPBILITY_MAX_LINK_SPEED_2500MBPS      1
#define PCIE_IP_BLK428_LINK_CAPBILITY_MAX_LINK_SPEED_5000MBPS      2
  uint32 bar2Config;
  uint32 pcieDeviceCapability2;
  uint32 pcieLinkCapability2;
  uint32 pcieLinkControl;
  uint32 pcieLinkCapabilityRc;
  uint32 bar3Config;
  uint32 rootCap;
  uint32 devSerNumCapId;
  uint32 lowerSerNum;
  uint32 upperSerNum;
  uint32 advErrCap;
  uint32 pwrBdgtData0;
  uint32 pwrBdgtData1;
  uint32 pwrBdgtData2;
  uint32 pwdBdgtData3;
  uint32 pwrBdgtData4;
  uint32 pwrBdgtData5;
  uint32 pwrBdgtData6;
  uint32 pwrBdgtData7;
  uint32 ext2CapAddr;
  uint32 pwrBdgtData8;
  uint32 pwrBdgtCapability;
  uint32 vsecHdr;
  uint32 rcUserMemLo1;
  uint32 rcUserMemHi1;
  uint32 rcUserMemLo2;
  uint32 rcUserMemHi2;
  uint32 tphCap;
  uint32 resizebarCap;
  uint32 ariCap;
  uint32 initvf;
  uint32 vfOffset;
  uint32 vfBarReg;
  uint32 vfSuppPageSize;
  uint32 vfCap_en;
  uint32 vfMsixTblBirOff;
  uint32 vfMsixPbaOffBit;
  uint32 vfMsixControl;
  uint32 vfBar4Reg;
  uint32 pfInitvf;
  uint32 vfNsp;
  uint32 atsInldQueueDepth;
} PcieBlk428Regs;

typedef struct PcieBlk800Regs{
#define NUM_PCIE_BLK_800_CTRL_REGS  6
  uint32 tlControl[NUM_PCIE_BLK_800_CTRL_REGS];
  /* followed by others not used */

} PcieBlk800Regs;


typedef struct PcieBlk1000Regs{
#define NUM_PCIE_BLK_1000_PDL_CTRL_REGS  16
  uint32 pdlControl[NUM_PCIE_BLK_1000_PDL_CTRL_REGS];
  uint32 dlattnVec;
  uint32 dlAttnMask;
  uint32 dlStatus;        /* 0x1048 */
#define PCIE_IP_BLK1000_DL_STATUS_PHYLINKUP_MASK                   0x00002000
#define PCIE_IP_BLK1000_DL_STATUS_PHYLINKUP_SHIFT                  13
  uint32 dlTxChecksum;
  uint32 dlForcedUpdateGen1;
  uint32 dlReg_spare0;
  uint32 dlErrStatistic_ctl;
  uint32 dlErrStatistic; 
  uint32 reserved[40];
  uint32 mdioAddr;
  uint32 mdioWrData;
  uint32 mdioRdData;
  uint32 dlAteTlpHdr_0;
  uint32 dlAteTlpHdr_1;
  uint32 dlAteTlpHdr_2;
  uint32 dlAteTlpHdr_3;
  uint32 dlAteTlpCfg;
  uint32 dlAteTlpCtl;
  uint32 reserved1[183];
  uint32 dlRxPFcCl;
  uint32 dlRxCFcCl;
  uint32 dlRxAckNack;
  uint32 dlTxRxSeqnb;
  uint32 dlTxPFcAl;
  uint32 dlTxNpFcAl;
  uint32 regDlSpare;
  uint32 dlRegSpare;
  uint32 dlTxRxSeq;
  uint32 dlRxNpFcCl;
} PcieBlk1000Regs;

typedef struct PcieBlk1800Regs{
#define NUM_PCIE_BLK_1800_PHY_CTRL_REGS         8
  uint32 phyCtrl[NUM_PCIE_BLK_1800_PHY_CTRL_REGS];
#define REG_POWERDOWN_P1PLL_ENA                      (1<<12)
  uint32 phyErrorAttnVec;
  uint32 phyErrorAttnMask;
  uint32 reserved[2];
  uint32 phyCtl8;				/* 0x1830 */
  uint32 reserved1[243];
  uint32 phyReceivedMcpErrors; 	/* 0x1c00 */
  uint32 reserved2[3];
  uint32 phyTransmittedMcpErrors;/* 0x1c10 */
  uint32 reserved3[3];
  uint32 rxFtsLimit;			/* 0x1c20 */
  uint32 reserved4[46];
  uint32 ftsHist;				/* 0x1cd8 */
  uint32 phyGenDebug;
  uint32 phyRecoveryHist;
#define NUM_PCIE_BLK_1800_PHY_LTSSM_HIST_REGS   5
  uint32 phyltssmHist[NUM_PCIE_BLK_1800_PHY_LTSSM_HIST_REGS];
#define NUM_PCIE_BLK_1800_PHY_DBG_REGS          11
  uint32 phyDbg[NUM_PCIE_BLK_1800_PHY_DBG_REGS];
  uint32 phyAteLoopbkInfo;		/* 0x1d30 */
  uint32 reserved5[55];
#define NUM_PCIE_BLK_1800_PHY_DBG_CLK_REGS      4
  uint32 phyDbgClk[NUM_PCIE_BLK_1800_PHY_DBG_CLK_REGS]; /* 0x1e10 */
} PcieBlk1800Regs;

typedef struct PcieMiscRegs{
  uint32 reset_ctrl;                    /* 4000 Reset Control Register */
  uint32 eco_ctrl_core;                 /* 4004 ECO Core Reset Control Register */
  uint32 misc_ctrl;                     /* 4008 MISC Control Register */
#define PCIE_MISC_CTRL_MAX_BURST_SIZE_128B                         (1<<20)
#define PCIE_MISC_CTRL_BURST_ALIGN                                 (1<<19)
#define PCIE_MISC_CTRL_CFG_READ_UR_MODE                            (1<<13)
#define PCIE_MISC_CTRL_PCIE_IN_WR_COMBINE                          (1<<11)
#define PCIE_MISC_CTRL_PCIE_RCB_MPS_MODE                           (1<<10)
#define PCIE_MISC_CTRL_PCIE_RCB_64B_MODE                           (1<<7)

  uint32 cpu_2_pcie_mem_win0_lo;        /* 400c CPU to PCIe Memory Window 0 Low */
#define PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_ADDR_MASK              0xfff00000
#define PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_ADDR_SHIFT             20
#define PCIE_MISC_CPU_2_PCIE_MEM_ENDIAN_MODE_NO_SWAP               0
#define PCIE_MISC_CPU_2_PCIE_MEM_ENDIAN_MODE_HALF_WORD_SWAP        1
#define PCIE_MISC_CPU_2_PCIE_MEM_ENDIAN_MODE_HALF_BYTE_SWAP        2
  uint32 cpu_2_pcie_mem_win0_hi;        /* 4010 CPU to PCIe Memory Window 0 High */
  uint32 cpu_2_pcie_mem_win1_lo;        /* 4014 CPU to PCIe Memory Window 1 Low */
  uint32 cpu_2_pcie_mem_win1_hi;        /* 4018 CPU to PCIe Memory Window 1 High */
  uint32 cpu_2_pcie_mem_win2_lo;        /* 401c CPU to PCIe Memory Window 2 Low */
  uint32 cpu_2_pcie_mem_win2_hi;        /* 4020 CPU to PCIe Memory Window 2 High */
  uint32 cpu_2_pcie_mem_win3_lo;        /* 4024 CPU to PCIe Memory Window 3 Low */
  uint32 cpu_2_pcie_mem_win3_hi;        /* 4028 CPU to PCIe Memory Window 3 High */
  uint32 rc_bar1_config_lo;             /* 402c RC BAR1 Configuration Low Register */
#define  PCIE_MISC_RC_BAR_CONFIG_LO_MATCH_ADDRESS_MASK             0xfff00000
#define  PCIE_MISC_RC_BAR_CONFIG_LO_SIZE_256MB                     0xd
#define  PCIE_MISC_RC_BAR_CONFIG_LO_SIZE_MAX                       0x14     /* max is 32GB */
  uint32 rc_bar1_config_hi;             /* 4030 RC BAR1 Configuration High Register */
  uint32 rc_bar2_config_lo;             /* 4034 RC BAR2 Configuration Low Register */
  uint32 rc_bar2_config_hi;             /* 4038 RC BAR2 Configuration High Register */
  uint32 rc_bar3_config_lo;             /* 403c RC BAR3 Configuration Low Register */
  uint32 rc_bar3_config_hi;             /* 4040 RC BAR3 Configuration High Register */
#define PCIE_MISC_MSI_BAR_CONFIG_LO_MATCH_ADDR_MASK                0xfffffffc
#define	PCIE_MISC_MSI_BAR_CONFIG_LO_ENABLE_MASK                    0x00000001
  uint32 msi_bar_config_lo;             /* 4044 Message Signaled Interrupt Base Address Low Register */
  uint32 msi_bar_config_hi;             /* 4048 Message Signaled Interrupt Base Address High Register */
#define PCIE_MISC_MSI_DATA_CONFIG_MATCH_MASK                       0xffff0000
#define PCIE_MISC_MSI_DATA_CONFIG_MATCH_SHIFT                      16
#define PCIE_MISC_MSI_DATA_CONFIG_DATA_MASK                        0x0000ffff
  uint32 msi_data_config;               /* 404c Message Signaled Interrupt Data Configuration Register */
  uint32 rc_bad_address_lo;             /* 4050 RC Bad Address Register Low */
  uint32 rc_bad_address_hi;             /* 4054 RC Bad Address Register High */
  uint32 rc_bad_data;                   /* 4058 RC Bad Data Register */
  uint32 rc_config_retry_timeout;       /* 405c RC Configuration Retry Timeout Register */
  uint32 eoi_ctrl;                      /* 4060 End of Interrupt Control Register */
  uint32 pcie_ctrl;                     /* 4064 PCIe Control */
  uint32 pcie_status;                   /* 4068 PCIe Status */
  uint32 revision;                      /* 406c PCIe Revision */
  uint32 cpu_2_pcie_mem_win0_base_limit;/* 4070 CPU to PCIe Memory Window 0 base/limit */
#define PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_LIMIT_LIMIT_MASK        0xfff00000
#define PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_LIMIT_LIMIT_SHIFT       20
#define PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_LIMIT_BASE_MASK         0x0000fff0
#define PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_LIMIT_BASE_SHIFT        4
  uint32 cpu_2_pcie_mem_win1_base_limit;/* 4074 CPU to PCIe Memory Window 1 base/limit */
  uint32 cpu_2_pcie_mem_win2_base_limit;/* 4078 CPU to PCIe Memory Window 2 base/limit */
  uint32 cpu_2_pcie_mem_win3_base_limit;/* 407c CPU to PCIe Memory Window 3 base/limit */
  uint32 cpu_2_pcie_mem_win0_base_hi;
  uint32 cpu_2_pcie_mem_win0_limit_hi;
  uint32 cpu_2_pcie_mem_win1_base_hi;
  uint32 cpu_2_pcie_mem_win1_limit_hi;
  uint32 cpu_2_pcie_mem_win2_base_hi;
  uint32 cpu_2_pcie_mem_win2_limit_hi;
  uint32 cpu_2_pcie_mem_win3_base_hi;
  uint32 cpu_2_pcie_mem_win3_limit_hi;
  uint32 misc_ctrl_1;
  uint32 ubus_ctrl;
  uint32 ubus_timeout;
  uint32 ubus_bar1_config_remap;
#define  PCIE_MISC_UBUS_BAR_CONFIG_OFFSET_MASK                      0xfff00000
#define  PCIE_MISC_UBUS_BAR_CONFIG_ACCESS_EN                        1
  uint32 ubus_bar1_config_remap_hi;
  uint32 ubus_bar2_config_remap;
  uint32 ubus_bar2_config_remap_hi;
  uint32 ubus_bar3_config_remap;
  uint32 ubus_bar3_config_remap_hi;
  uint32 ubus_status;
} PcieMiscRegs;

typedef struct PcieMiscPerstRegs{
  uint32 perst_eco_ctrl_perst;          /* 4100 ECO PCIE Reset Control Register */
  uint32 perst_eco_cce_status;          /* 4104 Config Copy Engine Status */
} PcieMiscPerstRegs;

typedef struct PcieMiscHardRegs{
  uint32 hard_eco_ctrl_hard;            /* 4200 ECO Hard Reset Control Register */
  uint32 hard_pcie_hard_debug;          /* 4204 PCIE Hard Debug Register */
} PcieMiscHardRegs;

typedef struct PcieL2IntrControl{
#define	PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR07                      0x80000000
#define	PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR06                      0x40000000
#define	PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR05                      0x20000000
#define	PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR04                      0x10000000
#define	PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR03                      0x08000000
#define	PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR02                      0x04000000
#define	PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR01                      0x02000000
#define	PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR00                      0x01000000
#define	PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR00_SHIFT                24
#define	PCIE_L2_INTR_CTRL_MSI_CPU_INTR_MASK                          (                                               \
							             PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR00    |    \
							             PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR01    |    \
							             PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR02    |    \
							             PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR03    |    \
							             PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR04    |    \
							             PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR05    |    \
							             PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR06    |    \
							             PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR07 )
#define	PCIE_L2_INTR_CTRL_MSI_CPU_INTR_SHIFT                        (PCIE_L2_INTR_CTRL_CPU_STATUS_MSI_INTR00_SHIFT)
  uint32 Intr2CpuStatus;
  uint32 Intr2CpuSet;
  uint32 Intr2CpuClear;
  uint32 Intr2CpuMask_status;
  uint32 Intr2CpuMask_set;
  uint32 Intr2CpuMask_clear;
  uint32 Intr2PciStatus;
  uint32 Intr2PciSet;
  uint32 Intr2PciClear;
  uint32 Intr2PciMask_status;
  uint32 Intr2PciMask_set;
  uint32 Intr2PciMask_clear;
} PcieL2IntrControl;

typedef struct PcieDMAregs{
  uint32 TxFirstDesc_l_AddrList0;
  uint32 TxFirstDesc_u_AddrList0;
  uint32 TxFirstDesc_l_AddrList1;
  uint32 TxFirstDesc_u_AddrList1;
  uint32 TxSwDescListCtrlSts;
  uint32 TxWakeCtrl;
  uint32 TxErrorStatus;
  uint32 TxList0CurDesc_l_Addr;
  uint32 TxList0CurDesc_u_Addr;
  uint32 TxList0CurByteCnt;
  uint32 TxList1CurDesc_l_Addr;
  uint32 TxList1CurDesc_u_Addr;
  uint32 TxList1CurByteCnt;
  uint32 RxFirstDesc_l_AddrList0;
  uint32 RxFirstDesc_u_AddrList0;
  uint32 RxFirstDesc_l_AddrList1;
  uint32 RxFirstDesc_u_AddrList1;
  uint32 RxSwDescListCtrlSts;
  uint32 RxWakeCtrl;
  uint32 RxErrorStatus;
  uint32 RxList0CurDesc_l_Addr;
  uint32 RxList0CurDesc_u_Addr;
  uint32 RxList0CurByteCnt;
  uint32 RxList1CurDesc_l_Addr;
  uint32 RxList1CurDesc_u_Addr;
  uint32 RxList1CurByteCnt;
  uint32 Dma_debug_options_reg;
  uint32 ReadChannelErrorStatus;
} PcieDMAregs;

typedef struct PcieUBUSL2IntrControl{
  uint32 UBUSIntr2CPUStatus;
  uint32 UBUSIntr2CPUSet;
  uint32 UBUSIntr2CPUClear;
  uint32 UBUSIntr2CPUMaskStatus;
  uint32 UBUSIntr2CPUMaskSet;
  uint32 UBUSIntr2CPUMaskClear;
  uint32 UBUSIntr2PCIStatus;
  uint32 UBUSIntr2PCISet;
  uint32 UBUSIntr2PCIClear;
  uint32 UBUSIntr2PCIMaskStatus;
  uint32 UBUSIntr2PCIMaskSet;
  uint32 UBUSIntr2PCIMaskClear;
} PcieUBUSL2IntrControl;

typedef struct PcieIPIL2IntrControl{
  uint32 IPIIntr2CPUStatus;
  uint32 IPIIntr2CPUSet;
  uint32 IPIIntr2CPUClear;
  uint32 IPIIntr2CPUMask_status;
  uint32 IPIIntr2CPUMask_set;
  uint32 IPIIntr2CPUMask_clear;
  uint32 IPIIntr2PCIStatus;
  uint32 IPIIntr2PCISet;
  uint32 IPIIntr2PCIClear;
  uint32 IPIIntr2PCIMask_status;
  uint32 IPIIntr2PCIMask_set;
  uint32 IPIIntr2PCIMask_clear;
} PcieIPIL2IntrControl;

typedef struct PciePcieL1Intr1Regs{
  uint32 status;

  uint32 maskStatus;
  uint32 maskSet;
  uint32 maskClear;  
} PciePcieL1Intr1Regs;

typedef struct PcieCpuL1Intr1Regs{
  uint32 status;
#define PCIE_CPU_INTR1_IPI_CPU_INTR                                (1<<8)
#define PCIE_CPU_INTR1_PCIE_UBUS_CPU_INTR                          (1<<7)
#define PCIE_CPU_INTR1_PCIE_NMI_CPU_INTR                           (1<<6)
#define PCIE_CPU_INTR1_PCIE_INTR_CPU_INTR                          (1<<5)
#define PCIE_CPU_INTR1_PCIE_INTD_CPU_INTR                          (1<<4)
#define PCIE_CPU_INTR1_PCIE_INTC_CPU_INTR                          (1<<3)
#define PCIE_CPU_INTR1_PCIE_INTB_CPU_INTR                          (1<<2)
#define PCIE_CPU_INTR1_PCIE_INTA_CPU_INTR                          (1<<1)
#define PCIE_CPU_INTR1_PCIE_ERR_ATTN_CPU_INTR                      (1<<0)
  uint32 maskStatus;
  uint32 maskSet;
  uint32 maskClear;  
} PcieCpuL1Intr1Regs;

typedef struct PcieExtCfgRegs{
  uint32 index;
#define PCIE_EXT_CFG_BUS_NUM_MASK                                  0x0ff00000
#define PCIE_EXT_CFG_BUS_NUM_SHIFT                                 20
#define PCIE_EXT_CFG_DEV_NUM_MASK                                  0x000f0000
#define PCIE_EXT_CFG_DEV_NUM_SHIFT                                 15
#define PCIE_EXT_CFG_FUNC_NUM_MASK                                 0x00007000
#define PCIE_EXT_CFG_FUNC_NUM_SHIFT                                12
  uint32 data;
  uint32 scratch;
} PcieExtCfgRegs;
#define PCIEH                         ((volatile uint32 * const) PCIE_0_BASE)
#define PCIEH_REGS                     0
#define PCIEH_RC_CFG_VENDOR_REGS       0x180 
#define PCIEH_BLK_404_REGS             0x404
#define PCIEH_BLK_428_REGS             0x428
#define PCIEH_BLK_800_REGS             0x800
#define PCIEH_BLK_1000_REGS            0x1000
#define PCIEH_BLK_1800_REGS            0x1800
#define PCIEH_MISC_REGS           	   0x4000
#define PCIEH_MISC_PERST_REGS          0x4100
#define PCIEH_MISC_HARD_REGS           0x4200
#define PCIEH_L2_INTR_CTRL_REGS        0x4300
#define PCIEH_DMA_REGS           	     0x4400
#define PCIEH_PCIE_EXT_CFG_REGS        0x9000
#define PCIEH_UBUS_L2_INTR_CTRL_REGS   0x9100
#define PCIEH_IPI_L2_INTR_CTRL_REGS    0x9200
#define PCIEH_PCIE_INTR1_REGS          0x9300
#define PCIEH_CPU_INTR1_REGS           0x9400

#define PCIEH_1                        ((volatile uint32 * const) PCIE_1_BASE)

#define PCIEH_DEV_OFFSET               0x8000
#define PCIEH_MEM_BASE                 0x90000000
#define PCIEH_MEM_SIZE                 0x10000000
#define PCIEH_0_MEM_BASE              PCIEH_MEM_BASE
#define PCIEH_0_MEM_SIZE              PCIEH_MEM_SIZE
#define PCIEH_1_MEM_BASE               0xa0000000
#define PCIEH_1_MEM_SIZE               0x10000000
#define DDR_UBUS_ADDRESS_BASE         0
//#define PCIEH_PCIE_IS_DEFAULT_TARGET            /* define this only if pcie is the default ubus target and size can be extended*/

#if 0
#if defined(CONFIG_BCM_GMAC)
#define GMAC_PORT_ID            3
#define GMAC_PHY_CHAN           0

#define MASK32(rightbitindex, length) ( (0xffffffff << rightbitindex) & (0xffffffff >> (32-(rightbitindex+length))) )


typedef union MibCtrlReg_s {
    uint32 word;
    struct {
        uint32    unused: 31;
        uint32    clrMib:  1;
    };
} MibCtrlReg_t;

typedef union MibMaxPktSizeReg_s {
    uint32 word;
    struct {
        uint32    unused      : 18;
        uint32    max_pkt_size: 14;
    };
} MibMaxPktSizeReg_t;

typedef union RxBpThreshReg_s {
    uint32 word;
    struct {
        uint32    unused    : 21;
        uint32    rx_thresh : 11;
    };
} RxBpThreshReg_t;

typedef union RxFlowCtrlReg_s {
    uint32 word;
    struct {
        uint32    unused    : 20;
        uint32    pause_en  :  1;
        uint32    fc_en     :  1;
    };
} RxFlowCtrlReg_t;

typedef union BpForceReg_s {
    uint32 word;
    struct {
        uint32    unused: 31;
        uint32    force :  1;
    };
} BpForceReg_t;

typedef union IrqEnableReg_s {
    uint32 word;
    struct {
        uint32    unused: 31;
        uint32    ovfl  :  1;
    };
} IrqEnableReg_t;

typedef union IrqStatusReg_s {
    uint32 word;
    struct {
        uint32    unused: 31;
        uint32    ovfl  :  1;
    };
} IrqStatusReg_t;

typedef union GmacStatusReg_s {
    uint32  word;
    struct {
        uint32  unused      :27;
        uint32  link_up     : 1;
        uint32  auto_cfg_en : 1;
        uint32  hd          : 1;
        uint32  eth_speed   : 2;
#define GMAC_STATUS_SPEED_10        0
#define GMAC_STATUS_SPEED_100       1
#define GMAC_STATUS_SPEED_1000      2
    };
} GmacStatusReg_t;

typedef union MacSwResetReg_s {
    uint32  word;
    struct {
        uint32  unused       :29;
        uint32  txfifo_flush : 1;
        uint32  rxfifo_flush : 1;
        uint32  mac_sw_reset : 1;
    };
} MacSwResetReg_t;

typedef union DmaRxStatusSelReg_s {
    uint32  word;
    struct {
        uint32  unused    :23;
        uint32  runt_det  : 1;
        uint32  frm_trunc : 1;
        uint32  ucast_det : 1;
        uint32  vlan      : 1;
        uint32  ctrl_frm  : 1;
        uint32  bcast_det : 1;
        uint32  mcast_det : 1;
        uint32  crc_err   : 1;
        uint32  rx_err    : 1;
    };
} DmaRxStatusSelReg_t;

typedef union DmaRxOkToSendCountReg_s {
    uint32  word;
    struct {
        uint32  unused           :28;
        uint32  ok_to_send_count : 4;
    };
} DmaRxOkToSendCountReg_t;




typedef struct GmacIntf {
/*0x00*/    uint32                  Control; 
/*0x04*/    MibCtrlReg_t            MibCtrl;
/*0x08*/    uint32                  unused; 
/*0x0C*/    MibMaxPktSizeReg_t      MibMaxPktSize;
/*0x10*/    RxBpThreshReg_t         RxBpThreshLo;
/*0x14*/    RxBpThreshReg_t         RxBpThreshHi;
/*0x18*/    RxFlowCtrlReg_t         RxFlowCtrl; 
/*0x1C*/    uint32                  DiagOut; 
/*0x20*/    BpForceReg_t            BpForce;
/*0x24*/    IrqEnableReg_t          IrqEnable;
/*0x28*/    GmacStatusReg_t         GmacStatus;
/*0x2C*/    IrqStatusReg_t          IrqStatus; 
/*0x30*/    uint32                  OverFlowCounter; 
/*0x34*/    uint32                  BackPressCounter;
/*0x38*/    MacSwResetReg_t         MacSwReset;  
/*0x3C*/    DmaRxStatusSelReg_t     DmaRxStatusSel;
/*0x40*/    DmaRxOkToSendCountReg_t DmaRxOkToSendCount;
} GmacIntf;

#define GMAC_INTF ((volatile GmacIntf * const) (GMAC_BASE+0x800))

typedef struct GmacMIBRegs {
/*0x00*/    unsigned int RxFCSErrs;
/*0x04*/    unsigned int RxCtrlFrame;
/*0x08*/    unsigned int RxPausePkts;
/*0x0c*/    unsigned int RxUnknown;
/*0x10*/    unsigned int RxAlignErrs;
/*0x14*/    unsigned int RxExcessSizeDisc; /* TODO not sure about counter */
/*0x18*/    unsigned int RxSymbolError;
/*0x1c*/    unsigned int RxCarrierSenseErrs;
/*0x20*/    unsigned int RxOversizePkts;
/*0x24*/    unsigned int RxJabbers;
/*0x28*/    unsigned int RxMtuErrs;
/*0x2c*/    unsigned int RxRuntPkts; /* RxUnderSizePkts + RxFragments */
/*0x30*/    unsigned int RxUndersizePkts;
/*0x34*/    unsigned int RxFragments;
/*0x38*/    unsigned int RxRuntOctets;
/*0x3c*/    unsigned int RxMulticastPkts;
/*0x40*/    unsigned int RxBroadcastPkts;
/*0x44*/    unsigned int Pkts64Octets;
/*0x48*/    unsigned int Pkts65to127Octets;
/*0x4c*/    unsigned int Pkts128to255Octets;
/*0x50*/    unsigned int Pkts256to511Octets;
/*0x54*/    unsigned int Pkts512to1023Octets;

/*TODO mapping to ROBO */
/*0x58*/    unsigned int Pkts1024to1518Octets;
/*0x5c*/    unsigned int Pkts1519to1522;
/*0x60*/    unsigned int Pkts1523to2047;
/*0x64*/    unsigned int Pkts2048to4095;
/*0x68*/    unsigned int Pkts4096to8191; /* Actually it is upto 9216 */
/*0x6c*/    unsigned int RxPkts;
/*0x70*/    unsigned int RxOctetsLo;

/*0x74*/    unsigned int RxUnicastPkts;
/*0x78*/    unsigned int RxGoodPkts;
/*0x7c*/    unsigned int RxPPPPkts;
/*0x80*/    unsigned int RxCRCMatchPkts;

/*0x84*/    unsigned int TxPausePkts;
/*0x88*/    unsigned int TxJabber;
/*0x8c*/    unsigned int TxFCSErrs;
/*0x90*/    unsigned int TxCtrlFrame;
/*0x94*/    unsigned int TxOversizePkts;
/*0x98*/    unsigned int TxDeferredTx;
/*0x9c*/    unsigned int TxExcessiveDef;
/*0xa0*/    unsigned int TxSingleCol;
/*0xa4*/    unsigned int TxMultipleCol;
/*0xa8*/    unsigned int TxLateCol;
/*0xac*/    unsigned int TxExcessiveCol;
/*0xb0*/    unsigned int TxFragments;
/*0xb4*/    unsigned int TxCol;
/*0xb8*/    unsigned int TxMulticastPkts;
/*0xbc*/    unsigned int TxBroadcastPkts;

/* No mapping in ROBO for TX octet counters */
/*0xc0*/    unsigned int TxPkts64Octets;
/*0xc4*/    unsigned int TxPkts65to127Octets;
/*0xc8*/    unsigned int TxPkts128to255Octets;
/*0xcc*/    unsigned int TxPkts256to511Octets;
/*0xd0*/    unsigned int TxPkts512to1023Octets;
/*0xd4*/    unsigned int TxPkts1024to1518Octets;
/*0xd8*/    unsigned int TxPkts1519to1522;
/*0xdc*/    unsigned int TxPkts1523to2047;
/*0xe0*/    unsigned int TxPkts2048to4095;
/*0xe4*/    unsigned int TxPkts4096to8191; /* Actually it is upto 9216 */

/*0xe8*/    unsigned int TxPkts;
/*0xec*/    unsigned int TxOctetsLo;
/*0xf0*/    unsigned int TxUnicastPkts;
/*0xf4*/    unsigned int TxGoodPkts;

/*
 * Need to map GMAC counters to these ROBO counters
    unsigned int TxDropPkts;
    unsigned int TxQoSPkts;
    unsigned int TxFrameInDisc;
    unsigned int TxQoSOctetsLo;
    unsigned int TxQoSOctetsHi;

    unsigned int RxGoodOctetsLo;
    unsigned int RxGoodOctetsHi;
    unsigned int RxDropPkts;
    unsigned int RxSAChanges;
    unsigned int RxQoSOctetsLo;
    unsigned int RxQoSOctetsHi;
*/
} GmacMIBRegs;

#define GMAC_MIB ((volatile GmacMac * const) (GMAC_BASE + 0xA00))

typedef union CmdReg_s {
    uint32 word;
    struct {
        uint32 unused3       : 1;   /* bit 31 */
        uint32 runt_filt_dis : 1;   /* bit 30 */
        uint32 txrx_en_cfg   : 1;   /* bit 29 */
        uint32 tx_pause_ign  : 1;   /* bit 28 */
        uint32 prbl_ena      : 1;   /* bit 27 */
        uint32 rx_err_disc   : 1;   /* bit 26 */
        uint32 rmt_loop_ena  : 1;   /* bit 25 */
        uint32 len_chk_dis   : 1;   /* bit 24 */
        uint32 ctrl_frm_ena  : 1;   /* bit 23 */
        uint32 ena_ext_cfg   : 1;   /* bit 22 */
        uint32 unused2       : 6;   /* bit 21:16 */
        uint32 lcl_loop_ena  : 1;   /* bit 15 */
        uint32 unused1       : 1;   /* bit 14 */
        uint32 sw_reset      : 1;   /* bit 13 */
        uint32 unused0       : 2;   /* bit 12:11 */
        uint32 hd_ena        : 1;   /* bit 10 */
        uint32 tx_addr_ins   : 1;   /* bit  9 */
        uint32 rx_pause_ign  : 1;   /* bit  8 */
        uint32 pause_fwd     : 1;   /* bit  7 */
        uint32 crc_fwd       : 1;   /* bit  6 */
        uint32 pad_rem_en    : 1;   /* bit  5 */
        uint32 promis_en     : 1;   /* bit  4 */
        uint32 eth_speed     : 2;   /* bit 3:2 */
#define CMD_ETH_SPEED_10            0
#define CMD_ETH_SPEED_100           1
#define CMD_ETH_SPEED_1000          2
#define CMD_ETH_SPEED_2500          3

        uint32 rx_ena        : 1;   /* bit  1 */
        uint32 tx_ena        : 1;   /* bit  0 */
    };
} CmdReg_t;

typedef union FrmLenReg_s {
    uint32  word;
    struct {
        uint32 unused  : 18;   
        uint32 frm_len : 14;   /* bit 13:0 */
    };
} FrmLenReg_t;

typedef union PauseQuantaReg_s {
    uint32  word;
    struct {
        uint32 unused       : 16;   
        uint32 pause_quanta : 16;   /* bit 15:0 */
    };
} PauseQuantaReg_t;

typedef union ModeReg_s {
    uint32  word;
    struct {
        uint32 unused       : 26;   
        uint32 mac_link_up  : 1;   /* bit  5 */
        uint32 mac_tx_pause : 1;   /* bit  4 */
        uint32 mac_rx_pause : 1;   /* bit  3 */
        uint32 mac_dplx     : 1;   /* bit  2 */
        uint32 mac_speed    : 2;   /* bit 1:0 */
    };
} ModeReg_t;

typedef union FrmTagReg_s {
    uint32  word;
    struct {
        uint32 unused  : 15;   
        uint32 tpid_en :  1;    /* bit 16 */
        uint32 tag     : 16;    /* bit 15:0 */
    };
} FrmTagReg_t;

typedef union TxIpgLenReg_s {
    uint32  word;
    struct {
        uint32 unused     :27;   
        uint32 tx_ipg_len : 5;  /* bit 4:0 */
    };
} TxIpgLenReg_t;

typedef union RxIpgInvReg_s {
    uint32  word;
    struct {
        uint32 unused     :31;   
        uint32 rx_ipg_inv : 1;  /* bit 0 */
    };
} RxIpgInvReg_t;

typedef union RepPauseCtrlReg_s {
    uint32  word;
    struct {
        uint32 unused      :14;   
        uint32 pause_en    : 1; /* bit 17 */
        uint32 pause_timer :17; /* bit 16:0 */
    };
} RepPauseCtrlReg_t;

typedef union TxFifoFlushReg_s {
    uint32  word;
    struct {
        uint32 unused   :31;   
        uint32 tx_flush : 1; /* bit 0 */
    };
} TxFifoFlushReg_t;

typedef struct RxFifoStatusReg_s {
    uint32  word;
    struct {
        uint32 unused          :30;   
        uint32 rxfifo_overrun  : 1; /* bit 1 */
        uint32 rxfifo_underrun : 1; /* bit 0 */
    };
} RxFifoStatusReg_t;

typedef union TxFifoStatusReg_s {
    uint32 word;
    struct {
        uint32 unused          :30;   
        uint32 txfifo_overrun  : 1; /* bit 1 */
        uint32 txfifo_underrun : 1; /* bit 0 */
    };
} TxFifoStatusReg_t;


typedef struct GmacMac {
    uint32 UmacDummy;               /* 0x00 */
    uint32 HdBkpCntl;               /* 0x04 */
    CmdReg_t Cmd;                   /* 0x08 */
    uint32 Mac0;                    /* 0x0c */
    uint32 Mac1;                    /* 0x10 */
    FrmLenReg_t FrmLen;             /* 0x14 */
    PauseQuantaReg_t PauseQuanta;   /* 0x18 */
    uint32 unused1[9];              /* 0x1c - 0x3c */
    uint32 SfdOffset;               /* 0x40 */
    ModeReg_t Mode;                 /* 0x44 */
    FrmTagReg_t FrmTag0;            /* 0x48 */
    FrmTagReg_t FrmTag1;            /* 0x4c */
    uint32 unused2[3];              /* 0x50 - 0x58 */
    TxIpgLenReg_t TxIpgLen;         /* 0x5c */
    uint32 unused3[6];              /* 0x60 - 0x74 */
    RxIpgInvReg_t RxIpgInv;         /* 0x78 */
    uint32 unused4[165];            /* 0x7c - 0x30c */
    uint32 MacsecProgTxCrc;         /* 0x310 */
    uint32 MacsecCtrl;              /* 0x314 */
    uint32 unused5[6];              /* 0x318 - 0x32c */
    RepPauseCtrlReg_t PauseCtrl;    /* 0x330 */
    TxFifoFlushReg_t TxFifoFlush;   /* 0x334 */
    RxFifoStatusReg_t RxFifoStatus; /* 0x338 */
    TxFifoStatusReg_t TxFifoStatus; /* 0x33c */
} GmacMac;

#define GMAC_MAC ((volatile GmacMac * const) (GMAC_BASE + 0xC00))

typedef struct gmacEEECtrl {
    uint32 unused1       :  16;
    uint32 softReset     :  1;
    uint32 unused2       :  10;
    uint32 linkUp        :  1;
    uint32 lpiCntrSnap   :  1;
    uint32 lpiCntrClr    :  1;
    uint32 halt          :  1;
    uint32 enable        :  1;
} gmacEEECtrl_t;
typedef struct gmacEEEStat {
    uint32 idle          :  1;
    uint32 halt          :  1;
    uint32 enable        :  1;
    uint32 softReset     :  1;
    uint32 pktFull       :  1;
    uint32 pktEmpty      :  1;
    uint32 fifoFull      :  1;
    uint32 fifoEmpty     :  1;
    uint32 fullDplx      :  1;
    uint32 speed         :  2;
    uint32 unused1       :  1;
    uint32 currState     :  4;
    uint32 lpiCntr       :  16;
} gmacEEEStat_t;
typedef struct GmacEEE {
    gmacEEECtrl_t eeeCtrl;   /* @ 0x1000e850 */
    gmacEEEStat_t eeeStat;
    uint32  eeeT1000WakeTime;
    uint32  eeeTx100WakeTime;
    uint32  eeeLPIWaitTime;
} GmacEEE_t;

#define GMAC_EEE (volatile GmacEEE_t *const) ((unsigned char *)GMAC_INTF + 0x50)
#endif /* defined(CONFIG_BCM_GMAC) */
#endif

typedef struct {
    unsigned int led_f;
    unsigned int reserved;
} LED_F;

typedef struct EthernetSwitchCore
{
    unsigned int port_traffic_ctrl[9];            /* 0x00 - 0x08 */
    unsigned int reserved1[2];                    /* 0x09 - 0x0a */
    unsigned int switch_mode;                     /* 0x0b */
#define ETHSW_SM_RETRY_LIMIT_DIS                  0x04
#define ETHSW_SM_FORWARDING_EN                    0x02
    unsigned int pause_quanta;                    /* 0x0c */
    unsigned int reserved33; 
    unsigned int imp_port_state;                  /*0x0e */
#define ETHSW_IPS_USE_MII_HW_STS                  0x00
#define ETHSW_IPS_USE_REG_CONTENTS                0x80
#define ETHSW_IPS_GMII_SPEED_UP_NORMAL            0x00
#define ETHSW_IPS_GMII_SPEED_UP_2G                0x40
#define ETHSW_IPS_TXFLOW_NOT_PAUSE_CAPABLE        0x00
#define ETHSW_IPS_TXFLOW_PAUSE_CAPABLE            0x20
#define ETHSW_IPS_RXFLOW_NOT_PAUSE_CAPABLE        0x00
#define ETHSW_IPS_RXFLOW_PAUSE_CAPABLE            0x10
#define ETHSW_IPS_SW_PORT_SPEED_1000M_2000M       0x08
#define ETHSW_IPS_DUPLEX_MODE                     0x02
#define ETHSW_IPS_LINK_FAIL                       0x00
#define ETHSW_IPS_LINK_PASS                       0x01
    unsigned int led_refresh;                     /* 0x0f */
    LED_F        led_function[2];                 /* 0x10 */
    unsigned int led_function_map;                /* 0x14 */
    unsigned int reserved14; 
    unsigned int led_enable_map;                  /* 0x16 */
    unsigned int reserved15; 
    unsigned int led_mode_map0;                   /* 0x18 */
    unsigned int reserved16; 
    unsigned int led_function_map1;               /* 0x1a */
    unsigned int reserved17; 
    unsigned int reserved2[5];                    /* 0x1c - 0x20 */
    unsigned int port_forward_ctrl;               /* 0x21 */
    unsigned int switch_ctrl;                     /* 0x22 */
#define ETHSW_SC_MII_DUMP_FORWARDING_EN           0x40
#define ETHSW_SC_MII2_VOL_SEL                     0x02
    unsigned int reserved3;                       /* 0x23 */
    unsigned int protected_port_selection;        /* 0x24 */
    unsigned int reserved18; 
    unsigned int wan_port_select;                 /* 0x26 */
    unsigned int reserved19; 
    unsigned int pause_capability;                /* 0x28 */
    unsigned int reserved20[3]; 
    unsigned int reserved4[3];                    /* 0x2c - 0x2e */
    unsigned int reserved_multicast_control;      /* 0x2f */
    unsigned int reserved5;                       /* 0x30 */
    unsigned int txq_flush_mode_control;          /* 0x31 */
    unsigned int ulf_forward_map;                 /* 0x32 */
    unsigned int reserved21; 
    unsigned int mlf_forward_map;                 /* 0x34 */
    unsigned int reserved22; 
    unsigned int mlf_impc_forward_map;            /* 0x36 */
    unsigned int reserved23; 
    unsigned int pause_pass_through_for_rx;       /* 0x38 */
    unsigned int reserved24; 
    unsigned int pause_pass_through_for_tx;       /* 0x3a */
    unsigned int reserved25; 
    unsigned int disable_learning;                /* 0x3c */
    unsigned int reserved26; 
    unsigned int reserved6[26];                   /* 0x3e - 0x57 */
    unsigned int port_state_override[8];          /* 0x58 - 0x5f */
#define ETHSW_PS_SW_OVERRIDE                      0x40
#define ETHSW_PS_SW_TX_FLOW_CTRL_EN               0x20
#define ETHSW_PS_SW_RX_FLOW_CTRL_EN               0x10
#define ETHSW_PS_SW_PORT_SPEED_1000M              0x80
#define ETHSW_PS_SW_PORT_SPEED_100M               0x40
#define ETHSW_PS_SW_PORT_SPEED_10M                0x00
#define ETHSW_PS_DUPLEX_MODE                      0x02
#define ETHSW_PS_LINK_DOWN                        0x00
#define ETHSW_PS_LINK_UP                          0x01
    unsigned int reserved7[4];                    /* 0x60 - 0x63 */
    unsigned int imp_rgmii_ctrl_p4;               /* 0x64 */
    unsigned int imp_rgmii_ctrl_p5;               /* 0x65 */
    unsigned int reserved8[6];                    /* 0x66 - 0x6b */
    unsigned int rgmii_timing_delay_p4;           /* 0x6c */
    unsigned int gmii_timing_delay_p5;            /* 0x6d */
    unsigned int reserved9[11];                   /* 0x6e - 0x78 */
    unsigned int software_reset;                  /* 0x79 */
    unsigned int reserved13[6];                   /* 0x7a - 0x7f */
    unsigned int pause_frame_detection;           /* 0x80 */
    unsigned int reserved10[7];                   /* 0x81 - 0x87 */
    unsigned int fast_aging_ctrl;                 /* 0x88 */
    unsigned int fast_aging_port;                 /* 0x89 */
    unsigned int fast_aging_vid;                  /* 0x8a */
    unsigned int anonymous1[376];                 /* 0x8b */
    unsigned int brcm_hdr_ctrl;                   /* 0x203 */
    unsigned int anonymous2[895];                 /* 0x204 */
} EthernetSwitchCore;

#define ETHSW_CORE ((volatile EthernetSwitchCore * const) SWITCH_BASE)

typedef struct EthernetSwitchReg
{
    uint32 switch_ctrl;                      /* 0x0000 */
    uint32 switch_status;                    /* 0x0004 */
    uint32 dir_data_write_reg;               /* 0x0008 */
    uint32 dir_data_read_reg;                /* 0x000c */
    uint32 led_serial_refresh_time_unit;     /* 0x0010 */
    uint32 reserved1;                        /* 0x0014 */
    uint32 switch_rev;                       /* 0x0018 */
    uint32 phy_rev;                          /* 0x001c */
    uint32 phy_test_ctrl;                    /* 0x0020 */
    uint32 qphy_ctrl;                        /* 0x0024 */
#define ETHSW_QPHY_CTRL_PHYAD_BASE_SHIFT     12
#define ETHSW_QPHY_CTRL_PHYAD_BASE_MASK      (0x1f<<ETHSW_QPHY_CTRL_PHYAD_BASE_SHIFT)
#define ETHSW_QPHY_CTRL_RESET_SHIFT          8
#define ETHSW_QPHY_CTRL_RESET_MASK           (0x1<<ETHSW_QPHY_CTRL_RESET_SHIFT )
#define ETHSW_QPHY_CTRL_CK25_DIS_SHIFT       7
#define ETHSW_QPHY_CTRL_CK25_DIS_MASK        (0x1<<ETHSW_QPHY_CTRL_CK25_DIS_SHIFT)
#define ETHSW_QPHY_CTRL_EXT_PWR_DOWN_SHIFT   1
#define ETHSW_QPHY_CTRL_EXT_PWR_DOWN_MASK    (0xf<<ETHSW_QPHY_CTRL_EXT_PWR_DOWN_SHIFT)
#define ETHSW_QPHY_CTRL_IDDQ_BIAS_SHIFT      0
#define ETHSW_QPHY_CTRL_IDDQ_BIAS_MASK       (0x1<<ETHSW_QPHY_CTRL_IDDQ_BIAS_SHIFT)
    uint32 qphy_status;                      /* 0x0028 */
    uint32 sphy_ctrl;                        /* 0x002c */
#define ETHSW_SPHY_CTRL_PHYAD_SHIFT          8
#define ETHSW_SPHY_CTRL_PHYAD_MASK           (0x1f<<ETHSW_SPHY_CTRL_PHYAD_SHIFT)
#define ETHSW_SPHY_CTRL_RESET_SHIFT          5
#define ETHSW_SPHY_CTRL_RESET_MASK           (0x1<<ETHSW_SPHY_CTRL_RESET_SHIFT )
#define ETHSW_SPHY_CTRL_EXT_PWR_DOWN_SHIFT   1
#define ETHSW_SPHY_CTRL_EXT_PWR_DOWN_MASK    (0x1<<ETHSW_SPHY_CTRL_EXT_PWR_DOWN_SHIFT)
#define ETHSW_SPHY_CTRL_IDDQ_BIAS_SHIFT      0
#define ETHSW_SPHY_CTRL_IDDQ_BIAS_MASK       (0x1<<ETHSW_SPHY_CTRL_IDDQ_BIAS_SHIFT)
    uint32 sphy_status;                      /* 0x0030 */
    uint32 reserved2[15];                    /* 0x0034 */
    uint32 rgmii_5_ctrl;                     /* 0x0070 */
    uint32 rgmii_5_ib_status;                /* 0x0074 */
    uint32 rgmii_5_rx_clk_delay_ctrl;        /* 0x0078 */
    uint32 rgmii_7_ctrl;                     /* 0x007c */
    uint32 rgmii_7_ib_status;                /* 0x0080 */
    uint32 rgmii_7_rx_clk_delay_ctrl;        /* 0x0084 */
    uint32 led_blink_rate_ctrl;              /* 0x0088 */
    uint32 led_serial_ctrl;                  /* 0x008c */
    uint32 led_ctrl[5];                      /* 0x0090 */
#define ETHSW_LED_CTRL_SPD0_ON               0x0
#define ETHSW_LED_CTRL_SPD0_OFF              0x1
#define ETHSW_LED_CTRL_SPD1_ON               0x0
#define ETHSW_LED_CTRL_SPD1_OFF              0x2
#define ETHSW_LED_CTRL_1000M_SHIFT           6
#define ETHSW_LED_CTRL_100M_SHIFT            4
#define ETHSW_LED_CTRL_10M_SHIFT             2
#define ETHSW_LED_CTRL_NOLINK_SHIFT          0
#define ETHSW_LED_CTRL_SPEED_MASK            0xff
    uint32 reserved3[2];                     /* 0x00a4 */
    uint32 crossbar_switch_ctrl;             /* 0x00ac */
    uint32 reserved4[6];                     /* 0x00b0 */
    uint32 rgmii_11_ctrl;                    /* 0x00c8 */
    uint32 rgmii_11_ib_status;               /* 0x00cc */
    uint32 rgmii_11_rx_clk_delay_ctrl;       /* 0x00d0 */
    uint32 rgmii_12_ctrl;                    /* 0x00d4 */
    uint32 rgmii_12_ib_status;               /* 0x00d8 */
    uint32 rgmii_12_rx_clk_delay_ctrl;       /* 0x00dc */
#define ETHSW_RC_EXT_RVMII                   0x10
#define ETHSW_RC_EXT_GPHY                    0x0c
#define ETHSW_RC_EXT_EPHY                    0x08
#define ETHSW_RC_INT_GPHY                    0x04
#define ETHSW_RC_INT_EPHY                    0x00
#define ETHSW_RC_ID_MODE_DIS                 0x01
    uint32 anonymous1[44];                   /* 0x00e0 */
    uint32 single_serdes_rev;                /* 0x0190 */
    uint32 single_serdes_ctrl;               /* 0x0194 */
    uint32 single_serdes_stat;               /* 0x0198 */
    uint32 led_wan_ctrl;                     /* 0x019c */
} EthernetSwitchReg;

#define ETHSW_REG ((volatile EthernetSwitchReg * const) (SWITCH_BASE + 0x00040000))

typedef struct EthernetSwitchMDIO
{
    uint32 mdio_cmd;                          /* 0x0000 */
#define ETHSW_MDIO_BUSY                       (1 << 29)
#define ETHSW_MDIO_FAIL                       (1 << 28)
#define ETHSW_MDIO_CMD_SHIFT                  26
#define ETHSW_MDIO_CMD_MASK                   (0x3<<ETHSW_MDIO_CMD_SHIFT) 
#define ETHSW_MDIO_CMD_C22_READ               2
#define ETHSW_MDIO_CMD_C22_WRITE              1
#define ETHSW_MDIO_C22_PHY_ADDR_SHIFT         21
#define ETHSW_MDIO_C22_PHY_ADDR_MASK          (0x1f<<ETHSW_MDIO_C22_PHY_ADDR_SHIFT)
#define ETHSW_MDIO_C22_PHY_REG_SHIFT          16
#define ETHSW_MDIO_C22_PHY_REG_MASK           (0x1f<<ETHSW_MDIO_C22_PHY_REG_SHIFT)
#define ETHSW_MDIO_PHY_DATA_SHIFT             0
#define ETHSW_MDIO_PHY_DATA_MASK              (0xffff<<ETHSW_MDIO_PHY_DATA_SHIFT)
    uint32 mdio_cfg;                      /* 0x0004 */
} EthernetSwitchMDIO;

#define ETHSW_MDIO ((volatile EthernetSwitchMDIO * const) (SWITCH_BASE + 0x000403C0))

#ifdef __cplusplus
}
#endif

typedef struct usb_ctrl{
    uint32 setup;
#define USBH_IPP                (1<<5)
#define USBH_IOC                (1<<4)
#define USB_DEVICE_SEL          (1<<11)
    uint32 pll_ctl;
    uint32 fladj_value;
    uint32 bridge_ctl;
#define EHCI_ENDIAN_SWAP        (1<<3)
#define EHCI_DATA_SWAP          (1<<2)
#define OHCI_ENDIAN_SWAP        (1<<1)
#define OHCI_DATA_SWAP          (1<<0)
    uint32 spare1;
    uint32 mdio;
    uint32 mdio2;
    uint32 test_port_control;
    uint32 usb_simctl;
    uint32 usb_testctl;
    uint32 usb_testmon;
    uint32 utmi_ctl_1;
    uint32 spare2;
    uint32 usb_pm;
    uint32 usb_pm_status;
    uint32 spare4;
    uint32 pll_ldo_ctl;
    uint32 pll_ldo_pllbias;
    uint32 pll_afe_bg_cntl;
    uint32 afe_usbio_tst;
    uint32 pll_ndiv_frac;
    uint32 unused1;
    uint32 unused2;
    uint32 unused3;
    uint32 usb30_ctl1;
#define PHY3_PLL_SEQ_START      (1<<4)
#define XHC_SOFT_RESETB         (1<<17)
#define USB3_IOC                (1<<28)
    uint32 usb30_ctl2;
    uint32 usb30_ctl3;
    uint32 usb30_ctl4;
    uint32 usb30_pctl;
    uint32 usb30_ctl5;
    uint32 spare5;
#define GC_PLL_SUSPEND_EN       (1<<1)
} usb_ctrl;

#define USBH_CTRL ((volatile usb_ctrl * const) USBH_CFG_BASE)

typedef struct dslphy_afe {
    uint32 AfeReg[2];
    uint32 Private1[42];
    uint32 BgBiasReg[12];
} dslphy_afe ;

#define DSLPHY_AFE ((volatile dslphy_afe  * const) DSLPHY_AFE_BASE)

/*
** BCM APM Register Structure and definitions
*/
typedef struct ApmControlRegisters_pub
{
  uint32 reserved[75];
  uint32 reg_apm_analog_bg;                      // (0x12c)
#define APM_ANALOG_BG_BOOST (1<<16)

  uint32        reg_codec_config_4;              // (0x130)
#define   APM_LDO_VREGCNTL_7  (1<<(7+8))
} ApmControlRegisters_pub;

#define APM_PUB ((volatile ApmControlRegisters_pub * const) APM_BASE)

#define CONFIG_4x2_CROSSBAR_SUPPORT /* 63148A0 supports only 4x2 crossbar */

typedef struct Jtag_Otp {
   uint32 ctrl0;           /* 0x00 */
#define JTAG_OTP_CTRL_ACCESS_MODE       (0x3 << 22)
#define JTAG_OTP_CTRL_PROG_EN           (1 << 21)
#define JTAG_OTP_CTRL_START             (1 << 0)
   uint32 ctrl1;           /* 0x04 */
#define JTAG_OTP_CTRL_CPU_MODE          (1 << 0)
   uint32 ctrl2;           /* 0x08 */
   uint32 ctrl3;           /* 0x0c */
   uint32 ctrl4;           /* 0x10 */
   uint32 status0;         /* 0x14 */
   uint32 status1;         /* 0x18 */
#define JTAG_OTP_STATUS_1_CMD_DONE      (1 << 1)
} Jtag_Otp;

#define JTAG_OTP ((volatile Jtag_Otp * const) JTAG_OTP_BASE)

#define BTRM_OTP_READ_TIMEOUT_CNT               0x10000

#endif

