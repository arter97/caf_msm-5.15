/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ECPRI_DMA_ECPRI_SS_H_
#define _ECPRI_DMA_ECPRI_SS_H_

#include "ecpri_dma.h"

/* Defines & Enums */

#define ECPRI_DMA_RING_PER_PORT_MAX 4
#define ECPRI_DMA_NUM_PORT_MAX 3

/**
 * enum ecpri_dma_event_type - DMA event types
 * Further events TBD
 */
enum ecpri_dma_event_type {
	ECPRI_DMA_EVENT_TYPE_NONE = 0,
};

enum ecpri_dma_ring_type {
	ECPRI_DMA_RING_TYPE_FH_DEFAULT,
	ECPRI_DMA_RING_TYPE_FH_EXCEPTION,
	ECPRI_DMA_RING_TYPE_NFAPI_A55,
	ECPRI_DMA_RING_TYPE_NFAPI_Q6,
	ECPRI_DMA_RING_TYPE_IPC_A55,
	ECPRI_DMA_RING_TYPE_IPC_Q6,
	ECPRI_DMA_RING_TYPE_C2C_DEFAULT,
	ECPRI_DMA_RING_TYPE_MAX
};

/* Architecture prototypes */

/**
 * struct ecpri_dma_ecpri_ss_dma_event_notify_cb - dma_ecpri_ss client to
 * notify eCPRI SS driver of DMA events
 *
 * @user_data:		 [in] User data provided by eCPRI SS to be used when DMA is
 *                   invokeing the event notify CB
 * @dma_event_type:  [in] Event type identifer
 */
typedef void (*ecpri_dma_ecpri_ss_dma_event_notify_cb)(
	void *user_data, enum ecpri_dma_event_type);

/**
 * struct ecpri_dma_ecpri_ss_log_msg_cb - dma_ecpri_ss client to log a message
 * with eCPRI SS driver logging API
 *
 * @user_data:  [in]  User data provided by eCPRI SS to be used when DMA is
 *              invokeing the Tx comp CB
 * @fmt:        [in]  Message to log
 */
typedef void (*ecpri_dma_ecpri_ss_log_msg_cb)(void *user_data,
    const char *fmt, ...);

/**
 * struct ecpri_dma_ecpri_ss_register_params - DMA readiness parameters
 *
 * @notify_ready:               dma_ecpri_ss client ready callback notifier
 * @userdata_ready:             userdata for dma_ecpri_ss ready cb
 * @dma_event_notify:           dma_ecpri_ss client to notify eCPRI SS driver
 *                              of DMA events
 * @userdata_dma_event_notify:  userdata for dma_ecpri_ss event notify CB
 * @log_msg:                    dma_ecpri_ss client to log a message with eCPRI
 *                              SS driver logging API
 * @userdata_log_msg:           userdata for dma_ecpri_ss logging API
 */
struct ecpri_dma_ecpri_ss_register_params {
	ecpri_dma_ready_cb notify_ready;
	void *userdata_ready;
	ecpri_dma_ecpri_ss_dma_event_notify_cb dma_event_notify;
	void *userdata_dma_event_notify;
	ecpri_dma_ecpri_ss_log_msg_cb log_msg;
	void *userdata_log_msg;
};

/**
 * struct ecpri_dma_ring_params - DMA ring parameters
 *
 * @link_index:       Link index corresponding to the DMA channel, -1
 *                    for exception packets
 * @nfapi_vm_id:      VM ID associated with the Q6 DMA channel for L2
 * @dma_ring_type:    DMA ring type associated with the particular
 *                    DMA channel
 * @src_dma_ring_id:  SRC DMA ring ID associated for the given DMA channel
 * @dest_dma_ring_id: DEST DMA ring ID associated for the given DMA channel
 *
 */
struct ecpri_dma_ring_params {
	u32 link_index;
	enum ecpri_dma_vm_ids nfapi_vm_id;
	enum ecpri_dma_ring_type dma_ring_type;
	u32 src_dma_ring_id;
	u32 dest_dma_ring_id;
};

/**
 * struct ecpri_dma_port_params - DMA port parameters
 *
 * @port_index:         Port index corresponding to the port to be
 *                      configured FH0/FH1/FH2 etc
 * @num_of_rings:       Number of active DMA channels
 * @dma_rings_params:   Values associated with dma ring id
 */
struct ecpri_dma_port_params {
	u32 port_index;
	u32 num_of_rings;
	struct ecpri_dma_ring_params dma_rings_param[ECPRI_DMA_RING_PER_PORT_MAX];
};

/**
 * struct ecpri_dma_topology_params - DMA port topology parameters
 *
 * @num_of_ports:       Number of ports of this topology
 * @port_type:          Type of this port
 * @dma_port_param:     Port configurations per port of this type
 */
struct ecpri_dma_topology_params {
	u32 num_of_ports;
	enum ecpri_dma_endp_stream_dest port_type;
	struct ecpri_dma_port_params dma_port_param[ECPRI_DMA_NUM_PORT_MAX];
};

/**
 * struct ecpri_dma_endp_map - eCPRI DMA endpoint mapping
 *
 * @flv:                    eCPRI HW flavor
 * @num_of_port_types:      Number of traffic types supported in this flavor
 * @port_type:              Type of port - FH/C2C/L2/FH_EXCEPTION
 * @num_ports:              Number of active ports
 * @port_params:            All the DMA params per port type, max dma channel
 *                          type is 5 per port
 */
struct ecpri_dma_endp_mapping {
	enum ecpri_hw_flavor flv;
	u32 num_of_port_types;
	struct ecpri_dma_topology_params
		topology_params[ECPRI_DMA_ENDP_STREAM_DEST_MAX];
};


/**
 * struct ecpri_dma_ecpri_ss_nfapi_cfg - Structure to contain nFAPI
 * configuration passed from nFAPI app to DMA driver
*/
struct ecpri_dma_ecpri_ss_nfapi_cfg {
	/* TBD */
	char dummy;
};

/**
 * struct ecpri_dma_stats - Structure to contain DMA statistics
*/
struct ecpri_dma_stats {
	/* TBD */
	char dummy;
};

/**
 * struct ecpri_dma_ecpri_ss_ops - Structure to contain DMA driver API functions
*/
struct ecpri_dma_ecpri_ss_ops {
	int (*ecpri_dma_ecpri_ss_register)(
		struct ecpri_dma_ecpri_ss_register_params *ready_info,
		bool *is_dma_ready);
	void (*ecpri_dma_ecpri_ss_deregister)(void);
	int (*ecpri_dma_ecpri_ss_get_endp_mapping)(
		struct ecpri_dma_endp_mapping *endp_map);
	int (*ecpri_dma_ecpri_ss_nfapi_config)(
		struct ecpri_dma_ecpri_ss_nfapi_cfg cfg);
	int (*ecpri_dma_ecpri_ss_query_stats)(
		struct ecpri_dma_stats *stats);
	int (*ecpri_dma_ecpri_ss_notify_ssr)(void);
};

/* Architecture API functions */

/**
 * ecpri_dma_ecpri_ss_register() - eCPRI driver registration with DMA driver
 * @ready_info:	[in] ready_info struct with pointers to eCPRI SS CB functions
 * @is_dma_ready: [out] true - if DMA eCPRI client is already ready
 *
 * eCPRI SS driver registration with DMA driver.
 * eCPRI SS to provide DMA with callback functions and DMA to initialize its
 * eCPRI SS context and register the CBs provided by eCPRI SS.
 * In case DMA is ready the ready call back will be invoked and is_dma_ready
 * will return true.
 * In case DMA is not ready is_dma_ready will return false and DMA will use the
 * notify_ready CB to
 * notify eCPRI SS when it becomes ready.
 *
 * Returns:	0 on success, negative on failure
 */
int ecpri_dma_ecpri_ss_register(
    struct ecpri_dma_ecpri_ss_register_params *ready_info,
	bool *is_dma_ready);

/**
 * ecpri_dma_ecpri_ss_deregister() - eCPRI SS driver remove registeration with
 * DMA driver
 */
void ecpri_dma_ecpri_ss_deregister(void);

/**
 * ecpri_dma_ecpri_ss_get_endp_mapping() - DMA driver to provide ENDP mapping
 * to eCPRI SS driver
 * @endp_map:   [out] Pointer to structure with ENDP mapping & port information
 *              according to eCPRI HW flavor
 *
 * Returns:	0 on success, negative on failure
 */
int ecpri_dma_ecpri_ss_get_endp_mapping(
    struct ecpri_dma_endp_mapping *endp_map);

/**
 * ecpri_dma_ecpri_ss_nfapi_config() - eCPRI SS driver to provide nFAPI
 * configurations to DMA driver
 * @cfg:    [out] nFAPI configurations provided by nFAPI user app
 *
 * Returns:	0 on success, negative on failure
 */
int ecpri_dma_ecpri_ss_nfapi_config(struct ecpri_dma_ecpri_ss_nfapi_cfg cfg);

/**
 * ecpri_dma_ecpri_ss_query_stats() - eCPRI SS driver to query DMA driver
 * statistics
 * @stats:		  [out] Structure of the DMA statistics collected
 *
 * Returns:	0 on success, negative on failure
 */
int ecpri_dma_ecpri_ss_query_stats(struct ecpri_dma_stats *stats);

/**
 * ecpri_dma_ecpri_ss_notify_ssr() - TBD
 *
 * Returns:	0 on success, negative on failure
 */
int ecpri_dma_ecpri_ss_notify_ssr(void);

#endif //_ECPRI_DMA_ECPRI_SS_H_
