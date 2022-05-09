/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include "dmahal.h"
#include "dmahal_i.h"
#include "dmahal_reg_i.h"

#define CHECK_SET_PARAM(member, p_cmd_data, p_params, p_params_mask) \
	if ((p_params_mask)->member) {\
		(p_cmd_data)->member = (p_params)->member;\
	}

struct ecpri_dma_hal_context *ecpri_dma_hal_ctx;

int ecpri_dma_hal_init(enum ecpri_hw_ver ecpri_hw_ver, void __iomem *base,
	u32 ecpri_dma_cfg_offset, struct device *ecpri_dma_pdev)
{
	int result;

	DMAHAL_DBG("Entry - DMA HW TYPE=%d base=%px ecpri_dma_pdev=%px\n",
		ecpri_hw_ver, base, ecpri_dma_pdev);

	ecpri_dma_hal_ctx = kzalloc(sizeof(*ecpri_dma_hal_ctx), GFP_KERNEL);
	if (!ecpri_dma_hal_ctx) {
		DMAHAL_ERR("kzalloc err for ecpri_dma_hal_ctx\n");
		result = -ENOMEM;
		goto bail_err_exit;
	}

	if (ecpri_hw_ver >= ECPRI_HW_MAX) {
		DMAHAL_ERR("invalid DMA HW type (%d)\n", ecpri_hw_ver);
		result = -EINVAL;
		goto bail_free_ctx;
	}

	if (!base) {
		DMAHAL_ERR("invalid memory io mapping addr\n");
		result = -EINVAL;
		goto bail_free_ctx;
	}

	if (!ecpri_dma_pdev) {
		DMAHAL_ERR("invalid DMA platform device\n");
		result = -EINVAL;
		goto bail_free_ctx;
	}

	ecpri_dma_hal_ctx->ecpri_hw_ver = ecpri_hw_ver;
	ecpri_dma_hal_ctx->base = base;
	ecpri_dma_hal_ctx->ecpri_dma_cfg_offset = ecpri_dma_cfg_offset;
	ecpri_dma_hal_ctx->ecpri_dma_pdev = ecpri_dma_pdev;

	if (ecpri_dma_hal_reg_init(ecpri_hw_ver)) {
		DMAHAL_ERR("failed to init ecpri_dma_hal reg\n");
		result = -EFAULT;
		goto bail_free_ctx;
	}

	/* create an IPC buffer for the registers dump */
	ecpri_dma_hal_ctx->regdumpbuf = ipc_log_context_create(DMAHAL_IPC_LOG_PAGES,
		"ecpri_dma_regs", 0);
	if (ecpri_dma_hal_ctx->regdumpbuf == NULL)
		DMAHAL_ERR("failed to create DMA regdump log, continue...\n");

	return 0;

bail_free_ctx:
	if (ecpri_dma_hal_ctx->regdumpbuf)
		ipc_log_context_destroy(ecpri_dma_hal_ctx->regdumpbuf);
	kfree(ecpri_dma_hal_ctx);
	ecpri_dma_hal_ctx = NULL;
bail_err_exit:
	return result;
}

void ecpri_dma_hal_destroy(void)
{
	DMAHAL_DBG("Entry\n");
	kfree(ecpri_dma_hal_ctx);
	ecpri_dma_hal_ctx = NULL;
}
