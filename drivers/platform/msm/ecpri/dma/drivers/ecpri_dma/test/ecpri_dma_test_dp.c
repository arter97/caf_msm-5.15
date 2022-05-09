/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "ecpri_dma_ut_framework.h"
#include "ecpri_dma_utils.h"
#include "ecpri_dma_dp.h"
#include "dmahal.h"

/**
  * DMA Driver Data Path DMA Unit-test suite
  * Suite will verify DMA data path via loopback SRC ENDP to DEST ENDP and
  * transfering data.
  *
  * DP testing requires DU-PCIE flavor
  *	------------						     -----------
  *	|          |  ==={SRC  ENDP 0  EE 0}==>  |         |
  *	|   TEST   |			  			     |   DMA   |
  *	|          |  <=={DEST ENDP 37 EE 0}===  |         |
  *	------------						     -----------
  */

/* For loopback configuration used first A55 SRC ENDP and first A55 DEST ENDP*/
#define ECPRI_DMA_DP_UT_SRC_ENDP_ID 0
#define ECPRI_DMA_DP_UT_DEST_ENDP_ID 37

/* Define for test endp IRQ timer moderation */
#define ECPRI_DMA_DP_TEST_ENDP_MODT 0

/* Define for test endp IRQ counter moderation */
#define ECPRI_DMA_DP_TEST_ENDP_MODC 1

/* Define for test endp buffer size */
#define ECPRI_DMA_DP_TEST_BUFF_SIZE 1500

/* Define for test Tx endp buffer size */
#define ECPRI_DMA_DP_TEST_TX_BUFF_SIZE 16

/* Define for test endp ring length */
#define ECPRI_DMA_DP_TEST_RING_LEN 10

/* Define for number of buffs in ring */
#define ECPRI_DMA_DP_TEST_NUM_OF_BUFFS_IN_RING (ECPRI_DMA_DP_TEST_RING_LEN - 1)

/* Define for test max buffers per packet */
#define ECPRI_DMA_DP_TEST_MAX_BUFFS 3

/* Define for test packet initial content */
#define ECPRI_DMA_DP_TEST_PACKET_CONTENT 0x12345678

#define ECPRI_DMA_DP_TEST_RX_BUDGET (5)
#define ECPRI_DMA_ENDP_DIR_MAX (ECPRI_DMA_ENDP_DIR_DEST + 1)

struct ecpri_dma_dp_test_suite_context {
	struct completion irq_received[ECPRI_DMA_ENDP_DIR_MAX];
	u32 num_irq_received[ECPRI_DMA_ENDP_DIR_MAX];
	u32 rx_received_irq_in_poll;
	u32 tx_comp_pkts_num;
	struct ecpri_dma_pkt *rx_pkts[ECPRI_DMA_DP_TEST_NUM_OF_BUFFS_IN_RING];
	u32 rx_pkt_idx;
};

struct ecpri_dma_dp_test_suite_context dp_test_suite_ctx;

static void ecpri_dma_test_dump_packet(char* buf, int len)
{
	int payload_len = len;
	int num_dumps;
	int i;
	int index = 0;

	DMA_UT_LOG("dumping buff of length: %d\n", len);

	if (payload_len > 0) {
		num_dumps = payload_len / 8;

		if (num_dumps > 8) {
			num_dumps = 8;
		}

		for (i = 0; i < num_dumps; ++i) {
			DMA_UT_LOG("buff: %d, %x:%x:%x:%x:%x:%x:%x:%x\n", i,
				buf[index + 0], buf[index + 1],
				buf[index + 2], buf[index + 3],
				buf[index + 4], buf[index + 5],
				buf[index + 6], buf[index + 7]);
			index += 8;
			if (index + 8 > len) {
				break;
			}
		}
	}
}

static int ecpri_dma_dp_test_setup_dma_endps(enum ecpri_dma_endp_dir dir,
	bool enable_loopback)
{
	int endp_id;
	ecpri_hwio_def_ecpri_endp_cfg_destn_u endp_cfg_dest = { 0 };
	ecpri_hwio_def_ecpri_endp_cfg_xbarn_u endp_cfg_xbar = { 0 };
	ecpri_hwio_def_ecpri_endp_gsi_cfg_n_u endp_gsi_cfg = { 0 };
	ecpri_hwio_def_ecpri_endp_cfg_aggr_n_u cfg_aggr = { 0 };
	ecpri_hwio_def_ecpri_endp_nfapi_reassembly_cfg_n_u reassembly_cfg = { 0 };

	if (!ecpri_dma_ctx->endp_map)
		return -EINVAL;

	if (dir == ECPRI_DMA_ENDP_DIR_SRC)
		endp_id = ECPRI_DMA_DP_UT_SRC_ENDP_ID;
	else
		endp_id = ECPRI_DMA_DP_UT_DEST_ENDP_ID;

	if (!ecpri_dma_ctx->endp_map[endp_id].valid ||
		ecpri_dma_ctx->endp_map[endp_id].is_exception)
		return -EINVAL;

	/* Configure test SRC ENDP to loopback into test DEST ENDP*/
	memset(&endp_cfg_dest, 0, sizeof(endp_cfg_dest));
	memset(&endp_cfg_xbar, 0, sizeof(endp_cfg_xbar));
	memset(&endp_gsi_cfg, 0, sizeof(endp_gsi_cfg));
	memset(&cfg_aggr, 0, sizeof(cfg_aggr));
	memset(&reassembly_cfg, 0, sizeof(reassembly_cfg));

	/* First disable ENDP*/
	ecpri_dma_hal_write_reg_n(
		ECPRI_ENDP_GSI_CFG_n, endp_id, 0);

	if (enable_loopback)
	{
		switch (ecpri_dma_ctx->endp_map[endp_id].dir) {
		case ECPRI_DMA_ENDP_DIR_SRC:
			/* Set SRC to M2M mode*/
			endp_cfg_dest.def.use_dest_cfg = 1;
			endp_cfg_dest.def.dest_mem_channel =
				ECPRI_DMA_DP_UT_DEST_ENDP_ID;
			ecpri_dma_hal_write_reg_n(
				ECPRI_ENDP_CFG_DEST_n, endp_id,
				endp_cfg_dest.value);
			ecpri_dma_hal_write_reg_n(
				ECPRI_ENDP_CFG_XBAR_n, endp_id,
				endp_cfg_xbar.value);
			break;
		case ECPRI_DMA_ENDP_DIR_DEST:
			ecpri_dma_hal_write_reg_n(
				ECPRI_ENDP_CFG_AGGR_n,
				endp_id, cfg_aggr.value);
			ecpri_dma_hal_write_reg_n(
				ECPRI_ENDP_NFAPI_REASSEMBLY_CFG_n,
				endp_id, reassembly_cfg.value);
			break;
		default:
			DMAERR("ENDP isn't SRC nor DEST\n");
			return -EINVAL;
			break;
		}
	}
	else {
		switch (ecpri_dma_ctx->endp_map[endp_id].dir) {
		case ECPRI_DMA_ENDP_DIR_SRC:
			switch (ecpri_dma_ctx->endp_map[endp_id].stream_mode) {
			case ECPRI_DMA_ENDP_STREAM_MODE_M2M:
				endp_cfg_dest.def.use_dest_cfg = 1;
				endp_cfg_dest.def.dest_mem_channel =
					ecpri_dma_ctx->endp_map[endp_id].dest;
				ecpri_dma_hal_write_reg_n(
					ECPRI_ENDP_CFG_DEST_n, endp_id,
					endp_cfg_dest.value);
				break;
			case ECPRI_DMA_ENDP_STREAM_MODE_M2S:
				endp_cfg_dest.def.use_dest_cfg = 0;
				endp_cfg_xbar.def.dest_stream =
					ecpri_dma_ctx->endp_map[endp_id].dest;
				endp_cfg_xbar.def.xbar_tid =
					ecpri_dma_ctx->endp_map[endp_id].tid.value;
				//TODO: Below are required for nFAPI
				//endp_cfg_xbar.xbar_user = Get from Core driver, need API
				endp_cfg_xbar.def.l2_segmentation_en =
					ecpri_dma_ctx->endp_map[endp_id].is_nfapi ? 1 : 0;
				ecpri_dma_hal_write_reg_n(
					ECPRI_ENDP_CFG_DEST_n, endp_id,
					endp_cfg_dest.value);
				ecpri_dma_hal_write_reg_n(
					ECPRI_ENDP_CFG_XBAR_n, endp_id,
					endp_cfg_xbar.value);
				break;
			default:
				DMAERR("SRC ENDP %d isn't M2M or S2M, address = 0x%px\n",
					endp_id, &ecpri_dma_ctx->endp_map[endp_id]);
				return -EINVAL;
				break;
			}
			break;
		case ECPRI_DMA_ENDP_DIR_DEST:
			switch (ecpri_dma_ctx->endp_map[endp_id].stream_mode) {
			case ECPRI_DMA_ENDP_STREAM_MODE_M2M:
				break;
			case ECPRI_DMA_ENDP_STREAM_MODE_S2M:
				if (ecpri_dma_ctx->endp_map[endp_id].is_nfapi) {
					memset(&cfg_aggr, 0,
						sizeof(cfg_aggr));
					memset(&reassembly_cfg, 0,
						sizeof(reassembly_cfg));
					cfg_aggr.def.aggr_type = 1;
					reassembly_cfg.def.vm_id =
						ecpri_dma_ctx->endp_map[endp_id]
						.nfapi_dest_vm_id;
					ecpri_dma_hal_write_reg_n(
						ECPRI_ENDP_CFG_AGGR_n,
						endp_id, cfg_aggr.value);
					ecpri_dma_hal_write_reg_n(
						ECPRI_ENDP_NFAPI_REASSEMBLY_CFG_n,
						endp_id, reassembly_cfg.value);
				}
				break;
			default:
				DMAERR("DEST ENDP %d isn't M2M or S2M, address = 0x%px\n",
					endp_id, &ecpri_dma_ctx->endp_map[endp_id]);
				return -EINVAL;
				break;
			}
			break;
		default:
			DMAERR("ENDP isn't SRC nor DEST\n");
			return -EINVAL;
			break;
			}
	}

	/* Re-enable ENDP*/
	ecpri_dma_hal_write_reg_n(
		ECPRI_ENDP_GSI_CFG_n, endp_id, 1);

	return 0;
}

int ecpri_dma_dp_test_rx_replenish(struct ecpri_dma_endp_context *endp,
	u32 num_to_replenish)
{
	int ret = 0;
	int rem_to_repelnish = num_to_replenish;
	int first_batch = 0;
	u32 first_idx = dp_test_suite_ctx.rx_pkt_idx;
	u32 *i = &dp_test_suite_ctx.rx_pkt_idx;

	if (!endp || !endp->valid || endp->gsi_ep_cfg->is_exception) {
		DMA_UT_LOG("Test DEST ENDP isn't valid or is defined as exception");
		return -EINVAL;
	}

	while (rem_to_repelnish) {
		dp_test_suite_ctx.rx_pkts[*i] =
			kzalloc(sizeof(*dp_test_suite_ctx.rx_pkts[*i]), GFP_KERNEL);
		if (!dp_test_suite_ctx.rx_pkts[*i]) {
			DMA_UT_LOG("failed to alloc packet\n");
			ret = -ENOMEM;
			goto fail_alloc;
		}

		dp_test_suite_ctx.rx_pkts[*i]->buffs =
			kzalloc(sizeof(*dp_test_suite_ctx.rx_pkts[*i]->buffs), GFP_KERNEL);
		if (!dp_test_suite_ctx.rx_pkts[*i]->buffs) {
			DMA_UT_LOG("failed to alloc dma buff wrapper\n");
			kfree(dp_test_suite_ctx.rx_pkts[*i]);
			ret = -ENOMEM;
			goto fail_alloc;
		}

		dp_test_suite_ctx.rx_pkts[*i]->buffs[0] =
			kzalloc(sizeof(*dp_test_suite_ctx.rx_pkts[*i]->buffs[0]), GFP_KERNEL);
		if (!dp_test_suite_ctx.rx_pkts[*i]->buffs[0]) {
			DMA_UT_LOG("failed to alloc dma buff wrapper\n");
			kfree(dp_test_suite_ctx.rx_pkts[*i]->buffs);
			kfree(dp_test_suite_ctx.rx_pkts[*i]);
			ret = -ENOMEM;
			goto fail_alloc;
		}
		dp_test_suite_ctx.rx_pkts[*i]->num_of_buffers = 1;

		dp_test_suite_ctx.rx_pkts[*i]->buffs[0]->virt_base =
			kzalloc(ECPRI_DMA_DP_TEST_BUFF_SIZE, GFP_KERNEL);
		if (!dp_test_suite_ctx.rx_pkts[*i]->buffs[0]->virt_base) {
			DMA_UT_LOG("failed to alloc buffer\n");
			kfree(dp_test_suite_ctx.rx_pkts[*i]->buffs[0]);
			kfree(dp_test_suite_ctx.rx_pkts[*i]->buffs);
			kfree(dp_test_suite_ctx.rx_pkts[*i]);
			ret = -ENOMEM;
			goto fail_alloc;
		}

		dp_test_suite_ctx.rx_pkts[*i]->buffs[0]->size =
			ECPRI_DMA_DP_TEST_BUFF_SIZE;

		*i = (*i + 1) % (ECPRI_DMA_DP_TEST_NUM_OF_BUFFS_IN_RING);
		rem_to_repelnish--;
	}

	if (first_idx + num_to_replenish > ECPRI_DMA_DP_TEST_NUM_OF_BUFFS_IN_RING)
	{
		/* Handle array wrap around */
		first_batch = ECPRI_DMA_DP_TEST_NUM_OF_BUFFS_IN_RING - first_idx;
		rem_to_repelnish = num_to_replenish - first_batch;
		ret = ecpri_dma_dp_transmit(endp, &dp_test_suite_ctx.rx_pkts[first_idx],
			first_batch, true);
		if (ret) {
			DMA_UT_LOG("failed to replenish Rx endp\n");
			goto fail_alloc;
		}

		ret = ecpri_dma_dp_transmit(endp, &dp_test_suite_ctx.rx_pkts[0],
			rem_to_repelnish, true);
		if (ret) {
			DMA_UT_LOG("failed to replenish Rx endp\n");
			goto fail_alloc;
		}
	}
	else
	{
		/* No need to handle wrap around*/
		ret = ecpri_dma_dp_transmit(endp, &dp_test_suite_ctx.rx_pkts[first_idx],
			num_to_replenish, true);
		if (ret) {
			DMA_UT_LOG("failed to replenish Rx endp\n");
			goto fail_alloc;
		}
	}

	return 0;

fail_alloc:
	/* An allocation failed, free memory backwards */
	while (rem_to_repelnish < num_to_replenish) {
		*i = (*i - 1) % ECPRI_DMA_DP_TEST_RING_LEN;
		kfree(dp_test_suite_ctx.rx_pkts[*i]->buffs[0]->virt_base);
		kfree(dp_test_suite_ctx.rx_pkts[*i]->buffs[0]);
		kfree(dp_test_suite_ctx.rx_pkts[*i]->buffs);
		kfree(dp_test_suite_ctx.rx_pkts[*i]);
		rem_to_repelnish++;
	}

	return ret;
}

static void ecpri_dma_dp_test_rx_client_notify_comp(
	struct ecpri_dma_endp_context *endp,
	struct ecpri_dma_pkt_completion_wrapper **comp_pkt,
	u32 num_of_completed)
{
	int ret = 0;
	enum ecpri_dma_notify_mode mode;
	if (!endp || !endp->valid) {
		DMA_UT_LOG("Rx Test ENDP is not valid\n");
	}

	ret = ecpri_dma_get_endp_mode(endp, &mode);
	if (ret) {
		DMA_UT_LOG("Failed to get current Rx test ENDP mode\n");
	}

	dp_test_suite_ctx.num_irq_received[ECPRI_DMA_ENDP_DIR_DEST]++;
	complete(&dp_test_suite_ctx.irq_received[ECPRI_DMA_ENDP_DIR_DEST]);
}

static void ecpri_dma_dp_test_tx_client_notify_comp(
	struct ecpri_dma_endp_context *endp,
	struct ecpri_dma_pkt_completion_wrapper **comp_pkt,
	u32 num_of_completed)
{
	if (!endp || !endp->valid) {
		DMA_UT_LOG("Tx Test ENDP is not valid\n");
	}

	dp_test_suite_ctx.tx_comp_pkts_num += num_of_completed;
	dp_test_suite_ctx.num_irq_received[ECPRI_DMA_ENDP_DIR_SRC]++;
	complete(&dp_test_suite_ctx.irq_received[ECPRI_DMA_ENDP_DIR_SRC]);
}

static int ecpri_dma_create_test_endp(enum ecpri_dma_endp_dir dir)
{
	struct ecpri_dma_endp_context *ep;
	struct ecpri_dma_moderation_config mod_cfg = { 0, 0 };
	int ret = 0;
	int endp_id;
	client_notify_comp notify;

	if (dir == ECPRI_DMA_ENDP_DIR_SRC) {
		endp_id = ECPRI_DMA_DP_UT_SRC_ENDP_ID;
		notify = &ecpri_dma_dp_test_tx_client_notify_comp;
	} else {
		endp_id = ECPRI_DMA_DP_UT_DEST_ENDP_ID;
		notify = &ecpri_dma_dp_test_rx_client_notify_comp;
	}

	mod_cfg.moderation_counter_threshold =
		ECPRI_DMA_DP_TEST_ENDP_MODC;
	mod_cfg.moderation_timer_threshold =
		ECPRI_DMA_DP_TEST_ENDP_MODT;

	ep = &ecpri_dma_ctx->endp_ctx[endp_id];

	ret = ecpri_dma_alloc_endp(endp_id, ECPRI_DMA_DP_TEST_RING_LEN,
		&mod_cfg, false, notify);
	if (ret) {
		DMA_UT_LOG("Failed to allocte test ENDP %d\n", endp_id);
		goto fail_gen;
	}

	ret = ecpri_dma_start_endp(ep);
	if (ret) {
		DMA_UT_LOG("Failed to start test ENDP %d\n", endp_id);
		goto fail_start;
	}

	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		/* Fill ring with credtis */
		if (dir == ECPRI_DMA_ENDP_DIR_DEST) {
			ret = ecpri_dma_dp_test_rx_replenish(ep, ep->ring_length - 1);
			if (ret) {
				DMA_UT_LOG("Failed to replenish test Rx endp\n");
				goto fail_replenish;
			}
		}
	}

	return 0;

fail_replenish:
	ecpri_dma_stop_endp(ep);
fail_start:
	ecpri_dma_dealloc_endp(ep);
fail_gen:
	return ret;
}

static void ecpri_dma_destroy_test_endp(enum ecpri_dma_endp_dir dir)
{
	int ret = 0;
	int endp_id;
	struct ecpri_dma_endp_context *endp_cfg = NULL;

	if (dir == ECPRI_DMA_ENDP_DIR_SRC)
		endp_id = ECPRI_DMA_DP_UT_SRC_ENDP_ID;
	else
		endp_id = ECPRI_DMA_DP_UT_DEST_ENDP_ID;

	endp_cfg = &ecpri_dma_ctx->endp_ctx[endp_id];

	ret = ecpri_dma_stop_endp(endp_cfg);
	if (ret != GSI_STATUS_SUCCESS) {
		DMA_UT_LOG("Stop ENDP %d failed with code %d\n", endp_id, ret);
		return;
	}

	ret = ecpri_dma_reset_endp(endp_cfg);
	if (ret != GSI_STATUS_SUCCESS) {
		DMA_UT_LOG("Reset ENDP %d failed with code %d\n", endp_id, ret);
		return;
	}

	ret = ecpri_dma_dealloc_endp(endp_cfg);
	if (ret != GSI_STATUS_SUCCESS) {
		DMA_UT_LOG("Dealloc ENDP %d failed with code %d\n", endp_id, ret);
	}
}

static int ecpri_dma_test_dp_suite_setup(void **ppriv)
{
	int ret = 0;
	DMA_UT_DBG("Start Setup\n");
	memset(&dp_test_suite_ctx, 0, sizeof(dp_test_suite_ctx));

	init_completion(
		&dp_test_suite_ctx.irq_received[ECPRI_DMA_ENDP_DIR_SRC]);
	init_completion(
		&dp_test_suite_ctx.irq_received[ECPRI_DMA_ENDP_DIR_DEST]);

	ret = ecpri_dma_dp_test_setup_dma_endps(ECPRI_DMA_ENDP_DIR_SRC, true);
	if (ret)
		return ret;

	ret = ecpri_dma_dp_test_setup_dma_endps(ECPRI_DMA_ENDP_DIR_DEST, true);
	if (ret)
		return ret;

	ret = ecpri_dma_create_test_endp(ECPRI_DMA_ENDP_DIR_SRC);
	if (ret)
		return ret;

	ret = ecpri_dma_create_test_endp(ECPRI_DMA_ENDP_DIR_DEST);
	if (ret) {
		ecpri_dma_destroy_test_endp(ECPRI_DMA_ENDP_DIR_SRC);
		return ret;
	}

	return 0;
}

static int ecpri_dma_test_dp_suite_teardown(void *priv)
{
	int ret = 0;
	int i = 0;
	DMA_UT_DBG("Start Teardown\n");

	ecpri_dma_destroy_test_endp(ECPRI_DMA_ENDP_DIR_SRC);
	ecpri_dma_destroy_test_endp(ECPRI_DMA_ENDP_DIR_DEST);

	/* Once endps are stopped, remove loopback config */
	ret = ecpri_dma_dp_test_setup_dma_endps(ECPRI_DMA_ENDP_DIR_SRC, false);
	if (ret)
		return ret;

	ret = ecpri_dma_dp_test_setup_dma_endps(ECPRI_DMA_ENDP_DIR_DEST, false);
	if (ret)
		return ret;

	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		for (i = 0; i < ECPRI_DMA_DP_TEST_NUM_OF_BUFFS_IN_RING; i++) {
			kfree(dp_test_suite_ctx.rx_pkts[i]->buffs[0]->virt_base);
			kfree(dp_test_suite_ctx.rx_pkts[i]->buffs[0]);
			kfree(dp_test_suite_ctx.rx_pkts[i]->buffs);
			kfree(dp_test_suite_ctx.rx_pkts[i]);
		}
	}
	dp_test_suite_ctx.rx_pkt_idx = 0;

	return 0;
}

static int ecpri_dma_test_dp_suite_prepare_test_data(
	int num_of_pkts, bool single_buffer, struct ecpri_dma_pkt ***tx_pkts_ptr,
	struct ecpri_dma_pkt_completion_wrapper ***rx_pkts_ptr)
{
	int i = 0, j = 0;
	int res = 0;
	u32 num_of_buffs;
	u32 packet_content = ECPRI_DMA_DP_TEST_PACKET_CONTENT;
	struct ecpri_dma_pkt **tx_pkts;
	struct ecpri_dma_pkt_completion_wrapper **rx_pkts;

	/* Allocate memory for the credits */
	rx_pkts = kzalloc(sizeof(*rx_pkts) * num_of_pkts,
		GFP_KERNEL);
	if (!rx_pkts) {
		DMA_UT_LOG("failed to alloc Rx packets array \n");
		kfree(rx_pkts);
		return -ENOMEM;
	}
	memset(rx_pkts, 0, sizeof(*rx_pkts) * num_of_pkts);

	for (i = 0; i < num_of_pkts; i++) {
		rx_pkts[i] =
			kzalloc(sizeof(*rx_pkts[i]), GFP_KERNEL);
		if (!rx_pkts[i]) {
			DMA_UT_LOG("failed to alloc rx packet struct \n");
			res = -ENOMEM;
			goto fail_alloc;
		}
	}

	/* Allocate and fill the Tx data packets and buffers */
	tx_pkts = kzalloc(sizeof(*tx_pkts) * num_of_pkts, GFP_KERNEL);
	if (!tx_pkts) {
		DMA_UT_LOG("failed to alloc Tx packets array \n");
		return -ENOMEM;
	}
	memset(tx_pkts, 0, sizeof(*tx_pkts) * num_of_pkts);

	num_of_buffs = single_buffer ? 1 : ECPRI_DMA_DP_TEST_MAX_BUFFS;

	for (i = 0; i < num_of_pkts; i++) {
		tx_pkts[i] =
			kzalloc(sizeof(*tx_pkts[i]), GFP_KERNEL);
		if (!tx_pkts[i]) {
			DMA_UT_LOG("failed to alloc tx packet struct \n");
			res = -ENOMEM;
			goto fail_alloc;
		}

		tx_pkts[i]->buffs =
			kzalloc(sizeof(tx_pkts[i]->buffs) * num_of_buffs, GFP_KERNEL);
		if (!tx_pkts[i]->buffs) {
			DMA_UT_LOG("failed to alloc buffers array \n");
			kfree(tx_pkts[i]);
			res = -ENOMEM;
			goto fail_alloc;
		}

		for (j = 0; j < num_of_buffs; j++) {
			tx_pkts[i]->buffs[j] = kzalloc(
				sizeof(tx_pkts[i]->buffs[j]), GFP_KERNEL);
			if (!tx_pkts[i]->buffs[j]) {
				DMA_UT_LOG("failed to alloc dma buff wrapper\n");
				kfree(tx_pkts[i]->buffs);
				kfree(tx_pkts[i]);
				res = -ENOMEM;
				goto fail_alloc;
			}

			tx_pkts[i]->buffs[j]->virt_base =
				kzalloc(ECPRI_DMA_DP_TEST_TX_BUFF_SIZE, GFP_KERNEL);
			if (!tx_pkts[i]->buffs[j]->virt_base) {
				DMA_UT_LOG("failed to alloc buffer\n");
				kfree(tx_pkts[i]->buffs[j]);

				for (j--; j >= 0; j--) {
					kfree(tx_pkts[i]->buffs[j]->virt_base);
					kfree(tx_pkts[i]->buffs[j]);
				}
				kfree(tx_pkts[i]->buffs);
				kfree(tx_pkts[i]);
				res = -ENOMEM;
				goto fail_alloc;
			}

			tx_pkts[i]->buffs[j]->size = ECPRI_DMA_DP_TEST_TX_BUFF_SIZE;

			*(u32 *)tx_pkts[i]->buffs[j]->virt_base =
				packet_content++;
		}
		tx_pkts[i]->num_of_buffers = num_of_buffs;
	}

	/* Put in the privoded pointers the allocated data */
	*tx_pkts_ptr = tx_pkts;
	*rx_pkts_ptr = rx_pkts;

	return 0;

fail_alloc:
	for (i--; i >= 0; i--) {
		for (j = 0; j < num_of_buffs; j++) {
			kfree(tx_pkts[i]->buffs[j]->virt_base);
			kfree(tx_pkts[i]->buffs[j]);
		}
		kfree(tx_pkts[i]->buffs);
		kfree(tx_pkts[i]);
		kfree(rx_pkts[i]);
	}
	kfree(tx_pkts);
	kfree(rx_pkts);
	return res;
}

static void ecpri_dma_test_dp_suite_destroy_test_data(
	int num_of_pkts, bool single_buffer, struct ecpri_dma_pkt **tx_pkts,
	struct ecpri_dma_pkt_completion_wrapper **rx_pkts)
{
	int i = 0, j = 0;
	u32 num_of_buffs;

	num_of_buffs = single_buffer ? 1 : ECPRI_DMA_DP_TEST_MAX_BUFFS;
	for (i=0; i < num_of_pkts; i++) {
		for (j = 0; j < num_of_buffs; j++) {
			kfree(tx_pkts[i]->buffs[j]->virt_base);
			kfree(tx_pkts[i]->buffs[j]);
		}
		kfree(tx_pkts[i]->buffs);
		kfree(tx_pkts[i]);
		kfree(rx_pkts[i]);
	}
	kfree(tx_pkts);
	kfree(rx_pkts);

	dp_test_suite_ctx.num_irq_received[ECPRI_DMA_ENDP_DIR_SRC] = 0;
	dp_test_suite_ctx.num_irq_received[ECPRI_DMA_ENDP_DIR_SRC] = 0;
	dp_test_suite_ctx.rx_received_irq_in_poll = 0;
	dp_test_suite_ctx.tx_comp_pkts_num = 0;
}

static int ecpri_dma_test_dp_suite_wait_for_tx_comp(u32 num_of_pkts_sent)
{
	int res = 0;
	while (dp_test_suite_ctx.tx_comp_pkts_num != num_of_pkts_sent) {
		res = wait_for_completion_timeout(
			&dp_test_suite_ctx.irq_received[ECPRI_DMA_ENDP_DIR_SRC],
			msecs_to_jiffies(10000));
		if (res == 0 && dp_test_suite_ctx.tx_comp_pkts_num != num_of_pkts_sent)
		{
			DMA_UT_LOG("Test failed due to Tx timeout");
			return -EFAULT;
		}
	}

	return 0;
}

static int ecpri_dma_test_dp_suite_verify_rx(
	bool expecting_irq, u32 num_of_pkts_sent, struct ecpri_dma_pkt** tx_pkts,
	struct ecpri_dma_pkt_completion_wrapper** rx_pkts,
	int* total_rx_buffs_to_replenish)
{
	int res = 0;
	int actual_num = 0;
	int sent_pkt_idx = 0, sent_pkt_buff_idx = 0;
	int i = 0;
	int curr_pkt_recv_size = 0;
	unsigned long tx_buff_size = 0;
	void* tx_buff_to_compare = NULL;
	void* rx_buff_to_compare = NULL;
	struct ecpri_dma_endp_context* rx_endp = NULL;

	rx_endp = &ecpri_dma_ctx->endp_ctx[ECPRI_DMA_DP_UT_DEST_ENDP_ID];
	*total_rx_buffs_to_replenish = 0;

	if (expecting_irq)
	{
		res = wait_for_completion_timeout(
			&dp_test_suite_ctx.irq_received[ECPRI_DMA_ENDP_DIR_DEST],
			msecs_to_jiffies(10000));
		if (res == 0) {
			DMA_UT_LOG("Test failed due to Rx timeout");
			return -EFAULT;
		}
	}

	while (sent_pkt_idx != num_of_pkts_sent) {
		actual_num = 0;
		res = ecpri_dma_dp_rx_poll(rx_endp, ECPRI_DMA_DP_TEST_RX_BUDGET,
			rx_pkts, &actual_num);
		if (res || !actual_num) {
			DMA_UT_LOG("Test failed to perform rx poll, res = %d, actual = %d",
				res, actual_num);
			return -EFAULT;
		}

		tx_buff_to_compare =
			tx_pkts[sent_pkt_idx]->buffs[sent_pkt_buff_idx]->virt_base;
		tx_buff_size = tx_pkts[sent_pkt_idx]->buffs[sent_pkt_buff_idx]->size;
		for (i = 0; i < actual_num; i++) {
			curr_pkt_recv_size = rx_pkts[i]->pkt->buffs[0]->size;

			if (curr_pkt_recv_size < tx_buff_size &&
				curr_pkt_recv_size == ECPRI_DMA_DP_TEST_BUFF_SIZE)
			{
				/* Over flow event */
				if (memcmp(
					tx_buff_to_compare,
					rx_pkts[i]->pkt->buffs[0]->virt_base,
					curr_pkt_recv_size)) {
					DMA_UT_LOG(
						"Test failed due to buffers don't match i= %d\n",i);
					DMA_UT_LOG("tx buff:\n");
						ecpri_dma_test_dump_packet(tx_buff_to_compare,
							curr_pkt_recv_size);
					DMA_UT_LOG("rx buff:\n");
						ecpri_dma_test_dump_packet(
							rx_pkts[i]->pkt->buffs[0]->virt_base,
							curr_pkt_recv_size);
					return -EFAULT;
				}
				tx_buff_to_compare =
					(void*)((unsigned long)tx_buff_to_compare +
						rx_pkts[i]->pkt->buffs[0]->size);
				tx_buff_size -= ECPRI_DMA_DP_TEST_BUFF_SIZE;
			}
			else if (curr_pkt_recv_size == tx_buff_size)
			{
				/* Buffers context matches and Tx EOT */
				if (memcmp(
					tx_buff_to_compare,
					rx_pkts[i]->pkt->buffs[0]->virt_base,
					curr_pkt_recv_size)) {
					DMA_UT_LOG(
						"Test failed due to buffers don't match i= %d\n",i);
					DMA_UT_LOG("tx buff:\n");
						ecpri_dma_test_dump_packet(tx_buff_to_compare,
							curr_pkt_recv_size);
					DMA_UT_LOG("rx buff:\n");
						ecpri_dma_test_dump_packet(
							rx_pkts[i]->pkt->buffs[0]->virt_base,
							curr_pkt_recv_size);
					return -EFAULT;
				}
				sent_pkt_buff_idx++;
				if (sent_pkt_buff_idx == tx_pkts[sent_pkt_idx]->num_of_buffers)
				{
					sent_pkt_buff_idx = 0;
					sent_pkt_idx++;
					if (num_of_pkts_sent == sent_pkt_idx)
					{
						kfree(rx_pkts[i]->pkt->buffs[0]->virt_base);
						kfree(rx_pkts[i]->pkt->buffs[0]);
						kfree(rx_pkts[i]->pkt->buffs);
						kfree(rx_pkts[i]->pkt);
						(*total_rx_buffs_to_replenish)++;
						break;
					}
				}
				tx_buff_to_compare =
					tx_pkts[sent_pkt_idx]->buffs[sent_pkt_buff_idx]->virt_base;
				tx_buff_size =
					tx_pkts[sent_pkt_idx]->buffs[sent_pkt_buff_idx]->size;
			}
			else if (curr_pkt_recv_size > tx_buff_size)
			{
				/*	Tx chained buffers smaller than credit so HW put them all in
					one credit buffer */
				rx_buff_to_compare = rx_pkts[i]->pkt->buffs[0]->virt_base;
				while (curr_pkt_recv_size &&
					sent_pkt_buff_idx < tx_pkts[sent_pkt_idx]->num_of_buffers)
				{
					if (curr_pkt_recv_size < tx_buff_size)
					{
						/* Overflow to next Rx buff*/
						/* Over flow event */
						if (memcmp(
							tx_buff_to_compare,
							rx_buff_to_compare,
							curr_pkt_recv_size)) {
							DMA_UT_LOG(
								"Test failed due to buffers don't match i= %d\n",i);
							DMA_UT_LOG("tx buff:\n");
								ecpri_dma_test_dump_packet(tx_buff_to_compare,
									curr_pkt_recv_size);
							DMA_UT_LOG("rx buff:\n");
								ecpri_dma_test_dump_packet(
									rx_buff_to_compare,
									curr_pkt_recv_size);
							return -EFAULT;
						}
						tx_buff_to_compare =
							(void*)((unsigned long)tx_buff_to_compare +
								curr_pkt_recv_size);
						tx_buff_size -= curr_pkt_recv_size;

					}
					else
					{
						/* No overflow */
						if (memcmp(
							tx_buff_to_compare,
							rx_buff_to_compare,
							tx_buff_size)) {
							DMA_UT_LOG(
								"Test failed due to buffers don't match i= %d\n",i);
							DMA_UT_LOG("tx buff:\n");
								ecpri_dma_test_dump_packet(tx_buff_to_compare,
									curr_pkt_recv_size);
							DMA_UT_LOG("rx buff:\n");
								ecpri_dma_test_dump_packet(
									rx_buff_to_compare,
									curr_pkt_recv_size);
							return -EFAULT;
						}

						rx_buff_to_compare =
							(void*)((unsigned long)rx_buff_to_compare +
								tx_buff_size);
						curr_pkt_recv_size -= tx_buff_size;
						sent_pkt_buff_idx++;
						if (sent_pkt_buff_idx ==
							tx_pkts[sent_pkt_idx]->num_of_buffers)
						{
							/* Got all Tx packet buffers in this Rx buffer */
							sent_pkt_buff_idx = 0;
							sent_pkt_idx++;
							if (num_of_pkts_sent == sent_pkt_idx)
								break;
						}
						tx_buff_to_compare =
							tx_pkts[sent_pkt_idx]->buffs[sent_pkt_buff_idx]->
							virt_base;
						tx_buff_size =
							tx_pkts[sent_pkt_idx]->buffs[sent_pkt_buff_idx]->
							size;
					}
				}
			}

			kfree(rx_pkts[i]->pkt->buffs[0]->virt_base);
			kfree(rx_pkts[i]->pkt->buffs[0]);
			kfree(rx_pkts[i]->pkt->buffs);
			kfree(rx_pkts[i]->pkt);
			(*total_rx_buffs_to_replenish)++;
		}
	}

	return 0;
}

static int ecpri_dma_test_dp_suite_verify_read_write_ptr(
	struct ecpri_dma_endp_context* rx_endp, struct gsi_chan_info* ch_info)
{
	int res = 0;

	res = gsi_query_channel_info(rx_endp->gsi_chan_hdl, ch_info);
	if (res) {
		DMA_UT_LOG("Failed to query Rx channel info\n");
		return -EFAULT;
	}

	if (ch_info->rp != ch_info->wp) {
		DMA_UT_LOG("Channel: 0x%x - rp:  0x%x and "
			"wp: 0x%x mismatch \n", rx_endp->gsi_chan_hdl,
			(unsigned long)ch_info->rp, (unsigned long)ch_info->wp);
		return -EFAULT;
	}

	return res;
}

static int ecpri_dma_test_dp_suite_calculate_credits(int idx,
	struct ecpri_dma_pkt** tx_pkts, int num_of_pkts, int* credits)
{
	int i;
	int res = 0;
	int curr_pkt;
	int total_buff_size = 0;
	int total_credits = 0;

	DMA_UT_LOG("Curr idx %d\n", idx);
	for (curr_pkt = idx; curr_pkt < idx + num_of_pkts; curr_pkt++) {
		DMA_UT_LOG("curr_pkt %d\n", curr_pkt);
		total_buff_size = 0;
		/* For each packet calculate total buffers size */
		for (i = 0; i < tx_pkts[curr_pkt]->num_of_buffers; i++) {
			total_buff_size += tx_pkts[curr_pkt]->buffs[i]->size;
			DMA_UT_LOG("Total buff size %d\n", total_buff_size);
		}

		if(ECPRI_DMA_DP_TEST_BUFF_SIZE < total_buff_size) {
			total_credits += (total_buff_size / ECPRI_DMA_DP_TEST_BUFF_SIZE);
			/* Ceiling */
			if (total_buff_size % ECPRI_DMA_DP_TEST_BUFF_SIZE != 0) {
				total_credits++;
			}
		} else {
			total_credits++;
		}
	}

	*credits = total_credits;
	DMA_UT_LOG("Total credits %d\n", total_credits);
	return res;
}

static int ecpri_dma_test_dp_suite_single_pkt_single_buffer(void *priv)
{
	int res = 0;
	int num_of_pkts = 1;
	bool single_buffer = true;
	int buffs_to_replenish = 0;
	struct gsi_chan_info ch_info;
	struct ecpri_dma_pkt **tx_pkts = NULL;
	struct ecpri_dma_pkt_completion_wrapper **rx_pkts = NULL;
	struct ecpri_dma_endp_context *tx_endp = NULL, *rx_endp = NULL;

	DMA_UT_LOG("Start single packet single buffer test\n");

	tx_endp = &ecpri_dma_ctx->endp_ctx[ECPRI_DMA_DP_UT_SRC_ENDP_ID];
	rx_endp = &ecpri_dma_ctx->endp_ctx[ECPRI_DMA_DP_UT_DEST_ENDP_ID];

	/* Provide mocked pointers to allocated space for data and credit packets */
	res = ecpri_dma_test_dp_suite_prepare_test_data(num_of_pkts, single_buffer,
		&tx_pkts, &rx_pkts);
	if (res) {
		DMA_UT_LOG("failed to prepare test data\n");
		return res;
	}

	/* Prepare exact amount of credits equal to amount of buffers sent */
	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		res = ecpri_dma_test_dp_suite_calculate_credits(0, tx_pkts, num_of_pkts,
			&buffs_to_replenish);
		if (res) {
			DMA_UT_LOG("Failed to calculate credits\n");
			return res;
		}

		res = ecpri_dma_dp_test_rx_replenish(rx_endp, buffs_to_replenish);
		if (res) {
			DMA_UT_LOG("Failed to replenish test Rx endp\n");
			return res;
		}
	}

	/* Transmit data packets */
	res = ecpri_dma_dp_transmit(tx_endp, tx_pkts, num_of_pkts, true);
	if (res != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed on transmit");
		return -EFAULT;
	}

	/* Wait for transmit completion of ALL sent packets */
	res = ecpri_dma_test_dp_suite_wait_for_tx_comp(num_of_pkts);
	if (res != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx timeout");
		return -EFAULT;
	}

	/* Poll completed packets and compare to the sent one */
	res = ecpri_dma_test_dp_suite_verify_rx(true, num_of_pkts, tx_pkts, rx_pkts,
		&buffs_to_replenish);
	if (res != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Rx fail");
		return -EFAULT;
	}

	/* Replenish all buffers */
	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		res = ecpri_dma_dp_test_rx_replenish(rx_endp, buffs_to_replenish);
		if (res) {
			DMA_UT_TEST_FAIL_REPORT("Test failed to perform rx replenish");
			return -EFAULT;
		}
	}

	ecpri_dma_test_dp_suite_destroy_test_data(num_of_pkts, single_buffer,
		tx_pkts, rx_pkts);

	res = ecpri_dma_set_endp_mode(rx_endp, ECPRI_DMA_NOTIFY_MODE_IRQ);
	if (res) {
		DMA_UT_LOG("Failed to change Rx test ENDP mode to POLL\n");
	}

	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		res = ecpri_dma_test_dp_suite_verify_read_write_ptr(rx_endp, &ch_info);
		if (res) {
			DMA_UT_LOG("RP/WP mismatch\n");
			return -EFAULT;
		}
	}

	return 0;
}

static int ecpri_dma_test_dp_suite_single_pkt_mult_buffer(void *priv)
{
	int res = 0;
	int num_of_pkts = 1;
	bool single_buffer = false;
	int buffs_to_replenish = 0;
	struct gsi_chan_info ch_info;
	struct ecpri_dma_pkt **tx_pkts = NULL;
	struct ecpri_dma_pkt_completion_wrapper **rx_pkts = NULL;
	struct ecpri_dma_endp_context *tx_endp = NULL, *rx_endp = NULL;

	DMA_UT_LOG("Start single packet multiple buffer test\n");

	tx_endp = &ecpri_dma_ctx->endp_ctx[ECPRI_DMA_DP_UT_SRC_ENDP_ID];
	rx_endp = &ecpri_dma_ctx->endp_ctx[ECPRI_DMA_DP_UT_DEST_ENDP_ID];

	res = ecpri_dma_test_dp_suite_prepare_test_data(num_of_pkts, single_buffer,
		&tx_pkts, &rx_pkts);
	if (res) {
		DMA_UT_LOG("failed to prepare test data\n");
		return res;
	}

	/* Prepare exact amount of credits equal to amount of buffers sent */
	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		res = ecpri_dma_test_dp_suite_calculate_credits(0, tx_pkts, num_of_pkts,
			&buffs_to_replenish);
		if (res) {
			DMA_UT_LOG("Failed to calculate credits\n");
			return res;
		}

		res = ecpri_dma_dp_test_rx_replenish(rx_endp, buffs_to_replenish);
		if (res) {
			DMA_UT_LOG("Failed to replenish test Rx endp\n");
			return res;
		}
	}

	res = ecpri_dma_dp_transmit(tx_endp, tx_pkts, num_of_pkts, true);
	if (res != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed on transmit");
		return -EFAULT;
	}

	res = ecpri_dma_test_dp_suite_wait_for_tx_comp(num_of_pkts);
	if (res != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx timeout");
		return -EFAULT;
	}

	res = ecpri_dma_test_dp_suite_verify_rx(true, num_of_pkts, tx_pkts, rx_pkts,
		&buffs_to_replenish);
	if (res != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx fail");
		return -EFAULT;
	}

	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		res = ecpri_dma_dp_test_rx_replenish(rx_endp, buffs_to_replenish);
		if (res) {
			DMA_UT_TEST_FAIL_REPORT("Test failed to perform rx replenish");
			return -EFAULT;
		}
	}

	ecpri_dma_test_dp_suite_destroy_test_data(num_of_pkts, false,
		tx_pkts, rx_pkts);

	res = ecpri_dma_set_endp_mode(rx_endp, ECPRI_DMA_NOTIFY_MODE_IRQ);
	if (res) {
		DMA_UT_LOG("Failed to change Rx test ENDP mode to POLL\n");
	}

	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		res = ecpri_dma_test_dp_suite_verify_read_write_ptr(rx_endp, &ch_info);
		if (res) {
			DMA_UT_LOG("RP/WP mismatch\n");
			return -EFAULT;
		}
	}

	return 0;
}

static int ecpri_dma_test_dp_suite_mult_pkt_single_buffer(void *priv)
{
	int res = 0;
	bool single_buffer = true;
	int buffs_to_replenish = 0;
	struct gsi_chan_info ch_info;
	struct ecpri_dma_pkt **tx_pkts = NULL;
	struct ecpri_dma_pkt_completion_wrapper **rx_pkts = NULL;
	struct ecpri_dma_endp_context *tx_endp = NULL, *rx_endp = NULL;
	int num_of_pkts_to_send = ECPRI_DMA_DP_TEST_NUM_OF_BUFFS_IN_RING;

	DMA_UT_LOG("Start multiple packet singel buffer test\n");

	tx_endp = &ecpri_dma_ctx->endp_ctx[ECPRI_DMA_DP_UT_SRC_ENDP_ID];
	rx_endp = &ecpri_dma_ctx->endp_ctx[ECPRI_DMA_DP_UT_DEST_ENDP_ID];

	res = ecpri_dma_test_dp_suite_prepare_test_data(
		num_of_pkts_to_send, single_buffer, &tx_pkts, &rx_pkts);
	if (res) {
		DMA_UT_LOG("failed to prepare test data\n");
		return res;
	}

	/* Prepare exact amount of credits equal to amount of buffers sent */
	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		res = ecpri_dma_test_dp_suite_calculate_credits(0, tx_pkts,
			num_of_pkts_to_send, &buffs_to_replenish);
		if (res) {
			DMA_UT_LOG("Failed to calculate credits\n");
			return res;
		}

		res = ecpri_dma_dp_test_rx_replenish(rx_endp, buffs_to_replenish);
		if (res) {
			DMA_UT_LOG("Failed to replenish test Rx endp\n");
			return res;
		}
	}

	res = ecpri_dma_dp_transmit(tx_endp, tx_pkts, num_of_pkts_to_send, true);
	if (res != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed on transmit");
		return -EFAULT;
	}

	res = ecpri_dma_test_dp_suite_wait_for_tx_comp(num_of_pkts_to_send);
	if (res != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx timeout");
		return -EFAULT;
	}

	res = ecpri_dma_test_dp_suite_verify_rx(true, num_of_pkts_to_send, tx_pkts,
						rx_pkts, &buffs_to_replenish);
	if (res != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx fail");
		return -EFAULT;
	}

	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		res = ecpri_dma_dp_test_rx_replenish(rx_endp, buffs_to_replenish);
		if (res) {
			DMA_UT_TEST_FAIL_REPORT("Test failed to perform rx replenish");
			return -EFAULT;
		}
	}

	ecpri_dma_test_dp_suite_destroy_test_data(num_of_pkts_to_send, true,
						   tx_pkts, rx_pkts);

	res = ecpri_dma_set_endp_mode(rx_endp, ECPRI_DMA_NOTIFY_MODE_IRQ);
	if (res) {
		DMA_UT_LOG("Failed to change Rx test ENDP mode to POLL\n");
	}

	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		res = ecpri_dma_test_dp_suite_verify_read_write_ptr(rx_endp, &ch_info);
		if (res) {
			DMA_UT_LOG("RP/WP mismatch\n");
			return -EFAULT;
		}
	}

	return 0;
}

static int ecpri_dma_test_dp_suite_mult_pkt_mult_buffer(void *priv)
{
	int res = 0;
	bool single_buffer = false;
	int buffs_to_replenish = 0;
	struct gsi_chan_info ch_info;
	struct ecpri_dma_pkt **tx_pkts = NULL;
	struct ecpri_dma_pkt_completion_wrapper **rx_pkts = NULL;
	struct ecpri_dma_endp_context *tx_endp = NULL, *rx_endp = NULL;
	int num_of_pkts_to_send =
		(ECPRI_DMA_DP_TEST_NUM_OF_BUFFS_IN_RING) /
		ECPRI_DMA_DP_TEST_MAX_BUFFS;

	DMA_UT_LOG("Start multiple packet multiple buffer test\n");

	tx_endp = &ecpri_dma_ctx->endp_ctx[ECPRI_DMA_DP_UT_SRC_ENDP_ID];
	rx_endp = &ecpri_dma_ctx->endp_ctx[ECPRI_DMA_DP_UT_DEST_ENDP_ID];

	res = ecpri_dma_test_dp_suite_prepare_test_data(
		num_of_pkts_to_send, single_buffer, &tx_pkts, &rx_pkts);
	if (res) {
		DMA_UT_LOG("failed to prepare test data\n");
		return res;
	}

	/* Prepare exact amount of credits equal to amount of buffers sent */
	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		res = ecpri_dma_test_dp_suite_calculate_credits(0, tx_pkts,
			num_of_pkts_to_send, &buffs_to_replenish);
		if (res) {
			DMA_UT_LOG("Failed to calculate credits\n");
			return res;
		}

		res = ecpri_dma_dp_test_rx_replenish(rx_endp, buffs_to_replenish);
		if (res) {
			DMA_UT_LOG("Failed to replenish test Rx endp\n");
			return res;
		}
	}

	res = ecpri_dma_dp_transmit(tx_endp, tx_pkts, num_of_pkts_to_send, true);
	if (res != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed on transmit");
		return -EFAULT;
	}

	res = ecpri_dma_test_dp_suite_wait_for_tx_comp(num_of_pkts_to_send);
	if (res != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx timeout");
		return -EFAULT;
	}

	res = ecpri_dma_test_dp_suite_verify_rx(true, num_of_pkts_to_send, tx_pkts,
		rx_pkts, &buffs_to_replenish);
	if (res != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx fail");
		return -EFAULT;
	}

	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		res = ecpri_dma_dp_test_rx_replenish(
			rx_endp, buffs_to_replenish);
		if (res) {
			DMA_UT_TEST_FAIL_REPORT("Test failed to perform rx replenish");
			return -EFAULT;
		}
	}

	ecpri_dma_test_dp_suite_destroy_test_data(num_of_pkts_to_send, single_buffer,
		tx_pkts, rx_pkts);

	res = ecpri_dma_set_endp_mode(rx_endp, ECPRI_DMA_NOTIFY_MODE_IRQ);
	if (res) {
		DMA_UT_LOG("Failed to change Rx test ENDP mode to POLL\n");
	}

	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		res = ecpri_dma_test_dp_suite_verify_read_write_ptr(rx_endp, &ch_info);
		if (res) {
			DMA_UT_LOG("RP/WP mismatch\n");
			return -EFAULT;
		}
	}

	return 0;
}

static int ecpri_dma_test_dp_suite_wrap_around_single_buffer(void *priv)
{
	int res = 0;
	bool single_buffer = true;
	int buffs_to_replenish = 0;
	struct gsi_chan_info ch_info;
	struct ecpri_dma_pkt **tx_pkts = NULL;
	struct ecpri_dma_pkt_completion_wrapper **rx_pkts = NULL;
	struct ecpri_dma_endp_context *tx_endp = NULL, *rx_endp = NULL;
	int num_of_pkts_to_send = ECPRI_DMA_DP_TEST_NUM_OF_BUFFS_IN_RING;
	int num_of_iter = 3;
	int i = 0;
	bool expecting_irq = true;

	DMA_UT_LOG("Start wrap around singel buffer test\n");

	tx_endp = &ecpri_dma_ctx->endp_ctx[ECPRI_DMA_DP_UT_SRC_ENDP_ID];
	rx_endp = &ecpri_dma_ctx->endp_ctx[ECPRI_DMA_DP_UT_DEST_ENDP_ID];

	res = ecpri_dma_test_dp_suite_prepare_test_data(
		num_of_iter * num_of_pkts_to_send, single_buffer, &tx_pkts, &rx_pkts);
	if (res) {
		DMA_UT_LOG("failed to prepare test data\n");
		return res;
	}

	for (i = 0; i < num_of_iter; i++) {
		/* Prepare exact amount of credits equal to amount of buffers sent */
		if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
			res = ecpri_dma_test_dp_suite_calculate_credits(
				i * num_of_pkts_to_send, tx_pkts,
				num_of_pkts_to_send,
				&buffs_to_replenish);
			if (res) {
				DMA_UT_LOG("Failed to calculate credits\n");
				return res;
			}

			DMA_UT_LOG("Credits to replensih %d at index %d\n",
				buffs_to_replenish, num_of_pkts_to_send);

			res = ecpri_dma_dp_test_rx_replenish(rx_endp, buffs_to_replenish);
			if (res) {
				DMA_UT_LOG("Failed to replenish test Rx endp\n");
				return res;
			}
		}

		res = ecpri_dma_dp_transmit(tx_endp,
			&tx_pkts[i * num_of_pkts_to_send],
			num_of_pkts_to_send, true);
		if (res != 0) {
			DMA_UT_TEST_FAIL_REPORT("Failed on transmit");
			return -EFAULT;
		}

		res = ecpri_dma_test_dp_suite_wait_for_tx_comp(
			(i + 1) * num_of_pkts_to_send);
		if (res != 0) {
			DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx timeout");
			return -EFAULT;
		}

		res = ecpri_dma_test_dp_suite_verify_rx(expecting_irq,
			num_of_pkts_to_send, &tx_pkts[i * num_of_pkts_to_send],
			rx_pkts, &buffs_to_replenish);
		if (res != 0) {
			DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx fail");
			return -EFAULT;
		}

		expecting_irq = false;

		if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
			res = ecpri_dma_dp_test_rx_replenish(rx_endp, buffs_to_replenish);
			if (res) {
				DMA_UT_TEST_FAIL_REPORT("Test failed to perform rx replenish");
				return -EFAULT;
			}
		}
	}

	ecpri_dma_test_dp_suite_destroy_test_data(
		num_of_iter * num_of_pkts_to_send, single_buffer,
		tx_pkts, rx_pkts);

	res = ecpri_dma_set_endp_mode(rx_endp, ECPRI_DMA_NOTIFY_MODE_IRQ);
	if (res) {
		DMA_UT_LOG("Failed to change Rx test ENDP mode to POLL\n");
	}

	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		res = ecpri_dma_test_dp_suite_verify_read_write_ptr(rx_endp, &ch_info);
		if (res) {
			DMA_UT_LOG("RP/WP mismatch\n");
			return -EFAULT;
		}
	}

	return 0;
}

static int ecpri_dma_test_dp_suite_wrap_around_mult_buffer(void *priv)
{
	int res = 0;
	bool single_buffer = false;
	int buffs_to_replenish = 0;
	struct gsi_chan_info ch_info;
	struct ecpri_dma_pkt **tx_pkts = NULL;
	struct ecpri_dma_pkt_completion_wrapper **rx_pkts = NULL;
	struct ecpri_dma_endp_context *tx_endp = NULL, *rx_endp = NULL;
	int num_of_pkts_to_send =
		(ECPRI_DMA_DP_TEST_NUM_OF_BUFFS_IN_RING) /
		ECPRI_DMA_DP_TEST_MAX_BUFFS;
	int num_of_iter = 3;
	int i = 0;
	bool expecting_irq = true;

	DMA_UT_LOG("Start wrap around mult buffer test\n");

	tx_endp = &ecpri_dma_ctx->endp_ctx[ECPRI_DMA_DP_UT_SRC_ENDP_ID];
	rx_endp = &ecpri_dma_ctx->endp_ctx[ECPRI_DMA_DP_UT_DEST_ENDP_ID];

	res = ecpri_dma_test_dp_suite_prepare_test_data(
		num_of_iter * num_of_pkts_to_send, single_buffer, &tx_pkts, &rx_pkts);
	if (res) {
		DMA_UT_LOG("failed to prepare test data\n");
		return res;
	}

	for (i = 0; i < num_of_iter; i++) {
		/* Prepare exact amount of credits equal to amount of buffers sent */
		if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
			res = ecpri_dma_test_dp_suite_calculate_credits(
				i * num_of_pkts_to_send, tx_pkts,
				num_of_pkts_to_send,
				&buffs_to_replenish);
			if (res) {
				DMA_UT_LOG("Failed to calculate credits\n");
				return res;
			}

			DMA_UT_LOG("Credits to replensih %d at index %d\n",
				buffs_to_replenish, num_of_pkts_to_send);

			res = ecpri_dma_dp_test_rx_replenish(rx_endp, buffs_to_replenish);
			if (res) {
				DMA_UT_LOG("Failed to replenish test Rx endp\n");
				return res;
			}
		}

		res = ecpri_dma_dp_transmit(tx_endp,
			&tx_pkts[i * num_of_pkts_to_send],
			num_of_pkts_to_send, true);
		if (res != 0) {
			DMA_UT_TEST_FAIL_REPORT("Failed on transmit");
			return -EFAULT;
		}

		res = ecpri_dma_test_dp_suite_wait_for_tx_comp((i+1) *
			num_of_pkts_to_send);
		if (res != 0) {
			DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx timeout");
			return -EFAULT;
		}

		res = ecpri_dma_test_dp_suite_verify_rx(expecting_irq,
			num_of_pkts_to_send, &tx_pkts[i * num_of_pkts_to_send],
			rx_pkts, &buffs_to_replenish);
		if (res != 0) {
			DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx fail");
			return -EFAULT;
		}

		expecting_irq = false;

		if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
			res = ecpri_dma_dp_test_rx_replenish(
				rx_endp, buffs_to_replenish);
			if (res) {
				DMA_UT_TEST_FAIL_REPORT("Test failed to perform rx replenish");
				return -EFAULT;
			}
		}
	}

	ecpri_dma_test_dp_suite_destroy_test_data(
		num_of_iter * num_of_pkts_to_send, single_buffer,
		tx_pkts, rx_pkts);

	res = ecpri_dma_set_endp_mode(rx_endp, ECPRI_DMA_NOTIFY_MODE_IRQ);
	if (res) {
		DMA_UT_LOG("Failed to change Rx test ENDP mode to POLL\n");
	}

	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		res = ecpri_dma_test_dp_suite_verify_read_write_ptr(rx_endp, &ch_info);
		if (res) {
			DMA_UT_LOG("RP/WP mismatch\n");
			return -EFAULT;
		}
	}

	return 0;
}

/* Suite definition block */
DMA_UT_DEFINE_SUITE_START(driver_dp, "Driver Data Path suite",
			  ecpri_dma_test_dp_suite_setup,
			  ecpri_dma_test_dp_suite_teardown){
	DMA_UT_ADD_TEST(
		single_pkt_single_buffer,
		"This test will verify data path by sending one packet with single buffer on SRC ENDP and verifying it is received the same on DEST ENDP",
		ecpri_dma_test_dp_suite_single_pkt_single_buffer, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
	DMA_UT_ADD_TEST(
		single_pkt_mult_buffer,
		"This test will verify data path by sending one packet with multiple buffers on SRC ENDP and verifying it is received the same on DEST ENDP",
		ecpri_dma_test_dp_suite_single_pkt_mult_buffer, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
	DMA_UT_ADD_TEST(
		mult_pkt_single_buffer,
		"This test will verify data path by sending multiple packets with single buffer on SRC ENDP and verifying they are received the same on DEST ENDP",
		ecpri_dma_test_dp_suite_mult_pkt_single_buffer, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
	DMA_UT_ADD_TEST(
		mult_pkt_mult_buffer,
		"This test will verify data path by sending multiple packets with multiple buffers on SRC ENDP and verifying they are received the same on DEST ENDP",
		ecpri_dma_test_dp_suite_mult_pkt_mult_buffer, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
	DMA_UT_ADD_TEST(
		wrap_around_single_buffer,
		"This test will verify data path by sending multiple packets with single buffer for more than one iteration on SRC ENDP and verifying they are received the same on DEST ENDP",
		ecpri_dma_test_dp_suite_wrap_around_single_buffer, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
	DMA_UT_ADD_TEST(
		wrap_around_mult_buffer,
		"This test will verify data path by sending multiple packets with multiple buffers for more than one iteration on SRC ENDP and verifying they are received the same on DEST ENDP",
		ecpri_dma_test_dp_suite_wrap_around_mult_buffer, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
} DMA_UT_DEFINE_SUITE_END(driver_dp);
