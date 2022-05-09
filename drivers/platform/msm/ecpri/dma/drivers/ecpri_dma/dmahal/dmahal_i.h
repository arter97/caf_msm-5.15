/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DMAHAL_I_H_
#define _DMAHAL_I_H_

#include "ecpri_dma_i.h"
#include "ecpri_dma_utils.h"

#define DMAHAL_DRV_NAME "ecpri_dma_hal"

#define DMAHAL_DBG(fmt, args...) \
	do { \
		pr_debug(DMAHAL_DRV_NAME " %s:%d " fmt, __func__, __LINE__, \
			## args); \
		DMA_IPC_LOGGING(ecpri_dma_get_ipc_logbuf(), \
			DMAHAL_DRV_NAME " %s:%d " fmt, ## args); \
		DMA_IPC_LOGGING(ecpri_dma_get_ipc_logbuf_low(), \
			DMAHAL_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define DMAHAL_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(DMAHAL_DRV_NAME " %s:%d " fmt, __func__, __LINE__, \
			## args); \
		DMA_IPC_LOGGING(ecpri_dma_get_ipc_logbuf_low(), \
			DMAHAL_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define DMAHAL_ERR(fmt, args...) \
	do { \
		pr_err(DMAHAL_DRV_NAME " %s:%d " fmt, __func__, __LINE__, \
			## args); \
		DMA_IPC_LOGGING(ecpri_dma_get_ipc_logbuf(), \
			DMAHAL_DRV_NAME " %s:%d " fmt, ## args); \
		DMA_IPC_LOGGING(ecpri_dma_get_ipc_logbuf_low(), \
			DMAHAL_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define DMAHAL_ERR_RL(fmt, args...) \
		do { \
			pr_err_ratelimited_ecpri_dma(DMAHAL_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
			DMA_IPC_LOGGING(ecpri_dma_get_ipc_logbuf(), \
				DMAHAL_DRV_NAME " %s:%d " fmt, ## args); \
			DMA_IPC_LOGGING(ecpri_dma_get_ipc_logbuf_low(), \
				DMAHAL_DRV_NAME " %s:%d " fmt, ## args); \
		} while (0)

#define DMAHAL_DBG_REG(fmt, args...) \
	do { \
		pr_err(fmt, ## args); \
		DMA_IPC_LOGGING(ecpri_dma_hal_ctx->regdumpbuf, \
			" %s:%d " fmt, ## args); \
	} while (0)

#define DMAHAL_DBG_REG_IPC_ONLY(fmt, args...) \
		DMA_IPC_LOGGING(ecpri_dma_hal_ctx->regdumpbuf, " %s:%d " fmt, ## args)

#define DMAHAL_MEM_ALLOC(__size, __is_atomic_ctx) \
	(kzalloc((__size), ((__is_atomic_ctx) ? GFP_ATOMIC : GFP_KERNEL)))

#define DMAHAL_IPC_LOG_PAGES 50

/*
 * struct ecpri_dma_hal_context - HAL global context data
 * @hw_type: eCPRI H/W type/version.
 * @base: Base address to be used for accessing DMA memory. This is
 *  I/O memory mapped address.
 * @ecpri_dma_pdev: DMA Platform Device. Will be used for DMA memory
 * @regdumpbuf: IPC log buffer
 */
struct ecpri_dma_hal_context {
	enum ecpri_hw_ver ecpri_hw_ver;
	void __iomem *base;
	u32 ecpri_dma_cfg_offset;
	struct device *ecpri_dma_pdev;
	void *regdumpbuf;
};

extern struct ecpri_dma_hal_context *ecpri_dma_hal_ctx;

#endif /* _DMAHAL_I_H_ */
