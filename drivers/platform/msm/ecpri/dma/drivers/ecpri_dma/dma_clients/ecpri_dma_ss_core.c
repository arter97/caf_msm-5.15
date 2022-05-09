/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "ecpri_dma_ss_core.h"

struct ecpri_dma_ss_core_context
	*ecpri_dma_ss_core_ctx = NULL;

static void ecpri_dma_ss_core_wq_notify_ready(struct work_struct* work)
{
	unsigned long flags;
	void* ready_user_data;
	ecpri_dma_ready_cb ready_cb;

	if (!ecpri_dma_ss_core_ctx)
	{
		DMAERR("Context isn't initialized\n");
		return;
	}

	spin_lock_irqsave(&ecpri_dma_ss_core_ctx->lock, flags);
	ready_cb = ecpri_dma_ss_core_ctx->ready_cb;
	ready_user_data = ecpri_dma_ss_core_ctx->ready_user_data;
	spin_unlock_irqrestore(&ecpri_dma_ss_core_ctx->lock, flags);

	ready_cb(ready_user_data);
}

static DECLARE_WORK(ecpri_dma_ss_core_notify_ready_work,
	ecpri_dma_ss_core_wq_notify_ready);

int ecpri_dma_ecpri_ss_register(
	struct ecpri_dma_ecpri_ss_register_params* ready_info,
	bool* is_dma_ready)
{
	int ret = 0;
	unsigned long flags;

	if (ecpri_dma_ss_core_ctx != NULL) {
		DMAERR("Context already initialized\n");
		return -EPERM;
	}

	/* Verify parameters validity */
	if (!ready_info
		|| !ready_info->notify_ready
		|| !ready_info->dma_event_notify
		|| !ready_info->log_msg
		|| !is_dma_ready) {
		DMAERR("Invalid paramteres\n");
		return -EINVAL;
	}

	/* Initialize context */
	ecpri_dma_ss_core_ctx = kzalloc(
		sizeof(*ecpri_dma_ss_core_ctx),
		GFP_KERNEL);
	if (!ecpri_dma_ss_core_ctx) {
		DMAERR("SS Core Client context"
			" is not initialized\n");
		return -ENOMEM;
	}

	spin_lock_init(&ecpri_dma_ss_core_ctx->lock);

	spin_lock_irqsave(&ecpri_dma_ss_core_ctx->lock, flags);

	ecpri_dma_ss_core_ctx->ready_cb =
		ready_info->notify_ready;
	ecpri_dma_ss_core_ctx->ready_user_data =
		ready_info->userdata_ready;

	ecpri_dma_ss_core_ctx->event_notify_cb =
		ready_info->dma_event_notify;
	ecpri_dma_ss_core_ctx->event_notify_user_data =
		ready_info->userdata_dma_event_notify;

	ecpri_dma_ss_core_ctx->log_msg_cb =
		ready_info->log_msg;
	ecpri_dma_ss_core_ctx->log_msg_user_data =
		ready_info->userdata_log_msg;

	ecpri_dma_ss_core_ctx->ecpri_hw_ver =
		ecpri_dma_get_ctx_hw_ver();
	ecpri_dma_ss_core_ctx->hw_flavor =
		ecpri_dma_get_ctx_hw_flavor();

	spin_unlock_irqrestore(&ecpri_dma_ss_core_ctx->lock, flags);

	/* Initialize workqueue */
	ecpri_dma_ss_core_ctx->wq =
		create_singlethread_workqueue("ecpri_dma_ss_core_wq");
	if (!ecpri_dma_ss_core_ctx->wq) {
		DMAERR("Failed to create workqueue\n");
		ret = -EFAULT;
		goto fail_create_wq;
	}

	/* Check dma is ready */
	mutex_lock(&ecpri_dma_ctx->lock);
	*(is_dma_ready) = ecpri_dma_is_ready();
	mutex_unlock(&ecpri_dma_ctx->lock);

	if (*(is_dma_ready)) {
		queue_work(ecpri_dma_ss_core_ctx->wq,
			&ecpri_dma_ss_core_notify_ready_work);
	}
	else {
		ret = ecpri_dma_register_ready_cb(
			ecpri_dma_ss_core_ctx->ready_cb,
			ecpri_dma_ss_core_ctx->ready_user_data);
	}

	return 0;

fail_create_wq:
	kfree(ecpri_dma_ss_core_ctx);
	ecpri_dma_ss_core_ctx = NULL;
	return ret;
}

/**
 * ecpri_dma_ecpri_ss_deregister() - eCPRI SS driver remove registeration with
 * DMA driver
 */
void ecpri_dma_ecpri_ss_deregister()
{
	ecpri_dma_ss_core_ctx->ready_cb = NULL;
	ecpri_dma_ss_core_ctx->ready_user_data = NULL;

	ecpri_dma_ss_core_ctx->event_notify_cb = NULL;
	ecpri_dma_ss_core_ctx->event_notify_user_data = NULL;

	ecpri_dma_ss_core_ctx->log_msg_cb = NULL;
	ecpri_dma_ss_core_ctx->log_msg_user_data = NULL;

	destroy_workqueue(ecpri_dma_ss_core_ctx->wq);

	kfree(ecpri_dma_ss_core_ctx);
	ecpri_dma_ss_core_ctx = NULL;
}

/**
 * ecpri_dma_ecpri_ss_get_endp_mapping() - DMA driver to provide ENDP mapping
 * to eCPRI SS driver
 * @endp_map:   [out] Pointer to structure with ENDP mapping & port information
 *              according to eCPRI HW flavor
 *
 * Returns:	0 on success, negative on failure
 */
int ecpri_dma_ecpri_ss_get_endp_mapping(
	struct ecpri_dma_endp_mapping* endp_map)
{
	int ret = 0;

	if (ecpri_dma_ss_core_ctx == NULL) {
		DMAERR("Context is not initialized\n");
		return -EPERM;
	}

	ret = ecpri_dma_get_port_mapping(
		ecpri_dma_ss_core_ctx->ecpri_hw_ver,
		ecpri_dma_ss_core_ctx->hw_flavor, endp_map);
	if (ret != 0) {
		DMAERR("Cant get port mapping\n");
		return -EPERM;
	}

	return 0;
}

/**
 * ecpri_dma_ecpri_ss_nfapi_config() - eCPRI SS driver to provide nFAPI
 * configurations to DMA driver
 * @cfg:    [out] nFAPI configurations provided by nFAPI user app
 *
 * Returns:	0 on success, negative on failure
 */
int ecpri_dma_ecpri_ss_nfapi_config(struct ecpri_dma_ecpri_ss_nfapi_cfg cfg)
{
	return 0;
}

/**
 * ecpri_dma_ecpri_ss_query_stats() - eCPRI SS driver to query DMA driver
 * statistics
 * @stats:		  [out] Structure of the DMA statistics collected
 *
 * Returns:	0 on success, negative on failure
 */
int ecpri_dma_ecpri_ss_query_stats(struct ecpri_dma_stats* stats)
{
	return 0;
}

/**
 * ecpri_dma_ecpri_ss_notify_ssr() - TBD
 *
 * Returns:	0 on success, negative on failure
 */
int ecpri_dma_ecpri_ss_notify_ssr(void)
{
	return 0;
}

/* API exposed structure */
const struct ecpri_dma_ecpri_ss_ops dma_ecpri_ss_driver_ops = {
	.ecpri_dma_ecpri_ss_register = ecpri_dma_ecpri_ss_register,
	.ecpri_dma_ecpri_ss_deregister = ecpri_dma_ecpri_ss_deregister,
	.ecpri_dma_ecpri_ss_get_endp_mapping = ecpri_dma_ecpri_ss_get_endp_mapping,
	.ecpri_dma_ecpri_ss_nfapi_config = ecpri_dma_ecpri_ss_nfapi_config,
	.ecpri_dma_ecpri_ss_query_stats = ecpri_dma_ecpri_ss_query_stats,
	.ecpri_dma_ecpri_ss_notify_ssr = ecpri_dma_ecpri_ss_notify_ssr,
};

EXPORT_SYMBOL(dma_ecpri_ss_driver_ops);