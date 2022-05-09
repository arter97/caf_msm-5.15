/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DMA_UT_SUITE_LIST_H_
#define _DMA_UT_SUITE_LIST_H_

#include "ecpri_dma_ut_framework.h"

 /**
  * Declare every suite here so that it will be found later below
  * No importance for order.
  */
DMA_UT_DECLARE_SUITE(example);
DMA_UT_DECLARE_SUITE(driver_init);
DMA_UT_DECLARE_SUITE(driver_dp);
DMA_UT_DECLARE_SUITE(eth_dp);
DMA_UT_DECLARE_SUITE(ss_core_client);
DMA_UT_DECLARE_SUITE(mhi_client);

/**
 * Register every suite inside the below block.
 * Unregistered suites will be ignored
 */
DMA_UT_DEFINE_ALL_SUITES_START
{
    DMA_UT_REGISTER_SUITE(example),
    DMA_UT_REGISTER_SUITE(driver_init),
    DMA_UT_REGISTER_SUITE(driver_dp),
    DMA_UT_REGISTER_SUITE(eth_dp),
    DMA_UT_REGISTER_SUITE(ss_core_client),
    DMA_UT_REGISTER_SUITE(mhi_client),
} DMA_UT_DEFINE_ALL_SUITES_END;

#endif /* _DMA_UT_SUITE_LIST_H_ */
