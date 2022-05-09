/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DMAHAL_H_
#define _DMAHAL_H_

#include "ecpri_dma_i.h"
#include "ecpri_dma_utils.h"
#include "dmahal_reg.h"

int ecpri_dma_hal_init(enum ecpri_hw_ver ecpri_hw_ver, void __iomem *base,
    u32 ecpri_dma_cfg_offset, struct device *ecpri_dma_pdev);
void ecpri_dma_hal_destroy(void);

/*
* ecpri_dma_hal_test_ep_bit() - return true if a ep bit is set
*/
bool ecpri_dma_hal_test_ep_bit(u32 reg_val, u32 ep_num);

/*
* ecpri_dma_hal_get_ep_bit() - get ep bit set in the right offset
*/
u32 ecpri_dma_hal_get_ep_bit(u32 ep_num);

/*
* ecpri_dma_hal_get_ep_reg_idx() - get ep reg index according to ep num
*/
u32 ecpri_dma_hal_get_ep_reg_idx(u32 ep_num);

#endif /* _DMAHAL_H_ */
