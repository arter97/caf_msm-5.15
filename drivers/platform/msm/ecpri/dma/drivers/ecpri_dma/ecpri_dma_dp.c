/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/dmapool.h>
#include <linux/list.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/ratelimit.h>
#include <linux/msm_gsi.h>
#include "ecpri_dma_i.h"
#include "dmahal.h"
#include "gsi.h"
#include "ecpri_dma_dp.h"

#define ECPRI_DMA_DP_EXCEPTION_BUDGET (5)
#define ECPRI_DMA_DP_EXCEPTION_BUFF_SIZE (1500)

void ecpri_dma_dp_gsi_evt_ring_err_cb(struct gsi_evt_err_notify *notify)
{
	switch (notify->evt_id) {
	case GSI_EVT_OUT_OF_BUFFERS_ERR:
		DMAERR("Got GSI_EVT_OUT_OF_BUFFERS_ERR\n");
		break;
	case GSI_EVT_OUT_OF_RESOURCES_ERR:
		DMAERR("Got GSI_EVT_OUT_OF_RESOURCES_ERR\n");
		break;
	case GSI_EVT_UNSUPPORTED_INTER_EE_OP_ERR:
		DMAERR("Got GSI_EVT_UNSUPPORTED_INTER_EE_OP_ERR\n");
		break;
	case GSI_EVT_EVT_RING_EMPTY_ERR:
		DMAERR("Got GSI_EVT_EVT_RING_EMPTY_ERR\n");
		break;
	default:
		DMAERR("Unexpected err evt: %d\n", notify->evt_id);
	}
}

int ecpri_dma_dp_exception_replenish(struct ecpri_dma_endp_context *endp,
				  u32 num_to_replenish)
{
	int ret = 0;
	int i = 0;
	struct ecpri_dma_pkt **pkts;
	unsigned long flags;

	if (!endp || !endp->valid || !endp->gsi_ep_cfg->is_exception) {
		DMAERR("Exception ENDP isn't valid");
		return -EINVAL;
	}

	pkts = kzalloc(sizeof(*pkts) * num_to_replenish, GFP_NOWAIT);
	if (!pkts) {
		DMAERR("failed to alloc packets array \n");
		return -ENOMEM;
	}

	memset(pkts, 0, sizeof(*pkts) * num_to_replenish);

	spin_lock_irqsave(&endp->spinlock, flags);
	for (i = 0; i < num_to_replenish; i++) {
		pkts[i] = kmem_cache_zalloc(
			endp->available_exception_pkts_cache, GFP_NOWAIT);
		if (!pkts[i]) {
			DMAERR("failed to alloc packet\n");
			ret = -ENOMEM;
			goto fail_alloc;
		}
		memset(pkts[i], 0, sizeof(*pkts[i]));

		pkts[i]->buffs = kzalloc(sizeof(*pkts[i]->buffs), GFP_NOWAIT);
		if (!pkts[i]->buffs) {
			DMAERR("failed to alloc dma buff wrapper\n");
			kmem_cache_free(endp->available_exception_pkts_cache,
				pkts[i]);
			ret = -ENOMEM;
			goto fail_alloc;
		}

		pkts[i]->buffs[0] = kmem_cache_zalloc(
			endp->available_exception_buffs_cache, GFP_NOWAIT);
		if (!pkts[i]->buffs[0]) {
			DMAERR("failed to alloc dma buff wrapper\n");
			kmem_cache_free(endp->available_exception_pkts_cache,
					pkts[i]);
			kfree(pkts[i]->buffs);
			ret = -ENOMEM;
			goto fail_alloc;
		}
		memset(pkts[i]->buffs[0], 0, sizeof(*(pkts[i]->buffs[0])));
		pkts[i]->num_of_buffers = 1;

		pkts[i]->buffs[0]->virt_base =
			dma_alloc_coherent(ecpri_dma_ctx->pdev,
				ECPRI_DMA_DP_EXCEPTION_BUFF_SIZE,
			&(pkts[i]->buffs[0]->phys_base), GFP_NOWAIT);
		if (!pkts[i]->buffs[0]->virt_base) {
			DMAERR("failed to alloc buffer\n");
			kmem_cache_free(endp->available_exception_buffs_cache,
					pkts[i]->buffs[0]);
			kfree(pkts[i]->buffs);
			kmem_cache_free(endp->available_exception_pkts_cache,
					pkts[i]);
			ret = -ENOMEM;
			goto fail_alloc;
		}
		memset(pkts[i]->buffs[0]->virt_base, 0,
			ECPRI_DMA_DP_EXCEPTION_BUFF_SIZE);

		pkts[i]->buffs[0]->size = ECPRI_DMA_DP_EXCEPTION_BUFF_SIZE;
	}

	/* transmit takes the spinlock, so need to free it here */
	spin_unlock_irqrestore(&endp->spinlock, flags);

	ret = ecpri_dma_dp_transmit(endp, pkts, num_to_replenish, true);
	if (ret) {
		DMAERR("failed to replenish exception endp\n");
		for (--i; i >= 0; i--) {
			dma_free_coherent(ecpri_dma_ctx->pdev, pkts[i]->buffs[0]->size,
				pkts[i]->buffs[0]->virt_base, pkts[i]->buffs[0]->phys_base);
			kmem_cache_free(endp->available_exception_buffs_cache,
				pkts[i]->buffs[0]);
			kfree(pkts[i]->buffs);
			kmem_cache_free(endp->available_exception_pkts_cache, pkts[i]);
		}
		kfree(pkts);
		return -EFAULT;
	}

	kfree(pkts);
	return 0;

fail_alloc:
	/* An allocation failed, free memory backwards */
	for (--i; i >= 0; i--) {
		kfree(pkts[i]->buffs[0]->virt_base);
		kmem_cache_free(endp->available_exception_buffs_cache,
				pkts[i]->buffs);
		kfree(pkts[i]->buffs);
		kmem_cache_free(endp->available_exception_pkts_cache, pkts[i]);
	}

	spin_unlock_irqrestore(&endp->spinlock, flags);

	kfree(pkts);
	return ret;
}

static void ecpri_dma_dump_packet(char *buf, int len)
{
	int payload_len = len - 14;
	int num_dumps;
	int i;
	int index = 0;

	DMADBG("dumping packet of length: %d\n", len);

	DMADBG("packet dest MAC addr: %x:%x:%x:%x:%x:%x\n", buf[0], buf[1],
		   buf[2], buf[3], buf[4], buf[5]);
	DMADBG("packet src MAC addr: %x:%x:%x:%x:%x:%x\n", buf[6], buf[7],
		   buf[8], buf[9], buf[10], buf[11]);
	DMADBG("packet EtherType: %x:%x\n", buf[12], buf[13]);

	DMADBG("dumping packet payload length: %d\n", payload_len);
	if (payload_len > 0) {
		num_dumps = payload_len / 8;

		if (num_dumps > 8) {
			num_dumps = 8;
		}

		index = 14;
		for (i = 0; i < num_dumps; ++i) {
			DMADBG("Packet: %d, %x:%x:%x:%x:%x:%x:%x:%x\n", i,
				   buf[index + 0], buf[index + 1],
				   buf[index + 2], buf[index + 3],
				   buf[index + 4], buf[index + 5],
				   buf[index + 6], buf[index + 7]);
			index += 8;
			if (index + 8 > len) {
				break;
			}
		}
	}
}

void ecpri_dma_dp_tasklet_exception_notify(unsigned long data)
{
	int i = 0;
	int ret = 0;
	struct ecpri_dma_pkt_completion_wrapper
		exception_pkts_arr[ECPRI_DMA_DP_EXCEPTION_BUDGET];
	struct ecpri_dma_pkt_completion_wrapper **exception_pkts;
	u32 actual_num = 0;
	struct ecpri_dma_endp_context *endp;

	endp = (struct ecpri_dma_endp_context *)data;

	if (!endp || !endp->valid || !endp->gsi_ep_cfg->is_exception) {
		DMAERR("Exception pkt recieved on non exception endp\n");
		ecpri_dma_assert();
	}

	exception_pkts = kzalloc(sizeof(struct ecpri_dma_pkt_completion_wrapper*) *
				ECPRI_DMA_DP_EXCEPTION_BUDGET, GFP_NOWAIT);
	ecpri_dma_assert_on(!exception_pkts);

	for (i = 0; i < ECPRI_DMA_DP_EXCEPTION_BUDGET;i++) {
		exception_pkts[i] = &exception_pkts_arr[i];
	}

	/* Poll Exceptions & Increase exception statistics */
	ret = ecpri_dma_dp_rx_poll(endp, ECPRI_DMA_DP_EXCEPTION_BUDGET,
				   exception_pkts, &actual_num);
	if (ret) {
		DMAERR("Exception endp polling failed\n");
		kfree(exception_pkts);
		ecpri_dma_assert();
	}
	ecpri_dma_ctx->exception_stats.num_of_pkts_recieved += actual_num;

	/* Credits have only one buffer so no need to check num_of_buffs */
	for (i = 0; i < actual_num; i++) {
		DMAERR("Got exception packet with status %d, dumping:\n",
		       exception_pkts[i]->status_code);
		//TODO: change from dump to terminal to dump to array
		ecpri_dma_dump_packet(exception_pkts[i]->pkt->buffs[0]->virt_base,
				      exception_pkts[i]->pkt->buffs[0]->size);
		ecpri_dma_ctx->exception_stats.num_of_bytes_recieved +=
			exception_pkts[i]->pkt->buffs[0]->size;
	}

	/* Free exception credits and prepare for replenish */
	for (i = 0; i < actual_num; i++) {
		DMADBG("Freeing exception packt: %d\n", i);
		dma_free_coherent(ecpri_dma_ctx->pdev,
				  exception_pkts[i]->pkt->buffs[0]->size,
				  exception_pkts[i]->pkt->buffs[0]->virt_base,
				  exception_pkts[i]->pkt->buffs[0]->phys_base);
		kmem_cache_free(endp->available_exception_buffs_cache,
				exception_pkts[i]->pkt->buffs[0]);
		kfree(exception_pkts[i]->pkt->buffs);
		kmem_cache_free(endp->available_exception_pkts_cache,
				exception_pkts[i]->pkt);
	}

	if(actual_num == 0) {
		/* No more packet to poll, change back to IRQ mode */
		ret = ecpri_dma_set_endp_mode(endp, ECPRI_DMA_NOTIFY_MODE_IRQ);
		if (ret) {
			DMAERR("Setting exception endp to IRQ mode failed\n");
			kfree(exception_pkts);
			ecpri_dma_assert();
		}
	} else {
		ret = ecpri_dma_dp_exception_replenish(endp, actual_num);
		if (ret) {
			DMAERR("Failed to replenish exception endp\n");
			kfree(exception_pkts);
			ecpri_dma_assert();
		}

		/* There might be more packet to poll, rescheduale tasklet */
		tasklet_schedule(&endp->tasklet);
	}

	kfree(exception_pkts);
}

void ecpri_dma_dp_exception_endp_notify_completion(
	struct ecpri_dma_endp_context *endp,
	struct ecpri_dma_pkt_completion_wrapper **comp_pkt,
	u32 num_of_completed)
{
	int ret = 0;
	if (!endp || !endp->valid || !endp->gsi_ep_cfg->is_exception) {
		DMAERR("Exception pkt recieved on non exception endp");
		ecpri_dma_assert();
	}

	ret = ecpri_dma_set_endp_mode(endp, ECPRI_DMA_NOTIFY_MODE_POLL);
	if(ret) {
		DMAERR("Setting exception endp to POLL mode failed\n");
		ecpri_dma_assert();
	}

	tasklet_schedule(&endp->tasklet);
}

//TODO: add documentation
static int ecpri_dma_dp_gen_gsi_xfer(struct ecpri_dma_pkt *pkt,
	struct gsi_xfer_elem *gsi_xfer,
	struct ecpri_dma_outstanding_pkt_wrapper *pkt_wrapper,
	u32 *total_bytes, int dma_dir)
{
	int i = 0;

	for (i = 0; i < pkt->num_of_buffers; i++) {
		if (!pkt->buffs[i]->phys_base)
		{
			/* Perform mapping using SMMU */
			pkt->buffs[i]->phys_base =
				dma_map_single(ecpri_dma_ctx->pdev,
					pkt->buffs[i]->virt_base,
					pkt->buffs[i]->size, dma_dir);
			if (dma_mapping_error(ecpri_dma_ctx->pdev,
				pkt->buffs[i]->phys_base)) {
				DMAERR("failed to do dma map.\n");
				return -EFAULT;
			}
		}

		/* Set gsi_xfer fields */
		gsi_xfer[i].addr = pkt->buffs[i]->phys_base;
		gsi_xfer[i].len = pkt->buffs[i]->size;
		gsi_xfer[i].type = GSI_XFER_ELEM_DATA;
		gsi_xfer[i].flags |= GSI_XFER_FLAG_BEI;

		/* Set EOT or CHAIN bits as appropriate */
		//TODO: Implement event moderation by not setting EOT flag here
		if (i == pkt->num_of_buffers - 1) {
			gsi_xfer[i].flags |= GSI_XFER_FLAG_EOT;
			gsi_xfer[i].xfer_user_data = (void*)pkt_wrapper;
		}
		else
			gsi_xfer[i].flags |= GSI_XFER_FLAG_CHAIN;

		total_bytes += pkt->buffs[i]->size;
	}

	return 0;
}

/**
 * ecpri_dma_tasklet_rx_done() - this function will be (eventually)
 * called when a Rx operation is complete
 * @data: user pointer point to the ecpri_dma_endp_context
 *
 * Will be called in deferred context.
 * - Notify client for Rx completion
 */
void ecpri_dma_tasklet_rx_done(unsigned long data)
{
	struct ecpri_dma_endp_context *endp;

	endp = (struct ecpri_dma_endp_context *)data;
	DMADBG("Notify Rx ENDP %d on completion\n", endp->endp_id);
	/* Notify client on Rx completion */
	if (endp->notify_comp != NULL) {
		endp->notify_comp(endp, NULL, 0);
	}
	else {
		DMADBG("ENDP %d doesn't have notify function\n",
			endp->endp_id);
	}
}

/**
 * ecpri_dma_tasklet_transmit_done() - this function will be (eventually)
 * called when a Tx operation is complete
 * @data: user pointer point to the ecpri_dma_endp_context
 *
 * Will be called in deferred context.
 * - iterate over all completed packets and unmap the buffers using the SMMU
 * - invoke the callback supplied by the client who transmited the packets
 * - remove all the tx packet wrappers from the endp outstanding packets list
 */
void ecpri_dma_tasklet_transmit_done(unsigned long data)
{
	struct ecpri_dma_endp_context *endp;
	struct ecpri_dma_outstanding_pkt_wrapper *curr_pkt_wrapper;
	struct ecpri_dma_outstanding_pkt_wrapper *next_pkt_wrapper;
	struct ecpri_dma_pkt_completion_wrapper **comp_pkts_arr;
	struct ecpri_dma_pkt *pkt;
	struct list_head *pos, *n;
	u32 num_of_completed, completed_pkt_index = 0;
	int i = 0;
	int dma_dir = 0;
	unsigned long flags;

	endp = (struct ecpri_dma_endp_context *)data;
	num_of_completed = atomic_read(&endp->xmit_eot_cnt);

	if (endp->gsi_ep_cfg->dir == ECPRI_DMA_ENDP_DIR_SRC)
		dma_dir = DMA_TO_DEVICE;
	else
		dma_dir = DMA_FROM_DEVICE;

	comp_pkts_arr =
		kzalloc(sizeof(struct ecpri_dma_pkt_completion_wrapper *) *
				num_of_completed, GFP_NOWAIT);
	if (!comp_pkts_arr) {
		DMAERR("Cannot allocate completed packets array\n");
		ecpri_dma_assert();
	}

	spin_lock_irqsave(&endp->spinlock, flags);
	curr_pkt_wrapper = list_first_entry(&endp->outstanding_pkt_list,
				 struct ecpri_dma_outstanding_pkt_wrapper, link);

	while (!list_empty(&endp->outstanding_pkt_list) &&
	       curr_pkt_wrapper->xfer_done &&
	       atomic_add_unless(&endp->xmit_eot_cnt, -1, 0)) {
		/* Perform unmapping using SMMU */
		pkt = curr_pkt_wrapper->comp_pkt.pkt;

		/* Unmapping is only required for ETH M2S ENDPs */
		if (endp->gsi_ep_cfg->stream_mode != ECPRI_DMA_ENDP_STREAM_MODE_M2M)
		{
			for (i = 0; i < pkt->num_of_buffers; i++) {
				dma_unmap_single(ecpri_dma_ctx->pdev,
					pkt->buffs[i]->phys_base,
					pkt->buffs[i]->size, dma_dir);
			}
		}

		/*	Remove completed packet wrapper from endp list and prepare
				the completed packets array for the client */
		comp_pkts_arr[completed_pkt_index] = &curr_pkt_wrapper->comp_pkt;
		next_pkt_wrapper = list_next_entry(curr_pkt_wrapper, link);
		list_move_tail(&curr_pkt_wrapper->link, &endp->completed_pkt_list);
		endp->curr_outstanding_num--;
		endp->curr_completed_num++;

		curr_pkt_wrapper = next_pkt_wrapper;
		completed_pkt_index++;
	}

	spin_unlock_irqrestore(&endp->spinlock, flags);

	/* Notify client on all completed packets */
	if (endp->notify_comp != NULL) {
		endp->notify_comp(endp, comp_pkts_arr, completed_pkt_index);
	}
	else {
		DMADBG("ENDP %d doesn't have notify function\n",
			endp->endp_id);
	}

	/* Free allocated entry */
	spin_lock_irqsave(&endp->spinlock, flags);
	list_for_each_safe(pos, n, &endp->completed_pkt_list)
	{
		curr_pkt_wrapper = list_entry(pos,
			struct ecpri_dma_outstanding_pkt_wrapper,
			link);
		if (endp->avail_outstanding_pkts >=
		    ECPRI_DMA_OUTSTANDING_PKTS_CACHE_MAX_THRESHOLD) {
			list_del(&curr_pkt_wrapper->link);
			kmem_cache_free(endp->available_outstanding_pkts_cache,
				curr_pkt_wrapper);
		} else {
			list_move_tail(&curr_pkt_wrapper->link,
				&endp->available_outstanding_pkts_list);
			endp->avail_outstanding_pkts++;
		}
		endp->curr_completed_num--;
	}
	spin_unlock_irqrestore(&endp->spinlock, flags);

	kfree(comp_pkts_arr);
}

int ecpri_dma_set_endp_mode(struct ecpri_dma_endp_context *endp,
	enum ecpri_dma_notify_mode mode)
{
	int ret = 0;

	if (!endp || !endp->valid)
		return -EINVAL;

	atomic_set(&endp->curr_polling_state, mode);
	switch (mode) {
	case ECPRI_DMA_NOTIFY_MODE_IRQ:
		ret = gsi_config_channel_mode(endp->gsi_chan_hdl,
					      GSI_CHAN_MODE_CALLBACK);
		if ((ret != GSI_STATUS_SUCCESS) &&
		    !atomic_read(&endp->curr_polling_state)) {
			DMAERR("Failed to switch to intr mode %d ch_id %d\n",
			       endp->curr_polling_state, endp->gsi_chan_hdl);
		}
		break;
	case ECPRI_DMA_NOTIFY_MODE_POLL:
		ret = gsi_config_channel_mode(endp->gsi_chan_hdl,
					      GSI_CHAN_MODE_POLL);
		if ((ret != GSI_STATUS_SUCCESS) &&
		    atomic_read(&endp->curr_polling_state)) {
			DMAERR("Failed to switch to poll mode %d ch_id %d\n",
			       endp->curr_polling_state, endp->gsi_chan_hdl);
		}
		break;
	default:
		DMAERR("Invalid ENDP Notify mode recieved\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

int ecpri_dma_get_endp_mode(struct ecpri_dma_endp_context *endp,
	enum ecpri_dma_notify_mode *mode)
{
	if (!endp || !endp->valid)
		return -EINVAL;

	*mode = atomic_read(&endp->curr_polling_state);

	return 0;
}

void ecpri_dma_dp_rx_comp_hdlr(struct gsi_chan_xfer_notify *notify)
{
	struct ecpri_dma_outstanding_pkt_wrapper *comp_pkt;
	struct ecpri_dma_endp_context *endp;
	int ret = 0;

	DMADBG_LOW("event code %d recieved for CH %d\n", notify->evt_id,
		notify->chid);

	switch (notify->evt_id) {
	case GSI_CHAN_EVT_EOT:
	case GSI_CHAN_EVT_OVERFLOW:
		comp_pkt = notify->xfer_user_data;
		endp = comp_pkt->endp;

		ret = ecpri_dma_set_endp_mode(endp, ECPRI_DMA_NOTIFY_MODE_POLL);
		if (ret) {
			DMAERR("Setting ENDP %d endp to POLL mode failed\n",endp->endp_id);
			ecpri_dma_assert();
		}

		/* Notify client on Rx completion */
		tasklet_schedule(&endp->tasklet);
		break;
	default:
		DMAERR("received unexpected event code %d on CH %d\n", notify->evt_id,
			notify->chid);
	}
}

void ecpri_dma_dp_tx_comp_hdlr(struct gsi_chan_xfer_notify *notify)
{
	struct ecpri_dma_outstanding_pkt_wrapper *comp_pkt;
	struct ecpri_dma_endp_context *endp;

	DMADBG_LOW("event code %d recieved for CH %d\n", notify->evt_id,
		   notify->chid);

	switch (notify->evt_id) {
	case GSI_CHAN_EVT_EOT:
		comp_pkt = notify->xfer_user_data;
		comp_pkt->xfer_done = true;
		endp = comp_pkt->endp;

		atomic_inc(&endp->xmit_eot_cnt);
		tasklet_schedule(&endp->tasklet);
		break;
	default:
		DMAERR("received unexpected event code %d on CH %d\n", notify->evt_id,
			notify->chid);
	}
}

int ecpri_dma_dp_rx_poll(struct ecpri_dma_endp_context *endp, u32 budget,
	struct ecpri_dma_pkt_completion_wrapper **pkts, u32 *actual_num)
{
	int ret = 0, i = 0;
	u32 rem_budget = budget, num_of_buff = 0;
	u32 curr_iter_num_of_pkts = 0;
	bool is_poll_empty = false;
	struct ecpri_dma_outstanding_pkt_wrapper *curr_pkt_wrapper;
	struct list_head *pos, *n;
	struct gsi_chan_xfer_notify notify[ECPRI_DMA_DP_MAX_DESC];
	unsigned long flags;

	if (!endp || !endp->valid || !budget || !pkts || !actual_num) {
		DMAERR("Invalid parameters\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&endp->spinlock, flags);
	/* Begin polling the GSI event */
	while (rem_budget && !is_poll_empty) {
		ret = gsi_poll_n_channel(endp->gsi_chan_hdl, notify,
					rem_budget < ECPRI_DMA_DP_MAX_DESC ?
					rem_budget : ECPRI_DMA_DP_MAX_DESC, &num_of_buff);

		if (ret == GSI_STATUS_POLL_EMPTY) {
			is_poll_empty = true;
		} else if (ret != GSI_STATUS_SUCCESS && !num_of_buff) {
			/*	GSI encountered an issue, stop polling even if there's
					more budget, pass packets polled until now to the client */
			DMAERR("Poll channel err: %d\n", ret);
			break;
		}

		endp->total_pkts_recv += num_of_buff;

		/*	Transfer completion data from GSI event to completion pkts list */
		for (i = 0; i < num_of_buff; i++) {
			curr_pkt_wrapper = list_first_entry(&endp->outstanding_pkt_list,
				struct ecpri_dma_outstanding_pkt_wrapper, link);

			curr_pkt_wrapper->comp_pkt.status_code = notify[i].status;
			curr_pkt_wrapper->bytes_xfered = notify[i].bytes_xfered;

			switch (notify[i].evt_id) {
			case GSI_CHAN_EVT_EOT:
				curr_iter_num_of_pkts++;
				curr_pkt_wrapper->comp_pkt.comp_code =
					ECPRI_DMA_COMPLETION_CODE_EOT;
				break;
			case GSI_CHAN_EVT_OVERFLOW:
				curr_pkt_wrapper->comp_pkt.comp_code =
					ECPRI_DMA_COMPLETION_CODE_OVERFLOW;
				break;
			default:
				DMAERR("Unexpected event code %d\n", notify[i].evt_id);
				break;
			}

			list_move_tail(&curr_pkt_wrapper->link, &endp->completed_pkt_list);
			endp->curr_outstanding_num--;
			endp->curr_completed_num++;
		}

		/*	Budget is in packets, not buffers so we need to only count EoTs */
		rem_budget -= curr_iter_num_of_pkts;

		curr_iter_num_of_pkts = 0;
		num_of_buff = 0;
	}

	ret = 0;
	i = 0;
	list_for_each_safe(pos, n, &endp->completed_pkt_list)
	{
		curr_pkt_wrapper = list_entry(pos,
			struct ecpri_dma_outstanding_pkt_wrapper,
			link);

		/*	Transfer compeltion info from completed pkt wrapper to array
			provided by the client */
		pkts[i]->status_code = curr_pkt_wrapper->comp_pkt.status_code;
		pkts[i]->comp_code = curr_pkt_wrapper->comp_pkt.comp_code;
		pkts[i]->pkt = curr_pkt_wrapper->comp_pkt.pkt;
		pkts[i]->pkt->buffs[0]->size = curr_pkt_wrapper->bytes_xfered;
		i++;

		/* Free completed packet wrappers*/
		if (endp->avail_outstanding_pkts >=
			ECPRI_DMA_OUTSTANDING_PKTS_CACHE_MAX_THRESHOLD) {
			list_del(&curr_pkt_wrapper->link);
			kmem_cache_free(endp->available_outstanding_pkts_cache,
				curr_pkt_wrapper);
		} else {
			list_move_tail(&curr_pkt_wrapper->link,
				&endp->available_outstanding_pkts_list);
			endp->avail_outstanding_pkts++;
		}
		endp->curr_completed_num--;
	}
	spin_unlock_irqrestore(&endp->spinlock, flags);

	*actual_num = i;

	return ret;
}

int ecpri_dma_dp_commit(struct ecpri_dma_endp_context *endp)
{
	int ret;
	unsigned long flags;

	if (!endp || !endp->valid) {
		DMAERR("Invalid parameters\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&endp->spinlock, flags);

	ret = gsi_queue_xfer(endp->gsi_chan_hdl, 0, NULL,
			     true);
	if (ret != GSI_STATUS_SUCCESS) {
		DMAERR("GSI xfer failed, ENDP ID%x\n", endp->endp_id);
		spin_unlock_irqrestore(&endp->spinlock, flags);
		return -EFAULT;
	}

	/* Release spinlock before returning from commit function */
	spin_unlock_irqrestore(&endp->spinlock, flags);

	return 0;
}

int ecpri_dma_dp_transmit(struct ecpri_dma_endp_context *endp,
			  struct ecpri_dma_pkt **pkts, u32 num_of_pkts,
			  bool commit)
{
	struct gsi_xfer_elem gsi_xfer_arr[ECPRI_DMA_DP_MAX_DESC];
	struct ecpri_dma_outstanding_pkt_wrapper *pkt_wrapper = NULL;
	int i = 0, j = 0, k = 0;
	int gsi_xfer_index = 0;
	int ret;
	int dma_dir;
	u32 total_bytes = 0;
	unsigned long flags;

	if (!endp || !endp->valid || !pkts || num_of_pkts == 0) {
		DMAERR("Invalid parameters\n");
		return -EINVAL;
	}

	DMADBG("Transmit start\n");

	if (endp->gsi_ep_cfg->dir == ECPRI_DMA_ENDP_DIR_SRC)
		dma_dir = DMA_TO_DEVICE;
	else
		dma_dir = DMA_FROM_DEVICE;

	spin_lock_irqsave(&endp->spinlock, flags);

	if (unlikely(atomic_read(&endp->disconnect_in_progress))) {
		DMAERR("Pipe disconnect in progress dropping the packet\n");
		spin_unlock_irqrestore(&endp->spinlock, flags);
		return -EFAULT;
	}

	for (i = 0; i < num_of_pkts; i++) {
		/* Verify all packets have chains smaller than TLV fifo size */
		if (pkts[i]->num_of_buffers > endp->gsi_ep_cfg->dma_if_tlv) {
			DMAERR("Chain too long for one packet, discarding all\n");
			spin_unlock_irqrestore(&endp->spinlock, flags);
			return -EFAULT;
		}

		/* Verify we have enough space in the gsi_xfer array for all buffers */
		if (gsi_xfer_index + pkts[i]->num_of_buffers >
		    ECPRI_DMA_DP_MAX_DESC) {
			DMAERR("Too many buffers for one transmit, discarding all\n");
			spin_unlock_irqrestore(&endp->spinlock, flags);
			return -EFAULT;
		}

		if (!endp->eventless_endp) {
			/*  Prepare outstanding packet wrapper which will be used to follow up
            on transfer status */
			if (!list_empty(
				    &endp->available_outstanding_pkts_list)) {
				pkt_wrapper = list_first_entry(
					&endp->available_outstanding_pkts_list,
					struct ecpri_dma_outstanding_pkt_wrapper,
					link);
				list_del(&pkt_wrapper->link);
				endp->avail_outstanding_pkts--;
			} else {
				pkt_wrapper = kmem_cache_zalloc(
					endp->available_outstanding_pkts_cache,
					GFP_ATOMIC);
			}
			if (!pkt_wrapper) {
				DMAERR("failed to alloc packet wrapper\n");
				spin_unlock_irqrestore(&endp->spinlock, flags);
				return -ENOMEM;
			}
			memset(pkt_wrapper, 0, sizeof(*pkt_wrapper));

			/* Init the packet wrapper and add it to endp list */
			pkt_wrapper->comp_pkt.pkt = pkts[i];
			pkt_wrapper->endp = endp;
			list_add_tail(&pkt_wrapper->link,
				      &endp->outstanding_pkt_list);
			endp->curr_outstanding_num++;
		}

		/* Generate parameters for GSI transfer */
		ret = ecpri_dma_dp_gen_gsi_xfer(pkts[i],
						&gsi_xfer_arr[gsi_xfer_index],
						pkt_wrapper, &total_bytes, dma_dir);
		if (ret)
		{
			DMAERR("Failed to generate gsi xfer for pkt %d\n", i);
			goto fail_handling;
		}
		gsi_xfer_index += pkts[i]->num_of_buffers;
	}

	if (endp->curr_outstanding_num > endp->ring_length) {
		DMAERR("This transfer will exceed ring length, dropping all\n");
		goto fail_handling;
	}

	/* We're now ready to queue the transfer to the GSI ring */
	DMADBG_LOW("ch:%lu queue xfer %d descriptors with commit %d\n",
		   endp->gsi_chan_hdl, gsi_xfer_index, commit);

	ret = gsi_queue_xfer(endp->gsi_chan_hdl, gsi_xfer_index, &gsi_xfer_arr[0],
			     commit);
	if (ret != GSI_STATUS_SUCCESS) {
		DMAERR("GSI xfer failed.\n");
		goto fail_handling;
	}

	/* Increase ENDP stats */
	endp->total_pkts_sent += num_of_pkts;
	endp->total_bytes_sent += total_bytes;

	/* Release spinlock before returning from transmit function */
	spin_unlock_irqrestore(&endp->spinlock, flags);

	DMADBG("Transmit finished\n");

	return 0;

fail_handling:
	/*  Error occured need to remove pkts from endp list & unmap buffers
        Only remove packet that were already queued and mapped */
	for (k = 0; k < i; k++) {
		for (j = 0; j < pkts[k]->num_of_buffers; j++) {
			/* Perform unmapping using SMMU */
			dma_unmap_single(ecpri_dma_ctx->pdev,
				pkts[k]->buffs[j]->phys_base,
				pkts[k]->buffs[j]->size, dma_dir);
		}

		/* Remove from list */
		pkt_wrapper = list_last_entry(&endp->outstanding_pkt_list,
			struct ecpri_dma_outstanding_pkt_wrapper, link);
		list_del(&pkt_wrapper->link);
		endp->curr_outstanding_num--;
		kmem_cache_free(endp->available_outstanding_pkts_cache,
				pkt_wrapper);
	}
	spin_unlock_irqrestore(&endp->spinlock, flags);
	return -EFAULT;
}
