/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ECPRI_DMA_H_
#define _ECPRI_DMA_H_

#include <linux/types.h>
/* Defines & Enums */

/* Max ENDP ID for eCPRI DMA */
#define ECPRI_DMA_ENDP_NUM_MAX 74

/**
 * max size of the name of the resource
 */
#define ECPRI_DMA_RESOURCE_NAME_MAX 32

/**
 * enum ecpri_hw_ver - eCPRI hardware version type
 * @ECPRI_HW_NONE: eCPRI hardware version not defined
 * @ECPRI_HW_V1_0: eCPRI hardware version 1.0
 */
enum ecpri_hw_ver {
	ECPRI_HW_NONE = 0,
	ECPRI_HW_V1_0 = 1,
	ECPRI_HW_MAX = 2,
};

/**
 * enum ecpri_hw_flavor - eCPRI hardware flavor type
 * @ECPRI_HW_FLAVOR_NONE: eCPRI hardware flavor not defined
 * @ECPRI_HW_FLAVOR_RU: eCPRI hardware flavor RU
 * @ECPRI_HW_FLAVOR_DU_PCIE: eCPRI hardware flavor DU - PCIe
 * @ECPRI_HW_FLAVOR_DU_L2: eCPRI hardware flavor DU - L2
 */
enum ecpri_hw_flavor {
	ECPRI_HW_FLAVOR_NONE = 0,
	ECPRI_HW_FLAVOR_RU = 1,
	ECPRI_HW_FLAVOR_DU_PCIE = 2,
	ECPRI_HW_FLAVOR_DU_L2 = 3,
	ECPRI_HW_FLAVOR_MAX,
};

/**
 * enum ecpri_dma_endp_dir - DMA ENDP direction
 */
enum ecpri_dma_endp_dir {
	ECPRI_DMA_ENDP_DIR_SRC = 0,
	ECPRI_DMA_ENDP_DIR_DEST = 1,
};

/**
 * enum ecpri_dma_endp_stream_mode - DMA ENDP stream mode
 */
enum ecpri_dma_endp_stream_mode {
	ECPRI_DMA_ENDP_STREAM_MODE_M2M = 0,
	ECPRI_DMA_ENDP_STREAM_MODE_S2M = 1,
	ECPRI_DMA_ENDP_STREAM_MODE_M2S = 2,
};

/**
 * enum ecpri_dma_endp_stream_dest - DMA ENDP stream destination
 */
enum ecpri_dma_endp_stream_dest {
	ECPRI_DMA_ENDP_STREAM_DEST_FH = 0,
	ECPRI_DMA_ENDP_STREAM_DEST_C2C = 1,
	ECPRI_DMA_ENDP_STREAM_DEST_L2 = 2,
	ECPRI_DMA_ENDP_STREAM_DEST_FH_EXCEPTION = 3,
	ECPRI_DMA_ENDP_STREAM_DEST_MAX = 4,
};

/**
 * enum ecpri_dma_ees - DMA Execution environments
 */
enum ecpri_dma_ees {
	ECPRI_DMA_EE_AP = 0,
	ECPRI_DMA_EE_Q6 = 1,
	ECPRI_DMA_EE_VM0 = 2,
	ECPRI_DMA_EE_VM1 = 3,
	ECPRI_DMA_EE_VM2 = 4,
	ECPRI_DMA_EE_VM3 = 5,
	ECPRI_DMA_EE_PF = 6,
};

enum ecpri_dma_vm_ids {
	ECPRI_DMA_VM_IDS_VM0 = 0,
	ECPRI_DMA_VM_IDS_VM1 = 1,
	ECPRI_DMA_VM_IDS_VM2 = 2,
	ECPRI_DMA_VM_IDS_VM3 = 3,
	ECPRI_DMA_VM_IDS_MAX = 4,
	ECPRI_DMA_VM_IDS_NONE = ECPRI_DMA_VM_IDS_MAX,
};

/**
 * enum ecpri_dma_status_code - DMA trafer status code
 */
enum ecpri_dma_status_code {
	/* General*/
	ECPRI_DMA_STATUS_CODE_NORMAL = 0,
	ECPRI_DMA_STATUS_CODE_PTP = 1,
	ECPRI_DMA_STATUS_CODE_FLUSHED = 2,
	ECPRI_DMA_STATUS_CODE_P7_EXCEPTION = 3,
	/* nFAPI Error code */
	ECPRI_DMA_STATUS_CODE_TIMEOUT_ERROR = 16,
	ECPRI_DMA_STATUS_CODE_PKT_LENGTH_ERROR = 17,
	ECPRI_DMA_STATUS_CODE_SDU_LENGTH_MISMATCH_ERROR = 18,
	ECPRI_DMA_STATUS_CODE_TIMESTAMP_ERROR = 19,
	ECPRI_DMA_STATUS_CODE_MSG_LENGTH_ERROR = 20,
	/* Ethernet Error code */
	ECPRI_DMA_STATUS_CODE_MAC_FCS_ERROR = 32,
	ECPRI_DMA_STATUS_CODE_SECURITY_ERROR = 33,
	ECPRI_DMA_STATUS_CODE_IPV4_CHECKSUM_ERROR = 34,
	ECPRI_DMA_STATUS_CODE_UDP_CHECKSUM_ERROR = 35,
	/* Unsupported Ethernet Error code */
	ECPRI_DMA_STATUS_CODE_QUDP_TRAPPED_PKT = 40,
	ECPRI_DMA_STATUS_CODE_MAC_DST_MULTICAST = 41,
	ECPRI_DMA_STATUS_CODE_NON_LCL_MAC_DST = 42,
	ECPRI_DMA_STATUS_CODE_VLAN_FLTR_MISS = 43,
	ECPRI_DMA_STATUS_CODE_IP_FLTR_MISS = 44,
	ECPRI_DMA_STATUS_CODE_IPV4_FRAG = 45,
	ECPRI_DMA_STATUS_CODE_IPV4_OPTIONS = 46,
	ECPRI_DMA_STATUS_CODE_UNSUP_ETHER_TYPE = 47,
	ECPRI_DMA_STATUS_CODE_UNSUP_IP_PROT = 48,
	ECPRI_DMA_STATUS_CODE_UDP_FLRT_MISS = 49,
};

/**
 * enum ecpri_dma_completion_code - DMA completion code
 */
enum ecpri_dma_completion_code {
	ECPRI_DMA_COMPLETION_CODE_EOT = 0,
	ECPRI_DMA_COMPLETION_CODE_OVERFLOW,
};

/**
 * enum ecpri_dma_notify_mode - Rx endpoints notification mode
 */
enum ecpri_dma_notify_mode {
	ECPRI_DMA_NOTIFY_MODE_IRQ = 0,
	ECPRI_DMA_NOTIFY_MODE_POLL,
	ECPRI_DMA_NOTIFY_MODE_MAX,
};

/* Architecture prototypes */

/**
 * struct ecpri_dma_moderation_config - DMA Tx endpoint parameters
 *
 * @moderation_counter_threshold: Threshold for moderation counter.
 * @moderation_timer_threshold: Threshold for moderation timer in msec.
 */
struct ecpri_dma_moderation_config {
	u32 moderation_counter_threshold;
	u32 moderation_timer_threshold;
};

/**
 * struct ecpri_dma_mem_buffer - DMA memory buffer
 * @virt_base: virtual base
 * @phys_base: physical base address
 * @size: size of memory buffer
 */
struct ecpri_dma_mem_buffer {
	void *virt_base;
	dma_addr_t phys_base;
	u32 size;
};

/**
 * struct ecpri_dma_pkt - DMA packet
 * @buffs: array of buffers with the packet payload
 * @num_of_buffers: array size
 * @user_data: per packet user data
 */
struct ecpri_dma_pkt {
	struct ecpri_dma_mem_buffer **buffs;
	u32 num_of_buffers;
	void *user_data;
};

/**
 * struct ecpri_dma_pkt_completion_wrapper - completed DMA packet
 * @pkt: pointer to the completed packet provided by ETH driver
 * @status_code: DMA status code returned from DMA upon completion
 * @comp_code: Trasnfer completion code.
 */
struct ecpri_dma_pkt_completion_wrapper {
	struct ecpri_dma_pkt *pkt;
	enum ecpri_dma_status_code status_code;
	enum ecpri_dma_completion_code comp_code;
};

typedef void (*ecpri_dma_ready_cb)(void *user_data);

/* Architecture API functions */

#endif //_ECPRI_DMA_H_
