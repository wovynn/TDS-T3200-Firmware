#ifndef _SERVERPACKET_H
#define _SERVERPACKET_H


int sendOffer(struct dhcpMessage *oldpacket);
int sendNAK(struct dhcpMessage *oldpacket);
int sendACK(struct dhcpMessage *oldpacket, u_int32_t yiaddr);
int send_inform(struct dhcpMessage *oldpacket);

#if defined(AEI_VDSL_DHCP_LEASE) || defined(SUPPORT_GPL_2)
UBOOL8 is_stb(const char *vid);
#if defined(AEI_VDSL_WP)
UBOOL8 is_WP(const char *vid);
#endif
#endif
#if defined (AEI_VDSL_STB_NO_FIREWALL)
void do_mark(u_int32_t ip __attribute__((unused)), UBOOL8 mark __attribute__((unused)));
void do_all_mark(UBOOL8 mark);
struct ip_mac_list* find_stb_by_chaddr(u_int8_t *mac);
void del_stb_from_list(u_int8_t *mac, u_int32_t ip);
void add_stb_to_list(u_int8_t *mac, u_int32_t ip);
#endif
#if defined(NOT_USED_6) || defined(NOT_USED_23)
int is_in_range(struct dhcpMessage *oldpacket, u_int32_t req_align);
#endif

#endif
