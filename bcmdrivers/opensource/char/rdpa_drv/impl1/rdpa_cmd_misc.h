#ifndef __RDPA_CMD_MISC_H_INCLUDED__
#define __RDPA_CMD_MISC_H_INCLUDED__

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

/*
 *******************************************************************************
 * File Name  : rdpa_cmd_misc.h
 *
 * Description: This file contains the miscellaneous API.
 *
 *******************************************************************************
 */
int rdpa_cmd_misc_ioctl(unsigned long arg);
void rdpa_cmd_misc_init(void);

#endif /* __RDPA_CMD_MISC_H_INCLUDED__ */

