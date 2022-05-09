/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ECPRI_DMA_DEBUG_FS_H_
#define _ECPRI_DMA_DEBUG_FS_H_

#include "ecpri_dma_i.h"

 /** Functions **/

void ecpri_dma_debugfs_init(void);
void ecpri_dma_debugfs_remove(void);
struct dentry *ecpri_dma_debugfs_get_root(void);

#ifdef CONFIG_ECPRI_DMA_UT
int ecpri_dma_ut_module_init(void);
void ecpri_dma_ut_module_exit(void);
#endif

#endif /* _ECPRI_DMA_DEBUG_FS_H_ */