/***********************************************************************
 *
 *  Copyright (c) 2007  Broadcom Corporation
 *  All Rights Reserved
 *
 * <:label-BRCM:2011:DUAL/GPL:standard
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as published by
 * the Free Software Foundation (the "GPL").
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * 
 * A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
 * writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * :>
 ************************************************************************/


#ifndef __CMS_IMAGE_H__
#define __CMS_IMAGE_H__

#include "cms.h"


/*!\file cms_image.h
 * \brief Header file for public flash image functions.
 *
 * These functions can be included by any application, including GPL applications.
 * The image can be software images (cfe+fs+kernel, fs+kernel) or config files.
 *
 */


/** Minimum size for image uploads.
 *  Config file uploads can be less than 2K.
 */
#define  CMS_IMAGE_MIN_LEN    2048


/** For NAND ftpd upload, image size is not known and just set to 32MG as the max len
 * for memory allocation now and can be changed if needed later on.
 */
#define CMS_IMAGE_MAX_ALLOC_LEN    32 * 1024 * 1024

/** For NAND ftpd upload, the min required image size for image uploads for malloc is set to 4 MG
 * for memory allocation now and can be changed if needed. 
 */
#define CMS_IMAGE_REQUIRED_LEN        4 * 1024 * 1024

/*!\enum CmsImageFormat
 * \brief cms image formats that we recognize.
 *
 * Starting with release 4.14L.01, the CMS_IMAGE_FORMAT_PART1,
 * CMS_IMAGE_FORMAT_PART2, and CMS_IMAGE_FORMAT_NO_REBOOT enums
 * have been removed from cmsImageFormat enum since these are not formats.
 * Rather, they are flags which control the behavior of the system
 * when writing an image.  See cmsImage_writeValidatedImageEx.
 */
typedef enum
{
    CMS_IMAGE_FORMAT_INVALID=0,   /**< invalid or unrecognized format */
    CMS_IMAGE_FORMAT_BROADCOM=1,  /**< broadcom image (with our header) */
    CMS_IMAGE_FORMAT_FLASH=2,     /**< raw flash image */
    CMS_IMAGE_FORMAT_XML_CFG=3,   /**< CMS XML config file */
    CMS_IMAGE_FORMAT_MODSW_LINUXPFP=4, /**< modular software Linux Primary Firmware Patch */
    CMS_IMAGE_FORMAT_MODSW_LINUXEE_DU=5, /**< modular software Linux Execution Environment Deployment Unit */
#if defined(NOT_USED_6)
    CMS_IMAGE_FORMAT_CORRUPTED=6, /**< corrupted format*/
    CMS_IMAGE_FORMAT_WRONG=7,
#endif
	CMS_IMAGE_FORMAT_PART1=0x10,   /**< Specify to write to partition1 */
    CMS_IMAGE_FORMAT_PART2=0x20,   /**< Specify to write to partition2 */
    CMS_IMAGE_FORMAT_NO_REBOOT=0x80 /**< Do not reboot after flashing  */
} CmsImageFormat;
#ifdef NOT_USED_4
/** Options for write to active partition */
#define CMS_IMAGE_WR_OPT_ACTIVE        0x1
#define CMS_IMAGE_WR_OPT_ACTIVE_STR    "1"
/** Options for write to non-active partition */
#define CMS_IMAGE_WR_OPT_INACTIVE      0x2
#define CMS_IMAGE_WR_OPT_INACTIVE_STR  "2"
#endif

/** Options for cmsImg_writeValidatedImageEx: write to partition1 */
#define CMS_IMAGE_WR_OPT_PART1        0x10

/** Options for cmsImg_writeValidatedImageEx: write to partition2 */
#define CMS_IMAGE_WR_OPT_PART2        0x20

/** Options for cmsImg_writeValidatedImageEx: Do not reboot after writing
  * image to non-active partition */
#define CMS_IMAGE_WR_OPT_NO_REBOOT    0x80


/*!\structure CmsImageTransferStats
 * \Keep track of upload/download image statistics
 */
typedef struct
{
   UBOOL8 isDownload;         /**< indicate whether transfer was a download(true) or upload(false). */
   UINT8  fileType;           /**< CmsImageFormat */
   UINT32 fileSize;           /**< size of file/image */
   UINT32 faultCode;          /**< fault code as defined in TR69 Amendment 5 */
   char faultStr[BUFLEN_64];  /**< fault string as defined in TR69 Amendment 5; but reduce to 64 byte instead of 256 */
   UINT32 startTime;          /**< record at start of transfer number of seconds since Jan 1 1970. */
   UINT32 completeTime;       /**< record at end of transfer number of seconds since Jan 1 1970. */
} CmsImageTransferStats;

/** Validate the given image and return the image format of the given image.
 * 
 * The image can be a broadcom image, whole image, or config file.
 * This function will also verify that the image will fit in flash.
 *
 * @param imagePtr (IN) image to be parsed.
 * @param imageLen (IN) Length of image in bytes.
 * @param msgHandle (IN) message handle from the caller.
 * 
 * @return CmsImageFormat enum.
 */
CmsImageFormat cmsImg_validateImage(const char *imagePtr, UINT32 imageLen, void *msgHandle);


/** Write the image to flash.
 *  The image can be a config file, a broadcom image, or flash image.
 *  This function will validate the image before writing to flash.
 *
 * @param imagePtr (IN) image to be written.  Surprisingly, for cfe+kernel+fs
 *                      flash writes, the image is modified, so we cannot
 *                      declare this parameter as const.
 * @param imageLen (IN) Length of image in bytes.
 * @param msgHandle (IN) message handle from the caller.
 * 
 * @return CmsRet enum.
 */
CmsRet cmsImg_writeImage(char *imagePtr, UINT32 imageLen, void *msgHandle);


/** Just calls cmsImg_writeValidatedImageEx with opts=0 */
CmsRet cmsImg_writeValidatedImage(char *imagePtr, UINT32 imageLen,
                                  CmsImageFormat format, void *msgHandle);


/** Write a validated image in known format to flash.
 *  The image can be a config file, a broadcom image, or flash image.
 *
 * @param imagePtr (IN) image to be written.  Surprisingly, for cfe+kernel+fs
 *                      flash writes, the image is modified, so we cannot
 *                      declare this parameter as const.
 * @param imageLen (IN) Length of image in bytes.
 * @param format   (IN) CmsImageFormat of the image.
 * @param msgHandle (IN) message handle from the caller.
 * @param opts      (IN) Additional options, see CMS_IMAGE_WR_OPT_XXX above.
 * 
 * @return CmsRet enum.
 */
CmsRet cmsImg_writeValidatedImageEx(char *imagePtr, UINT32 imageLen,
                                    CmsImageFormat format, void *msgHandle,
                                    UINT32 opts);




#define FLASH_INFO_FLAG_NOR    0x0001
#define FLASH_INFO_FLAG_NAND   0x0002

/** Get info about the flash.  Currently, just returns the type of flash,
 *  but in the future, could return more useful info.
 *
 *  @flags (OUT)  Bit field containing info about the flash type.
 *
 *  @return CmsRet enum.
 */
CmsRet cmsImg_getFlashInfo(UINT32 *flags);


/** Return the number of bytes available in the flash for storing an image.
 * 
 * Note this function does not return the size of the entire flash.
 * Rather, it returns the number of bytes available in
 * the flash for storing the image.
 * 
 * @return the number of bytes available in the flash for storing an image.
 */
UINT32 cmsImg_getImageFlashSize(void);


/** Return the number of bytes available in the flash for storing a config file.
 *
 * Note, if compressed config file is enabled, this function will return
 * a number that is larger than the actual number of bytes available in
 * the flash for storing the config file
 *
 * @return the number of bytes available for storing a config file.
 */
UINT32 cmsImg_getConfigFlashSize(void);


/** Return the actual number of bytes available in the flash for storing a config file.
 *
 * This returns the actual number of bytes available in the flash for
 * storing a config file.  External callers should not use this function.
 *
 * @return the number of bytes available for storing a config file.
 */
UINT32 cmsImg_getRealConfigFlashSize(void);


/** Return TRUE if the flash has space for a backup config file.
 *
 * @return TRUE if the flash has space for a backup config file.
 */
UBOOL8 cmsImg_isBackupConfigFlashAvailable(void);


/** Return the size, in bytes, of the broadcom image tag header.
 * 
 * @return the number of bytes in the broadcom image tag header that is present
 *         at the beginning of broadcom images.
 */
UINT32 cmsImg_getBroadcomImageTagSize(void);


/** Safety margin to use when determining if an image will fit into the flash.
 *  Used by cmsImg_willFitInFlash().
 *
 * Not clear if this is really needed.
 */
#define  CMS_IMAGE_OVERHEAD          256

/** Return true if the image will fit in the flash.
 * 
 * Compares the given image length with the image flash size minus a
 * CMS_IMAGE_OVERHEAD margin.  Do we really need to have this margin?
 * Currently, only httpd uses this code.
 * 
 * @param imageLen (IN) Length of image in bytes.
 * 
 * @return True if the image will fit in the flash.
 */
UBOOL8 cmsImg_willFitInFlash(UINT32 imageLen);


/** Minimum length needed to detect if a buffer could be a config file.
 */
#define CMS_CONFIG_FILE_DETECTION_LENGTH 64

/** Check the first CMS_CONFIG_FILE_DETECTION_LENGTH bytes of the given
 *  buffer to see if this is a config file.
 * 
 * @param buf (IN) buffer containing the image to be analyzed.
 * 
 * @return TRUE if the image is likely to be a config file.
 */
UBOOL8 cmsImg_isConfigFileLikely(const char *buf);


/** Send a message to smd informing it that we are starting a big image download.
 * 
 * This function only needs to be called if we are doing a broadcom or
 * flash image download.  Not needed for config file downloads.
 * 
 * @param msgHandle (IN) the message handle to use to send the message.
 * @param connIfName (IN) the connection interface name saved at connection time.
 */
void cmsImg_sendLoadStartingMsg(void *msgHandle, const char *connIfName); 


/** Send a message to smd informing it that we are done with a big image download.
 * 
 * smd will now restart any applications that it killed while the image download
 * was in progress.  This only needs to be called if the image download failed
 * and we want the modem to continue working as normal.  If the image download
 * succeeded, then we will probably reboot the modem as the last step.  No
 * need to restart apps again.
 * 
 * @param msgHandle (IN) the message handle to use to send the message.
 */
void cmsImg_sendLoadDoneMsg(void *msgHandle); 


/** Send a reboot request msg to smd.
 * 
 * @param msgHandle (IN) the message handle to use to send the message.
 */
void cmsUtil_sendRequestRebootMsg(void *msgHandle);


/** Given a socket file descriptor, return the interface that the socket is bound to.
 *
 * This function is useful during image upload when we need to know if the 
 * image data will be coming from the LAN side or WAN side, and if WAN side, 
 * which interface on the WAN it will be coming from (so we can bring down the
 * other WAN interfaces to save memory.)
 *
 * @param socketfd    (IN) the socket of the connection
 * @param connIfName (OUT) on successful return, this buffer will contain the
 *                         linux interface name that is associated with the socket.
 *                         Caller must supply a buffer of at least CMS_IFNAME_LENGTH bytes.
 *
 * @return CmsRet enum.
 */
CmsRet cmsImg_saveIfNameFromSocket(SINT32 socketfd, char *connIfName);

/** If the image has bcmTag, return TRUE, and fill the broadcom tagged image length
 *
 * @param imageBuf (IN) ImageBuf to be checked
 * @param imageLen (OUT) Length of image in bytes if it is a bcmTagged image.
 * 
 * @return TRUE/FALSE.
 */
UBOOL8 cmsImg_isBcmTaggedImage(const char *imageBuf, UINT32 *imageSize);

#if defined(AEI_SIGNED_FIRMWARE)
CmsRet AEI_verifyImageTobeAEIFormat(const char *imagePtr, UINT32 imageLen, void *msgHandle, CmsImageFormat format);
#endif

#ifdef AEI_DEFAULT_CFG_CUSTOMER
extern UBOOL8 glbCustomerSpecifyDefConfigFile;
#endif

#ifdef DMP_DEVICE2_HOMEPLUG_1
/** Update the PowerLine configuration State variable for image
 *  upgrades or for restoring factory defaults.
 *  
 * This function is only useful to PLC code and should not be 
 * used by other applications. 
 *
 * @param state      (IN) a char pointer to the state value to 
 *                   be written
 * @return CmsRet enum.
 */
CmsRet cmsImg_setPLCconfigState(const char *state);
#endif

#ifdef SUPPORT_TR69C_AUTONOMOUS_TRANSFER_COMPLETE
/** Store the image statistics into scratch pad.
 *  After flashing image or config file successfully,
 *  system reboots.  So, it is neccessary to store
 *  statistic of the upload/download process.
 *
 * @param stats (IN) a pointer this structure CmsImageTransferStats
 *                   to be stored
 * @return CmsRet enum.
 */
CmsRet cmsImg_storeImageTransferStats(const CmsImageTransferStats *stats);

/** Send a autonomous transfer complete msg to smd.
 * 
 * @param msgHandle (IN) the message handle to use to send the message.
 * @param pStats (IN) the pointer to CmsImageTransferStats
 */
void cmsImg_sendAutonomousTransferCompleteMsg(void *msgHandle, const CmsImageTransferStats *pStats);

/** Get the file transfer statistics from scratch pad.
 *
 * @param stats (OUT) a pointer this structure CmsImageTransferStats
 *                   to be stored
 * @return CmsRet enum.
 */
CmsRet cmsImg_getImageTransferStats(CmsImageTransferStats *pStats);

/** Clear the file transfer statistics from scratch pad stored with CMS_IMAGE_TRANSFER_STATS_PSP_KEY
 *
 * @return CmsRet enum.
 */
CmsRet cmsImg_clearImageTransferStats(void);

#define CMS_IMAGE_TRANSFER_STATS_PSP_KEY  "xferStats"
#endif /* SUPPORT_TR69C_AUTONOMOUS_TRANSFER_COMPLETE */

#if defined(SUPPORT_GPL_1)
#define CMS_IMAGE_FLASH_STATUS_FAILED   0
#define CMS_IMAGE_FLASH_STATUS_SUCCESS   1
#define CMS_IMAGE_FLASH_STATUS_UPGRADING   2
#define CMS_IMAGE_UPDATE_STATE_PSP_KEY  "imageState"
CmsRet cmsImg_storeUpdateState(int *pState);
CmsRet cmsImg_getUpdateState(int *pState);
#endif

#endif /*__CMS_IMAGE_H__*/

