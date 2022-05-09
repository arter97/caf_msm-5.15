/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "ecpri_dma_mhi_client.h"
#include "gsihal.h"

#define ECPRI_DMA_MHI_CLIENT_MEMCPY_ASYNC_BUDGET (5)

struct ecpri_dma_mhi_client_context*
	ecpri_dma_mhi_client_ctx[ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM] = { NULL };

struct ecpri_dma_mhi_memcpy_context*
	ecpri_dma_mhi_memcpy_ctx[ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM] = { NULL };
#define ECPRI_DMA_MHI_CLIENT_WQ_NAME_LEN (20)

static const char* wq_name[ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM]
[ECPRI_DMA_MHI_CLIENT_WQ_NAME_LEN] =
{
	{
		"ecpri_dma_mhi_wq_vm_0",
		"ecpri_dma_mhi_wq_vm_1",
		"ecpri_dma_mhi_wq_vm_2",
		"ecpri_dma_mhi_wq_vm_3",
		"ecpri_dma_mhi_wq_pf"
	}
};

static const struct ecpri_dma_mhi_function_endp_data
ecpri_dma_mhi_function_endp_dt[ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM] = {
	[ECPRI_DMA_VM_IDS_VM0] = {
		.sync_src_id = 22,
		.sync_dest_id = 59,
		.async_src_id =
			ECPRI_DMA_MHI_INVALID_ENDP_ID,
		.async_dest_id =
			ECPRI_DMA_MHI_INVALID_ENDP_ID
	},
	[ECPRI_DMA_VM_IDS_VM1] = {
		.sync_src_id = 25,
		.sync_dest_id = 62,
		.async_src_id =
			ECPRI_DMA_MHI_INVALID_ENDP_ID,
		.async_dest_id =
			ECPRI_DMA_MHI_INVALID_ENDP_ID
	},
	[ECPRI_DMA_VM_IDS_VM2] = {
		.sync_src_id = 28,
		.sync_dest_id = 65,
		.async_src_id =
			ECPRI_DMA_MHI_INVALID_ENDP_ID,
		.async_dest_id =
			ECPRI_DMA_MHI_INVALID_ENDP_ID
	},
	[ECPRI_DMA_VM_IDS_VM3] = {
		.sync_src_id = 31,
		.sync_dest_id = 68,
		.async_src_id =
			ECPRI_DMA_MHI_INVALID_ENDP_ID,
		.async_dest_id =
			ECPRI_DMA_MHI_INVALID_ENDP_ID
	},
	[ECPRI_DMA_MHI_PF_ID] = {
		.sync_src_id = 32,
		.sync_dest_id = 69,
		.async_src_id = 33,
		.async_dest_id = 70
	}
};

static inline bool ecpri_dma_mhi_check_destroy_pending(
	struct ecpri_dma_mhi_memcpy_context* memcpy_ctx)
{
	bool destroy_pending = false;
	unsigned long flags;
	unsigned long flags_sync;
	unsigned long flags_async;

	spin_lock_irqsave(&memcpy_ctx->lock, flags);
	spin_lock_irqsave(&memcpy_ctx->async_lock, flags_async);
	spin_lock_irqsave(&memcpy_ctx->sync_lock, flags_sync);

	destroy_pending = memcpy_ctx->destroy_pending;

	spin_unlock_irqrestore(&memcpy_ctx->sync_lock,
		flags_sync);
	spin_unlock_irqrestore(&memcpy_ctx->async_lock,
		flags_async);
	spin_unlock_irqrestore(&memcpy_ctx->lock,
		flags);

	return destroy_pending;
}

/**
 * ecpri_dma_mhi_get_function_context_index() -
 * Gets the VF/PF index in Global Context Array
 *
 * @function: [IN] Function parameters, type and id
 * @idx:            [OUT] The translated index
 *
 * Return codes: 0: success
 *		-EINVAL: Unexpected function type
 */
static inline int ecpri_dma_mhi_get_function_context_index(
	struct mhi_dma_function_params function,
	int* idx)
{
	int ret = 0;
	if (function.function_type ==
		MHI_DMA_FUNCTION_TYPE_VIRTUAL &&
		function.vf_id < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM - 1) {
		*(idx) = function.vf_id;
	}
	else if (function.function_type ==
		MHI_DMA_FUNCTION_TYPE_PHYSICAL) {
		*(idx) = (ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM - 1);
	}
	else {
		DMAERR("Unexpected function type, type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * ecpri_dma_mhi_get_ee_index() - Gets the correspodning EE ID
 *
 * @function: [IN] Function parameters, type and id
 * @ee_idx:         [OUT] The EE index from map array
 *
 * Return codes: 0: success
 *		-EINVAL: Unexpected function type
 */
static inline int ecpri_dma_mhi_get_ee_index(
	struct mhi_dma_function_params function,
	enum ecpri_dma_ees* ee_idx)
{
	int ret = 0;
	if (function.function_type ==
		MHI_DMA_FUNCTION_TYPE_VIRTUAL &&
		function.vf_id < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM - 1) {
		switch (function.vf_id)
		{
		case ECPRI_DMA_VM_IDS_VM0:
			*(ee_idx) = ECPRI_DMA_EE_VM0;
			break;
		case ECPRI_DMA_VM_IDS_VM1:
			*(ee_idx) = ECPRI_DMA_EE_VM1;
			break;
		case ECPRI_DMA_VM_IDS_VM2:
			*(ee_idx) = ECPRI_DMA_EE_VM2;
			break;
		case ECPRI_DMA_VM_IDS_VM3:
			*(ee_idx) = ECPRI_DMA_EE_VM3;
			break;
		default:
			DMAERR("Unexpected function type, type: %d, vf_id: %d\n",
				function.function_type, function.vf_id);
			ret = -EINVAL;
			break;
		}
	}
	else if (function.function_type ==
		MHI_DMA_FUNCTION_TYPE_PHYSICAL) {
		*(ee_idx) = ECPRI_DMA_EE_PF;
	}
	else {
		DMAERR("Unexpected function type, type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * ecpri_dma_mhi_get_endp_ctx() - Gets the endpoint
 * GSI configuration data, by EE ID and CH ID
 *
 * @ee_idx:      [IN] EE ID
 * @channel_id:  [IN] CH ID
 * @endp_cfg:    [OUT] GSI endp configuration data
 *
 */
static int ecpri_dma_mhi_get_endp_ctx(
	enum ecpri_dma_ees ee_idx, u8 channel_id,
	struct ecpri_dma_endp_context** endp_ctx)
{
	int ret = -EINVAL;
	int i;

	for (i = 0; i < ECPRI_DMA_ENDP_NUM_MAX; i++)
	{
		if (ecpri_dma_ctx->endp_map[i].valid &&
			ecpri_dma_ctx->endp_map[i].dma_gsi_chan_num == channel_id &&
			ecpri_dma_ctx->endp_map[i].ee == ee_idx) {
			*(endp_ctx) = &ecpri_dma_ctx->endp_ctx[i];
			ecpri_dma_ctx->endp_ctx[i].endp_id = i;
			ret = 0;
			break;
		}
	}

	return ret;
}

static void ecpri_dma_mhi_get_l2_ch_bitmap(
	enum ecpri_dma_ees ee_idx, u32 function, u32 *bitmap)
{
	int i;

	DMADBG("Begin\n");

	*bitmap = 0;

	for (i = 0; i < ECPRI_DMA_ENDP_NUM_MAX; i++)
	{
		if (ecpri_dma_ctx->endp_map[i].valid &&
			ecpri_dma_ctx->endp_map[i].ee == ee_idx &&
			i != ecpri_dma_mhi_function_endp_dt[function].sync_src_id &&
			i != ecpri_dma_mhi_function_endp_dt[function].sync_dest_id &&
			i != ecpri_dma_mhi_function_endp_dt[function].async_src_id &&
			i != ecpri_dma_mhi_function_endp_dt[function].async_dest_id) {
			*bitmap |= 1 << ecpri_dma_ctx->endp_map[i].dma_gsi_chan_num;
		}
	}
}

/**
 * ecpri_dma_mhi_dma_alloc_pkt() - Allocates
 * the memory for packet's buffer
 *
 * @buff_addr:  [IN] Buffer address
 * @len:        [IN] Buffer length
 * @pkts:       [OUT] Allocated packets data
 *
 * Return codes: 0: success
 *		-EINVAL: Illegal buffer address
 *		-ENOMEM: allocating memory error
 */
static int ecpri_dma_mhi_dma_alloc_pkt(
	u64 buff_addr,
	int len,
	struct ecpri_dma_pkt*** pkt_ptr)
{
	int ret = 0;
	struct ecpri_dma_pkt** pkt = NULL;

	pkt = kzalloc(sizeof(*pkt), GFP_KERNEL);
	if (!pkt) {
		DMAERR("failed to alloc packets array \n");
		return -ENOMEM;
	}

	pkt[0] = kzalloc(sizeof(*pkt[0]), GFP_KERNEL);
	if (!pkt[0]) {
		DMAERR("failed to alloc packet\n");
		kfree(pkt);
		ret = -ENOMEM;
		goto fail_alloc;
	}

	pkt[0]->buffs =
		kzalloc(sizeof(*(pkt[0]->buffs)), GFP_KERNEL);
	if (!(pkt[0]->buffs)) {
		DMAERR("failed to alloc buffers array \n");
		kfree(pkt[0]);
		kfree(pkt);
		ret = -ENOMEM;
		goto fail_alloc;
	}

	pkt[0]->buffs[0] = kzalloc(
		sizeof(*(pkt[0]->buffs[0])), GFP_KERNEL);
	if (!pkt[0]->buffs[0]) {
		DMAERR("failed to alloc dma buff wrapper\n");
		kfree(pkt[0]->buffs);
		kfree(pkt[0]);
		kfree(pkt);
		ret = -ENOMEM;
		goto fail_alloc;
	}

	pkt[0]->buffs[0]->phys_base = (dma_addr_t)buff_addr;
	pkt[0]->buffs[0]->size = len;
	pkt[0]->num_of_buffers = 1;

	*pkt_ptr = pkt;

fail_alloc:
	return ret;
}

/**
 * ecpri_dma_mhi_dma_free_pkt() - Frees
 * the allocated memory for packet's
 *
 * @pkts:       [IN] Allocated packets data
 *
 */
static void ecpri_dma_mhi_dma_free_pkt(
	struct ecpri_dma_pkt* pkts)
{
	kfree(pkts->buffs[0]);
	kfree(pkts->buffs);
	kfree(pkts);
}

/**
 * ecpri_dma_mhi_wq_notify_ready() - Notify MHI client on ready
 *
 * This function is called from DMA MHI workqueue to notify
 * MHI client driver on ready event
 *
 */
static void ecpri_dma_mhi_wq_notify_ready(struct work_struct* work)
{
	struct ecpri_dma_mhi_wq_work_type* ecpri_dma_mhi_work =
		container_of(work,
			struct ecpri_dma_mhi_wq_work_type,
			work);
	ecpri_dma_mhi_work->ctx->notify_cb(
		ecpri_dma_mhi_work->ctx->user_data,
		MHI_DMA_EVENT_READY,
		0);

	kfree(ecpri_dma_mhi_work);
}

/**
 * ecpri_dma_mhi_memcpy_async_wq_cb_ready() - Notify MHI client on async comp
 *
 * This function is called to notify ASYNC transfer completion.
 *
 */
static void ecpri_dma_mhi_memcpy_async_wq_cb_ready(struct work_struct* work)
{
	struct ecpri_dma_mhi_memcpy_context *memcpy_ctx = NULL;
	unsigned long flags;
	struct ecpri_dma_mhi_async_wq_work_type *async_work = container_of(
		work, struct ecpri_dma_mhi_async_wq_work_type, work);

	DMADBG("Begin\n");

	memcpy_ctx = ecpri_dma_mhi_memcpy_ctx[ECPRI_DMA_MHI_PF_ID];
	if (!memcpy_ctx) {
		DMAERR("Memcpy context is not initialized\n");
		return;
	}

	if (ecpri_dma_mhi_check_destroy_pending(memcpy_ctx) == true) {
		DMAERR("Memcpy destroy in progress\n");
		return;
	}

	spin_lock_irqsave(&memcpy_ctx->async_lock, flags);

	if (list_empty(
		&memcpy_ctx->cbs_list)) {
		DMAERR("The callback list is empty but shouldn't be\n");
		spin_unlock_irqrestore(&memcpy_ctx->async_lock, flags);
		return;
	}

	async_work->xfer_desc = list_first_entry(
		&memcpy_ctx->cbs_list,
		struct ecpri_dma_mhi_xfer_wrapper,
		link);

	list_del(&async_work->xfer_desc->link);

	atomic_dec(&memcpy_ctx->async_pending);
	atomic_inc(&memcpy_ctx->async_total);

	spin_unlock_irqrestore(&memcpy_ctx->async_lock, flags);

	async_work->xfer_desc->user_cb(async_work->xfer_desc->user_data);

	kmem_cache_free(memcpy_ctx->xfer_wrapper_cache, async_work->xfer_desc);
	kfree(async_work);
}

/**
 * ecpri_dma_mhi_memcpy_async_wq_cb_ready_vms() - Notify MHI client on async comp
 *
 * This function is called to notify ASYNC transfer completion.
 *
 */
static void ecpri_dma_mhi_memcpy_async_wq_cb_ready_vms(struct work_struct* work)
{
	int idx = 0, ret = 0;
	struct ecpri_dma_mhi_memcpy_context* memcpy_ctx = NULL;
	struct ecpri_dma_mhi_async_wq_work_type* async_work = container_of(
		work, struct ecpri_dma_mhi_async_wq_work_type, work);

	DMADBG("Begin\n");

	/* Get the index of VM/PF */
	ret = ecpri_dma_mhi_get_function_context_index(
		async_work->xfer_desc->function, &idx);
	if (ret != 0) {
		DMAERR("Function params are invalid,"
			"function type: %d, vf_id: %d\n",
			async_work->xfer_desc->function.function_type,
			async_work->xfer_desc->function.vf_id);
		return;
	}

	memcpy_ctx = ecpri_dma_mhi_memcpy_ctx[idx];
	if (!memcpy_ctx) {
		DMAERR("Memcpy context is not initialized\n");
		return;
	}

	if (ecpri_dma_mhi_check_destroy_pending(memcpy_ctx) == true) {
		DMAERR("Memcpy destroy in progress\n");
		return;
	}

	async_work->xfer_desc->user_cb(async_work->xfer_desc->user_data);

	kmem_cache_free(memcpy_ctx->xfer_wrapper_cache, async_work->xfer_desc);
	kfree(async_work);
}

/**
 * ecpri_dma_mhi_memcpy_async_notify_comp() - Notifies on sending completion
 *
 * @endp: endpoint context
 * @comp_pkt: the packets data
 * @num_of_completed: Number of completed packets
 *
 * This function notifies MHI driver on async completion
 * using supplied callback function
 *
 */
static void ecpri_dma_mhi_memcpy_async_notify_comp(
	struct ecpri_dma_endp_context* endp,
	struct ecpri_dma_pkt_completion_wrapper** comp_pkt,
	u32 num_of_completed)
{
	int i = 0;
	int ret = 0;
	struct ecpri_dma_mhi_memcpy_context* memcpy_ctx = NULL;
	struct ecpri_dma_pkt_completion_wrapper
		async_pkts_arr[ECPRI_DMA_MHI_CLIENT_MEMCPY_ASYNC_BUDGET];
	struct ecpri_dma_pkt_completion_wrapper*
		async_pkts[ECPRI_DMA_MHI_CLIENT_MEMCPY_ASYNC_BUDGET];
	struct ecpri_dma_mhi_async_wq_work_type *work = NULL;
	u32 actual_num = 0;

	if (!endp) {
		DMAERR("Null params args\n");
		return;
	}

	/* Skip VMs, pass the PF only*/
	if (endp->endp_id !=
		ecpri_dma_mhi_function_endp_dt[ECPRI_DMA_MHI_PF_ID].async_dest_id) {
		DMAERR("Received unexpected async completion on ENDP %d\n",
			endp->endp_id);
		return;
	}

	memcpy_ctx = ecpri_dma_mhi_memcpy_ctx[ECPRI_DMA_MHI_PF_ID];
	if (!memcpy_ctx) {
		DMAERR("Memcpy context is not initialized\n");
		return;
	}

	if (ecpri_dma_mhi_check_destroy_pending(memcpy_ctx) == true) {
		DMAERR("Memcpy destroy in progress\n");
		return;
	}

	for (i = 0; i < ECPRI_DMA_MHI_CLIENT_MEMCPY_ASYNC_BUDGET; i++) {
		async_pkts[i] = &async_pkts_arr[i];
	}

	ret = ecpri_dma_dp_rx_poll(endp,
				   ECPRI_DMA_MHI_CLIENT_MEMCPY_ASYNC_BUDGET,
				   async_pkts, &actual_num);
	if (ret) {
		DMAERR("ASYNC endp polling failed\n");
		ecpri_dma_assert();
	}

	if (!actual_num)
	{
		/* No Async PKTs pending */
		ret = ecpri_dma_set_endp_mode(endp, ECPRI_DMA_NOTIFY_MODE_IRQ);
		if (ret) {
			DMAERR("Setting ASYNC endp to IRQ mode failed\n");
			ecpri_dma_assert();
		}
		return;
	}

	for (i = 0; i < actual_num; i++)
	{
		/* Create notifier for ASYNC COMP */
		work = kzalloc(sizeof(*work), GFP_NOWAIT);

		if (work) {
			INIT_WORK(&work->work,
				  ecpri_dma_mhi_memcpy_async_wq_cb_ready);

			queue_work(memcpy_ctx->async_wq, &work->work);
		} else {
			DMAERR("Allocation error in workqueue\n");
			ecpri_dma_assert();
		}

		ecpri_dma_mhi_dma_free_pkt(async_pkts[i]->pkt);
	}

	/* There might be more packet to poll, rescheduale tasklet */
	tasklet_schedule(&endp->tasklet);
}

/**
 * ecpri_dma_mhi_alloc_sync_async_endps() - Helper function
 *                                          to allocate endpnts
 */
static int ecpri_dma_mhi_alloc_sync_async_endps(
	struct ecpri_dma_moderation_config* mod_cfg,
	int sync_src_id, int sync_dest_id,
	int async_src_id, int async_dest_id)
{
	int ret = 0;

	ecpri_dma_ctx->endp_ctx[sync_src_id].eventless_endp = true;
	ret = ecpri_dma_alloc_endp(
		sync_src_id, ECPRI_DMA_MHI_MEMCPY_RLEN,
		mod_cfg, false, NULL);
	if (ret != 0) {
		DMAERR("Unable to allocate SYNC_SRC ENDP, endp_id: %d\n",
			sync_src_id);
		goto fail_alloc_sync_src;
	}

	ret = ecpri_dma_alloc_endp(
		sync_dest_id, ECPRI_DMA_MHI_MEMCPY_RLEN,
		mod_cfg, false, NULL);
	if (ret != 0) {
		DMAERR("Unable to allocate SYNC_DEST ENDP, endp_id: %d\n",
			sync_dest_id);
		goto fail_alloc_sync_dest;
	}

	/* Skip the VMs */
	if (async_src_id == ECPRI_DMA_MHI_INVALID_ENDP_ID ||
		async_dest_id == ECPRI_DMA_MHI_INVALID_ENDP_ID)
		return ret;

	ecpri_dma_ctx->endp_ctx[async_src_id].eventless_endp = true;
	ret = ecpri_dma_alloc_endp(
		async_src_id, ECPRI_DMA_MHI_MEMCPY_RLEN,
		mod_cfg, false, NULL);
	if (ret != 0) {
		DMAERR("Unable to allocate ASYNC_SRC ENDP, endp_id: %d\n",
			async_src_id);
		goto fail_alloc_async_src;
	}

	ret = ecpri_dma_alloc_endp(
		async_dest_id, ECPRI_DMA_MHI_MEMCPY_RLEN,
		mod_cfg, false,
		ecpri_dma_mhi_memcpy_async_notify_comp);
	if (ret != 0) {
		DMAERR("Unable to allocate ASYNC_DEST ENDP, endp_id: %d\n",
			async_dest_id);
		goto fail_alloc_async_dest;
	}

	ret = 0;
	goto success;

fail_alloc_async_dest:
	ecpri_dma_dealloc_endp(
		&ecpri_dma_ctx->endp_ctx[async_src_id]);
fail_alloc_async_src:
	ecpri_dma_dealloc_endp(
		&ecpri_dma_ctx->endp_ctx[sync_dest_id]);
fail_alloc_sync_dest:
	ecpri_dma_dealloc_endp(
		&ecpri_dma_ctx->endp_ctx[sync_src_id]);
fail_alloc_sync_src:
success:
	return ret;
}

/**
 * ecpri_dma_mhi_enable_mhi_memcpy_endps() - Helper function
 * to enable endpnts
 *
 */
static int ecpri_dma_mhi_enable_mhi_memcpy_endps(int sync_src_id,
	int sync_dest_id, int async_src_id, int async_dest_id)
{
	int ret = 0;

	ret = ecpri_dma_enable_dma_endp(
		&ecpri_dma_ctx->endp_ctx[sync_src_id]);
	if (ret != 0) {
		DMAERR("Unable to enable SYNC SRC endpoint %d\n",
			sync_src_id);
		ret = -EFAULT;
		goto fail_enable_sync_src;
	}

	ret = ecpri_dma_enable_dma_endp(
		&ecpri_dma_ctx->endp_ctx[sync_dest_id]);
	if (ret != 0) {
		DMAERR("Unable to enable SYNC DEST endpoint %d\n",
			sync_dest_id);
		ret = -EFAULT;
		goto fail_enable_sync_dest;
	}

	/* Skip the VMs */
	if (async_src_id == ECPRI_DMA_MHI_INVALID_ENDP_ID ||
		async_dest_id == ECPRI_DMA_MHI_INVALID_ENDP_ID)
		return ret;

	ret = ecpri_dma_enable_dma_endp(
		&ecpri_dma_ctx->endp_ctx[async_src_id]);
	if (ret != 0) {
		DMAERR("Unable to enable ASYNC SRC endpoint %d\n",
			async_src_id);
		ret = -EFAULT;
		goto fail_enable_async_src;
	}

	ret = ecpri_dma_enable_dma_endp(
		&ecpri_dma_ctx->endp_ctx[async_dest_id]);
	if (ret != 0) {
		DMAERR("Unable to enable ASYNC DEST endpoint %d\n",
			async_dest_id);
		ret = -EFAULT;
		goto fail_enable_async_dest;
	}

	ret = 0;
	goto success;

fail_enable_async_dest:
	ecpri_dma_reset_endp(
		&ecpri_dma_ctx->endp_ctx[async_src_id]);
fail_enable_async_src:
	ecpri_dma_reset_endp(
		&ecpri_dma_ctx->endp_ctx[sync_dest_id]);
fail_enable_sync_dest:
	ecpri_dma_reset_endp(
		&ecpri_dma_ctx->endp_ctx[sync_src_id]);
fail_enable_sync_src:
success:
	return ret;
}

/**
 * ecpri_dma_mhi_start_memcpy_endps() - Helper function to start endpnts
 *
 */
static int ecpri_dma_mhi_start_memcpy_endps(int sync_src_id,
	int sync_dest_id, int async_src_id, int async_dest_id)
{
	int ret = 0;

	ret = ecpri_dma_start_endp(
		&ecpri_dma_ctx->endp_ctx[sync_src_id]);
	if (ret != 0) {
		DMAERR("Unable to start endp, endp_id: %d\n",
			sync_src_id);
		ret = -EFAULT;
		goto fail_start_sync_src;
	}

	ret = ecpri_dma_start_endp(
		&ecpri_dma_ctx->endp_ctx[sync_dest_id]);
	if (ret != 0) {
		DMAERR("Unable to start endp, endp_id: %d\n",
			sync_dest_id);
		ret = -EFAULT;
		goto fail_start_sync_dest;
	}

	/* Skip the VMs */
	if (async_src_id == ECPRI_DMA_MHI_INVALID_ENDP_ID ||
		async_dest_id == ECPRI_DMA_MHI_INVALID_ENDP_ID)
		goto success;

	ret = ecpri_dma_start_endp(
		&ecpri_dma_ctx->endp_ctx[async_src_id]);
	if (ret != 0) {
		DMAERR("Unable to start endp, endp_id: %d\n",
			async_src_id);
		ret = -EFAULT;
		goto fail_start_async_src;
	}

	ret = ecpri_dma_start_endp(
		&ecpri_dma_ctx->endp_ctx[async_dest_id]);
	if (ret != 0) {
		DMAERR("Unable to start endp, endp_id: %d\n",
			async_dest_id);
		ret = -EFAULT;
		goto fail_start_async_dest;
	}

	ret = 0;
	goto success;

fail_start_async_dest:
	ecpri_dma_stop_endp(
		&ecpri_dma_ctx->endp_ctx[async_src_id]);
fail_start_async_src:
	ecpri_dma_stop_endp(
		&ecpri_dma_ctx->endp_ctx[sync_dest_id]);
fail_start_sync_dest:
	ecpri_dma_stop_endp(
		&ecpri_dma_ctx->endp_ctx[sync_src_id]);
fail_start_sync_src:
success:
	return ret;
}

/**
 * ecpri_dma_mhi_set_memcpy_endps_mode() - Helper function to set endpnts mode
 *
 */
static int ecpri_dma_mhi_set_memcpy_endps_mode(int sync_src_id,
	int sync_dest_id, int async_src_id, int async_dest_id)
{
	int ret = 0;

	ret = ecpri_dma_set_endp_mode(
		&ecpri_dma_ctx->endp_ctx[sync_dest_id],
		ECPRI_DMA_NOTIFY_MODE_POLL);
	if (ret != 0) {
		DMAERR("Unable to set mode, ENDP ID:%d, mode: %d, \n",
			sync_dest_id,
			ECPRI_DMA_NOTIFY_MODE_POLL);
		return ret;
	}

	/* Skip the VMs */
	if (async_src_id == ECPRI_DMA_MHI_INVALID_ENDP_ID ||
		async_dest_id == ECPRI_DMA_MHI_INVALID_ENDP_ID)
		return ret;

	/* Note: Async channels are by default in IRQ mode, no need to set  */

	return ret;
}

/**
 * ecpri_dma_mhi_stop_memcpy_endps() - Helper function to stop endpnts
 *
 */
static int ecpri_dma_mhi_stop_memcpy_endps(int sync_src_id,
	int sync_dest_id, int async_src_id, int async_dest_id)
{
	int ret = 0;

	ret = ecpri_dma_stop_endp(
		&ecpri_dma_ctx->endp_ctx[sync_src_id]);
	if (ret != 0) {
		DMAERR("Unable to stop endp, endp_id: %d\n",
			sync_src_id);
		return ret;
	}


	ret = ecpri_dma_stop_endp(
		&ecpri_dma_ctx->endp_ctx[sync_dest_id]);
	if (ret != 0) {
		DMAERR("Unable to stop endp, endp_id: %d\n",
			sync_dest_id);
		return ret;
	}

	/* Skip the VMs */
	if (async_src_id == ECPRI_DMA_MHI_INVALID_ENDP_ID ||
		async_dest_id == ECPRI_DMA_MHI_INVALID_ENDP_ID)
		return ret;

	ret = ecpri_dma_stop_endp(
		&ecpri_dma_ctx->endp_ctx[async_src_id]);
	if (ret != 0) {
		DMAERR("Unable to stop endp, endp_id: %d\n",
			async_src_id);
		return ret;
	}

	ret = ecpri_dma_stop_endp(
		&ecpri_dma_ctx->endp_ctx[async_dest_id]);
	if (ret != 0) {
		DMAERR("Unable to stop endp, endp_id: %d\n",
			async_dest_id);
		return ret;
	}

	return ret;
}

/**
 * ecpri_dma_mhi_disable_memcpy_endps() - Helper function to disable endpnts
 *
 */
static int ecpri_dma_mhi_disable_memcpy_endps(int sync_src_id,
	int sync_dest_id, int async_src_id, int async_dest_id)
{
	int ret = 0;

	ret = ecpri_dma_disable_dma_endp(
		&ecpri_dma_ctx->endp_ctx[sync_src_id]);
	if (ret != 0) {
		DMAERR("Unable to disable endpoint %d\n",
			sync_src_id);
		return ret;
	}

	ret = ecpri_dma_disable_dma_endp(
		&ecpri_dma_ctx->endp_ctx[sync_dest_id]);
	if (ret != 0) {
		DMAERR("Unable to disable endpoint %d\n",
			sync_dest_id);
		return ret;
	}

	/* Skip the VMs */
	if (async_src_id == ECPRI_DMA_MHI_INVALID_ENDP_ID ||
		async_dest_id == ECPRI_DMA_MHI_INVALID_ENDP_ID)
		return ret;

	ret = ecpri_dma_disable_dma_endp(
		&ecpri_dma_ctx->endp_ctx[async_src_id]);
	if (ret != 0) {
		DMAERR("Unable to disable endpoint %d\n",
			async_src_id);
		return ret;
	}

	ret = ecpri_dma_disable_dma_endp(
		&ecpri_dma_ctx->endp_ctx[async_dest_id]);
	if (ret != 0) {
		DMAERR("Unable to disable endpoint %d\n",
			async_dest_id);
		return ret;
	}

	return ret;
}

/**
 * ecpri_dma_mhi_dealloc_memcpy_endps() - Helper function to dealloc endpnts
 *
 */
static int ecpri_dma_mhi_dealloc_memcpy_endps(int sync_src_id,
	int sync_dest_id, int async_src_id, int async_dest_id)
{
	int ret = 0;

	ret = ecpri_dma_dealloc_endp(
		&ecpri_dma_ctx->endp_ctx[sync_src_id]);
	if (ret != 0) {
		DMAERR("Unable to deallocate endp, endp_id: %d\n",
			sync_src_id);
		return ret;
	}

	ret = ecpri_dma_dealloc_endp(
		&ecpri_dma_ctx->endp_ctx[sync_dest_id]);
	if (ret != 0) {
		DMAERR("Unable to deallocate endp, endp_id: %d\n",
			sync_dest_id);
		return ret;
	}

	/* Skip the VMs */
	if (async_src_id == ECPRI_DMA_MHI_INVALID_ENDP_ID ||
		async_dest_id == ECPRI_DMA_MHI_INVALID_ENDP_ID)
		return ret;

	ret = ecpri_dma_dealloc_endp(
		&ecpri_dma_ctx->endp_ctx[async_src_id]);
	if (ret != 0) {
		DMAERR("Unable to deallocate endp, endp_id: %d\n",
			async_src_id);
		return ret;
	}

	ret = ecpri_dma_dealloc_endp(
		&ecpri_dma_ctx->endp_ctx[async_dest_id]);
	if (ret != 0) {
		DMAERR("Unable to deallocate endp, endp_id: %d\n",
			async_dest_id);
		return ret;
	}

	return ret;
}

/**
 * ecpri_dma_mhi_reset_memcpy_endps() - Helper function to reset endpnts
 *
 */
static int ecpri_dma_mhi_reset_memcpy_endps(int sync_src_id,
	int sync_dest_id, int async_src_id, int async_dest_id)
{
	int ret = 0;
	struct ecpri_dma_endp_context* endp_cfg = NULL;

	endp_cfg = &ecpri_dma_ctx->endp_ctx[sync_src_id];
	ret = ecpri_dma_reset_endp(endp_cfg);
	if (ret != GSI_STATUS_SUCCESS) {
		DMAERR("Unable to reset endp, endp_id: %d\n",
			sync_src_id);
		return ret;
	}

	endp_cfg = &ecpri_dma_ctx->endp_ctx[sync_dest_id];
	ret = ecpri_dma_reset_endp(endp_cfg);
	if (ret != GSI_STATUS_SUCCESS) {
		DMAERR("Unable to reset endp, endp_id: %d\n",
			sync_dest_id);
		return ret;
	}

	/* Skip the VMs */
	if (async_src_id == ECPRI_DMA_MHI_INVALID_ENDP_ID ||
		async_dest_id == ECPRI_DMA_MHI_INVALID_ENDP_ID)
		return ret;

	endp_cfg = &ecpri_dma_ctx->endp_ctx[async_src_id];
	ret = ecpri_dma_reset_endp(endp_cfg);
	if (ret != GSI_STATUS_SUCCESS) {
		DMAERR("Unable to reset endp, endp_id: %d\n",
			async_src_id);
		return ret;
	}

	endp_cfg = &ecpri_dma_ctx->endp_ctx[async_dest_id];
	ret = ecpri_dma_reset_endp(endp_cfg);
	if (ret != GSI_STATUS_SUCCESS) {
		DMAERR("Unable to reset endp, endp_id: %d\n",
			async_dest_id);
		return ret;
	}

	return ret;
}

/**
 * ecpri_dma_mhi_memcpy_init() - Initialize memory copy DMA.
 * @function: function parameters
 *
 * This function initialize all memory copy DMA internal data and connect dma:
 *	MEMCPY_DMA_SYNC_PROD ->MEMCPY_DMA_SYNC_CONS
 *	MEMCPY_DMA_ASYNC_PROD->MEMCPY_DMA_SYNC_CONS
 *
 * Can be executed several times (re-entrant)
 *
 * NOTE: Init memcpy API for SW ENDPs.
 *		 These ENDPs are managed by the A55 DMA driver and are not
 *		 HW accelerated.
 *		 VMs \ PF manages these ENDPs only via the PCIe and
 *		 A55 DMA drivers.
 *
 * Return codes: 0: success
 *		-EFAULT: Mismatch between context existence and init ref_cnt
 *		-EINVAL: HW driver is not initialized
 *		-ENOMEM: allocating memory error
 *		-EPERM: ENDP connection failed
 */
static int ecpri_dma_mhi_memcpy_init(
	struct mhi_dma_function_params function)
{
	int idx;
	int ret;
	int sync_src_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	int sync_dest_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	int async_src_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	int async_dest_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	bool ready = false;
	struct ecpri_dma_moderation_config mod_cfg;
	struct ecpri_dma_mhi_memcpy_context* memcpy_ctx = NULL;

	if (function.function_type != MHI_DMA_FUNCTION_TYPE_PHYSICAL &&
		function.vf_id > ECPRI_DMA_MHI_MAX_VIRTUAL_FUNCTIONS_ID) {
		DMAERR("Illegal function id, vf_id: %d\n",
			function.vf_id);
		return -EINVAL;
	}

	/* Check DMA driver state */
	mutex_lock(&ecpri_dma_ctx->lock);
	ready = ecpri_dma_is_ready();
	mutex_unlock(&ecpri_dma_ctx->lock);

	if (!ready) {
		DMAERR("DMA driver is not ready\n");
		return -EPERM;
	}

	/* Get the index of VM/PF */
	ret = ecpri_dma_mhi_get_function_context_index(
		function, &idx);
	if (ret != 0) {
		DMAERR("Function params are invalid,"
			"function type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		return -EINVAL;
	}

	if (!ecpri_dma_mhi_memcpy_ctx[idx]) {
		ecpri_dma_mhi_memcpy_ctx[idx] = kzalloc(
			sizeof(*ecpri_dma_mhi_memcpy_ctx[0]),
			GFP_KERNEL);
		if (!ecpri_dma_mhi_memcpy_ctx[idx]) {
			ret = -EFAULT;
			goto fail_alloc_ctx;
		}
	}
	else {
		return 0;
	}
	memcpy_ctx = ecpri_dma_mhi_memcpy_ctx[idx];

	spin_lock_init(&memcpy_ctx->lock);
	spin_lock_init(&memcpy_ctx->sync_lock);
	spin_lock_init(&memcpy_ctx->async_lock);
	memcpy_ctx->async_wq = create_singlethread_workqueue(
		"ECPRI_DMA_MHI_MEMCPY_ASYNC_WQ");
	init_completion(&memcpy_ctx->done);
	memcpy_ctx->destroy_pending = false;
	atomic_set(&memcpy_ctx->ref_count, 0);
	atomic_set(&memcpy_ctx->async_pending, 0);
	atomic_set(&memcpy_ctx->sync_pending, 0);
	atomic_set(&memcpy_ctx->sync_total, 0);
	atomic_set(&memcpy_ctx->async_total, 0);

	INIT_LIST_HEAD(&memcpy_ctx->cbs_list);

	memcpy_ctx->xfer_wrapper_cache = kmem_cache_create(
		"DMA_MEMCPY_XFER_WRAPPER",
		sizeof(struct ecpri_dma_mhi_xfer_wrapper), 0, 0, NULL);
	if (!memcpy_ctx->xfer_wrapper_cache) {
		DMAERR("MHI DMA xfer wrapper cache create failed\n");
		ret = -ENOMEM;
		goto fail_xwrapper;
	}

	mod_cfg.moderation_counter_threshold = 1;
	mod_cfg.moderation_timer_threshold = 0;

	sync_src_id =
		ecpri_dma_mhi_function_endp_dt[idx].sync_src_id;
	sync_dest_id =
		ecpri_dma_mhi_function_endp_dt[idx].sync_dest_id;
	async_src_id =
		ecpri_dma_mhi_function_endp_dt[idx].async_src_id;
	async_dest_id =
		ecpri_dma_mhi_function_endp_dt[idx].async_dest_id;

	/* Allocate endpoints */
	ret = ecpri_dma_mhi_alloc_sync_async_endps(&mod_cfg, sync_src_id,
		sync_dest_id, async_src_id, async_dest_id);
	if (ret != 0) {
		DMAERR("Unable to allocate endp\n");
		goto fail_alloc_endp;
	}

	/* Enable endpoints */
	ret = ecpri_dma_mhi_enable_mhi_memcpy_endps(sync_src_id,
		sync_dest_id, async_src_id, async_dest_id);
	if (ret != 0) {
		DMAERR("Unable to enable endp\n");
		goto fail_enable_endp;
	}

	/* Set endpoints */
	memcpy_ctx->sync_dest_endp = &ecpri_dma_ctx->endp_ctx[sync_dest_id];
	memcpy_ctx->sync_src_endp = &ecpri_dma_ctx->endp_ctx[sync_src_id];

	if (async_src_id != ECPRI_DMA_MHI_INVALID_ENDP_ID &&
		async_dest_id != ECPRI_DMA_MHI_INVALID_ENDP_ID)
	{
		memcpy_ctx->async_dest_endp = &ecpri_dma_ctx->endp_ctx[async_dest_id];
		memcpy_ctx->async_src_endp = &ecpri_dma_ctx->endp_ctx[async_src_id];
	}

	// TODO: init_memcpy_debugfs()

	ret = 0;
	goto success;

fail_enable_endp:
	/* Dealloc endpoints */
	ret = ecpri_dma_mhi_dealloc_memcpy_endps(sync_src_id,
		sync_dest_id, async_src_id, async_dest_id);
	if (ret != 0) {
		DMAERR("Unable to deallocate endps\n");
		ecpri_dma_assert();
	}
fail_alloc_endp:
	kfree(ecpri_dma_mhi_memcpy_ctx[idx]);
	ecpri_dma_mhi_memcpy_ctx[idx] = NULL;
success:
fail_xwrapper:
fail_alloc_ctx:
	return ret;
}

static void ecpri_dma_mhi_memcpy_destroy(
	struct mhi_dma_function_params function)
{
	int ret;
	int idx;
	int sync_src_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	int sync_dest_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	int async_src_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	int async_dest_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	struct ecpri_dma_mhi_memcpy_context* memcpy_ctx = NULL;

	/* Get the index of VM/PF */
	ret = ecpri_dma_mhi_get_function_context_index(
		function, &idx);
	if (ret != 0) {
		DMAERR("Function params are invalid,"
			"function type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		return;
	}

	sync_src_id =
		ecpri_dma_mhi_function_endp_dt[idx].sync_src_id;
	sync_dest_id =
		ecpri_dma_mhi_function_endp_dt[idx].sync_dest_id;
	async_src_id =
		ecpri_dma_mhi_function_endp_dt[idx].async_src_id;
	async_dest_id =
		ecpri_dma_mhi_function_endp_dt[idx].async_dest_id;

	memcpy_ctx = ecpri_dma_mhi_memcpy_ctx[idx];
	if (!memcpy_ctx) {
		DMAERR("Memcpy context for IDX %d uninitialized\n",
			idx);
		return;
	}

	if (!ecpri_dma_mhi_check_destroy_pending(memcpy_ctx)) {
		DMAERR("Memcpy destroy is not in progress\n");
		return;
	}

	/* Reset endpoints */
	ret = ecpri_dma_mhi_reset_memcpy_endps(sync_src_id,
		sync_dest_id, async_src_id, async_dest_id);
	if (ret != 0) {
		DMAERR("Unable to reset endps\n");
		ecpri_dma_assert();
	}

	/* Disable endpoints */
	ret = ecpri_dma_mhi_disable_memcpy_endps(sync_src_id,
		sync_dest_id, async_src_id, async_dest_id);
	if (ret != 0) {
		DMAERR("Unable to disable GSI endps\n");
		ecpri_dma_assert();
	}

	/* Dealloc endpoints */
	ret = ecpri_dma_mhi_dealloc_memcpy_endps(sync_src_id,
		sync_dest_id, async_src_id, async_dest_id);
	if (ret != 0) {
		DMAERR("Unable to deallocate endps\n");
		ecpri_dma_assert();
	}

	/* Reset variables */
	memcpy_ctx->destroy_pending = false;
	atomic_set(&memcpy_ctx->ref_count, 0);
	atomic_set(&memcpy_ctx->async_pending, 0);
	atomic_set(&memcpy_ctx->sync_pending, 0);
	atomic_set(&memcpy_ctx->sync_total, 0);
	atomic_set(&memcpy_ctx->async_total, 0);

	kmem_cache_destroy(memcpy_ctx->xfer_wrapper_cache);

	/* Reset endpoints */
	memcpy_ctx->sync_dest_endp = NULL;
	memcpy_ctx->sync_src_endp = NULL;

	if (async_src_id != ECPRI_DMA_MHI_INVALID_ENDP_ID &&
		async_dest_id != ECPRI_DMA_MHI_INVALID_ENDP_ID)
	{
		memcpy_ctx->async_dest_endp = NULL;
		memcpy_ctx->async_src_endp = NULL;
	}

	kfree(ecpri_dma_mhi_memcpy_ctx[idx]);
	ecpri_dma_mhi_memcpy_ctx[idx] = NULL;
}

/**
* ecpri_dma_mhi_dma_sync_memcpy()- Perform synchronous
* memcpy using DMA.
*
* @dest: physical address to store the copied data.
* @src: physical address of the source data to copy.
* @len: number of bytes to copy.
* @function: function parameters
*
* Return codes:    0: success
*		            -EINVAL: invalid params
*		            -EPERM: operation not permitted as dma isn't
*		            	    enable or initialized
*		            -gsi_status : on GSI failures
*		            -EFAULT: other
*/
static int ecpri_dma_mhi_dma_sync_memcpy(
	u64 dest, u64 src, int len,
	struct mhi_dma_function_params function)
{
	int idx;
	int ret;
	u32 actual_num;
	unsigned long flags;

	struct ecpri_dma_pkt** pkts_dest = NULL;
	struct ecpri_dma_pkt** pkts_src = NULL;
	struct ecpri_dma_mhi_memcpy_context* memcpy_ctx = NULL;
	struct ecpri_dma_pkt_completion_wrapper* pkt_wrapper = NULL;

	if ((max(src, dest) - min(src, dest)) < len) {
		DMAERR("Invalid addresses - overlapping buffers\n");
		return -EINVAL;
	}

	if (len > ECPRI_DMA_MHI_DMA_MAX_PKT_SZ || len <= 0) {
		DMAERR("Invalid len, %d\n", len);
		return -EINVAL;
	}

	/* Get the index of VM/PF */
	ret = ecpri_dma_mhi_get_function_context_index(
		function, &idx);
	if (ret != 0) {
		DMAERR("Function params are invalid,"
			"function type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		return -EINVAL;
	}

	memcpy_ctx = ecpri_dma_mhi_memcpy_ctx[idx];
	if (!memcpy_ctx) {
		DMAERR("Memcpy context is not initialized\n");
		return -EPERM;
	}

	if (ecpri_dma_mhi_check_destroy_pending(memcpy_ctx) == true) {
		DMAERR("Memcpy destroy in progress\n");
		return -EFAULT;
	}

	if (atomic_read(&memcpy_ctx->ref_count) == 0) {
		DMAERR("Reference count is equal to zero\n");
		return -EPERM;
	}

	/* Only single SYNC transfer allowed */
	spin_lock_irqsave(&memcpy_ctx->sync_lock, flags);
	while (atomic_read(&memcpy_ctx->sync_pending) > 0) {
		spin_unlock_irqrestore(&memcpy_ctx->sync_lock, flags);
		usleep_range(ECPRI_DMA_MHI_POLLING_MIN_SLEEP_RX,
			ECPRI_DMA_MHI_POLLING_MAX_SLEEP_RX);
		spin_lock_irqsave(&memcpy_ctx->sync_lock, flags);
	}

	atomic_inc(&memcpy_ctx->sync_pending);
	spin_unlock_irqrestore(&memcpy_ctx->sync_lock, flags);

	/* Allocate packets */
	ret = ecpri_dma_mhi_dma_alloc_pkt(dest, len, &pkts_dest);
	if (ret != 0) {
		DMAERR("Unable to allocate packets for destination\n");
		ret = -EPERM;
		goto fail_dest_alloc;
	}

	ret = ecpri_dma_mhi_dma_alloc_pkt(src, len, &pkts_src);
	if (ret != 0) {
		DMAERR("Unable to allocate packets for source\n");
		ret = -EPERM;
		goto fail_src_alloc;
	}

	/* Transmit packets */
	ret = ecpri_dma_dp_transmit(memcpy_ctx->sync_dest_endp,
		pkts_dest, 1, true);
	if (ret != 0) {
		DMAERR("Unable to transmit dest\n");
		ret = -EPERM;
		goto fail_transmit;
	}

	ret = ecpri_dma_dp_transmit(memcpy_ctx->sync_src_endp,
		pkts_src, 1, true);
	if (ret != 0) {
		DMAERR("Unable to transmit src\n");
		ret = -EPERM;
		goto fail_transmit;
	}

	actual_num = 0;
	memcpy_ctx->loop_counter = 0;
	pkt_wrapper = kzalloc(sizeof(struct ecpri_dma_pkt_completion_wrapper),
		GFP_KERNEL);
	if (!pkt_wrapper) {
		DMAERR("Unable to allocate pkt_wrapper\n");
		ret = -ENOMEM;
		goto fail_alloc_wrapper;
	}

	while (actual_num == 0)
	{
		memcpy_ctx->loop_counter++;
		ret = ecpri_dma_dp_rx_poll(memcpy_ctx->sync_dest_endp, 1,
			&pkt_wrapper, &actual_num);
		if (ret != 0) {
			DMAERR("Unable to poll\n");
			ret = -EPERM;
			goto fail_poll_rx;
		}
	}
	memcpy_ctx->loop_counter = 0;

	spin_lock_irqsave(&memcpy_ctx->sync_lock, flags);
	atomic_inc(&memcpy_ctx->sync_total);
	atomic_dec(&memcpy_ctx->sync_pending);
	spin_unlock_irqrestore(&memcpy_ctx->sync_lock, flags);

	ret = 0;
	goto success;

fail_poll_rx:
	kfree(pkt_wrapper);
	atomic_dec(&memcpy_ctx->sync_pending);
fail_alloc_wrapper:
fail_transmit:
success:
	ecpri_dma_mhi_dma_free_pkt(pkts_src[0]);
fail_src_alloc:
	ecpri_dma_mhi_dma_free_pkt(pkts_dest[0]);
fail_dest_alloc:
	return ret;
}

static int ecpri_dma_mhi_dma_async_memcpy_vm_handling(
	u64 dest, u64 src, int len,
	struct mhi_dma_function_params function,
	void (*user_cb)(void* user1),
	void* user_param)
{
	int ret = 0, idx = 0;
	struct ecpri_dma_mhi_async_wq_work_type* work = NULL;
	struct ecpri_dma_mhi_xfer_wrapper* xfer_descr = NULL;
	struct ecpri_dma_mhi_memcpy_context* memcpy_ctx = NULL;

	/* Get the index of VM/PF */
	ret = ecpri_dma_mhi_get_function_context_index(
		function, &idx);
	if (ret != 0) {
		DMAERR("Function params are invalid,"
			"function type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		return -EINVAL;
	}

	memcpy_ctx = ecpri_dma_mhi_memcpy_ctx[idx];
	if (!memcpy_ctx) {
		DMAERR("Memcpy context is not initialized\n");
		return -EPERM;
	}

	if (ecpri_dma_mhi_check_destroy_pending(memcpy_ctx) == true) {
		DMAERR("Memcpy destroy in progress\n");
		return -EFAULT;
	}

	xfer_descr = kmem_cache_zalloc(
		memcpy_ctx->xfer_wrapper_cache,
		GFP_KERNEL);
	if (!xfer_descr) {
		DMAERR("Allocation error\n");
		return -ENOMEM;
	}

	xfer_descr->user_cb = user_cb;
	xfer_descr->user_data = user_param;
	xfer_descr->function = function;

	/* Create notifier for ASYNC COMP */
	work = kzalloc(sizeof(*work), GFP_KERNEL);

	if (!work) {
		DMAERR("Allocation error\n");
		return -ENOMEM;
	}

	ret = ecpri_dma_mhi_dma_sync_memcpy(dest, src, len, function);
	if (ret)
	{
		DMAERR("SYNC transfer failed, ret = %d\n", ret);
		kmem_cache_free(memcpy_ctx->xfer_wrapper_cache, xfer_descr);
		kfree(work);
		return ret;
	}

	INIT_WORK(&work->work,
		ecpri_dma_mhi_memcpy_async_wq_cb_ready_vms);
	work->xfer_desc = xfer_descr;

	queue_work(memcpy_ctx->async_wq, &work->work);

	return 0;
}

/**
 * ecpri_dma_mhi_dma_async_memcpy()- Perform asynchronous
 * memcpy using DMA.
 *
 * @dest: physical address to store the copied data.
 * @src: physical address of the source data to copy.
 * @len: number of bytes to copy.
 * @user_cb: callback function to notify the client when the copy was done.
 * @user_param: cookie for user_cb.
 *
 * Return codes:    0: success
 *		            -EINVAL: invalid params
 *		            -EPERM: operation not permitted as dma isn't
 *		            	    enable or initialized
 *		            -gsi_status : on GSI failures
 *		            -EFAULT: descr fifo is full.
 */
static int ecpri_dma_mhi_dma_async_memcpy(
	u64 dest, u64 src, int len,
	struct mhi_dma_function_params function,
	void (*user_cb)(void* user1),
	void* user_param)
{
	int idx;
	int ret;
	unsigned long flags;
	struct ecpri_dma_pkt** pkt_dest = NULL;
	struct ecpri_dma_pkt** pkt_src = NULL;
	struct ecpri_dma_mhi_memcpy_context* memcpy_ctx = NULL;
	struct ecpri_dma_mhi_xfer_wrapper* xfer_descr = NULL;

	if ((max(src, dest) - min(src, dest)) < len) {
		DMAERR("Invalid addresses - overlapping buffers\n");
		return -EINVAL;
	}
	if (len > ECPRI_DMA_MHI_DMA_MAX_PKT_SZ || len <= 0) {
		DMAERR("Invalid len, %d\n", len);
		return -EINVAL;
	}

	/* Get the index of VM/PF */
	ret = ecpri_dma_mhi_get_function_context_index(
		function, &idx);
	if (ret != 0) {
		DMAERR("Function params are invalid,"
			"function type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		return -EINVAL;
	}

	memcpy_ctx = ecpri_dma_mhi_memcpy_ctx[idx];
	if (!memcpy_ctx) {
		DMAERR("Memcpy context is not initialized\n");
		return -EPERM;
	}

	if (ecpri_dma_mhi_check_destroy_pending(memcpy_ctx) == true) {
		DMAERR("Memcpy destroy in progress\n");
		return -EFAULT;
	}

	if (!user_cb) {
		DMAERR("null pointer: user_cb\n");
		return -EINVAL;
	}

	if (function.function_type ==
		MHI_DMA_FUNCTION_TYPE_VIRTUAL) {
		DMADBG("No ASYNC for VM, using SYNC instead\n");
		return ecpri_dma_mhi_dma_async_memcpy_vm_handling(dest, src, len,
			function, user_cb, user_param);
	}

	if (atomic_read(&memcpy_ctx->ref_count) == 0) {
		DMAERR("Reference count is equal to zero\n");
		return -EPERM;
	}

	xfer_descr = kmem_cache_zalloc(
		memcpy_ctx->xfer_wrapper_cache,
		GFP_KERNEL);
	if (!xfer_descr) {
		return -ENOMEM;
	}

	xfer_descr->user_cb = user_cb;
	xfer_descr->user_data = user_param;

	ret = ecpri_dma_mhi_dma_alloc_pkt(dest, len, &pkt_dest);
	if (ret != 0) {
		DMAERR("Unable to allocate packets for destination\n");
		return -ENOMEM;
	}

	ret = ecpri_dma_mhi_dma_alloc_pkt(src, len, &pkt_src);
	if (ret != 0) {
		ecpri_dma_mhi_dma_free_pkt(pkt_dest[0]);
		DMAERR("Unable to allocate packets for source\n");
		return -ENOMEM;
	}

	spin_lock_irqsave(
		&memcpy_ctx->async_lock,
		flags);

	list_add_tail(&xfer_descr->link, &memcpy_ctx->cbs_list);
	atomic_inc(&memcpy_ctx->async_pending);

	ret = ecpri_dma_dp_transmit(
		memcpy_ctx->async_dest_endp,
		pkt_dest, 1, true);
	if (ret != 0) {
		DMAERR("Unable to transmit\n");
		ret = -EPERM;
		goto fail_dest_transmit;
	}
	ret = ecpri_dma_dp_transmit(memcpy_ctx->async_src_endp,
		pkt_src, 1, true);
	if (ret != 0) {
		DMAERR("Unable to transmit\n");
		ecpri_dma_assert();
	}

	spin_unlock_irqrestore(&memcpy_ctx->async_lock, flags);
	return 0;

fail_dest_transmit:
	xfer_descr = list_last_entry(
		&memcpy_ctx->cbs_list,
		struct ecpri_dma_mhi_xfer_wrapper,
		link);
	list_del(&xfer_descr->link);
	atomic_dec(&memcpy_ctx->async_pending);
	spin_unlock_irqrestore(&memcpy_ctx->async_lock, flags);

	kmem_cache_free(memcpy_ctx->xfer_wrapper_cache,
		xfer_descr);
	ecpri_dma_mhi_dma_free_pkt(pkt_dest[0]);
	ecpri_dma_mhi_dma_free_pkt(pkt_src[0]);

	return ret;
}

/**
 * ecpri_dma_mhi_dma_memcpy_disable()- Unvote for HW clocks.
 *
 * @function: function parameters
 *
 * enter to power save mode.
 *
 * Return codes: 0: success
 *		-EINVAL: HW DMA is not initialized
 *		-EPERM: Operation not permitted as mhi_dma is already diabled
 *		-EFAULT: can not disable mhi_dma as there are pending memcopy works
 */
static int ecpri_dma_mhi_dma_memcpy_disable(
	struct mhi_dma_function_params function)
{
	int idx;
	int ret = 0;
	int sync_src_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	int sync_dest_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	int async_src_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	int async_dest_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	unsigned long flags;
	unsigned long flags_sync;
	unsigned long flags_async;

	struct ecpri_dma_mhi_memcpy_context* memcpy_ctx;

	/* Get the index of VM/PF */
	ret = ecpri_dma_mhi_get_function_context_index(
		function, &idx);
	if (ret != 0) {
		DMAERR("Function params are invalid,"
			"function type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		return -EINVAL;
	}

	memcpy_ctx = ecpri_dma_mhi_memcpy_ctx[idx];
	if (!memcpy_ctx) {
		DMAERR("Memcpy context is not initialized\n");
		return -EPERM;
	}

	if (ecpri_dma_mhi_check_destroy_pending(memcpy_ctx) == true) {
		DMAERR("Memcpy destroy in progress\n");
		return -EFAULT;
	}

	sync_src_id =
		ecpri_dma_mhi_function_endp_dt[idx].sync_src_id;
	sync_dest_id =
		ecpri_dma_mhi_function_endp_dt[idx].sync_dest_id;
	async_src_id =
		ecpri_dma_mhi_function_endp_dt[idx].async_src_id;
	async_dest_id =
		ecpri_dma_mhi_function_endp_dt[idx].async_dest_id;

	spin_lock_irqsave(&memcpy_ctx->lock,
		flags);

	if (atomic_read(&memcpy_ctx->ref_count) > 1) {
		atomic_dec(&memcpy_ctx->ref_count);
		spin_unlock_irqrestore(
			&memcpy_ctx->lock,
			flags);
		return 0;
	}

	if (atomic_read(&memcpy_ctx->ref_count) == 0) {
		spin_unlock_irqrestore(
			&memcpy_ctx->lock,
			flags);
		return 0;
	}

	spin_lock_irqsave(
		&memcpy_ctx->async_lock,
		flags_async);
	if (atomic_read(&memcpy_ctx->async_pending) > 0) {
		spin_unlock_irqrestore(
			&memcpy_ctx->async_lock,
			flags_async);
		spin_unlock_irqrestore(
			&memcpy_ctx->lock,
			flags);
		return -EFAULT;
	}

	spin_lock_irqsave(
		&memcpy_ctx->sync_lock,
		flags_sync);
	if (atomic_read(&memcpy_ctx->sync_pending) > 0) {
		spin_unlock_irqrestore(
			&memcpy_ctx->sync_lock,
			flags_sync);
		spin_unlock_irqrestore(
			&memcpy_ctx->async_lock,
			flags_async);
		spin_unlock_irqrestore(
			&memcpy_ctx->lock,
			flags);
		return -EFAULT;
	}

	memcpy_ctx->destroy_pending = true;

	spin_unlock_irqrestore(&memcpy_ctx->sync_lock,
		flags_sync);
	spin_unlock_irqrestore(&memcpy_ctx->async_lock,
		flags_async);
	spin_unlock_irqrestore(&memcpy_ctx->lock,
		flags);

	/* Stop endpoints */
	ret = ecpri_dma_mhi_stop_memcpy_endps(sync_src_id,
		sync_dest_id, async_src_id, async_dest_id);
	if (ret != 0) {
		DMAERR("Unable to stop endp\n");
		return ret;
	}

	return ret;
}

/**
 * ecpri_dma_mhi_set_state() - Set the state of the MHI client
 * @ctx: MHI client context
 * @new_state: state to be changed to
 *
 * Return codes: 0: success
 *		-EPERM: failed to set state
 */
static int ecpri_dma_mhi_set_state(
	struct ecpri_dma_mhi_client_context* ctx,
	enum ecpri_dma_mhi_state new_state)
{
	int ret = -EPERM;
	unsigned long flags;

	DMADBG("Current state: %s\n",
		ECPRI_DMA_MHI_STATE_STR(ctx->state));
	spin_lock_irqsave(&ctx->lock, flags);

	if (ctx->state == ECPRI_DMA_MHI_STATE_INITIALIZED &&
		new_state == ECPRI_DMA_MHI_STATE_READY) {
		ctx->state = ECPRI_DMA_MHI_STATE_READY;
		ret = 0;
	}
	else if (ctx->state == ECPRI_DMA_MHI_STATE_READY &&
		new_state == ECPRI_DMA_MHI_STATE_STARTED) {
		ctx->state = ECPRI_DMA_MHI_STATE_STARTED;
		ret = 0;
	}
	else {
		DMAERR("Invalid state, current state: %d, new state: %d\n",
			ctx->state, new_state);
	}

	spin_unlock_irqrestore(&ctx->lock, flags);
	return ret;
}

/**
* mhi_dma_memcpy_enable() - Vote for HW clocks.
*
* @function: function parameters
*
* Return codes: 0: success
*	-EINVAL: HW DMA is not initialized
*/
static int ecpri_dma_mhi_dma_memcpy_enable(
	struct mhi_dma_function_params function)
{
	int idx;
	int ret = 0;
	int sync_src_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	int sync_dest_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	int async_src_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	int async_dest_id = ECPRI_DMA_MHI_INVALID_ENDP_ID;
	struct ecpri_dma_mhi_memcpy_context* memcpy_ctx;

	/* Check function params are valid */
	ret = ecpri_dma_mhi_get_function_context_index(
		function, &idx);
	if (ret != 0) {
		DMAERR("Function params are invalid,"
			"function type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		return -EINVAL;
	}

	if (!ecpri_dma_mhi_memcpy_ctx[idx]) {
		DMAERR("Memcpy context for IDX %d is not initialized\n",
			idx);
		return -EINVAL;
	}

	memcpy_ctx = ecpri_dma_mhi_memcpy_ctx[idx];

	if (ecpri_dma_mhi_check_destroy_pending(memcpy_ctx) == true) {
		DMAERR("Memcpy destroy in progress\n");
		return -EFAULT;
	}

	if (atomic_read(&memcpy_ctx->ref_count)) {
		atomic_inc(&memcpy_ctx->ref_count);
		return 0;
	}

	sync_src_id =
		ecpri_dma_mhi_function_endp_dt[idx].sync_src_id;
	sync_dest_id =
		ecpri_dma_mhi_function_endp_dt[idx].sync_dest_id;
	async_src_id =
		ecpri_dma_mhi_function_endp_dt[idx].async_src_id;
	async_dest_id =
		ecpri_dma_mhi_function_endp_dt[idx].async_dest_id;

	atomic_inc(&memcpy_ctx->ref_count);

	/* Start endpoints */
	ret = ecpri_dma_mhi_start_memcpy_endps(sync_src_id,
		sync_dest_id, async_src_id, async_dest_id);
	if (ret != 0) {
		DMAERR("Unable to start endp\n");
		ret = -EPERM;
		goto fail_start_endp;
	}

	/* Set endpoints mode */
	ret = ecpri_dma_mhi_set_memcpy_endps_mode(sync_src_id,
		sync_dest_id, async_src_id, async_dest_id);
	if (ret != 0) {
		DMAERR("Unable to set endpoints mode\n");
		goto fail_set_mode;
	}

	ret = 0;
	goto success;

fail_set_mode:
	/* Stop endpoints */
	ret = ecpri_dma_mhi_stop_memcpy_endps(sync_src_id,
		sync_dest_id, async_src_id, async_dest_id);
	if (ret != 0) {
		DMAERR("Unable to stop endps\n");
		ecpri_dma_assert();
	}
	/* Reset endpoints */
	ret = ecpri_dma_mhi_reset_memcpy_endps(sync_src_id,
		sync_dest_id, async_src_id, async_dest_id);
	if (ret != 0) {
		DMAERR("Unable to reset endps\n");
		ecpri_dma_assert();
	}
fail_start_endp:
success:
	return ret;
}

/**
 * ecpri_dma_mhi_client_init() - Register to DMA MHI client
 * @function: function parameters
 * @params: Registration params
 *
 * This function is called by MHI client driver on boot to register DMA MHI
 * Client. When this function returns device can move to READY state.
 * This function is doing the following:
 *	- Initialize MHI DMA internal data structures
 *	- Initialize debugfs
 *	- Initialize SYNC and ASYNC DMA ENDPs
 *
 * Return codes:    0 : success
 *		            negative : error
 */
static int ecpri_dma_mhi_client_init(
	struct mhi_dma_function_params function,
	struct mhi_dma_init_params *params, struct mhi_dma_init_out *out)
{
	int i;
	int idx;
	int ret;
	enum ecpri_dma_ees ee_idx;
	struct ecpri_dma_mhi_wq_work_type* work = NULL;

	if (!params) {
		DMAERR("Null params args\n");
		return -EINVAL;
	}

	if (!params->notify) {
		DMAERR("Null notify function\n");
		return -EINVAL;
	}

	ret = ecpri_dma_mhi_get_function_context_index(
		function, &idx);
	if (ret != 0) {
		DMAERR("Function params are invalid,"
			"function type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		return -EINVAL;
	}

	if (ecpri_dma_mhi_client_ctx[idx] &&
		ecpri_dma_mhi_client_ctx[idx]->state ==
		ECPRI_DMA_MHI_STATE_READY) {
		DMAERR("Context with IDX: %d already in READY state\n",
			idx);
		ret = -EEXIST;
		goto fail_ready_state;
	}
	else if(ecpri_dma_mhi_client_ctx[idx])
	{
		DMAERR("Context with IDX: %d is already initialized\n",
			idx);
		return -EPERM;
	}

	/* Initialize context */
	ecpri_dma_mhi_client_ctx[idx] = kzalloc(
		sizeof(*ecpri_dma_mhi_client_ctx[0]),
		GFP_KERNEL);
	if (!ecpri_dma_mhi_client_ctx[idx]) {
		ret = -ENOMEM;
		goto fail_alloc_ctx;
	}

	ecpri_dma_mhi_client_ctx[idx]->notify_cb =
		params->notify;
	ecpri_dma_mhi_client_ctx[idx]->user_data =
		params->priv;
	ecpri_dma_mhi_client_ctx[idx]->msi_config =
		params->msi;
	ecpri_dma_mhi_client_ctx[idx]->mmio_addr =
		params->mmio_addr;
	ecpri_dma_mhi_client_ctx[idx]->first_ch =
		params->first_ch_idx;
	ecpri_dma_mhi_client_ctx[idx]->dev_scratch
		.mhi_base_chan_idx_valid = true;
	ecpri_dma_mhi_client_ctx[idx]->dev_scratch
		.mhi_base_chan_idx = params->first_ch_idx;
	ecpri_dma_mhi_client_ctx[idx]->first_ev =
		params->first_er_idx;
	ecpri_dma_mhi_client_ctx[idx]->is_over_pcie =
		!!params->assert_bit40;
	ecpri_dma_mhi_client_ctx[idx]->mhi_mstate =
		MHI_DMA_STATE_M0;
	ecpri_dma_mhi_client_ctx[idx]->state =
		ECPRI_DMA_MHI_STATE_INITIALIZED;

	spin_lock_init(&ecpri_dma_mhi_client_ctx[idx]->lock);

	/* Initialize workqueue */
	ecpri_dma_mhi_client_ctx[idx]->wq =
		create_singlethread_workqueue(wq_name[idx]);
	if (!ecpri_dma_mhi_client_ctx[idx]->wq) {
		DMAERR("Failed to create workqueue\n");
		ret = -EFAULT;
		goto fail_create_wq;
	}

	ret = ecpri_dma_mhi_memcpy_init(function);
	if (ret != 0) {
		DMAERR("Failed to init memcpy\n");
		ret = -EFAULT;
		goto fail_memcpy_init;
	}

	ret = ecpri_dma_mhi_dma_memcpy_enable(function);
	if (ret != 0) {
		DMAERR("Failed to enable memcpy\n");
		ret = -EFAULT;
		goto fail_memcpy_enable;
	}

	// TODO: Debugfs init()

	/* Init channels */
	for (i = 0; i < ECPRI_DMA_MHI_MAX_HW_CHANNELS; i++)
	{
		ecpri_dma_mhi_client_ctx[idx]->
			channels[i].valid = false;
		ecpri_dma_mhi_client_ctx[idx]->
			channels[i].state =
			ECPRI_DMA_HW_MHI_CHANNEL_STATE_INVALID;
	}

	idr_init(&ecpri_dma_mhi_client_ctx[idx]->idr);
	spin_lock_init(&ecpri_dma_mhi_client_ctx[idx]->idr_lock);

	/* Set state to READY */
	ret = ecpri_dma_mhi_set_state(ecpri_dma_mhi_client_ctx[idx],
		ECPRI_DMA_MHI_STATE_READY);
	if (ret != 0) {
		DMAERR("Unable to set state, State:%d\n",
			ECPRI_DMA_MHI_STATE_READY);
		ret = -EFAULT;
		goto fail_set_state;
	}

	ret = ecpri_dma_mhi_get_ee_index(function, &ee_idx);
	if (ret != 0) {
		DMAERR("Unable to translate VF/PF to EE index\n");
		return -EINVAL;
	}

	/* Fill out param */
	ecpri_dma_mhi_get_l2_ch_bitmap(ee_idx, idx, &out->ch_db_fwd_msk);
	out->ev_db_fwd_msk = out->ch_db_fwd_msk;
	out->ch_db_fwd_base = gsihal_get_reg_nk_addr(
		GSI_EE_n_GSI_CH_k_DOORBELL_0, ee_idx, 0);
	out->ev_db_fwd_base =gsihal_get_reg_nk_addr(
		GSI_EE_n_EV_CH_k_DOORBELL_0, ee_idx, 0);

	/* Create notifier for driver ready */
	work = kzalloc(sizeof(*work), GFP_KERNEL);

	if (work) {
		INIT_WORK(&work->work,
			  ecpri_dma_mhi_wq_notify_ready);
		work->ctx = ecpri_dma_mhi_client_ctx[idx];

		queue_work(ecpri_dma_mhi_client_ctx[idx]->wq, &work->work);
	} else {
		DMAERR("Allocation error in workqueue\n");
		ret = -ENOMEM;
		goto fail_queue_work_wq;
	}

	return 0;

fail_set_state:
fail_queue_work_wq:
	destroy_workqueue(ecpri_dma_mhi_client_ctx[idx]->wq);
	kfree(work);
fail_memcpy_enable:
	ecpri_dma_mhi_memcpy_destroy(function);
fail_memcpy_init:
fail_create_wq:
	kfree(ecpri_dma_mhi_client_ctx[idx]);
	ecpri_dma_mhi_client_ctx[idx] = NULL;
fail_alloc_ctx:
fail_ready_state:
	return ret;
}

/**
 * ecpri_dma_mhi_destroy() - Destroy MHI DMA
 *
 * This function is called by MHI client driver on MHI reset to destroy all DMA
 * MHI resources.
 *
 */
static void ecpri_dma_mhi_destroy(
	struct mhi_dma_function_params function)
{
	int idx;
	int ret;

	ret = ecpri_dma_mhi_get_function_context_index(
		function, &idx);
	if (ret != 0) {
		DMAERR("Function params are invalid,"
			"function type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		return;
	}

	if (!ecpri_dma_mhi_client_ctx[idx]) {
		DMAERR("Context is not initialized\n");
		return;
	}

	ret = ecpri_dma_mhi_dma_memcpy_disable(function);
	if (ret != 0) {
		DMAERR("Failed to disable memcpy\n");
		ecpri_dma_assert();
	}

	ecpri_dma_mhi_memcpy_destroy(function);

	idr_destroy(&ecpri_dma_mhi_client_ctx[idx]->idr);

	// TODO: Destroy debugfs

	destroy_workqueue(ecpri_dma_mhi_client_ctx[idx]->wq);

	ecpri_dma_mhi_client_ctx[idx]->notify_cb = NULL;
	ecpri_dma_mhi_client_ctx[idx]->user_data = NULL;
	ecpri_dma_mhi_client_ctx[idx]->msi_config.addr_low = 0;
	ecpri_dma_mhi_client_ctx[idx]->msi_config.addr_hi = 0;
	ecpri_dma_mhi_client_ctx[idx]->msi_config.data = 0;
	ecpri_dma_mhi_client_ctx[idx]->msi_config.mask = 0;
	ecpri_dma_mhi_client_ctx[idx]->mmio_addr = 0;
	ecpri_dma_mhi_client_ctx[idx]->first_ch = 0;
	ecpri_dma_mhi_client_ctx[idx]->first_ev = 0;
	ecpri_dma_mhi_client_ctx[idx]->is_over_pcie = false;
	ecpri_dma_mhi_client_ctx[idx]->mhi_mstate =
		MHI_DMA_STATE_M_MAX;
	ecpri_dma_mhi_client_ctx[idx]->state =
		ECPRI_DMA_MHI_STATE_INVALID;
	ecpri_dma_mhi_client_ctx[idx]->dev_scratch
		.mhi_base_chan_idx_valid = false;
	ecpri_dma_mhi_client_ctx[idx]->dev_scratch
		.mhi_base_chan_idx = 0;

	kfree(ecpri_dma_mhi_client_ctx[idx]);
	ecpri_dma_mhi_client_ctx[idx] = NULL;
	DMADBG("DMA MHI was reset, ready for re-init\n");

	return;
}

/**
 * ecpri_dma_mhi_client_dma_start() - Start DMA MHI engine
 * @function: function parameters
 * @params: pcie addresses for MHI
 *
 * This function is called by MHI client driver on MHI engine start for
 * handling MHI accelerated channels. This function is called after
 * mhi_dma_init() was called and can be called after MHI reset to restart
 * MHI engine. When this function returns device can move to M0 state.
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
static int ecpri_dma_mhi_client_dma_start(
	struct mhi_dma_function_params function,
	struct mhi_dma_start_params* params)
{
	int idx;
	int ret;
	unsigned long flags;
	unsigned long gsi_dev_hdl;
	enum ecpri_dma_ees ee_idx;

	if (!params) {
		DMAERR("Null params args\n");
		return -EINVAL;
	}

	ret = ecpri_dma_mhi_get_function_context_index(function, &idx);
	if (ret != 0) {
		DMAERR("Function params are invalid, function"
			" type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		return -EINVAL;
	}

	if (!ecpri_dma_mhi_client_ctx[idx]) {
		DMAERR("Context not initialized\n");
		return -EPERM;
	}

	spin_lock_irqsave(
		&ecpri_dma_mhi_client_ctx[idx]->lock, flags);
	if (ecpri_dma_mhi_client_ctx[idx]->state !=
		ECPRI_DMA_MHI_STATE_READY) {
		DMAERR("MHI Client for EE %d is not ready\n", idx);
		spin_unlock_irqrestore(
			&ecpri_dma_mhi_client_ctx[idx]->lock, flags);
		return -EPERM;
	}
	spin_unlock_irqrestore(
		&ecpri_dma_mhi_client_ctx[idx]->lock, flags);

	ecpri_dma_mhi_client_ctx[idx]->host_ctrl_addr =
		params->host_ctrl_addr;
	ecpri_dma_mhi_client_ctx[idx]->host_data_addr =
		params->host_data_addr;
	ecpri_dma_mhi_client_ctx[idx]->channel_context_array_addr =
		params->channel_context_array_addr;
	ecpri_dma_mhi_client_ctx[idx]->event_context_array_addr =
		params->event_context_array_addr;

	ret = ecpri_dma_get_gsi_dev_hdl(&gsi_dev_hdl);
	if (ret != 0) {
		DMAERR("Unable to retrieve the GSI device handle\n");
		return -EFAULT;
	}

	ret = ecpri_dma_mhi_get_ee_index(function, &ee_idx);
	if (ret != 0) {
		DMAERR("Unable to translate VF/PF to EE index\n");
		return -EINVAL;
	}

	ret = gsi_write_device_scratch(gsi_dev_hdl, ee_idx,
		&ecpri_dma_mhi_client_ctx[idx]->dev_scratch);
	if (ret != 0) {
		DMAERR("Unable to write device scratch, idx: %d,"
			"device handle: %lu\n",
			idx, gsi_dev_hdl);
		return -EFAULT;
	}

	ret = ecpri_dma_mhi_set_state(
		ecpri_dma_mhi_client_ctx[idx],
		ECPRI_DMA_MHI_STATE_STARTED);
	if (ret != 0) {
		DMAERR("Unable to set state, State:%d\n",
			ECPRI_DMA_MHI_STATE_STARTED);
		return -EFAULT;
	}

	return ret;
}

/**
 * ecpri_dma_mhi_client_read_write_host() - Perform read/write operation
 * between host and device.
 *
 * @ctx: the MHI Client context
 * @dir: The direction of the operation
 * @dev_addr: device address
 * @host_addr: host adress
 * @size: buffer size
 *
 * Function reads/writes from/to device by using ecpri_dma_mhi_dma_sync_memcpy
 * Return codes: 0: success
 *		-EFAULT: invalid platform
 *		-ENOMEM: allocating memory error
 *      -EINVAL: invalid addresses
 */
static int ecpri_dma_mhi_client_read_write_host(
	struct ecpri_dma_mhi_client_context* ctx,
	enum ecpri_dma_mhi_dma_dir dir, void *dev_addr,
	u64 host_addr, int size,
	struct mhi_dma_function_params function)
{
	int ret = 0;
	struct device* pdev;
	struct ecpri_dma_mem_buffer mem;

	pdev = ecpri_dma_get_pdev();
	host_addr = ECPRI_DMA_MHI_HOST_ADDR_COND(host_addr, ctx);

	mem.size = size;
	if (pdev) {
		mem.virt_base = dma_alloc_coherent(pdev, mem.size,
			&mem.phys_base, GFP_KERNEL);
	}
	else {
		DMAERR("Platform dev is not valid");
		return -EFAULT;
	}
	if (!mem.virt_base) {
		DMAERR("dma_alloc_coherent failed, DMA buff size %d\n", mem.size);
		return -ENOMEM;
	}

	if (dir == ECPRI_DMA_MHI_DMA_FROM_HOST) {
		ret = ecpri_dma_mhi_dma_sync_memcpy(mem.phys_base, host_addr,
			size, function);
		if (ret) {
			DMAERR("ecpri_dma_mhi_dma_sync_memcpy from host fail%d\n", ret);
			goto failed_memcopy;
		}
		memcpy(dev_addr, mem.virt_base, size);
	}
	else {
		memcpy(mem.virt_base, dev_addr, size);
		ret = ecpri_dma_mhi_dma_sync_memcpy(host_addr, mem.phys_base,
			size, function);
		if (ret) {
			DMAERR("ecpri_dma_mhi_dma_sync_memcpy to host fail %d\n", ret);
			goto failed_memcopy;
		}
	}

	return 0;

failed_memcopy:
	dma_free_coherent(pdev, mem.size, mem.virt_base, mem.phys_base);
	return ret;
}


/**
 * ecpri_dma_mhi_client_read_ch_ctx() - Reads channel context.
 *
 * @ctx: the MHI Client context
 * @channel: pointer to MHI channel context
 * @function: params
 *
 * Return codes: 0: success
 *		-EFAULT: invalid platform
 *		-ENOMEM: allocating memory error
 *      -EINVAL: invalid addresses
 */
static int ecpri_dma_mhi_client_read_ch_ctx(
	struct ecpri_dma_mhi_client_context* ctx,
	struct ecpri_dma_mhi_channel_ctx* channel,
	struct mhi_dma_function_params function)
{
	int ret = 0;

	ret = ecpri_dma_mhi_client_read_write_host(ctx, ECPRI_DMA_MHI_DMA_FROM_HOST,
		&channel->ch_ctx_host, channel->channel_context_addr,
		sizeof(channel->ch_ctx_host), function);
	if (ret != 0) {
		DMAERR("ecpri_dma_mhi_client_read_write_host failed,"
			" return code %d\n", ret);
		return ret;
	}

	channel->ev_context_addr = ctx->event_context_array_addr +
		(channel->ch_ctx_host.erindex) *
		sizeof(struct ecpri_dma_mhi_host_ev_ctx);

	ret = ecpri_dma_mhi_client_read_write_host(ctx, ECPRI_DMA_MHI_DMA_FROM_HOST,
		&channel->ev_ctx_host, channel->ev_context_addr,
		sizeof(channel->ev_ctx_host), function);
	if (ret != 0) {
		DMAERR("ecpri_dma_mhi_client_read_write_host failed,"
			" return code %d\n", ret);
		return ret;
	}

	return ret;
}

/**
 * ecpri_dma_mhi_client_connect_internal() - Connect pipe to DMA
 * and start corresponding MHI channel
 *
 * @channel: MHI channel context
 * @ctx: MHI Client context
 * @function: params
 *
 * This function is called by MHI client driver on MHI channel start.
 * This function is called after MHI engine was started.
 *
 * Return codes: 0	  : success
 *		 negative     : error
 */
static int ecpri_dma_mhi_client_connect_internal(
	struct ecpri_dma_mhi_channel_ctx* channel,
	struct ecpri_dma_mhi_client_context* ctx,
	struct mhi_dma_function_params function)
{
	int ret;
	union __packed gsi_channel_scratch ch_scratch;
	struct ecpri_dma_moderation_config mod_cfg;

	mod_cfg.moderation_counter_threshold = channel->int_modc;
	mod_cfg.moderation_timer_threshold = channel->int_modt;

	ret = ecpri_dma_alloc_endp(channel->endp_ctx->endp_id, channel->rlen,
		&mod_cfg, channel->is_over_pcie, NULL);
	if (ret != 0) {
		DMAERR("Failed to allocate endp %d\n", channel->endp_ctx->endp_id);
		goto fail_al_endp;
	}

	ch_scratch.mhi.is_over_pcie = channel->is_over_pcie;
	ch_scratch.mhi.skip_overflow_ev = channel->disable_overflow_event;
	ret = gsi_write_channel_scratch(channel->endp_ctx->gsi_chan_hdl,
		ch_scratch);
	if (ret != 0) {
		DMAERR("Unable to write channel scratch,"
			"gsi channel handle: %lu\n",
			channel->endp_ctx->gsi_chan_hdl);
		goto fail_write_scratch;
	}

	ret = ecpri_dma_start_endp(channel->endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to start endp, endp ID: %d\n",
			channel->endp_ctx->endp_id);
		goto fail_start_endp;
	}

	ret = ecpri_dma_enable_dma_endp(channel->endp_ctx);
	if (ret != 0) {
		DMAERR("Failed to enable the channel, endp ID: %d\n",
			channel->endp_ctx->endp_id);
		goto fail_enable_endp;
	}

	channel->state = ECPRI_DMA_HW_MHI_CHANNEL_STATE_RUN;

	ret = ecpri_dma_mhi_client_read_write_host(
		ctx, ECPRI_DMA_MHI_DMA_TO_HOST, &channel->ch_ctx_host,
		channel->channel_context_addr +
		offsetof(struct ecpri_dma_mhi_host_ch_ctx, chstate),
		sizeof(channel->ch_ctx_host.chstate),
		function);
	if (ret != 0) {
		DMAERR("Unable to read write host\n");
		goto fail_read_write;
	}

	ret = 0;
	goto success;

fail_read_write:
fail_enable_endp:
	ret = ecpri_dma_stop_endp(channel->endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to stop the endp, ENDP ID: %d\n",
			channel->endp_ctx->endp_id);
		ecpri_dma_assert();
	}
fail_start_endp:
	ret = ecpri_dma_dealloc_endp(channel->endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to deallocate the endp, ENDP ID: %d\n",
			channel->endp_ctx->endp_id);
		ecpri_dma_assert();
	}
fail_write_scratch:
fail_al_endp:
success:
	return ret;
}

/**
 * ecpri_dma_mhi_dma_connect_endp() - Connect endp to DMA and start
 * corresponding MHI channel
 * @function: function parameters
 * @in: connect parameters
 * @clnt_hdl: [out] client handle for this endp
 *
 * This function is called by MHI client driver on MHI channel start.
 * This function is called after MHI engine was started.
 *
 * Return codes:    0 : success
 *		            negative : error
 */
static int ecpri_dma_mhi_dma_connect_endp(
	struct mhi_dma_function_params function,
	struct mhi_dma_connect_params* in, u32* clnt_hdl)
{
	int idx;
	int ret;
	unsigned long flags;
	enum ecpri_dma_ees ee_idx;
	struct ecpri_dma_mhi_channel_ctx* channel;
	struct ecpri_dma_endp_context* endp_ctx;
	u32 ch_idx = ECPRI_DMA_MHI_INVALID_CH_ID;

	if (!in) {
		DMAERR("Null params args\n");
		return -EINVAL;
	}

	ret = ecpri_dma_mhi_get_function_context_index(function, &idx);
	if (ret != 0) {
		DMAERR("Function params are invalid, function type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		return -EINVAL;
	}

	if (!ecpri_dma_mhi_client_ctx[idx]) {
		DMAERR("Context not initialized\n");
		return -EPERM;
	}

	spin_lock_irqsave(
		&ecpri_dma_mhi_client_ctx[idx]->lock, flags);

	if (ecpri_dma_mhi_client_ctx[idx]->state !=
		ECPRI_DMA_MHI_STATE_STARTED) {
		DMAERR("MHI Client for EE %d is not started\n", idx);
		spin_unlock_irqrestore(
			&ecpri_dma_mhi_client_ctx[idx]->lock, flags);
		return -EPERM;
	}

	spin_unlock_irqrestore(
		&ecpri_dma_mhi_client_ctx[idx]->lock, flags);

	ch_idx = in->channel_id - ecpri_dma_mhi_client_ctx[idx]->first_ch;
	if (ch_idx > ECPRI_DMA_MHI_MAX_HW_CHANNELS) {
		DMAERR("CH IDX out of bounds %d\n", ch_idx);
		return -EINVAL;
	}

	channel = &ecpri_dma_mhi_client_ctx[idx]->channels[ch_idx];
	if (channel->valid) {
		DMAERR("Channel already connected\n");
		return -EPERM;
	}

	channel->channel_context_addr =
		ecpri_dma_mhi_client_ctx[idx]->channel_context_array_addr +
		(in->channel_id * sizeof(struct ecpri_dma_mhi_host_ch_ctx));

	ret = ecpri_dma_mhi_client_read_ch_ctx(
		ecpri_dma_mhi_client_ctx[idx], channel,
		function);
	if (ret != 0) {
		DMAERR("Unable to read channel context\n");
		return -EPERM;
	}

	channel->channel_id = ch_idx;
	channel->event_id = channel->ch_ctx_host.erindex -
		ecpri_dma_mhi_client_ctx[idx]->first_ev;
	ret = ecpri_dma_mhi_get_ee_index(function, &ee_idx);
	if (ret != 0) {
		DMAERR("Unable to translate VF/PF to EE index\n");
		return -EINVAL;
	}

	ret = ecpri_dma_mhi_get_endp_ctx(ee_idx, ch_idx,
		&endp_ctx);
	if (ret != 0) {
		DMAERR("endp_ctx, ee: %d, ch_id: %d\n",
			ee_idx, ch_idx);
		return ret;
	}

	channel->endp_ctx = endp_ctx;
	channel->endp_ctx->is_endp_mhi_l2 = true;
	channel->endp_ctx->is_over_pcie =
		ecpri_dma_mhi_client_ctx[idx]->is_over_pcie;
	channel->is_over_pcie = channel->endp_ctx->is_over_pcie;
	channel->endp_ctx->disable_overflow_event = in->disable_overflow_event;
	channel->disable_overflow_event = in->disable_overflow_event;
	channel->endp_ctx->l2_mhi_channel_ptr = channel;
	channel->msi_config =
		&ecpri_dma_mhi_client_ctx[idx]->msi_config;
	channel->rlen = channel->ch_ctx_host.rlen;
	channel->rlen = channel->ev_ctx_host.rlen;
	channel->int_modt = channel->ev_ctx_host.intmodt;
	channel->int_modc = channel->ev_ctx_host.intmodc;

	ret = ecpri_dma_mhi_client_connect_internal(channel,
		ecpri_dma_mhi_client_ctx[idx], function);
	if (ret != 0) {
		DMAERR("GSI endp config not found, ee: %d, ch_id: %d\n",
			ee_idx, ch_idx);
		return ret;
	}

	idr_preload(GFP_KERNEL);

	spin_lock_irqsave(
		&ecpri_dma_mhi_client_ctx[idx]->idr_lock, flags);

	*(clnt_hdl) = idr_alloc(
		&ecpri_dma_mhi_client_ctx[idx]->idr,
		channel, ECPRI_DMA_MHI_MIN_VALID_HDL, 0, GFP_NOWAIT);

	spin_unlock_irqrestore(
		&ecpri_dma_mhi_client_ctx[idx]->idr_lock, flags);

	idr_preload_end();

	channel->valid = true;
	return ret;
}

/**
 * ecpri_dma_mhi_dma_disconnect_endp() - Disconnect endp from DMA and
 * reset corresponding MHI channel
 *
 * @clnt_hdl: client handle for this endp
 *
 * This function is called by DMA MHI client driver on MHI channel reset.
 * This function is called after MHI channel was started.
 * This function is doing the following:
 *	- Send command to GSI to reset corresponding MHI channel
 *
 * Return codes: 0		: success
 *		  negative		: error
 */
static int ecpri_dma_mhi_dma_disconnect_endp(
	struct mhi_dma_function_params function,
	struct mhi_dma_disconnect_params* in)
{
	int idx;
	int ret = 0;
	struct ecpri_dma_mhi_channel_ctx* channel = NULL;

	ret = ecpri_dma_mhi_get_function_context_index(function, &idx);
	if (ret != 0) {
		DMAERR("Function params are invalid, function type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		return -EINVAL;
	}

	/* Get channel context */
	spin_lock_bh(&ecpri_dma_mhi_client_ctx[idx]->idr_lock);
	channel = idr_find(&ecpri_dma_mhi_client_ctx[idx]->idr, in->clnt_hdl);
	spin_unlock_bh(&ecpri_dma_mhi_client_ctx[idx]->idr_lock);

	channel->state = ECPRI_DMA_HW_MHI_CHANNEL_STATE_DISABLE;
	channel->valid = false;

	/* Write new state */
	ret = ecpri_dma_mhi_client_read_write_host(
		ecpri_dma_mhi_client_ctx[idx],
		ECPRI_DMA_MHI_DMA_TO_HOST, &channel->ch_ctx_host,
		channel->channel_context_addr +
		offsetof(struct ecpri_dma_mhi_host_ch_ctx, chstate),
		sizeof(channel->ch_ctx_host.chstate),
		function);
	if (ret != 0) {
		DMAERR("Unable to read write host\n");
		return -EPERM;
	}

	/* Stop */
	ret = ecpri_dma_stop_endp(channel->endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to stop the endp, ENDP ID: %d\n",
			channel->endp_ctx->endp_id);
		ecpri_dma_assert();
	}

	/* Reset */
	ret = ecpri_dma_reset_endp(channel->endp_ctx);
	if (ret != GSI_STATUS_SUCCESS) {
		DMAERR("Unable to reset endp, endp_id: %d\n",
			channel->endp_ctx->endp_id);
		return ret;
	}

	/* Disable */
	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		ret = ecpri_dma_disable_dma_endp(channel->endp_ctx);
		if (ret != 0) {
			DMAERR("Unable to disable endpoint %d\n",
				channel->endp_ctx->endp_id);
			ecpri_dma_assert();
		}
	}

	/* Dealloc */
	ret = ecpri_dma_dealloc_endp(channel->endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to deallocate the endp, ENDP ID: %d\n",
			channel->endp_ctx->endp_id);
		ecpri_dma_assert();
	}

	DMADBG("client (ep: %d) disconnected\n",
		channel->endp_ctx->endp_id);

	return ret;
}

int ecpri_dma_mhi_client_ready_cb(void (*mhi_ready_cb)(void *user_data),
			      void *user_data)
{
	return ecpri_dma_register_ready_cb(mhi_ready_cb, user_data);
}

int ecpri_dma_mhi_client_update_mstate(struct mhi_dma_function_params function,
			  enum mhi_dma_mstate mstate_info)
{
	int ret = 0;
	int idx = 0;
	unsigned long flags;

	DMADBG("Begin\n");

	/* Function params validity check */
	ret = ecpri_dma_mhi_get_function_context_index(function, &idx);
	if (ret != 0) {
		DMAERR("Function params are invalid,"
		       "function type: %d, vf_id: %d\n",
		       function.function_type, function.vf_id);
		return -EINVAL;
	}

	if (!ecpri_dma_mhi_client_ctx[idx]) {
		DMAERR("MHI ontext for IDX %d is not initialized\n", idx);
		return -EINVAL;
	}

	DMADBG("Req update mstate to %d\n", mstate_info);
	spin_lock_irqsave(&ecpri_dma_mhi_client_ctx[idx]->lock, flags);
	ecpri_dma_mhi_client_ctx[idx]->mhi_mstate = mstate_info;
	spin_unlock_irqrestore(&ecpri_dma_mhi_client_ctx[idx]->lock, flags);

	return 0;
}

/* API exposed structure */
const struct mhi_dma_ops ecpri_dma_mhi_driver_ops = {
	.mhi_dma_register_ready_cb = ecpri_dma_mhi_client_ready_cb,
	.mhi_dma_init = ecpri_dma_mhi_client_init,
	.mhi_dma_start = ecpri_dma_mhi_client_dma_start,
	.mhi_dma_connect_endp = ecpri_dma_mhi_dma_connect_endp,
	.mhi_dma_disconnect_endp = ecpri_dma_mhi_dma_disconnect_endp,
	.mhi_dma_destroy = ecpri_dma_mhi_destroy,
	.mhi_dma_memcpy_init = ecpri_dma_mhi_memcpy_init,
	.mhi_dma_memcpy_destroy = ecpri_dma_mhi_memcpy_destroy,
	.mhi_dma_sync_memcpy = ecpri_dma_mhi_dma_sync_memcpy,
	.mhi_dma_async_memcpy = ecpri_dma_mhi_dma_async_memcpy,
	.mhi_dma_memcpy_enable = ecpri_dma_mhi_dma_memcpy_enable,
	.mhi_dma_memcpy_disable = ecpri_dma_mhi_dma_memcpy_disable,
};

inline int
mhi_dma_register_ready_cb(void (*mhi_ready_cb)(void *user_data),
			  void *user_data)
{
	return ecpri_dma_mhi_driver_ops.mhi_dma_register_ready_cb(mhi_ready_cb,
								  user_data);
}
EXPORT_SYMBOL(mhi_dma_register_ready_cb);

inline int mhi_dma_init(struct mhi_dma_function_params function,
	struct mhi_dma_init_params* params, struct mhi_dma_init_out* out)
{
	return ecpri_dma_mhi_driver_ops.mhi_dma_init(function, params, out);
}
EXPORT_SYMBOL(mhi_dma_init);

inline int mhi_dma_start(struct mhi_dma_function_params function,
				struct mhi_dma_start_params *params)
{
	return ecpri_dma_mhi_driver_ops.mhi_dma_start(function, params);
}
EXPORT_SYMBOL(mhi_dma_start);

inline int mhi_dma_connect_endp(struct mhi_dma_function_params function,
				       struct mhi_dma_connect_params *in,
				       u32 *clnt_hdl)
{
	return ecpri_dma_mhi_driver_ops.mhi_dma_connect_endp(function, in,
							      clnt_hdl);
}
EXPORT_SYMBOL(mhi_dma_connect_endp);

inline int
mhi_dma_disconnect_endp(struct mhi_dma_function_params function,
			struct mhi_dma_disconnect_params *in)
{
	enum ecpri_hw_ver hw_ver = ecpri_dma_get_ctx_hw_ver();

	if (hw_ver == ECPRI_HW_V1_0)
		return -EPERM;

	return ecpri_dma_mhi_driver_ops.mhi_dma_disconnect_endp(function, in);
}
EXPORT_SYMBOL(mhi_dma_disconnect_endp);

inline int mhi_dma_suspend(struct mhi_dma_function_params function,
				  bool force)
{
	return -EPERM;
}

int mhi_dma_resume(struct mhi_dma_function_params function)
{
	return -EPERM;
}
EXPORT_SYMBOL(mhi_dma_resume);

inline int mhi_dma_update_mstate(struct mhi_dma_function_params function,
					enum mhi_dma_mstate mstate_info)
{

	//TODO: add mhi_dma_update_mstate to ops structure in mhi_dma.h as it's
	// currently missing

	return ecpri_dma_mhi_client_update_mstate(function,mstate_info);
}
EXPORT_SYMBOL(mhi_dma_update_mstate);

inline void mhi_dma_destroy(struct mhi_dma_function_params function)
{
	ecpri_dma_mhi_driver_ops.mhi_dma_destroy(function);
}
EXPORT_SYMBOL(mhi_dma_destroy);

inline int mhi_dma_memcpy_init(struct mhi_dma_function_params function)
{
	return ecpri_dma_mhi_driver_ops.mhi_dma_memcpy_init(function);
}
EXPORT_SYMBOL(mhi_dma_memcpy_init);

inline void
mhi_dma_memcpy_destroy(struct mhi_dma_function_params function)
{
	ecpri_dma_mhi_driver_ops.mhi_dma_memcpy_destroy(function);
}
EXPORT_SYMBOL(mhi_dma_memcpy_destroy);

inline int mhi_dma_memcpy_enable(struct mhi_dma_function_params function)
{
	return ecpri_dma_mhi_driver_ops.mhi_dma_memcpy_enable(function);
}
EXPORT_SYMBOL(mhi_dma_memcpy_enable);

inline int
mhi_dma_memcpy_disable(struct mhi_dma_function_params function)
{
	return ecpri_dma_mhi_driver_ops.mhi_dma_memcpy_disable(function);
}
EXPORT_SYMBOL(mhi_dma_memcpy_disable);

inline int mhi_dma_sync_memcpy(u64 dest, u64 src, int len,
				      struct mhi_dma_function_params function)
{
	return ecpri_dma_mhi_driver_ops.mhi_dma_sync_memcpy(dest, src, len,
							    function);
}
EXPORT_SYMBOL(mhi_dma_sync_memcpy);

inline int mhi_dma_async_memcpy(u64 dest, u64 src, int len,
				       struct mhi_dma_function_params function,
				       void (*user_cb)(void *user1),
				       void *user_param)
{
	return ecpri_dma_mhi_driver_ops.mhi_dma_async_memcpy(dest, src, len,
							    function, user_cb, user_param);
}
EXPORT_SYMBOL(mhi_dma_async_memcpy);
