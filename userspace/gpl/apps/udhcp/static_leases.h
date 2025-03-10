/* static_leases.h */
#ifndef _STATIC_LEASES_H
#define _STATIC_LEASES_H

#include "dhcpd.h"

/* Config file will pass static lease info to this function which will add it
 * to a data structure that can be searched later */
int addStaticLease(struct static_lease **lease_struct, uint8_t *mac, uint32_t *ip
#if defined(AEI_VDSL_CUSTOMER_ADVANCED_DMZ)
	,uint32_t gw,uint32_t subnet,uint32_t dns1,uint32_t dns2
#endif
);

/* Check to see if a mac has an associated static lease */
uint32_t getIpByMac(struct static_lease *lease_struct, void *arg);

/* Check to see if an ip is reserved as a static ip */
uint32_t reservedIp(struct static_lease *lease_struct, uint32_t ip);
#if defined(SUPPORT_GPL_1)
struct static_lease * AEI_getLeaseByMac(struct static_lease *lease_struct, void *arg);
struct static_lease * AEI_getLeaseByIp(struct static_lease *lease_struct, uint32_t ip);
#endif

#ifdef UDHCP_DEBUG
/* Print out static leases just to check what's going on */
void printStaticLeases(struct static_lease **lease_struct);
#endif

#endif



