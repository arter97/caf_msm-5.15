/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "ecpri_dma_utils.h"
#include "ecpri_dma_ss_core.h"
#include "ecpri_dma_ut_framework.h"

extern struct ecpri_dma_ss_core_context* ecpri_dma_ss_core_ctx;

#define ECPRI_DMA_SS_CORE_CLIENT_UT_READY_RCVD_TMOUT_MS (500)
#define ECPRI_DMA_SS_CORE_CLIENT_UT_READY_USER_DATA     (0xDEAD0001)
#define ECPRI_DMA_SS_CORE_CLIENT_UT_DMA_EVENT_USER_DATA (0xDEAD0002)
#define ECPRI_DMA_SS_CORE_CLIENT_UT_LOG_MSG_USER_DATA   (0xDEAD0003)

/**
 * ecpri_dma_ss_core_suite_context - all the information that
 * has to be available globally during the test
 *
 * @userdata_dma_event_notify:
 * @userdata_log_msg:
 * @userdata_ready: user data for Ready notify callback
 * @ready_received: Callback notier of DMA driver is ready
 *
 */
struct ecpri_dma_ss_core_suite_context {
	u32 userdata_dma_event_notify;
	u32 userdata_log_msg;
	u32 userdata_ready;
	struct completion ready_received;
	struct ecpri_dma_ecpri_ss_register_params ready_info;
};

struct ecpri_dma_ss_core_suite_context ecpri_dma_ss_core_suite_ctx;

static void ecpri_dma_ss_core_client_test_ready_cb(
	void* userdata_ready)
{
	if (ecpri_dma_ss_core_suite_ctx.userdata_ready
		!= (*(u32*)userdata_ready)) {
		DMA_UT_TEST_FAIL_REPORT("Received mismatched "
			"Ready user data\n");
		return;
	}

	complete(&ecpri_dma_ss_core_suite_ctx.ready_received);
}

static void ecpri_dma_ss_core_client_test_event_notify_cb(
	void* user_data,
	enum ecpri_dma_event_type ev_type)
{
	if (ecpri_dma_ss_core_suite_ctx.userdata_dma_event_notify
		!= (*(u32*)user_data)) {
		DMA_UT_TEST_FAIL_REPORT("Received mismatched "
			"DMA Event Notify user data\n");
		return;
	}
}

static void ecpri_dma_ss_core_client_test_log_msg_cb(
	void* user_data,
	const char* fmt, ...)
{
	if (ecpri_dma_ss_core_suite_ctx.userdata_log_msg != (*(u32*)user_data)) {
		DMA_UT_TEST_FAIL_REPORT("Received mismatched "
			"Log_Msg user data\n");
		return;
	}
}

/**
 * ecpri_dma_ss_core_client_test_util_create_params() - mock ETH
 * callbacks and register provides DMA driver with ETH client data
 *
 */
static void ecpri_dma_ss_core_client_test_util_create_params(
	bool* is_dma_ready,
	struct ecpri_dma_ecpri_ss_register_params* ready_info)
{
	DMA_UT_DBG("ENTRY\n");
	ready_info->notify_ready =
		&ecpri_dma_ss_core_client_test_ready_cb;
	ready_info->userdata_ready =
		&ecpri_dma_ss_core_suite_ctx.userdata_ready;

	ready_info->dma_event_notify =
		&ecpri_dma_ss_core_client_test_event_notify_cb;
	ready_info->userdata_dma_event_notify =
		&ecpri_dma_ss_core_suite_ctx.userdata_dma_event_notify;

	ready_info->log_msg =
		&ecpri_dma_ss_core_client_test_log_msg_cb;
	ready_info->userdata_log_msg =
		&ecpri_dma_ss_core_suite_ctx.userdata_log_msg;
}

static int ecpri_dma_ss_core_client_test_suite_setup(void** ppriv)
{
	DMA_UT_DBG("Start Setup\n");

	memset(&ecpri_dma_ss_core_suite_ctx, 0,
		sizeof(ecpri_dma_ss_core_suite_ctx));

	init_completion(
		&ecpri_dma_ss_core_suite_ctx.ready_received);

	ecpri_dma_ss_core_suite_ctx.userdata_ready =
		ECPRI_DMA_SS_CORE_CLIENT_UT_READY_USER_DATA;

	ecpri_dma_ss_core_suite_ctx.userdata_dma_event_notify =
		ECPRI_DMA_SS_CORE_CLIENT_UT_DMA_EVENT_USER_DATA;

	ecpri_dma_ss_core_suite_ctx.userdata_log_msg =
		ECPRI_DMA_SS_CORE_CLIENT_UT_LOG_MSG_USER_DATA;

	return 0;
}

static int ecpri_dma_ss_core_client_test_suite_teardown(void* priv)
{
	DMA_UT_DBG("Start Teardown\n");

	return 0;
}

static int ecpri_dma_ss_core_client_test_suite_registration(void* priv) {
	int ret = 0;
	bool is_dma_ready = false;

	DMA_UT_DBG("Start Registration\n");

	/* Register */
	DMA_UT_DBG("Prepare params\n");
	ecpri_dma_ss_core_client_test_util_create_params(&is_dma_ready,
		&ecpri_dma_ss_core_suite_ctx.ready_info);

	DMA_UT_DBG("Registration\n");

	ret = ecpri_dma_ecpri_ss_register(&ecpri_dma_ss_core_suite_ctx.ready_info,
		&is_dma_ready);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed on register");
		ret = -EFAULT;
		goto fail;
	}

	msleep(100);

	if (wait_for_completion_timeout(&ecpri_dma_ss_core_suite_ctx.ready_received,
		msecs_to_jiffies(ECPRI_DMA_SS_CORE_CLIENT_UT_READY_RCVD_TMOUT_MS)) == 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to completion timeout");
		return -EFAULT;
	}

	/* Check SS Core driver state matches the expectations */
	if (ecpri_dma_ss_core_ctx->ready_cb
		!= ecpri_dma_ss_core_suite_ctx.ready_info.notify_ready) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"mismatch of Ready CB address\n");
		ret = -EFAULT;
		goto fail;
	}

	if (ecpri_dma_ss_core_ctx->event_notify_cb
		!= ecpri_dma_ss_core_suite_ctx.ready_info.dma_event_notify) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"mismatch of DMA Event Notify CB address\n");
		ret = -EFAULT;
		goto fail;
	}

	if (ecpri_dma_ss_core_ctx->log_msg_cb
		!= ecpri_dma_ss_core_suite_ctx.ready_info.log_msg) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"mismatch of Log Msg CB comp cb address\n");
		ret = -EFAULT;
		goto fail;
	}

	/* Deregister and check */
	ecpri_dma_ecpri_ss_deregister();

	if (ecpri_dma_ss_core_ctx != NULL) {
		DMA_UT_TEST_FAIL_REPORT("Context wasn't destroyed\n");
		ret = -EFAULT;
		goto fail;
	}

fail:
	return ret;
}

/* Suite definition block */
DMA_UT_DEFINE_SUITE_START(ss_core_client, "SS Core Client suite",
	ecpri_dma_ss_core_client_test_suite_setup,
	ecpri_dma_ss_core_client_test_suite_teardown) {
	DMA_UT_ADD_TEST(
		register,
		"This test will verify the registration process"
		" of the SS Core Client.",
		ecpri_dma_ss_core_client_test_suite_registration, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
} DMA_UT_DEFINE_SUITE_END(ss_core_client);
