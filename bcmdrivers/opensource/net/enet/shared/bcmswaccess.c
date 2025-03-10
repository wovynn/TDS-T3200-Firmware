/*
<:copyright-BRCM:2002:DUAL/GPL:standard

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

#define _BCMENET_LOCAL_
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <linux/mii.h>
#include <asm/uaccess.h>
#include <board.h>
#include "boardparms.h"
#include <bcm_map_part.h>
#include "bcmSpiRes.h"
#include <linux/bcm_log.h>
#include <bcm/bcmswapitypes.h>
#include "bcmtypes.h"
#include "bcmswshared.h"
#include "bcmenet.h"
#include "bcmswaccess.h"
#include "bcmmii.h"
#include "ethsw_phy.h"
#include "bcmsw.h"
#include "eth_pwrmngt.h"

#define IS_PHY_ADDR_FLAG 0x80000000
#define IS_SPI_ACCESS    0x40000000

#define PORT_ID_M 0xF
#define PORT_ID_S 0
#define PHY_REG_M 0x1F
#define PHY_REG_S 4

extern int dump_enable;

extern struct semaphore bcm_ethlock_switch_config;
extern spinlock_t bcm_extsw_access;
/* The external switch physical port to phyid mapping */

#if defined(CONFIG_BCM_ETH_PWRSAVE)
DEFINE_SPINLOCK(bcmsw_pll_control_lock);
#endif

#define SWITCH_ADDR_MASK 0xFFFF

static int bcmsw_phy_access(struct ethswctl_data *e, int access_type)
{
    uint16 phy_reg_val;
    uint32 data = 0;
    int ext_bit = 0, phy_id, reg_offset; 

    if (access_type == MBUS_UBUS)
        phy_id = enet_sw_port_to_phyid(0, e->offset & PORT_ID_M);
    else
        phy_id = enet_sw_port_to_phyid(1, e->offset & PORT_ID_M);

    reg_offset = (e->offset >> PHY_REG_S) & PHY_REG_M;
    
    if (e->type == TYPE_GET) {
        down(&bcm_ethlock_switch_config);
        if (access_type == MBUS_UBUS) {
            ethsw_phy_rreg(phy_id, reg_offset, &phy_reg_val);
        } else {
            ext_bit = 1;
            ethsw_phy_read_reg(phy_id, reg_offset, &phy_reg_val, ext_bit);
        }
        up(&bcm_ethlock_switch_config);
        data = phy_reg_val;
        data = __le32_to_cpu(data);
        BCM_ENET_LINK_DEBUG("%s accType 0x%x regoff 0x%x phy 0x%x phy_reg_val =0x%x \n", __FUNCTION__,
                            access_type, reg_offset, phy_id, data);
        
        if (copy_to_user((void*)(&e->data), (void*)&data, 4)) {
            return -EFAULT;
        }
        BCM_ENET_LINK_DEBUG("e->data:%x %x  %x %x \n", e->data[3], e->data[2], e->data[1], e->data[0]);
    } else {
        BCM_ENET_LINK_DEBUG("Phy Data: %x %x %x %x \n", e->data[3], e->data[2], 
            e->data[1], e->data[0]);
        memcpy((void *)&phy_reg_val, (void *)e->data, 2);
        phy_reg_val = __cpu_to_le16(phy_reg_val);
        BCM_ENET_LINK_DEBUG("phy_reg_val = %x \n", phy_reg_val);
        down(&bcm_ethlock_switch_config);
        if (access_type == MBUS_UBUS) {
            ethsw_phy_wreg(phy_id, reg_offset, &phy_reg_val);
        } else {
            ext_bit = 1;
            ethsw_phy_write_reg(phy_id, reg_offset, &phy_reg_val, ext_bit);
        }
        up(&bcm_ethlock_switch_config);
    }
    return 0;
}

int enet_ioctl_ethsw_regaccess(struct ethswctl_data *e)
{
    int i, access_type = MBUS_UBUS;
    unsigned char data[8] = {0};

#if defined(CONFIG_BCM_ETH_PWRSAVE)
    ethsw_phy_pll_up(0);
#endif

    if (e->offset & IS_PHY_ADDR_FLAG) {
#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
        access_type = MBUS_MDIO;
#endif
        return bcmsw_phy_access(e, access_type);
    }

    if ( ((e->length != 1) && (e->length % 2)) || (e->length > 8)) {
        BCM_ENET_LINK_DEBUG("Invalid length");
        return -EINVAL;
    }

#if defined(STAR_FIGHTER2)
    if (e->unit != 1) /* Assumption : SF2 is unit#1.
                       * Offset checks are not valid below for SF2 because all offsets 
                       * are converted to 4-byte accesses later down the road. */  
#endif
    {
        /* 
            We do have unlignment register access: 0x583 4 bytes and 0x581 2bytes for VLAN configuration.
            So unligned access is needed and driver underneath is working correctly.
            Make exaption for 0x583 and 0x581
        */
        if (!((e->offset == 0x581 && e->length == 2) || (e->offset == 0x583 && e->length == 4)) &&
             (((e->offset % 2) && (e->length == 2 || e->length == 6)) ||
             ((e->offset % 4) && (e->length == 4 || e->length == 8)) ))
        {
            printk("%s: len = %d offset = %d alignment error !! \n", __FUNCTION__, e->length, e->offset);            
            return -EINVAL;
        }
    }

    if (e->type == TYPE_GET) {
        if (e->offset & IS_SPI_ACCESS) {
            ethsw_rreg(0, e->offset&(~IS_SPI_ACCESS), data, e->length);
        }
#if defined(CONFIG_BCM_EXT_SWITCH)			
        else if (e->unit == 1) {
#if !defined(CONFIG_BCM963138) && !defined(CONFIG_BCM963148) /* FIXME - we should always call _wrap ; Thought currently this has no affect for 63138/63148 */
            extsw_rreg(((e->offset & 0xFF00)>>8), (e->offset & 0xFF), data, e->length);
#else
            extsw_rreg_wrap(((e->offset & 0xFF00)>>8), (e->offset & 0xFF), data, e->length);
#endif
        }
#endif /* EXT_SWITCH */		
        else
        {
            ethsw_read_reg(e->offset, data, e->length);
        }
        if (copy_to_user((void*)(&e->data), (void*)&data, e->length)) {
            return -EFAULT;
        }
        BCM_ENET_LINK_DEBUG("Data: ");
        for (i = e->length-1; i >= 0; i--)
            BCM_ENET_LINK_DEBUG("%02x ", e->data[i]);
        BCM_ENET_LINK_DEBUG("\n");
    } else {
        BCM_ENET_LINK_DEBUG("Data: ");
        for (i = e->length-1; i >= 0; i--)
            BCM_ENET_LINK_DEBUG("%02x ", data[i]);
        BCM_ENET_LINK_DEBUG("\n");
#if defined(CONFIG_BCM_EXT_SWITCH)					
        if (e->unit == 1) {
#if !defined(CONFIG_BCM963138) && !defined(CONFIG_BCM963148) /* FIXME - we should always call _wrap ; Thought currently this has no affect for 63138/63148 */
            extsw_wreg(((e->offset & 0xFF00)>>8), (e->offset & 0xFF), e->data, e->length);
#else
            extsw_wreg_wrap(((e->offset & 0xFF00)>>8), (e->offset & 0xFF), e->data, e->length|DATA_TYPE_HOST_ENDIAN);
#endif
        }
        else
#endif /* EXT_SWITCH */		
            ethsw_write_reg(e->offset, e->data, e->length);
    }
    return BCM_E_NONE;
}

int enet_ioctl_ethsw_pmdioaccess(struct net_device *dev, struct ethswctl_data *e) 
{
    int page, reg;
    uint8 data[8] = {0};

    if (e->offset & IS_PHY_ADDR_FLAG) {
        return bcmsw_phy_access(e, MBUS_MDIO);
    } else {
        page = (e->offset >> 8) & 0xFF;
        reg = e->offset & 0xFF;
        if (e->type == TYPE_GET) {
            bcmsw_pmdio_rreg(page, reg, data, e->length);
            if (copy_to_user((void*)(e->data), (void*)data, e->length))
                return -EFAULT;
        } else {
            bcmsw_pmdio_wreg(page, reg, e->data, e->length);
        }
    }
    return 0;
}

void get_ext_switch_access_info(int usConfigType, int *bus_type, int *spi_id) 
{
    switch (usConfigType) {
      case BP_ENET_CONFIG_SPI_SSB_0:
      case BP_ENET_CONFIG_SPI_SSB_1:
      case BP_ENET_CONFIG_SPI_SSB_2:
      case BP_ENET_CONFIG_SPI_SSB_3:
        *bus_type = MBUS_SPI;
        /* No ambiguity here, SSB_0 -> 0 and so on. So hard-coding instead of using SPI_DEV_ID_# defines. 
                  This also avoids the need to have separate SS# defines for the HS SPI in 63xx_map.h files*/
        *spi_id = usConfigType - BP_ENET_CONFIG_SPI_SSB_0;
        break;
      case BP_ENET_CONFIG_HS_SPI_SSB_0:
      case BP_ENET_CONFIG_HS_SPI_SSB_1:
      case BP_ENET_CONFIG_HS_SPI_SSB_2:
      case BP_ENET_CONFIG_HS_SPI_SSB_3:
      case BP_ENET_CONFIG_HS_SPI_SSB_4:
      case BP_ENET_CONFIG_HS_SPI_SSB_5:
      case BP_ENET_CONFIG_HS_SPI_SSB_6:
      case BP_ENET_CONFIG_HS_SPI_SSB_7:
        *bus_type = MBUS_HS_SPI;
        *spi_id =  usConfigType - BP_ENET_CONFIG_HS_SPI_SSB_0;
        break;
      case BP_ENET_CONFIG_MDIO_PSEUDO_PHY:
      case BP_ENET_CONFIG_GPIO_MDIO:
      case BP_ENET_CONFIG_MDIO:
        *bus_type = MBUS_MDIO;
        break;
      case BP_ENET_CONFIG_MMAP:
#if !defined(CONFIG_BCM963138) && !defined(CONFIG_BCM963148) /* FIXME - we should use same MMAP/UBUS for both chips */
        *bus_type = MBUS_UBUS;
#else
        *bus_type = MBUS_MMAP;
#endif
        break;
      default:
        *bus_type = MBUS_NONE;          
        break;
    }
}
int enet_is_mmapped_external_switch(unit)
{
    int bus_type = MBUS_NONE, spi_id = 0, ret = 0;
    ETHERNET_MAC_INFO *EnetInfo = EnetGetEthernetMacInfo();
    ETHERNET_MAC_INFO *info;
    
    if (unit == 0 || unit >  BP_MAX_ENET_MACS) {
        return ret;
    }
    info = &EnetInfo[unit];
    get_ext_switch_access_info(info->usConfigType, &bus_type, &spi_id);
    if (bus_type == MBUS_MMAP) {
        ret = 1;
    }
    return ret;
}

int enet_ioctl_ethsw_info(struct net_device *dev, struct ethswctl_data *e) 
{
    int bus_type = MBUS_NONE, spi_id = 0, spi_cid = 0;
    ETHERNET_MAC_INFO *EnetInfo = EnetGetEthernetMacInfo();
    ETHERNET_MAC_INFO *info;
    unsigned int vend_id = 0, dev_id = 0, rev_id = 0;
    uint16 data[2];
    unsigned int port_map, phy_map;
    int epon_port = -1, i;

    if (e->val > BP_MAX_ENET_MACS) {
        if (copy_to_user((void*)(&e->ret_val), (void*)&bus_type, sizeof(int))) {
            return -EFAULT;
        }
        return BCM_E_NONE;
    }

    info = &EnetInfo[e->val];
    if ((info->ucPhyType == BP_ENET_EXTERNAL_SWITCH) || 
            (info->ucPhyType == BP_ENET_SWITCH_VIA_INTERNAL_PHY)) {

        get_ext_switch_access_info(info->usConfigType, &bus_type, &spi_id);

        switch (info->usConfigType) {
            case BP_ENET_CONFIG_MDIO_PSEUDO_PHY:
            case BP_ENET_CONFIG_GPIO_MDIO:
            case BP_ENET_CONFIG_MDIO:
                /*  MDIO or SPI, reads port phyid registers -- reg 2 & reg 3. */
                ethsw_phy_read_reg(0, 2, (uint16 *)data, 1);
                *data = __be16_to_cpu(*data);
                vend_id = data[0];
                ethsw_phy_read_reg(0, 3, (uint16 *)data, 1);
                *data = __be16_to_cpu(*data);
                dev_id = data[0];
                if (dev_id >= 0xb000) {
                    rev_id = dev_id & 0xF;
                    dev_id &= 0xFFF0;
                }
                break;
            case BP_ENET_CONFIG_SPI_SSB_0:
            case BP_ENET_CONFIG_SPI_SSB_1:
            case BP_ENET_CONFIG_SPI_SSB_2:
            case BP_ENET_CONFIG_SPI_SSB_3:
                bcmsw_spi_rreg(LEG_SPI_BUS_NUM, spi_id, spi_cid, 0x10, 0x04, (uint8 *)data, 2);
                *data = __le16_to_cpu(*data);
                vend_id = data[0];
                bcmsw_spi_rreg(LEG_SPI_BUS_NUM, spi_id, spi_cid, 0x10, 0x06, (uint8 *)data, 2);
                *data = __le16_to_cpu(*data);
                dev_id = data[0];
                if (dev_id >= 0xb000) {
                    rev_id = dev_id & 0xF;
                    dev_id &= 0xFFF0;
                }
                break;
            case BP_ENET_CONFIG_HS_SPI_SSB_0:
            case BP_ENET_CONFIG_HS_SPI_SSB_1:
            case BP_ENET_CONFIG_HS_SPI_SSB_2:
            case BP_ENET_CONFIG_HS_SPI_SSB_3:
            case BP_ENET_CONFIG_HS_SPI_SSB_4:
            case BP_ENET_CONFIG_HS_SPI_SSB_5:
            case BP_ENET_CONFIG_HS_SPI_SSB_6:
            case BP_ENET_CONFIG_HS_SPI_SSB_7:
                bcmsw_spi_rreg(HS_SPI_BUS_NUM, spi_id, spi_cid, 0x10, 0x04, (uint8 *)data, 2);
                *data = __le16_to_cpu(*data);
                vend_id = data[0];
                bcmsw_spi_rreg(HS_SPI_BUS_NUM, spi_id, spi_cid, 0x10, 0x06, (uint8 *)data, 2);
                *data = __le16_to_cpu(*data);
                dev_id = data[0];
                if (dev_id >= 0xb000) {
                    rev_id = dev_id & 0xF;
                    dev_id &= 0xFFF0;
                }
                break;
            case BP_ENET_CONFIG_MMAP:
#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148) /* MMapped external switch on 63138/63148 */
                if (e->val == 1) { /* unit 1 */
                    port_map = info->sw.port_map;
                    for (i = 0; i < BP_MAX_SWITCH_PORTS; i++) {
                        if(((1<<i) & port_map) == 0 || !IsPhyConnected(info->sw.phy_id[i]) || 
                                IsExtPhyId(info->sw.phy_id[i])) {
                            continue;
                        }

                        ethsw_phy_read_reg(info->sw.phy_id[i], 2, (uint16 *)data, 1);
                        *data = __le16_to_cpu(*data);
                        vend_id = data[0];
                        ethsw_phy_read_reg(info->sw.phy_id[i], 3, (uint16 *)data, 1);
                        *data = __le16_to_cpu(*data);
                        dev_id = data[0];
                        if (dev_id >= 0xb000) {
                            rev_id = dev_id & 0xF;
                            dev_id &= 0xFFF0;
                        }
                        break;
                    }

                    if(i == BP_MAX_SWITCH_PORTS) {
                        printk("Error: No integrated PHY defined for device ID in this board design.\n");
                        return -EFAULT;
                    }

                } else
#endif
                {
                    vend_id = 0x6300;
                    dev_id = PERF->RevID >> CHIP_ID_SHIFT;
                }
                break;
            default:
                break;
        }
    }

    if (copy_to_user((void*)(&e->ret_val), (void*)&bus_type, sizeof(int))) {
        return -EFAULT;
    }
    if (copy_to_user((void*)(&e->vendor_id), (void*)&vend_id, sizeof(int))) {
        return -EFAULT;
    }
    if (copy_to_user((void*)(&e->dev_id), (void*)&dev_id, sizeof(int))) {
        return -EFAULT;
    }
    if (copy_to_user((void*)(&e->rev_id), (void*)&rev_id, sizeof(int))) {
        return -EFAULT;
    }
    if (copy_to_user((void*)(&e->spi_id), (void*)&spi_id, sizeof(int))) {
        return -EFAULT;
    }
    if (copy_to_user((void*)(&e->chip_id), (void*)&spi_cid, sizeof(int))) {
        return -EFAULT;
    }
    port_map = info->sw.port_map;
    BCM_ENET_DEBUG("port_map = 0x%x \n", port_map);
    if (copy_to_user((void*)(&e->port_map), (void*)&port_map, sizeof(int))) {
        return -EFAULT;
    }

    phy_map = info->sw.port_map;
    {
        for (i = 0; i < BP_MAX_SWITCH_PORTS; i++) {
            if (vend_id == 0x6300) {
                if (info->sw.phy_id[i] & CONNECTED_TO_EPON_MAC) {
                    epon_port = i;
                }
               if (!IsPhyConnected(info->sw.phy_id[i]) || 
                   (info->sw.phy_id[i] & BCM_EXT_6829))
                   phy_map &= ~(1 << i);
          }
          else {
             if (!IsPhyConnected(info->sw.phy_id[i]))
                 phy_map &= ~(1 << i);
          }
       }
    }
    if (copy_to_user((void*)(&e->phy_portmap), (void*)&phy_map, sizeof(int))) {
        return -EFAULT;
    }

    if (copy_to_user((void*)(&e->epon_port), (void*)&epon_port, sizeof(int))) {
        return -EFAULT;
    }

    return BCM_E_NONE;
}

int enet_ioctl_phy_cfg_get(struct net_device *dev, struct ethswctl_data *e)
{
    ETHERNET_MAC_INFO *EnetInfo = EnetGetEthernetMacInfo();
    ETHERNET_MAC_INFO *info;
    int phycfg, ret_val = -1;

    if (e->val > BP_MAX_ENET_MACS) {
        if (copy_to_user((void*)(&e->ret_val), (void*)&ret_val, sizeof(int))) {
            return -EFAULT;
        }
        return BCM_E_NONE;
    }

    info = &EnetInfo[e->val];
    phycfg = info->sw.phy_id[e->port];

    if (copy_to_user((void*)(&e->phycfg), (void*)&phycfg, sizeof(int))) {
        return -EFAULT;
    }
    return BCM_E_NONE;
}

#if !defined(CONFIG_BCM96838) && !defined(CONFIG_BCM96848)
void ethsw_rreg(int page, int reg, uint8 *data, int len)
{
    unsigned long   addr = (SWITCH_BASE + (page << 8) + reg);
    volatile uint8 *base = (volatile uint8 *)addr;

#if defined(CONFIG_BCM_ETH_PWRSAVE)
    unsigned long flags;
    spin_lock_irqsave(&bcmsw_pll_control_lock, flags);
    ethsw_phy_pll_up(0);
#endif

    if (((len != 1) && (len % 2) != 0) || len > 8)
        panic("ethsw_rreg: wrong length!\n");

    if (len == 1)
    {
        *data = *base;
    }
    else if (len == 2)
    {
        *(uint16 *)(data + 0) = *(uint16 *)(base + 0);
    }
    else if (len == 4)
    {
        if ((reg % 4) == 0)
        {
            *(uint32 *)(data + 0) = *(uint32 *)(base + 0);
        }
        else
        {
            *(uint16 *)(data + 2) = *(uint16 *)(base + 0);
            *(uint16 *)(data + 0) = *(uint16 *)(base + 2);
        }
    }
    else if (len == 6)
    {
        if ((reg % 4) == 0)
        {
            *(uint32 *)(data + 2) = *(uint32 *)(base + 0);
            *(uint16 *)(data + 0) = *(uint16 *)(base + 4);
        }
        else
        {
            *(uint16 *)(data + 4) = *(uint16 *)(base + 0);
            *(uint32 *)(data + 0) = *(uint32 *)(base + 2);
        }
    }
    else if (len == 8)
    {
        *(uint32 *)(data + 4) = *(uint32 *)(base + 0);
        *(uint32 *)(data + 0) = *(uint32 *)(base + 4);
    }
#if defined(CONFIG_BCM_ETH_PWRSAVE)
    spin_unlock_irqrestore(&bcmsw_pll_control_lock, flags);
#endif
}

void ethsw_wreg(int page, int reg, uint8 *data, int len)
{
    unsigned long   addr = (SWITCH_BASE + (page << 8) + reg);
    volatile uint8 *base = (volatile uint8 *)addr;

#if defined(CONFIG_BCM_ETH_PWRSAVE)
    unsigned long flags;
    spin_lock_irqsave(&bcmsw_pll_control_lock, flags);
    ethsw_phy_pll_up(0);
#endif

    if (((len != 1) && (len % 2) != 0) || len > 8)
        panic("ethsw_wreg: wrong length!\n");

    if (len == 1)
    {
        *base = *data;
    }
    else if (len == 2)
    {
        *(uint16 *)base = *(uint16 *)data;
    }
    else if (len == 4)
    {
        if ((reg % 4) == 0)
        {
            if ( (int)data & 3 )
            {
                *(uint32 *)(base + 0) = ((*(uint16 *)(data + 0) << 16) | 
                        (*(uint16 *)(data + 2) <<  0));
            }
            else
            {
                *(uint32 *)(base + 0) = *(uint32 *)(data + 0);
            }
        }
        else
        {
            *(uint16 *)(base + 0) = *(uint16 *)(data + 2);
            *(uint16 *)(base + 2) = *(uint16 *)(data + 0);
        }
    }
    else if (len == 6)
        {
            if (reg % 4 == 0)
            {
                if ( (int)(data + 2) & 3 )
                {
                    *(uint32 *)(base + 0) = ((*(uint16 *)(data + 2) << 16) | 
                                             (*(uint16 *)(data + 4) <<  0));
                }
                else
                {
                   *(uint32 *)(base + 0) = *(uint32 *)(data + 2);
                }
                *(uint16 *)(base + 4) = *(uint16 *)(data + 0);
            }
            else
            {
                *(uint16 *)(base + 0) = *(uint16 *)(data + 4);
                if ( (int)(data + 0) & 3 )
                {
                    *(uint32 *)(base + 2) = ((*(uint16 *)(data + 0) << 16) | 
                                             (*(uint16 *)(data + 2) <<  0));
                }
                else
                {
                   *(uint32 *)(base + 2) = *(uint32 *)(data + 0);
                }
            }
        }
        else if (len == 8)
        {
            if ( (int)(data + 0) & 3 )
            {
                *(uint32 *)(base + 0) = ((*(uint16 *)(data + 4) << 16) | 
                                         (*(uint16 *)(data + 6) <<  0));
                *(uint32 *)(base + 4) = ((*(uint16 *)(data + 0) << 16) | 
                                         (*(uint16 *)(data + 2) <<  0));
            }
            else
            {
                *(uint32 *)(base + 0) = *(uint32 *)(data + 4);
                *(uint32 *)(base + 4) = *(uint32 *)(data + 0);
            }
        }
#if defined(CONFIG_BCM_ETH_PWRSAVE)
    spin_unlock_irqrestore(&bcmsw_pll_control_lock, flags);
#endif
}

void ethsw_read_reg(int addr, uint8 *data, int len)
{
    volatile uint8 *base = (volatile uint8 *)
       (SWITCH_BASE + (addr & SWITCH_ADDR_MASK));

#if defined(CONFIG_BCM_ETH_PWRSAVE)
    unsigned long flags;
    spin_lock_irqsave(&bcmsw_pll_control_lock, flags);
    ethsw_phy_pll_up(0);
#endif

    if (len == 1) {
        *data = *base;
    } else if (len == 2) {
        *(uint16 *)data = swab16(*(uint16 *)base);
    } else if (len == 4) {
        *(uint32 *)data = swab32(*(uint32 *)base);
    } else if (len == 6) {
        if ((addr % 4) == 0) {
            *(uint32 *)data = swab32(*(uint32 *)base);
            *(uint16 *)(data + 4) = swab16(*(uint16 *)(base + 4));
        } else {
            *(uint16 *)data = swab16(*(uint16 *)base);
            *(uint32 *)(data + 2) = swab32(*(uint32 *)(base + 2));
        }
    } else if (len == 8) {
        *(uint32 *)data = swab32(*(uint32 *)base);
        *(uint32 *)(data + 4) = swab32(*(uint32 *)(base + 4));
    }
#if defined(CONFIG_BCM_ETH_PWRSAVE)
    spin_unlock_irqrestore(&bcmsw_pll_control_lock, flags);
#endif
}

void ethsw_write_reg(int addr, uint8 *data, int len)
{
    volatile uint8 *base = (volatile uint8 *)
       (SWITCH_BASE + (addr & SWITCH_ADDR_MASK));
    int val32;

#if defined(CONFIG_BCM_ETH_PWRSAVE)
    unsigned long flags;
    spin_lock_irqsave(&bcmsw_pll_control_lock, flags);
    ethsw_phy_pll_up(0);
#endif

    if (len == 1) {
        *base = *data;
    } else if (len == 2) {
        *(uint16 *)base = swab16(*(uint16 *)data);
    } else if (len == 4) {
        if ( (int)data & 0x3 )
        {
           val32 = ((*(uint16 *)data) << 16) | ((*(uint16 *)(data+2)) << 0);
           *(uint32 *)base = swab32(val32);
        }
        else
        {
           *(uint32 *)base = swab32(*(uint32 *)data);
        }
    } else if (len == 6) {
        if (addr % 4 == 0) {
            if ( (int)data & 0x3 )
            {
               val32 = ((*(uint16 *)data) << 16) | ((*(uint16 *)(data+2)) << 0);
               *(uint32 *)base = swab32(val32);
            }
            else
            {
               *(uint32 *)base = swab32(*(uint32 *)data);
            }
            *(uint16 *)(base + 4) = swab16(*(uint16 *)(data + 4));
        } else {
            *(uint16 *)base = swab16(*(uint16 *)data);
            if ( (int)(data+2) & 0x3 )
            {
               val32 = ((*(uint16 *)(data+2)) << 16) | ((*(uint16 *)(data+4)) << 0);
               *(uint32 *)(base + 2) = swab32(val32);
            }
            else
            {
               *(uint32 *)(base + 2) = swab32(*(uint32 *)(data + 2));
            }
        }
    } else if (len == 8) {
         if ( (int)data & 0x3 )
         {
            val32 = ((*(uint16 *)data) << 16) | ((*(uint16 *)(data+2)) << 0);
            *(uint32 *)base = swab32(val32);
            val32 = ((*(uint16 *)(data+4)) << 16) | ((*(uint16 *)(data+6)) << 0);
            *(uint32 *)(base + 4) = swab32(val32);
         }
         else
         {
            *(uint32 *)base = swab32(*(uint32 *)data);
            *(uint32 *)(base + 4) = swab32(*(uint32 *)(data + 4));
         }
    }
#if defined(CONFIG_BCM_ETH_PWRSAVE)
    spin_unlock_irqrestore(&bcmsw_pll_control_lock, flags);
#endif
}

#endif

