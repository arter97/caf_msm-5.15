/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "ecpri_dma_ut_framework.h"

/**
 * Example DMA Unit-test suite
 * To be a reference for writing new suites and tests.
 * This suite is also used as unit-test for the testing framework itself.
 * Structure:
 *	1- Define the setup and teardown  functions
 *	 Not Mandatory. Null may be used as well
 *	2- For each test, define its Run() function
 *	3- Use DMA_UT_DEFINE_SUITE_START() to start defining the suite
 *	4- use DMA_UT_ADD_TEST() for adding tests within
 *	 the suite definition block
 *	5- DMA_UT_DEFINE_SUITE_END() close the suite definition
 */

static int ecpri_dma_test_example_dummy;

static int ecpri_dma_test_example_suite_setup(void **ppriv)
{
	DMA_UT_DBG("Start Setup - set 0x1234F\n");

	ecpri_dma_test_example_dummy = 0x1234F;
	*ppriv = (void *)&ecpri_dma_test_example_dummy;

	return 0;
}

static int ecpri_dma_test_example_teardown(void *priv)
{
	DMA_UT_DBG("Start Teardown\n");
	DMA_UT_DBG("priv=0x%px - value=0x%x\n", priv, *((int *)priv));

	return 0;
}

static int ecpri_dma_test_example_test1(void *priv)
{
	DMA_UT_LOG("priv=0x%px - value=0x%x\n", priv, *((int *)priv));
	ecpri_dma_test_example_dummy++;

	return 0;
}

static int ecpri_dma_test_example_test2(void *priv)
{
	DMA_UT_LOG("priv=0x%px - value=0x%x\n", priv, *((int *)priv));
	ecpri_dma_test_example_dummy++;

	return 0;
}

static int ecpri_dma_test_example_test3(void *priv)
{
	DMA_UT_LOG("priv=0x%px - value=0x%x\n", priv, *((int *)priv));
	ecpri_dma_test_example_dummy++;

	return 0;
}

static int ecpri_dma_test_example_test4(void *priv)
{
	DMA_UT_LOG("priv=0x%px - value=0x%x\n", priv, *((int *)priv));
	ecpri_dma_test_example_dummy++;

	DMA_UT_TEST_FAIL_REPORT("failed on test");

	return -EFAULT;
}

/* Suite definition block */
DMA_UT_DEFINE_SUITE_START(example, "Example suite",
	ecpri_dma_test_example_suite_setup, ecpri_dma_test_example_teardown)
{
	DMA_UT_ADD_TEST(test1, "This is test number 1",
		ecpri_dma_test_example_test1, false, ECPRI_HW_V1_0, ECPRI_HW_MAX),

	DMA_UT_ADD_TEST(test2, "This is test number 2",
		ecpri_dma_test_example_test2, false, ECPRI_HW_V1_0, ECPRI_HW_MAX),

	DMA_UT_ADD_TEST(test3, "This is test number 3",
		ecpri_dma_test_example_test3, false, ECPRI_HW_V1_0, ECPRI_HW_MAX),

	DMA_UT_ADD_TEST(test4, "This is test number 4",
		ecpri_dma_test_example_test4, false, ECPRI_HW_V1_0, ECPRI_HW_MAX),

} DMA_UT_DEFINE_SUITE_END(example);
