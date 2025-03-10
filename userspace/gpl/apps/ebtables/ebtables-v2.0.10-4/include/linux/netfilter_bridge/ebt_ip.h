/*
 *  ebt_ip
 *
 *	Authors:
 *	Bart De Schuymer <bart.de.schuymer@pandora.be>
 *
 *  April, 2002
 *
 *  Changes:
 *    added ip-sport and ip-dport
 *    Innominate Security Technologies AG <mhopf@innominate.com>
 *    September, 2002
 */

#ifndef __LINUX_BRIDGE_EBT_IP_H
#define __LINUX_BRIDGE_EBT_IP_H

#include <linux/types.h>

#define EBT_IP_SOURCE 0x01
#define EBT_IP_DEST 0x02
#define EBT_IP_TOS 0x04
#define EBT_IP_PROTO 0x08
#define EBT_IP_SPORT 0x10
#define EBT_IP_DPORT 0x20
#define EBT_IP_DSCP  0x40  /* brcm */
#define EBT_IP_TCPFLAG 0x80  /* aei */
#if defined(SUPPORT_GPL_1)
#define EBT_IP_LEN  0x100
#define EBT_IP_MASK (EBT_IP_SOURCE | EBT_IP_DEST | EBT_IP_TOS | EBT_IP_PROTO |\
 EBT_IP_SPORT | EBT_IP_DPORT | EBT_IP_DSCP | EBT_IP_TCPFLAG | EBT_IP_LEN )
#else
#define EBT_IP_MASK (EBT_IP_SOURCE | EBT_IP_DEST | EBT_IP_TOS | EBT_IP_PROTO |\
 EBT_IP_SPORT | EBT_IP_DPORT | EBT_IP_DSCP | EBT_IP_TCPFLAG )
#endif
#define EBT_IP_MATCH "ip"

/* the same values are used for the invflags */
struct ebt_ip_info {
	__be32 saddr;
	__be32 daddr;
	__be32 smsk;
	__be32 dmsk;
	__u8  tos;
	__u8  dscp; /* brcm */
        __u8  flg_mask;  /* aei */
        __u8  flg_cmp;   /* aei */
	__u8  protocol;
#if defined(SUPPORT_GPL_1)
	__u16 iplen[2];
	__u16 bitmask;
	__u16 invflags;
#else
	__u8  bitmask;
	__u8  invflags;
#endif
	__u16 sport[2];
	__u16 dport[2];
};

#endif
