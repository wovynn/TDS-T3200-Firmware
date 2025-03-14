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

#ifndef __CMS_TMS_H__
#define __CMS_TMS_H__


/*!\file cms_tms.h
 * \brief Header file for the CMS Timestamp API.
 *  This is in the cms_util library.
 *
 */

#include "cms.h"

#if defined(SUPPORT_GPL_1)
#include "aei_cms_tms.h"
#endif

/** Number of nanoseconds in 1 second. */
#define NSECS_IN_SEC 1000000000

/** Number of nanoseconds in 1 milli-second. */
#define NSECS_IN_MSEC 1000000

/** Number of nanoseconds in 1 micro-second. */
#define NSECS_IN_USEC 1000

/** Number of micro-seconds in 1 second. */
#define USECS_IN_SEC  1000000

/** Number of micro-seconds in a milli-second. */
#define USECS_IN_MSEC 1000

/** Number of milliseconds in 1 second */
#define MSECS_IN_SEC  1000

/** Number of seconds in a minute */
#define SECS_IN_MINUTE   60

/** Number of seconds in an hour */
#define SECS_IN_HOUR     (SECS_IN_MINUTE * 60)

/** Number of seconds in a day */
#define SECS_IN_DAY      (SECS_IN_HOUR * 24)

/** Base format of the XSI datetime string */
#define UNKNOWN_DATETIME_STRING "0001-01-01T00:00:00Z"


/** OS independent timestamp structure.
 */
typedef struct
{
   UINT32 sec;   /**< Number of seconds since some arbitrary point. */
   UINT32 nsec;  /**< Number of nanoseconds since some arbitrary point. */
} CmsTimestamp;



/** Get the current timestamp.
 * 
 * The timestamps are supposed to be unaffected by changes in the system
 * time.  Even though timestamps have a nanosecond field, the resolution
 * of the timetime is probably around 2.5 to 10ms.
 *
 *@param tms (IN) This structure is filled with the current timestamp.
 */   
void cmsTms_get(CmsTimestamp *tms);


/** Calculate the difference between two timestamps.
 *
 * This function correctly handles rollover of the timestamps.
 * That is, if newTms={0.0} and oldTms={4294967295,999999000} (1000ns before
 * rollover) then deltaTms={0,1000}.
 * Therefore,  caller must be careful to specify the two timestamps for
 * this function in the correct order.  Otherwise, the function will
 * think that a rollover has occured and return a surpringly large delta.
 *
 * In the more common, non-rollover case, example would be
 * newTms={10,500000000} and oldTms={3,100000000} so
 * deltaTms={7,400000000}.
 *
 * @param newTms    (IN) The timestamp that was obtained more recently.
 * @param oldTms    (IN) The timestamp that was obtained in the past.
 * @param deltaTms (OUT) The difference between newTms and oldTms.
 */   
void cmsTms_delta(const CmsTimestamp *newTms,
                  const CmsTimestamp *oldTms,
                  CmsTimestamp *deltaTms);


/** Calculate the difference between two timestamps and return their
 *  difference in milliseconds.
 *
 * This function uses cmsTms_delta(), so the comments for that function
 * applies to this function as well.
 *
 * @param newTms    (IN) The timestamp that was obtained more recently.
 * @param oldTms    (IN) The timestamp that was obtained in the past.
 * @return delta in milli-seconds.  If the delta in milliseconds is too
 *         large for an UINT32, then MAX_UINT32 is returned.
 */   
UINT32 cmsTms_deltaInMilliSeconds(const CmsTimestamp *newTms,
                                  const CmsTimestamp *oldTms);


/** Add the specified number of milliseconds to the given timestamp.
 *
 * @param tms    (IN/OUT) The timestamp to operate on.
 * @param ms     (IN)     The number of millisconds to add to tms.
 */
void cmsTms_addMilliSeconds(CmsTimestamp *tms, UINT32 ms);


/** Format the specified time in XSI format for TR69.
 *
 * @param t      (IN) Number of seconds since Jan 1 1970.  If 0, then this
 *                    function will use the current time.
 * @param buf   (OUT) Buffer to hold the formatted time.
 * @param bufLen (IN) Length of buffer.
 *
 * @return CmsRet enum, specifically, if the buffer is not big enough,
 *                      CMSRET_RESOURCE_EXCEEDED will be returned.
 */
CmsRet cmsTms_getXSIDateTime(UINT32 t, char *buf, UINT32 bufLen);


/** Format the specified time in XSI format for TR143.
 *
 * @param t      (IN) Number of seconds since Jan 1 1970.  If 0, then this
 *                    function will use the current time. the micro-seconds
 *                    use the current system time.
 * @param buf   (OUT) Buffer to hold the formatted time.
 * @param bufLen (IN) Length of buffer.
 *
 * @return CmsRet enum, specifically, if the buffer is not big enough,
 *                      CMSRET_RESOURCE_EXCEEDED will be returned.
 */
CmsRet cmsTms_getXSIDateTimeMicroseconds(UINT32 t, char *buf, UINT32 bufLen);


/** Format the specified number of seconds into days, hours, minutes, seconds.
 *
 * @param t      (IN) Number of seconds
 * @param buf   (OUT) Buffer to hold the formatted time.
 * @param bufLen (IN) Length of buffer.
 *
 * @return CmsRet enum, specifically, if the buffer is not big enough,
 *                      CMSRET_RESOURCE_EXCEEDED will be returned.
 */
CmsRet cmsTms_getDaysHoursMinutesSeconds(UINT32 t, char *buf, UINT32 bufLen);


/** Get just the seconds portion of the current timestamp.
 */
UINT32 cmsTms_getSeconds(void);


#endif  /* __CMS_TMS_H__ */
