/*
<:copyright-BRCM:2015:DUAL/GPL:standard

   Copyright (c) 2015 Broadcom Corporation
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

#include "bl_os_wraper.h"
#include "bcm_misc_hw_init.h"
#include "rdp_map.h"
#include "phys_common_drv.h"
#include "hwapi_mac.h"
#include "pmc_drv.h"
#include "BPCM.h"
#ifndef _CFE_
#include "board.h"
#include "wan_drv.h"
#endif

#define SHIFTL(_a) ( 1 << _a)

#define BPCM_SRESET_CNTL_REG            BPCMRegOffset(sr_control)

#define RDP_S_RST_UBUS_DBR              0 // DMA ubus bridge
#define RDP_S_RST_UBUS_RABR             1 // Runner A ubus bridge
#define RDP_S_RST_UBUS_RBBR             2 // Runner B ubus bridge
#define RDP_S_RST_UBUS_VPBR             3 // VPB ubus bridge
#define RDP_S_RST_RNR_0                 4
#define RDP_S_RST_RNR_1                 5
#define RDP_S_RST_IPSEC_RNR             6
#define RDP_S_RST_IPSEC_RNG             7
#define RDP_S_RST_IPSEC_MAIN            8
#define RDP_S_RST_RNR_SUB               9
#define RDP_S_RST_IH_RNR               10
#define RDP_S_RST_BB_RNR               11
#define RDP_S_RST_GEN_MAIN             12 // (main tm)
#define RDP_S_RST_VDSL                 13
#define RDP_S_RST_E0_MAIN              14 // (emac0)
#define RDP_S_RST_E0_RST_L             15 // (emac0)
#define RDP_S_RST_E1_MAIN              16 // (emac1)
#define RDP_S_RST_E1_RST_L             17 // (emac1)
#define RDP_S_RST_E2_MAIN              18 // (emac2)
#define RDP_S_RST_E2_RST_L             19 // (emac2)
#define RDP_S_RST_E3_MAIN              20 // (emac3)
#define RDP_S_RST_E3_RST_L             21 // (emac3)
#define RDP_S_RST_E4_MAIN              22 // (emac4)
#define RDP_S_RST_E4_RST_L             23 // (emac4)

static const ETHERNET_MAC_INFO *emac_info;

static void pmc_rdp_module_unreset(uint32_t rdp_module)
{
    uint32_t reg;
    uint32_t ret;

    ret = ReadBPCMRegister(PMB_ADDR_RDP, BPCM_SRESET_CNTL_REG, (uint32 *)&reg);
    if (ret)
        printk("Failed to ReadBPCMRegister RDP block BPCM_SRESET_CNTL_REG. Error=%d\n", ret);

    reg &= ~(SHIFTL(rdp_module));

    ret = WriteBPCMRegister(PMB_ADDR_RDP, BPCM_SRESET_CNTL_REG, reg);
    if (ret)
        printk("Failed to WriteBPCMRegister RDP block BPCM_SRESET_CNTL_REG. Error=%d\n", ret);
}

static void pmc_rdp_reset(void)
{
    uint32_t ret;

    ret = WriteBPCMRegister(PMB_ADDR_RDP, BPCM_SRESET_CNTL_REG, ~0);
    if (ret)
        printk("Failed to WriteBPCMRegister RDP block BPCM_SRESET_CNTL_REG. Error=%d\n", ret);
}

static void ubus_masters_enable(void)
{
    uint32_t reg;

    READ_32(UBUS_MASTER_1_RDP_UBUS_MASTER_BRDG_REG_EN, reg);
    reg |= 1;
    WRITE_32(UBUS_MASTER_1_RDP_UBUS_MASTER_BRDG_REG_EN, reg);

    READ_32(UBUS_MASTER_2_RDP_UBUS_MASTER_BRDG_REG_EN, reg);
    reg |= 1;
    WRITE_32(UBUS_MASTER_2_RDP_UBUS_MASTER_BRDG_REG_EN, reg);

    READ_32(UBUS_MASTER_3_RDP_UBUS_MASTER_BRDG_REG_EN, reg);
    reg |= 1;
    WRITE_32(UBUS_MASTER_3_RDP_UBUS_MASTER_BRDG_REG_EN, reg);

    READ_32(UBUS_MASTER_3_RDP_UBUS_MASTER_BRDG_REG_HP, reg);
    reg |= 0xf0e01; // allow forwarding of Urgent to High priority on UBUS
    WRITE_32(UBUS_MASTER_3_RDP_UBUS_MASTER_BRDG_REG_HP,reg);
}

int rdp_pre_init(void)
{
    uint32_t iter;

    if ((emac_info = BpGetEthernetMacInfoArrayPtr()) == NULL)
    {
        printk("Error reading Ethernet MAC info from board params\n");
        return -1;
    }

    pmc_rdp_reset();

    pmc_rdp_module_unreset(RDP_S_RST_UBUS_DBR);
    pmc_rdp_module_unreset(RDP_S_RST_UBUS_RABR);
    pmc_rdp_module_unreset(RDP_S_RST_UBUS_RBBR);
    pmc_rdp_module_unreset(RDP_S_RST_UBUS_VPBR);

    ubus_masters_enable();

    pmc_rdp_module_unreset(RDP_S_RST_BB_RNR);
    udelay(100);

    pmc_rdp_module_unreset(RDP_S_RST_RNR_SUB);
    pmc_rdp_module_unreset(RDP_S_RST_RNR_1);
    pmc_rdp_module_unreset(RDP_S_RST_RNR_0);
    pmc_rdp_module_unreset(RDP_S_RST_IPSEC_RNR);
    pmc_rdp_module_unreset(RDP_S_RST_IPSEC_RNG);
    pmc_rdp_module_unreset(RDP_S_RST_IPSEC_MAIN);
    pmc_rdp_module_unreset(RDP_S_RST_GEN_MAIN);
    pmc_rdp_module_unreset(RDP_S_RST_IH_RNR);
    pmc_rdp_module_unreset(RDP_S_RST_BB_RNR);

    for (iter = 0; iter < 4; iter++)
    {
        if (emac_info->sw.port_map & (1 << iter))
        {
            pmc_rdp_module_unreset(RDP_S_RST_E0_MAIN + iter * 2);
            pmc_rdp_module_unreset(RDP_S_RST_E0_RST_L + iter * 2);
        }
    }

    return 0;
}
EXPORT_SYMBOL(rdp_pre_init);

#ifndef _CFE_
int bcm_misc_g9991_debug_port_get(void)
{
    int iter;

    for (iter = 0; iter < BP_MAX_SWITCH_PORTS; iter++)
    {
        if (emac_info[0].sw.port_map & (1 << iter) &&
            emac_info[0].sw.port_flags[iter] & PORT_FLAG_MGMT)
        {
            return (rdpa_emac)(rdpa_emac0 + iter);
        }
    }

    return -1;
}
EXPORT_SYMBOL(bcm_misc_g9991_debug_port_get);

int rdp_post_init_fiber(void)
{
    unsigned short tx_gpio;

    /* Init UNIMAC 5 if AE is enabled */
    if (wan_serdes_type_get() == SERDES_WAN_TYPE_AE && emac_info->sw.port_map & (1<<5))
    {
        mac_hwapi_init_emac(5);
        mac_hwapi_set_unimac_cfg(5);
        mac_hwapi_set_rxtx_enable(5, 1, 1);
    }

    /* Configure PON GPIO */
    if (BpGetPonTxEnGpio(&tx_gpio) == BP_SUCCESS)
    {
        kerSysSetGpioDir(tx_gpio & BP_GPIO_NUM_MASK);
        kerSysSetGpioState((tx_gpio & BP_GPIO_NUM_MASK), (tx_gpio & BP_ACTIVE_LOW) ? kGpioInactive : kGpioActive);
    }

    return 0;
}
EXPORT_SYMBOL(rdp_post_init_fiber);
#endif

int rdp_post_init(void)
{
    uint32_t iter;

    /* Init EGPHY before accessing UNIMACs */
    phy_reset(emac_info[0].sw.port_map);

    /* Init UNIMAC 0-3 */
    for (iter = 0; iter < 4; iter++)
    {
        if (emac_info->sw.port_map & (1<<iter))
        {
            mac_hwapi_init_emac(iter);
            mac_hwapi_set_unimac_cfg(iter);
            mac_hwapi_set_rxtx_enable(iter, 1, 1);
        }
    }

    return 0;
}
EXPORT_SYMBOL(rdp_post_init);

int rdp_shut_down(void)
{
    pmc_rdp_reset();

    return 0;
}
EXPORT_SYMBOL(rdp_shut_down);

int bcm_misc_hw_init(void)
{
    rdp_pre_init();

    return 0;
}

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

int proc_show_rdp_mem(char *buf, char **start, off_t off, int cnt, int *eof, void *data)
{
    int rc;
    void *tm_base_addr;
    void *mc_base_addr;
    int size_dummy;
    int n = 0;

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

    n = sprintf(buf, "RDP MEM tm_base=%pK mc_base=%pK\n",tm_base_addr, mc_base_addr);
    return n;

}

arch_initcall(bcm_misc_hw_init);
#endif
