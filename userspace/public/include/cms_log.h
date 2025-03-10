/***********************************************************************
 *
 *  Copyright (c) 2006-2007  Broadcom Corporation
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
:>
 *
 ************************************************************************/

#ifndef __CMS_LOG_H__
#define __CMS_LOG_H__

#if defined __cplusplus
extern "C" {
#endif
#include "cms.h"
#include "cms_eid.h"

/*!\file cms_log.h
 * \brief Public header file for Broadcom DSL CPE Management System Logging API.
 * Applications which need to call Logging API functions must
 * include this file.
 *
 * Here is a general description of how to use this interface.
 *
 */

/*!\enum CmsLogLevel
 * \brief Logging levels.
 * These correspond to LINUX log levels for convenience.  Other OS's
 * will have to map these values to their system.
 */
typedef enum
{
   LOG_LEVEL_ERR    = 3, /**< Message at error level. */
#ifdef AEI_CMS_LOG_DEBUG
   LOG_LEVEL_AEIDEBUG  = 4 , /**< Message at aeidebug level. */
#endif
   LOG_LEVEL_NOTICE = 5, /**< Message at notice level. */
   LOG_LEVEL_DEBUG  = 7  /**< Message at debug level. */
} CmsLogLevel;


/*!\enum CmsLogDestination
 * \brief identifiers for message logging destinations.
 */
typedef enum
{
   LOG_DEST_STDERR  = 1,  /**< Message output to stderr. */
   LOG_DEST_SYSLOG  = 2,  /**< Message output to syslog. */
   LOG_DEST_TELNET  = 3   /**< Message output to telnet clients. */
} CmsLogDestination;


/** Show application name in the log line. */
#define CMSLOG_HDRMASK_APPNAME    0x0001 

/** Show log level in the log line. */
#define CMSLOG_HDRMASK_LEVEL      0x0002 

/** Show timestamp in the log line. */
#define CMSLOG_HDRMASK_TIMESTAMP  0x0004

/** Show location (function name and line number) level in the log line. */
#define CMSLOG_HDRMASK_LOCATION   0x0008 
 

/** Default log level is error messages only. */
#define DEFAULT_LOG_LEVEL        LOG_LEVEL_ERR

/** Default log destination is standard error */
#define DEFAULT_LOG_DESTINATION  LOG_DEST_STDERR

/** Default log header mask */
#define DEFAULT_LOG_HEADER_MASK (CMSLOG_HDRMASK_APPNAME|CMSLOG_HDRMASK_LEVEL|CMSLOG_HDRMASK_TIMESTAMP|CMSLOG_HDRMASK_LOCATION)


/** Maxmimu length of a single log line; messages longer than this are truncated. */
#define MAX_LOG_LINE_LENGTH      512


/** Macros Definition.
 * Applications should use these macros for message logging, instead of
 * calling the cmsLog_log function directly.
 */
#ifdef CMS_LOG0
#define cmsLog_error(args...)
#define cmsLog_notice(args...)
#define cmsLog_debug(args...)
#ifdef AEI_CMS_LOG_DEBUG
#define cmsLog_aeidebug(args...)
#endif
#endif

#ifdef CMS_LOG2
#define cmsLog_error(args...)  log_log((CmsLogLevel)LOG_ERR, __FUNCTION__, __LINE__, args)
#define cmsLog_notice(args...) log_log((CmsLogLevel)LOG_NOTICE, __FUNCTION__, __LINE__, args)
#define cmsLog_debug(args...)
#ifdef AEI_CMS_LOG_DEBUG
#define cmsLog_aeidebug(args...)
#endif
#endif

#ifdef CMS_LOG3
#define cmsLog_error(args...)  log_log((CmsLogLevel)LOG_ERR, __FUNCTION__, __LINE__, args)
#define cmsLog_notice(args...) log_log((CmsLogLevel)LOG_NOTICE, __FUNCTION__, __LINE__, args)
#define cmsLog_debug(args...)  log_log((CmsLogLevel)LOG_DEBUG, __FUNCTION__, __LINE__, args)
#ifdef AEI_CMS_LOG_DEBUG
#define cmsLog_aeidebug(args...)  log_log((CmsLogLevel)LOG_WARNING, __FUNCTION__, __LINE__, args)
#endif
#endif
#if defined(SUPPORT_GPL_4)

#define DEVICE_CRASH_LOG_FILE         "/data/crashlog"

#define DEVICE_HISTORY_LOG_FILE         "/var/devicehistory"

#define DEVICE_HISTORY_FLASH_LOG_FILE           "/data/devicehistory"

void AEI_log_local_info(char *szName, char *szInfo);
#endif

/** Internal message log function; do not call this function directly.
 *
 * NOTE: Applications should NOT call this function directly from code.
 *       Use the macros defined in cms_log.h, i.e.
 *       cmsLog_error, cmsLog_notice, cmsLog_debug.
 *
 * This function performs message logging based on two control
 * variables, "logLevel" and "logDestination".  These two control
 * variables are local to each process.  Each log message has an
 * associated severity level.  If the severity level of the message is
 * numerically lower than or equal to logLevel, the message will be logged to
 * either stderr or syslogd based on logDestination setting.
 * Otherwise, the message will not be logged.
 * 
 * @param level (IN) The message severity level as defined in "syslog.h".
 *                   The levels are, in order of decreasing importance:
 *                   LOG_EMERG (0)- system is unusable 
 *                   LOG_ALERT (1)- action must be taken immediately
 *                   LOG_CRIT  (2)- critical conditions
 *                   LOG_ERR   (3)- error conditions 
 *                   LOG_WARNING(4) - warning conditions 
 *                   LOG_NOTICE(5)- normal, but significant, condition
 *                   LOG_INFO  (6)- informational message 
 *                   LOG_DEBUG (7)- debug-level message
 * @param func (IN) Function name where the log message occured.
 * @param lineNum (IN) Line number where the log message occured.
 * @param pFmt (IN) The message string.
 *
 */
void log_log(CmsLogLevel level, const char *func, UINT32 lineNum, const char *pFmt, ... );


/** Message log initialization.  This is the preferred init call because
 * it is more efficient.
 *
 * This function initializes the message log utility.  The openlog
 * function is called to open a connection to syslogd for the
 * process.  The process name string identified by entityId will
 * be prepended to every message logged by the system logger syslogd.
 *
 * @param eid (IN) The entity ID of the calling process.
 * @param appName (IN) The name of the calling process.
 */
void cmsLog_initWithName(CmsEntityId eid, const char *appName);

/** Message log initialization.  If possible, use cmsLog_initWithName().
 *
 * @param eid (IN) The entity ID of the calling process.
 */
void cmsLog_init(CmsEntityId eid);
  
/** Message log cleanup.
 * This function performs all the necessary cleanup of the message
 * log utility. The closelog function is called to close the
 * descriptor being used to write to the system logger syslogd.
 *
 */
void cmsLog_cleanup(void);
  
/** Set process message logging level.
 * This function sets the logging level of a process.
 *
 * @param level (IN) The logging level to set.
 */
void cmsLog_setLevel(CmsLogLevel level);

/** Get process message logging level.
 * This function gets the logging level of a process.
 *
 * @return The process message logging level.
 */
CmsLogLevel cmsLog_getLevel(void);

/** Set process message logging destination.
 * This function sets the logging destination of a process.
 *
 * @param dest (IN) The process message logging destination.
 */
void cmsLog_setDestination(CmsLogDestination dest);

/** Get process message logging destination.
 * This function gets the logging destination of a process.
 *
 * @return The process message logging destination.
 */
CmsLogDestination cmsLog_getDestination(void);

/** Set process message log header mask which determines which pieces of
 * info are included in each log line.
 *
 * @param mask (IN) Bitmask of CMSLOG_HDRMASK_xxx
 */
void cmsLog_setHeaderMask(UINT32 headerMask);

/** Get process message log header mask.
 *
 * @return The process message log header mask.
 */
UINT32 cmsLog_getHeaderMask(void);


/** indicate first read */
#define BCM_SYSLOG_FIRST_READ           -2

/** indicates error */
#define BCM_SYSLOG_READ_BUFFER_ERROR    -1

/** indicates last line was read */
#define BCM_SYSLOG_READ_BUFFER_END      -3

/** max log buffer length */
#define BCM_SYSLOG_MAX_LINE_SIZE        255


/** Legacy method for reading the system log line by line.
 *
 * @param ptr     (IN) Current line to read.
 * @param buffer (OUT) Line that was read.
 * @return new ptr value for next read.
 */
SINT32 cmsLog_readPartial(SINT32 ptr, char* buffer);

#if defined __cplusplus
};
#endif
#endif /* __CMS_LOG_H__ */
