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
#if defined(CONFIG_BCM96838) || defined(CONFIG_BCM96848)
#include "phys_common_drv.h"
#endif
#include "hwapi_mac.h"
#include <rdpa_api.h>
#include "bcmswshared.h"
#include "ethsw.h"
#include "ethsw_phy.h"
#include "eth_pwrmngt.h"
#include "bcmsw_runner.h"

#if defined(CONFIG_BCM96838) || defined(CONFIG_BCM96848)
#define RUNNER_PORT_MIRROR_SUPPORT
#endif

#if defined(AEI_SCKIPIO_SUPPORT)
/* Code is designed to be daughter board agnostic so even if it is loaded to a 63138 board without 
 * Sckipio daughter board, image shouldstill run normally. It does this by checking for presence of 
 * Sckipio daughter ID early during ethernet driver boot and setting proper flags to later execute
 * or not execute code Sckipio setup.  
 */
#include <board.h>

/* There are functions in shared dir like bcm_gpio_set_dir,bcm_gpio_set_data and
   bcm_set_pinmux that we can use but they need to be exported. Instead
   of modifying files there, just add 1 function here to do what we need.
 */
void driveGPIO(unsigned int gpio_num,unsigned int mux_num, unsigned int dir, unsigned int io)
{
    unsigned int tp_blk_data_lsb;

    tp_blk_data_lsb= 0;
    tp_blk_data_lsb |= gpio_num;
    tp_blk_data_lsb |= (mux_num << PINMUX_DATA_SHIFT);
    GPIO->TestPortBlockDataMSB = 0;
    GPIO->TestPortBlockDataLSB = tp_blk_data_lsb;
    GPIO->TestPortCmd = LOAD_MUX_REG_CMD;

    udelay(1);

    if(dir)	
        GPIO->GPIODir[GPIO_NUM_TO_ARRAY_IDX(gpio_num)] |= GPIO_NUM_TO_MASK(gpio_num);	
    else
        GPIO->GPIODir[GPIO_NUM_TO_ARRAY_IDX(gpio_num)] &= ~GPIO_NUM_TO_MASK(gpio_num);

    udelay(1);

    if (io)
        GPIO->GPIOio[GPIO_NUM_TO_ARRAY_IDX(gpio_num)] |= GPIO_NUM_TO_MASK(gpio_num);	
    else
        GPIO->GPIOio[GPIO_NUM_TO_ARRAY_IDX(gpio_num)] &= ~GPIO_NUM_TO_MASK(gpio_num);
}

extern int pca9555_get_input_value(void);
extern int AEI_get_boardid(void);

enum
{
    SGMII_TO_SFP,
    SGMII_TO_SCKIPIO,
};

enum
{
    GPIO_DIR_INPUT,
    GPIO_DIR_OUTPUT,
};

enum
{
    GPIO_IO_LOW,
    GPIO_IO_HIGH,
};

#define GPIO_SCKIPIO_RESET 22
#define GPIO_SGMII_REDIRECT 138
#define SFP_PLUGGED !(*(u32*)(GPIO_BASE + 0x14) & BIT(27))

bool SckipioInitComplete = false;
bool SckipioDaughterBoard = false;
bool enableSckipio = false;
#endif

extern struct semaphore bcm_ethlock_switch_config;

#if defined(SWITCH_REG_SINGLE_SERDES_CNTRL)
enum
{
    SFP_NO_MODULE,
    SFP_FIBER,
    SFP_COPPER,
#if defined(AEI_SCKIPIO_SUPPORT)
    DAUGHTER_SCKIPIO,
#endif
};

static PHY_STAT ethsw_serdes_stat(int phyId);
static int sfp_module_type = SFP_NO_MODULE;
static int sfp_module_fiber_speed = BMCR_SPEED1000|BMCR_FULLDPLX;

int ethsw_sfp_get_speed(int phyId)
{
    return sfp_module_fiber_speed;
}

void ethsw_sfp_restore_from_power_saving(int phyId)
{
    if (sfp_module_type == SFP_NO_MODULE)
        return;

#if defined(CONFIG_I2C)
    if(sfp_module_type == SFP_COPPER)
    {
        /* Configure Serdes into SGMII mode */
        ethsw_config_serdes_sgmii(phyId);
        /* Delay for hardware stablized */
        msleep(50);
    }
    else
#endif
#if defined(AEI_SCKIPIO_SUPPORT)
    if(sfp_module_type == DAUGHTER_SCKIPIO)
    {
        ethsw_config_serdes_sgmii(phyId);
        //ethsw_config_serdes_1kx(phyId);
        /* Delay for hardware stablized */
        msleep(50);
    }
    else
#endif
    {
        switch(sfp_module_fiber_speed)
        {
            case BMCR_SPEED1000|BMCR_FULLDPLX:
                ethsw_config_serdes_1kx(phyId);
                /* Delay for hardware stablized */
                msleep(50);
                break;
            case BMCR_SPEED100|BMCR_FULLDPLX:
                ethsw_config_serdes_100fx(phyId);
                /* Delay for hardware stablized */
                msleep(80);
                break;
            default:
                break;
        }
    }
}

void ethsw_sfp_set_speed(int phyId, int speed)
{
    if (speed != (BMCR_SPEED1000| BMCR_FULLDPLX) && speed != (BMCR_SPEED100| BMCR_FULLDPLX))
    {
        printk("Not Supported Speed %x attempted to set to Serdes\n", speed);
        return;
    }

    ETHSW_POWERDOWN_SERDES(phyId);
    sfp_module_fiber_speed = speed;
    ETHSW_POWERUP_SERDES(phyId);
    ethsw_sfp_restore_from_power_saving(phyId);
    ethsw_serdes_stat(phyId);
}

#if defined(CONFIG_I2C)
static void ethsw_init_copper_sfp(void)
{
    sfp_i2c_phy_write(0x0, 0x8140);     /* Software reset */

    /* Configure SFP PHY into SGMII mode */
    sfp_i2c_phy_write(0x1b, 0x9084);    /* Enable SGMII mode */
    sfp_i2c_phy_write(0x9, 0x0f00);     /* Advertise 1kBase-T Full/Half-Duplex */
    sfp_i2c_phy_write(0x0, 0x8140);     /* Software reset */
    sfp_i2c_phy_write(0x4, 0x0de1);     /* Adverstize 100/10Base-T Full/Half-Duplex */
    sfp_i2c_phy_write(0x0, 0x9140);     /* Software reset */
}

static int sfp_i2c_module_detect(void)
{
    u32 val32;

    /* If I2C read operation succeeds, I2C module is connected
        and which means it is a copper SFP module */
    if (sfp_i2c_phy_read(0, &val32))
    {
        sfp_module_type = SFP_COPPER;
        ethsw_init_copper_sfp();

        printk("Copper SFP Module Plugged in\n");
        return 1;
    }
    return 0;
}
#endif

enum {SFP_MODULE_OUT, SFP_MODULE_IN, SFP_CABLE_IN, SFP_LINK_UP};
static int sfp_status = SFP_MODULE_OUT;
static int serdes_power_mode = SERDES_BASIC_POWER_SAVING;

#if defined(AEI_SCKIPIO_SUPPORT)
/* put this block of code here so don't have to move the original
   globals like sfp_status and sfp_module_type above
*/
void resetSckipio(void)
{
    driveGPIO(GPIO_SCKIPIO_RESET,5,GPIO_DIR_OUTPUT,GPIO_IO_LOW);
    msleep(500);
    driveGPIO(GPIO_SCKIPIO_RESET,5,GPIO_DIR_OUTPUT,GPIO_IO_HIGH);
}

void redirectSGMII(int target)
{
    if (target == SGMII_TO_SCKIPIO)
        driveGPIO(GPIO_SGMII_REDIRECT,5,GPIO_DIR_OUTPUT,GPIO_IO_LOW);
    else
        driveGPIO(GPIO_SGMII_REDIRECT,5,GPIO_DIR_OUTPUT,GPIO_IO_HIGH);
}

void configSckipio(int phyId)
{
    /* need to do this sequence of
       1. powerdown serdes
       2. config serdes
       3. powerup serdes 
       for Sckipio to work through SGMII 
     */
    ETHSW_POWERDOWN_SERDES(phyId);
    sfp_module_type = DAUGHTER_SCKIPIO;
    ethsw_sfp_restore_from_power_saving(phyId);
    ETHSW_POWERUP_SERDES(phyId);

    sfp_status = SFP_LINK_UP;

    resetSckipio();
    redirectSGMII(SGMII_TO_SCKIPIO);

    return;
}
#endif

void ethsw_serdes_power_mode_set(int phy_id, int mode)
{
    if (serdes_power_mode == mode)
        return;

    serdes_power_mode = mode;

    if (sfp_status == SFP_LINK_UP)
        return;

    if(mode == SERDES_NO_POWER_SAVING)
    {
        ETHSW_POWERUP_SERDES(phy_id);
        ethsw_sfp_restore_from_power_saving(phy_id);
    }
    else if (mode == SERDES_FORCE_OFF)
        ETHSW_POWERDOWN_FORCE_OFF_SERDES(phy_id);
    else
        ETHSW_POWERDOWN_SERDES(phy_id);

    if(sfp_status == SFP_CABLE_IN && mode != SERDES_AGGRESIVE_POWER_SAVING)
        sfp_status = SFP_MODULE_OUT;
}

void ethsw_serdes_power_mode_get(int phy_id, int *mode)
{
    *mode = serdes_power_mode;
}

/*
    Module detection is not going through SGMII,
    so it can be down even under SGMII power down.
*/
static int ethsw_sfp_module_detect(int phyId)
{
    u32 val32;
#if defined(CONFIG_I2C)
#define I2CDetectDelay 8
    static int i2cDetectDelay = 0;
#endif

#if defined(AEI_SCKIPIO_SUPPORT)
    if (enableSckipio == true)
    {
        sfp_module_type = DAUGHTER_SCKIPIO;
        return 1;
    }
#endif

#if defined(AEI_SFP_DETECTION_BY_GPIO)
    val32 = *(u32*)GPIO_CTL_PIN_DIR_0;

    /* All T3200Ms checked have both GPIO 27 and 28 set when SFP is not plugged in.
     * GPIO 27 is SFP module absent, GPIO 28 is SFP receiver signal lost.
     * If both GPIO 27 and 28 are set, then SFP is unplugged.
     * FiberStore unsets GPIO 28 and 27 when SFP is plugged in.
     * GLC-T(USA and Malaysia) and Method unset only GPIO 27 when SFP is plugged in.
     * Sckipio SFP unsets only GPIO 28 when SFP is plugged in.
     */
    if (val32 & GPIO_CTL_PIN_SFP_PLUG_OUT
       && (val32 & BIT(28)))
    {
        if(sfp_module_type != SFP_NO_MODULE)
        {
            sfp_module_type = SFP_NO_MODULE;
            printk("SFP module unplugged\n");
        }
        return 0;
    }
#else
    val32 = *(u32*)SWITCH_SINGLE_SERDES_STAT;
    if ((val32 & SWITCH_REG_SSER_RXSIG_DET) == 0)

    {
        if(sfp_module_type != SFP_NO_MODULE)
        {
            sfp_module_type = SFP_NO_MODULE;
            printk("SFP module unplugged\n");
        }
        return 0;
    }
#endif

    if (sfp_module_type == SFP_NO_MODULE)
    {
#if defined(CONFIG_I2C)
        if (!sfp_i2c_module_detect())
#endif
        {
#if 0 // defined(SUPPORT_GPL_1) && defined(CONFIG_I2C)
            /*Issue   : SFP couldn't get IP with 100M ethernet line sometimes after reboot. But could get IP after unplug-plugin Ethernet line twice
             *Reason  : Sometimes, sfp_i2c_phy_read read fail first time, then it will read success
             *Solution: Skip fail until sfp_i2c_phy_read success*/
            printk("sfp_i2c_phy_read failed\n");
#else
            /* Configure Serdes into 1000Base-X mode */
            sfp_module_type = SFP_FIBER;

#if defined(CONFIG_I2C)
            i2cDetectDelay = 0;
            printk("Fibre SFP Module Plugged in\n");
#else
            printk("SFP Module Plugged in\n");
#endif
#endif
        }
    }
#if defined(CONFIG_I2C)
    else    /* MODULE is plug in */
    {
        /* Work around for some I2C long initialization, do more I2C after Fiber type detected */
        if (sfp_module_type == SFP_FIBER && i2cDetectDelay < I2CDetectDelay)
        {
            if (sfp_i2c_module_detect())
            {
                i2cDetectDelay = I2CDetectDelay;
            }
            else
            {
                i2cDetectDelay++;
            }
        }
    }
#endif
    return 1;
}

static PHY_STAT ethsw_phyid_sfp_stat(int phyId)
{
    PHY_STAT ps;
    uint16 v16;

    memset(&ps, 0, sizeof(ps));

    /* Based on suggestion from ASIC team, read twice here */
    ethsw_phy_rreg(phyId, MII_STATUS, &v16);
    ethsw_phy_rreg(phyId, MII_STATUS, &v16);
    ps.lnk = (v16 & MII_STATUS_LINK) != 0;

    if(ps.lnk)
    {
        ethsw_phy_exp_rreg(phyId, MIIEX_DIGITAL_STATUS_1000X, &v16);
        ps.spd1000 = (v16 & MIIEX_SPEED) == MIIEX_SPD1000;
        ps.spd100 = (v16 & MIIEX_SPEED) == MIIEX_SPD100;
        ps.spd10 = (v16 & MIIEX_SPEED) == MIIEX_SPD10;
        ps.fdx = (v16 & MIIEX_DUPLEX) > 0;
    }

    return ps;
}

void ethsw_sfp_init(int phyId)
{
#if defined(AEI_SCKIPIO_SUPPORT)
    /* Check for Sckipio daughter board early during driver init */
    int hwBoardId = AEI_get_boardid();
    int daugterBoardId = pca9555_get_input_value() & 0xff;

    if (hwBoardId == 0x6 && daugterBoardId == 0x12)
    {
        SckipioDaughterBoard = true;
        printk("Detected Sckipio daughter board\n");
    }
    else
        SckipioDaughterBoard = false;
#endif

    /* Call the function to init state machine without leaving
       SFP on without control during initialization */
    ethsw_serdes_stat(phyId);
}

static PHY_STAT ethsw_serdes_stat(int phyId)
{
    PHY_STAT ps;
    uint32 val32;

    memset(&ps, 0, sizeof(ps));
    if (serdes_power_mode == SERDES_FORCE_OFF) {
        //should already be powered down -- just exit
        goto sfp_end;
    }
    
    if(    (serdes_power_mode == SERDES_BASIC_POWER_SAVING || serdes_power_mode == SERDES_AGGRESIVE_POWER_SAVING)
        && sfp_status < SFP_LINK_UP) 
        ETHSW_POWERSTANDBY_SERDES(phyId);

    val32 = *(u32*)SWITCH_SINGLE_SERDES_STAT;
    switch (sfp_status)
    {
        case SFP_MODULE_OUT:
sfp_module_out:
            if(sfp_status == SFP_MODULE_OUT && ethsw_sfp_module_detect(phyId))
                goto sfp_module_in;

            sfp_status = SFP_MODULE_OUT;
            goto sfp_end;

        case SFP_MODULE_IN:
sfp_module_in:
            if(sfp_status >= SFP_MODULE_IN && !ethsw_sfp_module_detect(phyId))
            {
                sfp_status = SFP_MODULE_IN;
                goto sfp_module_out;
            }

            if(sfp_status <= SFP_MODULE_IN)
            {
                if(serdes_power_mode == SERDES_BASIC_POWER_SAVING)
                {
                    ETHSW_POWERUP_SERDES(phyId);
                    ethsw_sfp_restore_from_power_saving(phyId);
                }

                if(serdes_power_mode < SERDES_AGGRESIVE_POWER_SAVING)
                {
                    ps = ethsw_phyid_sfp_stat(phyId);

#if defined(AEI_SCKIPIO_SUPPORT)
                    if (enableSckipio && !SckipioInitComplete)
                    {
                        configSckipio(phyId);
                        sfp_status = SFP_MODULE_IN;
                        SckipioInitComplete = true;
                        goto sfp_link_up;
                    }
#endif
                    if(ps.lnk)
                    {
                        sfp_status = SFP_MODULE_IN;
                        goto sfp_link_up;
                    }
                }
                else if (serdes_power_mode == SERDES_AGGRESIVE_POWER_SAVING)
                {
                    if ((val32 & SWITCH_REG_SSER_EXTFB_DET) == 0) /* Signal Detected*/
                    {
                        sfp_status = SFP_MODULE_IN;
                        printk("Carrier signal detected, power up Serdes interface\n");
                        goto sfp_cable_in;
                    }
                }
            }
            sfp_status = SFP_MODULE_IN;
            goto sfp_end;

        case SFP_CABLE_IN:  /* For SERDES_AGGRESIVE_POWER_SAVING mode only */
sfp_cable_in:
            if (sfp_status >= SFP_CABLE_IN && (val32 & SWITCH_REG_SSER_EXTFB_DET) == 1) /* Not signal */
            {
                /* Power standby serdes */
                printk("Carrier signal LOSS detected, power standby Serdes interface\n");
                sfp_status = SFP_CABLE_IN;
                goto sfp_module_in;
            }

            if(sfp_status <= SFP_CABLE_IN)
            {
                ETHSW_POWERUP_SERDES(phyId);
                ethsw_sfp_restore_from_power_saving(phyId);
                ps = ethsw_phyid_sfp_stat(phyId);
                if(ps.lnk)
                {
                    sfp_status = SFP_CABLE_IN;
                    goto sfp_link_up;
                }
            }

            sfp_status = SFP_CABLE_IN;
            goto sfp_end;

        case SFP_LINK_UP:
sfp_link_up:
            if(sfp_status == SFP_LINK_UP)
            {
#if defined(AEI_SCKIPIO_SUPPORT)
                /* advertize a link up fdx 1Gps for SGMII to Sckipio */
                if (enableSckipio && SckipioInitComplete)
                {
                    ps.lnk = 1;
                    ps.spd1000 = 1;
                    ps.fdx = 1;
                } else 
#endif
                ps = ethsw_phyid_sfp_stat(phyId);
                if(!ps.lnk)
                {
                    if(serdes_power_mode == SERDES_AGGRESIVE_POWER_SAVING)
                        goto sfp_cable_in;
                    else
                        goto sfp_module_in;
                }
            }
            sfp_status = SFP_LINK_UP;
            goto sfp_end;
    }

sfp_end:
    if(   (serdes_power_mode == SERDES_BASIC_POWER_SAVING || serdes_power_mode == SERDES_AGGRESIVE_POWER_SAVING)
       && sfp_status != SFP_LINK_UP)
        ETHSW_POWERDOWN_SERDES(phyId);
    return ps;
}
#endif

#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
PHY_STAT ethsw_phyid_stat(int phyId) /* FIXME  - duplicate code; Merge together with ethsw_dma.c */
{
    PHY_STAT ps;
    uint16 v16;
    uint16 ctrl;
    uint16 mii_esr = 0;
    uint16 mii_stat = 0, mii_adv = 0, mii_lpa = 0;
    uint16 mii_gb_ctrl = 0, mii_gb_stat = 0;

    memset(&ps, 0, sizeof(ps));
    if (!IsPhyConnected(phyId))
    {
        ps.lnk = 1;
        ps.fdx = 1;
        if (IsMII(phyId))
            ps.spd100 = 1;
        else
            ps.spd1000 = 1;
        return ps;
    }

    down(&bcm_ethlock_switch_config);

#if defined(SWITCH_REG_SINGLE_SERDES_CNTRL)
    if((phyId & MAC_IFACE) == MAC_IF_SERDES)
    {

#if defined(AEI_SCKIPIO_SUPPORT)
        /* Logic: If Sckipio daughter board is detected, check
         * if SFP is plugged in. If yes, SFP trumps Sckipio
         * and we switch SGMII to SFP.  If SFP is not plugged in,
         * then switch SGMII to Sckipio.  Since SFP and 
         * Sckipio share 1 SGMII, it does not make much sense to
         * have SFP plugged in and wanting to run Sckipio
         */ 
        if (SckipioDaughterBoard)
        {
            if (SFP_PLUGGED)
            {
                enableSckipio = false;

                if (SckipioInitComplete)
                { 
                    redirectSGMII(SGMII_TO_SFP);
                    SckipioInitComplete=false;
                }
            }
            else
                enableSckipio = true;

            if (enableSckipio && SckipioInitComplete)
            {
                ps.lnk = 1;
                ps.spd1000 = 1;
                ps.fdx = 1;
            }
            else 
                ps = ethsw_serdes_stat(phyId);
        }
        else
            ps = ethsw_serdes_stat(phyId);
#else
        ps = ethsw_serdes_stat(phyId);
#endif
        goto end;
    }
#endif  /* SWITCH_REG_SINGLE_SERDES_CNTRL */

    ethsw_phy_rreg(phyId, MII_INTERRUPT, &v16);
    ethsw_phy_rreg(phyId, MII_ASR, &v16);
    BCM_ENET_DEBUG("%s mii_asr (reg 25) 0x%x\n", __FUNCTION__, v16);


    if (!MII_ASR_LINK(v16)) {
        goto end;
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

        goto end;
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
end:
    up(&bcm_ethlock_switch_config);
    return ps;
}
#endif /* 963138 || 963148 */

#if defined(CONFIG_BCM96838) || defined(CONFIG_BCM96848)
PHY_STAT ethsw_phy_stat(int unit, int port, int cb_port) /* FIXME  - very similar code/functionality; Merge together with the above */
{
    PHY_STAT phys;
    phy_rate_t curr_phy_rate;
    int phyId;

    memset(&phys, 0, sizeof(phys));
    phyId = enet_sw_port_to_phyid(0, port);
    if (!IsPhyConnected(phyId))
    {
        // 0xff PHY ID means no PHY on this port.
        phys.lnk = 1;
        phys.fdx = 1;
        if (IsMII(phyId))
            phys.spd100 = 1;
        else
            phys.spd1000 = 1;
        return phys;
    }

    curr_phy_rate = phy_get_line_rate_and_duplex(port);
    phys.lnk = curr_phy_rate < PHY_RATE_LINK_DOWN;
    switch (curr_phy_rate)
    {
    case PHY_RATE_10_FULL:
        phys.fdx = 1;
        break;
    case PHY_RATE_10_HALF:
        break;
    case PHY_RATE_100_FULL:
        phys.fdx = 1;
        phys.spd100 = 1;
        break;
    case PHY_RATE_100_HALF:
        phys.spd100 = 1;
        break;
    case PHY_RATE_1000_FULL:
        phys.spd1000 = 1;
        phys.fdx = 1;
        break;
    case PHY_RATE_1000_HALF:
        phys.spd1000 = 1;
        break;
    case PHY_RATE_LINK_DOWN:
        break;
    default:
        break;
    }

    return phys;
}
#endif

int ethsw_reset_ports(struct net_device *dev)
{
    return 0;
}

int ethsw_add_proc_files(struct net_device *dev)
{
    return 0;
}

int ethsw_del_proc_files(void)
{
    return 0;
}

int ethsw_disable_hw_switching(void)
{
    return 0;
}

void ethsw_dump_page(int page)
{
}

void fast_age_port(uint8_t port, uint8_t age_static)
{
}

int ethsw_counter_collect(uint32_t portmap, int discard)
{
    return 0;
}

void ethsw_get_txrx_imp_port_pkts(unsigned int *tx, unsigned int *rx)
{
}
EXPORT_SYMBOL(ethsw_get_txrx_imp_port_pkts);

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

int ethsw_phy_intr_ctrl(int port, int on)
{
    return 0;
}
void ethsw_phy_apply_init_bp(void)
{
}
int ethsw_setup_led(void)
{
    return 0;
}
int ethsw_setup_phys(void)
{
#if defined(AEI_VDSL_63138_WAN_ETHER_LINK_PATCH)
    //enable ethernet@wirespeed of GE port
    uint16 v16 = 0x8077;
    // runner ethWAN (GPHY4)
    ethsw_phy_wreg(12, 0x18, &v16);       
    
    // set retry limit to 2, test result is about 10 sec, default is about 20 sec.
    ethsw_phy_rreg(12, MII_REGISTER_1C, &v16);
    v16 |= MII_1C_WRITE_ENABLE; //Write Enable
    v16 &= ~(0x1F << 10); //clear Shadow Register Selector
    v16 |= 1<<12;//set Shadow Register Selector
    v16 &= ~(0x7 << 2);//Ethernet@WireSpeed Retry Limit
    ethsw_phy_wreg(12, MII_REGISTER_1C, &v16);

#endif
#if defined(AEI_VDSL_63138_LAN_ETHER_LINK_PATCH)
    {
        //enable ethernet@wirespeed of GE port
        uint16 v16;
        // Lan port (GPHY0-GPHY3)
        int phyid;
        for (phyid=8; phyid<12; phyid++)
        {
            v16 = 0x8077;
            ethsw_phy_wreg(phyid, 0x18, &v16);

            // set retry limit to 2, test result is about 10 sec, default is about 20 sec.
            ethsw_phy_rreg(phyid, MII_REGISTER_1C, &v16);
            v16 |= MII_1C_WRITE_ENABLE; //Write Enable
            v16 &= ~(0x1F << 10); //clear Shadow Register Selector
            v16 |= 1<<12;//set Shadow Register Selector
            v16 &= ~(0x7 << 2);//Ethernet@WireSpeed Retry Limit
            ethsw_phy_wreg(phyid, MII_REGISTER_1C, &v16);
        }

    }
#endif
    ethsw_shutdown_unused_phys();
    return 0;
}
int ethsw_enable_hw_switching(void)
{
    return 0;
}

/* Code to handle exceptions chip specific cases */
void ethsw_phy_handle_exception_cases(void)
{
}

int bcmeapi_ethsw_dump_mib(int port, int type)
{
    int					rc;
    rdpa_emac_stat_t 	emac_cntrs;
    bdmf_object_handle 	port_obj = NULL;
    rdpa_if             rdpa_port = rdpa_if_lan0 + (rdpa_if)port;
    rdpa_port_dp_cfg_t  port_cfg = {};


    rc = rdpa_port_get(rdpa_port,&port_obj);
    if ( rc != BDMF_ERR_OK)
    {
        printk("failed to get rdpa port object rc=%d\n",rc);
        return -1;
    }

    rc = rdpa_port_cfg_get(port_obj, &port_cfg);
    if ( rc != BDMF_ERR_OK)
    {
       printk("failed to rdpa_port_cfg_get rc=%d\n",rc);
       return -1;
    }

    if (rdpa_emac_none == port_cfg.emac)
    {
        printk("Invalid emac id = %d\n", port_cfg.emac);
        return -1;
    }

    mac_hwapi_get_rx_counters (port_cfg.emac, &emac_cntrs.rx);
    mac_hwapi_get_tx_counters (port_cfg.emac, &emac_cntrs.tx);

    bdmf_put(port_obj);
    printk("\nRunner Stats : Port# %d\n",port);

    /* Display Tx statistics */
    printk("\n");
    printk("TxUnicastPkts:          %10u \n", (unsigned int)emac_cntrs.tx.packet);
    printk("TxMulticastPkts:        %10u \n", (unsigned int)emac_cntrs.tx.multicast_packet);
    printk("TxBroadcastPkts:        %10u \n", (unsigned int)emac_cntrs.tx.broadcast_packet);
    printk("TxDropPkts:             %10u \n", (unsigned int)emac_cntrs.tx.error);

    /* Display remaining tx stats only if requested */
    if (type) {
        printk("TxBytes:                %10u \n", (unsigned int)emac_cntrs.tx.byte);
        printk("TxFragments:            %10u \n", (unsigned int)emac_cntrs.tx.fragments_frame);
        printk("TxCol:                  %10u \n", (unsigned int)emac_cntrs.tx.total_collision);
        printk("TxSingleCol:            %10u \n", (unsigned int)emac_cntrs.tx.single_collision);
        printk("TxMultipleCol:          %10u \n", (unsigned int)emac_cntrs.tx.multiple_collision);
        printk("TxDeferredTx:           %10u \n", (unsigned int)emac_cntrs.tx.deferral_packet);
        printk("TxLateCol:              %10u \n", (unsigned int)emac_cntrs.tx.late_collision);
        printk("TxExcessiveCol:         %10u \n", (unsigned int)emac_cntrs.tx.excessive_collision);
        printk("TxPausePkts:            %10u \n", (unsigned int)emac_cntrs.tx.pause_control_frame);
        printk("TxExcessivePkts:        %10u \n", (unsigned int)emac_cntrs.tx.excessive_deferral_packet);
        printk("TxJabberFrames:         %10u \n", (unsigned int)emac_cntrs.tx.jabber_frame);
        printk("TxFcsError:             %10u \n", (unsigned int)emac_cntrs.tx.fcs_error);
        printk("TxCtrlFrames:           %10u \n", (unsigned int)emac_cntrs.tx.control_frame);
        printk("TxOverSzFrames:         %10u \n", (unsigned int)emac_cntrs.tx.oversize_frame);
        printk("TxUnderSzFrames:        %10u \n", (unsigned int)emac_cntrs.tx.undersize_frame);
        printk("TxUnderrun:             %10u \n", (unsigned int)emac_cntrs.tx.underrun);
        printk("TxPkts64Octets:         %10u \n", (unsigned int)emac_cntrs.tx.frame_64);
        printk("TxPkts65to127Octets:    %10u \n", (unsigned int)emac_cntrs.tx.frame_65_127);
        printk("TxPkts128to255Octets:   %10u \n", (unsigned int)emac_cntrs.tx.frame_128_255);
        printk("TxPkts256to511Octets:   %10u \n", (unsigned int)emac_cntrs.tx.frame_256_511);
        printk("TxPkts512to1023Octets:  %10u \n", (unsigned int)emac_cntrs.tx.frame_512_1023);
        printk("TxPkts1024to1518Octets: %10u \n", (unsigned int)emac_cntrs.tx.frame_1024_1518);
        printk("TxPkts1519toMTUOctets:  %10u \n", (unsigned int)emac_cntrs.tx.frame_1519_mtu);
    }

    /* Display Rx statistics */
    printk("\n");
    printk("RxUnicastPkts:          %10u \n", (unsigned int)emac_cntrs.rx.packet);
    printk("RxMulticastPkts:        %10u \n", (unsigned int)emac_cntrs.rx.multicast_packet);
    printk("RxBroadcastPkts:        %10u \n", (unsigned int)emac_cntrs.rx.broadcast_packet);

    /* Display remaining rx stats only if requested */
    if (type) {
        printk("RxBytes:                %10u \n", (unsigned int)emac_cntrs.rx.byte);
        printk("RxJabbers:              %10u \n", (unsigned int)emac_cntrs.rx.jabber);
        printk("RxAlignErrs:            %10u \n", (unsigned int)emac_cntrs.rx.alignment_error);
        printk("RxFCSErrs:              %10u \n", (unsigned int)emac_cntrs.rx.fcs_error);
        printk("RxFragments:            %10u \n", (unsigned int)emac_cntrs.rx.fragments);
        printk("RxOversizePkts:         %10u \n", (unsigned int)emac_cntrs.rx.oversize_packet);
        printk("RxUndersizePkts:        %10u \n", (unsigned int)emac_cntrs.rx.undersize_packet);
        printk("RxPausePkts:            %10u \n", (unsigned int)emac_cntrs.rx.pause_control_frame);
        printk("RxOverflow:             %10u \n", (unsigned int)emac_cntrs.rx.overflow);
        printk("RxCtrlPkts:             %10u \n", (unsigned int)emac_cntrs.rx.control_frame);
        printk("RxUnknownOp:            %10u \n", (unsigned int)emac_cntrs.rx.unknown_opcode);
        printk("RxLenError:             %10u \n", (unsigned int)emac_cntrs.rx.frame_length_error);
        printk("RxCodeError:            %10u \n", (unsigned int)emac_cntrs.rx.code_error);
        printk("RxCarrierSenseErr:      %10u \n", (unsigned int)emac_cntrs.rx.carrier_sense_error);
        printk("RxPkts64Octets:         %10u \n", (unsigned int)emac_cntrs.rx.frame_64);
        printk("RxPkts65to127Octets:    %10u \n", (unsigned int)emac_cntrs.rx.frame_65_127);
        printk("RxPkts128to255Octets:   %10u \n", (unsigned int)emac_cntrs.rx.frame_128_255);
        printk("RxPkts256to511Octets:   %10u \n", (unsigned int)emac_cntrs.rx.frame_256_511);
        printk("RxPkts512to1023Octets:  %10u \n", (unsigned int)emac_cntrs.rx.frame_512_1023);
        printk("RxPkts1024to1522Octets: %10u \n", (unsigned int)emac_cntrs.rx.frame_1024_1518);
        printk("RxPkts1523toMTU:        %10u \n", (unsigned int)emac_cntrs.rx.frame_1519_mtu);
    }
    return 0;
}

#if defined(RUNNER_PORT_MIRROR_SUPPORT)
static int convert_pmap2portNum(unsigned int pmap)
{
    int portNum = 0xFFFF;

    if (pmap == 0)
        return portNum;

    for (portNum = 0;;portNum++) {
        if (pmap & (1 << portNum))
            break;
    }

    return portNum;
}
#endif

void ethsw_port_mirror_get(int *enable, int *mirror_port, unsigned int *ing_pmap,
                           unsigned int *eg_pmap, unsigned int *blk_no_mrr,
                           int *tx_port, int *rx_port)
{
#if defined(RUNNER_PORT_MIRROR_SUPPORT)
    bdmf_object_handle port_obj = NULL;
    rdpa_port_mirror_cfg_t mirror_cfg;
    rdpa_if port_index;

    *enable = 0;
    *mirror_port = 0;
    *ing_pmap = 0;
    *eg_pmap = 0;
    *blk_no_mrr = 0;
    *tx_port = -1;
    *rx_port = -1;

    if (rdpa_port_get(rdpa_if_wan0, &port_obj) != 0)
        return;

    memset(&mirror_cfg, 0, sizeof(mirror_cfg));

    if (rdpa_port_mirror_cfg_get(port_obj, &mirror_cfg) != 0)
        return;

    if (mirror_cfg.rx_dst_port != NULL)
    {
        if (rdpa_port_index_get(mirror_cfg.rx_dst_port, &port_index) == 0)
        {
            *ing_pmap = 1 << EPON_PORT_ID;
            *enable = 1;
            *rx_port = port_index - rdpa_if_lan0;
        }
    }

    if (mirror_cfg.tx_dst_port != NULL)
    {
        if (rdpa_port_index_get(mirror_cfg.tx_dst_port, &port_index) == 0)
        {
            *eg_pmap = 1 << EPON_PORT_ID;
            *enable = 1;
            *tx_port = port_index - rdpa_if_lan0;
        }
    }

    if (port_obj)
        bdmf_put(port_obj);

#else
    printk("Runner port mirroring is not supported\n");
#endif
}

void ethsw_port_mirror_set(int enable, int mirror_port, unsigned int ing_pmap,
                           unsigned int eg_pmap, unsigned int blk_no_mrr,
                           int tx_port, int rx_port)
{
#if defined(RUNNER_PORT_MIRROR_SUPPORT)
    bdmf_object_handle port_obj = NULL, tx_port_obj = NULL, rx_port_obj = NULL;
    rdpa_port_mirror_cfg_t mirror_cfg;
    int src_rx_port = convert_pmap2portNum(ing_pmap);
    int dst_rx_port = (rx_port == -1)?mirror_port:rx_port;
    int src_tx_port = convert_pmap2portNum(eg_pmap);
    int dst_tx_port = (tx_port == -1)?mirror_port:tx_port;

    /* Only support mirror WAN port now */
    if ((ing_pmap ==0) && (eg_pmap == 0)) {
        printk("Invalid ingress and egress port map %x - %x\n", ing_pmap, eg_pmap);
        return;
    }

    if ((ing_pmap != 0) && (src_rx_port != EPON_PORT_ID)) {
        printk("Invalid ingress port map %x\n", ing_pmap);
        return;
    }

    if ((eg_pmap != 0) && (src_tx_port != EPON_PORT_ID)) {
        printk("Invalid egress port map %x\n", eg_pmap);
        return;
    }

    if (rdpa_port_get(rdpa_if_wan0, &port_obj) != 0)
        return;

    memset(&mirror_cfg, 0, sizeof(mirror_cfg));

    if (rdpa_port_mirror_cfg_get(port_obj, &mirror_cfg) != 0)
        goto free_all;

    /* Get the port which mirrors Rx traffic */
    if (src_rx_port == EPON_PORT_ID) {
        if(!rdpa_if_is_lan(rdpa_if_lan0 + dst_rx_port) ||
           (rdpa_port_get(rdpa_if_lan0 + dst_rx_port, &rx_port_obj) != 0)) {
            printk("Invalid mirror port(Rx) %d\n", dst_rx_port);
            goto free_all;
        }
    }

    /* Get the port which mirrors Tx traffic */
    if (src_tx_port == EPON_PORT_ID) {
        if(!rdpa_if_is_lan(rdpa_if_lan0 + dst_tx_port) ||
           (rdpa_port_get(rdpa_if_lan0 + dst_tx_port, &tx_port_obj) != 0)) {
            printk("Invalid mirror port(Tx) %d\n", dst_tx_port);
            goto free_all;
        }
    }

    if (!enable) {
        if (mirror_cfg.rx_dst_port == rx_port_obj)
            mirror_cfg.rx_dst_port = NULL;

        if (mirror_cfg.tx_dst_port == tx_port_obj)
            mirror_cfg.tx_dst_port = NULL;
    }
    else {
        if (rx_port_obj != NULL)
            mirror_cfg.rx_dst_port = rx_port_obj;

        if (tx_port_obj != NULL )
            mirror_cfg.tx_dst_port = tx_port_obj;
    }

    if (rdpa_port_mirror_cfg_set(port_obj, &mirror_cfg) != 0)
        printk("Set port mirror failed!\n");

free_all:
    if (rx_port_obj)
        bdmf_put(rx_port_obj);

    if (tx_port_obj)
        bdmf_put(tx_port_obj);

    if (port_obj)
        bdmf_put(port_obj);

#else
    printk("Runner port mirroring is not supported\n");
#endif
}

