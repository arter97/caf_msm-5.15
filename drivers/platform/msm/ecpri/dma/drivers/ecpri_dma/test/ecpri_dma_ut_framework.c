/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include "ecpri_dma_i.h"
#include "ecpri_dma_ut_framework.h"
#include "ecpri_dma_ut_suite_list.h"


#define DMA_UT_DEBUG_WRITE_BUF_SIZE 256
#define DMA_UT_DEBUG_READ_BUF_SIZE 1024

#define DMA_UT_READ_WRITE_DBG_FILE_MODE 0664

 /**
  * struct ecpri_dma_ut_context - I/S context
  * @inited: Will wait untill DMA is ready. Will create the enable file
  * @enabled: All tests and suite debugfs files are created
  * @lock: Lock for mutual exclustion
  * @ecpri_dma_dbgfs_root: DMA root debugfs folder
  * @test_dbgfs_root: UT root debugfs folder. Sub-folder of DMA root
  * @test_dbgfs_suites: Suites root debugfs folder. Sub-folder of UT root
  * @wq: workqueue struct for write operations
  */
struct ecpri_dma_ut_context {
	bool inited;
	bool enabled;
	struct mutex lock;
	struct dentry *ecpri_dma_dbgfs_root;
	struct dentry *test_dbgfs_root;
	struct dentry *test_dbgfs_suites;
	struct workqueue_struct *wq;
};

/**
 * struct ecpri_dma_ut_dbgfs_test_write_work_ctx - work_queue context
 * @dbgfs_Work: work_struct for the write_work
 * @meta_type: See enum ecpri_dma_ut_meta_test_type
 * @user_data: user data usually used to point to suite or test object
 */
struct ecpri_dma_ut_dbgfs_test_write_work_ctx {
	struct work_struct dbgfs_work;
	long meta_type;
	void *user_data;
};

static ssize_t ecpri_dma_ut_dbgfs_enable_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t ecpri_dma_ut_dbgfs_enable_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos);
static ssize_t ecpri_dma_ut_dbgfs_test_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t ecpri_dma_ut_dbgfs_test_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos);
static int ecpri_dma_ut_dbgfs_all_test_open(struct inode *inode,
	struct file *filp);
static int ecpri_dma_ut_dbgfs_regression_test_open(struct inode *inode,
	struct file *filp);
static ssize_t ecpri_dma_ut_dbgfs_meta_test_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t ecpri_dma_ut_dbgfs_meta_test_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos);


static const struct file_operations ecpri_dma_ut_dbgfs_enable_fops = {
	.read = ecpri_dma_ut_dbgfs_enable_read,
	.write = ecpri_dma_ut_dbgfs_enable_write,
};
static const struct file_operations ecpri_dma_ut_dbgfs_test_fops = {
	.read = ecpri_dma_ut_dbgfs_test_read,
	.write = ecpri_dma_ut_dbgfs_test_write,
};
static const struct file_operations ecpri_dma_ut_dbgfs_all_test_fops = {
	.open = ecpri_dma_ut_dbgfs_all_test_open,
	.read = ecpri_dma_ut_dbgfs_meta_test_read,
	.write = ecpri_dma_ut_dbgfs_meta_test_write,
};
static const struct file_operations ecpri_dma_ut_dbgfs_regression_test_fops = {
	.open = ecpri_dma_ut_dbgfs_regression_test_open,
	.read = ecpri_dma_ut_dbgfs_meta_test_read,
	.write = ecpri_dma_ut_dbgfs_meta_test_write,
};

static struct ecpri_dma_ut_context *ecpri_dma_ut_ctx;
char *_DMA_UT_TEST_LOG_BUF_NAME;
struct ecpri_dma_ut_tst_fail_report
	_DMA_UT_TEST_FAIL_REPORT_DATA[_DMA_UT_TEST_FAIL_REPORT_SIZE];
u32 _DMA_UT_TEST_FAIL_REPORT_IDX;

/**
 * ecpri_dma_ut_print_log_buf() - Dump given buffer via kernel error mechanism
 * @buf: Buffer to print
 *
 * Tokenize the string according to new-line and then print
 *
 * Note: Assumes lock acquired
 */
static void ecpri_dma_ut_print_log_buf(char *buf)
{
	char *token;

	if (!buf) {
		DMA_UT_ERR("Input error - no buf\n");
		return;
	}

	for (token = strsep(&buf, "\n"); token; token = strsep(&buf, "\n"))
		pr_err("%s\n", token);
}

/**
 * ecpri_dma_ut_dump_fail_report_stack() - dump the report info stack via kernel err
 *
 * Note: Assumes lock acquired
 */
static void ecpri_dma_ut_dump_fail_report_stack(void)
{
	int i;

	DMA_UT_DBG("Entry\n");

	if (_DMA_UT_TEST_FAIL_REPORT_IDX == 0) {
		DMA_UT_DBG("no report info\n");
		return;
	}

	for (i = 0; i < _DMA_UT_TEST_FAIL_REPORT_IDX; i++) {
		if (i == 0)
			pr_err("***** FAIL INFO STACK *****:\n");
		else
			pr_err("Called From:\n");

		pr_err("\tFILE = %s\n\tFUNC = %s()\n\tLINE = %d\n",
			_DMA_UT_TEST_FAIL_REPORT_DATA[i].file,
			_DMA_UT_TEST_FAIL_REPORT_DATA[i].func,
			_DMA_UT_TEST_FAIL_REPORT_DATA[i].line);
		pr_err("\t%s\n", _DMA_UT_TEST_FAIL_REPORT_DATA[i].info);
	}
}

/**
 * ecpri_dma_ut_show_suite_exec_summary() - Show tests run summary
 * @suite: suite to print its running summary
 *
 * Print list of succeeded tests, failed tests and skipped tests
 *
 * Note: Assumes lock acquired
 */
static void ecpri_dma_ut_show_suite_exec_summary(const struct ecpri_dma_ut_suite *suite)
{
	int i;

	DMA_UT_DBG("Entry\n");

	ecpri_dma_assert_on(!suite);

	pr_info("\n\n");
	pr_info("\t  Suite '%s' summary\n", suite->meta_data->name);
	pr_info("===========================\n");
	pr_info("Successful tests\n");
	pr_info("----------------\n");
	for (i = 0; i < suite->tests_cnt; i++) {
		if (suite->tests[i].res != DMA_UT_TEST_RES_SUCCESS)
			continue;
		pr_info("\t%s\n", suite->tests[i].name);
	}
	pr_info("\nFailed tests\n");
	pr_info("------------\n");
	for (i = 0; i < suite->tests_cnt; i++) {
		if (suite->tests[i].res != DMA_UT_TEST_RES_FAIL)
			continue;
		pr_info("\t%s\n", suite->tests[i].name);
	}
	pr_info("\nSkipped tests\n");
	pr_info("-------------\n");
	for (i = 0; i < suite->tests_cnt; i++) {
		if (suite->tests[i].res != DMA_UT_TEST_RES_SKIP)
			continue;
		pr_info("\t%s\n", suite->tests[i].name);
	}
	pr_info("\n");
}

/**
 * ecpri_dma_ut_dbgfs_meta_test_write_work_func() - Debugfs write func for a
 * for a meta test
 * @params: work struct containing write fops and completion object
 *
 * Used to run all/regression tests in a suite
 * Create log buffer that the test can use to store ongoing logs
 * DMA clocks need to be voted.
 * Run setup() once before running the tests and teardown() once after
 * If no such call-backs then ignore it; if failed then fail the suite
 * Print tests progress during running
 * Test log and fail report will be showed only if the test failed.
 * Finally show Summary of the suite tests running
 *
 * Note: If test supported DMA H/W version mismatch, skip it
 *	 If a test lack run function, skip it
 *	 If test doesn't belong to regression and it is regression run, skip it
 * Note: Running mode: Do not stop running on failure
 *
 * Return: Negative in failure, given characters amount in success
 */
static void ecpri_dma_ut_dbgfs_meta_test_write_work_func(struct work_struct *work)
{
	struct ecpri_dma_ut_dbgfs_test_write_work_ctx *write_work_ctx;
	struct ecpri_dma_ut_suite *suite;
	int i;
	enum ecpri_hw_ver ecpri_dma_ver;
	int rc = 0;
	long meta_type;
	bool tst_fail = false, ready = false;

	write_work_ctx = container_of(work, struct
		ecpri_dma_ut_dbgfs_test_write_work_ctx, dbgfs_work);

	DMA_UT_DBG("Entry\n");
	mutex_lock(&ecpri_dma_ctx->lock);
	ready = ecpri_dma_is_ready();
	mutex_unlock(&ecpri_dma_ctx->lock);
	ecpri_dma_assert_on(!ready);

	mutex_lock(&ecpri_dma_ut_ctx->lock);
	suite = (struct ecpri_dma_ut_suite *)(write_work_ctx->user_data);
	ecpri_dma_assert_on(!suite);
	meta_type = write_work_ctx->meta_type;
	DMA_UT_DBG("Meta test type %ld\n", meta_type);

	_DMA_UT_TEST_LOG_BUF_NAME = kzalloc(_DMA_UT_TEST_LOG_BUF_SIZE,
		GFP_KERNEL);
	if (!_DMA_UT_TEST_LOG_BUF_NAME) {
		DMA_UT_ERR("failed to allocate %d bytes\n",
			_DMA_UT_TEST_LOG_BUF_SIZE);
		rc = -ENOMEM;
		goto unlock_mutex;
	}

	if (!suite->tests_cnt || !suite->tests) {
		pr_info("No tests for suite '%s'\n", suite->meta_data->name);
		goto free_mem;
	}

	ecpri_dma_ver = ecpri_dma_ctx->ecpri_hw_ver;

	if (suite->meta_data->setup) {
		pr_info("*** Suite '%s': Run setup ***\n",
			suite->meta_data->name);
		rc = suite->meta_data->setup(&suite->meta_data->priv);
		if (rc) {
			DMA_UT_ERR("Setup failed for suite %s\n",
				suite->meta_data->name);
			rc = -EFAULT;
			goto release_clock;
		}
	} else {
		pr_info("*** Suite '%s': No Setup ***\n",
			suite->meta_data->name);
	}

	pr_info("*** Suite '%s': Run %s tests ***\n\n",
		suite->meta_data->name,
		meta_type == DMA_UT_META_TEST_REGRESSION ? "regression" : "all"
	);
	for (i = 0; i < suite->tests_cnt; i++) {
		if (meta_type == DMA_UT_META_TEST_REGRESSION &&
			!suite->tests[i].run_in_regression) {
			pr_info(
				"*** Test '%s': Skip - Not in regression ***\n\n"
				, suite->tests[i].name);
			suite->tests[i].res = DMA_UT_TEST_RES_SKIP;
			continue;
		}
		if (suite->tests[i].min_ecpri_dma_hw_ver > ecpri_dma_ver ||
			suite->tests[i].max_ecpri_dma_hw_ver < ecpri_dma_ver) {
			pr_info(
				"*** Test '%s': Skip - DMA VER mismatch ***\n\n"
				, suite->tests[i].name);
			suite->tests[i].res = DMA_UT_TEST_RES_SKIP;
			continue;
		}
		if (!suite->tests[i].run) {
			pr_info(
				"*** Test '%s': Skip - No Run function ***\n\n"
				, suite->tests[i].name);
			suite->tests[i].res = DMA_UT_TEST_RES_SKIP;
			continue;
		}

		_DMA_UT_TEST_LOG_BUF_NAME[0] = '\0';
		_DMA_UT_TEST_FAIL_REPORT_IDX = 0;
		pr_info("*** Test '%s': Running... ***\n",
			suite->tests[i].name);
		rc = suite->tests[i].run(suite->meta_data->priv);
		if (rc) {
			tst_fail = true;
			suite->tests[i].res = DMA_UT_TEST_RES_FAIL;
			ecpri_dma_ut_print_log_buf(_DMA_UT_TEST_LOG_BUF_NAME);
		} else {
			suite->tests[i].res = DMA_UT_TEST_RES_SUCCESS;
		}

		pr_info(">>>>>>**** TEST '%s': %s ****<<<<<<\n",
			suite->tests[i].name, tst_fail ? "FAIL" : "SUCCESS");

		if (tst_fail)
			ecpri_dma_ut_dump_fail_report_stack();

		pr_info("\n");
	}

	if (suite->meta_data->teardown) {
		pr_info("*** Suite '%s': Run Teardown ***\n",
			suite->meta_data->name);
		rc = suite->meta_data->teardown(suite->meta_data->priv);
		if (rc) {
			DMA_UT_ERR("Teardown failed for suite %s\n",
				suite->meta_data->name);
			rc = -EFAULT;
			goto release_clock;
		}
	} else {
		pr_info("*** Suite '%s': No Teardown ***\n",
			suite->meta_data->name);
	}

	ecpri_dma_ut_show_suite_exec_summary(suite);

release_clock:
	//No need to release clock in Lassen
free_mem:
	kfree(_DMA_UT_TEST_LOG_BUF_NAME);
	_DMA_UT_TEST_LOG_BUF_NAME = NULL;
unlock_mutex:
	mutex_unlock(&ecpri_dma_ut_ctx->lock);
	kfree(write_work_ctx);
}

/*
 * ecpri_dma_ut_dbgfs_meta_test_write() - Debugfs write func for a meta test
 * @params: write fops
 *
 * Run all tests in a suite using a work queue so it does not race with
 * debugfs_remove_recursive
 *
 * Return: Negative if failure. Amount of characters written if success.
 */
static ssize_t ecpri_dma_ut_dbgfs_meta_test_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	struct ecpri_dma_ut_dbgfs_test_write_work_ctx *write_work_ctx;

	write_work_ctx = kzalloc(sizeof(*write_work_ctx), GFP_KERNEL);
	if (!write_work_ctx) {
		DMA_UT_ERR("kzalloc err.\n");
		return -ENOMEM;
	}

	write_work_ctx->user_data = file->f_inode->i_private;
	write_work_ctx->meta_type = (long)(file->private_data);

	INIT_WORK(&write_work_ctx->dbgfs_work,
		ecpri_dma_ut_dbgfs_meta_test_write_work_func);

	queue_work(ecpri_dma_ut_ctx->wq, &write_work_ctx->dbgfs_work);

	return count;
}

/**
 * ecpri_dma_ut_dbgfs_meta_test_read() - Debugfs read func for a meta test
 * @params: read fops
 *
 * Meta test, is a test that describes other test or bunch of tests.
 *  for example, the 'all' test. Running this test will run all
 *  the tests in the suite.
 *
 * Show information regard the suite. E.g. name and description
 * If regression - List the regression tests names
 *
 * Return: Amount of characters written to user space buffer
 */
static ssize_t ecpri_dma_ut_dbgfs_meta_test_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	char *buf;
	struct ecpri_dma_ut_suite *suite;
	int nbytes;
	ssize_t cnt;
	long meta_type;
	int i;

	DMA_UT_DBG("Entry\n");

	mutex_lock(&ecpri_dma_ut_ctx->lock);
	suite = file->f_inode->i_private;
	ecpri_dma_assert_on(!suite);
	meta_type = (long)(file->private_data);
	DMA_UT_DBG("Meta test type %ld\n", meta_type);

	buf = kmalloc(DMA_UT_DEBUG_READ_BUF_SIZE + 1, GFP_KERNEL);
	if (!buf) {
		DMA_UT_ERR("failed to allocate %d bytes\n",
			DMA_UT_DEBUG_READ_BUF_SIZE + 1);
		cnt = 0;
		goto unlock_mutex;
	}

	if (meta_type == DMA_UT_META_TEST_ALL) {
		nbytes = scnprintf(buf, DMA_UT_DEBUG_READ_BUF_SIZE,
			"\tMeta-test running all the tests in the suite:\n"
			"\tSuite Name: %s\n"
			"\tDescription: %s\n"
			"\tNumber of test in suite: %zu\n",
			suite->meta_data->name,
			suite->meta_data->desc ? : "",
			suite->tests_cnt);
	} else {
		nbytes = scnprintf(buf, DMA_UT_DEBUG_READ_BUF_SIZE,
			"\tMeta-test running regression tests in the suite:\n"
			"\tSuite Name: %s\n"
			"\tDescription: %s\n"
			"\tRegression tests:\n",
			suite->meta_data->name,
			suite->meta_data->desc ? : "");
		for (i = 0; i < suite->tests_cnt; i++) {
			if (!suite->tests[i].run_in_regression)
				continue;
			nbytes += scnprintf(buf + nbytes,
				DMA_UT_DEBUG_READ_BUF_SIZE - nbytes,
				"\t\t%s\n", suite->tests[i].name);
		}
	}

	cnt = simple_read_from_buffer(ubuf, count, ppos, buf, nbytes);
	kfree(buf);

unlock_mutex:
	mutex_unlock(&ecpri_dma_ut_ctx->lock);
	return cnt;
}

/**
 * ecpri_dma_ut_dbgfs_regression_test_open() - Debugfs open function for
 * 'regression' tests
 * @params: open fops
 *
 * Mark "Regression tests" for meta-tests later operations.
 *
 * Return: Zero (always success).
 */
static int ecpri_dma_ut_dbgfs_regression_test_open(struct inode *inode,
	struct file *filp)
{
	DMA_UT_DBG("Entry\n");

	filp->private_data = (void *)(DMA_UT_META_TEST_REGRESSION);

	return 0;
}

/**
 * ecpri_dma_ut_dbgfs_all_test_open() - Debugfs open function for 'all' tests
 * @params: open fops
 *
 * Mark "All tests" for meta-tests later operations.
 *
 * Return: Zero (always success).
 */
static int ecpri_dma_ut_dbgfs_all_test_open(struct inode *inode,
	struct file *filp)
{
	DMA_UT_DBG("Entry\n");

	filp->private_data = (void *)(DMA_UT_META_TEST_ALL);

	return 0;
}

/**
 * ecpri_dma_ut_dbgfs_test_write() - Debugfs write function for a test
 * @params: write fops
 *
 * Used to run a test.
 * Create log buffer that the test can use to store ongoing logs
 * DMA clocks need to be voted.
 * Run setup() before the test and teardown() after the tests.
 * If no such call-backs then ignore it; if failed then fail the test
 * If all succeeds, no printing to user
 * If failed, test logs and failure report will be printed to user
 *
 * Note: Test must has run function and it's supported DMA H/W version
 * must be matching. Otherwise test will fail.
 *
 * Return: Negative in failure, given characters amount in success
 */
static void ecpri_dma_ut_dbgfs_test_write_work_func(struct work_struct *work)
{
	struct ecpri_dma_ut_dbgfs_test_write_work_ctx *write_work_ctx;
	struct ecpri_dma_ut_test *test;
	struct ecpri_dma_ut_suite *suite;
	bool tst_fail = false, ready = false;
	int rc = 0;
	enum ecpri_hw_ver ecpri_dma_ver;

	write_work_ctx = container_of(work, struct
		ecpri_dma_ut_dbgfs_test_write_work_ctx, dbgfs_work);

	DMA_UT_DBG("Entry\n");
	mutex_lock(&ecpri_dma_ctx->lock);
	ready = ecpri_dma_is_ready();
	mutex_unlock(&ecpri_dma_ctx->lock);
	ecpri_dma_assert_on(!ready);

	mutex_lock(&ecpri_dma_ut_ctx->lock);
	test = (struct ecpri_dma_ut_test *)(write_work_ctx->user_data);
	ecpri_dma_assert_on(!test);

	_DMA_UT_TEST_LOG_BUF_NAME = kzalloc(_DMA_UT_TEST_LOG_BUF_SIZE,
		GFP_KERNEL);
	if (!_DMA_UT_TEST_LOG_BUF_NAME) {
		DMA_UT_ERR("failed to allocate %d bytes\n",
			_DMA_UT_TEST_LOG_BUF_SIZE);
		rc = -ENOMEM;
		goto unlock_mutex;
	}

	if (!test->run) {
		DMA_UT_ERR("*** Test %s - No run func ***\n",
			test->name);
		rc = -EFAULT;
		goto free_mem;
	}

	ecpri_dma_ver = ecpri_dma_ctx->ecpri_hw_ver;
	if (test->min_ecpri_dma_hw_ver > ecpri_dma_ver ||
		test->max_ecpri_dma_hw_ver < ecpri_dma_ver) {
		DMA_UT_ERR("Cannot run test %s on DMA HW Ver %s\n",
			test->name, ecpri_dma_get_version_string(ecpri_dma_ver));
		rc = -EFAULT;
		goto free_mem;
	}

	suite = test->suite;
	if (!suite || !suite->meta_data) {
		DMA_UT_ERR("test %s with invalid suite\n", test->name);
		rc = -EINVAL;
		goto free_mem;
	}

	if (suite->meta_data->setup) {
		DMA_UT_DBG("*** Suite '%s': Run setup ***\n",
			suite->meta_data->name);
		rc = suite->meta_data->setup(&suite->meta_data->priv);
		if (rc) {
			DMA_UT_ERR("Setup failed for suite %s\n",
				suite->meta_data->name);
			rc = -EFAULT;
			goto release_clock;
		}
	} else {
		DMA_UT_DBG("*** Suite '%s': No Setup ***\n",
			suite->meta_data->name);
	}

	DMA_UT_DBG("*** Test '%s': Running... ***\n", test->name);
	_DMA_UT_TEST_FAIL_REPORT_IDX = 0;
	rc = test->run(suite->meta_data->priv);
	if (rc)
		tst_fail = true;
	DMA_UT_DBG("*** Test %s - ***\n", tst_fail ? "FAIL" : "SUCCESS");
	if (tst_fail) {
		pr_info("=================>>>>>>>>>>>\n");
		ecpri_dma_ut_print_log_buf(_DMA_UT_TEST_LOG_BUF_NAME);
		pr_info("**** TEST %s FAILED ****\n", test->name);
		ecpri_dma_ut_dump_fail_report_stack();
		pr_info("<<<<<<<<<<<=================\n");
	}

	if (suite->meta_data->teardown) {
		DMA_UT_DBG("*** Suite '%s': Run Teardown ***\n",
			suite->meta_data->name);
		rc = suite->meta_data->teardown(suite->meta_data->priv);
		if (rc) {
			DMA_UT_ERR("Teardown failed for suite %s\n",
				suite->meta_data->name);
			rc = -EFAULT;
			goto release_clock;
		}
	} else {
		DMA_UT_DBG("*** Suite '%s': No Teardown ***\n",
			suite->meta_data->name);
	}

release_clock:
	//No need to release clock in Lassen
free_mem:
	kfree(_DMA_UT_TEST_LOG_BUF_NAME);
	_DMA_UT_TEST_LOG_BUF_NAME = NULL;
unlock_mutex:
	mutex_unlock(&ecpri_dma_ut_ctx->lock);
	kfree(write_work_ctx);
}

static ssize_t ecpri_dma_ut_dbgfs_test_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	struct ecpri_dma_ut_dbgfs_test_write_work_ctx *write_work_ctx;

	write_work_ctx = kzalloc(sizeof(*write_work_ctx), GFP_KERNEL);
	if (!write_work_ctx) {
		DMA_UT_ERR("kzalloc err.\n");
		return -ENOMEM;
	}

	write_work_ctx->user_data = file->f_inode->i_private;
	write_work_ctx->meta_type = (long)(file->private_data);

	INIT_WORK(&write_work_ctx->dbgfs_work,
		ecpri_dma_ut_dbgfs_test_write_work_func);

	queue_work(ecpri_dma_ut_ctx->wq, &write_work_ctx->dbgfs_work);

	return count;
}
/**
 * ecpri_dma_ut_dbgfs_test_read() - Debugfs read function for a test
 * @params: read fops
 *
 * print information regard the test. E.g. name and description
 *
 * Return: Amount of characters written to user space buffer
 */
static ssize_t ecpri_dma_ut_dbgfs_test_read(struct file *file, char __user *ubuf,
	size_t count, loff_t *ppos)
{
	char *buf;
	struct ecpri_dma_ut_test *test;
	int nbytes;
	ssize_t cnt;

	DMA_UT_DBG("Entry\n");

	mutex_lock(&ecpri_dma_ut_ctx->lock);
	test = file->f_inode->i_private;
	ecpri_dma_assert_on(!test);

	buf = kmalloc(DMA_UT_DEBUG_READ_BUF_SIZE, GFP_KERNEL);
	if (!buf) {
		DMA_UT_ERR("failed to allocate %d bytes\n",
			DMA_UT_DEBUG_READ_BUF_SIZE);
		cnt = 0;
		goto unlock_mutex;
	}

	nbytes = scnprintf(buf, DMA_UT_DEBUG_READ_BUF_SIZE,
		"\t Test Name: %s\n"
		"\t Description: %s\n"
		"\t Suite Name: %s\n"
		"\t Run In Regression: %s\n"
		"\t Supported DMA versions: [%s -> %s]\n",
		test->name, test->desc ? : "", test->suite->meta_data->name,
		test->run_in_regression ? "Yes" : "No",
		ecpri_dma_get_version_string(test->min_ecpri_dma_hw_ver),
		test->max_ecpri_dma_hw_ver == ECPRI_HW_MAX ? "MAX" :
		ecpri_dma_get_version_string(test->max_ecpri_dma_hw_ver));

	if (nbytes > count)
		DMA_UT_ERR("User buf too small - return partial info\n");

	cnt = simple_read_from_buffer(ubuf, count, ppos, buf, nbytes);
	kfree(buf);

unlock_mutex:
	mutex_unlock(&ecpri_dma_ut_ctx->lock);
	return cnt;
}

/**
 * ecpri_dma_ut_framework_load_suites() - Load tests and expose them to user space
 *
 * Creates debugfs folder for each suite and then file for each test in it.
 * Create debugfs "all" file for each suite for meta-test to run all tests.
 *
 * Note: Assumes lock acquired
 *
 * Return: Zero in success, otherwise in failure
 */
static int ecpri_dma_ut_framework_load_suites(void)
{
	int suite_idx;
	int tst_idx;
	struct ecpri_dma_ut_suite *suite;
	struct dentry *s_dent;
	struct dentry *f_dent;

	DMA_UT_DBG("Entry\n");

	for (suite_idx = DMA_UT_SUITE_FIRST_INDEX;
		suite_idx < DMA_UT_SUITES_COUNT; suite_idx++) {
		suite = DMA_UT_GET_SUITE(suite_idx);

		if (!suite->meta_data->name) {
			DMA_UT_ERR("No suite name\n");
			return -EFAULT;
		}

		s_dent = debugfs_create_dir(suite->meta_data->name,
			ecpri_dma_ut_ctx->test_dbgfs_suites);

		if (!s_dent || IS_ERR(s_dent)) {
			DMA_UT_ERR("fail create dbg entry - suite %s\n",
				suite->meta_data->name);
			return -EFAULT;
		}

		for (tst_idx = 0; tst_idx < suite->tests_cnt; tst_idx++) {
			if (!suite->tests[tst_idx].name) {
				DMA_UT_ERR("No test name on suite %s\n",
					suite->meta_data->name);
				return -EFAULT;
			}
			f_dent = debugfs_create_file(
				suite->tests[tst_idx].name,
				DMA_UT_READ_WRITE_DBG_FILE_MODE, s_dent,
				&suite->tests[tst_idx],
				&ecpri_dma_ut_dbgfs_test_fops);
			if (!f_dent || IS_ERR(f_dent)) {
				DMA_UT_ERR("fail create dbg entry - tst %s\n",
					suite->tests[tst_idx].name);
				return -EFAULT;
			}
		}

		/* entry for meta-test all to run all tests in suites */
		f_dent = debugfs_create_file(_DMA_UT_RUN_ALL_TEST_NAME,
			DMA_UT_READ_WRITE_DBG_FILE_MODE, s_dent,
			suite, &ecpri_dma_ut_dbgfs_all_test_fops);
		if (!f_dent || IS_ERR(f_dent)) {
			DMA_UT_ERR("fail to create dbg entry - %s\n",
				_DMA_UT_RUN_ALL_TEST_NAME);
			return -EFAULT;
		}

		/*
		 * entry for meta-test regression to run all regression
		 * tests in suites
		 */
		f_dent = debugfs_create_file(_DMA_UT_RUN_REGRESSION_TEST_NAME,
			DMA_UT_READ_WRITE_DBG_FILE_MODE, s_dent,
			suite, &ecpri_dma_ut_dbgfs_regression_test_fops);
		if (!f_dent || IS_ERR(f_dent)) {
			DMA_UT_ERR("fail to create dbg entry - %s\n",
				_DMA_UT_RUN_ALL_TEST_NAME);
			return -EFAULT;
		}
	}

	return 0;
}

/**
 * ecpri_dma_ut_framework_enable() - Enable the framework
 *
 * Creates the tests and suites debugfs entries and load them.
 * This will expose the tests to user space.
 *
 * Return: Zero in success, otherwise in failure
 */
static int ecpri_dma_ut_framework_enable(void)
{
	int ret = 0;

	DMA_UT_DBG("Entry\n");

	mutex_lock(&ecpri_dma_ut_ctx->lock);

	if (ecpri_dma_ut_ctx->enabled) {
		DMA_UT_ERR("Already enabled\n");
		goto unlock_mutex;
	}

	ecpri_dma_ut_ctx->test_dbgfs_suites = debugfs_create_dir("suites",
		ecpri_dma_ut_ctx->test_dbgfs_root);
	if (!ecpri_dma_ut_ctx->test_dbgfs_suites ||
		IS_ERR(ecpri_dma_ut_ctx->test_dbgfs_suites)) {
		DMA_UT_ERR("failed to create suites debugfs dir\n");
		ret = -EFAULT;
		goto unlock_mutex;
	}

	if (ecpri_dma_ut_framework_load_suites()) {
		DMA_UT_ERR("failed to load the suites into debugfs\n");
		ret = -EFAULT;
		goto fail_clean_suites_dbgfs;
	}

	ecpri_dma_ut_ctx->enabled = true;
	goto unlock_mutex;

fail_clean_suites_dbgfs:
	debugfs_remove_recursive(ecpri_dma_ut_ctx->test_dbgfs_suites);
unlock_mutex:
	mutex_unlock(&ecpri_dma_ut_ctx->lock);
	return ret;
}

/**
 * ecpri_dma_ut_framework_disable() - Disable the framework
 *
 * Remove the tests and suites debugfs exposure.
 *
 * Return: Zero in success, otherwise in failure
 */
static int ecpri_dma_ut_framework_disable(void)
{
	DMA_UT_DBG("Entry\n");

	mutex_lock(&ecpri_dma_ut_ctx->lock);

	if (!ecpri_dma_ut_ctx->enabled) {
		DMA_UT_ERR("Already disabled\n");
		goto unlock_mutex;
	}

	debugfs_remove_recursive(ecpri_dma_ut_ctx->test_dbgfs_suites);

	ecpri_dma_ut_ctx->enabled = false;

unlock_mutex:
	mutex_unlock(&ecpri_dma_ut_ctx->lock);
	return 0;
}

/**
 * ecpri_dma_ut_dbgfs_enable_write() - Debugfs enable file write fops
 * @params: write fops
 *
 * Input should be number. If 0, then disable. Otherwise enable.
 *
 * Return: if failed then negative value, if succeeds, amount of given chars
 */
static ssize_t ecpri_dma_ut_dbgfs_enable_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	char lcl_buf[DMA_UT_DEBUG_WRITE_BUF_SIZE];
	s8 option = 0;
	int ret;

	DMA_UT_DBG("Entry\n");

	if (count >= sizeof(lcl_buf)) {
		DMA_UT_ERR("No enough space\n");
		return -E2BIG;
	}

	if (copy_from_user(lcl_buf, buf, count)) {
		DMA_UT_ERR("fail to copy buf from user space\n");
		return -EFAULT;
	}

	lcl_buf[count] = '\0';
	if (kstrtos8(lcl_buf, 0, &option)) {
		DMA_UT_ERR("fail convert str to s8\n");
		return -EINVAL;
	}

	if (option == 0)
		ret = ecpri_dma_ut_framework_disable();
	else
		ret = ecpri_dma_ut_framework_enable();

	return ret ? : count;
}

/**
 * ecpri_dma_ut_dbgfs_enable_read() - Debugfs enable file read fops
 * @params: read fops
 *
 * To show to user space if the I/S is enabled or disabled.
 *
 * Return: amount of characters returned to user space
 */
static ssize_t ecpri_dma_ut_dbgfs_enable_read(struct file *file, char __user *ubuf,
	size_t count, loff_t *ppos)
{
	const char *status;

	DMA_UT_DBG("Entry\n");

	mutex_lock(&ecpri_dma_ut_ctx->lock);
	status = ecpri_dma_ut_ctx->enabled ?
		"Enabled - Write 0 to disable\n" :
		"Disabled - Write 1 to enable\n";
	mutex_unlock(&ecpri_dma_ut_ctx->lock);
	return simple_read_from_buffer(ubuf, count, ppos,
		status, strlen(status));
}

/**
 * ecpri_dma_ut_framework_init() - Unit-tests framework initialization
 *
 * Complete tests initialization: Each tests needs to point to it's
 * corresponing suite.
 * Creates the framework debugfs root directory  under DMA directory.
 * Create enable debugfs file - to enable/disable the framework.
 *
 * Return: Zero in success, otherwise in failure
 */
static int ecpri_dma_ut_framework_init(void)
{
	struct dentry *dfile_enable;
	int ret;
	int suite_idx;
	int test_idx;
	struct ecpri_dma_ut_suite *suite;

	DMA_UT_DBG("Entry\n");

	ecpri_dma_assert_on(!ecpri_dma_ut_ctx);

#ifdef CONFIG_DEBUG_FS
	ecpri_dma_ut_ctx->ecpri_dma_dbgfs_root = ecpri_dma_debugfs_get_root();
#endif
	if (!ecpri_dma_ut_ctx->ecpri_dma_dbgfs_root) {
		DMA_UT_ERR("No DMA debugfs root entry\n");
		return -EFAULT;
	}

	mutex_lock(&ecpri_dma_ut_ctx->lock);

	/* tests needs to point to their corresponding suites structures */
	for (suite_idx = DMA_UT_SUITE_FIRST_INDEX;
		suite_idx < DMA_UT_SUITES_COUNT; suite_idx++) {
		suite = DMA_UT_GET_SUITE(suite_idx);
		ecpri_dma_assert_on(!suite);
		if (!suite->tests) {
			DMA_UT_DBG("No tests for suite %s\n",
				suite->meta_data->name);
			continue;
		}
		for (test_idx = 0; test_idx < suite->tests_cnt; test_idx++) {
			suite->tests[test_idx].suite = suite;
			DMA_UT_DBG("Updating test %s info for suite %s\n",
				suite->tests[test_idx].name,
				suite->meta_data->name);
		}
	}

	ecpri_dma_ut_ctx->wq = create_singlethread_workqueue("ecpri_dma_ut_dbgfs");
	if (!ecpri_dma_ut_ctx->wq) {
		DMA_UT_ERR("create workqueue failed\n");
		ret = -ENOMEM;
		goto unlock_mutex;
	}

	ecpri_dma_ut_ctx->test_dbgfs_root = debugfs_create_dir("test",
		ecpri_dma_ut_ctx->ecpri_dma_dbgfs_root);
	if (!ecpri_dma_ut_ctx->test_dbgfs_root ||
		IS_ERR(ecpri_dma_ut_ctx->test_dbgfs_root)) {
		DMA_UT_ERR("failed to create test debugfs dir\n");
		ret = -EFAULT;
		destroy_workqueue(ecpri_dma_ut_ctx->wq);
		goto unlock_mutex;
	}

	dfile_enable = debugfs_create_file("enable",
		DMA_UT_READ_WRITE_DBG_FILE_MODE,
		ecpri_dma_ut_ctx->test_dbgfs_root, 0, &ecpri_dma_ut_dbgfs_enable_fops);
	if (!dfile_enable || IS_ERR(dfile_enable)) {
		DMA_UT_ERR("failed to create enable debugfs file\n");
		ret = -EFAULT;
		destroy_workqueue(ecpri_dma_ut_ctx->wq);
		goto fail_clean_dbgfs;
	}

	_DMA_UT_TEST_FAIL_REPORT_IDX = 0;
	ecpri_dma_ut_ctx->inited = true;
	DMA_UT_DBG("Done\n");
	ret = 0;
	goto unlock_mutex;

fail_clean_dbgfs:
	debugfs_remove_recursive(ecpri_dma_ut_ctx->test_dbgfs_root);
unlock_mutex:
	mutex_unlock(&ecpri_dma_ut_ctx->lock);
	return ret;
}

/**
 * ecpri_dma_ut_framework_destroy() - Destroy the UT framework info
 *
 * Disable it if enabled.
 * Remove the debugfs entries using the root entry
 */
static void ecpri_dma_ut_framework_destroy(void)
{
	DMA_UT_DBG("Entry\n");

	mutex_lock(&ecpri_dma_ut_ctx->lock);
	destroy_workqueue(ecpri_dma_ut_ctx->wq);
	if (ecpri_dma_ut_ctx->enabled)
		ecpri_dma_ut_framework_disable();
	if (ecpri_dma_ut_ctx->inited)
		debugfs_remove_recursive(ecpri_dma_ut_ctx->test_dbgfs_root);
	mutex_unlock(&ecpri_dma_ut_ctx->lock);
}

/**
 * ecpri_dma_ut_ecpri_dma_ready_cb() - DMA ready CB
 *
 * Once DMA is ready starting initializing  the unit-test framework
 */
static void ecpri_dma_ut_ecpri_dma_ready_cb(void *user_data)
{
	DMA_UT_DBG("Entry\n");
	(void)ecpri_dma_ut_framework_init();
}

/**
 * ecpri_dma_ut_module_init() - Module init
 *
 * Create the framework context, wait for DMA driver readiness
 * and Initialize it.
 * If DMA driver already ready, continue initialization immediately.
 * if not, wait for DMA ready notification by DMA driver context
 */
int ecpri_dma_ut_module_init(void)
{
	int ret = 0;
	bool init_framewok = true, ready = false;

	DMA_UT_INFO("Loading DMA test module...\n");

	ecpri_dma_ut_ctx = kzalloc(sizeof(struct ecpri_dma_ut_context), GFP_KERNEL);
	if (!ecpri_dma_ut_ctx) {
		DMA_UT_ERR("Failed to allocate ctx\n");
		return -ENOMEM;
	}
	mutex_init(&ecpri_dma_ut_ctx->lock);

	mutex_lock(&ecpri_dma_ctx->lock);
	ready = ecpri_dma_is_ready();
	mutex_unlock(&ecpri_dma_ctx->lock);

	if (!ready) {
		init_framewok = false;

		DMA_UT_DBG("DMA driver not ready, registering callback\n");

		ret = ecpri_dma_register_ready_cb(ecpri_dma_ut_ecpri_dma_ready_cb, NULL);

		/*
		 * If the call to ecpri_dma_register_ready_cb() above
		 * returns 0, this means that we've succeeded in
		 * queuing up a future call to ecpri_dma_ut_framework_init()
		 * and that the call to it will be made once the DMA
		 * becomes ready.  If this is the case, the call to
		 * ecpri_dma_ut_framework_init() below need not be made.
		 *
		 * If the call to ecpri_dma_register_ready_cb() above
		 * returns -EEXIST, it means that during the call to
		 * ecpri_dma_register_ready_cb(), the DMA has become
		 * ready, and hence, no indirect call to
		 * ecpri_dma_ut_framework_init() will be made, so we need to
		 * call it ourselves below.
		 *
		 * If the call to ecpri_dma_register_ready_cb() above
		 * return something other than 0 or -EEXIST, that's a
		 * hard error.
		 */
		if (ret == -EEXIST) {
			init_framewok = true;
		} else {
			if (ret) {
				DMA_UT_ERR("DMA CB reg failed - %d\n", ret);
				kfree(ecpri_dma_ut_ctx);
				ecpri_dma_ut_ctx = NULL;
			}
			return ret;
		}
	}

	if (init_framewok) {
		ret = ecpri_dma_ut_framework_init();
		if (ret) {
			DMA_UT_ERR("framework init failed\n");
			kfree(ecpri_dma_ut_ctx);
			ecpri_dma_ut_ctx = NULL;
		}
	}

	return ret;
}

/**
 * ecpri_dma_ut_module_exit() - Module exit function
 *
 * Destroys the Framework and removes its context
 */
void ecpri_dma_ut_module_exit(void)
{
	DMA_UT_DBG("Entry\n");

	if (!ecpri_dma_ut_ctx)
		return;

	ecpri_dma_ut_framework_destroy();
	kfree(ecpri_dma_ut_ctx);
	ecpri_dma_ut_ctx = NULL;
}
