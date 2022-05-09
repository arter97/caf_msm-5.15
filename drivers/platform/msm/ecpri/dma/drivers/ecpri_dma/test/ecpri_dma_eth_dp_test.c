/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "ecpri_dma_ut_framework.h"
#include "ecpri_dma_utils.h"
#include "ecpri_dma_dp.h"
#include "ecpri_dma_eth_client.h"
#include "dmahal.h"

extern struct ecpri_dma_eth_client_context *ecpri_dma_eth_client_ctx;

/**
  * ECPRI DMA ETH Client Unit-test suite
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

#define ECPRI_DMA_ETH_CLIENT_UT_LINK_INDEX                0
#define ECPRI_DMA_ETH_CLIENT_UT_READY_RCVD_TMOUT_MS       500
#define ECPRI_DMA_ETH_CLIENT_UT_WAIT_FOR_CMPLTN_TIMEOUT   300
#define ECPRI_DMA_ETH_CLIENT_UT_MODER_CNTR_THRSHLD        1
#define ECPRI_DMA_ETH_CLIENT_UT_MODER_TIMER_THRSHLD       0
#define ECPRI_DMA_ETH_CLIENT_UT_TEST_PACKET_CONTENT       0x12345678
#define ECPRI_DMA_ETH_CLIENT_UT_READY_USER_DATA_VAL       (0xDEAD0001)
#define ECPRI_DMA_ETH_CLIENT_UT_RX_USER_DATA_VAL          (0xDEAD0002)
#define ECPRI_DMA_ETH_CLIENT_UT_TX_USER_DATA_VAL          (0xDEAD0003)

#define ECPRI_DMA_ETH_CLIENT_UT_TEST_RX_BUDGET            (5)
#define ECPRI_DMA_ETH_CLIENT_UT_ENDP_DIR_MAX (ECPRI_DMA_ENDP_DIR_DEST + 1)

/* Define for test endp ring length, in packets */
#define ECPRI_DMA_ETH_CLIENT_UT_RING_LENGTH               10

/* Define for number of buffs in ring */
#define ECPRI_DMA_ETH_CLIENT_UT_NUM_OF_BUFFS_IN_RING \
	(ECPRI_DMA_ETH_CLIENT_UT_RING_LENGTH - 1)

/* For loopback configuration used first A55 SRC ENDP and first A55 DEST ENDP*/
#define ECPRI_DMA_ETH_CLIENT_UT_SRC_ENDP_ID               0
#define ECPRI_DMA_ETH_CLIENT_UT_DEST_ENDP_ID              37

/* Define for test max buffers per packet */
#define ECPRI_DMA_ETH_CLIENT_UT_TEST_MAX_BUFFS            3

/* Define for test endp buffer size */
#define ECPRI_DMA_ETH_CLIENT_UT_TEST_BUFF_SIZE            1500

/* Define for test endp buffer size */
#define ECPRI_DMA_ETH_CLIENT_UT_TEST_TX_BUFF_SIZE            16

/* Define for test endp large buffer size */
#define ECPRI_DMA_ETH_CLIENT_UT_TEST_LARGE_BUFFER_SIZE    2000

/* Define for test endp large buffer size */
#define ECPRI_DMA_ETH_CLIENT_UT_TEST_LARGE_BUFFER_SIZE    2000

/* Define for test large payload number of buffers */
#define ECPRI_DMA_ETH_CLIENT_UT_TEST_LARGE_BUFFERS_AMOUNT 2

/**
 * ecpri_dma_eth_client_test_suite_context - all the information that
 * has to be available globally during the test
 * @rx_cb_user_data - user data for Rx callback function
 * @tx_cb_user_data - user data for Tx callback function
 * @tx_comp_pkts_num - number of transmited packets
 * @ready_cb_user_data - user data for Ready notify callback
 * @rx_received_irq_in_poll - amount of IRQs received
 * while the driver is in POLL mode
 * @hdl - uniaue connection hadle
 * @expected_mode - mode driver expected to be during a specific test
 * @connection - the pointer to connection
 * @num_irq_received - amount of IRQs received
 * @ready_received - completion for ready notificatuibs
 * @irq_received - completion for IRQ receiving
 * @rx_pkts - array of pointers to Rx packets
 * @rx_pkt_idx - index of Rx packet
 */
struct ecpri_dma_eth_client_test_suite_context {
	u32 rx_cb_user_data;
	u32 tx_cb_user_data;
	u32 tx_comp_pkts_num;
	u32 ready_cb_user_data;
	u32 rx_received_irq_in_poll;
	ecpri_dma_eth_conn_hdl_t hdl;
	struct ecpri_dma_eth_client_connection* connection;
	struct completion ready_received;
	u32 num_irq_received[ECPRI_DMA_ETH_CLIENT_UT_ENDP_DIR_MAX];
	struct completion irq_received[ECPRI_DMA_ETH_CLIENT_UT_ENDP_DIR_MAX];
	struct ecpri_dma_pkt* rx_pkts[ECPRI_DMA_ETH_CLIENT_UT_NUM_OF_BUFFS_IN_RING];
	u32 rx_pkt_idx;
	bool allocated_pkts;
};

struct ecpri_dma_eth_client_test_suite_context eth_client_test_suite_ctx;

static int ecpri_dma_eth_dp_test_suite_calculate_credits(int idx,
	struct ecpri_dma_pkt** tx_pkts, int num_of_pkts, int* credits)
{
	int i;
	int ret = 0;
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

		if (ECPRI_DMA_ETH_CLIENT_UT_TEST_BUFF_SIZE < total_buff_size) {
			total_credits += (total_buff_size / ECPRI_DMA_ETH_CLIENT_UT_TEST_BUFF_SIZE);
			/* Ceiling */
			if (total_buff_size % ECPRI_DMA_ETH_CLIENT_UT_TEST_BUFF_SIZE != 0) {
				total_credits++;
			}
		}
		else {
			total_credits++;
		}
	}

	*credits = total_credits;
	DMA_UT_LOG("Total credits %d\n", total_credits);
	return ret;
}

/**
 * ecpri_dma_eth_dp_test_util_get_conn_from_hdl() - checks and get connection
 * for the given connection handle
 *
 * @hdl - the unique connection handle
 */
static struct ecpri_dma_eth_client_connection*
ecpri_dma_eth_dp_test_util_get_conn_from_hdl (ecpri_dma_eth_conn_hdl_t hdl)
{
	struct ecpri_dma_eth_client_connection *connection;

	spin_lock_bh(&ecpri_dma_eth_client_ctx->idr_lock);
	connection = idr_find(&ecpri_dma_eth_client_ctx->idr, hdl);
	spin_unlock_bh(&ecpri_dma_eth_client_ctx->idr_lock);

	return connection;
}

/**
 * ecpri_dma_eth_dp_test_util_setup_dma_endps() - Configures the driver to the
 * loopback mode by using specific endpoints.
 *
 * @dir - direction of datapath
 *
 */
static int ecpri_dma_eth_dp_test_util_setup_dma_endps(enum ecpri_dma_endp_dir dir,
	bool enable_loopback) {
	int endp_id;
	ecpri_hwio_def_ecpri_endp_cfg_destn_u endp_cfg_dest = { 0 };
	ecpri_hwio_def_ecpri_endp_cfg_xbarn_u endp_cfg_xbar = { 0 };
	ecpri_hwio_def_ecpri_endp_gsi_cfg_n_u endp_gsi_cfg = { 0 };
	ecpri_hwio_def_ecpri_endp_cfg_aggr_n_u cfg_aggr = { 0 };
	ecpri_hwio_def_ecpri_endp_nfapi_reassembly_cfg_n_u reassembly_cfg = { 0 };

	if (!ecpri_dma_ctx->endp_map)
		return -EINVAL;

	if (dir == ECPRI_DMA_ENDP_DIR_SRC)
		endp_id = ECPRI_DMA_ETH_CLIENT_UT_SRC_ENDP_ID;
	else
		endp_id = ECPRI_DMA_ETH_CLIENT_UT_DEST_ENDP_ID;

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
				ECPRI_DMA_ETH_CLIENT_UT_DEST_ENDP_ID;
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

/**
 * Parameters are being verified outside of this function and there's no need
 * to verify them again.
 */
static void ecpri_dma_eth_dp_test_util_conn_hdl_remove(
	ecpri_dma_eth_conn_hdl_t hdl)
{
	spin_lock_bh(&ecpri_dma_eth_client_ctx->idr_lock);
	idr_remove(&ecpri_dma_eth_client_ctx->idr, hdl);
	spin_unlock_bh(&ecpri_dma_eth_client_ctx->idr_lock);
}

/**
 * ecpri_dma_eth_dp_test_util_prepare_test_data() - allocates data needed
 * for specific test
 * @num_of_pkts - number of packets to allocate
 * @single_buffer - single buffer packet or not
 * @tx_pkts - pointer to array of packets content to transmit
 * @rx_pkts - wrapper with relevant information about received packets
 *
 */
static int ecpri_dma_eth_dp_test_util_prepare_test_data(int num_of_pkts,
		bool single_buffer, struct ecpri_dma_pkt*** tx_pkts_ptr,
		struct ecpri_dma_pkt_completion_wrapper*** rx_pkts_ptr)
{
	int i = 0, j = 0;
	int ret = 0;
	u32 num_of_buffs;
	u32 packet_content = ECPRI_DMA_ETH_CLIENT_UT_TEST_PACKET_CONTENT;
	struct ecpri_dma_pkt** tx_pkts;
	struct ecpri_dma_pkt_completion_wrapper** rx_pkts;

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
			ret = -ENOMEM;
			goto fail_alloc;
		}
	}

	tx_pkts = kzalloc(sizeof(*tx_pkts) * num_of_pkts, GFP_KERNEL);
	if (!tx_pkts) {
		DMA_UT_LOG("failed to alloc Tx packets array \n");
		return -ENOMEM;
	}
	memset(tx_pkts, 0, sizeof(*tx_pkts) * num_of_pkts);

	num_of_buffs = single_buffer ? 1 : ECPRI_DMA_ETH_CLIENT_UT_TEST_MAX_BUFFS;

	for (i = 0; i < num_of_pkts; i++) {
		tx_pkts[i] =
			kzalloc(sizeof(*tx_pkts[i]), GFP_KERNEL);
		if (!tx_pkts[i]) {
			DMA_UT_LOG("failed to alloc tx packet struct \n");
			ret = -ENOMEM;
			goto fail_alloc;
		}

		tx_pkts[i]->buffs =
			kzalloc(sizeof(tx_pkts[i]->buffs) * num_of_buffs, GFP_KERNEL);
		if (!tx_pkts[i]->buffs) {
			DMA_UT_LOG("failed to alloc buffers array \n");
			kfree(tx_pkts[i]);
			ret = -ENOMEM;
			goto fail_alloc;
		}

		for (j = 0; j < num_of_buffs; j++) {
			tx_pkts[i]->buffs[j] = kzalloc(
				sizeof(tx_pkts[i]->buffs[j]), GFP_KERNEL);
			if (!tx_pkts[i]->buffs[j]) {
				DMA_UT_LOG("failed to alloc dma buff wrapper\n");
				kfree(tx_pkts[i]->buffs);
				kfree(tx_pkts[i]);
				ret = -ENOMEM;
				goto fail_alloc;
			}

			tx_pkts[i]->buffs[j]->virt_base =
				kzalloc(ECPRI_DMA_ETH_CLIENT_UT_TEST_TX_BUFF_SIZE,
					GFP_KERNEL);
			if (!tx_pkts[i]->buffs[j]->virt_base) {
				DMA_UT_LOG("failed to alloc buffer\n");
				kfree(tx_pkts[i]->buffs[j]);

				for (j--; j >= 0; j--) {
					kfree(tx_pkts[i]->buffs[j]->virt_base);
					kfree(tx_pkts[i]->buffs[j]);
				}
				kfree(tx_pkts[i]->buffs);
				kfree(tx_pkts[i]);
				ret = -ENOMEM;
				goto fail_alloc;
			}

			tx_pkts[i]->buffs[j]->size =
				ECPRI_DMA_ETH_CLIENT_UT_TEST_TX_BUFF_SIZE;

			*(u32*)tx_pkts[i]->buffs[j]->virt_base =
				packet_content++;
		}
		tx_pkts[i]->num_of_buffers = num_of_buffs;
	}

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
	return ret;
}

/**
 * ecpri_dma_eth_dp_test_util_prepare_test_large_data() - allocates
 * large payload packets and necessary data needed for specific test
 * @num_of_pkts - number of packets to allocate
 * @tx_pkts - pointer to array of packets content to transmit
 * @rx_pkts - wrapper with relevant information about received packets
 *
 */
static int ecpri_dma_eth_dp_test_util_prepare_test_large_data(
	struct ecpri_dma_pkt*** tx_pkts_ptr,
	struct ecpri_dma_pkt_completion_wrapper*** rx_pkts_ptr)
{
	int i = 0, k = 0;
	int ret = 0;
	u32 packet_content = ECPRI_DMA_ETH_CLIENT_UT_TEST_PACKET_CONTENT;
	struct ecpri_dma_pkt** tx_pkts;
	struct ecpri_dma_pkt_completion_wrapper** rx_pkts;
	int large_payload_last_index =
		(ECPRI_DMA_ETH_CLIENT_UT_TEST_LARGE_BUFFER_SIZE /
			sizeof(u32)) - 1;

	rx_pkts = kzalloc(sizeof(*rx_pkts) * 2, GFP_KERNEL);
	if (!rx_pkts) {
		DMA_UT_LOG("failed to alloc Rx packets array \n");
		return -ENOMEM;
	}
	memset(rx_pkts, 0, sizeof(*rx_pkts) * 2);

	for (i = 0; i < 2; i++) {
		rx_pkts[i] =
			kzalloc(sizeof(*rx_pkts[i]), GFP_KERNEL);
		if (!rx_pkts[i]) {
			DMA_UT_LOG("failed to alloc rx packet struct \n");
			ret = -ENOMEM;
			goto fail_alloc;
		}
	}

	tx_pkts = kzalloc(sizeof(*tx_pkts), GFP_KERNEL);
	if (!tx_pkts) {
		DMA_UT_LOG("failed to alloc Tx packets array \n");
		return -ENOMEM;
	}
	memset(tx_pkts, 0, sizeof(*tx_pkts));

	tx_pkts[0] = kzalloc(sizeof(*tx_pkts[0]), GFP_KERNEL);
	if (!tx_pkts[0]) {
		DMA_UT_LOG("failed to alloc tx packet struct \n");
		ret = -ENOMEM;
		goto fail_alloc;
	}

	tx_pkts[0]->buffs =
		kzalloc(sizeof(tx_pkts[0]->buffs), GFP_KERNEL);
	if (!tx_pkts[0]->buffs) {
		DMA_UT_LOG("failed to alloc buffers array \n");
		kfree(tx_pkts[0]);
		ret = -ENOMEM;
		goto fail_alloc;
	}

	tx_pkts[0]->buffs[0] = kzalloc(
		sizeof(tx_pkts[0]->buffs[0]), GFP_KERNEL);
	if (!tx_pkts[0]->buffs[0]) {
		DMA_UT_LOG("failed to alloc dma buff wrapper\n");
		kfree(tx_pkts[0]->buffs);
		kfree(tx_pkts[0]);
		ret = -ENOMEM;
		goto fail_alloc;
	}

	tx_pkts[0]->buffs[0]->virt_base =
		kzalloc(ECPRI_DMA_ETH_CLIENT_UT_TEST_LARGE_BUFFER_SIZE,
			GFP_KERNEL);
	if (!tx_pkts[0]->buffs[0]->virt_base) {
		DMA_UT_LOG("failed to alloc buffer\n");
		kfree(tx_pkts[0]->buffs[0]);
		kfree(tx_pkts[0]->buffs);
		kfree(tx_pkts[0]);
		ret = -ENOMEM;
		goto fail_alloc;
	}

	tx_pkts[0]->buffs[0]->size =
		ECPRI_DMA_ETH_CLIENT_UT_TEST_LARGE_BUFFER_SIZE;

	for (k = 0; k < large_payload_last_index; k++) {
			((u32*)tx_pkts[0]->buffs[0]->
				virt_base)[k] =
				packet_content++;
		}
	tx_pkts[0]->num_of_buffers = 1;

	*tx_pkts_ptr = tx_pkts;
	*rx_pkts_ptr = rx_pkts;

	return 0;

fail_alloc:
	for (i--; i >= 0; i--) {
		kfree(rx_pkts[i]);
	}
	kfree(rx_pkts);
	return ret;
}

/**
 *ecpri_dma_eth_dp_test_util_destroy_test_data() - free allocated resources
 * for test data
 *
 * @num_of_pkts - number of packets to allocate
 * @single_buffer - indicates if test data packet contains single buffer
 * @tx_pkts - pointer to array of packets content to transmit
 * @rx_pkts - wrapper with relevant information about received packets
 *
 */
static void ecpri_dma_eth_dp_test_util_destroy_test_data(
	int num_of_pkts, bool single_buffer, struct ecpri_dma_pkt** tx_pkts,
	struct ecpri_dma_pkt_completion_wrapper** rx_pkts)
{
	int i = 0, j = 0;
	u32 num_of_buffs;

	num_of_buffs = single_buffer ? 1 : ECPRI_DMA_ETH_CLIENT_UT_TEST_MAX_BUFFS;
	for (i = 0; i < num_of_pkts; i++) {
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

	eth_client_test_suite_ctx.num_irq_received[ECPRI_DMA_ENDP_DIR_SRC] = 0;
	eth_client_test_suite_ctx.num_irq_received[ECPRI_DMA_ENDP_DIR_SRC] = 0;
	eth_client_test_suite_ctx.rx_received_irq_in_poll = 0;
	eth_client_test_suite_ctx.tx_comp_pkts_num = 0;
}

static int ecpri_dma_eth_dp_test_disconnect_endpoints(
	ecpri_dma_eth_conn_hdl_t hdl)
{
	int ret = 0;
	struct ecpri_dma_eth_client_connection* connection;

	DMADBG_LOW("Begin\n");

	if (!ecpri_dma_eth_client_ctx
		|| !ecpri_dma_eth_client_ctx->is_eth_ready) {
		DMAERR("Context not initialized\n");
		return -EPERM;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_dp_test_util_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMAERR("Connection invalid handle:%x\n", hdl);
		return -EINVAL;
	}

	ecpri_dma_eth_dp_test_util_conn_hdl_remove(connection->hdl);

	ret = ecpri_dma_reset_endp(connection->tx_endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to reset Tx endpoint, ENDP ID:%d\n",
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}

	/* Deallocation of GSI channels */
	ret = ecpri_dma_dealloc_endp(connection->tx_endp_ctx);

	if (ret != 0) {
		DMAERR("Unable to de-allocate Tx endpoint, ENDP ID:%d\n",
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}

	ret = ecpri_dma_reset_endp(connection->rx_endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to reset Rx endpoint, ENDP ID:%d\n",
			connection->rx_endp_ctx->endp_id);
		return -EINVAL;
	}

	ret = ecpri_dma_dealloc_endp(connection->rx_endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to de-allocate Rx endpoint, ENDP ID:%d\n",
			connection->rx_endp_ctx->endp_id);
		return -EINVAL;
	}

	/* Disabling endpoints in DMA HW */
	ret = ecpri_dma_disable_dma_endp(connection->tx_endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to disable channel - Tx, ENDP ID:%d\n",
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}
	connection->tx_endp_ctx->hdl = 0;

	ret = ecpri_dma_disable_dma_endp(connection->rx_endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to disable channel - Rx, ENDP ID:%d\n",
			connection->rx_endp_ctx->endp_id);
		return -EINVAL;
	}
	connection->rx_endp_ctx->hdl = 0;

	connection->valid = false;
	connection->hdl = 0;
	connection->p_type = 0;
	connection->rx_notify_mode = 0;
	connection->link_idx = 0;
	connection->received_irq_during_poll = 0;
	connection->tx_mod_cfg.moderation_timer_threshold = 0;
	connection->tx_mod_cfg.moderation_counter_threshold = 0;

	DMADBG_LOW("Exit\n");

	return ret;
}

static int ecpri_dma_test_eth_dp_test_suite_verify_read_write_ptr(
	struct ecpri_dma_endp_context* rx_endp)
{
	int ret = 0;
	struct gsi_chan_info ch_info;

	ret = gsi_query_channel_info(rx_endp->gsi_chan_hdl, &ch_info);
	if (ret) {
		DMA_UT_LOG("Failed to query Rx channel info\n");
		return -EFAULT;
	}

	if (ch_info.rp != ch_info.wp) {
		DMA_UT_LOG("Channel: 0x%x - rp:  0x%x and "
			"wp: 0x%x mismatch \n", rx_endp->gsi_chan_hdl,
			(unsigned long)ch_info.rp, (unsigned long)ch_info.wp);
		return -EFAULT;
	}

	return ret;
}

static int ecpri_dma_eth_dp_test_util_wait_for_tx_comp(u32 num_of_pkts_sent)
{
	int ret = 0;
	while (eth_client_test_suite_ctx.tx_comp_pkts_num !=
	       num_of_pkts_sent) {
		ret = wait_for_completion_timeout(
			&eth_client_test_suite_ctx.
			irq_received[ECPRI_DMA_ENDP_DIR_SRC],
			msecs_to_jiffies(100));
		if (ret == 0 &&
			eth_client_test_suite_ctx.tx_comp_pkts_num !=
			num_of_pkts_sent)
		{
			DMA_UT_LOG("Test failed due to Tx timeout");
			return -EFAULT;
		}
	}

	return 0;
}

static void ecpri_dma_eth_dp_test_ready_cb(void *user_data)
{
	if (eth_client_test_suite_ctx.ready_cb_user_data
	    != (*(u32 *)user_data)) {
		DMA_UT_TEST_FAIL_REPORT("Received mismatched "
					"Ready user data\n");
		return;
	}
	complete(&eth_client_test_suite_ctx.ready_received);
}

static void ecpri_dma_eth_dp_test_rx_comp_cb(void *user_data,
					 ecpri_dma_eth_conn_hdl_t hdl)
{
	int ret = 0;
	enum ecpri_dma_notify_mode mode;
	struct ecpri_dma_eth_client_connection *connection;

	/* Check if the connection handle is correct */
	if (eth_client_test_suite_ctx.hdl != hdl) {
		DMA_UT_LOG("Test failed due to invalid connection handle,"
			" handle received:%d, handle expected:%d\n", hdl,
			eth_client_test_suite_ctx.hdl);
		return;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_dp_test_util_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMA_UT_LOG("Test failed due to invalid connection or connection"
			   " handle:%x, ENDP ID:%d\n", hdl,
			connection->rx_endp_ctx->endp_id);
		return;
	}

	if (eth_client_test_suite_ctx.rx_cb_user_data
	    != (*(u32 *)user_data)) {
		DMA_UT_TEST_FAIL_REPORT("Received mismatched "
					"Rx user data\n");
		return;
	}

	ret = ecpri_dma_eth_rx_mode_get(hdl, &mode);
	if (ret) {
		DMA_UT_LOG("Failed to get current Rx test ENDP mode\n");
	}

	DMA_UT_LOG("Got IRQ on Rx test while in polling mode\n");
	eth_client_test_suite_ctx.rx_received_irq_in_poll++;

	eth_client_test_suite_ctx.num_irq_received[ECPRI_DMA_ENDP_DIR_DEST]++;
	complete(&eth_client_test_suite_ctx.irq_received[
		ECPRI_DMA_ENDP_DIR_DEST]);
}

static void ecpri_dma_eth_dp_test_tx_comp_cb(
	void *user_data, ecpri_dma_eth_conn_hdl_t hdl,
	struct ecpri_dma_pkt_completion_wrapper **comp_pkts,
	u32 num_of_completed)
{
	struct ecpri_dma_eth_client_connection *connection;

	/* Check if the connection handle is correct */
	if (eth_client_test_suite_ctx.hdl != hdl) {
		DMA_UT_LOG("Test failed due to invalid connection handle,"
			" handle received:%d, handle expected:%d\n", hdl,
			eth_client_test_suite_ctx.hdl);
		return;
	}

	if (eth_client_test_suite_ctx.tx_cb_user_data != (*(u32 *)user_data)) {
		DMA_UT_TEST_FAIL_REPORT("Received mismatched "
					"Tx user data\n");
		return;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_dp_test_util_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMA_UT_LOG("Test failed due to invalid connection or connection"
			   " handle:%x, ENDP ID:%d\n", hdl,
			connection->rx_endp_ctx->endp_id);
		return;
	}

	eth_client_test_suite_ctx.tx_comp_pkts_num += num_of_completed;
	eth_client_test_suite_ctx.num_irq_received[ECPRI_DMA_ENDP_DIR_SRC]++;

	complete(&eth_client_test_suite_ctx.irq_received[
		ECPRI_DMA_ENDP_DIR_SRC]);
}

/**
 * ecpri_dma_eth_dp_test_util_verify_rx() - Verifies that the DMA
 * driver received, modified, and correctly partitioned the packet.
 *
 * @hdl - the unique conection handle with all the data
 * @num_of_pkts_sent - num of packets to expect ot receive
 * @tx_pkts - array to pointers to packets that have been sent
 * @rx_pkts - array to pointers to wrapper of packets that have been received
 * @expected_mode - mode that driver is expected to be in
 * @new_mode - indicates state driver should be in after the verification
 */
static int ecpri_dma_eth_dp_test_util_verify_rx(ecpri_dma_eth_conn_hdl_t hdl,
	u32 num_of_pkts_sent, struct ecpri_dma_pkt** tx_pkts,
	struct ecpri_dma_pkt_completion_wrapper** rx_pkts,
	enum ecpri_dma_notify_mode expected_mode,
	enum ecpri_dma_notify_mode new_mode,
	int* total_rx_buffs_to_replenish)
{
	int ret = 0;
	int actual_num = 0;
	int sent_pkt_idx = 0, sent_pkt_buff_idx = 0;
	int i = 0;
	int curr_pkt_recv_size = 0;
	unsigned long tx_buff_size = 0;
	void* tx_buff_to_compare = NULL;
	void* rx_buff_to_compare = NULL;
	enum ecpri_dma_notify_mode curr_mode;
	struct ecpri_dma_eth_client_connection* connection;
	int timeout_msec = (expected_mode == ECPRI_DMA_NOTIFY_MODE_POLL) ?
		ECPRI_DMA_ETH_CLIENT_UT_WAIT_FOR_CMPLTN_TIMEOUT : 100;

	*total_rx_buffs_to_replenish = 0;

	/* Retrieve Connection */
	connection = ecpri_dma_eth_dp_test_util_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMA_UT_LOG("Test failed due to invalid connection or connection"
			" handle:%x, ENDP ID:%d\n", hdl,
			connection->rx_endp_ctx->endp_id);
		return -EFAULT;
	}

	ret = wait_for_completion_timeout(
		&eth_client_test_suite_ctx.
		irq_received[ECPRI_DMA_ENDP_DIR_DEST],
		msecs_to_jiffies(timeout_msec));

	/*
	 * - wait_for_completion_timeout - returns time left to timeout.
	 * - For IRQ mode, we expect to get IRQ before completion times out (ret != 0).
	 * - For POLL mode, we don't expect any IRQ, so we expect completion to timeout
	 *   (ret == 0).
	 */
	switch (expected_mode) {
	case ECPRI_DMA_NOTIFY_MODE_POLL:
		if (ret != 0) {
			DMA_UT_LOG("Test failed due to Rx is not timed out");
			return -EFAULT;
		}
		break;
	case ECPRI_DMA_NOTIFY_MODE_IRQ:
		if (ret == 0) {
			DMA_UT_LOG("Test failed due to Rx timeout");
			return -EFAULT;
		}
		break;
	default:
		if (ret == 0) {
			DMA_UT_LOG("Test failed due to unexpected Rx timeout");
		}
		break;
	}

	/* Check if state changed */
	if (connection->rx_notify_mode != ECPRI_DMA_NOTIFY_MODE_POLL) {
		DMA_UT_LOG("Test failed due to set to POLL mode, "
			"the mode hasn't been changed, "
			"current mode: %d\n",
			connection->rx_notify_mode);
		return -EFAULT;
	}

	while (sent_pkt_idx != num_of_pkts_sent) {
		ret = ecpri_dma_eth_rx_poll(hdl, ECPRI_DMA_ETH_CLIENT_UT_TEST_RX_BUDGET,
			rx_pkts, &actual_num);
		if (ret || !actual_num) {
			DMA_UT_LOG("Test failed to perform Rx poll");
			return -EFAULT;
		}

		tx_buff_to_compare =
			tx_pkts[sent_pkt_idx]->buffs[sent_pkt_buff_idx]->virt_base;
		tx_buff_size = tx_pkts[sent_pkt_idx]->buffs[sent_pkt_buff_idx]->size;
		for (i = 0; i < actual_num; i++) {
			curr_pkt_recv_size = rx_pkts[i]->pkt->buffs[0]->size;

			if (curr_pkt_recv_size < tx_buff_size &&
				curr_pkt_recv_size == ECPRI_DMA_ETH_CLIENT_UT_TEST_TX_BUFF_SIZE)
			{
				/* Over flow event */
				if (memcmp(
					tx_buff_to_compare,
					rx_pkts[i]->pkt->buffs[0]->virt_base,
					curr_pkt_recv_size)) {
					DMA_UT_LOG(
						"Test failed due to buffers don't match");
					return -EFAULT;
				}
				tx_buff_to_compare =
					(void*)((unsigned long)tx_buff_to_compare +
						rx_pkts[i]->pkt->buffs[0]->size);
				tx_buff_size -= ECPRI_DMA_ETH_CLIENT_UT_TEST_TX_BUFF_SIZE;
			}
			else if (curr_pkt_recv_size == tx_buff_size)
			{
				/* Buffers context matches and Tx EOT */
				if (memcmp(
					tx_buff_to_compare,
					rx_pkts[i]->pkt->buffs[0]->virt_base,
					curr_pkt_recv_size)) {
					DMA_UT_LOG(
						"Test failed due to buffers don't match");
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
								"Test failed due to buffers don't match");
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
								"Test failed due to buffers don't match");
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

	ret = ecpri_dma_eth_rx_mode_get(hdl, &curr_mode);
	if (ret != 0) {
		DMA_UT_LOG("Test failed due to get to the current mode, "
			"current mode: %d\n",
			curr_mode);
		return -EFAULT;
	}

	if (curr_mode != new_mode)
	{
		ret = ecpri_dma_eth_rx_mode_set(hdl, new_mode);
		if (ret != 0) {
			DMA_UT_LOG("Test failed due to set to the new mode, "
				"new mode: %d, current mode: %d\n",
				new_mode, connection->rx_notify_mode);
			return -EFAULT;
		}
	}

	/* Check if switched */
	if (connection->rx_notify_mode != new_mode) {
		DMA_UT_LOG("Test failed due to set to the new mode, "
			"current mode: %d, new mode: %d\n",
			connection->rx_notify_mode, new_mode);
		return -EFAULT;
	}

	return 0;
}

/**
 * ecpri_dma_eth_dp_test_util_verify_rx_large_data() - Verifies that the DMA
 * driver received, modified, and correctly partitioned the packet with a large
 * payload (buffer size - 2000). Checks partitioned parts content
 * and flags.
 *
 * @hdl - the unique conection handle with all the data
 * @num_of_pkts_sent - num of packets to expect ot receive
 * @tx_pkts - array to pointers to packets that have been sent
 * @rx_pkts - array to pointers to wrapper of packets that have been received
 * @expected_mode - mode that driver is expected to be in
 */
static int ecpri_dma_eth_dp_test_util_verify_rx_large_data(
	ecpri_dma_eth_conn_hdl_t hdl, u32 num_of_pkts_sent,
	struct ecpri_dma_pkt **tx_pkts,
	struct ecpri_dma_pkt_completion_wrapper **rx_pkts,
	enum ecpri_dma_notify_mode expected_mode)
{
	int ret = 0;
	int actual_num = 0;
	struct ecpri_dma_eth_client_connection *connection;
	u32 buffer_size_delta = ECPRI_DMA_ETH_CLIENT_UT_TEST_LARGE_BUFFER_SIZE-
		ECPRI_DMA_ETH_CLIENT_UT_TEST_BUFF_SIZE;

	/* Retrieve Connection */
	connection = ecpri_dma_eth_dp_test_util_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMA_UT_LOG("Test failed due to invalid connection or connection"
			   " handle:%x, ENDP ID:%d\n", hdl,
			connection->rx_endp_ctx->endp_id);
		return -EFAULT;
	}

	ret = wait_for_completion_timeout(
		&eth_client_test_suite_ctx.
		irq_received[ECPRI_DMA_ENDP_DIR_DEST],
		msecs_to_jiffies(100));
	if (ret == 0) {
		DMA_UT_LOG("Test failed due to Rx timeout");
		return -EFAULT;
	}

	/* Check if state changed */
	if (connection->rx_notify_mode != ECPRI_DMA_NOTIFY_MODE_POLL) {
		DMA_UT_LOG("Test failed due to set to POLL mode, "
			"the mode hasn't been changed, "
			"current mode: %d\n",
			connection->rx_notify_mode);
		return -EFAULT;
	}

	ret = ecpri_dma_eth_rx_poll(hdl, ECPRI_DMA_ETH_CLIENT_UT_TEST_RX_BUDGET,
		rx_pkts, &actual_num);
	if (ret || actual_num != 2) {
		DMA_UT_LOG(
			"Test failed to perform Rx poll, expecting actual num = 2, got %d",
			actual_num);
		return -EFAULT;
	}

	/* Check that the first buffer is full */
	if (rx_pkts[0]->pkt->buffs[0]->size !=
		ECPRI_DMA_ETH_CLIENT_UT_TEST_BUFF_SIZE) {
		DMA_UT_LOG(
			"Test failed due to wrong size of buffers");
		return -EFAULT;
	}

	/* Check completion state */
	if (rx_pkts[0]->comp_code !=
		ECPRI_DMA_COMPLETION_CODE_OVERFLOW) {
		DMA_UT_LOG(
			"Test failed due to "
			"wrong completion code,"
			", on the first (full) buffer."
			" Expected: %d, currnet: %d\n",
			ECPRI_DMA_COMPLETION_CODE_OVERFLOW,
			rx_pkts[0]->comp_code);
		return -EFAULT;
	}

	/* Check that the second part buffer is delta size */
	if (rx_pkts[1]->pkt->buffs[0]->size !=
		buffer_size_delta) {
		DMA_UT_LOG(
			"Test failed due to "
			"wrong size of overflowed buffers");
		return -EFAULT;
	}

	if (rx_pkts[1]->comp_code !=
		ECPRI_DMA_COMPLETION_CODE_EOT) {
		DMA_UT_LOG(
			"Test failed due to "
			"wrong completion code,"
			", on the second (part) buffer."
			" Expected: %d, currnet: %d\n",
			ECPRI_DMA_COMPLETION_CODE_OVERFLOW,
			rx_pkts[1]->comp_code);
		return -EFAULT;
	}

	if (memcmp(tx_pkts[0]->
		buffs[0]->virt_base,
		rx_pkts[0]->pkt->buffs[0]->virt_base,
		rx_pkts[0]->pkt->buffs[0]->size)) {
		DMA_UT_LOG(
			"Test failed due to buffers don't match");
		return -EFAULT;
	}

	if (memcmp((char*)tx_pkts[0]->
		buffs[0]->virt_base +
		rx_pkts[0]->pkt->buffs[0]->size,
		rx_pkts[1]->pkt->buffs[0]->virt_base,
		rx_pkts[1]->pkt->buffs[0]->size)) {
		DMA_UT_LOG(
			"Test failed due to buffers don't match");
		return -EFAULT;
	}

	ret = ecpri_dma_eth_rx_mode_set(hdl, ECPRI_DMA_NOTIFY_MODE_IRQ);
	if (ret != 0) {
		DMA_UT_LOG("Test failed due to set to IRQ mode\n");
		return -EFAULT;
	}

	if (connection->rx_notify_mode != ECPRI_DMA_NOTIFY_MODE_IRQ) {
		DMA_UT_LOG("Test failed due to set to POLL mode, "
			   "the mode hasn't been changed, "
			   "current mode: %d\n",
			connection->rx_notify_mode);
		return -EFAULT;
	}

	return 0;
}

/**
 * ecpri_dma_eth_dp_test_util_register() - mock ETH callbacks and register
 * provides DMA driver with ETH client data
 */
static int ecpri_dma_eth_dp_test_util_register(bool *is_dma_ready,
	struct ecpri_dma_eth_register_params *ready_info) {

	ready_info->notify_ready = &ecpri_dma_eth_dp_test_ready_cb;
	ready_info->userdata_ready =
		&eth_client_test_suite_ctx.ready_cb_user_data;

	ready_info->notify_rx_comp = &ecpri_dma_eth_dp_test_rx_comp_cb;
	ready_info->userdata_rx =
		&eth_client_test_suite_ctx.rx_cb_user_data;

	ready_info->notify_tx_comp = &ecpri_dma_eth_dp_test_tx_comp_cb;
	ready_info->userdata_tx =
		&eth_client_test_suite_ctx.tx_cb_user_data;

	return ecpri_dma_eth_register(ready_info, is_dma_ready);
}

/**
 * ecpri_dma_eth_dp_test_util_is_dma_ready() - checks if the DMA driver is ready and
 * notification is received
 */
static int ecpri_dma_eth_dp_test_util_is_dma_ready(bool is_dma_ready) {
	int ret = 0;
	if (is_dma_ready == false) {
		ret = wait_for_completion_timeout(
			&eth_client_test_suite_ctx.
			ready_received,
			msecs_to_jiffies(
				ECPRI_DMA_ETH_CLIENT_UT_READY_RCVD_TMOUT_MS));
		if (ret != 0) {
			DMA_UT_LOG("Test failed due to DMA driver timeout\n");
			return -EFAULT;
		}

		if (ecpri_dma_eth_client_ctx->is_eth_notified_ready == false) {
			DMA_UT_LOG("Test failed due to"
				   " DMA driver eth notified is not ready\n");
			return -EFAULT;
		}
	}
	return ret;
}

/**
 * ecpri_dma_eth_dp_test_util_connect() - performs endpoints connections
 */
static int ecpri_dma_eth_dp_test_util_connect(
	struct ecpri_dma_eth_endpoint_connect_params *connect_params)
{
	/* Mock ETH to attempt connect pipes. */
	connect_params->link_index = ECPRI_DMA_ETH_CLIENT_UT_LINK_INDEX;
	connect_params->tx_ring_length = ECPRI_DMA_ETH_CLIENT_UT_RING_LENGTH;
	connect_params->rx_ring_length = ECPRI_DMA_ETH_CLIENT_UT_RING_LENGTH;
	connect_params->p_type = ECPRI_DMA_ENDP_STREAM_DEST_FH;
	connect_params->tx_mod_cfg.moderation_counter_threshold =
		ECPRI_DMA_ETH_CLIENT_UT_MODER_CNTR_THRSHLD;
	connect_params->tx_mod_cfg.moderation_timer_threshold =
		ECPRI_DMA_ETH_CLIENT_UT_MODER_TIMER_THRSHLD;


	return ecpri_dma_eth_connect_endpoints(connect_params,
		&eth_client_test_suite_ctx.hdl);
}

/**
 * ecpri_dma_eth_dp_test_util_rx_replenish() - Replenish empty buffers.
 *
 * @num_to_replenish - amount of buffers to replenish.
 */
int ecpri_dma_eth_dp_test_util_rx_replenish(u32 num_to_replenish)
{
	int ret = 0;
	int rem_to_repelnish = num_to_replenish;
	int first_batch = 0;
	u32 first_idx = eth_client_test_suite_ctx.rx_pkt_idx;
	u32* i = &eth_client_test_suite_ctx.rx_pkt_idx;
	ecpri_dma_eth_conn_hdl_t hdl = eth_client_test_suite_ctx.hdl;

	eth_client_test_suite_ctx.allocated_pkts = true;

	while (rem_to_repelnish) {
		eth_client_test_suite_ctx.rx_pkts[*i] =
			kzalloc(sizeof(*eth_client_test_suite_ctx.rx_pkts[*i]),
				GFP_KERNEL);
		if (!eth_client_test_suite_ctx.rx_pkts[*i]) {
			DMA_UT_LOG("failed to alloc packet\n");
			ret = -ENOMEM;
			goto fail_alloc;
		}

		eth_client_test_suite_ctx.rx_pkts[*i]->buffs =
			kzalloc(sizeof(*eth_client_test_suite_ctx.rx_pkts[*i]->buffs),
				GFP_KERNEL);
		if (!eth_client_test_suite_ctx.rx_pkts[*i]->buffs) {
			DMA_UT_LOG("failed to alloc dma buff wrapper\n");
			kfree(eth_client_test_suite_ctx.rx_pkts[*i]);
			ret = -ENOMEM;
			goto fail_alloc;
		}

		eth_client_test_suite_ctx.rx_pkts[*i]->buffs[0] =
			kzalloc(sizeof(*eth_client_test_suite_ctx.rx_pkts[*i]->buffs[0]),
				GFP_KERNEL);
		if (!eth_client_test_suite_ctx.rx_pkts[*i]->buffs[0]) {
			DMA_UT_LOG("failed to alloc dma buff wrapper\n");
			kfree(eth_client_test_suite_ctx.rx_pkts[*i]->buffs);
			kfree(eth_client_test_suite_ctx.rx_pkts[*i]);
			ret = -ENOMEM;
			goto fail_alloc;
		}
		eth_client_test_suite_ctx.rx_pkts[*i]->num_of_buffers = 1;

		eth_client_test_suite_ctx.rx_pkts[*i]->buffs[0]->virt_base =
			kzalloc(ECPRI_DMA_ETH_CLIENT_UT_TEST_BUFF_SIZE,
				GFP_KERNEL);
		if (!eth_client_test_suite_ctx.rx_pkts[*i]->buffs[0]->virt_base) {
			DMA_UT_LOG("failed to alloc buffer\n");
			kfree(eth_client_test_suite_ctx.rx_pkts[*i]->buffs[0]);
			kfree(eth_client_test_suite_ctx.rx_pkts[*i]->buffs);
			kfree(eth_client_test_suite_ctx.rx_pkts[*i]);
			ret = -ENOMEM;
			goto fail_alloc;
		}

		eth_client_test_suite_ctx.rx_pkts[*i]->buffs[0]->size =
			ECPRI_DMA_ETH_CLIENT_UT_TEST_BUFF_SIZE;

		*i = (*i + 1) % (ECPRI_DMA_ETH_CLIENT_UT_NUM_OF_BUFFS_IN_RING);
		rem_to_repelnish--;
	}

	if (first_idx + num_to_replenish >
		ECPRI_DMA_ETH_CLIENT_UT_NUM_OF_BUFFS_IN_RING)
	{
		/* Handle array wrap around */
		first_batch = ECPRI_DMA_ETH_CLIENT_UT_NUM_OF_BUFFS_IN_RING -
			first_idx;
		rem_to_repelnish = num_to_replenish - first_batch;
		ret = ecpri_dma_eth_replenish_buffers(hdl,
			&eth_client_test_suite_ctx.rx_pkts[first_idx],
			first_batch, true);
		if (ret) {
			DMA_UT_LOG("failed to replenish Rx endp\n");
			goto fail_alloc;
		}
		ret = ecpri_dma_eth_replenish_buffers(hdl,
			&eth_client_test_suite_ctx.rx_pkts[0],
			rem_to_repelnish, true);
		if (ret) {
			DMA_UT_LOG("failed to replenish Rx endp\n");
			goto fail_alloc;
		}
	}
	else
	{
		/* No need to handle wrap around*/
		ret = ecpri_dma_eth_replenish_buffers(hdl,
			&eth_client_test_suite_ctx.rx_pkts[first_idx],
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
		*i = (*i - 1) % ECPRI_DMA_ETH_CLIENT_UT_NUM_OF_BUFFS_IN_RING;
		kfree(eth_client_test_suite_ctx.rx_pkts[*i]->buffs[0]->virt_base);
		kfree(eth_client_test_suite_ctx.rx_pkts[*i]->buffs[0]);
		kfree(eth_client_test_suite_ctx.rx_pkts[*i]->buffs);
		kfree(eth_client_test_suite_ctx.rx_pkts[*i]);
		rem_to_repelnish++;
	}

	return ret;
}

/**
 * ecpri_dma_eth_dp_test_util_init() - Performs all the necessary preparations,
 * configurations, and registration before executing the core test part.
 */
static int ecpri_dma_eth_dp_test_util_init()
{
	int ret = 0;
	bool is_dma_ready = false;
	struct ecpri_dma_eth_register_params ready_info;
	struct ecpri_dma_eth_endpoint_connect_params connect_params;

	reinit_completion(&eth_client_test_suite_ctx.
		irq_received[ECPRI_DMA_ENDP_DIR_SRC]);

	reinit_completion(&eth_client_test_suite_ctx.
		irq_received[ECPRI_DMA_ENDP_DIR_DEST]);

	ret = ecpri_dma_eth_dp_test_util_register(&is_dma_ready, &ready_info);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed on register");
		return -EFAULT;
	}

	/* Wait for DMA driver to be ready */
	ret = ecpri_dma_eth_dp_test_util_is_dma_ready(is_dma_ready);
	if (ret != 0) {
		DMA_UT_LOG("Failed on is_dma_ready wait\n");
		return -EFAULT;
	}

	/* Connect endpoints */
	ret = ecpri_dma_eth_dp_test_util_connect(&connect_params);
	if (ret != 0) {
		DMA_UT_LOG("Test failed due to DMA driver timeout\n");
		return -EFAULT;
	}

	/* Retrieve connection and verify its validity */
	eth_client_test_suite_ctx.connection =
		ecpri_dma_eth_dp_test_util_get_conn_from_hdl(
		eth_client_test_suite_ctx.hdl);
	if (!eth_client_test_suite_ctx.connection ||
		!eth_client_test_suite_ctx.connection->valid) {
		DMA_UT_LOG("Test failed due to invalid connection or connection"
			" handle:%x\n", eth_client_test_suite_ctx.hdl);
		return -EFAULT;
	}

	/* Start endpoints */
	ret = ecpri_dma_eth_start_endpoints(eth_client_test_suite_ctx.hdl);
	if (ret != 0) {
		DMA_UT_LOG("Test failed due to Start Endpoint failure\n");
		return -EFAULT;
	}

	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		ret = ecpri_dma_eth_dp_test_util_rx_replenish(
			ECPRI_DMA_ETH_CLIENT_UT_NUM_OF_BUFFS_IN_RING);
		if (ret != 0) {
			DMA_UT_LOG("Test failed due toreplenish buffers failure\n");
			return -EFAULT;
		}
	}

	return ret;
}

/**
 * ecpri_dma_eth_dp_test_util_clean_up() - Before exiting the test, cleanse all the
 * allocated for the test data
 *
 * @hdl - the unique connection handle
 * @num_of_pkts_to_send - number of packets to create
 * @tx_pkts - pointer to packets to transmit
 * @rx_pkts - pointer to array of received packets data
 */
static int ecpri_dma_eth_dp_test_util_clean_up(ecpri_dma_eth_conn_hdl_t hdl,
	int num_of_pkts_to_send, bool single_buffer, struct ecpri_dma_pkt **tx_pkts,
	struct ecpri_dma_pkt_completion_wrapper **rx_pkts)
{
	int ret = 0;
	ret = ecpri_dma_eth_stop_endpoints(hdl);
	if (ret != 0) {
		DMA_UT_LOG("Test failed due to Stop Endpoint failure\n");
		return -EFAULT;
	}

	ecpri_dma_eth_dp_test_util_destroy_test_data(num_of_pkts_to_send,
		single_buffer, tx_pkts, rx_pkts);

	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		ret = ecpri_dma_eth_disconnect_endpoints(hdl);
		if (ret != 0) {
			DMA_UT_TEST_FAIL_REPORT("Failed on disconnect\n");
			return -EFAULT;
		}
	}
	else {
		ret = ecpri_dma_eth_dp_test_disconnect_endpoints(hdl);
		if (ret != 0) {
			DMA_UT_TEST_FAIL_REPORT("Failed on disconnect\n");
			return -EFAULT;
		}
	}

	eth_client_test_suite_ctx.hdl = 0;
	eth_client_test_suite_ctx.connection = NULL;

	/* Deregister and check */
	ecpri_dma_eth_deregister();
	return ret;
}

/* Tests section */
static int ecpri_dma_eth_dp_test_suite_setup(void **ppriv)
{
	int ret = 0;
	DMA_UT_DBG("Start Setup\n");

	memset(&eth_client_test_suite_ctx, 0,
		sizeof(eth_client_test_suite_ctx));

	init_completion(
		&eth_client_test_suite_ctx.
		irq_received[ECPRI_DMA_ENDP_DIR_SRC]);
	init_completion(
		&eth_client_test_suite_ctx.
		irq_received[ECPRI_DMA_ENDP_DIR_DEST]);
	init_completion(&eth_client_test_suite_ctx.ready_received);

	eth_client_test_suite_ctx.ready_cb_user_data =
		ECPRI_DMA_ETH_CLIENT_UT_READY_USER_DATA_VAL;

	eth_client_test_suite_ctx.rx_cb_user_data =
		ECPRI_DMA_ETH_CLIENT_UT_RX_USER_DATA_VAL;

	eth_client_test_suite_ctx.tx_cb_user_data =
		ECPRI_DMA_ETH_CLIENT_UT_TX_USER_DATA_VAL;

	ret = ecpri_dma_eth_dp_test_util_setup_dma_endps(
		ECPRI_DMA_ENDP_DIR_SRC, true);
	if (ret)
		return ret;

	ret = ecpri_dma_eth_dp_test_util_setup_dma_endps(
		ECPRI_DMA_ENDP_DIR_DEST, true);
	if (ret)
		return ret;

	return 0;
}

static int ecpri_dma_eth_dp_test_suite_teardown(void *priv)
{
	int ret = 0;
	int i = 0;
	DMA_UT_DBG("Start Teardown\n");

	/* Once endps are stopped, remove loopback config */
	ret = ecpri_dma_eth_dp_test_util_setup_dma_endps(
		ECPRI_DMA_ETH_CLIENT_UT_SRC_ENDP_ID, false);
	if (ret)
		return ret;

	ret = ecpri_dma_eth_dp_test_util_setup_dma_endps(
		ECPRI_DMA_ETH_CLIENT_UT_DEST_ENDP_ID, false);
	if (ret)
		return ret;

	if (eth_client_test_suite_ctx.allocated_pkts)
	{
		if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
			for (i = 0; i < ECPRI_DMA_ETH_CLIENT_UT_NUM_OF_BUFFS_IN_RING; i++) {
				kfree(eth_client_test_suite_ctx.rx_pkts[i]->buffs[0]->virt_base);
				kfree(eth_client_test_suite_ctx.rx_pkts[i]->buffs[0]);
				kfree(eth_client_test_suite_ctx.rx_pkts[i]->buffs);
				kfree(eth_client_test_suite_ctx.rx_pkts[i]);
			}
		}
		eth_client_test_suite_ctx.rx_pkt_idx = 0;
	}
	return 0;
}

static int ecpri_dma_eth_dp_test_suite_registration(void *priv) {
	int ret = 0;
	bool is_dma_ready = false;
	struct ecpri_dma_eth_register_params ready_info;

	DMA_UT_DBG("Start Handshake\n");

	/* Register */
	ret = ecpri_dma_eth_dp_test_util_register(&is_dma_ready, &ready_info);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed on register");
		return -EFAULT;
	}

	/* Wait for DMA driver to be ready */
	ret = ecpri_dma_eth_dp_test_util_is_dma_ready(is_dma_ready);
	if (ret != 0) {
		DMA_UT_LOG("Failed on is_dma_ready wait\n");
		return -EFAULT;
	}

	/* Check DMA driver state matches the expectations */
	if (ecpri_dma_eth_client_ctx->ready_cb
	    != ready_info.notify_ready) {
		DMA_UT_LOG("Test failed due to "
			   "mismatch of ready cb address\n");
		return -EFAULT;
	}

	if (ecpri_dma_eth_client_ctx->ready_cb
		!= ready_info.notify_ready) {
		DMA_UT_LOG("Test failed due to "
			   "ismatch of ready cb address\n");
		return -EFAULT;
	}

	if (ecpri_dma_eth_client_ctx->rx_comp_cb
		!= ready_info.notify_rx_comp) {
		DMA_UT_LOG("Test failed due to "
			   "mismatch of Rx comp cb address\n");
		return -EFAULT;
	}

	if (ecpri_dma_eth_client_ctx->tx_comp_cb
		!= ready_info.notify_tx_comp) {
		DMA_UT_LOG("Test failed due to "
			   "mismatch of tx comp cb address\n");
		return -EFAULT;
	}

	/* Deregister and check */
	ecpri_dma_eth_deregister();

	if (ecpri_dma_eth_client_ctx->ready_cb != NULL) {
		DMA_UT_LOG("Test failed due to "
			   "failure on de-registration of ready_cb\n");
		return -EFAULT;
	}

	if (ecpri_dma_eth_client_ctx->ready_cb_user_data != NULL) {
		DMA_UT_LOG("Test failed due to "
			   "failure on de-registration of "
			   "ready_cb_user_data\n");
		return -EFAULT;
	}

	if (ecpri_dma_eth_client_ctx->rx_comp_cb != NULL) {
		DMA_UT_LOG("Test failed due to "
			   "failure on de-registration of rx_comp_cb\n");
		return -EFAULT;
	}

	if (ecpri_dma_eth_client_ctx->rx_comp_cb_user_data != NULL) {
		DMA_UT_LOG("Test failed due to "
			   "failure on de-registration of"
			   " rx_comp_cb_user_data\n");
		return -EFAULT;
	}

	if (ecpri_dma_eth_client_ctx->tx_comp_cb != NULL) {
		DMA_UT_LOG("Test failed due to "
			   "failure on de-registration of tx_comp_cb\n");
		return -EFAULT;
	}

	if (ecpri_dma_eth_client_ctx->tx_comp_cb_user_data != NULL) {
		DMA_UT_LOG("Test failed due to "
			   "failure on de-registration of"
			   " tx_comp_cb_user_data\n");
		return -EFAULT;
	}

	return ret;
}

static int ecpri_dma_eth_dp_test_suite_connect(void *priv) {
	int ret = 0;
	bool is_dma_ready = false;
	ecpri_hwio_def_ecpri_endp_gsi_cfg_n_u endp_gsi_cfg = { 0 };
	struct ecpri_dma_eth_client_connection *connection;
	struct ecpri_dma_eth_endpoint_connect_params connect_params;
	struct ecpri_dma_eth_register_params ready_info;

	DMA_UT_DBG("Start Connect\n");

	/* Register */
	ret = ecpri_dma_eth_dp_test_util_register(&is_dma_ready, &ready_info);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed on register");
		return -EFAULT;
	}

	/* Wait for DMA driver to be ready */
	ret = ecpri_dma_eth_dp_test_util_is_dma_ready(is_dma_ready);
	if (ret != 0) {
		DMA_UT_LOG("Failed on is_dma_ready wait\n");
		return -EFAULT;
	}

	/* Mock ETH to attempt connect pipes. */
	ret = ecpri_dma_eth_dp_test_util_connect(&connect_params);
	if (ret != 0) {
		DMA_UT_LOG("Test failed due to DMA driver fail to connect pipes\n");
		return -EFAULT;
	}

	/* Retrieve Connection */
	connection = &ecpri_dma_eth_client_ctx->
		connections[connect_params.link_index];
	if (!connection->valid) {
		DMA_UT_LOG("Connection is not connected, link_index: %d\n",
			connect_params.link_index);
		return -EFAULT;
	}

	if (connection->hdl != eth_client_test_suite_ctx.hdl ||
		connection->tx_endp_ctx->hdl != eth_client_test_suite_ctx.hdl
	    || connection->rx_endp_ctx->hdl != eth_client_test_suite_ctx.hdl) {
		DMA_UT_LOG("Test failed due to "
			   "hdl mismatch\n");
		return -EFAULT;
	}

	if (connection->p_type != connect_params.p_type) {
		DMA_UT_LOG("Test failed due to "
			   "port mismatch\n");
		return -EFAULT;
	}

	if (connection->link_idx != connect_params.link_index) {
		DMA_UT_LOG("Test failed due to "
			   "link index mismatch\n");
		return -EFAULT;
	}

	if (connection->tx_mod_cfg.moderation_counter_threshold
	    != connect_params.tx_mod_cfg.moderation_counter_threshold) {
		DMA_UT_LOG("Test failed due to "
			   "Tx Mod Configuration - Counter Threshold "
			   "mismatch\n");
		return -EFAULT;
	}

	if (connection->tx_mod_cfg.moderation_timer_threshold
	    != connect_params.tx_mod_cfg.moderation_timer_threshold) {
		DMA_UT_LOG("Test failed due to "
			   "Tx Mod Configuration - Timer Threshold "
			   "mismatch\n");
		return -EFAULT;
	}

	endp_gsi_cfg.value = ecpri_dma_hal_read_reg_n(
		ECPRI_ENDP_GSI_CFG_n, connection->tx_endp_ctx->endp_id);
	if (endp_gsi_cfg.def.endp_en != 1) {
		DMA_UT_LOG("Test failed due to "
			   "Tx endpoint was not enabled\n");
		return -EFAULT;
	}

	endp_gsi_cfg.value = ecpri_dma_hal_read_reg_n(
		ECPRI_ENDP_GSI_CFG_n, connection->rx_endp_ctx->endp_id);
	if (endp_gsi_cfg.def.endp_en != 1) {
		DMA_UT_LOG("Test failed due to "
			   "Rx endpoint was not enabled\n");
		return -EFAULT;
	}

	/* Disconnect and check */
	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		ret = ecpri_dma_eth_disconnect_endpoints(
			eth_client_test_suite_ctx.hdl);
		if (ret != 0) {
			DMA_UT_TEST_FAIL_REPORT("Failed on disconnect\n");
			return -EFAULT;
		}
	}
	else {
		ret = ecpri_dma_eth_dp_test_disconnect_endpoints(
			eth_client_test_suite_ctx.hdl);
		if (ret != 0) {
			DMA_UT_TEST_FAIL_REPORT("Failed on disconnect\n");
			return -EFAULT;
		}
	}

	if (connection->valid) {
		DMA_UT_LOG("Connection still connected,"
			   "after disconnect, link_index: %d\n",
			connect_params.link_index);
		return -EFAULT;
	}

	if (connection->hdl !=0) {
		DMA_UT_LOG("Test failed due to "
			   "hdl was not cleared\n");
		return -EFAULT;
	}

	if (connection->p_type != 0) {
		DMA_UT_LOG("Test failed due to "
			   "port was not cleared\n");
		return -EFAULT;
	}

	if (connection->link_idx != 0) {
		DMA_UT_LOG("Test failed due to "
			   "link index was not cleared\n");
		return -EFAULT;
	}

	if (connection->tx_mod_cfg.moderation_counter_threshold != 0) {
		DMA_UT_LOG("Test failed due to "
			   "Tx Mod Configuration - Counter Threshold "
			   "was not cleared\n");
		return -EFAULT;
	}

	if (connection->tx_mod_cfg.moderation_timer_threshold != 0) {
		DMA_UT_LOG("Test failed due to "
			   "Tx Mod Configuration - Timer Threshold "
			   "was not cleared\n");
		return -EFAULT;
	}

	endp_gsi_cfg.value = ecpri_dma_hal_read_reg_n(
		ECPRI_ENDP_GSI_CFG_n, connection->tx_endp_ctx->endp_id);
	if (endp_gsi_cfg.def.endp_en != 0) {
		DMA_UT_LOG("Test failed due to "
			   "Tx endpoint was not disabled\n");
		return -EFAULT;
	}

	endp_gsi_cfg.value = ecpri_dma_hal_read_reg_n(
		ECPRI_ENDP_GSI_CFG_n, connection->rx_endp_ctx->endp_id);
	if (endp_gsi_cfg.def.endp_en != 0) {
		DMA_UT_LOG("Test failed due to "
			   "Rx endpoint was not disabled\n");
		return -EFAULT;
	}

	/* Deregister and check */
	ecpri_dma_eth_deregister();

	return ret;
}

static int ecpri_dma_eth_dp_test_suite_single_pkt_single_buffer(void *priv) {
	int ret = 0;
	int num_of_pkts_to_send = 1;
	int num_to_repelnish = 0;
	struct ecpri_dma_pkt **tx_pkts;
	struct ecpri_dma_pkt_completion_wrapper **rx_pkts;

	DMA_UT_DBG("Start Loopback Single packet\n");

	ret = ecpri_dma_eth_dp_test_util_init();
	if (ret != 0) {
		DMA_UT_LOG("Failed to initialize the test\n");
		return ret;
	}

	ret = ecpri_dma_eth_dp_test_util_prepare_test_data(num_of_pkts_to_send,
		true, &tx_pkts, &rx_pkts);
	if (ret != 0) {
		DMA_UT_LOG("Failed to prepare test data\n");
		return ret;
	}

	/* Prepare exact amount of credits equal to amount of buffers sent */
	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		ret = ecpri_dma_eth_dp_test_suite_calculate_credits(0, tx_pkts,
			num_of_pkts_to_send, &num_to_repelnish);
		if (ret) {
			DMA_UT_LOG("Failed to calculate credits\n");
			return ret;
		}

		ret = ecpri_dma_eth_dp_test_util_rx_replenish(num_to_repelnish);
		if (ret != 0) {
			DMA_UT_LOG("Test failed due to replenish buffers failure\n");
			return -EFAULT;
		}
	}

	/* Transmit single packet */
	ret = ecpri_dma_eth_transmit(eth_client_test_suite_ctx.hdl,
		tx_pkts, num_of_pkts_to_send, true);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed on transmit");
		return -EFAULT;
	}

	/* Verify single packet */
	ret = ecpri_dma_eth_dp_test_util_wait_for_tx_comp(num_of_pkts_to_send);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx timeout");
		return -EFAULT;
	}

	ret = ecpri_dma_eth_dp_test_util_verify_rx(eth_client_test_suite_ctx.hdl,
		num_of_pkts_to_send,
		tx_pkts, rx_pkts, ECPRI_DMA_NOTIFY_MODE_IRQ, ECPRI_DMA_NOTIFY_MODE_IRQ,
		&num_to_repelnish);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx fail");
		return -EFAULT;
	}

	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		ret = ecpri_dma_eth_dp_test_util_rx_replenish(num_to_repelnish);
		if (ret != 0) {
			DMA_UT_LOG("Test failed due to replenish buffers failure\n");
			return -EFAULT;
		}
	}

	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		ret = ecpri_dma_test_eth_dp_test_suite_verify_read_write_ptr(
			eth_client_test_suite_ctx.connection->rx_endp_ctx);
		if (ret) {
			DMA_UT_LOG("RP/WP mismatch\n");
			return -EFAULT;
		}
	}

	/* Test clean-up */
	ret = ecpri_dma_eth_dp_test_util_clean_up(eth_client_test_suite_ctx.hdl,
		num_of_pkts_to_send, true, tx_pkts,
		rx_pkts);
	if (ret != 0) {
		DMA_UT_LOG("Failed to clean the test\n");
		return ret;
	}

	return ret;
}

static int ecpri_dma_eth_dp_test_suite_mult_pkt_single_buffer(void *priv) {
	int ret = 0;
	struct ecpri_dma_pkt **tx_pkts;
	struct ecpri_dma_pkt_completion_wrapper **rx_pkts;
	int num_to_repelnish = 0;
	int num_of_pkts_to_send = ECPRI_DMA_ETH_CLIENT_UT_NUM_OF_BUFFS_IN_RING;

	DMA_UT_DBG("Start Loopback Multiple packets\n");

	ret = ecpri_dma_eth_dp_test_util_init();
	if (ret != 0) {
		DMA_UT_LOG("Failed to initialize the test\n");
		return ret;
	}

	ret = ecpri_dma_eth_dp_test_util_prepare_test_data(
		num_of_pkts_to_send, true, &tx_pkts, &rx_pkts);
	if (ret != 0) {
		DMA_UT_LOG("Failed to prepare test data\n");
		return ret;
	}

	/* Prepare exact amount of credits equal to amount of buffers sent */
	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		ret = ecpri_dma_eth_dp_test_suite_calculate_credits(0, tx_pkts,
			num_of_pkts_to_send, &num_to_repelnish);
		if (ret) {
			DMA_UT_LOG("Failed to calculate credits\n");
			return ret;
		}

		ret = ecpri_dma_eth_dp_test_util_rx_replenish(num_to_repelnish);
		if (ret != 0) {
			DMA_UT_LOG("Test failed due to replenish buffers failure\n");
			return -EFAULT;
		}
	}

	/* Transmit multiple packet */
	ret = ecpri_dma_eth_transmit(eth_client_test_suite_ctx.hdl,
		tx_pkts, num_of_pkts_to_send, true);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed on transmit");
		return -EFAULT;
	}

	ret = ecpri_dma_eth_dp_test_util_wait_for_tx_comp(num_of_pkts_to_send);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx timeout");
		return -EFAULT;
	}

	ret = ecpri_dma_eth_dp_test_util_verify_rx(eth_client_test_suite_ctx.hdl,
		num_of_pkts_to_send, tx_pkts, rx_pkts,
		ECPRI_DMA_NOTIFY_MODE_IRQ, ECPRI_DMA_NOTIFY_MODE_IRQ,
		&num_to_repelnish);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx fail");
		return -EFAULT;
	}

	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		ret = ecpri_dma_eth_dp_test_util_rx_replenish(num_of_pkts_to_send);
		if (ret != 0) {
			DMA_UT_LOG("Test failed due to replenish buffers failure\n");
			return -EFAULT;
		}
	}

	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		ret = ecpri_dma_test_eth_dp_test_suite_verify_read_write_ptr(
			eth_client_test_suite_ctx.connection->rx_endp_ctx);
		if (ret) {
			DMA_UT_LOG("RP/WP mismatch\n");
			return -EFAULT;
		}
	}

	/* Test clean-up */
	ret = ecpri_dma_eth_dp_test_util_clean_up(eth_client_test_suite_ctx.hdl,
		num_of_pkts_to_send, true, tx_pkts,
		rx_pkts);
	if (ret != 0) {
		DMA_UT_LOG("Failed to clean the test\n");
		return ret;
	}

	return ret;
}

static int ecpri_dma_eth_dp_test_suite_mode_change(void *priv) {
	int ret = 0;
	struct ecpri_dma_pkt **tx_pkts;
	int num_to_repelnish = 0;
	int num_of_pkts_to_send = 1;
	struct ecpri_dma_pkt_completion_wrapper **rx_pkts;

	DMA_UT_DBG("Start Loopback Multiple packets\n");

	ret = ecpri_dma_eth_dp_test_util_init();
	if (ret != 0) {
		DMA_UT_LOG("Failed to initialize the test\n");
		return ret;
	}

	ret = ecpri_dma_eth_dp_test_util_prepare_test_data(
		num_of_pkts_to_send, true, &tx_pkts, &rx_pkts);
	if (ret != 0) {
		DMA_UT_LOG("Failed to prepare test data\n");
		return ret;
	}

	/* Prepare exact amount of credits equal to amount of buffers sent */
	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		ret = ecpri_dma_eth_dp_test_suite_calculate_credits(0, tx_pkts,
			num_of_pkts_to_send, &num_to_repelnish);
		if (ret) {
			DMA_UT_LOG("Failed to calculate credits\n");
			return ret;
		}

		ret = ecpri_dma_eth_dp_test_util_rx_replenish(num_to_repelnish);
		if (ret != 0) {
			DMA_UT_LOG("Test failed due to replenish buffers failure\n");
			return -EFAULT;
		}
	}

	/* Transmit a packet */
	ret = ecpri_dma_eth_transmit(eth_client_test_suite_ctx.hdl,
		tx_pkts, num_of_pkts_to_send, true);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed on transmit");
		return -EFAULT;
	}

	ret = ecpri_dma_eth_dp_test_util_wait_for_tx_comp(num_of_pkts_to_send);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx timeout");
		return -EFAULT;
	}

	ret = ecpri_dma_eth_dp_test_util_verify_rx(eth_client_test_suite_ctx.hdl,
		num_of_pkts_to_send, tx_pkts,
		rx_pkts, ECPRI_DMA_NOTIFY_MODE_IRQ, ECPRI_DMA_NOTIFY_MODE_POLL,
		&num_to_repelnish);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed to verify Rx");
		return -EFAULT;
	}

	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		ret = ecpri_dma_eth_dp_test_util_rx_replenish(num_of_pkts_to_send);
		if (ret != 0) {
			DMA_UT_LOG("Test failed due to replenish buffers failure\n");
			return -EFAULT;
		}
	}

	ret = ecpri_dma_eth_dp_test_util_prepare_test_data(
		num_of_pkts_to_send, true, &tx_pkts, &rx_pkts);
	if (ret != 0) {
		DMA_UT_LOG("Failed to prepare test data\n");
		return ret;
	}

	/* Prepare exact amount of credits equal to amount of buffers sent */
	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		ret = ecpri_dma_eth_dp_test_suite_calculate_credits(0, tx_pkts,
			num_of_pkts_to_send, &num_to_repelnish);
		if (ret) {
			DMA_UT_LOG("Failed to calculate credits\n");
			return ret;
		}

		ret = ecpri_dma_eth_dp_test_util_rx_replenish(num_to_repelnish);
		if (ret != 0) {
			DMA_UT_LOG("Test failed due to replenish buffers failure\n");
			return -EFAULT;
		}
	}

	/* Transmit again */
	ret = ecpri_dma_eth_transmit(eth_client_test_suite_ctx.hdl,
		tx_pkts, num_of_pkts_to_send, true);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed on transmit");
		return -EFAULT;
	}

	/* Ensure that packet sent */
	ret = ecpri_dma_eth_dp_test_util_wait_for_tx_comp(num_of_pkts_to_send + 1);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx timeout");
		return -EFAULT;
	}

	/* Try to receive a packet and change back to IRQ */
	ret = ecpri_dma_eth_dp_test_util_verify_rx(eth_client_test_suite_ctx.hdl,
		num_of_pkts_to_send, tx_pkts,
		rx_pkts, ECPRI_DMA_NOTIFY_MODE_POLL, ECPRI_DMA_NOTIFY_MODE_IRQ,
		&num_to_repelnish);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx fail");
		return -EFAULT;
	}

	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		ret = ecpri_dma_eth_dp_test_util_rx_replenish(num_of_pkts_to_send);
		if (ret != 0) {
			DMA_UT_LOG("Test failed due to replenish buffers failure\n");
			return -EFAULT;
		}
	}

	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		ret = ecpri_dma_test_eth_dp_test_suite_verify_read_write_ptr(
			eth_client_test_suite_ctx.connection->rx_endp_ctx);
		if (ret) {
			DMA_UT_LOG("RP/WP mismatch\n");
			return -EFAULT;
		}
	}

	/* Test clean-up */
	ret = ecpri_dma_eth_dp_test_util_clean_up(eth_client_test_suite_ctx.hdl,
		num_of_pkts_to_send, true, tx_pkts,
		rx_pkts);
	if (ret != 0) {
		DMA_UT_LOG("Failed to clean the test\n");
		return ret;
	}

	return ret;
}

static int ecpri_dma_eth_dp_test_suite_chains_large_payload(void *priv) {
	int ret = 0;
	int num_to_repelnish = 0;
	int num_of_pkts_to_send = 1;
	struct ecpri_dma_pkt **tx_pkts;
	struct ecpri_dma_pkt_completion_wrapper **rx_pkts;

	DMA_UT_DBG("Start Large Payload\n");

	ret = ecpri_dma_eth_dp_test_util_init();
	if (ret != 0) {
		DMA_UT_LOG("Failed to initialize the test\n");
		return ret;
	}

	ret = ecpri_dma_eth_dp_test_util_prepare_test_large_data(
		&tx_pkts, &rx_pkts);
	if (ret != 0) {
		DMA_UT_LOG("Failed to prepare test data\n");
		return ret;
	}

	/* Prepare exact amount of credits equal to amount of buffers sent */
	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		ret = ecpri_dma_eth_dp_test_suite_calculate_credits(0, tx_pkts,
			num_of_pkts_to_send, &num_to_repelnish);
		if (ret) {
			DMA_UT_LOG("Failed to calculate credits\n");
			return ret;
		}

		ret = ecpri_dma_eth_dp_test_util_rx_replenish(num_to_repelnish);
		if (ret != 0) {
			DMA_UT_LOG("Test failed due to replenish buffers failure\n");
			return -EFAULT;
		}
	}

	ret = ecpri_dma_eth_transmit(eth_client_test_suite_ctx.hdl,
		tx_pkts, num_of_pkts_to_send, true);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed on transmit");
		return -EFAULT;
	}

	/* Verify single packet sent */
	ret = ecpri_dma_eth_dp_test_util_wait_for_tx_comp(num_of_pkts_to_send);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to Tx timeout");
		return -EFAULT;
	}

	/* Verify if the large payload was partitioned and
	   processed correctly */
	ret = ecpri_dma_eth_dp_test_util_verify_rx_large_data(
		eth_client_test_suite_ctx.hdl, num_of_pkts_to_send, tx_pkts, rx_pkts,
		ECPRI_DMA_NOTIFY_MODE_IRQ);

	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		ret = ecpri_dma_eth_dp_test_util_rx_replenish(
			ECPRI_DMA_ETH_CLIENT_UT_TEST_LARGE_BUFFERS_AMOUNT);
		if (ret != 0) {
			DMA_UT_LOG("Test failed due to replenish buffers failure\n");
			return -EFAULT;
		}
	}

	/* Driver state check-up */
	if (eth_client_test_suite_ctx.connection->tx_endp_ctx->curr_outstanding_num
		!= 0 ||
		eth_client_test_suite_ctx.connection->tx_endp_ctx->curr_completed_num
		!= 0) {
		DMA_UT_LOG("Test failed due to wrong driver state\n");
		return -EFAULT;
	}

	/*Single packet sent but 2 rx pkts used so cleanup won't free the second */
	kfree(rx_pkts[1]);

	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		ret = ecpri_dma_test_eth_dp_test_suite_verify_read_write_ptr(
			eth_client_test_suite_ctx.connection->rx_endp_ctx);
		if (ret) {
			DMA_UT_LOG("RP/WP mismatch\n");
			return -EFAULT;
		}
	}

	/* Test clean-up */
	ret = ecpri_dma_eth_dp_test_util_clean_up(eth_client_test_suite_ctx.hdl,
		num_of_pkts_to_send, true, tx_pkts,
		rx_pkts);
	if (ret != 0) {
		DMA_UT_LOG("Failed to clean the test\n");
		return ret;
	}

	return ret;
}

static int ecpri_dma_eth_dp_test_suite_commit(void *priv) {
	int ret = 0;
	int num_of_pkts_to_send = 1;
	int num_to_repelnish = 0;
	struct ecpri_dma_pkt **tx_pkts;
	struct ecpri_dma_pkt_completion_wrapper **rx_pkts;

	DMA_UT_DBG("Start Commit\n");

	ret = ecpri_dma_eth_dp_test_util_init();
	if (ret != 0) {
		DMA_UT_LOG("Failed to initialize the test\n");
		return ret;
	}

	ret = ecpri_dma_eth_dp_test_util_prepare_test_data(
		num_of_pkts_to_send, true, &tx_pkts, &rx_pkts);
	if (ret != 0) {
		DMA_UT_LOG("Failed to prepare test data\n");
		return ret;
	}

	/* Prepare exact amount of credits equal to amount of buffers sent */
	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		ret = ecpri_dma_eth_dp_test_suite_calculate_credits(0, tx_pkts,
			num_of_pkts_to_send, &num_to_repelnish);
		if (ret) {
			DMA_UT_LOG("Failed to calculate credits\n");
			return ret;
		}

		ret = ecpri_dma_eth_dp_test_util_rx_replenish(num_to_repelnish);
		if (ret != 0) {
			DMA_UT_LOG("Test failed due to replenish buffers failure\n");
			return -EFAULT;
		}
	}

	/* Transmit single packet with 'false' commit*/
	ret = ecpri_dma_eth_transmit(eth_client_test_suite_ctx.hdl, tx_pkts,
		num_of_pkts_to_send, false);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed on transmit");
		return -EFAULT;
	}

	/* Verify single packet has not been sent */
	ret = wait_for_completion_timeout(
		&eth_client_test_suite_ctx.
		irq_received[ECPRI_DMA_ENDP_DIR_SRC],
		msecs_to_jiffies(1000));
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed, Tx should timeout");
		return -EFAULT;
	}

	if (eth_client_test_suite_ctx.connection->tx_endp_ctx->
		curr_outstanding_num != 1 ||
		eth_client_test_suite_ctx.connection->tx_endp_ctx->
		curr_completed_num != 0)  {
		DMA_UT_TEST_FAIL_REPORT("Test failed, packet was sent");
		return -EFAULT;
	}

	/* Commit the transmitted packet */
	ret = ecpri_dma_eth_commit(eth_client_test_suite_ctx.hdl);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed on commit");
		return -EFAULT;
	}

	/* Verify single packet sent */
	ret = ecpri_dma_eth_dp_test_util_wait_for_tx_comp(num_of_pkts_to_send);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed, Tx timeout");
		return -EFAULT;
	}

	/* Check if received after manual commit */
	ret = ecpri_dma_eth_dp_test_util_verify_rx(eth_client_test_suite_ctx.hdl,
		num_of_pkts_to_send, tx_pkts, rx_pkts, ECPRI_DMA_NOTIFY_MODE_IRQ,
		ECPRI_DMA_NOTIFY_MODE_IRQ, &num_to_repelnish);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Test failed to received packets "
					"after commit");
		return -EFAULT;
	}

	if (ecpri_dma_get_ctx_hw_ver() != ECPRI_HW_V1_0) {
		ret = ecpri_dma_eth_dp_test_util_rx_replenish(num_of_pkts_to_send);
		if (ret != 0) {
			DMA_UT_LOG("Test failed due to replenish buffers failure\n");
			return -EFAULT;
		}
	}

	/* Driver state check-up */
	if (eth_client_test_suite_ctx.connection->tx_endp_ctx->
		curr_outstanding_num != 0 ||
		eth_client_test_suite_ctx.connection->tx_endp_ctx->
		curr_completed_num != 0) {
		DMA_UT_LOG("Test failed due to wrong driver state\n");
		return -EFAULT;
	}

	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		ret = ecpri_dma_test_eth_dp_test_suite_verify_read_write_ptr(
			eth_client_test_suite_ctx.connection->rx_endp_ctx);
		if (ret) {
			DMA_UT_LOG("RP/WP mismatch\n");
			return -EFAULT;
		}
	}

	/* Test clean-up */
	ret = ecpri_dma_eth_dp_test_util_clean_up(eth_client_test_suite_ctx.hdl,
		num_of_pkts_to_send, true, tx_pkts, rx_pkts);
	if (ret != 0) {
		DMA_UT_LOG("Failed to clean the test\n");
		return ret;
	}

	return ret;
}

/* Suite definition block */
DMA_UT_DEFINE_SUITE_START(eth_dp, "ETH DP suite",
			  ecpri_dma_eth_dp_test_suite_setup,
			  ecpri_dma_eth_dp_test_suite_teardown){
	DMA_UT_ADD_TEST(
		handshake,
		"This test will verify ETH and DMA handshake init by mocking "
		"the ETH driver context.",
		ecpri_dma_eth_dp_test_suite_registration, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
	DMA_UT_ADD_TEST(
		connect,
		"This test will verify connections by ETH Client via mocking "
		"ETH Driver.",
		ecpri_dma_eth_dp_test_suite_connect, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
	DMA_UT_ADD_TEST(
		single_pkt_single_buffer,
		"This test will verify the data path by sending a single packet"
		" with a single buffer on SRC ENDP and confirming ETH Client "
		"received it the same on DEST ENDP.",
		ecpri_dma_eth_dp_test_suite_single_pkt_single_buffer, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
	DMA_UT_ADD_TEST(
		mult_pkt_single_buffer,
		"This test will verify the data path by sending multiple "
		"packets with a single buffer on SRC ENDP and confirming ETH "
		" Client received them the same on DEST ENDP.",
		ecpri_dma_eth_dp_test_suite_mult_pkt_single_buffer, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
	DMA_UT_ADD_TEST(
		mode_change,
		"This test will check the ability to set different modes in "
		"the DMA Driver by changing the modes in various stages of "
		" packet sending.",
		ecpri_dma_eth_dp_test_suite_mode_change, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
	DMA_UT_ADD_TEST(
		chains_large_payload,
		"This test will check the ability to chain the large payloads "
		"by sending large packets to the DMA Driver",
		ecpri_dma_eth_dp_test_suite_chains_large_payload, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
	DMA_UT_ADD_TEST(
		commit_test,
		"This test will check commit functionality by sending packets "
		"with false and true commit parameters.",
		ecpri_dma_eth_dp_test_suite_commit, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
} DMA_UT_DEFINE_SUITE_END(eth_dp);
