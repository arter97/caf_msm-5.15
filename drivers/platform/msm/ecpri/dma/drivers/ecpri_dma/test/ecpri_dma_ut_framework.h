/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DMA_UT_FRAMEWORK_H_
#define _DMA_UT_FRAMEWORK_H_

#include <linux/kernel.h>
#include "ecpri_dma_i.h"
#include "ecpri_dma_interrupts.h"
#include "ecpri_dma_dp.h"
#include "ecpri_dma_debugfs.h"
#include "ecpri_dma_utils.h"

 /* Suite data global structure  name */
#define _DMA_UT_SUITE_DATA(__name) ecpri_dma_ut_ ##__name ##_data

/* Suite meta-data global structure name */
#define _DMA_UT_SUITE_META_DATA(__name) ecpri_dma_ut_ ##__name ##_meta_data

/* Suite global array of tests */
#define _DMA_UT_SUITE_TESTS(__name) ecpri_dma_ut_ ##__name ##_tests

/* Global array of all suites */
#define _DMA_UT_ALL_SUITES ecpri_dma_ut_all_suites_data

/* Meta-test "all" name - test to run all tests in given suite */
#define _DMA_UT_RUN_ALL_TEST_NAME "all"

/**
 * Meta-test "regression" name -
 * test to run all regression tests in given suite
 */
#define _DMA_UT_RUN_REGRESSION_TEST_NAME "regression"


 /* Test Log buffer name and size */
#define _DMA_UT_TEST_LOG_BUF_NAME ecpri_dma_ut_tst_log_buf
#define _DMA_UT_TEST_LOG_BUF_SIZE 8192

/* Global structure  for test fail execution result information */
#define _DMA_UT_TEST_FAIL_REPORT_DATA ecpri_dma_ut_tst_fail_report_data
#define _DMA_UT_TEST_FAIL_REPORT_SIZE 5
#define _DMA_UT_TEST_FAIL_REPORT_IDX ecpri_dma_ut_tst_fail_report_data_index

/* Start/End definitions of the array of suites */
#define DMA_UT_DEFINE_ALL_SUITES_START \
	static struct ecpri_dma_ut_suite *_DMA_UT_ALL_SUITES[] =
#define DMA_UT_DEFINE_ALL_SUITES_END

/**
 * Suites iterator - Array-like container
 * First index, number of elements  and element fetcher
 */
#define DMA_UT_SUITE_FIRST_INDEX 0
#define DMA_UT_SUITES_COUNT \
	ARRAY_SIZE(_DMA_UT_ALL_SUITES)
#define DMA_UT_GET_SUITE(__index) \
	_DMA_UT_ALL_SUITES[__index]

 /**
  * enum ecpri_dma_ut_test_result - Test execution result
  * @DMA_UT_TEST_RES_FAIL: Test executed and failed
  * @DMA_UT_TEST_RES_SUCCESS: Test executed and succeeded
  * @DMA_UT_TEST_RES_SKIP: Test was not executed.
  *
  * When running all tests in a suite, a specific test could
  * be skipped and not executed. For example due to mismatch of
  * DMA H/W version.
  */
enum ecpri_dma_ut_test_result {
    DMA_UT_TEST_RES_FAIL,
    DMA_UT_TEST_RES_SUCCESS,
    DMA_UT_TEST_RES_SKIP,
};

/**
 * enum ecpri_dma_ut_meta_test_type - Type of suite meta-test
 * @DMA_UT_META_TEST_ALL: Represents all tests in suite
 * @DMA_UT_META_TEST_REGRESSION: Represents all regression tests in suite
 */
enum ecpri_dma_ut_meta_test_type {
    DMA_UT_META_TEST_ALL,
    DMA_UT_META_TEST_REGRESSION,
};

#define DMA_UT_DRV_NAME "ecpri_dma_ut"

#define DMA_UT_DBG(fmt, args...) \
	do { \
		pr_debug(DMA_UT_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		DMA_IPC_LOGGING(ecpri_dma_get_ipc_logbuf(), \
			DMA_UT_DRV_NAME " %s:%d " fmt, ## args); \
		DMA_IPC_LOGGING(ecpri_dma_get_ipc_logbuf_low(), \
			DMA_UT_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define DMA_UT_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(DMA_UT_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		DMA_IPC_LOGGING(ecpri_dma_get_ipc_logbuf_low(), \
			DMA_UT_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define DMA_UT_ERR(fmt, args...) \
	do { \
		pr_err(DMA_UT_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		DMA_IPC_LOGGING(ecpri_dma_get_ipc_logbuf(), \
			DMA_UT_DRV_NAME " %s:%d " fmt, ## args); \
		DMA_IPC_LOGGING(ecpri_dma_get_ipc_logbuf_low(), \
			DMA_UT_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define DMA_UT_INFO(fmt, args...) \
	do { \
		pr_info(DMA_UT_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		DMA_IPC_LOGGING(ecpri_dma_get_ipc_logbuf(), \
			DMA_UT_DRV_NAME " %s:%d " fmt, ## args); \
		DMA_IPC_LOGGING(ecpri_dma_get_ipc_logbuf_low(), \
			DMA_UT_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

/**
 * struct ecpri_dma_ut_tst_fail_report - Information on test failure
 * @valid: When a test posts a report, valid will be marked true
 * @file: File name containing  the failed test.
 * @line: Number of line in the file where the test failed.
 * @func: Function where the test failed in.
 * @info: Information about the failure.
 */
struct ecpri_dma_ut_tst_fail_report {
	bool valid;
	const char *file;
	int line;
	const char *func;
	const char *info;
};

/**
 * Report on test failure
 * To be used by tests to report a point were a test fail.
 * Failures are saved in a stack manner.
 * Dumping the failure info will dump the fail reports
 *  from all the function in the calling stack
 */
#define DMA_UT_TEST_FAIL_REPORT(__info) \
	do { \
		extern struct ecpri_dma_ut_tst_fail_report \
			_DMA_UT_TEST_FAIL_REPORT_DATA \
			[_DMA_UT_TEST_FAIL_REPORT_SIZE]; \
		extern u32 _DMA_UT_TEST_FAIL_REPORT_IDX; \
		struct ecpri_dma_ut_tst_fail_report *entry; \
		if (_DMA_UT_TEST_FAIL_REPORT_IDX >= \
			_DMA_UT_TEST_FAIL_REPORT_SIZE) \
			break; \
		entry = &(_DMA_UT_TEST_FAIL_REPORT_DATA \
			[_DMA_UT_TEST_FAIL_REPORT_IDX]); \
		entry->file = __FILE__; \
		entry->line = __LINE__; \
		entry->func = __func__; \
		if (__info) \
			entry->info = __info; \
		else \
			entry->info = ""; \
		_DMA_UT_TEST_FAIL_REPORT_IDX++; \
	} while (0)

 /**
  * To be used by tests to log progress and ongoing information
  * Logs are not printed to user, but saved to a buffer.
  * I/S shall print the buffer at different occasions - e.g. in test failure
  */
#define DMA_UT_LOG(fmt, args...) \
	do { \
		extern char *_DMA_UT_TEST_LOG_BUF_NAME; \
		char __buf[512]; \
		DMA_UT_DBG(fmt, ## args); \
		if (!_DMA_UT_TEST_LOG_BUF_NAME) {\
			pr_err(DMA_UT_DRV_NAME " %s:%d " fmt, \
				__func__, __LINE__, ## args); \
			break; \
		} \
		scnprintf(__buf, sizeof(__buf), \
			" %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		strlcat(_DMA_UT_TEST_LOG_BUF_NAME, __buf, \
			_DMA_UT_TEST_LOG_BUF_SIZE); \
	} while (0)

  /**
   * struct ecpri_dma_ut_suite_meta - Suite meta-data
   * @name: Suite unique name
   * @desc: Suite description
   * @setup: Setup Call-back of the suite
   * @teardown: Teardown Call-back of the suite
   * @priv: Private pointer of the suite
   *
   * Setup/Teardown  will be called once for the suite when running a tests of it.
   * priv field is shared between the Setup/Teardown and the tests
   */
struct ecpri_dma_ut_suite_meta {
	char *name;
	char *desc;
	int (*setup)(void **ppriv);
	int (*teardown)(void *priv);
	void *priv;
};

/* Test suite data structure declaration */
struct ecpri_dma_ut_suite;

/**
 * struct ecpri_dma_ut_test - Test information
 * @name: Test name
 * @desc: Test description
 * @run: Test execution call-back
 * @run_in_regression: To run this test as part of regression?
 * @min_ecpri_dma_hw_ver: Minimum DMA H/W version where the test is supported?
 * @max_ecpri_dma_hw_ver: Maximum DMA H/W version where the test is supported?
 * @suite: Pointer to suite containing this test
 * @res: Test execution result. Will be updated after running a test as part
 * of suite tests run
 */
struct ecpri_dma_ut_test {
	char *name;
	char *desc;
	int (*run)(void *priv);
	bool run_in_regression;
	int min_ecpri_dma_hw_ver;
	int max_ecpri_dma_hw_ver;
	struct ecpri_dma_ut_suite *suite;
	enum ecpri_dma_ut_test_result res;
};

/**
 * struct ecpri_dma_ut_suite - Suite information
 * @meta_data: Pointer to meta-data structure of the suite
 * @tests: Pointer to array of tests belongs to the suite
 * @tests_cnt: Number of tests
 */
struct ecpri_dma_ut_suite {
	struct ecpri_dma_ut_suite_meta *meta_data;
	struct ecpri_dma_ut_test *tests;
	size_t tests_cnt;
};


/**
 * Add a test to a suite.
 * Will add entry to tests array and update its info with
 * the given info, thus adding new test.
 */
#define DMA_UT_ADD_TEST(__name, __desc, __run, __run_in_regression, \
	__min_ecpri_dma_hw_ver, __max_ecpri_dma__hw_ver) \
	{ \
		.name = #__name, \
		.desc = __desc, \
		.run = __run, \
		.run_in_regression = __run_in_regression, \
		.min_ecpri_dma_hw_ver = __min_ecpri_dma_hw_ver, \
		.max_ecpri_dma_hw_ver = __max_ecpri_dma__hw_ver, \
		.suite = NULL, \
	}

 /**
  * Declare a suite
  * Every suite need to be declared  before it is registered.
  */
#define DMA_UT_DECLARE_SUITE(__name) \
	extern struct ecpri_dma_ut_suite _DMA_UT_SUITE_DATA(__name)

  /**
   * Register a suite
   * Registering a suite is mandatory so it will be considered.
   */
#define DMA_UT_REGISTER_SUITE(__name) \
	(&_DMA_UT_SUITE_DATA(__name))

   /**
	* Start/End suite definition
	* Will create the suite global structures and adds adding tests to it.
	* Use DMA_UT_ADD_TEST() with these macros to add tests when defining
	* a suite
	*/
#define DMA_UT_DEFINE_SUITE_START(__name, __desc, __setup, __teardown) \
	static struct ecpri_dma_ut_suite_meta _DMA_UT_SUITE_META_DATA(__name) = \
	{ \
		.name = #__name, \
		.desc = __desc, \
		.setup = __setup, \
		.teardown = __teardown, \
	}; \
	static struct ecpri_dma_ut_test _DMA_UT_SUITE_TESTS(__name)[] =
#define DMA_UT_DEFINE_SUITE_END(__name) \
	; \
	struct ecpri_dma_ut_suite _DMA_UT_SUITE_DATA(__name) = \
	{ \
		.meta_data = &_DMA_UT_SUITE_META_DATA(__name), \
		.tests = _DMA_UT_SUITE_TESTS(__name), \
		.tests_cnt = ARRAY_SIZE(_DMA_UT_SUITE_TESTS(__name)), \
	}

#endif /* _DMA_UT_FRAMEWORK_H_ */
