/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ECPRI_DMA_MHI_CLIENT_H_
#define _ECPRI_DMA_MHI_CLIENT_H_

#include "gsi.h"
#include "ecpri_dma_i.h"
#include "ecpri_dma_dp.h"
#include "ecpri_dma_utils.h"
#include "ecpri_dma.h"
#include <linux/mhi_dma.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>


#define ECPRI_DMA_MHI_DMA_MAX_PKT_SZ              0x1FFFFF
#define ECPRI_DMA_MHI_VIRTUAL_FUNCTION_NUM        (4)
#define ECPRI_DMA_MHI_MAX_HW_CHANNELS		      (4)
#define ECPRI_DMA_MHI_INVALID_CH_ID			      (-1)
#define ECPRI_DMA_MHI_INVALID_ENDP_ID			  (-1)
#define ECPRI_DMA_MHI_MEMCPY_RLEN			      (50)
#define ECPRI_DMA_MHI_POLLING_MIN_SLEEP_RX	      (1010)
#define ECPRI_DMA_MHI_POLLING_MAX_SLEEP_RX	      (1050)

#define ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM \
			(ECPRI_DMA_MHI_VIRTUAL_FUNCTION_NUM + 1)
#define ECPRI_DMA_MHI_MAX_VIRTUAL_FUNCTIONS_ID \
			(ECPRI_DMA_MHI_VIRTUAL_FUNCTION_NUM - 1)

/* Bit 40 mask */
#define ECPRI_DMA_MHI_ASSERT_BIT_MASK        0x10000000000
#define ECPRI_DMA_MHI_SLEEP_CLK_RATE_KHZ    (32)
#define ECPRI_DMA_MHI_MIN_VALID_HDL         (1)
#define ECPRI_DMA_MHI_PF_ID (ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM - 1)

/* bit #40 in address should be asserted for MHI transfers over pcie */
#define ECPRI_DMA_MHI_HOST_ADDR(addr) ((addr) | BIT_ULL(40))

/* bit #40 in address should be asserted for MHI transfers over pcie */
#define ECPRI_DMA_MHI_HOST_ADDR_COND(addr, ctx) \
	((ctx->is_over_pcie)?(ECPRI_DMA_MHI_HOST_ADDR(addr)):(addr))

#define ECPRI_DMA_VIRTUL_VFID_CFG(vf_num) (0x4 | vf_num)

enum ecpri_dma_mhi_dma_dir {
	ECPRI_DMA_MHI_DMA_TO_HOST,
	ECPRI_DMA_MHI_DMA_FROM_HOST,
};

enum ecpri_dma_mhi_state {
	ECPRI_DMA_MHI_STATE_INVALID = 0,
	ECPRI_DMA_MHI_STATE_INITIALIZED = 1,
	ECPRI_DMA_MHI_STATE_READY = 2,
	ECPRI_DMA_MHI_STATE_STARTED = 3,
	ECPRI_DMA_MHI_STATE_RUNNING = 4,
	ECPRI_DMA_MHI_STATE_MAX = 5
};

static char* ecpri_dma_mhi_state_str[] = {
	__stringify(ECPRI_DMA_MHI_STATE_INVALID),
	__stringify(ECPRI_DMA_MHI_STATE_INITIALIZED),
	__stringify(ECPRI_DMA_MHI_STATE_READY),
	__stringify(ECPRI_DMA_MHI_STATE_STARTED),
	__stringify(ECPRI_DMA_MHI_STATE_RUNNING),
};

#define ECPRI_DMA_MHI_STATE_STR(state) \
	(((state) >= 0 && (state) < ECPRI_DMA_MHI_STATE_MAX) ? \
		ecpri_dma_mhi_state_str[(state)] : \
		"INVALID")

struct ecpri_dma_mhi_host_ch_ctx {
	u8	chstate;	/*0-7*/
	u8	brsmode : 2;	/*8-9*/
	u8	pollcfg : 6;	/*10-15*/
	u16	reserved;	/*16-31*/
	u32	chtype;		/*channel type (inbound/outbound)*/
	u32	erindex;	/*event ring index*/
	u64	rbase;		/*ring base address in the host addr spc*/
	u64	rlen;		/*ring length in bytes*/
	u64	rp;		/*read pointer in the host system addr spc*/
	u64	wp;		/*write pointer in the host system addr spc*/
} __packed;

struct ecpri_dma_mhi_host_ev_ctx {
	u16	intmodc;
	u16	intmodt;/* Interrupt moderation timer (in microseconds) */
	u32	ertype;
	u32	msivec;	/* MSI vector for interrupt (MSI data)*/
	u64	rbase;	/* ring base address in host address space*/
	u64	rlen;	/* ring length in bytes*/
	u64	rp;	/* read pointer in the host system address space*/
	u64	wp;	/* write pointer in the host system address space*/
} __packed;

/**
 * enum ecpri_dma_hw_mhi_channel_states - MHI channel state machine
 *
 * Values are according to MHI specification
 * @ECPRI_DMA_HW_MHI_CHANNEL_STATE_DISABLE: Channel is disabled and not processed
 * by the host or device.
 * @ECPRI_DMA_HW_MHI_CHANNEL_STATE_ENABLE: A channel is enabled after being
 *	initialized and configured by host, including its channel context and
 *	associated transfer ring. While this state, the channel is not active
 *	and the device does not process transfer.
 * @ECPRI_DMA_HW_MHI_CHANNEL_STATE_RUN: The device processes transfers and doorbell
 *	for channels.
 * @ECPRI_DMA_HW_MHI_CHANNEL_STATE_SUSPEND: Used to halt operations on the channel.
 *	The device does not process transfers for the channel in this state.
 *	This state is typically used to synchronize the transition to low power
 *	modes.
 * @ECPRI_DMA_HW_MHI_CHANNEL_STATE_STOP: Used to halt operations on the channel.
 *	The device does not process transfers for the channel in this state.
 * @ECPRI_DMA_HW_MHI_CHANNEL_STATE_ERROR: The device detected an error in an element
 *	from the transfer ring associated with the channel.
 * @ECPRI_DMA_HW_MHI_CHANNEL_STATE_INVALID: Invalid state. Shall not be in use in
 *	operational scenario.
 */
enum ecpri_dma_hw_mhi_channel_states {
	ECPRI_DMA_HW_MHI_CHANNEL_STATE_DISABLE = 0,
	ECPRI_DMA_HW_MHI_CHANNEL_STATE_ENABLE = 1,
	ECPRI_DMA_HW_MHI_CHANNEL_STATE_RUN = 2,
	ECPRI_DMA_HW_MHI_CHANNEL_STATE_SUSPEND = 3,
	ECPRI_DMA_HW_MHI_CHANNEL_STATE_STOP = 4,
	ECPRI_DMA_HW_MHI_CHANNEL_STATE_ERROR = 5,
	ECPRI_DMA_HW_MHI_CHANNEL_STATE_INVALID = 0xFF
};

/**
 * struct ecpri_dma_mhi_channel_ctx - MHI Channel context
 * @channel_id: channel ID
 * @event_id: event ID
 * @valid: entry is valid
 * @state: channel state
 * @int_modt: GSI event ring interrupt moderation time
 *		cycles base interrupt moderation (32KHz clock)
 * @int_modc: GSI event ring interrupt moderation packet counter
 * @buff_size: actual buff size of rx_pkt
 * @rlen: ring length
 * @ev_rlen: event ring length
 * @ch_ctx_host: host channel context
 * @ev_ctx_host: host event context
 * @channel_context_addr: Channel context address
 * @ev_context_addr: Event context address
 * @endp_ctx: DMA end point context
 * @is_over_pcie: indicates channel should transact over PCIe � Configurable by
 *					SW.
 * @disable_overflow_event: when set overflow events are not generated on this
 *							ch.
 * @msi_config: MSI (Message Signaled Interrupts) parameters
 *
 */
struct ecpri_dma_mhi_channel_ctx {
	u8 channel_id;
	u8 event_id;
	bool valid;
	enum ecpri_dma_hw_mhi_channel_states state;
	u32 int_modt;
	u32 int_modc;
	u32 buff_size;
	u32 rlen;
	u32 ev_rlen;
	struct ecpri_dma_mhi_host_ch_ctx ch_ctx_host;
	struct ecpri_dma_mhi_host_ev_ctx ev_ctx_host;
	u64 channel_context_addr;
	u64 ev_context_addr;
	struct ecpri_dma_endp_context* endp_ctx;
	bool is_over_pcie;
	bool disable_overflow_event;
	struct mhi_dma_msi_info* msi_config;
};


/**
 * struct ecpri_dma_mhi_client_context - MHI Client context
 *
 * @notify_cb: client callback
 * @user_data: client private data to be provided in client callback
 * @msi_config: MSI (Message Signaled Interrupts) parameters
 * @mmio_addr: MHI MMIO physical address
 * @first_ch: First channel ID for hardware accelerated channels.
 * @first_ev: First event ring ID for hardware accelerated channels.
 * @is_over_pcie: should assert bit 40 in order to access host space.
 *	if PCIe iATU is configured then not need to assert bit40
 * @lock: lock to synchronize access
 * @idr: MHI client IDR instace for uniqe ID generation
 * @idr_lock: Spinlock to protect IDR usage
 * @host_ctrl_addr: Base address of MHI control data structures
 * @host_data_addr: Base address of MHI data buffers
 * @channel_context_array_addr: channel context array address in host address space
 * @event_context_array_addr: event context array address in host address space
 * @gsi_device_scratch: EE scratch config parameters
 *
 */
struct ecpri_dma_mhi_client_context {
	mhi_dma_cb notify_cb;
	void* user_data;
	struct mhi_dma_msi_info msi_config;
	u32 mmio_addr;
	u32 first_ch;
	u32 first_ev;
	bool is_over_pcie;
	enum mhi_dma_mstate mhi_mstate;
	enum ecpri_dma_mhi_state state;
	spinlock_t lock;
	struct workqueue_struct* wq;
	struct ecpri_dma_mhi_channel_ctx channels[ECPRI_DMA_MHI_MAX_HW_CHANNELS];
	struct idr idr;
	spinlock_t idr_lock;
	u32 host_ctrl_addr;
	u32 host_data_addr;
	u64 channel_context_array_addr;
	u64 event_context_array_addr;
	struct gsi_device_scratch dev_scratch;
};

/**
* struct ecpri_dma_mhi_xfer_wrapper - ECPRI DMA transfer descr wrapper
*
* @user_cb: ECPRI DMA client provided completion callback
* @user1: cookie1 for above callback
* @user_data: userdata for DMA cb
* @link: linked to the wrappers list on the proper(sync/async) cons pipe
*
* This struct can wrap both sync and async memcpy transfers descriptors.
*/
struct ecpri_dma_mhi_xfer_wrapper {
	void (*user_cb)(void *user1);
	void *user_data;
	struct list_head link;
	struct mhi_dma_function_params function;
};

/**
 * struct ecpri_dma_mhi_memcpy_context - MHI Memcpy context
 *
 * @lock: lock for synchronisation of the access
 * @sync_lock: lock for synchronisation in sync_memcpy
 * @async_lock: lock for synchronisation in async_memcpy
 * @ref_count: reference counter
 * @sync_pending: number of pending sync memcopy operations
 * @async_pending: number of pending async memcopy operations
 * @sync_total: total number of sync memcpy (statistics)
 * @async_total: total number of async memcpy (statistics)
 * @sync_dest_endp: destination sync endpoint context
 * @sync_src_endp: source sync endpoint context
 * @async_dest_endp: destination async endpoint context
 * @async_src_endp: source async endpoint context
 * @async_wq: awync WQ to call mhi CBs
 * @done: no pending works - ecpri dma can be destroyed
 * @destroy_pending: destroy ecpri_dma after handling all pending memcpy
 * @cbs_list: list of user callbacks and data
 * @loop_counter: Loop counter for sync_memcopy (statistics)
 * @xfer_wrapper_cache: cache of ecpri_dma_mhi_xfer_wrapper structs
 */
struct ecpri_dma_mhi_memcpy_context {
	spinlock_t lock;
	spinlock_t sync_lock;
	spinlock_t async_lock;
	atomic_t ref_count;
	atomic_t sync_pending;
	atomic_t async_pending;
	atomic_t sync_total;
	atomic_t async_total;
	struct ecpri_dma_endp_context* sync_dest_endp;
	struct ecpri_dma_endp_context* sync_src_endp;
	struct ecpri_dma_endp_context* async_dest_endp;
	struct ecpri_dma_endp_context* async_src_endp;
	struct workqueue_struct *async_wq;
	struct completion done;
	bool destroy_pending;
	struct list_head cbs_list;
	u32 loop_counter;
	struct kmem_cache* xfer_wrapper_cache;
};

struct ecpri_dma_mhi_async_wq_work_type {
	struct work_struct work;
	struct ecpri_dma_mhi_xfer_wrapper *xfer_desc;
};

struct ecpri_dma_mhi_wq_work_type {
	struct work_struct work;
	struct ecpri_dma_mhi_client_context* ctx;
};


/**
 * struct ecpri_dma_mhi_function_endp_data - Holds ENDPOINT IDs per VM/PF
 *
 * Note: VM has no ASYNC endpoints, only SYNC. ASYNC entries holds -1.
 *       PF - all SYNC/ASYNC endpoints are available.
 *
 * @sync_src_id:   SYNC SRC ENDP ID
 * @sync_dest_id:  SYNC DEST ENDP ID
 * @async_src_id:  ASYNC SRC ENDP ID
 * @async_dest_id: ASYNC DEST ENDP ID
 *
 */
struct ecpri_dma_mhi_function_endp_data {
	int sync_src_id;
	int sync_dest_id;
	int async_src_id;
	int async_dest_id;
};

#endif /* _ECPRI_DMA_MHI_CLIENT_H_ */
