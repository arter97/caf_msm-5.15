/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DMAHAL_REG_I_H_
#define _DMAHAL_REG_I_H_

#include "ecpri_dma_i.h"
#include "ecpri_hwio.h"
#include "ecpri_hwio_def.h"

int ecpri_dma_hal_reg_init(enum ecpri_hw_ver ecpri_hw_ver);

#define DMA_SETFIELD(val, shift, mask) (((val) << (shift)) & (mask))
#define DMA_SETFIELD_IN_REG(reg, val, shift, mask) \
			(reg |= ((val) << (shift)) & (mask))
#define DMA_GETFIELD_FROM_REG(reg, shift, mask) \
		(((reg) & (mask)) >> (shift))

#endif /* _DMAHAL_REG_I_H_ */
