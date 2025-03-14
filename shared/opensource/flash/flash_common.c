/*
    Copyright 2000-2010 Broadcom Corporation

   <:label-BRCM:2012:DUAL/GPL:standard
   
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

/*!\file flash_common.c
 * \brief This file contains NOR flash related functions used by both
 *        CFE and kernel.
 *
 */

/** Includes. */
#ifdef _CFE_                                                
#include "lib_types.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "bcm_map_part.h"  
#define printk  printf
#else // Linux
#include <linux/kernel.h>
#include "bcm_map_part.h"
#include <linux/string.h>
#endif

#include "bcmtypes.h"
#include "bcm_hwdefs.h"
#include "flash_api.h"
#include "flash_common.h"

// #define DEBUG_FLASH
FLASH_PARTITION_INFO fAuxFsInfo;

void flash_init_info(const NVRAM_DATA *nvRam, FLASH_ADDR_INFO *fInfo)
{
    int i = 0;
    int totalBlks = 0;
    int totalSize = 0;
    int auxFsSize = 0;
    int psiStartAddr = 0;
    int spStartAddr = 0;
    int usedBlkSize = 0;
    int needBytes = 0;
    int sBlk, eBlk;
    int blkSize, sectSize;      /* Size of all blocks in a partition or 0 */
    int totalPartSize;          /* Size in bytes for a partition */
    unsigned long  offset;      /* Offset calulations */
    int flash_type = flash_get_flash_type();

    if ((flash_type == FLASH_IFC_NAND) || (flash_type == FLASH_IFC_SPINAND))
    {
        /* When using NAND flash disable Bcm_flash */
        totalSize = 0;
    }
    else {
        totalBlks = flash_get_numsectors();
        totalSize = flash_get_total_size();
    }

    if (totalSize <= FLASH_LENGTH_BOOT_ROM) {
        /* NAND flash settings. NAND flash does not use raw flash partitioins
         * to store psi, backup psi, scratch pad and syslog.  These data items
         * are stored as files on a JFFS2 file system.
         */
        if ((nvRam->ulPsiSize != -1) && (nvRam->ulPsiSize != 0))
            fInfo->flash_persistent_length = nvRam->ulPsiSize * ONEK;
        else
            fInfo->flash_persistent_length = DEFAULT_PSI_SIZE * ONEK;

        fInfo->flash_persistent_start_blk = 0;
        fInfo->flash_rootfs_start_offset = 0;
        fInfo->flash_scratch_pad_length = SP_MAX_LEN;
        fInfo->flash_syslog_length = nvRam->ulSyslogSize * 1024;

        /* This is a boolean field for NAND flash. */
        fInfo->flash_backup_psi_number_blk = nvRam->backupPsi;
        return;
    }

    /*
    * calculate mandatory primary PSI size and set its fInfo parameters.
    */
    if ((nvRam->ulPsiSize != -1) && (nvRam->ulPsiSize != 0))
        fInfo->flash_persistent_length = nvRam->ulPsiSize * ONEK;
    else
        fInfo->flash_persistent_length = DEFAULT_PSI_SIZE * ONEK;

    psiStartAddr = totalSize - fInfo->flash_persistent_length;
    fInfo->flash_persistent_start_blk = flash_get_blk(FLASH_BASE+psiStartAddr);
    fInfo->flash_persistent_number_blk = totalBlks - fInfo->flash_persistent_start_blk;

    usedBlkSize = 0;
    for (i = fInfo->flash_persistent_start_blk; 
        i < (fInfo->flash_persistent_start_blk + fInfo->flash_persistent_number_blk); i++)
    {
        usedBlkSize += flash_get_sector_size((unsigned short) i);
    }
    fInfo->flash_persistent_blk_offset =  usedBlkSize - fInfo->flash_persistent_length;
    fInfo->flash_meta_start_blk = fInfo->flash_persistent_start_blk;

    /*
    * Next is the optional scratch pad, which is on top of the primary PSI.
    * Old code allowed scratch pad to share a sector with primary PSI.
    * That is retained for backward compatibility.  (Although depending on your
    * NOR flash sector sizes, they may still be in different sectors.)
    * If you have a new deployment, consider forcing separate sectors.
    */
    if ((fInfo->flash_persistent_blk_offset > 0) &&
        (fInfo->flash_persistent_blk_offset < SP_MAX_LEN))
    {
        /*
        * there is some room left in the first persistent sector, but it is
        * not big enough for the scratch pad. (Use this line unconditionally
        * if you want to guarentee scratch pad and primary PSI are on different
        * sectors.)
        */
        spStartAddr = psiStartAddr - fInfo->flash_persistent_blk_offset - SP_MAX_LEN;
    }
    else
    {
        /* either the primary PSI starts on a sector boundary, or there is
        * enough room at the top of the first sector for the scratch pad. */
        spStartAddr = psiStartAddr - SP_MAX_LEN ;
    }

    fInfo->flash_scratch_pad_start_blk = flash_get_blk(FLASH_BASE+spStartAddr);
    fInfo->flash_scratch_pad_length = SP_MAX_LEN;

    if (fInfo->flash_persistent_start_blk == fInfo->flash_scratch_pad_start_blk)  // share blk
    {
#if 0 /* do not used scratch pad unless it's in its own sector */
        printk("Scratch pad is not used for this flash part.\n");  
        fInfo->flash_scratch_pad_length = 0;     // no sp
#else /* allow scratch pad to share a sector with another section such as PSI */
        fInfo->flash_scratch_pad_number_blk = 1;
        fInfo->flash_scratch_pad_blk_offset = fInfo->flash_persistent_blk_offset - fInfo->flash_scratch_pad_length;
#endif
    }
    else // on different blk
    {
        fInfo->flash_scratch_pad_number_blk = fInfo->flash_persistent_start_blk - fInfo->flash_scratch_pad_start_blk;
        // find out the offset in the start_blk
        usedBlkSize = 0;
        for (i = fInfo->flash_scratch_pad_start_blk; 
            i < (fInfo->flash_scratch_pad_start_blk + fInfo->flash_scratch_pad_number_blk); i++)
            usedBlkSize += flash_get_sector_size((unsigned short) i);
        fInfo->flash_scratch_pad_blk_offset =  usedBlkSize - fInfo->flash_scratch_pad_length;
    }

    if (fInfo->flash_scratch_pad_length > 0) {

        fInfo->flash_meta_start_blk = fInfo->flash_scratch_pad_start_blk;
    }

    /*
    * Next is the optional backup PSI.
    */
    if (nvRam->backupPsi == 0x01)
    {
        needBytes = fInfo->flash_persistent_length;
        i = fInfo->flash_meta_start_blk;
        while (needBytes > 0)
        {
            i--;
            needBytes -= flash_get_sector_size((unsigned short) i);
        }
        fInfo->flash_backup_psi_start_blk = i;
        /* calclate how many blocks we actually consumed */
        needBytes = fInfo->flash_persistent_length;
        fInfo->flash_backup_psi_number_blk = 0;
        while (needBytes > 0)
        {
            needBytes -= flash_get_sector_size((unsigned short) i);
            i++;
            fInfo->flash_backup_psi_number_blk++;
        }

        fInfo->flash_meta_start_blk = fInfo->flash_backup_psi_start_blk;
    }
    else
    {
        fInfo->flash_backup_psi_number_blk = 0;
    }

    /*
    * Next is the optional persistent syslog.
    */
    if (nvRam->ulSyslogSize != 0 && nvRam->ulSyslogSize != -1)
    {
        fInfo->flash_syslog_length = nvRam->ulSyslogSize * 1024;
        needBytes = fInfo->flash_syslog_length;
        i = fInfo->flash_meta_start_blk;
        while (needBytes > 0)
        {
            i--;
            needBytes -= flash_get_sector_size((unsigned short) i);
        }
        fInfo->flash_syslog_start_blk = i;
        /* calclate how many blocks we actually consumed */
        needBytes = fInfo->flash_syslog_length;
        fInfo->flash_syslog_number_blk = 0;
        while (needBytes > 0)
        {
            needBytes -= flash_get_sector_size((unsigned short) i);
            i++;
            fInfo->flash_syslog_number_blk++;
        }

        fInfo->flash_meta_start_blk = fInfo->flash_syslog_start_blk;
    }
    else
    {
        fInfo->flash_syslog_length = 0;
        fInfo->flash_syslog_number_blk = 0;
    }

#ifdef AEI_DEFAULT_CFG_CUSTOMER
    /*
    * Next is the AEI customer PSI.
    * active according to macro AEI_DEFAULT_CFG_CUSTOMER, not nvRam info.
    */

    {
        needBytes = fInfo->flash_persistent_length;
        i = fInfo->flash_meta_start_blk;
        while (needBytes > 0)
        {
            i--;
            needBytes -= flash_get_sector_size((unsigned short) i);
        }
        fInfo->flash_customer_psi_start_blk = i;
        /* calclate how many blocks we actually consumed */
        needBytes = fInfo->flash_persistent_length;
        fInfo->flash_customer_psi_number_blk = 0;
        while (needBytes > 0)
        {
            needBytes -= flash_get_sector_size((unsigned short) i);
            i++;
            fInfo->flash_customer_psi_number_blk++;
        }

        fInfo->flash_meta_start_blk = fInfo->flash_customer_psi_start_blk;
    }
#endif
#ifdef AEI_CUSTOMER_REVERT_FIRMWARE_CONFIG
    /*
    * Next is the AEI customer PSI.
    * active according to macro AEI_CUSTOMER_REVERT_FIRMWARE_CONFIG, not nvRam info.
    */

    {
        needBytes = fInfo->flash_persistent_length;
        i = fInfo->flash_meta_start_blk;
        while (needBytes > 0)
        {
            i--;
            needBytes -= flash_get_sector_size((unsigned short) i);
        }
        fInfo->flash_old_image_cfg_start_blk = i;
        /* calclate how many blocks we actually consumed */
        needBytes = fInfo->flash_persistent_length;
        fInfo->flash_old_image_cfg_number_blk = 0;
        while (needBytes > 0)
        {
            needBytes -= flash_get_sector_size((unsigned short) i);
            i++;
            fInfo->flash_old_image_cfg_number_blk++;
        }

        fInfo->flash_meta_start_blk = fInfo->flash_old_image_cfg_start_blk;
    }
#endif

#if 1   //for AUXFS 

   if ( (nvRam->ucAuxFSPercent != 0)
    && (nvRam->ucAuxFSPercent <= MAX_AUXFS_PERCENT))
    {
        /* Estimate the Auxillary File System size */
        auxFsSize = (totalSize * (int)nvRam->ucAuxFSPercent)/100;
        
        /* JFFS_AUXFS offset */
        offset = totalSize - auxFsSize - flash_get_reserved_bytes_at_end(fInfo);

        sBlk = flash_get_blk(offset+FLASH_BASE);
        eBlk = fInfo->flash_meta_start_blk;

        /*
         * Implementation Note:
         * Ensure that we have even number of blocks for
         * ROOTFS+KERNEL to support dual image booting
        */
        if ( ( (sBlk+1) < eBlk)
          && ((((sBlk+1) - flash_get_blk(fInfo->flash_rootfs_start_offset + FLASH_BASE)) % 2) == 0))
        {
            sBlk += 1;  /* Round up */
        }

        blkSize = flash_get_sector_size(sBlk);
        for ( i=sBlk+1, totalPartSize = blkSize; i<eBlk; i++)
        {
            sectSize = flash_get_sector_size(i);
            //if ( blkSize != sectSize ) blkSize = 0;
            if ( blkSize != sectSize ) break;
            totalPartSize += sectSize;
        }
        fAuxFsInfo.sect_size = blkSize;
        auxFsSize = totalPartSize;


        printk("Flash split %d : AuxFS[%d]\n",
               (int)nvRam->ucAuxFSPercent,auxFsSize );
    }
    else
    {
    /*
     * Implementation Note: When there is no AuxFS Partition.
     * Total number of rootfs/kernel blocks will always be ODD.
     * Option: Increase RESERVED section ??? but this would
     * decrease the space available for a single kernel image
     */
        sBlk = eBlk = 0;
        fAuxFsInfo.sect_size = 0;
        auxFsSize = 0;
        printk("Flash not used for Auxillary File System\n");
     }


    /*------------*/
    /* JFFS_AUXFS */
    /*------------*/

    sprintf(fAuxFsInfo.name, "JFFS_AUXFS");
    fAuxFsInfo.start_blk = sBlk;
    fAuxFsInfo.number_blk = eBlk - sBlk;
    fAuxFsInfo.blk_offset = 0;
    fAuxFsInfo.total_len = auxFsSize;
    fAuxFsInfo.mem_base = (unsigned long) flash_get_memptr( sBlk )
                                   +  fAuxFsInfo.blk_offset;
    fAuxFsInfo.mem_length = auxFsSize;
    
#endif

#ifdef DEBUG_FLASH_TOO_MUCH
    /* dump sizes of all sectors in flash */
    for (i=0; i<totalBlks; i++)
        printk("blk %03d: %d\n", i, flash_get_sector_size((unsigned short) i));
#endif

#if defined(DEBUG_FLASH)
    printk("FLASH_BASE                    =0x%08x\n\n", (unsigned int)FLASH_BASE);

    printk("fInfo->flash_rootfs_start_offset =0x%08x\n\n", (unsigned int)fInfo->flash_rootfs_start_offset);

    printk("fInfo->flash_meta_start_blk = %d\n\n", fInfo->flash_meta_start_blk);

    printk("fInfo->flash_syslog_start_blk  = %d\n", fInfo->flash_syslog_start_blk);
    printk("fInfo->flash_syslog_number_blk = %d\n", fInfo->flash_syslog_number_blk);
    printk("fInfo->flash_syslog_length=0x%x\n\n", (unsigned int)fInfo->flash_syslog_length);

#ifdef AEI_DEFAULT_CFG_CUSTOMER
    printk("fInfo->flash_customer_psi_start_blk = %d\n", fInfo->flash_customer_psi_start_blk);
    printk("fInfo->flash_customer_psi_number_blk = %d\n\n", fInfo->flash_customer_psi_number_blk);
#endif
#ifdef AEI_CUSTOMER_REVERT_FIRMWARE_CONFIG
    printk("fInfo->flash_old_image_cfg_start_blk = %d\n", fInfo->flash_old_image_cfg_start_blk);
    printk("fInfo->flash_old_image_cfg_number_blk = %d\n\n", fInfo->flash_old_image_cfg_number_blk);
#endif
    printk("fInfo->flash_backup_psi_start_blk = %d\n", fInfo->flash_backup_psi_start_blk);
    printk("fInfo->flash_backup_psi_number_blk = %d\n\n", fInfo->flash_backup_psi_number_blk);

    printk("sp startAddr = %x\n", (unsigned int) (FLASH_BASE+spStartAddr));
    printk("fInfo->flash_scratch_pad_start_blk = %d\n", fInfo->flash_scratch_pad_start_blk);
    printk("fInfo->flash_scratch_pad_number_blk = %d\n", fInfo->flash_scratch_pad_number_blk);
    printk("fInfo->flash_scratch_pad_length = 0x%x\n", fInfo->flash_scratch_pad_length);
    printk("fInfo->flash_scratch_pad_blk_offset = 0x%x\n\n", (unsigned int)fInfo->flash_scratch_pad_blk_offset);

    printk("psi startAddr = %x\n", (unsigned int) (FLASH_BASE+psiStartAddr));
    printk("fInfo->flash_persistent_start_blk = %d\n", fInfo->flash_persistent_start_blk);
    printk("fInfo->flash_persistent_number_blk = %d\n", fInfo->flash_persistent_number_blk);
    printk("fInfo->flash_persistent_length=0x%x\n", (unsigned int)fInfo->flash_persistent_length);
    printk("fInfo->flash_persistent_blk_offset = 0x%x\n\n", (unsigned int)fInfo->flash_persistent_blk_offset);
    printk("AuxFs.start_blk = %d\n",fAuxFsInfo.start_blk );
    printk("AuxFs,number_blk = %d\n", fAuxFsInfo.number_blk);
    printk("AuxFs.total_len = 0x%x\n",fAuxFsInfo.total_len);
    printk("AuxFs.sect_size = 0x%x\n",fAuxFsInfo.sect_size);
#endif
}


void kerSysFlashPartInfoGet(PFLASH_PARTITION_INFO pflash_partition_info)
{
    memcpy((void*)pflash_partition_info,(const void*)(&fAuxFsInfo), sizeof(FLASH_PARTITION_INFO));
}


unsigned int flash_get_reserved_bytes_auxfs()
{
    int flash_type = flash_get_flash_type();

    if ((flash_type == FLASH_IFC_NAND) || (flash_type == FLASH_IFC_SPINAND))
    {
        return 0;
    }
    else
    {
        return fAuxFsInfo.total_len;
    }
}

unsigned int flash_get_reserved_bytes_at_end(const FLASH_ADDR_INFO *fInfo)
{
    unsigned int reserved=0;
    int i = fInfo->flash_meta_start_blk;
    int totalBlks = flash_get_numsectors();

    while (i < totalBlks)
    {
        reserved += flash_get_sector_size((unsigned short) i);
        i++;
    }

#if defined(DEBUG_FLASH)
    printk("reserved at bottom=%dKB\n", reserved/1024);
#endif

    return reserved;
}

