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
/*
 ***************************************************************************
 * File Name  :bcm_misc_hw_init_impl3.c
 *
 * Description: This file contains the flash device driver APIs for bcm63xx board.
 *
 * Created on :  7/23/2014  yonatani
 *
 ***************************************************************************/
#include "pmc_drv.h"
#include "clk_rst.h"
#include "egphy_drv_impl1.h"
#include "hwapi_mac.h"
#include "boardparms.h"
#include "phys_common_drv.h"
#include "rdp_drv_bbh.h"
#include "bcm_map_part.h"
#include "mdio_drv_impl1.h"

#ifndef _CFE_
#include "board.h"
#endif
#include "bcm_misc_hw_init.h"

#define SHIFTL(_a) ( 1 << _a)

#define PBCM_RESET 0
#define PBCM_UNRESET 1

#define BPCM_SRESET_CNTL_REG            8 //0x20 in words offset
/*BPCM-RDP*/
#define RDP_S_RST_UBUS_DBR              0// (dma ubus bridge)
#define RDP_S_RST_UBUS_RABR             1//(rnrA ubus bridge)
#define RDP_S_RST_UBUS_RBBR             2// (rnrB ubus bridge)
#define RDP_S_RST_UBUS_VPBR             3//(vpb ubus bridge)
#define RDP_S_RST_RNR_0                 4//
#define RDP_S_RST_RNR_1                 5//
#define RDP_S_RST_IPSEC_RNR             6//
#define RDP_S_RST_IPSEC_RNG             7//
#define RDP_S_RST_IPSEC_MAIN            8
#define RDP_S_RST_RNR_SUB               9
#define RDP_S_RST_IH_RNR               10
#define RDP_S_RST_BB_RNR               11
#define RDP_S_RST_GEN_MAIN             12// (main tm)
#define RDP_S_RST_VDSL                 13
#define RDP_S_RST_E0_MAIN              14// (mac0)
#define RDP_S_RST_E0_RST_L             15// (mac0)
#define RDP_S_RST_E1_MAIN              16// (mac1)
#define RDP_S_RST_E1_RST_L             17// (mac1)
#define RDP_S_RST_E2_MAIN              18// (mac2)
#define RDP_S_RST_E2_RST_L             19// (mac2)
#define RDP_S_RST_E3_MAIN              20// (mac3)
#define RDP_S_RST_E3_RST_L             21// (mac3)
#define RDP_S_RST_E4_MAIN              22// (mac4)
#define RDP_S_RST_E4_RST_L             23// (mac4)

static  const ETHERNET_MAC_INFO*   pBoardInfo;


int rdp_pre_init(void);
int rdp_post_init(void);
int rdp_post_init_fiber(void);
int rdp_shut_down(void);
int bcm_misc_g9991_debug_port_get(void);
#if defined(CONFIG_BCM96838) 
#if !defined(_CFE_)
int init_serdes_proc(void);
void cleanup_serdes_proc(void);
#endif
#endif

/*pmcSetModuleResetState used to set the reset state of
 * each rdp block module, the state is (1) - in reset state, un-oprational.
 * (0) - out of reset state - operational.
 */
static void pmcSetModuleResetState(uint32_t rdpModule,uint32_t state)
{
    uint32_t    bpcmResReg;
    uint32_t    error;


    error = ReadBPCMRegister(PMB_ADDR_RDP, BPCM_SRESET_CNTL_REG, (uint32*)&bpcmResReg);
    if( error )
    {
        printk("Failed to ReadBPCMRegister RDP block BPCM_SRESET_CNTL_REG error = %d\n",error);
    }

    if ( state )
    {
        bpcmResReg |= SHIFTL(rdpModule);
    }
    else
    {
        bpcmResReg &= ~SHIFTL(rdpModule);
    }

    error = WriteBPCMRegister(PMB_ADDR_RDP, BPCM_SRESET_CNTL_REG, bpcmResReg);

    if( error )
    {
        printk("Failed to WriteBPCMRegister RDP block BPCM_SRESET_CNTL_REG error = %d\n",error);
    }
}

static void pmcPutAllRdpModulesInReset(void)
{
    uint32_t    bpcmResReg = 0;
    uint32_t    error;
    error = WriteBPCMRegister(PMB_ADDR_RDP, BPCM_SRESET_CNTL_REG, bpcmResReg);

    if( error )
    {
        printk("Failed to WriteBPCMRegister RDP block BPCM_SRESET_CNTL_REG\n");
    }

}

static void f_enable_ubus_masters(void)
{
    uint32_t reg;

    /*first Ubus Master*/
    READ_32(UBUS_MASTER_1_RDP_UBUS_MASTER_BRDG_REG_EN,reg);
    reg |= 1; // bit 0 is the enable bit
    WRITE_32(UBUS_MASTER_1_RDP_UBUS_MASTER_BRDG_REG_EN,reg);

    /*second Ubus Master*/
    READ_32(UBUS_MASTER_2_RDP_UBUS_MASTER_BRDG_REG_EN,reg);
    reg |= 1; // bit 0 is the enable bit
    WRITE_32(UBUS_MASTER_2_RDP_UBUS_MASTER_BRDG_REG_EN,reg);

    /*third Ubus Master*/
    READ_32(UBUS_MASTER_3_RDP_UBUS_MASTER_BRDG_REG_EN,reg);
    reg |= 1; // bit 0 is the enable bit
    WRITE_32(UBUS_MASTER_3_RDP_UBUS_MASTER_BRDG_REG_EN,reg);

    READ_32(UBUS_MASTER_3_RDP_UBUS_MASTER_BRDG_REG_HP,reg);
    reg |= 0xf0e01; // allow forwarding of Urgent to High priority on UBUS
    WRITE_32(UBUS_MASTER_3_RDP_UBUS_MASTER_BRDG_REG_HP,reg);
}


static void ResetUnresetRdpBlock(void)
{
    /*put all RDP modules in reset state*/
    pmcPutAllRdpModulesInReset();

    /*when Oren is out of reset, RDP block reset bits are all set to reset,
     * first we have to take out of reset the Broadbus and VPB*/
    pmcSetModuleResetState(RDP_S_RST_UBUS_DBR,PBCM_UNRESET);
    pmcSetModuleResetState(RDP_S_RST_UBUS_RABR,PBCM_UNRESET);
    pmcSetModuleResetState(RDP_S_RST_UBUS_RBBR,PBCM_UNRESET);
    pmcSetModuleResetState(RDP_S_RST_UBUS_VPBR,PBCM_UNRESET);


    f_enable_ubus_masters();
}

int rdp_pre_init(void)
{
    uint32_t macIter;

    if ( (pBoardInfo = BpGetEthernetMacInfoArrayPtr()) == NULL )
    {
      printk("ERROR:BoardID not Set in BoardParams\n");
      return -1;
    }

    ResetUnresetRdpBlock();

    pmcSetModuleResetState(RDP_S_RST_BB_RNR,PBCM_UNRESET);

    udelay(100);

    /*take out of reset Runners 0 and 1*/
    pmcSetModuleResetState(RDP_S_RST_RNR_SUB,PBCM_UNRESET);
    pmcSetModuleResetState(RDP_S_RST_RNR_1,PBCM_UNRESET);
    pmcSetModuleResetState(RDP_S_RST_RNR_0,PBCM_UNRESET);
    pmcSetModuleResetState(RDP_S_RST_IPSEC_RNR,PBCM_UNRESET);
    pmcSetModuleResetState(RDP_S_RST_IPSEC_RNG,PBCM_UNRESET);
    pmcSetModuleResetState(RDP_S_RST_IPSEC_MAIN,PBCM_UNRESET);
    pmcSetModuleResetState(RDP_S_RST_GEN_MAIN,PBCM_UNRESET);

    /*take IH out of*/
    pmcSetModuleResetState(RDP_S_RST_IH_RNR,PBCM_UNRESET);

    /*take TM out of reset*/
    pmcSetModuleResetState(RDP_S_RST_BB_RNR,PBCM_UNRESET);

    for ( macIter =0; macIter <= DRV_BBH_EMAC_4 ; macIter++)
    {
       if(pBoardInfo[0].sw.port_map & (1<<macIter))
       {
               pmcSetModuleResetState((macIter*2) + RDP_S_RST_E0_MAIN,PBCM_UNRESET);
               pmcSetModuleResetState((macIter*2) + RDP_S_RST_E0_RST_L,PBCM_UNRESET);
       }
    }

    if(WriteBPCMRegister(PMB_ADDR_CHIP_CLKRST, 0xE, 0x33))
    {
        printk("failed to configure PMB CLKRST 0xE\n");
    }
    if(WriteBPCMRegister(PMB_ADDR_CHIP_CLKRST, 0xF, 0xFF))
    {
        printk("failed to configure PMB CLKRST 0xF\n");
    }

    /* XXX workaround for [JIRA SWBCACPE-14083]:
     * Reducing the header hold trigger threshold (hold asserted when 3 transactions are in queue) */
    MWRITE_32(0xb200088c, 0x00000033);

#if defined(CONFIG_BCM96838)
#if !defined(_CFE_)
    init_serdes_proc();
#endif
#endif

    return 0;
}
EXPORT_SYMBOL(rdp_pre_init);

#ifndef _CFE_
int runner_reserved_memory_get(uint32_t *tm_base_addr, uint32_t *mc_base_addr,
    uint32_t *tm_size)
{
    int rc;
    uint32_t size_dummy;

    rc = BcmMemReserveGetByName(TM_BASE_ADDR_STR, (void**)tm_base_addr, &size_dummy);
    *tm_size = size_dummy / 1024 / 1024;
    if (rc)
    {
        printk("%s %s Failed to get TM_BASE_ADDR_STR rc(%d)\n", __FILE__, __FUNCTION__, rc);
        return rc;
    }

    rc = BcmMemReserveGetByName(TM_MC_BASE_ADDR_STR, (void**)mc_base_addr, &size_dummy);
    if (rc)
        printk("%s %s Failed to get TM_BASE_ADDR_STR rc(%d)\n", __FILE__, __FUNCTION__, rc);

    return rc;
}
EXPORT_SYMBOL(runner_reserved_memory_get);
#endif

int bcm_misc_g9991_debug_port_get(void)
{
    int i;

    for (i = 0; i < BP_MAX_SWITCH_PORTS; ++i)
    {
        if (pBoardInfo[0].sw.port_map & (1 << i) &&
            pBoardInfo[0].sw.port_flags[i] & PORT_FLAG_MGMT)
        {
            return (rdpa_emac)(i + rdpa_emac0);
        }
    }

    return -1;
}
EXPORT_SYMBOL(bcm_misc_g9991_debug_port_get);

#ifndef _CFE_
static int init_pcs(void)
{
    int rc = 0;
#define PCS_MDIO_PHYID 2
    uint16_t mdio_data;

    mdio_data = 0x8000;
    rc = mdio_write_c22_register(MDIO_AE ,PCS_MDIO_PHYID, MII_BMCR, mdio_data);
    if (rc) {
      goto exit;
    }

    //autodet_en
    mdio_data = 0x4110;
    rc = mdio_write_c22_register(MDIO_AE ,PCS_MDIO_PHYID, MII_XCTL, mdio_data);
    if (rc)
      goto exit;

    rc = mdio_read_c22_register(MDIO_AE ,PCS_MDIO_PHYID, MII_FCSCOUNTER, &mdio_data);
    if (rc)
      goto exit;

    //turn on support of 100/10
    mdio_data |= 0x0800;
    rc = mdio_write_c22_register(MDIO_AE ,PCS_MDIO_PHYID, MII_FCSCOUNTER, mdio_data);
    if (rc)
      goto exit;


    //now finish to configure BMCR, different if SGMII phy is connected.
    if( pBoardInfo[0].sw.phy_id[5] & PHY_EXTERNAL)
    {
        mdio_data = 0x1140;
        rc = mdio_write_c22_register(MDIO_AE ,PCS_MDIO_PHYID, MII_BMCR, mdio_data);
    }
    else
    {
        mdio_data = 0x140;
        rc = mdio_write_c22_register(MDIO_AE ,PCS_MDIO_PHYID, MII_BMCR, mdio_data);
    }

exit:
    if (rc) 
    {
      printk("Failed on MDIO transaction to AE PCS\n");
    }
    return rc;
}
#endif

int rdp_post_init_fiber(void)
{
#ifndef _CFE_
    uint32_t wan_reg;

    /* Check if AE is enabled */
    READ_32(WAN_MISC_RDP_WAN_TOP_WAN_MISC_WAN_CFG,wan_reg);
    if ((wan_reg & 0x3) == 0x2)
    {
        mac_hwapi_init_emac(DRV_BBH_EMAC_5);
        mac_hwapi_set_rxtx_enable(DRV_BBH_EMAC_5,1,1);
        if(init_pcs())
        {
            printk("Failed to init_pcs()\n");
            return -1;
        }
    }
    else
    {
        /* configure PON gpio */
        unsigned short rx_gpio;
        unsigned short tx_gpio;
        unsigned short reset_gpio;
        int rc = 0;

        rc = BpGetPonRxEnGpio(&rx_gpio);

        rc |= BpGetPonTxEnGpio(&tx_gpio);
        if (rc != BP_SUCCESS)
        {
           printk("No PON gpio found in board parameters\n");
           return 0;
        }

        kerSysSetGpioDir(rx_gpio & BP_GPIO_NUM_MASK);
        kerSysSetGpioState((rx_gpio & BP_GPIO_NUM_MASK), (rx_gpio & BP_ACTIVE_LOW) ? kGpioInactive : kGpioActive);

        kerSysSetGpioDir(tx_gpio & BP_GPIO_NUM_MASK);
        kerSysSetGpioState((tx_gpio & BP_GPIO_NUM_MASK), (tx_gpio & BP_ACTIVE_LOW) ? kGpioInactive : kGpioActive);

        rc = BpGetPonResetGpio(&reset_gpio);
        if (rc == BP_SUCCESS)
        {
            kerSysSetGpioDir(reset_gpio & BP_GPIO_NUM_MASK);
            kerSysSetGpioState((reset_gpio & BP_GPIO_NUM_MASK), (reset_gpio & BP_ACTIVE_LOW) ? kGpioInactive : kGpioActive);
        }
        else if (rc != BP_VALUE_NOT_DEFINED)
            printk("init reset GPIO failed rc= %d\n", rc);

    }

#endif
    return 0;
}
EXPORT_SYMBOL(rdp_post_init_fiber);


int rdp_post_init(void)
{
    DRV_BBH_PORT_INDEX  macIter;
    uint32_t            rgmii_out_reg = 0;
    uint32_t            xmii_port;
    char                boardIdStr[BP_BOARD_ID_LEN];

    if (BpGetBoardId(boardIdStr) == BP_BOARD_ID_NOT_SET)
    {
        printk("Data Path init didn't finished \n");
        return -1;
    }

    /*init EGPHY before accessing unimacs*/
    phy_reset(pBoardInfo[0].sw.port_map);

    for ( macIter =  DRV_BBH_EMAC_0; macIter <= DRV_BBH_EMAC_4; macIter++)
    {
        if(pBoardInfo[0].sw.port_map & (1<<macIter))
        {
            mac_hwapi_init_emac(macIter);
            mac_hwapi_set_unimac_cfg(macIter);
            mac_hwapi_set_rxtx_enable(macIter,1,1);
        }

    }

    /*handle the xMII port*/
    xmii_port = pBoardInfo[0].sw.phy_id[4];
    if (xmii_port)
	{
	    if (IsRgmiiDirect(pBoardInfo[0].sw.phy_id[4]) || IsExtPhyId(pBoardInfo[0].sw.phy_id[4]))
	    {
	        rgmii_out_reg = EGPHY_RGMII_OUT_PORT_MODE_RGMII | (EGPHY_RGMII_OUT_REF_50_MHZ << EGPHY_RGMII_OUT_REF_OFFSET);
            if(IsPortTxInternalDelay(pBoardInfo[0].sw.port_flags[4]))
                    rgmii_out_reg |= 1<<EGPHY_RGMII_OUT_PORT_ID_OFFSET;
                printk("Set EMAC4 as RGMII\n");
	    }
	    else if(IsTMII(pBoardInfo[0].sw.phy_id[4]))
        {
	        rgmii_out_reg = EGPHY_RGMII_OUT_PORT_MODE_MII | (EGPHY_RGMII_OUT_REF_50_MHZ << EGPHY_RGMII_OUT_REF_OFFSET);
	        printk("Set EMAC4 as TMII\n");
        }
	    WRITE_32(EGPHY_RDP_UBUS_MISC_EGPHY_RGMII_OUT,rgmii_out_reg);
	}

    return 0;
}
EXPORT_SYMBOL(rdp_post_init);

int rdp_shut_down(void)
{
    /*put all RDP modules in reset state*/
    pmcPutAllRdpModulesInReset();

#if defined(CONFIG_BCM96838)
#if !defined(_CFE_)
    cleanup_serdes_proc();
#endif
#endif
    return 0;
}
EXPORT_SYMBOL(rdp_shut_down);

int bcm_misc_hw_init(void)
{
    rdp_pre_init();

    return 0;
}
#ifndef _CFE_
int proc_show_rdp_mem(char *buf, char **start, off_t off, int cnt, int *eof, void *data)
{
    int     rc;
    void*   tm_base_addr;
    void*   mc_base_addr;
    int     size_dummy;
    int     n = 0;

    rc = BcmMemReserveGetByName(TM_BASE_ADDR_STR, (void**)&tm_base_addr, &size_dummy);
    if (rc)
    {
       printk("%s %s Failed to get TM_BASE_ADDR_STR rc(%d)\n", __FILE__, __FUNCTION__, rc);
       return -1;
    }
    rc = BcmMemReserveGetByName(TM_MC_BASE_ADDR_STR, (void**)&mc_base_addr, &size_dummy);
    if (rc)
    {
       printk("%s %s Failed to get TM_BASE_ADDR_STR rc(%d)\n", __FILE__, __FUNCTION__, rc);
       return -1;
    }

    n = sprintf(buf, "RDP MEM tm_base=%pK mc_base=%pK\n",tm_base_addr,mc_base_addr);
    return n;

}


arch_initcall(bcm_misc_hw_init);
#endif
