/*
* <:copyright-BRCM:2013:DUAL/GPL:standard
* 
*    Copyright (c) 2013 Broadcom Corporation
*    All Rights Reserved
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
*/

#ifndef RDPA_CPU_H_
#define RDPA_CPU_H_

#include "rdpa_types.h"
#include "rdpa_cpu_basic.h"
#ifndef CM3390
#include "rdpa_ipsec.h"
#endif

/** \defgroup cpu CPU Interface
 * Functions in this module:
 * - Send packets from the host to any egress port or to virtual "bridge" port
 * - Receive packets via one of host termination (CPU_RX) or Wi-Fi acceleration (WLAN_TX) queues
 * - Connect per-port and/or per-receive queue interrupt handlers
 * - Configure CPU trap reason --> queue mapping
 *
 * Runner to host traffic management parameters can be configured similarly to any other egress queue.
 * Initial port level (CPU, WLAN0, WLAN1) configuration is done using the appropriate port configuration
 * that fixes parameters such as scheduling type, number of queues, etc.\n
 * @{
 */

/** CPU port */
typedef enum
{
    rdpa_cpu_host,      /**< Host RX */
    rdpa_cpu_wlan0,     /**< WLAN 0 TX */
    rdpa_cpu_wlan1,     /* Reserved for future use */

    rdpa_cpu_port__num_of
} rdpa_cpu_port;

/** \defgroup isr Interrupt Control
 * \ingroup cpu
 * Functions in this group allow to
 * - Register / unregister per-queue interrupt handler
 * - Enable / disable / clear per-queue interrupt
 * @{
 */

/** Enable CPU queue interrupt
 * \param[in]   port        port. rdpa_cpu, rdpa_wlan0, rdpa_wlan1
 * \param[in]   queue       Queue index < num_queues in port tm configuration
 */
void rdpa_cpu_int_enable(rdpa_cpu_port port, int queue);

/** Disable CPU queue interrupt
 * \param[in]   port        port. rdpa_cpu, rdpa_wlan0, rdpa_wlan1
 * \param[in]   queue       Queue index < num_queues in port tm configuration
 */
void rdpa_cpu_int_disable(rdpa_cpu_port port, int queue);

/** Clear CPU queue interrupt
 * \param[in]   port        port. rdpa_cpu, rdpa_wlan0, rdpa_wlan1
 * \param[in]   queue       Queue index < num_queues in port tm configuration
 */
void rdpa_cpu_int_clear(rdpa_cpu_port port, int queue);

/** @} end of isr Doxygen group */

/** \defgroup cpu_rx Receive
 * \ingroup cpu
 * Functions in this group allow to
 * - Register/unregister per-port and per-queue interrupt handlers
 * - Register/unregister packet handler
 * - Pull received packets from port (scheduling) or queue
 * @{
 */

/** Max number of CPU interface meters per direction */
#define RDPA_CPU_MAX_DS_METERS   4      /**< Number of meters for packets received from WAN port(s) */
#define RDPA_CPU_MAX_US_METERS   16     /**< Number of meters for packets received from LAN port(s) */

/** CPU meter SR validation constants */
#define RDPA_CPU_METER_MIN_SR       100         /**< Min CPU meter SR value in pps */
#define RDPA_CPU_METER_MAX_SR       40000       /**< Max CPU meter SR value in pps */
#define RDPA_CPU_METER_SR_QUANTA    100         /**< CPU meter SR must be a multiple of quanta */

/** Reason index. Underlying type of cpu_reason_index aggregate */
typedef struct
{
    rdpa_traffic_dir dir;       /**< Traffic direction */
    rdpa_cpu_reason reason;     /**< CPU reason */
#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
    int table_index;            /**< reason table index */
#endif
    int entry_index;            /* Reserved for internal use. */
} rdpa_cpu_reason_index_t;

/** Reason configuration structure.
 * Underlying structure for cpu_reason_cfg aggregate
 */
typedef struct
{
    bdmf_index queue;           /**< reason --> queue mapping */
    bdmf_index meter;           /**< CPU interface meter index < RDPA_CPU_MAX_METERS or BDMF_INDEX_UNDEFINED */
    rdpa_ports meter_ports;     /**< Mask of ports for which the policer is active */
} rdpa_cpu_reason_cfg_t;

/** CPU meter configuration
 * Underlying structure for cpu_meter_cfg aggregate
 */
typedef struct
{
    uint32_t sir;               /**< SIR */
} rdpa_cpu_meter_cfg_t;

/** Receive packet info */
typedef struct
{
    rdpa_cpu_reason reason;     /**< trap reason */
    rdpa_if src_port;           /**< source port */
    uint32_t reason_data;       /**< Reason-specific data.
                                     For CPU port it usually contains src_flow
                                     For Wi-Fi port it contains source SSID index in 16 MSB and destination SSID vector in 16 LSB
                                     It can contain other reason-specific info (such as OAM info, etc.).
                                */
    uint32_t  data;        /**<data pointer or FPM/BPM hw token*/
    uint32_t  size;        /**<data size */
    uint32_t  data_offset;    /**<data offset inside pointer */
    struct {
        uint32_t rx_csum_verified:1;
        uint32_t reserved:31;
    };
    uint32_t  dest_ssid;   /** <when traffic is to be forward to WFD*/
    uint16_t  wl_metadata; /** wlan metadata */
    uint16_t  ptp_index; /**< index of ptp RX entry */
#ifdef CM3390
    uint32_t  cmim_priority;
    uint32_t  cmim_inc_bm;
    uint32_t  cmim_if_bitmask;
#endif
    /* ToDo: we can consider adding a union here */
} rdpa_cpu_rx_info_t;

typedef void (*rdpa_cpu_rxq_rx_isr_cb_t)(long isr_priv);
typedef void (*rdpa_cpu_rxq_rx_cb_t)(long priv, bdmf_sysb sysb, rdpa_cpu_rx_info_t *info);
typedef void (*rdpa_cpu_rxq_dump_data_cb_t)(bdmf_index queue, bdmf_boolean enabled);

/** Host receive configuration.
 * Underlying structure of cpu_rx_cfg aggregate.
 */
typedef struct {
    bdmf_boolean  ic_enable; /**< True - Enable. False - Disable */
#define RDPA_CPU_IC_MAX_PKT_CNT       63
    uint8_t  ic_max_pktcnt;  /**< Interrupt Coalescing max packet count.
                                  MAX value 63
                             */
#define RDPA_CPU_IC_MAX_TIMEOUT_IN_US 1023
    uint16_t ic_timeout_us;  /**< The hardware timer period is configured to
                                  100 us. This means that the timeout period
                                  will round up to the next 100 us.
                                  For example, if the timeout is 201 us,
                                  a timeout will not be identified until 300us.
                                  This means for practical purposes, the
                                  timeout value is 100 us, 200 us, 300 us,
                                  400 us, 500 us, 600 us, 700 us, 800 us,
                                  900 us, 1000 us and 1100 us.
                             */
} rdpa_cpu_rxq_ic_cfg_t;

typedef struct {
    uint32_t received;
    uint32_t queued;
    uint32_t dropped;
} extern_rxq_stat_t;

typedef void (*extern_rxq_stat_callback)(int qid, extern_rxq_stat_t *stat, bdmf_boolean clear);

typedef void (*reason_stat_extern_callback_t)(uint32_t *stat, rdpa_cpu_reason_index_t *rindex);

typedef struct {
    /** ISR callback
     * If set ISR is connected to the appropriate port+queue
     * \param[in]   isr_priv
     */
    rdpa_cpu_rxq_rx_isr_cb_t rx_isr;
    void *ring_head;            /**< Allocated by client ring */
    uint32_t size;              /**< Queue size */
#define RDPA_CPU_QUEUE_MAX_SIZE  1024
#define RDPA_CPU_WLAN_QUEUE_MAX_SIZE  1024
    bdmf_boolean dump;          /**< Dump rx data */
    long isr_priv;              /**< Parameter to be passed to isr callback */
    rdpa_cpu_rxq_ic_cfg_t ic_cfg; /**< Interrupt Coalescing config */
    rdpa_cpu_rxq_dump_data_cb_t rx_dump_data_cb; /**< Dump RX data callback */
    extern_rxq_stat_callback rxq_stat; /**< Optional internal statistics callback */
} rdpa_cpu_rxq_cfg_t;

/** Receive statistics.
 * Underlying structure for cpu_rx_stat aggregate
 */
typedef struct
{
    uint32_t received;      /**< Packets passed to rx callback */
    uint32_t queued;        /**< Packets in the queue */
    uint32_t dropped;       /**< Packets dropped by RDD / FW */
    uint32_t interrupts;    /**< interrupts */
} rdpa_cpu_rx_stat_t;

/** Cpu loopback type
 */
typedef enum
{
    rdpa_cpu_loopback_ipsec,      /**< IPsec offload packet loopback */

    rdpa_cpu_loopback__num_of
} rdpa_cpu_loopback_type;

/** Pull a single received packet from host queue.
 * This function pulls a single packet from the host queue.
 * \param[in]   port            CPU port (CPU, PCI1, PCI2)
 * \param[in]   queue           Queue index < num_queues in rdpa_port_tm_cfg_t or UNASSIGNED
 * \ifnot PON
        * \param[out]  sysb            Packet data (buffer)
 * \endif
 * \param[out]  info            Packet info
 * \return =1:packet pulled ok, ==0:no packet pulled, <0:int error code\n
 */
int rdpa_cpu_packet_get(rdpa_cpu_port port, bdmf_index queue, rdpa_cpu_rx_info_t *info);

/** Pull a single offloaded packet from host queue.
 * This function receives a single packet that was offloaded from CPU.
 * \param[in]   loopback_type   Loopback type
 * \param[in]   queue           Queue index < num_queues in rdpa_port_tm_cfg_t or UNASSIGNED
 * \param[out]  sysb            Packet data (buffer)
 * \param[out]  info            Packet info
 * \return =1:packet pulled ok, ==0:no packet pulled, <0:int error code\n
 */
int rdpa_cpu_loopback_packet_get(rdpa_cpu_loopback_type loopback_type, bdmf_index queue,
                                 bdmf_sysb *sysb, rdpa_cpu_rx_info_t *info);

/** Pull a bulk of received FKB packets from host queue.
 * This function pulls a bulk of packets from the host queue.
 * Buffers will be provided with the FKB envelope.
 * Up to the maximum number of buffers specified via the budget parameter will be returned
 * to the caller.
 *
 * \param[in]   queue_id            CPU RX Queue ID
 * \param[in]   budget              Maximum number of packets to be read
 * \param[in]   rx_pkts             Array of received packets
 * \param[in]   rdpa_cpu_data_p     Opaque CPU handle retrieved via ::rdpa_cpu_data_get() API
 * \return Number of received packets\n
 */
int rdpa_cpu_wfd_bulk_fkb_get(bdmf_index queue_id, unsigned int budget, void **rx_pkts, void *rdpa_cpu_data_p);

/** Pull a bulk of received SKB packets from host queue.
 * This function pulls a bulk of packets from the host queue.
 * Buffers will be provided with tghe SKB envelope.
 * Up to the maximum number of buffers specified via the budget parameter will be returned
 * to the caller.
 * \param[in]   queue_id            CPU RX Queue ID
 * \param[in]   budget              Maximum number of packets to be read
 * \param[in]   rx_pkts             Array of received packets
 * \param[in]   rdpa_cpu_data_p     Opaque CPU handle retrieved via ::rdpa_cpu_data_get() API
 * \return Number of received packets\n
*/
int rdpa_cpu_wfd_bulk_skb_get(bdmf_index queue_id, unsigned int budget, void **rx_pkts, void *rdpa_cpu_data_p);
/** Retrieves Opaque CPU handle
 * \param[in]   rdpa_cpu_type      CPU port identifier ::rdpa_cpu_port
 * \return Opaque CPU handle.
*/
void *rdpa_cpu_data_get(int rdpa_cpu_type);


/** @} end of cpu_rx Doxygen group */

/** \defgroup cpu_tx Transmit
 * \ingroup cpu
 * Packets can be transmitted
 * - via specific port/queue
 * - using FW bridging & classification logic
 *
 * All rdpa_cpu_send_to_xx functions return int error code
 * - If rc==0, buffer ownership is passed to RDPA. The buffer will be released when safe.
 * - If rc!=0, buffer ownership stays with the caller
 * @{
 */

/** CPU tx packet insertion point */
typedef enum
{
    rdpa_cpu_tx_port,           /**< Egress port and priority are specified explicitly. This is the most common mode */
    rdpa_cpu_tx_bridge,         /**< Before bridge forwarding decision, before classification */

    rdpa_cpu_tx_entry__num_of   /**< Number of CPU TX entries */
} rdpa_cpu_tx_method;

/** Extra data that can be passed along with the packet to be transmitted */
typedef struct
{
    rdpa_cpu_tx_method method;  /**< Packet transmit method */
    rdpa_if port;               /**< Destination port for method=port, source port for method=bridge*/

    union {
        /* queue_id in the following substructures must overlap */
        struct {
            uint32_t queue_id;          /**< Egress queue id */
        } lan;

        struct {
            uint32_t queue_id;          /**< Egress queue id. method=port only */
            rdpa_flow flow;             /**< Destination flow for method=port, Source flow for method=bridge,port=wan */
        } wan;

        uint32_t oam_data;              /**< Extra data entry-specific */
    } x;
    uint32_t data;   /**<data pointer or FPM/BPM hw token */
    uint32_t data_offset;    /**<data offset inside pointer */
    uint32_t data_size; /**<size of actuall data */
} rdpa_cpu_tx_info_t;

/** Transmit statistics */
typedef struct
{
    uint32_t tx_ok;                     /**< Successfully passed to RDD for transmission */
    uint32_t tx_invalid_queue;          /**< Discarded because transmit queue is not configured */
    uint32_t tx_no_buf;                 /**< Discarded because couldn't allocate BPM buffer */
    uint32_t tx_too_long;               /**< Discarded packets with length > max MTU size */
    uint32_t tx_rdd_error;              /**< Discarded because RDD returned error */
} rdpa_cpu_tx_stat_t;


/** Send system buffer
 *
 * \param[in]   sysb        System buffer. Released regardless on the function outcome
 * \param[in]   info        Tx info
 * \return 0=OK or int error code\n
 */
int rdpa_cpu_send_sysb(bdmf_sysb sysb, const rdpa_cpu_tx_info_t *info);

int rdpa_cpu_send_wfd_to_bridge(bdmf_sysb sysb, const rdpa_cpu_tx_info_t *info, size_t offset_next);

#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963148)
/** Send system buffer to Ethernet/DSL WAN Interface
 *
 * \param[in]   sysb          System buffer. Released regardless on the function outcome
 * \param[in]   egress_queue  Ethernet Egress Queue
 * \return 0=OK or int error code\n
 */
int rdpa_cpu_tx_port_enet_or_dsl_wan(bdmf_sysb sysb, uint32_t egress_queue, rdpa_flow wan_flow, rdpa_if wan_if);

/** Send system buffer to Ethernet LAN Interface
 *
 * \param[in]   sysb          System buffer. Released regardless on the function outcome
 * \param[in]   egress_queue  Ethernet Egress Queue
 * \param[in]   phys_port     Ethernet LAN physical port
 * \return 0=OK or int error code\n
 */
int rdpa_cpu_tx_port_enet_lan(bdmf_sysb sysb, uint32_t egress_queue, uint32_t phys_port);

/** Send system buffer to Flow Cache Offload
 *
 * \param[in] sysb: System buffer. Released regardless on the function outcome
 * \param[in] cpu_rx_queue: CPU Rx Queue index, in case of Runner Flow miss
 * \param[in] dirty: Indicates whether a packet flush from D$ is required
 * \return 0=OK or int error code\n
 */
int rdpa_cpu_tx_flow_cache_offload(bdmf_sysb sysb, uint32_t cpu_rx_queue, int dirty);

/** Frees the given free index and returns a pointer to the associated
 *  System Buffer. It is up to the caller to process the System Buffer.
 *
 * \param[in] free_index: Runner free index
 * \return: Pointer to the associated system buffer\n
 */
bdmf_sysb rdpa_cpu_return_free_index(uint16_t free_index);

/** Receive a system buffer (FKB type) from an Ethernet Interface
 *
 * \param[in]   queue         CPU RX Queue ID
 * \param[in]   sysb          System buffer
 * \param[in]   src_port      Source RDPA Interface
 * \return 0=OK or int error code\n
 */
int rdpa_cpu_host_packet_get_enet(bdmf_index queue, bdmf_sysb *sysb, rdpa_if *src_port);

/** Free all pending Tx Buffers
 */
void rdpa_cpu_tx_reclaim(void);

/** Send system buffer to IPsec Offload
 *
 * \param[in] sysb: System buffer.
 * \param[in] dir: Indicates whether the packet is upstream or downstream.
 * \param[in] esphdr_offset: ESP header byte offset into the packet.
 * \param[in] sa_index: Entry index of the ddr SA descriptor table.
 * \param[in] cpu_qid:  Runner - HostCPU queue id
 * \return 0=OK or int error code\n
 */
int rdpa_cpu_tx_ipsec_offload(bdmf_sysb sysb, rdpa_traffic_dir dir,
                              uint8_t esphdr_offset, uint8_t sa_index, uint8_t cpu_qid);
#endif

/** Send raw packet
 *
 * \param[in]   data        Packet data
 * \param[in]   length      Packet length
 * \param[in]   info        Info
 * \return 0=OK or int error code\n
 */
int rdpa_cpu_send_raw(void *data, uint32_t length, const rdpa_cpu_tx_info_t *info);

/** Get cpu queue emptiness status
 *
 * \param[in]   port        CPU port (CPU, PCI1, PCI2)
 * \param[in]   queue       Queue index < num_queues in rdpa_port_tm_cfg_t or UNASSIGNED
 * \return 0=no packets are waiting or at least one packet in queue\n
 */
int rdpa_cpu_queue_not_empty(rdpa_cpu_port port, bdmf_index queue);

/** Get the Time Of Day from the FW FIFO, by the ptp index
 * \param[in]   ptp_index ptp_index is an entry of struct rdpa_cpu_rx_info_t and is copied from struct
 *                                      CPU_RX_PARAMS on each received packet. It is only relavent on ptp 1588 packets.
 *                                      By this index we get the corresponding Time Of Day of the received packet.
 * \param[out]  tod_h                   4 most bytes of the TOD
 * \param[out]  tod_l                   4 least bytes of the TOD
 * \param[out]  local_counter_delta     time propogation delta between HW and NP
 * \return 0=OK or int error code\n */
int rdpa_cpu_ptp_1588_get_tod(uint16_t ptp_index, uint32_t *tod_h,
    uint32_t *tod_l, uint16_t *local_counter_delta);
/** Send ptp-1588 system buffer
*
* \param[in]   sysb        System buffer. Released regardless on the function outcome
* \param[in]   info        Tx info
* \return 0=OK or int error code\n
*/
int rdpa_cpu_send_sysb_ptp(bdmf_sysb sysb, const rdpa_cpu_tx_info_t *info);

/** Send system buffer - Special function to send EPON Dying
 *  Gasp:
 *
 * \param[in]   sysb        System buffer. Released regardless on the function outcome
 * \param[in]   info        Tx info
 * \return 0=OK or int error code\n
 *
 *  */
int rdpa_cpu_send_epon_dying_gasp(bdmf_sysb sysb, const rdpa_cpu_tx_info_t *info);

/** Check if the reason is supported by port metering
 * \param[in]   reason        reason to test
 * \return 0=OK or int error code\n
 */
int rdpa_cpu_is_per_port_metering_supported(rdpa_cpu_reason reason);


/** @} end of cpu_tx Doxygen group */
/** @} end of cpu Doxygen group */

rdpa_ports rdpa_ports_all_lan(void);

void rdpa_cpu_rx_dump_packet(char *name, rdpa_cpu_port port,
    bdmf_index queue, rdpa_cpu_rx_info_t *info, uint32_t dst_ssid);

#endif /* RDPA_CPU_H_ */
