/*
   Copyright (c) 2015 Broadcom Corporation
   All Rights Reserved

    <:label-BRCM:2015:DUAL/GPL:standard
    
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

#include "access_macros.h"
#include "mdio_drv_impl2.h"
#include "egphy_drv_impl2.h"
#include "pmc_drv.h"
#ifndef _CFE_
#include "wan_drv.h"
#endif

#define EGPHY_BASE                          0x130d3000
#define EGPHY_UBUS_MISC_EGPHY_GPHY_OUT      (EGPHY_BASE + 0x00)
#define EGPHY_UBUS_MISC_EGPHY_RGMII_OUT     (EGPHY_BASE + 0x04)
#define EGPHY_UBUS_MISC_EGPHY_GPHY_IN       (EGPHY_BASE + 0x08)	 
#define EGPHY_UBUS_MISC_EGPHY_GPHY_IN2      (EGPHY_BASE + 0x0c)	 
#define EGPHY_UBUS_MISC_EGPHY_ECO_0         (EGPHY_BASE + 0x10)	 
#define EGPHY_UBUS_MISC_EGPHY_ECO_1         (EGPHY_BASE + 0x14)	 
#define EGPHY_RGMII_0_MUX_CONFIG            (EGPHY_BASE + 0x18)	 
#define EGPHY_EPHY_PHY_AD                   (EGPHY_BASE + 0x1c)	 
#define EGPHY_EPHY_PWR_MGNT                 (EGPHY_BASE + 0x20)	 
#define EGPHY_EPHY_RESET_CNTRL              (EGPHY_BASE + 0x24)	 
#define EGPHY_EPHY_REF_CLK_SEL              (EGPHY_BASE + 0x28)	 
#define EGPHY_EPHY_TEST_CNTRL               (EGPHY_BASE + 0x2c)	 
#define EGPHY_EPHY_PHY_SELECT               (EGPHY_BASE + 0x30)	 
#define EGPHY_RGMII_0_CNTRL                 (EGPHY_BASE + 0x38)	 
#define EGPHY_RGMII_0_IB_STATUS             (EGPHY_BASE + 0x3c)	 
#define EGPHY_RGMII_0_RX_CLOCK_DELAY_CNTRL  (EGPHY_BASE + 0x40)
#define EGPHY_RGMII_0_ATE_RX_CNTRL          (EGPHY_BASE + 0x44)	 
#define EGPHY_RGMII_0_ATE_RX_STATUS         (EGPHY_BASE + 0x48)	 
#define EGPHY_RGMII_0_ATE_TX_CNTRL_0        (EGPHY_BASE + 0x4c)	 
#define EGPHY_RGMII_0_ATE_TX_CNTRL_1        (EGPHY_BASE + 0x50)	 
#define EGPHY_SINGLE_SERDES_REV             (EGPHY_BASE + 0x54)	 
#define EGPHY_SINGLE_SERDES_STAT            (EGPHY_BASE + 0x58)	 
#define EGPHY_SINGLE_SERDES_CNTRL           (EGPHY_BASE + 0x5c)	 
#define EGPHY_SINGLE_SERDES_APD_CNTRL       (EGPHY_BASE + 0x60)	 
#define EGPHY_SINGLE_SERDES_APD_FSM_CNTRL   (EGPHY_BASE + 0x64)
#define EGPHY_RGMII_0_TX_DELAY_CNTRL_0      (EGPHY_BASE + 0x68)	 
#define EGPHY_RGMII_0_TX_DELAY_CNTRL_1      (EGPHY_BASE + 0x6c)	 
#define EGPHY_RGMII_0_RX_DELAY_CNTRL_0      (EGPHY_BASE + 0x70)	 
#define EGPHY_RGMII_0_RX_DELAY_CNTRL_1      (EGPHY_BASE + 0x74)	 
#define EGPHY_RGMII_0_RX_DELAY_CNTRL_2      (EGPHY_BASE + 0x78)	 

#define EXP_REG_BASE                        0x0f00

#define EXP_REG_ADDRRESS       			    0x17
#define EXP_REG_VALUE       			    0x15

#define RDB_REG_ADDRRESS       			    0x1e
#define RDB_REG_VALUE       			    0x1f

#define EGPHY_ERROR -1
#define EGPHY_OK     0

#pragma pack(push,1)
typedef struct
{
    uint32_t reserved:6;
    uint32_t gphy_testmux_sw_init:1;
    uint32_t gphy_pll_ref_clk_sel:2;
    uint32_t phy_pll_clk_sel_div5:2;
    uint32_t phy_sel:2;
    uint32_t phy_test_mode:4;
    uint32_t PHY_TEST_EN:1;
    uint32_t PHYA:5;
    uint32_t GPHY_CK25_DIS:1;
    uint32_t IDDQ_BIAS:1;
    uint32_t DLL_EN:1;
    uint32_t PWRDWN:4;
    uint32_t iddq_global_pwr:1;
    uint32_t RST:1;
} egphy_gphy_out_reg_t;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct
{
    uint32_t select_sgmii:1;
    uint32_t reserved3:5;
    uint32_t refclk_freq_sel:5;
    uint32_t link_down_tx_dis:1;
    uint32_t serdes_test_en:1;
    uint32_t mdio_st:1;
    uint32_t serdes_devad:5;
    uint32_t serdes_prtad:5;
    uint32_t reserved1:2;
    uint32_t serdes_reset:1;
    uint32_t reset_mdioregs:1;
    uint32_t reset_pll:1;
    uint32_t reserved0:1;
    uint32_t pwrdwn:1;
    uint32_t iddq:1;
} egphy_single_serdes_ctrl_t;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct
{
    uint32_t reserved:22;
    uint32_t apd_state:3;
    uint32_t debounced_signal_detect:1;
    uint32_t pll_lock:1;
    uint32_t sync_status:1;
    uint32_t sgmii:1;
    uint32_t rxseqdone1g:1;
    uint32_t rx_sigdet:1;
    uint32_t link_status:1;
} egphy_single_serdes_stat_t;
#pragma pack(pop)

uint16_t sgmii_read_register(uint32_t phy_addr, uint16_t reg_offset)
{
    uint16_t value = 0;
    uint32_t rdb_enabled = reg_offset & RDB_REG_ACCESS;

    if (rdb_enabled)
    {
        if (mdio_write_c22_register(MDIO_INT, phy_addr, 0x1f, reg_offset & 0xfff0) != MDIO_OK)
            goto Exit;

        if (mdio_read_c22_register(MDIO_INT, phy_addr, 0x10 | (reg_offset & 0x000f), &value) != MDIO_OK)
            goto Exit;

        if (mdio_write_c22_register(MDIO_INT, phy_addr, 0x1f, 0x8000) != MDIO_OK)
            goto Exit;
    }
    else
    {
        if (mdio_read_c22_register(MDIO_INT, phy_addr, reg_offset, &value) != MDIO_OK)
            goto Exit;
    }

Exit:
    return value;
}
EXPORT_SYMBOL(sgmii_read_register);

static inline int32_t egphy_exp_reg_write(uint32_t phy_addr, uint16_t reg_offset, uint16_t value)
{
    if (mdio_write_c22_register(MDIO_INT, phy_addr, EXP_REG_ADDRRESS, EXP_REG_BASE + reg_offset) != MDIO_OK)
        return EGPHY_ERROR;

    if (mdio_write_c22_register(MDIO_INT, phy_addr, EXP_REG_VALUE, value) != MDIO_OK)
        return EGPHY_ERROR;

    return EGPHY_OK;
}

static inline int32_t egphy_exp_reg_read(uint32_t phy_addr, uint16_t reg_offset, uint16_t *value)
{
    if (mdio_write_c22_register(MDIO_INT, phy_addr, EXP_REG_ADDRRESS, EXP_REG_BASE + reg_offset) != MDIO_OK)
        return EGPHY_ERROR;

    if (mdio_read_c22_register(MDIO_INT, phy_addr, EXP_REG_VALUE, value) != MDIO_OK)
        return EGPHY_ERROR;

    return EGPHY_OK;
}

static inline int32_t egphy_rdb_reg_write(uint32_t phy_addr, uint16_t reg_offset, uint16_t value)
{
    if (mdio_write_c22_register(MDIO_INT, phy_addr, RDB_REG_ADDRRESS, reg_offset) != MDIO_OK)
        return EGPHY_ERROR;

    if (mdio_write_c22_register(MDIO_INT, phy_addr, RDB_REG_VALUE, value) != MDIO_OK)
        return EGPHY_ERROR;

    return EGPHY_OK;
}

static inline int32_t egphy_rdb_reg_read(uint32_t phy_addr, uint16_t reg_offset, uint16_t *value)
{
    if (mdio_write_c22_register(MDIO_INT, phy_addr, RDB_REG_ADDRRESS, reg_offset) != MDIO_OK)
        return EGPHY_ERROR;

    if (mdio_read_c22_register(MDIO_INT, phy_addr, RDB_REG_VALUE, value) != MDIO_OK)
        return EGPHY_ERROR;

    return EGPHY_OK;
}

static inline int32_t egphy_rdb_mode_set(uint32_t phy_addr, uint32_t enable)
{
    if (enable)
        return egphy_exp_reg_write(phy_addr, 0x7e, 0x0000);
    else
        return egphy_rdb_reg_write(phy_addr, 0x87, 0x8000);
}

uint16_t egphy_read_register(uint32_t phy_addr, uint16_t reg_offset)
{
    uint16_t value = 0;
    uint32_t rdb_enabled = reg_offset & RDB_REG_ACCESS;

    if (rdb_enabled)
    {
        if (egphy_rdb_mode_set(phy_addr, 1) != EGPHY_OK)
            goto Exit;

        if (egphy_rdb_reg_read(phy_addr, reg_offset, &value) != EGPHY_OK) 
            goto Exit;

        if (egphy_rdb_mode_set(phy_addr, 0) != EGPHY_OK)
            goto Exit;
    }
    else
    {
        if (mdio_read_c22_register(MDIO_INT, phy_addr, reg_offset, &value) != MDIO_OK)
            goto Exit;
    }

Exit:
    return value;
}
EXPORT_SYMBOL(egphy_read_register);

int32_t egphy_write_register(uint32_t phy_addr, uint16_t reg_offset, uint16_t value)
{
    int32_t ret = EGPHY_ERROR;
    uint32_t rdb_enabled = reg_offset & RDB_REG_ACCESS;

    if (rdb_enabled)
    {
        if ((ret = egphy_rdb_mode_set(phy_addr, 1)) != EGPHY_OK)
            goto Exit;

        if ((ret = egphy_rdb_reg_write(phy_addr, reg_offset, value)) != EGPHY_OK) 
            goto Exit;

        if ((ret = egphy_rdb_mode_set(phy_addr, 0)) != EGPHY_OK)
            goto Exit;
    }
    else
    {
        if ((ret = mdio_write_c22_register(MDIO_INT, phy_addr, reg_offset, value)) != MDIO_OK)
            goto Exit;
        else
            ret = EGPHY_OK;
    }

Exit:
    return ret;
}
EXPORT_SYMBOL(egphy_write_register);

static void ephy_init(uint32_t phy_map)
{
    uint32_t data;

    /* RESET_N --> 1 */
    READ_32(EGPHY_EPHY_RESET_CNTRL, data);
    data |= 0x10;
    WRITE_32(EGPHY_EPHY_RESET_CNTRL, data);
    udelay(50);

    /* DLLBYP_PIN_F --> 1 */
    data = 0x02;
    WRITE_32(EGPHY_EPHY_TEST_CNTRL, data);
    udelay(50);

    /* RESET_N --> 0 */
    READ_32(EGPHY_EPHY_RESET_CNTRL, data);
    data &= ~0x10;
    WRITE_32(EGPHY_EPHY_RESET_CNTRL, data);
    udelay(50);

    /* DLLBYP_PIN_F --> 1 */
    data = 0x02;
    WRITE_32(EGPHY_EPHY_TEST_CNTRL, data);
    udelay(50);

    /* RESET_N_PHY_(0-3), RESET_N --> 1 */
    data |= 0x1f;
    WRITE_32(EGPHY_EPHY_RESET_CNTRL, data);
    udelay(50);

    /* RESET_N_PHY_(0-3), RESET_N --> 0 */
    data = 0x00;
    WRITE_32(EGPHY_EPHY_RESET_CNTRL, data);
    udelay(50);

    /* EGPHY_EPHY_PWR_MGNT */
    data = 0X21fffffd;
    WRITE_32(EGPHY_EPHY_PWR_MGNT, data);
    udelay(50);

    if (phy_map & 0x04) /* phy 3 */
        data &= ~(0x1f << 20); /* EXT_PWR_DOWN_PHY_0 */
    if (phy_map & 0x08) /* phy 4 */
        data &= ~(0x1f << 15); /* EXT_PWR_DOWN_PHY_1 */
    if (phy_map)
        data &= ~0x2000001c; /* IDDQ_GLOBAL_PWR, EXT_PWR_DOWN_BIAS, EXT_PWR_DOWN_DLL, EXT_PWR_DOWN_PHY */
    WRITE_32(EGPHY_EPHY_PWR_MGNT, data);
    udelay(50);

    /* RESET_N --> 1 */
    READ_32(EGPHY_EPHY_RESET_CNTRL, data);
    data |= 0x10;
    WRITE_32(EGPHY_EPHY_RESET_CNTRL, data);
    udelay(50);

    /* RESET_N --> 0 */
    data = 0x00;
    WRITE_32(EGPHY_EPHY_RESET_CNTRL, data);
    udelay(50);
}

static void egphy_init(uint32_t phy_map)
{
    egphy_gphy_out_reg_t gphy_out;

    READ_32(EGPHY_UBUS_MISC_EGPHY_GPHY_OUT, gphy_out);

    gphy_out.IDDQ_BIAS = 0;
    gphy_out.PWRDWN = 0;
    gphy_out.DLL_EN = 0;
    gphy_out.iddq_global_pwr = 0;
    gphy_out.GPHY_CK25_DIS = 0;
    gphy_out.RST = 0;
    gphy_out.PHYA = 0;

    WRITE_32(EGPHY_UBUS_MISC_EGPHY_GPHY_OUT, gphy_out);
    udelay(50);

    gphy_out.RST = 1;
    gphy_out.PHYA = 1;
    gphy_out.PWRDWN = ~phy_map;
    gphy_out.iddq_global_pwr = phy_map ? 0 : 1;
    gphy_out.IDDQ_BIAS = phy_map ? 0 : 1;

    WRITE_32(EGPHY_UBUS_MISC_EGPHY_GPHY_OUT, gphy_out);
    udelay(50);
}

static int sgmii_power_on(void)
{
    int ret;

    ret = PowerOnDevice(PMB_ADDR_SGMII);
    if (ret != 0)
        printk("Failed to PowerOnDevice PMB_ADDR_SGMII block\n");
    else
        udelay(100);

    return ret;
}

static void sgmii_mux_config(void)
{
    uint32_t data = 0x01;

    /* Mux EMAC0 to SGMII */
    WRITE_32(EGPHY_RGMII_0_MUX_CONFIG, data);
    udelay(50);
}

static void sgmii_serdes_cntrl_init(void)
{
    egphy_single_serdes_ctrl_t serdes_ctrl;

    READ_32(EGPHY_SINGLE_SERDES_CNTRL, serdes_ctrl);

    serdes_ctrl.select_sgmii = 1;
    WRITE_32(EGPHY_SINGLE_SERDES_CNTRL, serdes_ctrl);
    udelay(50);

    serdes_ctrl.iddq = 0;
    serdes_ctrl.pwrdwn = 0;
    WRITE_32(EGPHY_SINGLE_SERDES_CNTRL, serdes_ctrl);
    udelay(1000);

    serdes_ctrl.serdes_reset = 0;
    WRITE_32(EGPHY_SINGLE_SERDES_CNTRL, serdes_ctrl);
    udelay(50);

    serdes_ctrl.reset_mdioregs = 0;
    WRITE_32(EGPHY_SINGLE_SERDES_CNTRL, serdes_ctrl);
    udelay(50);

    serdes_ctrl.reset_pll = 0;
    WRITE_32(EGPHY_SINGLE_SERDES_CNTRL, serdes_ctrl);
    udelay(50);

    serdes_ctrl.serdes_prtad = 0x6;
    WRITE_32(EGPHY_SINGLE_SERDES_CNTRL, serdes_ctrl);
    udelay(50);
}

typedef struct {
    char *desc;
    uint16_t addr;
    uint16_t data;
} mdio_entry_t;

static mdio_entry_t toggle_pll75[] = {
    { "XGXSBlk2 Block Address",          0x1f, 0x8100 },
    { "Override pin, use mdio",          0x16, 0x0020 },
    { "TxPLL Block Address",             0x1f, 0x8050 },
    { "Toggle mmd_resetb to 0",          0x14, 0x8021 },
    { "Toggle mmd_resetb to 1",          0x14, 0x8821 },
    { NULL,                              0x00, 0x0000 }
};

#ifndef _CFE_
static mdio_entry_t refclk80m_vco6p25g[] = {
    { "Digital Block Address",           0x1f, 0x8300 },
    { "refclk_sel=3'h7 (80 MHz)",        0x18, 0xe000 },
    { "PLL AFE Block Address",           0x1f, 0x8050 },
    { "pll_afectrl0",                    0x10, 0x5740 },
    { "pll_afectrl1",                    0x11, 0x01d0 },
    { "pll_afectrl2",                    0x12, 0x59f0 },
    { "pll_afectrl3",                    0x13, 0xaa80 },
    { "pll_afectrl4",                    0x14, 0x8821 },
    { "pll_afectrl5",                    0x15, 0x0044 },
    { "pll_afectrl6",                    0x16, 0x8800 },
    { "pll_afectrl7",                    0x17, 0x0813 },
    { "pll_afectrl8",                    0x18, 0x0000 },
    { NULL,                              0x00, 0x0000 }
};

static mdio_entry_t refclk125m_vco6p25g[] = {
    { "Digital Block Address",           0x1f, 0x8300 },
    { "refclk_sel=3'h2 (125 MHz)",       0x18, 0x4000 },
    { "PLL AFE Block Address",           0x1f, 0x8050 },
    { "pll_afectrl0",                    0x10, 0x5740 },
    { "pll_afectrl1",                    0x11, 0x01d0 },
    { "pll_afectrl2",                    0x12, 0x19f0 },
    { "pll_afectrl3",                    0x13, 0x2b00 },
    { "pll_afectrl4",                    0x14, 0x0023 },
    { "pll_afectrl5",                    0x15, 0x0044 },
    { "pll_afectrl6",                    0x16, 0x0000 },
    { "pll_afectrl7",                    0x17, 0x0000 },
    { "pll_afectrl8",                    0x18, 0x0000 },
    { NULL,                              0x00, 0x0000 }
};
#endif

static mdio_entry_t forced_speed_1g_sgmii_os5[] = {
    { "BLK0 Block Address",             0x1f, 0x8000 },
    { "disable pll start sequencer",    0x10, 0x0c2f },
    { "Digital Block Address",          0x1f, 0x8300 },
    { "enable sgmii mode",              0x10, 0x0100 },
    { "RX2 Block Address",              0x1f, 0x8470 },
    { "Set step_one[1:0]",              0x13, 0x1251 },
    { "Digital5 Block Address",         0x1f, 0x8340 },
    { "set oversampling mode",          0x1a, 0x0003 },
    { "BLK0 Block Address",             0x1f, 0x8000 },
    { "disable AN, set 1G mode",        0x00, 0x0140 },
    { "enable pll start sequencer",     0x10, 0x2c2f },
    { NULL,                             0x00, 0x0000 }
}; 

static void mdio_reg_prog(uint32_t phy_addr, mdio_entry_t *mdio_entry)
{
    while (mdio_entry->desc)
    {
        printk("Configuring MDIO interface for phy_addr %d: ", phy_addr);
        printk("%s ... Writing 0x%04x to address 0x%02x\n", mdio_entry->desc, mdio_entry->data, mdio_entry->addr);

        if (mdio_write_c22_register(MDIO_INT, phy_addr,  mdio_entry->addr, mdio_entry->data) != MDIO_OK)
            break;

        mdio_entry++;
    }
}

static void wait_for_serdes_ready(void)
{
    uint32_t retry = 2000;
    egphy_single_serdes_stat_t serdes_stat;

    /* Wait till pll_lock and link_status become '1' */
    do {
        READ_32(EGPHY_SINGLE_SERDES_STAT, serdes_stat);
        if (serdes_stat.pll_lock && serdes_stat.link_status)
            break;
        udelay(10);
    } while (retry--);

    if (!retry)
    {
        printk("SGMII Error: wait_for_serdes_ready() reached maximum retries. pll_lock=%d link_status=%d\n",
            serdes_stat.pll_lock, serdes_stat.link_status);
    }
}

static void sgmii_refclk_init(int sgmii_phy_addr)
{
#ifndef _CFE_
    serdes_wan_type_t type = wan_serdes_type_get();

    if (type == SERDES_WAN_TYPE_NONE)
        return;

    if (type == SERDES_WAN_TYPE_GPON)
        mdio_reg_prog(sgmii_phy_addr, refclk80m_vco6p25g);
    else
        mdio_reg_prog(sgmii_phy_addr, refclk125m_vco6p25g);
#endif
}

static void sgmii_init(int sgmii_phy_addr)
{
    if (sgmii_power_on() != 0)
        return;

    sgmii_mux_config();
    sgmii_serdes_cntrl_init();
    mdio_reg_prog(sgmii_phy_addr, toggle_pll75);
    mdio_reg_prog(sgmii_phy_addr, forced_speed_1g_sgmii_os5);
    wait_for_serdes_ready();
    if (0) /* XXX SGMII does not work with SyncE yet */
        sgmii_refclk_init(sgmii_phy_addr);
}

void egphy_reset(uint32_t phy_map)
{
    egphy_init(phy_map & 0x03); /* phys 1,2 */
    ephy_init(phy_map & 0x0c); /* phys 3,4 */

    if (phy_map & 0x20) /* phy 6 */
        sgmii_init(0x6);
}

static void ephy_extra_config(uint32_t phy_addr)
{
}

#define CORE_SHD1C_0D                   0x1d
#define CORE_SHD18_111                  0x2f

#define CORE_SHD_BICOLOR_LED0           0x000a
#define FORCE_AUTO_MDIX                 0x0200

static void egphy_extra_config(uint32_t phy_addr)
{
    egphy_write_register(phy_addr, RDB_REG_ACCESS | CORE_SHD1C_0D, CORE_SHD_BICOLOR_LED0);
    egphy_write_register(phy_addr, RDB_REG_ACCESS | CORE_SHD18_111, FORCE_AUTO_MDIX);
}

void egphy_auto_enable_extra_config(uint32_t phy_addr)
{
    switch (phy_addr)
    {
    case 1:
    case 2:
        egphy_extra_config(phy_addr);
        break;
    case 3:
    case 4:
        ephy_extra_config(phy_addr);
        break;
    default:
        break;
    }
}
