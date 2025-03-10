/*
   Copyright 2007-2010 Broadcom Corp. All Rights Reserved.

   <:label-BRCM:2011:DUAL/GPL:standard
   
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
#define _BCMENET_LOCAL_
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/mii.h>
#include <linux/stddef.h>
#include <linux/ctype.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <board.h>
#include "boardparms.h"
#include <bcm_map_part.h>
#include "bcm_intr.h"
#include "bcmenet.h"
#include "bcmmii.h"
#include "ethswdefs.h"
#include "ethsw.h"
#include "ethsw_phy.h"
#include "eth_pwrmngt.h"
#include "bcmsw.h"
#include "bcmSpiRes.h"
#include "bcmswaccess.h"
#include "bcmswshared.h"
#if !defined(CONFIG_BCM963138) && !defined(CONFIG_BCM963148)
#include "bcmPktDma.h"
#endif
#if defined(_CONFIG_BCM_FAP)
#include "fap_packet.h"
#endif
#if defined(CONFIG_BCM_GMAC)
#include "bcmgmac.h"
#endif
#if defined(CONFIG_BCM963381)
#include "pmc_switch.h"
#endif


static void str_to_num(char *in, char *out, int len);
static int proc_get_sw_param(char *page, char **start, off_t off, int cnt, int *eof, void *data);
static int proc_set_sw_param(struct file *f, const char *buf, unsigned long cnt, void *data);

static int proc_get_mii_param(char *page, char **start, off_t off, int cnt, int *eof, void *data);
static int proc_set_mii_param(struct file *f, const char *buf, unsigned long cnt, void *data);


#if defined(ENET_GPON_CONFIG)
extern struct net_device *gponifid_to_dev[MAX_GEM_IDS];
#endif
extern struct semaphore bcm_ethlock_switch_config;
extern uint8_t port_in_loopback_mode[TOTAL_SWITCH_PORTS];
extern int vport_cnt;  /* number of vports: bitcount of Enetinfo.sw.port_map */

extern BcmEnet_devctrl *pVnetDev0_g;

static uint16_t dis_learning = 0x0180;
static uint8_t  port_fwd_ctrl = 0xC1;
static uint16_t pbvlan_map[TOTAL_SWITCH_PORTS];

PHY_STAT ethsw_phyid_stat(int phyId)
{
    PHY_STAT ps;
    uint16 v16;
    uint16 ctrl;
    uint16 mii_esr = 0;
    uint16 mii_stat = 0, mii_adv = 0, mii_lpa = 0;
    uint16 mii_gb_ctrl = 0, mii_gb_stat = 0;

    ps.lnk = 0;
    ps.fdx = 0;
    ps.spd1000 = 0;
    ps.spd100 = 0;
    if ( !IsPhyConnected(phyId) )
    {
        // 0xff PHY ID means no PHY on this port.
        ps.lnk = 1;
        ps.fdx = 1;
#if defined(CONFIG_BCM96318)
        if (IsMII(phyId))
        {
            ps.spd100 = 1;
        }
        else
#endif
        {
            ps.spd1000 = 1;
        }
        return ps;
    }

    down(&bcm_ethlock_switch_config);

    ethsw_phy_rreg(phyId, MII_INTERRUPT, &v16);
    ethsw_phy_rreg(phyId, MII_ASR, &v16);
    BCM_ENET_DEBUG("%s mii_asr (reg 25) 0x%x\n", __FUNCTION__, v16);


    if (!MII_ASR_LINK(v16)) {
        up(&bcm_ethlock_switch_config);
        return ps;
    }

    ps.lnk = 1;

    ethsw_phy_rreg(phyId, MII_BMCR, &ctrl);

    if (!MII_ASR_DONE(v16)) {
        ethsw_phy_rreg(phyId, MII_BMCR, &ctrl);
        if (ctrl & BMCR_ANENABLE) {
            up(&bcm_ethlock_switch_config);
            return ps;
        }
        // auto-negotiation disabled
        ps.fdx = (ctrl & BMCR_FULLDPLX) ? 1 : 0;
        if((ctrl & BMCR_SPEED100) && !(ctrl & BMCR_SPEED1000))
            ps.spd100 = 1;
        else if(!(ctrl & BMCR_SPEED100) && (ctrl & BMCR_SPEED1000))
            ps.spd1000 = 1;

        up(&bcm_ethlock_switch_config);
        return ps;
    }


    //Auto neg enabled (this end) cases
    ethsw_phy_rreg(phyId, MII_ADVERTISE, &mii_adv);
    ethsw_phy_rreg(phyId, MII_LPA, &mii_lpa);
    ethsw_phy_rreg(phyId, MII_BMSR, &mii_stat);

    BCM_ENET_DEBUG("%s mii_adv 0x%x mii_lpa 0x%x mii_stat 0x%x mii_ctrl 0x%x \n", __FUNCTION__,
            mii_adv, mii_lpa, mii_stat, v16);
    // read 1000mb Phy  registers if supported
    if (mii_stat & BMSR_ESTATEN) { 

        ethsw_phy_rreg(phyId, MII_ESTATUS, &mii_esr);
        if (mii_esr & (1 << 15 | 1 << 14 |
                    ESTATUS_1000_TFULL | ESTATUS_1000_THALF))
            ethsw_phy_rreg(phyId, MII_CTRL1000, &mii_gb_ctrl);
        ethsw_phy_rreg(phyId, MII_STAT1000, &mii_gb_stat);
    }

    mii_adv &= mii_lpa;

    if ((mii_gb_ctrl & ADVERTISE_1000FULL) &&  // 1000mb Adv
            (mii_gb_stat & LPA_1000FULL))
    {
        ps.spd1000 = 1;
        ps.fdx = 1;
    } else if ((mii_gb_ctrl & ADVERTISE_1000HALF) && 
            (mii_gb_stat & LPA_1000HALF))
    {
        ps.spd1000 = 1;
        ps.fdx = 0;
    } else if (mii_adv & ADVERTISE_100FULL) {  // 100mb adv
        ps.spd100 = 1;
        ps.fdx = 1;
    } else if (mii_adv & ADVERTISE_100BASE4) {
        ps.spd100 = 1;
        ps.fdx = 0;
    } else if (mii_adv & ADVERTISE_100HALF) {
        ps.spd100 = 1;
        ps.fdx = 0;
    } else if (mii_adv & ADVERTISE_10FULL) {
        ps.fdx = 1;
    }

    up(&bcm_ethlock_switch_config);

    return ps;
}

#if defined(CONFIG_BCM963268)
void ethsw_phy_advertise_all(uint32 phy_id)
{
    uint16 v16;
    /* Advertise all speed & duplex combinations */
    /* Advertise 100BaseTX FD/HD and 10BaseT FD/HD */
    ethsw_phy_rreg(phy_id, MII_ADVERTISE, &v16);
    v16 |= AN_ADV_ALL;
    ethsw_phy_wreg(phy_id, MII_ADVERTISE, &v16);
    /* Advertise 1000BaseT FD/HD */
    ethsw_phy_rreg(phy_id, MII_CTRL1000, &v16);
    v16 |= AN_1000BASET_CTRL_ADV_ALL;
    ethsw_phy_wreg(phy_id, MII_CTRL1000, &v16);
}
#endif

/* apply phy init board parameters for internal switch*/
void ethsw_phy_apply_init_bp(void)
{
    BcmEnet_devctrl *pVnetDev0 = (BcmEnet_devctrl *) netdev_priv(vnet_dev[0]);
    unsigned int portmap, i, phy_id;
    bp_mdio_init_t* phyinit;
    uint16 data;

    portmap = pVnetDev0->EnetInfo[0].sw.port_map;
    for (i = 0; i < (TOTAL_SWITCH_PORTS - 1); i++) {
        if ((portmap & (1U<<i)) != 0) {
            phy_id = pVnetDev0->EnetInfo[0].sw.phy_id[i];
            phyinit = pVnetDev0->EnetInfo[0].sw.phyinit[i];
            if( phyinit == 0 )
                continue;

            while(phyinit->u.op.op != BP_MDIO_INIT_OP_NULL)
            {
                if(phyinit->u.op.op == BP_MDIO_INIT_OP_WRITE)
                    ethsw_phy_wreg(phy_id, phyinit->u.write.reg, (uint16*)(&phyinit->u.write.data));
                else if(phyinit->u.op.op == BP_MDIO_INIT_OP_UPDATE)
                {
                    ethsw_phy_rreg(phy_id, phyinit->u.update.reg, &data);
                    data &= ~phyinit->u.update.mask;
                    data |= phyinit->u.update.data;
                    ethsw_phy_wreg(phy_id, phyinit->u.update.reg, &data);
                }
                phyinit++;
            }
        }
    }

}
/* Code to handle exceptions and chip specific cases */
void ethsw_phy_handle_exception_cases (void)
{
    /* In some chips, the GPhys do not advertise all capabilities. So, fix it first */ 
#if defined(CONFIG_BCM963268)
    ethsw_phy_advertise_all(GPHY_PORT_PHY_ID);
#endif

}

int ethsw_setup_phys(void)
{
    ethsw_shutdown_unused_phys();
    return 0;
}



void bcmeapi_ethsw_init_config(void)
{
    int i;

    /* Save the state that is restored in enable_hw_switching */
    for(i = 0; i < TOTAL_SWITCH_PORTS; i++)  {
        ethsw_rreg(PAGE_PORT_BASED_VLAN, REG_VLAN_CTRL_P0 + (i * 2),
                (uint8 *)&pbvlan_map[i], 2);
    }
    ethsw_rreg(PAGE_CONTROL, REG_DISABLE_LEARNING, (uint8 *)&dis_learning, 2);
    ethsw_rreg(PAGE_CONTROL, REG_PORT_FORWARD, (uint8 *)&port_fwd_ctrl, 1);

#if defined(CONFIG_BCM963268) ||  defined(CONFIG_BCM963381)
    {
        /* Disable tags for internal switch ports */
        uint32 tmp;
        ethsw_rreg(PAGE_CONTROL, REG_IUDMA_CTRL, (uint8_t *)&tmp, 4);
        tmp |= REG_IUDMA_CTRL_TX_MII_TAG_DISABLE;
        ethsw_wreg(PAGE_CONTROL, REG_IUDMA_CTRL, (uint8_t *)&tmp, 4); 
    }
#endif

}

int ethsw_setup_led(void)
{
    BcmEnet_devctrl *pVnetDev0 = (BcmEnet_devctrl *) netdev_priv(vnet_dev[0]);
    unsigned int phy_id, i;
    uint16 v16;

#if defined(SUPPORT_GPL_1)
    uint32 v32;

    extsw_rreg(PAGE_MANAGEMENT, REG_DEV_ID, (uint8 *)&v32, sizeof(v32));
    v32 = swab32(v32);
    if (v32 == 0x53115)
    {
        /* Enable traffic blinking for all ethernet port LEDs */
        v16 = REG_LED_FUNCTION1_LNK_ACT | REG_LED_FUNCTION1_10M_ACT | REG_LED_FUNCTION1_100M_ACT | REG_LED_FUNCTION1_1000M_ACT;
        v16 = swab16(v16);
        extsw_wreg(PAGE_CONTROL, REG_LED_FUNCTION1_CTRL, (uint8 *)&v16, sizeof(v16));
    }
#endif

    /* For each port that has an internal or external PHY, configure it
       as per the required initial configuration */
    for (i = 0; i < (TOTAL_SWITCH_PORTS - 1); i++) {
        /* Check if the port is in the portmap or not */
        if ((pVnetDev0->EnetInfo[0].sw.port_map & (1U<<i)) != 0) {
            /* Check if the port is connected to a PHY or not */
            phy_id = pVnetDev0->EnetInfo[0].sw.phy_id[i];
            /* If a Phy is connected, set it up with initial config */
            /* TBD: Maintain the config for each Phy */
            if(IsPhyConnected(phy_id) && !IsExtPhyId(phy_id)) {
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) ||  defined(CONFIG_BCM963268) ||    defined(CONFIG_BCM96318) ||     defined(CONFIG_BCM963381)
                v16 = 0xa410;
                // Enable Shadow register 2
                ethsw_phy_rreg(phy_id, MII_BRCM_TEST, &v16);
                v16 |= MII_BRCM_TEST_SHADOW2_ENABLE;
                ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &v16);

#if defined(CONFIG_BCM963268)
#if defined(CONFIG_BCM963268)
                if (i != GPHY_PORT_ID) 
#else
                    if ((i != GPHY1_PORT_ID) && (i != GPHY2_PORT_ID))
#endif
                    {
                        // Set LED1 to speed. Set LED0 to blinky link
                        v16 = 0x08;
                    }
#else
                // Set LED0 to speed. Set LED1 to blinky link
                v16 = 0x71;
#endif
                ethsw_phy_wreg(phy_id, 0x15, &v16);
                // Disable Shadow register 2
                ethsw_phy_rreg(phy_id, MII_BRCM_TEST, &v16);
                v16 &= ~MII_BRCM_TEST_SHADOW2_ENABLE;
                ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &v16);
#endif
            }
            if (IsExtPhyId(phy_id)) {
                /* Configure LED for link/activity */
                v16 = MII_1C_SHADOW_LED_CONTROL << MII_1C_SHADOW_REG_SEL_S;
                ethsw_phy_wreg(phy_id, MII_REGISTER_1C, &v16);
                ethsw_phy_rreg(phy_id, MII_REGISTER_1C, &v16);
                v16 |= ACT_LINK_LED_ENABLE;
                v16 |= MII_1C_WRITE_ENABLE;
                v16 &= ~(MII_1C_SHADOW_REG_SEL_M << MII_1C_SHADOW_REG_SEL_S);
                v16 |= (MII_1C_SHADOW_LED_CONTROL << MII_1C_SHADOW_REG_SEL_S);
                ethsw_phy_wreg(phy_id, MII_REGISTER_1C, &v16);

                ethsw_phy_rreg(phy_id, MII_PHYSID2, &v16);
                if ((v16 & BCM_PHYID_M) == (BCM54610_PHYID2 & BCM_PHYID_M)) {
                    /* Configure LOM LED Mode */
                    v16 = MII_1C_EXTERNAL_CONTROL_1 << MII_1C_SHADOW_REG_SEL_S;
                    ethsw_phy_wreg(phy_id, MII_REGISTER_1C, &v16);
                    ethsw_phy_rreg(phy_id, MII_REGISTER_1C, &v16);
                    v16 |= LOM_LED_MODE;
                    v16 |= MII_1C_WRITE_ENABLE;
                    v16 &= ~(MII_1C_SHADOW_REG_SEL_M << MII_1C_SHADOW_REG_SEL_S);
                    v16 |= (MII_1C_EXTERNAL_CONTROL_1 << MII_1C_SHADOW_REG_SEL_S);
                    ethsw_phy_wreg(phy_id, MII_REGISTER_1C, &v16);
                }
            }
        }
    }
    return 0;
}

int ethsw_reset_ports(struct net_device *dev)
{
    BcmEnet_devctrl *pDevCtrl = netdev_priv(dev);
    int map, cnt, i;
    uint16 v16, phy_identifier;
    int phyid;
    uint8 v8;
    unsigned long port_flags;

    map = pDevCtrl->EnetInfo[0].sw.port_map;
    bitcount(cnt, map);

    if (cnt <= 0)
        return 0;

#if defined(CONFIG_BCM963268)
    if (map & (1 << (RGMII_PORT_ID + 1))) {
        GPIO->RoboswSwitchCtrl |= (RSW_MII_2_IFC_EN | (RSW_MII_SEL_2P5V << RSW_MII_2_SEL_SHIFT));
    }
#endif

    for (i = 0; i < NUM_RGMII_PORTS; i++) {
#if defined(CONFIG_BCM96318)
        if (map & (1 << (RGMII_PORT_ID + i)))
#endif
        {
            phyid = pDevCtrl->EnetInfo[0].sw.phy_id[RGMII_PORT_ID + i];
            ethsw_phy_rreg(phyid, MII_PHYSID2, &phy_identifier);

            ethsw_rreg(PAGE_CONTROL, REG_RGMII_CTRL_P4 + i, &v8, 1);
#if defined(CONFIG_BCM963268)
            v8 |= REG_RGMII_CTRL_ENABLE_RGMII_OVERRIDE;
            v8 &= ~REG_RGMII_CTRL_MODE;
            if (IsRGMII(phyid)) {
                v8 |= REG_RGMII_CTRL_MODE_RGMII;
            } else if (IsRvMII(phyid)) {
                v8 |= REG_RGMII_CTRL_MODE_RvMII;
            } else if (IsGMII(phyid)) {
                v8 |= REG_RGMII_CTRL_MODE_GMII;
            } else {
                v8 |= REG_RGMII_CTRL_MODE_MII;
            }
#endif
            
#if defined(CONFIG_BCM963268)
            if ((pDevCtrl->chipRev == 0xA0) || (pDevCtrl->chipRev == 0xB0)) {
                /* RGMII timing workaround */
                v8 &= ~REG_RGMII_CTRL_TIMING_SEL;
            }
            else
#endif    
            {

                v8 |= REG_RGMII_CTRL_TIMING_SEL;
            }
            /* Enable Clock delay in RX */
            port_flags = enet_get_port_flags(0, RGMII_PORT_ID + i);
            if (IsPortRxInternalDelay(port_flags)) {
                v8 |= REG_RGMII_CTRL_DLL_RXC_BYPASS;
            }
            else if ((phy_identifier & BCM_PHYID_M) == (BCM54616_PHYID2 & BCM_PHYID_M)) {
                v8 |= REG_RGMII_CTRL_DLL_RXC_BYPASS;
            }

            ethsw_wreg(PAGE_CONTROL, REG_RGMII_CTRL_P4 + i, &v8, 1);

#if defined(CONFIG_BCM963268)
            if ((pDevCtrl->chipRev == 0xA0) || (pDevCtrl->chipRev == 0xB0)) {
                /* RGMII timing workaround */
                v8 = 0xAB;
                ethsw_wreg(PAGE_CONTROL, REG_RGMII_TIMING_P4 + i, &v8, 1);
            }
#endif

            /* No need to check the PhyID if the board params is set correctly for RGMII. However, keeping
             *   the phy id check to make it work even when customer does not set the RGMII flag in the phy_id
             *   in board params
             */
            if ((IsRGMII(phyid) && IsPhyConnected(phyid)) ||
                    ((phy_identifier & BCM_PHYID_M) == (BCM54610_PHYID2 & BCM_PHYID_M)) ||
                    ((phy_identifier & BCM_PHYID_M) == (BCM50612_PHYID2 & BCM_PHYID_M))) {

                v16 = MII_1C_SHADOW_CLK_ALIGN_CTRL << MII_1C_SHADOW_REG_SEL_S;
                ethsw_phy_wreg(phyid, MII_REGISTER_1C, &v16);
                ethsw_phy_rreg(phyid, MII_REGISTER_1C, &v16);
#if defined(CONFIG_BCM963268)
                /* Temporary work-around for MII2 port RGMII delay programming */
                if (i == 1 && ((pDevCtrl->chipRev == 0xA0) || (pDevCtrl->chipRev == 0xB0)) )
                    v16 |= GTXCLK_DELAY_BYPASS_DISABLE;
                else
#endif
                    v16 &= (~GTXCLK_DELAY_BYPASS_DISABLE);
                v16 |= MII_1C_WRITE_ENABLE;
                v16 &= ~(MII_1C_SHADOW_REG_SEL_M << MII_1C_SHADOW_REG_SEL_S);
                v16 |= (MII_1C_SHADOW_CLK_ALIGN_CTRL << MII_1C_SHADOW_REG_SEL_S);
                ethsw_phy_wreg(phyid, MII_REGISTER_1C, &v16);
                if ((phy_identifier & BCM_PHYID_M) == (BCM54616_PHYID2 & BCM_PHYID_M)) {
                    v16 = MII_REG_18_SEL(0x7);
                    ethsw_phy_wreg(phyid, MII_REGISTER_18, &v16);
                    ethsw_phy_rreg(phyid, MII_REGISTER_18, &v16);
                    /* Disable Skew */
                    v16 &= (~RGMII_RXD_TO_RXC_SKEW);
                    v16 = MII_REG_18_WR(0x7,v16);
                    ethsw_phy_wreg(phyid, MII_REGISTER_18, &v16);
                }
            }
        }
    }

    /*Remaining port reset functionality is moved into ethsw_init_hw*/

    return 0;
}

int bcmeapi_ethsw_init(void)
{
    robosw_init();

    return 0;
}

void bcmeapi_ethsw_init_ports()
{
    robosw_configure_ports();
}

static uint8 swdata[16];
static uint8 miidata[16];
#if defined(AEI_VDSL_EXTSW_DEBUG)
static uint8 is_extsw = 0;
#endif

int ethsw_add_proc_files(struct net_device *dev)
{
    struct proc_dir_entry *p;

    p = create_proc_entry("switch", 0644, NULL);

    if (p == NULL)
        return -1;

    memset(swdata, 0, sizeof(swdata));

    p->data        = dev;
    p->read_proc   = proc_get_sw_param;
    p->write_proc  = proc_set_sw_param;

    p = create_proc_entry("mii", 0644, NULL);

    if (p == NULL)
        return -1;

    memset(miidata, 0, sizeof(miidata));

    p->data       = dev;
    p->read_proc  = proc_get_mii_param;
    p->write_proc = proc_set_mii_param;

    return 0;
}

int ethsw_del_proc_files(void)
{
    remove_proc_entry("switch", NULL);

    remove_proc_entry("mii", NULL);
    return 0;
}

static void str_to_num(char* in, char* out, int len)
{
    int i;
    memset(out, 0, len);

    for (i = 0; i < len * 2; i ++)
    {
        if ((*in >= '0') && (*in <= '9'))
            *out += (*in - '0');
        else if ((*in >= 'a') && (*in <= 'f'))
            *out += (*in - 'a') + 10;
        else if ((*in >= 'A') && (*in <= 'F'))
            *out += (*in - 'A') + 10;
        else
            *out += 0;

        if ((i % 2) == 0)
            *out *= 16;
        else
            out ++;

        in ++;
    }
    return;
}

static int proc_get_sw_param(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
    int reg_page  = swdata[0];
    int reg_addr  = swdata[1];
    int reg_len   = swdata[2];
    int i = 0;
    int r = 0;

    *eof = 1;

    if (reg_len == 0)
        return 0;

#if defined(AEI_VDSL_EXTSW_DEBUG)
    if (is_extsw)
    {
        extsw_rreg(reg_page, reg_addr, swdata + 3, reg_len);

        if (reg_len == 2)
        {
            ((uint16 *)(swdata + 3))[0] = swab16(((uint16 *)(swdata + 3))[0]);
        }
        else if (reg_len == 4)
        {
            ((uint32 *)(swdata + 3))[0] = swab32(((uint32 *)(swdata + 3))[0]);
        }
        else if (reg_len == 6)
        {
            if ((reg_addr %4) == 0)
            {
                ((uint32 *)(swdata + 3))[0] = swab32(((uint32 *)(swdata + 3))[0]);
                ((uint16 *)(swdata + 7))[0] = swab16(((uint16 *)(swdata + 7))[0]);
            }
            else
            {
                ((uint16 *)(swdata + 3))[0] = swab32(((uint16 *)(swdata + 3))[0]);
                ((uint32 *)(swdata + 5))[0] = swab16(((uint32 *)(swdata + 5))[0]);
            }
        }
        else if (reg_len == 8)
        {
           ((uint32 *)(swdata + 3))[0] = swab32(((uint32 *)(swdata + 3))[0]);
           ((uint32 *)(swdata + 3))[1] = swab32(((uint32 *)(swdata + 3))[1]);
        }
    }
    else
    {
        down(&bcm_ethlock_switch_config);
        ethsw_rreg(reg_page, reg_addr, swdata + 3, reg_len);
        up(&bcm_ethlock_switch_config);
    }

    r += sprintf(page + r, "%s switch:", (is_extsw) ? "External" : "Internal");
#else
    down(&bcm_ethlock_switch_config);
    ethsw_rreg(reg_page, reg_addr, swdata + 3, reg_len);
    up(&bcm_ethlock_switch_config);
#endif

    r += sprintf(page + r, "[%02x:%02x] = ", swdata[0], swdata[1]);

    for (i = 0; i < reg_len; i ++)
        r += sprintf(page + r, "%02x ", swdata[3 + i]);

    r += sprintf(page + r, "\n");
    return (r < cnt)? r: 0;
}

static int proc_set_sw_param(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    char input[32];
    int i;
    int r;
    int num_of_octets;

    int reg_page;
    int reg_addr;
    int reg_len;

    if (cnt > 32)
        cnt = 32;

    if (copy_from_user(input, buf, cnt) != 0)
        return -EFAULT;

    r = cnt;

    for (i = 0; i < r; i ++)
    {
        if (!isxdigit(input[i]))
        {
            memmove(&input[i], &input[i + 1], r - i - 1);
            r --;
            i --;
        }
    }

    num_of_octets = r / 2;

    if (num_of_octets < 3) // page, addr, len
        return -EFAULT;

    str_to_num(input, swdata, num_of_octets);

    reg_page  = swdata[0];
    reg_addr  = swdata[1];
    reg_len   = swdata[2];

#if defined(AEI_VDSL_EXTSW_DEBUG)
    is_extsw = 0;
#endif

    if (((reg_len != 1) && (reg_len % 2) != 0) || reg_len > 8)
    {
        memset(swdata, 0, sizeof(swdata));
        return -EFAULT;
    }

#if defined(AEI_VDSL_EXTSW_DEBUG)
    if (num_of_octets > 3)
    {
        if (num_of_octets == reg_len + 4)
        {
            is_extsw = ((swdata[reg_len + 3]) ? 1 : 0);
            swdata[reg_len + 3] = 0;
        }
        else if (num_of_octets != reg_len + 3)
        {
            memset(swdata, 0, sizeof(swdata));
            return -EFAULT;
        }

        if (is_extsw)
        {
            if (reg_len == 2)
            {
                ((uint16 *)(swdata + 3))[0] = swab16(((uint16 *)(swdata + 3))[0]);
            }
            else if (reg_len == 4)
            {
                ((uint32 *)(swdata + 3))[0] = swab32(((uint32 *)(swdata + 3))[0]);
            }
            else if (reg_len == 6)
            {
                if ((reg_addr %4) == 0)
                {
                    ((uint32 *)(swdata + 3))[0] = swab32(((uint32 *)(swdata + 3))[0]);
                    ((uint16 *)(swdata + 7))[0] = swab16(((uint16 *)(swdata + 7))[0]);
                }
                else
                {
                    ((uint16 *)(swdata + 3))[0] = swab32(((uint16 *)(swdata + 3))[0]);
                    ((uint32 *)(swdata + 5))[0] = swab16(((uint32 *)(swdata + 5))[0]);
                }
            }
            else if (reg_len == 8)
            {
               ((uint32 *)(swdata + 3))[0] = swab32(((uint32 *)(swdata + 3))[0]);
               ((uint32 *)(swdata + 3))[1] = swab32(((uint32 *)(swdata + 3))[1]);
            }

            extsw_wreg(reg_page, reg_addr, swdata + 3, reg_len);
        }
        else
        {
            down(&bcm_ethlock_switch_config);
            ethsw_wreg(reg_page, reg_addr, swdata + 3, reg_len);
            up(&bcm_ethlock_switch_config);
        }
    }
#else
    if ((num_of_octets > 3) && (num_of_octets != reg_len + 3))
    {
        memset(swdata, 0, sizeof(swdata));
        return -EFAULT;
    }

    if (num_of_octets > 3) {
        down(&bcm_ethlock_switch_config);
        ethsw_wreg(reg_page, reg_addr, swdata + 3, reg_len);
        up(&bcm_ethlock_switch_config);
    }
#endif
    return cnt;
}

static int proc_get_mii_param(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
    int mii_port  = miidata[0];
    int mii_addr  = miidata[1];
    int r = 0;

    *eof = 1;

    down(&bcm_ethlock_switch_config);
    ethsw_phy_rreg(mii_port, mii_addr, (uint16 *)(miidata + 2));
    up(&bcm_ethlock_switch_config);

    r += sprintf(
            page + r,
            "[%02x:%02x] = %02x %02x\n",
            miidata[0], miidata[1], miidata[2], miidata[3]
            );

    return (r < cnt)? r: 0;
}

static int proc_set_mii_param(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    char input[32];
    int i;
    int r;
    int num_of_octets;

    int mii_port;
    int mii_addr;

    if (cnt > 32)
        cnt = 32;

    if (copy_from_user(input, buf, cnt) != 0)
        return -EFAULT;

    r = cnt;

    for (i = 0; i < r; i ++)
    {
        if (!isxdigit(input[i]))
        {
            memmove(&input[i], &input[i + 1], r - i - 1);
            r --;
            i --;
        }
    }

    num_of_octets = r / 2;

    if ((num_of_octets!= 2) && (num_of_octets != 4))
    {
        memset(miidata, 0, sizeof(miidata));
        return -EFAULT;
    }

    str_to_num(input, miidata, num_of_octets);
    mii_port  = miidata[0];
    mii_addr  = miidata[1];

    down(&bcm_ethlock_switch_config);

    if (num_of_octets > 2)
        ethsw_phy_wreg(mii_port, mii_addr, (uint16 *)(miidata + 2));

    up(&bcm_ethlock_switch_config);
    return cnt;
}


int ethsw_enable_hw_switching(void)
{
    u8 i;

    /* restore pbvlan config */
    for(i = 0; i < TOTAL_SWITCH_PORTS; i++)
    {
        ethsw_wreg(PAGE_PORT_BASED_VLAN, REG_VLAN_CTRL_P0 + (i * 2),
                (uint8 *)&pbvlan_map[i], 2);
    }

    /* restore disable learning register */
    ethsw_wreg(PAGE_CONTROL, REG_DISABLE_LEARNING, (uint8 *)&dis_learning, 2);

    /* restore port forward control register */
    ethsw_wreg(PAGE_CONTROL, REG_PORT_FORWARD, (uint8 *)&port_fwd_ctrl, 1);

    i = 0;
    while (vnet_dev[i])
    {
        if (LOGICAL_PORT_TO_UNIT_NUMBER(VPORT_TO_LOGICAL_PORT(i)) != 0) /* Not Internal switch port */
        {
            i++;  /* Go to next port */
            continue;
        }
        /* When hardware switching is enabled, enable the Linux bridge to
           not to forward the bcast packets on hardware ports */
        vnet_dev[i++]->priv_flags |= IFF_HW_SWITCH;
    }
#if defined(ENET_GPON_CONFIG)
    for (i = 0; i < MAX_GEM_IDS; i++)
    {
        if (gponifid_to_dev[i])
        {
            /* When hardware switching is enabled, enable the Linux bridge to
               not to forward the bcast packets on hardware ports */
            gponifid_to_dev[i]->priv_flags |= IFF_HW_SWITCH;
        }
    }
#endif

    return 0;
}

int ethsw_disable_hw_switching(void)
{
    u8 i, byte_value;
    u16 reg_value;


    /* set the port-based vlan control reg of each port with fwding mask of
       only that port and MIPS. For MIPS port, set the forwarding mask of
       all the ports */
    for(i = 0; i < TOTAL_SWITCH_PORTS; i++)
    {
        ethsw_rreg(PAGE_PORT_BASED_VLAN, REG_VLAN_CTRL_P0 + (i * 2),
                (uint8 *)&pbvlan_map[i], 2);
        if (i == MIPS_PORT_ID)
        {
            reg_value = PBMAP_ALL;
        }
        else
        {
            reg_value = PBMAP_MIPS;
        }
        ethsw_wreg(PAGE_PORT_BASED_VLAN, REG_VLAN_CTRL_P0 + (i * 2),
                (uint8 *)&reg_value, 2);
    }

    /* Save disable_learning_reg setting */
    ethsw_rreg(PAGE_CONTROL, REG_DISABLE_LEARNING, (uint8 *)&dis_learning, 2);
    /* disable learning on all ports */
    reg_value = PBMAP_ALL;
    ethsw_wreg(PAGE_CONTROL, REG_DISABLE_LEARNING, (uint8 *)&reg_value, 2);

    /* Save port forward control setting */
    ethsw_rreg(PAGE_CONTROL, REG_PORT_FORWARD, (uint8 *)&port_fwd_ctrl, 1);
    /* flood unlearned packets */
    byte_value = 0x00;
    ethsw_wreg(PAGE_CONTROL, REG_PORT_FORWARD, (uint8 *)&byte_value, 1);

    i = 0;
    while (vnet_dev[i])
    {
        if (LOGICAL_PORT_TO_UNIT_NUMBER(VPORT_TO_LOGICAL_PORT(i)) != 0) /* Not Internal switch port */
        {
            i++;  /* Go to next port */
            continue;
        }
        /* When hardware switching is disabled, enable the Linux bridge to
           forward the bcast on hardware ports as well */
        vnet_dev[i++]->priv_flags &= ~IFF_HW_SWITCH;
    }

#if defined(ENET_GPON_CONFIG)
    for (i = 0; i < MAX_GEM_IDS; i++)
    {
        if (gponifid_to_dev[i])
        {
            /* When hardware switching is enabled, enable the Linux bridge to
               not to forward the bcast on hardware ports */
            gponifid_to_dev[i]->priv_flags &= ~IFF_HW_SWITCH;
        }
    }
#endif

    /* Flush arl table dynamic entries */
    fast_age_all(0);
    return 0;
}


int ethsw_switch_manage_ports_leds(int led_mode)
{
#define AUX_MODE_REG 0x1d
#define LNK_LED_DIS  4 // Bit4

    uint16 v16, i;

    down(&bcm_ethlock_switch_config);

    for (i=0; i<4; i++) {
        ethsw_phy_rreg(enet_sw_port_to_phyid(0, i), AUX_MODE_REG, &v16);

        if(led_mode)
            v16 &= ~(1 << LNK_LED_DIS);
        else
            v16 |= (1 << LNK_LED_DIS);

        ethsw_phy_wreg(enet_sw_port_to_phyid(0, i), AUX_MODE_REG, &v16);
    }

    up(&bcm_ethlock_switch_config);
    return 0;
}
EXPORT_SYMBOL(ethsw_switch_manage_ports_leds);


#if defined(AEI_VDSL_HPNA)
unsigned int AEI_ethsw_get_hpna_link_status(void)
{
    uint16 v16;
    unsigned int link_status;

    ethsw_phy_rreg(HPNA_PHY_ID, 0x10, &v16);
    link_status = ((v16 & 0x8000) || (v16 & 0x0200)) ? 1 : 0;

    return link_status;
}
#endif

/* port = physical port */
int ethsw_phy_intr_ctrl(int port, int on)
{
    uint16 v16;
    int phyId = enet_sw_port_to_phyid(0, port);

    down(&bcm_ethlock_switch_config);

#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96368) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96828) || defined(CONFIG_BCM96818) || defined(CONFIG_BCM96318) || defined(CONFIG_BCM963138) | defined(CONFIG_BCM963381) || defined(CONFIG_BCM963148)
    if (on != 0)
        v16 = MII_INTR_ENABLE | MII_INTR_FDX | MII_INTR_SPD | MII_INTR_LNK;
    else
        v16 = 0;

    ethsw_phy_wreg(phyId, MII_INTERRUPT, &v16);
#endif

#if defined(CONFIG_BCM963268)
#if defined(CONFIG_BCM963268)
    if (port == GPHY_PORT_ID)
#endif
        {
            if (on != 0)
                v16 = ~(MII_INTR_FDX | MII_INTR_SPD | MII_INTR_LNK);
            else
                v16 = 0xFFFF;

            ethsw_phy_wreg(phyId, MII_INTERRUPT_MASK, &v16);
        }
#endif

    up(&bcm_ethlock_switch_config);

    return 0;
}

void ethsw_port_mirror_get(int *enable, int *mirror_port, unsigned int *ing_pmap,
                           unsigned int *eg_pmap, unsigned int *blk_no_mrr,
                           int *tx_port, int *rx_port)
{
    uint16 v16;
    ethsw_rreg(PAGE_MANAGEMENT, REG_MIRROR_CAPTURE_CTRL,  (uint8*)&v16, sizeof(v16));
    if (v16 & REG_MIRROR_ENABLE)
    {
        *enable = 1;
        *mirror_port = v16 & REG_CAPTURE_PORT_M;
        *blk_no_mrr = v16 & REG_BLK_NOT_MIRROR;
        ethsw_rreg(PAGE_MANAGEMENT, REG_MIRROR_INGRESS_CTRL, (uint8*)&v16, sizeof(v16));
        *ing_pmap = v16 & REG_INGRESS_MIRROR_M;
        ethsw_rreg(PAGE_MANAGEMENT, REG_MIRROR_EGRESS_CTRL, (uint8*)&v16, sizeof(v16));
        *eg_pmap = v16 & REG_EGRESS_MIRROR_M;
    }
    else
    {
        *enable = 0;
    }
}
void ethsw_port_mirror_set(int enable, int mirror_port, unsigned int ing_pmap, 
                           unsigned int eg_pmap, unsigned int blk_no_mrr, 
                           int tx_port, int rx_port)
{
    uint16 v16;
    if (enable)
    {
        v16 = REG_MIRROR_ENABLE;
        v16 |= (mirror_port & REG_CAPTURE_PORT_M);
        v16 |= blk_no_mrr?REG_BLK_NOT_MIRROR:0;

        ethsw_wreg(PAGE_MANAGEMENT, REG_MIRROR_CAPTURE_CTRL, (uint8*)&v16, sizeof(v16));
        v16 = ing_pmap & REG_INGRESS_MIRROR_M;
        ethsw_wreg(PAGE_MANAGEMENT, REG_MIRROR_INGRESS_CTRL, (uint8*)&v16, sizeof(v16));
        v16 = eg_pmap & REG_INGRESS_MIRROR_M;
        ethsw_wreg(PAGE_MANAGEMENT, REG_MIRROR_EGRESS_CTRL, (uint8*)&v16, sizeof(v16));
    }
    else
    {
        v16  = 0;
        ethsw_wreg(PAGE_MANAGEMENT, REG_MIRROR_CAPTURE_CTRL, (uint8*)&v16, sizeof(v16));
    }
}

MODULE_LICENSE("GPL");

