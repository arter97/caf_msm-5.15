/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ECPRI_DMA_DP_H_
#define _ECPRI_DMA_DP_H_

#include "ecpri_dma_i.h"

/** Enums & Defines **/
/* Define for exception endp IRQ timer moderation */
#define ECPRI_DMA_EXCEPTION_ENDP_MODT 0

/* Define for exception endp IRQ counter moderation */
#define ECPRI_DMA_EXCEPTION_ENDP_MODC 1

/* Define for exception endp buffer size */
#define ECPRI_DMA_EXCEPTION_ENDP_BUFF_SIZE 1500

/* Max size of outstanding packet cache */
#define ECPRI_DMA_OUTSTANDING_PKTS_CACHE_MAX_THRESHOLD 2000

/** Structures **/

/**
 * struct ecpri_dma_outstanding_pkt_wrapper - List node for outstanding pkts
 *
 * @comp_pkt: Completed packet pointer to handle once
 *        	  completion is recieved from HW
 * @xfer_done: Confirmation for transfer completed
 * @bytes_xfered: Actucal length transferred
 * @endp: Packet's ENDP context pointer
 * @link: List member
 */
struct ecpri_dma_outstanding_pkt_wrapper {
	struct ecpri_dma_pkt_completion_wrapper comp_pkt;
	bool xfer_done;
	u32 bytes_xfered;
	struct ecpri_dma_endp_context *endp;
	struct list_head link;
};

/** Functions **/

/**
 * ecpri_dma_dp_tx_comp_hdlr() - Tx completion handler
 * @notify:	[in] GSI completion notification
 *
 * Scheduale tasklet to complete Tx completion handling
 */
void ecpri_dma_dp_tx_comp_hdlr(struct gsi_chan_xfer_notify *notify);

/**
 * ecpri_dma_dp_rx_comp_hdlr() - Rx completion handler
 * @notify:	[in] GSI completion notification
 *
 * Notify client on Rx Completion
 */
void ecpri_dma_dp_rx_comp_hdlr(struct gsi_chan_xfer_notify *notify);

/**
 * ecpri_dma_dp_exception_replenish() - Replenish exception ENDP with credits
 * @endp:	[in] Exception endp context
 * @num_to_replenish: [in] Number of credits to replenish
 *
 * Returns:	0 on success, negative on failure
 */
int ecpri_dma_dp_exception_replenish(struct ecpri_dma_endp_context *endp,
				  u32 num_to_replenish);

/**
 * ecpri_dma_dp_exception_endp_notify_completion() -    Notify exception
 * endp on new
 *                                              completion
 * @endp:	[in] Exception endp context
 * @comp_pkt: [in] Not used
 * @num_of_completed: [in] Not used
 */
void ecpri_dma_dp_exception_endp_notify_completion(
	struct ecpri_dma_endp_context *endp,
	struct ecpri_dma_pkt_completion_wrapper **comp_pkt,
	u32 num_of_completed);

/**
 * ecpri_dma_dp_tasklet_exception_notify() - Exception endp completion handler
 *
 * @data:	[in] Exception endp context
 */
void ecpri_dma_dp_tasklet_exception_notify(unsigned long data);

/**
 * ecpri_dma_tasklet_transmit_done() - Tx endp completion handler
 *
 * @data:	[in] Tx endp context
 */
void ecpri_dma_tasklet_transmit_done(unsigned long data);

/**
 * ecpri_dma_tasklet_rx_done() - Rx endp completion handler
 *
 * @data:	[in] Rx endp context
 */
void ecpri_dma_tasklet_rx_done(unsigned long data);

/**
 * ecpri_dma_dp_commit() - Commits any descriptors on the ring by ringing GSI DB
 * @endp:	[in] endp context
 *
 * Returns:	0 on success, negative on failure
 */
int ecpri_dma_dp_commit(struct ecpri_dma_endp_context *endp);

/**
 * ecpri_dma_dp_transmit() - Transmit packets to the DMA HW
 * @endp:	[in] Exception endp context
 * @pkts: [in] Array of packets to transmit
 * @num_of_pkts: [in] Number of packets to transmit
 * @commit: [in] If true driver shall ring the GSI CH DB
 *
 * Returns:	0 on success, negative on failure
 */
int ecpri_dma_dp_transmit(struct ecpri_dma_endp_context *endp,
			  struct ecpri_dma_pkt **pkts, u32 num_of_pkts,
			  bool commit);

/**
 * ecpri_dma_dp_rx_poll() - Poll for completed Rx packets from DMA HW
 * @endp:	[in] Exception endp context
 * @budget: [in] Maximum number of packets to poll
 * @pkts: [out] Array of packets polled from HW
 * @actual_num: [in] Number of packets polled from HW
 *
 * Returns:	0 on success, negative on failure
 */
int ecpri_dma_dp_rx_poll(struct ecpri_dma_endp_context *endp, u32 budget,
			 struct ecpri_dma_pkt_completion_wrapper **pkts,
			 u32 *actual_num);

/**
 * ecpri_dma_dp_gsi_evt_ring_err_cb() - Event ring error handler
 * @notify:	[in] GSI error notification
 *
 * Notify client on event error
 */
void ecpri_dma_dp_gsi_evt_ring_err_cb(
	struct gsi_evt_err_notify *notify);

int ecpri_dma_set_endp_mode(struct ecpri_dma_endp_context *endp,
    enum ecpri_dma_notify_mode mode);
int ecpri_dma_get_endp_mode(struct ecpri_dma_endp_context *endp,
    enum ecpri_dma_notify_mode *mode);

#endif /* _ECPRI_DMA_DP_H_ */