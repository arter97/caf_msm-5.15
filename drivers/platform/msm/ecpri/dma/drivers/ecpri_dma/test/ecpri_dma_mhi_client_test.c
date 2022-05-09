/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "gsi.h"
#include "gsihal.h"
#include "dmahal.h"
#include "ecpri_dma_utils.h"
#include "ecpri_dma_mhi_client.h"
#include "ecpri_dma_ut_framework.h"


#define ECPRI_DMA_MHI_TEST_NUM_HW_CHS				4
#define ECPRI_DMA_MHI_TEST_NUM_EVENT_RINGS			ECPRI_DMA_MHI_TEST_NUM_HW_CHS
#define ECPRI_DMA_MHI_TEST_MSI_DATA					0x10000000
#define ECPRI_DMA_MHI_TEST_MSI_MASK					~0x10000000
#define ECPRI_DMA_MHI_TEST_CMPLN_TIMEOUT			5000
#define ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID			100
#define ECPRI_DMA_MHI_TEST_FIRST_EVENT_RING_ID		0
#define ECPRI_DMA_MHI_TEST_RING_LEN					(0x10)
#define ECPRI_DMA_MHI_TEST_FRST_SRC_CHANNEL_ID		0
#define ECPRI_DMA_MHI_TEST_SCND_SRC_CHANNEL_ID		2
#define ECPRI_DMA_MHI_TEST_FRST_DEST_CHANNEL_ID		1
#define ECPRI_DMA_MHI_TEST_SCND_DEST_CHANNEL_ID		3
#define ECPRI_DMA_MHI_TEST_PACKET_CONTENT			0x12345678
#define ECPRI_DMA_MHI_TEST_BUFF_SIZE				1500
#define ECPRI_DMA_MHI_TEST_VM0_SRC_ENDP_ID			20
#define ECPRI_DMA_MHI_TEST_VM1_SRC_ENDP_ID			22
#define ECPRI_DMA_MHI_TEST_VM2_SRC_ENDP_ID			24
#define ECPRI_DMA_MHI_TEST_VM3_SRC_ENDP_ID			26
#define ECPRI_DMA_MHI_TEST_VM0_DEST_ENDP_ID			57
#define ECPRI_DMA_MHI_TEST_VM1_DEST_ENDP_ID			59
#define ECPRI_DMA_MHI_TEST_VM2_DEST_ENDP_ID			61
#define ECPRI_DMA_MHI_TEST_VM3_DEST_ENDP_ID			63
#define ECPRI_DMA_MHI_TEST_EXPECTED_CH_EV_MASK_VMS	0xF
#define ECPRI_DMA_MHI_TEST_EXPECTED_CH_EV_MASK_PF	0x0
#define ECPRI_DMA_MHI_TEST_MSI_INTRPT_LOOPS			20
#define ECPRI_DMA_MHI_TEST_START_POLL_LOOPS			20

 /* Define for test endp buffer size */
#define ECPRI_DMA_MHI_TEST_BUFF_SIZE                1500
#define ECPRI_DMA_MHI_TEST_EMPTY_BUFF               0

/* Usage for setup, how many VMs to use in the test
*  1,..,4,5 ~ VM0,..,VM3,PF
*/
#define ECPRI_DMA_MHI_TEST_FUNCTIONS_TO_RUN         5

struct ecpri_dma_mhi_client_test_vf_id_mapping {
	u32 first_src_endp_id;
	u32 second_src_endp_id;
	u32 first_dest_endp_id;
	u32 second_dest_endp_id;
};

static const int ecpri_dma_mhi_client_test_ev_id_map
[ECPRI_DMA_MHI_TEST_NUM_EVENT_RINGS] =
{
	[0] = ECPRI_DMA_MHI_TEST_FIRST_EVENT_RING_ID + 0,
	[1] = ECPRI_DMA_MHI_TEST_FIRST_EVENT_RING_ID + 1,
	[2] = ECPRI_DMA_MHI_TEST_FIRST_EVENT_RING_ID + 2,
	[3] = ECPRI_DMA_MHI_TEST_FIRST_EVENT_RING_ID + 3
};

static const int ecpri_dma_mhi_client_test_host_ch_id_map
[ECPRI_DMA_MHI_TEST_NUM_HW_CHS] =
{
	[0] = ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID +
		  ECPRI_DMA_MHI_TEST_FRST_SRC_CHANNEL_ID,
	[1] = ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID +
		  ECPRI_DMA_MHI_TEST_FRST_DEST_CHANNEL_ID,
	[2] = ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID +
		  ECPRI_DMA_MHI_TEST_SCND_SRC_CHANNEL_ID,
	[3] = ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID +
		  ECPRI_DMA_MHI_TEST_SCND_DEST_CHANNEL_ID
};

/* HW CH */
static const struct ecpri_dma_mhi_client_test_vf_id_mapping
ecpri_dma_mhi_client_test_mapping
[ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM] = {
	[ECPRI_DMA_VM_IDS_VM0] = {
		.first_src_endp_id = 20,
		.second_src_endp_id = 21,
		.first_dest_endp_id = 57,
		.second_dest_endp_id = 58
	},
	[ECPRI_DMA_VM_IDS_VM1] = {
		.first_src_endp_id = 23,
		.second_src_endp_id = 24,
		.first_dest_endp_id = 60,
		.second_dest_endp_id = 61
	},
	[ECPRI_DMA_VM_IDS_VM2] = {
		.first_src_endp_id = 26,
		.second_src_endp_id = 27,
		.first_dest_endp_id = 63,
		.second_dest_endp_id = 64
	},
	[ECPRI_DMA_VM_IDS_VM3] = {
		.first_src_endp_id = 29,
		.second_src_endp_id = 30,
		.first_dest_endp_id = 66,
		.second_dest_endp_id = 67
	}
};

/**
 * enum ecpri_dma_mhi_client_test_burst_mode - MHI channel burst mode state
 *
 * Values are according to MHI specification
 * @ECPRI_DMA_MHI_BURST_MODE_DEFAULT: burst mode enabled for HW channels,
 * disabled for SW channels
 * @ECPRI_DMA_MHI_BURST_MODE_RESERVED:
 * @ECPRI_DMA_MHI_TEST_BURST_MODE_DISABLE: Burst mode is disabled for this ch
 * @ECPRI_DMA_MHI_BURST_MODE_ENABLE: Burst mode is enabled for this channel
 *
 */
enum ecpri_dma_mhi_client_test_burst_mode {
	ECPRI_DMA_MHI_TEST_BURST_MODE_DEFAULT,
	ECPRI_DMA_MHI_TEST_BURST_MODE_RESERVED,
	ECPRI_DMA_MHI_TEST_BURST_MODE_DISABLE,
	ECPRI_DMA_MHI_TEST_BURST_MODE_ENABLE,
};

/**
 * ecpri_dma_mhi_client_test_suite_context - all the information that
 * has to be available globally during the test
 * @dummy - dummy variable
 *
 */
struct ecpri_dma_mhi_client_test_suite_context {

	int priv;
	struct ecpri_dma_mem_buffer msi;
	struct ecpri_dma_mem_buffer ch_ctx_array;
	struct ecpri_dma_mem_buffer ev_ctx_array;
	struct ecpri_dma_mem_buffer mmio_buf;
	struct ecpri_dma_mem_buffer xfer_ring_bufs[ECPRI_DMA_MHI_TEST_NUM_HW_CHS];
	struct ecpri_dma_mem_buffer ev_ring_bufs[ECPRI_DMA_MHI_TEST_NUM_EVENT_RINGS];
	struct ecpri_dma_mem_buffer src_buffer;
	struct ecpri_dma_mem_buffer dest_buffer;

	struct completion ecpri_dma_mhi_test_ready_comp;
	struct completion ecpri_dma_mhi_test_async_comp;

	u8 channel_ids[ECPRI_DMA_MHI_TEST_NUM_HW_CHS];
	u32 channel_hdls[ECPRI_DMA_MHI_TEST_NUM_HW_CHS];

	int async_user_data;
	struct mhi_dma_function_params function;
	struct mhi_dma_init_params init_params;
	struct mhi_dma_init_out out_params;
	struct mhi_dma_start_params start_params;
	struct mhi_dma_connect_params frst_src_conn_params;
	struct mhi_dma_connect_params frst_dest_conn_params;
	struct mhi_dma_connect_params second_src_conn_params;
	struct mhi_dma_connect_params second_dest_conn_params;
	struct mhi_dma_disconnect_params frst_src_disc_params;
	struct mhi_dma_disconnect_params frst_dest_disc_params;
	struct mhi_dma_disconnect_params second_src_disc_params;
	struct mhi_dma_disconnect_params second_dest_disc_params;
};

struct ecpri_dma_mhi_client_test_suite_context*
	mhi_client_test_suite_ctx[ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM] = { NULL };

extern struct ecpri_dma_mhi_client_context*
ecpri_dma_mhi_client_ctx[ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM];

extern struct ecpri_dma_mhi_memcpy_context*
ecpri_dma_mhi_memcpy_ctx[ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM];

extern const struct mhi_dma_ops ecpri_dma_mhi_driver_ops;

/**
 *
 * struct ecpri_dma_mhi_mmio_register_set - MHI configuration registers,
 *	control registers, status registers, pointers to doorbell arrays,
 *	pointers to channel and event context arrays.
 *
 * The structure is defined in mhi spec (register names are taken from there).
 *	Only values accessed by HWP or test are documented
 */
struct ecpri_dma_mhi_mmio_register_set {
	u32	mhireglen;
	u32	reserved_08_04;
	u32	mhiver;
	u32	reserved_10_0c;
	struct mhicfg {
		u8		nch;
		u8		reserved_15_8;
		u8		ner;
		u8		reserved_31_23;
	} __packed mhicfg;

	u32	reserved_18_14;
	u32	chdboff;
	u32	reserved_20_1C;
	u32	erdboff;
	u32	reserved_28_24;
	u32	bhioff;
	u32	reserved_30_2C;
	u32	debugoff;
	u32	reserved_38_34;

	struct mhictrl {
		u32 rs : 1;
		u32 reset : 1;
		u32 reserved_7_2 : 6;
		u32 mhistate : 8;
		u32 reserved_31_16 : 16;
	} __packed mhictrl;

	u64	reserved_40_3c;
	u32	reserved_44_40;

	struct mhistatus {
		u32 ready : 1;
		u32 reserved_3_2 : 1;
		u32 syserr : 1;
		u32 reserved_7_3 : 5;
		u32 mhistate : 8;
		u32 reserved_31_16 : 16;
	} __packed mhistatus;

	/**
	 * Register is not accessed by HWP.
	 * In test register carries the handle for
	 *  the buffer of channel context array
	 */
	u32 reserved_50_4c;

	u32 mhierror;

	/**
	 * Register is not accessed by HWP.
	 * In test register carries the handle for
	 * the buffer of event ring context array
	 */
	u32 reserved_58_54;

	/**
	 * 64-bit pointer to the channel context array in the host memory space
	 *  host sets the pointer to the channel context array during
	 *  initialization.
	 */
	u64 ccabap;
	/**
	 * 64-bit pointer to the event context array in the host memory space
	 *  host sets the pointer to the event context array during
	 *  initialization
	 */
	u64 ecabap;
	/**
	 * Register is not accessed by HWP.
	 * In test register carries the pointer of virtual address
	 *  for the buffer of channel context array
	 */
	u64 crcbap;
	/**
	 * Register is not accessed by HWP.
	 * In test register carries the pointer of virtual address
	 *  for the buffer of event ring context array
	 */
	u64 crdb;

	u64	reserved_80_78;

	struct mhiaddr {
		/**
		 * Base address (64-bit) of the memory region in
		 *  the host address space where the MHI control
		 *  data structures are allocated by the host,
		 *  including channel context array, event context array,
		 *  and rings.
		 *  The device uses this information to set up its internal
		 *   address translation tables.
		 *  value must be aligned to 4 Kbytes.
		 */
		u64 mhicrtlbase;
		/**
		 * Upper limit address (64-bit) of the memory region in
		 *  the host address space where the MHI control
		 *  data structures are allocated by the host.
		 * The device uses this information to setup its internal
		 *  address translation tables.
		 * The most significant 32 bits of MHICTRLBASE and
		 * MHICTRLLIMIT registers must be equal.
		 */
		u64 mhictrllimit;
		u64 reserved_18_10;
		/**
		 * Base address (64-bit) of the memory region in
		 *  the host address space where the MHI data buffers
		 *  are allocated by the host.
		 * The device uses this information to setup its
		 *  internal address translation tables.
		 * value must be aligned to 4 Kbytes.
		 */
		u64 mhidatabase;
		/**
		 * Upper limit address (64-bit) of the memory region in
		 *  the host address space where the MHI data buffers
		 *  are allocated by the host.
		 * The device uses this information to setup its
		 *  internal address translation tables.
		 * The most significant 32 bits of MHIDATABASE and
		 *  MHIDATALIMIT registers must be equal.
		 */
		u64 mhidatalimit;
		u64 reserved_30_28;
	} __packed mhiaddr;

} __packed;

/**
 * ecpri_dma_mhi_client_test_get_funct_ctx_idx() -
 * Gets the VF/PF index in Global Context Array
 *
 * @function: [IN] Function parameters, type and id
 * @idx:            [OUT] The translated index
 *
 * Return codes: 0: success
 *		-EINVAL: Unexpected function type
 */
static inline int ecpri_dma_mhi_client_test_get_funct_ctx_idx(
	struct mhi_dma_function_params* function,
	int* idx)
{
	int ret = 0;
	if (function->function_type ==
		MHI_DMA_FUNCTION_TYPE_VIRTUAL &&
		function->vf_id < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM - 1) {
		*(idx) = function->vf_id;
	}
	else if (function->function_type ==
		MHI_DMA_FUNCTION_TYPE_PHYSICAL) {
		*(idx) = (ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM - 1);
	}
	else {
		DMA_UT_ERR("Unexpected function type, type: %d, vf_id: %d\n",
			function->function_type, function->vf_id);
		ret = -EINVAL;
	}

	return ret;
}

static int ecpri_dma_mhi_client_test_utils_invoke_start_dma(
	struct mhi_dma_function_params* function,
	struct mhi_dma_start_params* start_params)
{
	int ret = 0;
	DMA_UT_DBG("Invoking the mhi_start\n");
	ret = mhi_dma_start(*function, start_params);
	if (ret) {
		DMA_UT_ERR("mhi_start failed ret = %d\n", ret);
		DMA_UT_TEST_FAIL_REPORT("Fail to start MHI");
		return ret;
	}
	return ret;
}

static void ecpri_dma_mhi_test_poll_for_start(
	int idx, bool* timeout)
{
	int i = 0;

	while ((i < ECPRI_DMA_MHI_TEST_START_POLL_LOOPS) &&
		(ecpri_dma_mhi_client_ctx[idx]->state !=
			ECPRI_DMA_MHI_STATE_STARTED)) {
		msleep(100);
		i++;
	}

	if (i < ECPRI_DMA_MHI_TEST_START_POLL_LOOPS) {
		*timeout = false;
	}
}

static int ecpri_dma_mhi_client_test_util_setup_dma_endps(
	int src_endp_id,
	int dest_endp_id,
	bool enable_loopback)
{
	ecpri_hwio_def_ecpri_endp_cfg_destn_u endp_cfg_dest = { 0 };
	ecpri_hwio_def_ecpri_endp_cfg_xbarn_u endp_cfg_xbar = { 0 };
	ecpri_hwio_def_ecpri_endp_cfg_aggr_n_u cfg_aggr = { 0 };
	ecpri_hwio_def_ecpri_endp_nfapi_reassembly_cfg_n_u reassembly_cfg = { 0 };

	if (!ecpri_dma_ctx->endp_map)
		return -EINVAL;

	if (!ecpri_dma_ctx->endp_map[src_endp_id].valid ||
		ecpri_dma_ctx->endp_map[src_endp_id].is_exception ||
		!ecpri_dma_ctx->endp_map[dest_endp_id].valid ||
		ecpri_dma_ctx->endp_map[dest_endp_id].is_exception)
		return -EINVAL;

	/* Configure test SRC ENDP to loopback into test DEST ENDP */
	memset(&endp_cfg_dest, 0, sizeof(endp_cfg_dest));
	memset(&endp_cfg_xbar, 0, sizeof(endp_cfg_xbar));
	memset(&cfg_aggr, 0, sizeof(cfg_aggr));
	memset(&reassembly_cfg, 0, sizeof(reassembly_cfg));

	/* First disable ENDPs */
	ecpri_dma_hal_write_reg_n(
		ECPRI_ENDP_GSI_CFG_n, src_endp_id, 0);

	ecpri_dma_hal_write_reg_n(
		ECPRI_ENDP_GSI_CFG_n, dest_endp_id, 0);

	if (enable_loopback)
	{
		/* Set SRC to M2M mode*/
		endp_cfg_dest.def.use_dest_cfg = 1;
		endp_cfg_dest.def.dest_mem_channel =
			dest_endp_id;
		ecpri_dma_hal_write_reg_n(
			ECPRI_ENDP_CFG_DEST_n, src_endp_id,
			endp_cfg_dest.value);
		ecpri_dma_hal_write_reg_n(
			ECPRI_ENDP_CFG_XBAR_n, src_endp_id,
			endp_cfg_xbar.value);
		/* DEST */
		ecpri_dma_hal_write_reg_n(
			ECPRI_ENDP_CFG_AGGR_n,
			dest_endp_id, cfg_aggr.value);
		ecpri_dma_hal_write_reg_n(
			ECPRI_ENDP_NFAPI_REASSEMBLY_CFG_n,
			dest_endp_id, reassembly_cfg.value);
	}
	else {
		/* SRC */
		switch (ecpri_dma_ctx->endp_map[src_endp_id].stream_mode) {
		case ECPRI_DMA_ENDP_STREAM_MODE_M2M:
			endp_cfg_dest.def.use_dest_cfg = 1;
			endp_cfg_dest.def.dest_mem_channel =
				ecpri_dma_ctx->endp_map[src_endp_id].dest;
			ecpri_dma_hal_write_reg_n(
				ECPRI_ENDP_CFG_DEST_n, src_endp_id,
				endp_cfg_dest.value);
			break;
		case ECPRI_DMA_ENDP_STREAM_MODE_M2S:
			endp_cfg_dest.def.use_dest_cfg = 0;
			endp_cfg_xbar.def.dest_stream =
				ecpri_dma_ctx->endp_map[src_endp_id].dest;
			endp_cfg_xbar.def.xbar_tid =
				ecpri_dma_ctx->endp_map[src_endp_id].tid.value;
			//TODO: Below are required for nFAPI
			//endp_cfg_xbar.xbar_user = Get from Core driver, need API
			endp_cfg_xbar.def.l2_segmentation_en =
				ecpri_dma_ctx->endp_map[src_endp_id].is_nfapi ? 1 : 0;
			ecpri_dma_hal_write_reg_n(
				ECPRI_ENDP_CFG_DEST_n, src_endp_id,
				endp_cfg_dest.value);
			ecpri_dma_hal_write_reg_n(
				ECPRI_ENDP_CFG_XBAR_n, src_endp_id,
				endp_cfg_xbar.value);
			break;
		default:
			DMA_UT_ERR("SRC ENDP %d isn't M2M or S2M, address = 0x%px\n",
				src_endp_id, &ecpri_dma_ctx->endp_map[src_endp_id]);
			return -EINVAL;
			break;
		}

		/* DEST */
		switch (ecpri_dma_ctx->endp_map[dest_endp_id].stream_mode) {
		case ECPRI_DMA_ENDP_STREAM_MODE_M2M:
			break;
		case ECPRI_DMA_ENDP_STREAM_MODE_S2M:
			if (ecpri_dma_ctx->endp_map[dest_endp_id].is_nfapi) {
				memset(&cfg_aggr, 0,
					sizeof(cfg_aggr));
				memset(&reassembly_cfg, 0,
					sizeof(reassembly_cfg));
				cfg_aggr.def.aggr_type = 1;
				reassembly_cfg.def.vm_id =
					ecpri_dma_ctx->endp_map[dest_endp_id]
					.nfapi_dest_vm_id;
				ecpri_dma_hal_write_reg_n(
					ECPRI_ENDP_CFG_AGGR_n,
					dest_endp_id, cfg_aggr.value);
				ecpri_dma_hal_write_reg_n(
					ECPRI_ENDP_NFAPI_REASSEMBLY_CFG_n,
					dest_endp_id, reassembly_cfg.value);
			}
			break;
		default:
			DMA_UT_ERR("DEST ENDP %d isn't M2M or S2M, address = 0x%px\n",
				dest_endp_id, &ecpri_dma_ctx->endp_map[dest_endp_id]);
			return -EINVAL;
			break;
		}
	}

	/* Re-enable ENDPs */
	ecpri_dma_hal_write_reg_n(
		ECPRI_ENDP_GSI_CFG_n, src_endp_id, 1);

	ecpri_dma_hal_write_reg_n(
		ECPRI_ENDP_GSI_CFG_n, dest_endp_id, 1);

	return 0;
}

/**
 * ecpri_dma_mhi_client_test_get_ee_index() - Gets the correspodning EE ID
 *
 * @function: [IN] Function parameters, type and id
 * @ee_idx:         [OUT] The EE index from map array
 *
 * Return codes: 0: success
 *		-EINVAL: Unexpected function type
 */
static inline int ecpri_dma_mhi_client_test_get_ee_index(
	struct mhi_dma_function_params function,
	enum ecpri_dma_ees* ee_idx)
{
	int ret = 0;
	if (function.function_type ==
		MHI_DMA_FUNCTION_TYPE_VIRTUAL &&
		function.vf_id < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM - 1) {
		switch (function.vf_id)
		{
		case ECPRI_DMA_VM_IDS_VM0:
			*(ee_idx) = ECPRI_DMA_EE_VM0;
			break;
		case ECPRI_DMA_VM_IDS_VM1:
			*(ee_idx) = ECPRI_DMA_EE_VM1;
			break;
		case ECPRI_DMA_VM_IDS_VM2:
			*(ee_idx) = ECPRI_DMA_EE_VM2;
			break;
		case ECPRI_DMA_VM_IDS_VM3:
			*(ee_idx) = ECPRI_DMA_EE_VM3;
			break;
		default:
			DMA_UT_ERR("Unexpected function type, type: %d, vf_id: %d\n",
				function.function_type, function.vf_id);
			ret = -EINVAL;
			break;
		}
	}
	else if (function.function_type ==
		MHI_DMA_FUNCTION_TYPE_PHYSICAL) {
		*(ee_idx) = ECPRI_DMA_EE_PF;
	}
	else {
		DMA_UT_ERR("Unexpected function type, type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		ret = -EINVAL;
	}

	return ret;
}

static void ecpri_dma_mhi_client_test_free_mmio_space(int idx)
{

	DMA_UT_DBG("Free MMIO Space enter\n");

	if (!mhi_client_test_suite_ctx[idx]) {
		DMA_UT_ERR("MHI test context is not initialised, idx: %d\n",
			idx);
		return;
	}

	dma_free_coherent(ecpri_dma_ctx->pdev,
		mhi_client_test_suite_ctx[idx]->ev_ctx_array.size,
		mhi_client_test_suite_ctx[idx]->ev_ctx_array.virt_base,
		mhi_client_test_suite_ctx[idx]->ev_ctx_array.phys_base);

	dma_free_coherent(ecpri_dma_ctx->pdev,
		mhi_client_test_suite_ctx[idx]->ch_ctx_array.size,
		mhi_client_test_suite_ctx[idx]->ch_ctx_array.virt_base,
		mhi_client_test_suite_ctx[idx]->ch_ctx_array.phys_base);

	dma_free_coherent(((struct gsi_ctx*)ecpri_dma_ctx->gsi_dev_hdl)->dev,
		mhi_client_test_suite_ctx[idx]->msi.size,
		mhi_client_test_suite_ctx[idx]->msi.virt_base,
		mhi_client_test_suite_ctx[idx]->msi.phys_base);
}

static int ecpri_dma_mhi_client_test_alloc_mmio_space(int idx)
{
	int ret = 0;
	struct ecpri_dma_mem_buffer* msi;
	struct ecpri_dma_mem_buffer* ch_ctx_array;
	struct ecpri_dma_mem_buffer* ev_ctx_array;
	struct ecpri_dma_mem_buffer* mmio_buf;
	struct ecpri_dma_mhi_mmio_register_set* p_mmio;

	DMA_UT_DBG("Entry\n");

	msi = &mhi_client_test_suite_ctx[idx]->msi;
	ch_ctx_array = &mhi_client_test_suite_ctx[idx]->ch_ctx_array;
	ev_ctx_array = &mhi_client_test_suite_ctx[idx]->ev_ctx_array;
	mmio_buf = &mhi_client_test_suite_ctx[idx]->mmio_buf;

	/* Allocate MSI */
	msi->size = 4; // Mocks DB register
	msi->virt_base = dma_alloc_coherent(
		((struct gsi_ctx*)ecpri_dma_ctx->gsi_dev_hdl)->dev, msi->size,
		&msi->phys_base, GFP_KERNEL);
	if (!msi->virt_base) {
		DMA_UT_ERR("no mem for msi\n");
		ret = -ENOMEM;
		goto fail_alloc_msi;
	}

	DMA_UT_DBG("msi: virt_base 0x%px phys_addr 0x%pad size %d\n",
		msi->virt_base, &msi->phys_base, msi->size);

	/* allocate buffer for channel context */
	ch_ctx_array->size = sizeof(struct ecpri_dma_mhi_host_ch_ctx) *
		ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID + ECPRI_DMA_MHI_TEST_NUM_HW_CHS;
	ch_ctx_array->virt_base = dma_alloc_coherent(ecpri_dma_ctx->pdev,
		ch_ctx_array->size, &ch_ctx_array->phys_base, GFP_KERNEL);
	if (!ch_ctx_array->virt_base) {
		DMA_UT_ERR("no mem for ch ctx array\n");
		ret = -ENOMEM;
		goto fail_alloc_ch_ctx_arr;
	}
	DMA_UT_DBG("channel ctx array: virt_base 0x%px phys_addr %pad size %d\n",
		ch_ctx_array->virt_base, &ch_ctx_array->phys_base,
		ch_ctx_array->size);

	/* allocate buffer for event context */
	ev_ctx_array->size = sizeof(struct ecpri_dma_mhi_host_ev_ctx) *
		ECPRI_DMA_MHI_TEST_FIRST_EVENT_RING_ID +
		ECPRI_DMA_MHI_TEST_NUM_EVENT_RINGS;
	ev_ctx_array->virt_base = dma_alloc_coherent(ecpri_dma_ctx->pdev,
		ev_ctx_array->size, &ev_ctx_array->phys_base, GFP_KERNEL);
	if (!ev_ctx_array->virt_base) {
		DMA_UT_ERR("no mem for ev ctx array\n");
		ret = -ENOMEM;
		goto fail_alloc_ev_ctx_arr;
	}
	DMA_UT_DBG("event ctx array: base 0x%px phys_addr %pad size %d\n",
		ev_ctx_array->virt_base, &ev_ctx_array->phys_base,
		ev_ctx_array->size);

	/* allocate buffer for mmio */
	mmio_buf->size = sizeof(struct ecpri_dma_mhi_mmio_register_set);
	mmio_buf->virt_base = dma_alloc_coherent(ecpri_dma_ctx->pdev, mmio_buf->size,
		&mmio_buf->phys_base, GFP_KERNEL);
	if (!mmio_buf->virt_base) {
		DMA_UT_ERR("no mem for mmio buf\n");
		ret = -ENOMEM;
		goto fail_alloc_mmio_buf;
	}
	DMA_UT_DBG("mmio buffer: virt_base 0x%px phys_addr %pad size %d\n",
		mmio_buf->virt_base, &mmio_buf->phys_base, mmio_buf->size);

	/* initlize table */
	p_mmio = (struct ecpri_dma_mhi_mmio_register_set*)mmio_buf->virt_base;

	/**
	 * 64-bit pointer to the channel context array in the host memory space;
	 * Host sets the pointer to the channel context array
	 * during initialization.
	 */
	p_mmio->ccabap = (u32)ch_ctx_array->phys_base -
		(ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID *
			sizeof(struct ecpri_dma_mhi_host_ch_ctx));
	DMA_UT_DBG("pMmio->ccabap 0x%llx\n", p_mmio->ccabap);

	/**
	 * 64-bit pointer to the event context array in the host memory space;
	 * Host sets the pointer to the event context array
	 * during initialization
	 */
	p_mmio->ecabap = (u32)ev_ctx_array->phys_base -
		(ECPRI_DMA_MHI_TEST_FIRST_EVENT_RING_ID *
			sizeof(struct ecpri_dma_mhi_host_ev_ctx));
	DMA_UT_DBG("pMmio->ecabap 0x%llx\n", p_mmio->ecabap);

	/**
	 * Register is not accessed by HWP.
	 * In test register carries the pointer of
	 *  virtual address for the buffer of channel context array
	 */
	p_mmio->crcbap = (unsigned long)ch_ctx_array->virt_base;

	/**
	 * Register is not accessed by HWP.
	 * In test register carries the pointer of
	 *  virtual address for the buffer of channel context array
	 */
	p_mmio->crdb = (unsigned long)ev_ctx_array->virt_base;

	/* test is running only on device. no need to translate addresses */
	p_mmio->mhiaddr.mhicrtlbase = 0x04;
	p_mmio->mhiaddr.mhictrllimit = 0xFFFFFFFF;
	p_mmio->mhiaddr.mhidatabase = 0x04;
	p_mmio->mhiaddr.mhidatalimit = 0xFFFFFFFF;

	return ret;

fail_alloc_mmio_buf:
	dma_free_coherent(ecpri_dma_ctx->pdev, ev_ctx_array->size,
		ev_ctx_array->virt_base, ev_ctx_array->phys_base);
	ev_ctx_array->virt_base = NULL;
fail_alloc_ev_ctx_arr:
	dma_free_coherent(ecpri_dma_ctx->pdev, ch_ctx_array->size,
		ch_ctx_array->virt_base, ch_ctx_array->phys_base);
	ch_ctx_array->virt_base = NULL;
fail_alloc_ch_ctx_arr:
	dma_free_coherent(ecpri_dma_ctx->pdev, msi->size, msi->virt_base,
		msi->phys_base);
	msi->virt_base = NULL;
fail_alloc_msi:
	return ret;
}

static int ecpri_dma_mhi_client_test_alloc_src_dest_buffers(int idx)
{
	int ret = 0;
	struct ecpri_dma_mem_buffer* src_buffer;
	struct ecpri_dma_mem_buffer* dest_buffer;

	DMA_UT_DBG("Allocating buffers \n");

	src_buffer = &mhi_client_test_suite_ctx[idx]->src_buffer;
	dest_buffer = &mhi_client_test_suite_ctx[idx]->dest_buffer;

	/* allocate src buffer */
	src_buffer->size = sizeof(u32);

	src_buffer->virt_base = dma_alloc_coherent(ecpri_dma_ctx->pdev,
		src_buffer->size, &src_buffer->phys_base, GFP_KERNEL);
	if (!src_buffer->virt_base) {
		DMA_UT_ERR("no mem for src_buffer\n");
		ret = -ENOMEM;
		goto fail_alloc_src_buffer;
	}

	DMA_UT_DBG("src_buffer: virt_base 0x%px phys_addr %pad size %d\n",
		src_buffer->virt_base, &src_buffer->phys_base,
		src_buffer->size);

	/* allocate dest buffer */
	dest_buffer->size = sizeof(u32);

	dest_buffer->virt_base = dma_alloc_coherent(ecpri_dma_ctx->pdev,
		dest_buffer->size, &dest_buffer->phys_base, GFP_KERNEL);
	if (!dest_buffer->virt_base) {
		DMA_UT_ERR("no mem for dest_buffer\n");
		ret = -ENOMEM;
		goto fail_alloc_dest_buffer;
	}

	DMA_UT_DBG("dest_buffer: virt_base 0x%px phys_addr %pad size %d\n",
		dest_buffer->virt_base, &dest_buffer->phys_base,
		dest_buffer->size);

	return ret;

fail_alloc_dest_buffer:
	dma_free_coherent(ecpri_dma_ctx->pdev, src_buffer->size,
		src_buffer->virt_base,
		src_buffer->phys_base);
	src_buffer->virt_base = NULL;
fail_alloc_src_buffer:
	return ret;
}

static void ecpri_dma_mhi_client_test_free_src_dest_buffers(int idx)
{
	struct ecpri_dma_mem_buffer* src_buffer;
	struct ecpri_dma_mem_buffer* dest_buffer;

	DMA_UT_DBG("deallocating buffers \n");

	src_buffer = &mhi_client_test_suite_ctx[idx]->src_buffer;
	dest_buffer = &mhi_client_test_suite_ctx[idx]->dest_buffer;

	DMA_UT_DBG("dest buffers deallocation start \n");
	dma_free_coherent(ecpri_dma_ctx->pdev, dest_buffer->size,
		dest_buffer->virt_base,
		dest_buffer->phys_base);
	DMA_UT_DBG("dest buffers deallocated \n");
	dest_buffer->virt_base = NULL;

	DMA_UT_DBG("src buffers deallocation start \n");
	dma_free_coherent(ecpri_dma_ctx->pdev, src_buffer->size,
		src_buffer->virt_base,
		src_buffer->phys_base);
	DMA_UT_DBG("src buffers deallocated \n");
	src_buffer->virt_base = NULL;
	DMA_UT_DBG("Finished\n");
}

static void ecpri_dma_mhi_client_test_cb(void* priv,
	enum mhi_dma_event_type event, unsigned long data)
{
	int idx = *((int*)priv);
	DMA_UT_DBG("CB\n");

	switch (event)
	{
	case MHI_DMA_EVENT_READY:
		if (ecpri_dma_mhi_client_test_utils_invoke_start_dma(
			&mhi_client_test_suite_ctx[idx]->function,
			&mhi_client_test_suite_ctx[idx]->start_params) != 0) {
			DMA_UT_ERR("Start for VF_ID %d has failed\n",
				mhi_client_test_suite_ctx[idx]->function.vf_id);
			break;
		}
		DMA_UT_DBG("Completed on ready_cb for IDX: %d\n", idx);
		break;
	default:
		WARN_ON(1);
		break;
	}
}

static void ecpri_dma_mhi_client_test_async_comp_cb(void* user_data)
{
	DMA_UT_DBG("Async CB\n");
	/* Compare user data context user data */
	if (*(int*)user_data !=
		mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->async_user_data) {
		DMA_UT_DBG("User data missmatch, data received: %d, data expected: %d\n",
			mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->async_user_data,
			*(int*)user_data);
		return;
	}

	complete(&mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->
		ecpri_dma_mhi_test_async_comp);
}

static inline int ecpri_dma_mhi_client_test_utils_get_ee_index(
	struct mhi_dma_function_params function,
	enum ecpri_dma_ees* ee_idx)
{
	int ret = 0;
	if (function.function_type ==
		MHI_DMA_FUNCTION_TYPE_VIRTUAL &&
		function.vf_id < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM - 1) {
		switch (function.vf_id)
		{
		case ECPRI_DMA_VM_IDS_VM0:
			*(ee_idx) = ECPRI_DMA_EE_VM0;
			break;
		case ECPRI_DMA_VM_IDS_VM1:
			*(ee_idx) = ECPRI_DMA_EE_VM1;
			break;
		case ECPRI_DMA_VM_IDS_VM2:
			*(ee_idx) = ECPRI_DMA_EE_VM2;
			break;
		case ECPRI_DMA_VM_IDS_VM3:
			*(ee_idx) = ECPRI_DMA_EE_VM3;
			break;
		default:
			DMA_UT_ERR("Unexpected function type, type: %d, vf_id: %d\n",
				function.function_type, function.vf_id);
			ret = -EINVAL;
			break;
		}
	}
	else if (function.function_type ==
		MHI_DMA_FUNCTION_TYPE_PHYSICAL) {
		*(ee_idx) = ECPRI_DMA_EE_PF;
	}
	else {
		DMA_UT_ERR("Unexpected function type, type: %d, vf_id: %d\n",
			function.function_type, function.vf_id);
		ret = -EINVAL;
	}

	return ret;
}

static void  ecpri_dma_mhi_client_test_destroy_channel_context(
	struct ecpri_dma_mem_buffer transfer_ring_bufs[],
	struct ecpri_dma_mem_buffer event_ring_bufs[],
	u8 host_ch_id,
	u8 host_ev_id)
{
	u8 dev_ev_ring_idx;
	u8 dev_xfer_ring_idx;

	DMA_UT_DBG("Entry\n");

	if (host_ch_id < ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID) {
		DMA_UT_ERR("channal_id invalid %d\n", host_ch_id);
		return;
	}

	if (host_ev_id < ECPRI_DMA_MHI_TEST_FIRST_EVENT_RING_ID) {
		DMA_UT_ERR("host_ev_id invalid %d\n", host_ev_id);
		return;
	}

	dev_ev_ring_idx = host_ev_id - ECPRI_DMA_MHI_TEST_FIRST_EVENT_RING_ID;
	dev_xfer_ring_idx = host_ch_id - ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID;

	if (transfer_ring_bufs[dev_xfer_ring_idx].phys_base) {
		dma_free_coherent(((struct gsi_ctx*)ecpri_dma_ctx->gsi_dev_hdl)->dev,
			transfer_ring_bufs[dev_xfer_ring_idx].size,
			transfer_ring_bufs[dev_xfer_ring_idx].virt_base,
			transfer_ring_bufs[dev_xfer_ring_idx].phys_base);
		transfer_ring_bufs[dev_xfer_ring_idx].virt_base = NULL;
	}

	if (event_ring_bufs[dev_ev_ring_idx].phys_base) {
		dma_free_coherent(((struct gsi_ctx*)ecpri_dma_ctx->gsi_dev_hdl)->dev,
			event_ring_bufs[dev_ev_ring_idx].size,
			event_ring_bufs[dev_ev_ring_idx].virt_base,
			event_ring_bufs[dev_ev_ring_idx].phys_base);
		event_ring_bufs[dev_ev_ring_idx].virt_base = NULL;
	}
}

static int ecpri_dma_mhi_client_test_config_channel_context(
	struct ecpri_dma_mem_buffer* mmio,
	struct ecpri_dma_mem_buffer transfer_ring_bufs[],
	struct ecpri_dma_mem_buffer event_ring_bufs[],
	u8 host_ch_id,
	u8 host_ev_id,
	u16 transfer_ring_size,
	u16 event_ring_size,
	u8 ch_type)
{
	struct ecpri_dma_mhi_mmio_register_set* p_mmio;
	struct ecpri_dma_mhi_host_ch_ctx* host_channels;
	struct ecpri_dma_mhi_host_ev_ctx* host_events;
	u32 dev_ev_ring_idx;
	u32 dev_xfer_ring_idx;

	DMA_UT_DBG("Entry\n");

	if (host_ch_id < ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID) {
		DMA_UT_ERR("Host %d\n", host_ch_id);
		DMA_UT_TEST_FAIL_REPORT("host_ch_id invalid\n");
		return -EFAULT;
	}

	if (host_ev_id < ECPRI_DMA_MHI_TEST_FIRST_EVENT_RING_ID) {
		DMA_UT_ERR("Host %d\n", host_ev_id);
		DMA_UT_TEST_FAIL_REPORT("host_ev_id invalid\n");
		return -EFAULT;
	}

	p_mmio = (struct ecpri_dma_mhi_mmio_register_set*)mmio->virt_base;
	host_channels =
		(struct ecpri_dma_mhi_host_ch_ctx*)
		((unsigned long)p_mmio->crcbap);
	host_events = (struct ecpri_dma_mhi_host_ev_ctx*)
		((unsigned long)p_mmio->crdb);

	DMA_UT_DBG("p_mmio: %px host_channels: %px host_events: %px\n",
		p_mmio, host_channels, host_events);

	dev_xfer_ring_idx = host_ch_id - ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID;
	dev_ev_ring_idx = host_ev_id - ECPRI_DMA_MHI_TEST_FIRST_EVENT_RING_ID;

	DMA_UT_DBG("dev_xfer_ring_idx: %u host_ev_id: %u\n",
		dev_xfer_ring_idx, host_ev_id);
	if (transfer_ring_bufs[dev_xfer_ring_idx].virt_base) {
		DMA_UT_ERR("dev_xfer_ring_idx %d\n", dev_xfer_ring_idx);
		DMA_UT_TEST_FAIL_REPORT("dev_xfer_ring_idx is already allocated\n");
		return -EFAULT;
	}

	/* allocate and init event ring if needed */
	if (!event_ring_bufs[dev_ev_ring_idx].virt_base) {
		DMA_UT_DBG("Configuring event ring...\n");
		event_ring_bufs[dev_ev_ring_idx].size =
			event_ring_size *
			sizeof(struct gsi_xfer_compl_evt);
		event_ring_bufs[dev_ev_ring_idx].virt_base =
			dma_alloc_coherent(
				((struct gsi_ctx*)ecpri_dma_ctx->gsi_dev_hdl)->dev,
				event_ring_bufs[dev_ev_ring_idx].size,
				&event_ring_bufs[dev_ev_ring_idx].phys_base,
				GFP_KERNEL);
		if (!event_ring_bufs[dev_ev_ring_idx].virt_base) {
			DMA_UT_ERR("no mem for ev ring buf\n");
			return -ENOMEM;
		}
		host_events[host_ev_id].intmodc = 1;
		host_events[host_ev_id].intmodt = 0;
		host_events[host_ev_id].msivec = dev_ev_ring_idx;
		host_events[host_ev_id].rbase =
			(u32)event_ring_bufs[dev_ev_ring_idx].phys_base;
		host_events[host_ev_id].rlen =
			event_ring_bufs[dev_ev_ring_idx].size;
		host_events[host_ev_id].rp =
			(u32)event_ring_bufs[dev_ev_ring_idx].phys_base;
		host_events[host_ev_id].wp =
			(u32)event_ring_bufs[dev_ev_ring_idx].phys_base +
			event_ring_bufs[dev_ev_ring_idx].size - 16;
	}
	else {
		DMA_UT_DBG("Skip configuring event ring - already done\n");
	}

	transfer_ring_bufs[dev_xfer_ring_idx].size =
		transfer_ring_size *
		sizeof(struct gsi_tre);
	transfer_ring_bufs[dev_xfer_ring_idx].virt_base =
		dma_alloc_coherent(
			((struct gsi_ctx*)ecpri_dma_ctx->gsi_dev_hdl)->dev,
			transfer_ring_bufs[dev_xfer_ring_idx].size,
			&transfer_ring_bufs[dev_xfer_ring_idx].phys_base,
			GFP_KERNEL);
	if (!transfer_ring_bufs[dev_xfer_ring_idx].virt_base) {
		DMA_UT_ERR("no mem for xfer ring buf\n");
		dma_free_coherent(((struct gsi_ctx*)ecpri_dma_ctx->gsi_dev_hdl)->dev,
			event_ring_bufs[dev_ev_ring_idx].size,
			event_ring_bufs[dev_ev_ring_idx].virt_base,
			event_ring_bufs[dev_ev_ring_idx].phys_base);
		event_ring_bufs[dev_ev_ring_idx].virt_base = NULL;
		return -ENOMEM;
	}

	host_channels[host_ch_id].erindex = dev_ev_ring_idx;
	host_channels[host_ch_id].rbase =
		(u32)transfer_ring_bufs[dev_xfer_ring_idx].phys_base;
	host_channels[host_ch_id].rlen = transfer_ring_bufs[dev_xfer_ring_idx].size;
	host_channels[host_ch_id].rp =
		(u32)transfer_ring_bufs[dev_xfer_ring_idx].phys_base;
	host_channels[host_ch_id].wp =
		(u32)transfer_ring_bufs[dev_xfer_ring_idx].phys_base;
	host_channels[host_ch_id].chtype = ch_type;
	host_channels[host_ch_id].brsmode = ECPRI_DMA_MHI_TEST_BURST_MODE_DISABLE;
	host_channels[host_ch_id].pollcfg = 0;

	return 0;
}

static int ecpri_dma_mhi_client_test_verify_connect(
	struct mhi_dma_function_params* function,
	u32 src_endp_id,
	u32 dest_endp_id,
	u32 dev_src_ch_id,
	u32 dev_dest_ch_id,
	int idx)
{
	int ret = 0;
	struct ecpri_dma_endp_context* src_endp = NULL;
	struct ecpri_dma_endp_context* dest_endp = NULL;
	struct ecpri_dma_mhi_client_context* mhi_dma_ctx = NULL;

	mhi_dma_ctx = ecpri_dma_mhi_client_ctx[idx];
	if (!mhi_dma_ctx) {
		DMA_UT_ERR("VF_ID %d  failed\n", function->vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"context isn't initialized\n");
		return -EFAULT;
	}

	src_endp = &ecpri_dma_ctx->endp_ctx[src_endp_id];
	/* Verify SRC endp is enabled */
	if (!src_endp->valid) {
		DMA_UT_ERR("SRC endp %d for VM%d failed\n", src_endp_id, function->vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to SRC endp isn't enabled\n");
		return -EFAULT;
	}

	/* Verify SRC endp is running */
	if (mhi_dma_ctx->
		channels[mhi_client_test_suite_ctx[idx]->
		channel_ids[dev_src_ch_id]].state !=
		ECPRI_DMA_HW_MHI_CHANNEL_STATE_RUN) {
		DMA_UT_ERR("SRC endp %d for VM%d,"
			"dev_src_ch_id %d, test_channel_id %d failed\n",
			src_endp_id, function->vf_id, mhi_dma_ctx->
			channels[mhi_client_test_suite_ctx[idx]->
			channel_ids[dev_src_ch_id]].state,
			dev_src_ch_id,
			mhi_client_test_suite_ctx[idx]->
			channel_ids[dev_src_ch_id]);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"SRC endp isn't running\n");
		return -EFAULT;
	}

	/* Verify SRC endp's channel state */
	if (gsi_get_chan_state(src_endp->gsi_chan_hdl) !=
		GSI_CHAN_STATE_STARTED) {
		DMA_UT_ERR("SRC endp %d for VM%d channel %d\n",
			src_endp_id, function->vf_id,
			mhi_client_test_suite_ctx[idx]->channel_ids[dev_src_ch_id]);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"SRC endp isn't started\n");
		return -EFAULT;
	}

	dest_endp = &ecpri_dma_ctx->endp_ctx[dest_endp_id];
	/* Verify DEST endp is enabled */
	if (!dest_endp->valid) {
		DMA_UT_ERR("DEST endp %d for VM%d\n",
			dest_endp_id, function->vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to DEST endp"
			" isn't enabled\n");
		return -EFAULT;
	}

	/* Verify DEST endp is running */
	if (mhi_dma_ctx->
		channels[mhi_client_test_suite_ctx[idx]->
		channel_ids[dev_dest_ch_id]].state !=
		ECPRI_DMA_HW_MHI_CHANNEL_STATE_RUN) {
		DMA_UT_ERR("DEST endp %d for VM%d\n",
			dest_endp_id, function->vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"DEST endp isn't running\n");
		return -EFAULT;
	}

	/* Verify DEST endp's channel state */
	if (gsi_get_chan_state(dest_endp->gsi_chan_hdl) !=
		GSI_CHAN_STATE_STARTED) {
		DMA_UT_ERR("DEST endp %d for VM%d channel %d\n",
			dest_endp_id, function->vf_id,
			mhi_client_test_suite_ctx[idx]->
			channel_ids[dev_dest_ch_id]);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"DEST endp isn't started\n");
		return -EFAULT;
	}

	DMA_UT_DBG("Verification for VF_ID %d, SRC_ENDP_ID %d,"
		" DEST_ENDP_ID %d SUCCEEDDED\n",
		function->vf_id, src_endp_id, dest_endp_id);
	return ret;
}

static void ecpri_dma_mhi_client_test_destroy_data_context(int idx)
{
	/* Destroy DEST data buffer */
	dma_free_coherent(ecpri_dma_ctx->pdev,
		mhi_client_test_suite_ctx[idx]->dest_buffer.size,
		mhi_client_test_suite_ctx[idx]->dest_buffer.virt_base,
		mhi_client_test_suite_ctx[idx]->dest_buffer.phys_base);
	mhi_client_test_suite_ctx[idx]->dest_buffer.virt_base = NULL;

	/* Destroy SRC data buffer */
	dma_free_coherent(ecpri_dma_ctx->pdev,
		mhi_client_test_suite_ctx[idx]->src_buffer.size,
		mhi_client_test_suite_ctx[idx]->src_buffer.virt_base,
		mhi_client_test_suite_ctx[idx]->src_buffer.phys_base);
	mhi_client_test_suite_ctx[idx]->src_buffer.virt_base = NULL;

	/* Destroy channels context */
	ecpri_dma_mhi_client_test_destroy_channel_context(
		mhi_client_test_suite_ctx[idx]->xfer_ring_bufs,
		mhi_client_test_suite_ctx[idx]->ev_ring_bufs,
		ecpri_dma_mhi_client_test_host_ch_id_map[3],
		ecpri_dma_mhi_client_test_ev_id_map[3]);

	ecpri_dma_mhi_client_test_destroy_channel_context(
		mhi_client_test_suite_ctx[idx]->xfer_ring_bufs,
		mhi_client_test_suite_ctx[idx]->ev_ring_bufs,
		ecpri_dma_mhi_client_test_host_ch_id_map[2],
		ecpri_dma_mhi_client_test_ev_id_map[2]);

	ecpri_dma_mhi_client_test_destroy_channel_context(
		mhi_client_test_suite_ctx[idx]->xfer_ring_bufs,
		mhi_client_test_suite_ctx[idx]->ev_ring_bufs,
		ecpri_dma_mhi_client_test_host_ch_id_map[1],
		ecpri_dma_mhi_client_test_ev_id_map[1]);

	ecpri_dma_mhi_client_test_destroy_channel_context(
		mhi_client_test_suite_ctx[idx]->xfer_ring_bufs,
		mhi_client_test_suite_ctx[idx]->ev_ring_bufs,
		ecpri_dma_mhi_client_test_host_ch_id_map[0],
		ecpri_dma_mhi_client_test_ev_id_map[0]);
}

static int ecpri_dma_mhi_client_test_setup_channels(int idx)
{
	int ret = 0;

	DMA_UT_DBG("Entry setup_channels\n");

	/* Config First SRC Channel Context */
	ret = ecpri_dma_mhi_client_test_config_channel_context(
		&mhi_client_test_suite_ctx[idx]->mmio_buf,
		mhi_client_test_suite_ctx[idx]->xfer_ring_bufs,
		mhi_client_test_suite_ctx[idx]->ev_ring_bufs,
		ecpri_dma_mhi_client_test_host_ch_id_map[0],
		ecpri_dma_mhi_client_test_ev_id_map[0],
		ECPRI_DMA_MHI_TEST_RING_LEN,
		ECPRI_DMA_MHI_TEST_RING_LEN,
		ECPRI_DMA_ENDP_DIR_SRC);
	if (ret != 0) {
		DMA_UT_ERR("Fail to config first SRC ch ctx - err %d", ret);
		return ret;
	}

	/* Config Second SRC Channel Context */
	ret = ecpri_dma_mhi_client_test_config_channel_context(
		&mhi_client_test_suite_ctx[idx]->mmio_buf,
		mhi_client_test_suite_ctx[idx]->xfer_ring_bufs,
		mhi_client_test_suite_ctx[idx]->ev_ring_bufs,
		ecpri_dma_mhi_client_test_host_ch_id_map[1],
		ecpri_dma_mhi_client_test_ev_id_map[1],
		ECPRI_DMA_MHI_TEST_RING_LEN,
		ECPRI_DMA_MHI_TEST_RING_LEN,
		ECPRI_DMA_ENDP_DIR_SRC);
	if (ret) {
		DMA_UT_ERR("Fail to config second SRC ch ctx - err %d", ret);
		goto fail_destroy_frst_src_ch_ctx;
	}

	/* Config First DEST Channel Context */
	ret = ecpri_dma_mhi_client_test_config_channel_context(
		&mhi_client_test_suite_ctx[idx]->mmio_buf,
		mhi_client_test_suite_ctx[idx]->xfer_ring_bufs,
		mhi_client_test_suite_ctx[idx]->ev_ring_bufs,
		ecpri_dma_mhi_client_test_host_ch_id_map[2],
		ecpri_dma_mhi_client_test_ev_id_map[2],
		ECPRI_DMA_MHI_TEST_RING_LEN,
		ECPRI_DMA_MHI_TEST_RING_LEN,
		ECPRI_DMA_ENDP_DIR_DEST);
	if (ret) {
		DMA_UT_ERR("Fail to config first DEST ch ctx - err %d", ret);
		goto fail_destroy_scnd_src_ch_ctx;
	}

	/* Config Second DEST Channel Context */
	ret = ecpri_dma_mhi_client_test_config_channel_context(
		&mhi_client_test_suite_ctx[idx]->mmio_buf,
		mhi_client_test_suite_ctx[idx]->xfer_ring_bufs,
		mhi_client_test_suite_ctx[idx]->ev_ring_bufs,
		ecpri_dma_mhi_client_test_host_ch_id_map[3],
		ecpri_dma_mhi_client_test_ev_id_map[3],
		ECPRI_DMA_MHI_TEST_RING_LEN,
		ECPRI_DMA_MHI_TEST_RING_LEN,
		ECPRI_DMA_ENDP_DIR_DEST);
	if (ret) {
		DMA_UT_ERR("Fail to config second DEST ch ctx - err %d", ret);
		goto fail_destroy_first_dest_ch_ctx;
	}

	/* allocate DEST data buffer */
	mhi_client_test_suite_ctx[idx]->dest_buffer.size =
		ECPRI_DMA_MHI_TEST_BUFF_SIZE;
	mhi_client_test_suite_ctx[idx]->dest_buffer.virt_base = dma_alloc_coherent(
		ecpri_dma_ctx->pdev, mhi_client_test_suite_ctx[idx]->dest_buffer.size,
		&mhi_client_test_suite_ctx[idx]->dest_buffer.phys_base, GFP_KERNEL);
	if (!mhi_client_test_suite_ctx[idx]->dest_buffer.virt_base) {
		DMA_UT_ERR("no mem for dest data buffer\n");
		ret = -ENOMEM;
		goto fail_destroy_second_dest_ch_ctx;
	}

	memset(mhi_client_test_suite_ctx[idx]->dest_buffer.virt_base, 0,
		ECPRI_DMA_MHI_TEST_BUFF_SIZE);

	/* allocate SRC data buffer */
	mhi_client_test_suite_ctx[idx]->src_buffer.size =
		ECPRI_DMA_MHI_TEST_BUFF_SIZE;
	mhi_client_test_suite_ctx[idx]->src_buffer.virt_base = dma_alloc_coherent(
		ecpri_dma_ctx->pdev, mhi_client_test_suite_ctx[idx]->src_buffer.size,
		&mhi_client_test_suite_ctx[idx]->src_buffer.phys_base, GFP_KERNEL);
	if (!mhi_client_test_suite_ctx[idx]->src_buffer.virt_base) {
		DMA_UT_ERR("no mem for src data buffer\n");
		ret = -EFAULT;
		goto fail_destroy_dest_data_buf;
	}

	memset(mhi_client_test_suite_ctx[idx]->src_buffer.virt_base, 0,
		ECPRI_DMA_MHI_TEST_BUFF_SIZE);

	return 0;

fail_destroy_dest_data_buf:
	dma_free_coherent(ecpri_dma_ctx->pdev,
		mhi_client_test_suite_ctx[idx]->dest_buffer.size,
		mhi_client_test_suite_ctx[idx]->dest_buffer.virt_base,
		mhi_client_test_suite_ctx[idx]->dest_buffer.phys_base);
	mhi_client_test_suite_ctx[idx]->dest_buffer.virt_base = NULL;
fail_destroy_second_dest_ch_ctx:
	ecpri_dma_mhi_client_test_destroy_channel_context(
		mhi_client_test_suite_ctx[idx]->xfer_ring_bufs,
		mhi_client_test_suite_ctx[idx]->ev_ring_bufs,
		ecpri_dma_mhi_client_test_host_ch_id_map[3],
		ecpri_dma_mhi_client_test_ev_id_map[3]);
fail_destroy_first_dest_ch_ctx:
	ecpri_dma_mhi_client_test_destroy_channel_context(
		mhi_client_test_suite_ctx[idx]->xfer_ring_bufs,
		mhi_client_test_suite_ctx[idx]->ev_ring_bufs,
		ecpri_dma_mhi_client_test_host_ch_id_map[2],
		ecpri_dma_mhi_client_test_ev_id_map[2]);
fail_destroy_scnd_src_ch_ctx:
	ecpri_dma_mhi_client_test_destroy_channel_context(
		mhi_client_test_suite_ctx[idx]->xfer_ring_bufs,
		mhi_client_test_suite_ctx[idx]->ev_ring_bufs,
		ecpri_dma_mhi_client_test_host_ch_id_map[1],
		ecpri_dma_mhi_client_test_ev_id_map[1]);
fail_destroy_frst_src_ch_ctx:
	ecpri_dma_mhi_client_test_destroy_channel_context(
		mhi_client_test_suite_ctx[idx]->xfer_ring_bufs,
		mhi_client_test_suite_ctx[idx]->ev_ring_bufs,
		ecpri_dma_mhi_client_test_host_ch_id_map[0],
		ecpri_dma_mhi_client_test_ev_id_map[0]);
	return 0;
}

static int ecpri_dma_mhi_client_test_suite_setup(void** ppriv)
{
	int i;
	int j;
	int ret = 0;
	int vms_num = ECPRI_DMA_MHI_TEST_FUNCTIONS_TO_RUN;

	DMA_UT_DBG("Start Setup for %d VM(s)\n", vms_num);

	if (!ecpri_dma_ctx) {
		DMA_UT_ERR("ECPRI DMA ctx is not initialized\n");
		return -EINVAL;
	}

	/* Allocate test context array, slot for each VM/PF */
	for (i = 0; i < vms_num; i++) {
		mhi_client_test_suite_ctx[i] =
			kzalloc(sizeof(struct ecpri_dma_mhi_client_test_suite_context),
				GFP_KERNEL);
		if (!mhi_client_test_suite_ctx[i]) {
			DMA_UT_ERR("failed allocated ctx\n");
			ret = -ENOMEM;
			goto fail_alloc_ctx;
		}

		mhi_client_test_suite_ctx[i]->priv = i;

		ret = ecpri_dma_mhi_client_test_alloc_mmio_space(i);
		if (ret) {
			DMA_UT_ERR("failed to alloc mmio space");
			goto fail_alloc_mmio;
		}

		init_completion(
			&mhi_client_test_suite_ctx[i]->
			ecpri_dma_mhi_test_ready_comp);
		DMA_UT_DBG("Initiated ready completion for IDX: %d\n", i);

		init_completion(
			&mhi_client_test_suite_ctx[i]->
			ecpri_dma_mhi_test_async_comp);

		DMA_UT_DBG("Initiated async completion for IDX: %d\n", i);

		ret = ecpri_dma_mhi_client_test_setup_channels(i);
		if (ret) {
			DMA_UT_ERR("failed to alloc channels");
			goto fail_alloc_mmio;
		}
		DMA_UT_DBG("Allocated channels for idx IDX: %d\n", i);
	}

	return 0;

fail_alloc_mmio:
fail_alloc_ctx:
	for (j = 0; j < i; j++) {
		kfree(mhi_client_test_suite_ctx[j]);
		mhi_client_test_suite_ctx[j] = NULL;
	}

	return ret;
}

static int ecpri_dma_mhi_client_test_suite_teardown(void* priv)
{
	int i;
	int vms_num = ECPRI_DMA_MHI_TEST_FUNCTIONS_TO_RUN;

	DMA_UT_DBG("Start Teardown for %d VM(s)\n", vms_num);

	for (i = 0; i < vms_num; i++) {

		if (!mhi_client_test_suite_ctx[i])
			continue;

		ecpri_dma_mhi_client_test_destroy_data_context(i);
		ecpri_dma_mhi_client_test_free_mmio_space(i);
		kfree(mhi_client_test_suite_ctx[i]);
		mhi_client_test_suite_ctx[i] = NULL;
	}

	return 0;
}

static void ecpri_dma_mhi_client_test_utils_create_init_params(
	struct mhi_dma_init_params* init_params, int idx)
{
	memset(init_params, 0, sizeof(*init_params));

	init_params->msi.addr_low =
		mhi_client_test_suite_ctx[idx]->msi.phys_base;
	init_params->msi.data = ECPRI_DMA_MHI_TEST_MSI_DATA;
	init_params->msi.mask = ECPRI_DMA_MHI_TEST_MSI_MASK;
	init_params->first_ch_idx = ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID;
	init_params->first_er_idx = ECPRI_DMA_MHI_TEST_FIRST_EVENT_RING_ID;
	init_params->assert_bit40 = false;
	init_params->notify = ecpri_dma_mhi_client_test_cb;
	init_params->priv = &mhi_client_test_suite_ctx[idx]->priv;
	init_params->test_mode = true;
}

static int ecpri_dma_mhi_client_test_utils_invoke_init(
	struct mhi_dma_function_params function,
	struct mhi_dma_init_params* init_params,
	struct mhi_dma_init_out* out_params, int idx)
{
	int ret = 0;
	bool timeout = true;
	enum ecpri_dma_ees ee_idx = 0;
	u64 ch_db_base, ev_db_base;
	u32 expected_mask =
		function.function_type == MHI_DMA_FUNCTION_TYPE_PHYSICAL ?
		ECPRI_DMA_MHI_TEST_EXPECTED_CH_EV_MASK_PF :
		ECPRI_DMA_MHI_TEST_EXPECTED_CH_EV_MASK_VMS;

	DMA_UT_DBG("Running mhi_dma_init\n");
	ret = mhi_dma_init(function, init_params, out_params);
	if (ret) {
		DMA_UT_ERR("mhi_dma_init failed, RETURN CODE: %d\n", ret);
		DMA_UT_TEST_FAIL_REPORT("mhi_dma_init failed\n");
		return ret;
	}

	if (out_params->ch_db_fwd_msk !=
		expected_mask ||
		out_params->ev_db_fwd_msk !=
		expected_mask)
	{
		DMA_UT_ERR(
			"mhi_dma_init out params unexpected mask. "
			"expected:0x%x got ch 0x%x ev: 0x%x\n",
			expected_mask,
			out_params->ch_db_fwd_msk,
			out_params->ev_db_fwd_msk);
		DMA_UT_TEST_FAIL_REPORT("mhi_dma_init out params unexpected mask");
		return -EFAULT;
	}

	ret = ecpri_dma_mhi_client_test_utils_get_ee_index(function, &ee_idx);
	if (ret) {
		DMA_UT_ERR(
			"Return code %d\n", ret);
		DMA_UT_TEST_FAIL_REPORT("get_ee_index failed\n");
		return ret;
	}

	ch_db_base = gsihal_get_reg_nk_addr(
		GSI_EE_n_GSI_CH_k_DOORBELL_0, ee_idx, 0);
	ev_db_base = gsihal_get_reg_nk_addr(
		GSI_EE_n_EV_CH_k_DOORBELL_0, ee_idx, 0);

	if (out_params->ch_db_fwd_base !=
		ch_db_base ||
		out_params->ev_db_fwd_base !=
		ev_db_base)
	{
		DMA_UT_ERR(
			"mhi_dma_init out params unexpected db base."
			"expected ch:0x%x expected ev:0x%x got ch 0x%x ev: 0x%x\n",
			ch_db_base,
			ev_db_base,
			out_params->ch_db_fwd_base,
			out_params->ev_db_fwd_base);
		DMA_UT_TEST_FAIL_REPORT("mhi_dma_init out params unexpected db base");
		return -EFAULT;
	}

	DMA_UT_DBG("Wait DMA start event\n");
	ecpri_dma_mhi_test_poll_for_start(idx, &timeout);
	if (timeout)
	{
		DMA_UT_ERR("timeout waiting for start event");
		DMA_UT_TEST_FAIL_REPORT("failed waiting for start state");
		return -ETIME;
	}
	DMA_UT_DBG("DMA is started\n");
	return ret;
}

static void ecpri_dma_mhi_client_test_utils_create_start_params(
	struct mhi_dma_start_params* start_params, int idx)
{
	memset(start_params, 0, sizeof(*start_params));
	start_params->channel_context_array_addr =
		mhi_client_test_suite_ctx[idx]->ch_ctx_array.phys_base;
	start_params->event_context_array_addr =
		mhi_client_test_suite_ctx[idx]->ev_ctx_array.phys_base;
}

static int ecpri_dma_mhi_client_test_utils_check_driver_state(
	int idx,
	struct mhi_dma_init_params* init_params,
	struct ecpri_dma_mhi_client_context* mhi_dma_ctx,
	struct mhi_dma_function_params* function)
{
	int ret = 0;
	/* Check DMA driver state matches the expectations */
	mhi_dma_ctx = ecpri_dma_mhi_client_ctx[idx];
	if (!mhi_dma_ctx) {
		DMA_UT_ERR("VM ID: %d\n",
			function->vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"context isn't initialized\n");
		return -EFAULT;
	}
	if (mhi_dma_ctx->notify_cb
		!= init_params->notify) {
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"mismatch of ready cb address\n");
		return -EFAULT;
	}

	/* Check that MHI mock MSI info and DMA driver info the same */
	if (mhi_dma_ctx->msi_config.addr_low
		!= init_params->msi.addr_low) {
		DMA_UT_ERR("Context addr_low: %d,"
			"Test addr_low: %d\n",
			mhi_dma_ctx->msi_config.addr_low,
			init_params->msi.addr_low);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"mismatch of MSI adrr_low parameter\n");
		return -EFAULT;
	}

	if (mhi_dma_ctx->msi_config.addr_hi
		!= init_params->msi.addr_hi) {
		DMA_UT_ERR("Test failed due to "
			"mismatch of MSI adrr_hi parameter,"
			"Context addr_hi: %d,"
			"Test addr_hi: %d\n",
			mhi_dma_ctx->msi_config.addr_hi,
			init_params->msi.addr_hi);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"mismatch of MSI adrr_hi parameter");
		return -EFAULT;
	}

	if (mhi_dma_ctx->msi_config.data
		!= init_params->msi.data) {
		DMA_UT_ERR("Test failed due to "
			"mismatch of MSI data parameter,"
			"Context data: %X,"
			"Test data: %X\n",
			mhi_dma_ctx->msi_config.data,
			init_params->msi.data);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"mismatch of MSI data parameter");
		return -EFAULT;
	}

	if (mhi_dma_ctx->msi_config.mask
		!= init_params->msi.mask) {
		DMA_UT_ERR("Test failed due to "
			"mismatch of MSI mask parameter,"
			"Context mask: %X,"
			"Test mask: %X\n",
			mhi_dma_ctx->msi_config.mask,
			init_params->msi.mask);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"mismatch of MSI mask parameter");
		return -EFAULT;
	}

	/* Check that MHI mock ch array and ev array */
	if (ecpri_dma_mhi_client_ctx[idx]->channel_context_array_addr !=
		mhi_client_test_suite_ctx[idx]->ch_ctx_array.phys_base) {
		DMA_UT_ERR("Test failed due to "
			"mismatch channel_context_array_addr,"
			"Driver has: %X,"
			"Test has: %X\n",
			ecpri_dma_mhi_client_ctx[idx]->channel_context_array_addr,
			&mhi_client_test_suite_ctx[idx]->ch_ctx_array.phys_base);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"mismatch channel_context_array_addr");
		return -EFAULT;
	}

	if (ecpri_dma_mhi_client_ctx[idx]->event_context_array_addr !=
		mhi_client_test_suite_ctx[idx]->ev_ctx_array.phys_base) {
		DMA_UT_ERR("Test failed due to "
			"mismatch event_context_array_addr,"
			"Driver has: %X,"
			"Test has: %X\n",
			ecpri_dma_mhi_client_ctx[idx]->channel_context_array_addr,
			&mhi_client_test_suite_ctx[idx]->ch_ctx_array.phys_base);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"mismatch event_context_array_addr");
		return -EFAULT;
	}
	return ret;
}

static int ecpri_dma_mhi_client_test_utils_check_memcpy_init_state(
	int idx,
	struct ecpri_dma_mhi_memcpy_context* memcpy_ctx,
	struct mhi_dma_function_params* function)
{
	int ret = 0;

	/* Check DMA driver state matches the expectations */
	memcpy_ctx = ecpri_dma_mhi_memcpy_ctx[idx];
	if (!memcpy_ctx) {
		DMA_UT_ERR("Test failed due to "
			"context for the VM%d isn't initialized\n",
			function->vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"context isn't initialized");
		return -EFAULT;
	}

	if (memcpy_ctx->destroy_pending != false) {
		DMA_UT_ERR("Test failed due to "
			"memcpy_ctx->destroy_pending %d, VF_ID%d\n",
			memcpy_ctx->destroy_pending, function->vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"memcpy_ctx->destroy_pending");
		return -EFAULT;
	}
	DMA_UT_DBG("Checking memcpy_ctx->destroy_pending - OK\n");

	/* Each successful memcpy_init increases ref_count */
	if (atomic_read(&memcpy_ctx->ref_count) != 1) {
		DMA_UT_ERR("Test failed due to "
			"memcpy_ctx->ref_count %d, VF_ID%d\n",
			atomic_read(&memcpy_ctx->ref_count), function->vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"memcpy_ctx->ref_count");
		return -EFAULT;
	}
	DMA_UT_DBG("Checking memcpy_ctx->ref_count - OK\n");

	if (atomic_read(&memcpy_ctx->async_pending) != 0) {
		DMA_UT_ERR("Test failed due to "
			"memcpy_ctx->async_pending %d, VF_ID%d\n",
			atomic_read(&memcpy_ctx->async_pending), function->vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"memcpy_ctx->async_pending");
		return -EFAULT;
	}
	DMA_UT_DBG("Checking memcpy_ctx->async_pending - OK\n");

	if (atomic_read(&memcpy_ctx->sync_pending) != 0) {
		DMA_UT_ERR("Test failed due to "
			"memcpy_ctx->sync_pending %d, VF_ID%d\n",
			atomic_read(&memcpy_ctx->sync_pending), function->vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"memcpy_ctx->sync_pending");
		return -EFAULT;
	}
	DMA_UT_DBG("Checking memcpy_ctx->sync_pending - OK\n");

	if (atomic_read(&memcpy_ctx->sync_total) != 0) {
		DMA_UT_ERR("Test failed due to "
			"memcpy_ctx->sync_total %d, VF_ID%d\n",
			atomic_read(&memcpy_ctx->sync_total), function->vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"memcpy_ctx->sync_total");
		return -EFAULT;
	}
	DMA_UT_DBG("Checking memcpy_ctx->sync_total - OK\n");

	if (atomic_read(&memcpy_ctx->async_total) != 0) {
		DMA_UT_ERR("Test failed due to "
			"memcpy_ctx->async_total %d, VF_ID%d\n",
			atomic_read(&memcpy_ctx->async_total), function->vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test failed due to "
			"memcpy_ctx->async_total");
		return -EFAULT;
	}
	DMA_UT_DBG("Checking memcpy_ctx->async_total - OK\n");

	return ret;
}

static void ecpri_dma_mhi_client_test_utils_create_all_functions(
	struct mhi_dma_function_params* function)
{
	int i;
	for (i = 0; i < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM; i++)
	{
		if (i < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM - 1) {
			function[i].function_type =
				MHI_DMA_FUNCTION_TYPE_VIRTUAL;
			function[i].vf_id = ECPRI_DMA_VM_IDS_VM0 + i;
			DMA_UT_DBG("Preparing function params for IDX: %d, VF_ID %d,"
				"type %d\n",
				i, function[i].vf_id, ECPRI_DMA_VM_IDS_VM0 + i);
		}
		else {
			function[i].function_type =
				MHI_DMA_FUNCTION_TYPE_PHYSICAL;
			function[i].vf_id = ECPRI_DMA_MHI_PF_ID;
			DMA_UT_DBG("Preparing function params for IDX: %d, VF_ID %d\n",
				i, function[i].vf_id, ECPRI_DMA_MHI_PF_ID);
		}
	}
	DMA_UT_DBG("Function params are prepared successfully\n");
}

static void ecpri_dma_mhi_client_test_utils_create_conn_params(
	struct mhi_dma_connect_params* params, int idx,
	int buff_size, int channel_id)
{
	params->channel_id = ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID + channel_id;
	mhi_client_test_suite_ctx[idx]->channel_ids[channel_id] = channel_id;
	params->int_modt = 0;
	params->int_modc = 1;
	params->buff_size = buff_size;
	params->desc_fifo_sz = ECPRI_DMA_MHI_MEMCPY_RLEN;
	params->disable_overflow_event = false;
}

static inline void ecpri_dma_mhi_test_create_func_params(
	struct mhi_dma_function_params* function,
	enum mhi_dma_function_type function_type, u8 vf_id)
{
	memset(function, 0, sizeof(*function));
	function->function_type = function_type;
	function->vf_id = vf_id;
}

static inline int ecpri_dma_mhi_test_get_func_idx(
	struct mhi_dma_function_params* function, int* idx)
{
	int ret = 0;
	ret = ecpri_dma_mhi_client_test_get_funct_ctx_idx(function, idx);

	if (ret != 0) {
		DMA_UT_ERR("Function params are invalid,"
			"function type: %d, vf_id: %d\n",
			function->function_type, function->vf_id);
		return -EINVAL;
	}

	return ret;
}

static int ecpri_dma_mhi_client_test_utils_create_params_and_init(
	struct mhi_dma_function_params* function,
	struct mhi_dma_init_params* init_params,
	struct mhi_dma_init_out* out_params,
	struct mhi_dma_start_params* start_params)
{
	int idx;
	int ret = 0;

	DMA_UT_DBG("=============================== "
		"Starting init =============================== \n");

	/* Get VF index */
	ret = ecpri_dma_mhi_test_get_func_idx(function, &idx);
	if (ret != 0) {
		return ret;
	}

	/* Init params */
	DMA_UT_DBG("Init MHI with init params\n");
	ecpri_dma_mhi_client_test_utils_create_init_params(
		init_params, idx);

	/* Start params */
	DMA_UT_DBG("Start MHI with start_params for VF_ID %d\n",
		function->vf_id);
	ecpri_dma_mhi_client_test_utils_create_start_params(
		start_params, idx);

	mhi_client_test_suite_ctx[idx]->function = *function;
	mhi_client_test_suite_ctx[idx]->init_params = *init_params;
	mhi_client_test_suite_ctx[idx]->start_params = *start_params;

	/* Init */
	DMA_UT_DBG("mhi_client_init with init params\n");
	ret = ecpri_dma_mhi_client_test_utils_invoke_init(
		*function, init_params, out_params, idx);
	if (ret != 0) {
		DMA_UT_ERR("Init for VF_ID %d - Failed\n", function->vf_id);
		return -EPERM;
	}

	DMA_UT_DBG("=============================== Init for VF_ID %d SUCCESS"
		" =============================== \n", function->vf_id);
	return ret;
}

/*
* ecpri_dma_mhi_utils_connect_and_verify_endps() - Connects SRC and DEST endps,
* verifies connection succeeded.
* @function: function params of the given VF
* @src_conn_params: SRC endp connection parameters
* @dest_conn_params: DEST endp connection parameters
* @src_disc_params: SRC endp disconnect parameters
* @dest_disc_params: DEST endp disconnect parameters
* @idx: Relative index of VF
* @dev_src_ch_id: Device SRC channel ID
* @dev_dest_ch_id: Device DEST channel ID
* @src_endp_id: SRC endp ID
* @dest_endp_id: DEST nedp ID
*
*/
static int ecpri_dma_mhi_utils_connect_and_verify_endps(
	struct mhi_dma_function_params* function,
	struct mhi_dma_connect_params* src_conn_params,
	struct mhi_dma_connect_params* dest_conn_params,
	u32* src_disc_clnt_hdl,
	u32* dest_disc_clnt_hdl,
	int idx, int dev_src_ch_id, int dev_dest_ch_id,
	int src_endp_id, int dest_endp_id)
{
	int ret = 0;

	/* Create connect_params for SRC ENDP */
	DMA_UT_DBG("First SRC ENDP Connect params for CH_ID %d\n",
		dev_src_ch_id);
	ecpri_dma_mhi_client_test_utils_create_conn_params(
		src_conn_params, idx,
		ECPRI_DMA_MHI_TEST_EMPTY_BUFF, dev_src_ch_id);

	/* Create connect_params for DEST ENDP */
	DMA_UT_DBG("First DEST ENDP Connect params for CH_ID %d\n",
		dev_dest_ch_id);
	ecpri_dma_mhi_client_test_utils_create_conn_params(
		dest_conn_params, idx,
		ECPRI_DMA_MHI_TEST_BUFF_SIZE, dev_dest_ch_id);

	/*Connect SRC ENDP */
	ret = mhi_dma_connect_endp(*function,
		src_conn_params,
		src_disc_clnt_hdl);
	if (ret != 0) {
		DMA_UT_ERR("Connect endp for SRC, VF_ID %d has failed\n",
			function->vf_id);
		return -EPERM;
	}

	/* Connect DEST ENDP*/
	ret = mhi_dma_connect_endp(*function,
		dest_conn_params,
		dest_disc_clnt_hdl);
	if (ret != 0) {
		DMA_UT_ERR("Connect endp for DEST, VF_ID %d has failed\n",
			function->vf_id);
		return -EPERM;
	}

	/* Verify matching ENDPs & CHs */
	DMA_UT_DBG("First SRC & DEST ENDP verification\n");
	ret = ecpri_dma_mhi_client_test_verify_connect(
		function,
		src_endp_id,
		dest_endp_id,
		dev_src_ch_id,
		dev_dest_ch_id,
		idx);
	if (ret != 0) {
		DMA_UT_ERR("Verification for VF_ID %d has failed\n",
			function->vf_id);
		return -EPERM;
	}

	return ret;
}

/**
 * Function generates TRE and data and enqueues them to correct GSI ring.
 */
static int ecpri_dma_mhi_test_q_transfer_re(
	struct ecpri_dma_mem_buffer* mmio,
	struct ecpri_dma_mem_buffer xfer_ring_bufs[],
	struct ecpri_dma_mem_buffer ev_ring_bufs[],
	u8 host_ch_id,
	struct ecpri_dma_mem_buffer buffer,
	enum ecpri_dma_ees ee)
{
	struct gsi_tre* curr_re;
	struct ecpri_dma_mhi_mmio_register_set* p_mmio;
	struct ecpri_dma_mhi_host_ch_ctx* host_channels;
	struct ecpri_dma_mhi_host_ev_ctx* host_events;
	u32 device_ch_idx;
	u32 device_ev_idx;
	u32 wp_ofst;
	u32 rp_ofst;
	u32 avail_ev;
	u32 next_wp_ofst;

	DMA_UT_DBG("Entry for host CH  id %d ee %d\n", host_ch_id, ee);

	p_mmio = (struct ecpri_dma_mhi_mmio_register_set*)mmio->virt_base;
	host_channels = (struct ecpri_dma_mhi_host_ch_ctx*)
		((unsigned long)p_mmio->crcbap);
	host_events = (struct ecpri_dma_mhi_host_ev_ctx*)
		((unsigned long)p_mmio->crdb);

	if (host_ch_id >=
		(ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID + ECPRI_DMA_MHI_MAX_HW_CHANNELS) ||
		host_ch_id < ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID) {
		DMA_UT_DBG("Invalid Channel ID %d\n", host_ch_id);
		return -EFAULT;
	}

	device_ch_idx = host_ch_id - ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID;
	if (!xfer_ring_bufs[device_ch_idx].virt_base) {
		DMA_UT_DBG("Channel is not allocated\n");
		return -EFAULT;
	}

	/* First queue EDs */
	device_ev_idx = host_channels[host_ch_id].erindex;

	wp_ofst = (u32)(host_events[device_ev_idx].wp -
		host_events[device_ev_idx].rbase);
	rp_ofst = (u32)(host_events[device_ev_idx].rp -
		host_events[device_ev_idx].rbase);

	if (host_events[device_ev_idx].rlen & 0xFFFFFFFF00000000) {
		DMA_UT_DBG("invalid ev rlen %llu\n",
			host_events[device_ev_idx].rlen);
		return -EFAULT;
	}

	if (wp_ofst > rp_ofst) {
		avail_ev = (wp_ofst - rp_ofst) /
			sizeof(struct gsi_xfer_compl_evt);
	}
	else {
		avail_ev = (u32)host_events[device_ev_idx].rlen -
			(rp_ofst - wp_ofst);
		avail_ev /= sizeof(struct gsi_xfer_compl_evt);
	}

	DMA_UT_DBG("Event wp_ofst=0x%x rp_ofst=0x%x rlen=%llu avail_ev=%u\n",
		wp_ofst, rp_ofst, host_events[device_ev_idx].rlen, avail_ev);

	/* calculate virtual pointer for current WP and RP */
	wp_ofst = (u32)(host_channels[host_ch_id].wp -
		host_channels[host_ch_id].rbase);
	rp_ofst = (u32)(host_channels[host_ch_id].rp -
		host_channels[host_ch_id].rbase);

	curr_re = (struct gsi_tre*)
		((unsigned long)xfer_ring_bufs[device_ch_idx].virt_base +
			wp_ofst);
	if (host_channels[host_ch_id].rlen & 0xFFFFFFFF00000000) {
		DMA_UT_DBG("invalid ch rlen %llu\n",
			host_channels[host_ch_id].rlen);
		return -EFAULT;
	}

	next_wp_ofst = (wp_ofst +
		sizeof(struct gsi_tre)) %
		(u32)host_channels[host_ch_id].rlen;

	/* write current RE */
	curr_re->re_type = GSI_RE_XFER;
	curr_re->buf_len = (u16)buffer.size;
	curr_re->buffer_ptr = (u32)buffer.phys_base;
	curr_re->bei = false;
	curr_re->ieob = true;
	curr_re->ieot = true;
	curr_re->chain = 0;

	/* set next WP */
	host_channels[host_ch_id].wp =
		host_channels[host_ch_id].rbase + next_wp_ofst;

	/* Ring CH DB */
	gsihal_write_reg_nk(
		GSI_EE_n_GSI_CH_k_DOORBELL_0,
		ee, device_ch_idx,
		host_channels[host_ch_id].wp);

	DMA_UT_DBG("exit\n");

	return 0;
}

static void ecpri_dma_mhi_test_check_msi_intr(int both,
	int src_endp_id, int dest_endp_id,
	int idx, bool* timeout)
{
	int i = 0;
	int dev_src_endp_id = src_endp_id - ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID;
	int dev_dest_endp_id = dest_endp_id - ECPRI_DMA_MHI_TEST_FIRST_HW_CH_ID;

	/* GSI will add the first channel Id only if is_over_pcie flag is set */
	while (i < ECPRI_DMA_MHI_TEST_MSI_INTRPT_LOOPS) {
		if (*((u32*)mhi_client_test_suite_ctx[idx]->msi.virt_base) ==
			(ECPRI_DMA_MHI_TEST_MSI_DATA | dev_src_endp_id)) {
			*timeout = false;
			break;
		}
		if (both && (*((u32*)mhi_client_test_suite_ctx[idx]->msi.virt_base) ==
			(ECPRI_DMA_MHI_TEST_MSI_DATA | dev_dest_endp_id))) {
			/* sleep to be sure DEST MSI is generated */
			msleep(20);
			*timeout = false;
			break;
		}
		msleep(20);
		i++;
	}
}

/**
 * Generates TRE and data and enqueues them to correct GSI ring.
 * Sends data using loopback.
 */
static int ecpri_dma_mhi_test_loopback_data_transfer(int idx,
	int host_src_ch_id, int host_dest_ch_id, enum ecpri_dma_ees ee)
{
	u64 dest_orig_rp;
	u64 dest_new_rp;
	u64 src_orig_rp;
	u64 src_new_rp;
	struct ecpri_dma_mem_buffer* mmio;
	struct ecpri_dma_mhi_mmio_register_set* p_mmio;
	int i;
	int ret;
	static int val;
	bool timeout = true;
	u32 dest_device_ev_idx;
	u32 src_device_ev_idx;
	struct ecpri_dma_mhi_host_ch_ctx* host_channels;
	struct ecpri_dma_mhi_host_ev_ctx* host_events;

	DMA_UT_DBG("Entry  host_src_ch_id%d  host_dest_ch_id %d ee %d\n",
		host_src_ch_id, host_dest_ch_id, ee);

	mmio = &mhi_client_test_suite_ctx[idx]->mmio_buf;
	p_mmio = (struct ecpri_dma_mhi_mmio_register_set*)mmio->virt_base;
	host_channels = (struct ecpri_dma_mhi_host_ch_ctx*)
		((unsigned long)p_mmio->crcbap);
	host_events = (struct ecpri_dma_mhi_host_ev_ctx*)
		((unsigned long)p_mmio->crdb);

	/* invalidate MSI Interrupt value */
	memset(mhi_client_test_suite_ctx[idx]->msi.virt_base,
		0xFF,
		mhi_client_test_suite_ctx[idx]->msi.size);

	/* Generates different packet content values for each re-entry*/
	val++;

	/* Prepare packet */
	memset(mhi_client_test_suite_ctx[idx]->dest_buffer.virt_base, 0,
		ECPRI_DMA_MHI_TEST_BUFF_SIZE);
	for (i = 0; i < ECPRI_DMA_MHI_TEST_BUFF_SIZE; i++)
		memset(mhi_client_test_suite_ctx[idx]->src_buffer.virt_base + i,
			(val + i) & 0xFF, 1);

	dest_device_ev_idx = host_channels[host_dest_ch_id].erindex;
	dest_orig_rp = host_events[dest_device_ev_idx].rp;

	/* queue RE for DEST side and trigger doorbell */
	ret = ecpri_dma_mhi_test_q_transfer_re(mmio,
		mhi_client_test_suite_ctx[idx]->xfer_ring_bufs,
		mhi_client_test_suite_ctx[idx]->ev_ring_bufs,
		host_dest_ch_id,
		mhi_client_test_suite_ctx[idx]->dest_buffer, ee);
	if (ret) {
		DMA_UT_DBG("q_transfer_re failed %d\n", ret);
		DMA_UT_TEST_FAIL_REPORT("fail DEST q xfer re");
		return ret;
	}

	src_device_ev_idx = host_channels[host_src_ch_id].erindex;
	src_orig_rp = host_events[src_device_ev_idx].rp;

	/* queue REs for SRC side and trigger doorbell */
	ret = ecpri_dma_mhi_test_q_transfer_re(mmio,
		mhi_client_test_suite_ctx[idx]->xfer_ring_bufs,
		mhi_client_test_suite_ctx[idx]->ev_ring_bufs,
		host_src_ch_id,
		mhi_client_test_suite_ctx[idx]->src_buffer, ee);
	if (ret) {
		DMA_UT_DBG("q_transfer_re failed %d\n", ret);
		DMA_UT_TEST_FAIL_REPORT("fail SRC q xfer re");
		return ret;
	}

	ecpri_dma_mhi_test_check_msi_intr(true, host_src_ch_id,
		host_dest_ch_id, idx, &timeout);
	if (timeout) {
		DMA_UT_DBG("transfer timeout. MSI = 0x%x\n",
			*((u32*)mhi_client_test_suite_ctx[idx]->msi.virt_base));
		DMA_UT_TEST_FAIL_REPORT("xfter timeout");
		return -EFAULT;
	}

	dest_new_rp = host_events[dest_device_ev_idx].rp;
	src_new_rp = host_events[src_device_ev_idx].rp;

	/* Verify RP for both channels */
	if (((dest_new_rp - dest_orig_rp) != sizeof(struct gsi_xfer_compl_evt)) ||
		((src_new_rp - src_orig_rp) != sizeof(struct gsi_xfer_compl_evt))) {
		DMA_UT_DBG("RP failure\n");
		DMA_UT_TEST_FAIL_REPORT("RP should advance in offset of one TRE");
		return -EFAULT;
	}

	/* compare the two buffers */
	if (memcmp(mhi_client_test_suite_ctx[idx]->dest_buffer.virt_base,
		mhi_client_test_suite_ctx[idx]->src_buffer.virt_base,
		ECPRI_DMA_MHI_TEST_BUFF_SIZE)) {
		DMA_UT_DBG("buffer are not equal\n");
		DMA_UT_TEST_FAIL_REPORT("non-equal buffers after xfer");
		return -EFAULT;
	}

	return 0;
}

static int ecpri_dma_mhi_client_test_suite_utils_disconnect_endps(
	struct mhi_dma_function_params* function,
	struct mhi_dma_disconnect_params* frst_src_disc_params,
	struct mhi_dma_disconnect_params* frst_dest_disc_params,
	struct mhi_dma_disconnect_params* second_src_disc_params,
	struct mhi_dma_disconnect_params* second_dest_disc_params)
{
	int ret = 0;

	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		/* Disconnect first SRC&DEST pair */
		ret = ecpri_dma_mhi_driver_ops.mhi_dma_disconnect_endp(*function,
			frst_src_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Unable to disconnect FIRST SRC, ret = %d\n",
				ret);
			return -EPERM;
		}

		ret = ecpri_dma_mhi_driver_ops.mhi_dma_disconnect_endp(*function,
			frst_dest_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Unable to disconnect FIRST DEST, ret = %d\n",
				ret);
			return -EPERM;
		}

		/* Disconnect second SRC&DEST pair */
		ret = ecpri_dma_mhi_driver_ops.mhi_dma_disconnect_endp(*function,
			second_src_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Unable to disconnect SECOND SRC, ret = %d\n",
				ret);
			return -EPERM;
		}

		ret = ecpri_dma_mhi_driver_ops.mhi_dma_disconnect_endp(*function,
			second_dest_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Unable to disconnect SECOND DEST, ret = %d\n",
				ret);
			return -EPERM;
		}
	}
	else {
		ret = mhi_dma_disconnect_endp(*function,
			frst_src_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Unable to disconnect FIRST SRC, ret = %d\n",
				ret);
			return -EPERM;
		}

		ret = mhi_dma_disconnect_endp(*function,
			frst_dest_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Unable to disconnect FIRST DEST, ret = %d\n",
				ret);
			return -EPERM;
		}

		ret = mhi_dma_disconnect_endp(*function,
			second_src_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Unable to disconnect SECOND SRC, ret = %d\n",
				ret);
			return -EPERM;
		}

		ret = mhi_dma_disconnect_endp(*function,
			second_dest_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Unable to disconnect SECOND DEST, ret = %d\n",
				ret);
			return -EPERM;
		}
	}

	return ret;
}

static int ecpri_dma_mhi_client_test_suite_mhi_init(void* priv)
{
	int ret = 0;
	int idx;
	struct mhi_dma_function_params function;
	struct mhi_dma_init_params init_params;
	struct mhi_dma_init_out out_params;
	struct mhi_dma_start_params start_params;
	struct ecpri_dma_mhi_client_context* mhi_dma_ctx = NULL;

	DMA_UT_DBG("Start MHI Init\n");

	/* Create function params for VM0 */
	ecpri_dma_mhi_test_create_func_params(&function,
		MHI_DMA_FUNCTION_TYPE_VIRTUAL, ECPRI_DMA_VM_IDS_VM0);

	/* Get VF index */
	ret = ecpri_dma_mhi_test_get_func_idx(&function, &idx);
	if (ret != 0) {
		return ret;
	}

	mhi_client_test_suite_ctx[idx]->function = function;

	/* Register driver */
	ret = ecpri_dma_mhi_client_test_utils_create_params_and_init(
		&function, &init_params, &out_params, &start_params);
	if (ret != 0) {
		DMA_UT_ERR("VF_ID %d\n", function.vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test has failed\n");
		return -EFAULT;
	}

	/* Check driver state */
	ret = ecpri_dma_mhi_client_test_utils_check_driver_state(
		idx, &init_params, mhi_dma_ctx, &function);
	if (ret != 0) {
		DMA_UT_ERR("Driver state for VF_ID %d / IDX %d has failed\n",
			function.vf_id, idx);
		return -EPERM;
	}

	/* Clean Up */
	mhi_dma_destroy(function);

	if (ecpri_dma_mhi_client_ctx[idx] != NULL) {
		DMA_UT_ERR("IDX %d\n", idx);
		DMA_UT_TEST_FAIL_REPORT("MHI ctx was not freed\n");
		return -EFAULT;
	}

	DMA_UT_DBG("MHI Init for VM %d finished\n", function.vf_id);

	return ret;
}

static int ecpri_dma_mhi_client_test_suite_mhi_init_all_vms(void* priv)
{
	int test_i;
	int ret = 0;
	struct mhi_dma_function_params function[ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM];
	struct mhi_dma_init_params init_params[ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM];
	struct mhi_dma_init_out out_params[ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM];
	struct mhi_dma_start_params start_params[ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM];
	struct ecpri_dma_mhi_client_context* mhi_dma_ctx = NULL;

	DMA_UT_DBG("Start MHI Init for all VMs\n");
	DMA_UT_DBG("Preparing function params\n");

	/* Create function params */
	ecpri_dma_mhi_client_test_utils_create_all_functions(function);

	/* Run tests for all 4 VMs: [0, 3] and PF */
	for (test_i = 0; test_i < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM; test_i++) {
		ret = ecpri_dma_mhi_client_test_utils_create_params_and_init(
			&function[test_i], &init_params[test_i], &out_params[test_i],
			&start_params[test_i]);
		if (ret != 0) {
			DMA_UT_ERR("VF_ID %d\n", function[test_i].vf_id);
			DMA_UT_TEST_FAIL_REPORT("Test has failed\n");
			return -EFAULT;
		}
	}

	/* Compare driver states for all VMs/PF */
	for (test_i = 0; test_i < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM; test_i++) {
		DMA_UT_DBG("Starting DMA driver check for VF_ID %d\n",
			function[test_i].vf_id);
		ret = ecpri_dma_mhi_client_test_utils_check_driver_state(
			test_i, &init_params[test_i], mhi_dma_ctx, &function[test_i]);
		if (ret != 0) {
			DMA_UT_ERR("Driver state for VF_ID %d / IDX %d has failed\n",
				function[test_i].vf_id, test_i);
			return -EPERM;
		}

		DMA_UT_DBG("DMA driver context was checked successfully for VF_ID %d\n",
			function[test_i].vf_id);
	}

	for (test_i = 0; test_i < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM; test_i++) {
		mhi_dma_destroy(function[test_i]);
	}

	return ret;
}

static int ecpri_dma_mhi_client_test_suite_vm_memcpy_init(void* priv)
{
	int idx;
	int ret = 0;
	struct mhi_dma_function_params function;
	struct ecpri_dma_mhi_memcpy_context* memcpy_ctx = NULL;

	DMA_UT_DBG("Start MHI MEMCPY Init\n");

	/* Create func params for V0 */
	ecpri_dma_mhi_test_create_func_params(&function,
		MHI_DMA_FUNCTION_TYPE_VIRTUAL, ECPRI_DMA_VM_IDS_VM0);

	/* Get the index of VM/PF */
	ret = ecpri_dma_mhi_test_get_func_idx(&function, &idx);
	if (ret != 0) {
		return ret;
	}

	/* Invoke memcpy_init */
	ret = mhi_dma_memcpy_init(function);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed to init memcpy\n");
		return -EFAULT;
	}

	ret = mhi_dma_memcpy_enable(function);
	if (ret != 0) {
		DMA_UT_ERR("Failed to enable memcpy\n");
		ret = -EFAULT;
		goto fail;
	}

	if (!ecpri_dma_mhi_memcpy_ctx[idx]) {
		DMA_UT_ERR("memcpy_ctx for idx: %d, and"
			"function type: %d, vf_id: %d is not initialised\n",
			function.function_type, function.vf_id);
		ret = -EINVAL;
		goto fail_memcpy_enable;
	}
	memcpy_ctx = ecpri_dma_mhi_memcpy_ctx[idx];

	ret = ecpri_dma_mhi_client_test_utils_check_memcpy_init_state(
		idx, memcpy_ctx, &function);
	if (ret != 0) {
		DMA_UT_ERR("Driver state for VF_ID %d / IDX %d has failed\n",
			function.vf_id, idx);
		ret = -EPERM;
	}

fail_memcpy_enable:
	ret = mhi_dma_memcpy_disable(function);
	if (ret != 0) {
		DMA_UT_ERR("Unable to disable memcpy\n");
		ret = -EPERM;
	}
fail:
	mhi_dma_memcpy_destroy(function);

	return ret;
}

static int ecpri_dma_mhi_client_test_suite_pf_memcpy_init(void* priv)
{
	int idx;
	int ret = 0;
	struct mhi_dma_function_params function;
	struct ecpri_dma_mhi_memcpy_context* memcpy_ctx = NULL;

	DMA_UT_DBG("Start MHI MEMCPY Init\n");

	/* Create func params for PF */
	ecpri_dma_mhi_test_create_func_params(&function,
		MHI_DMA_FUNCTION_TYPE_PHYSICAL, ECPRI_DMA_MHI_PF_ID);

	/* Get the index of VM/PF */
	ret = ecpri_dma_mhi_test_get_func_idx(&function, &idx);
	if (ret != 0) {
		return ret;
	}

	/* Invoke memcpy_init */
	ret = mhi_dma_memcpy_init(function);
	if (ret != 0) {
		DMA_UT_TEST_FAIL_REPORT("Failed to init memcpy\n");
		return -EFAULT;
	}

	ret = mhi_dma_memcpy_enable(function);
	if (ret != 0) {
		DMA_UT_ERR("Failed to enable memcpy\n");
		ret = -EFAULT;
		goto fail;
	}

	if (!ecpri_dma_mhi_memcpy_ctx[idx]) {
		DMA_UT_ERR("memcpy_ctx for idx: %d, and"
			"function type: %d, vf_id: %d is not initialised\n",
			function.function_type, function.vf_id);
		ret = -EINVAL;
		goto fail_memcpy_enable;
	}
	memcpy_ctx = ecpri_dma_mhi_memcpy_ctx[idx];

	ret = ecpri_dma_mhi_client_test_utils_check_memcpy_init_state(
		idx, memcpy_ctx, &function);
	if (ret != 0) {
		DMA_UT_ERR("Driver state for VF_ID %d / IDX %d has failed\n",
			function.vf_id, idx);
		ret = -EPERM;
	}

fail_memcpy_enable:
	ret = mhi_dma_memcpy_disable(function);
	if (ret != 0) {
		DMA_UT_ERR("Unable to disable memcpy\n");
		ret = -EPERM;
	}
fail:
	mhi_dma_memcpy_destroy(function);

	return ret;
}

static int ecpri_dma_mhi_client_test_suite_memcpy_init_all_vms_pf(void* priv)
{
	int j;
	int ret = 0;
	int test_i;
	struct mhi_dma_function_params function[ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM];
	struct ecpri_dma_mhi_memcpy_context* mhi_memcpy_ctx = NULL;

	DMA_UT_DBG("Start MEMCPY_INIT ALL\n");
	DMA_UT_DBG("Preparing function params\n");

	/* Create function params */
	ecpri_dma_mhi_client_test_utils_create_all_functions(function);

	/* Run tests 4 for VMs: [0, 3] and PF: 4*/
	for (test_i = 0; test_i < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM; test_i++) {
		ret = mhi_dma_memcpy_init(function[test_i]);
		if (ret != 0) {
			DMA_UT_ERR("Memcopy_init failed for,"
				"FUNCTION TYPE: %d, VF_ID: %d\n",
				function[test_i].function_type,
				function[test_i].vf_id);
			goto fail_init;
		}

		ret = mhi_dma_memcpy_enable(function[test_i]);
		if (ret != 0) {
			DMA_UT_ERR("Failed to enable memcpy for,"
				"FUNCTION TYPE: %d, VF_ID: %d\n",
				function[test_i].function_type,
				function[test_i].vf_id);
			ret = -EFAULT;
			goto fail_enable;
		}
	}

	/* Compare driver states for all VMs/PF */
	for (test_i = 0; test_i < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM; test_i++) {
		DMA_UT_DBG("Starting DMA driver check for VF_ID %d\n",
			function[test_i].vf_id);
		ret = ecpri_dma_mhi_client_test_utils_check_memcpy_init_state(
			test_i, mhi_memcpy_ctx, &function[test_i]);
		if (ret != 0) {
			DMA_UT_ERR("Driver state for VF_ID %d / IDX %d has failed\n",
				function[test_i].vf_id, test_i);
			return -EPERM;
		}

		DMA_UT_DBG("DMA driver context was checked successfully for VF_ID %d\n",
			function[test_i].vf_id);
	}

fail_enable:
	/* Disable all initialized memcpy so far */
	for (j = 0; j < test_i; j++) {
		ret = mhi_dma_memcpy_disable(function[j]);
		if (ret != 0) {
			DMA_UT_ERR("Unable to disable memcpy\n");
			return -EPERM;
		}
	}

fail_init:
	/* Destroy all initialized memcpy so far */
	for (j = 0; j < test_i; j++) {
		mhi_dma_memcpy_destroy(function[j]);
	}

	return ret;
}

static int ecpri_dma_mhi_client_test_suite_vm_memcpy_pf_sync(void* priv)
{
	int ret = 0;
	int idx;
	struct mhi_dma_function_params function;
	u32 pkt_content = ECPRI_DMA_MHI_TEST_PACKET_CONTENT;
	struct ecpri_dma_mhi_memcpy_context* mhi_memcpy_ctx = NULL;

	DMA_UT_DBG("Start MHI MEMCPY SYNC PF\n");
	DMA_UT_DBG("=============================== Start MHI MEMCPY SYNC PF"
		" =============================== \n");

	/* Create func params for PF */
	ecpri_dma_mhi_test_create_func_params(&function,
		MHI_DMA_FUNCTION_TYPE_PHYSICAL, ECPRI_DMA_MHI_PF_ID);

	ret = ecpri_dma_mhi_test_get_func_idx(&function, &idx);
	if (ret != 0) {
		return ret;
	}

	ret = mhi_dma_memcpy_init(function);
	if (ret != 0) {
		DMA_UT_ERR("Memcopy_init failed for,"
			"FUNCTION TYPE: %d, VF_ID: %d\n",
			function.function_type,
			function.vf_id);
		goto fail_init;
	}

	/* Invoke mhi_dma_enable */
	ret = mhi_dma_memcpy_enable(function);
	if (ret != 0) {
		DMA_UT_ERR("mhi_dma_enable failed, ret %d\n", ret);
		DMA_UT_TEST_FAIL_REPORT("mhi_dma_enable failed");
		ret = -EFAULT;
		goto fail_enable;
	}

	/* Check driver context */
	DMA_UT_DBG("Starting DMA driver check for VF_ID %d\n",
		function.vf_id);

	ret = ecpri_dma_mhi_client_test_utils_check_memcpy_init_state(
		idx, mhi_memcpy_ctx, &function);
	if (ret != 0) {
		DMA_UT_ERR("Driver state for VF_ID %d / IDX %d has failed\n",
			function.vf_id, idx);
		ret = -EPERM;
		goto fail;
	}

	DMA_UT_DBG("DMA driver context was checked successfully for VF_ID %d\n",
		function.vf_id);

	/* Allocate packets and buffers */
	ret = ecpri_dma_mhi_client_test_alloc_src_dest_buffers(ECPRI_DMA_MHI_PF_ID);
	if (ret != 0) {
		DMA_UT_ERR("Failed to allocate buffers, ret %d\n", ret);
		ret = -EFAULT;
		goto fail;
	}

	/* Assign packet to src */
	*(u32*)mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->
		src_buffer.virt_base = pkt_content;

	/* invoke memcpy_sync */
	ret = mhi_dma_sync_memcpy(
		(u64)mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->
		dest_buffer.phys_base,
		(u64)mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->
		src_buffer.phys_base,
		sizeof(u32),
		function);
	if (ret != 0) {
		DMA_UT_ERR("Failed sync_memcpy, ret %d\n", ret);
		ret = -EFAULT;
		goto fail;
	}

	/* Verify content of src and dest buffers */
	if (memcmp(mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->
		src_buffer.virt_base,
		mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->
		dest_buffer.virt_base,
		mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->
		dest_buffer.size)) {
		DMA_UT_ERR("Buffers don't match, ret %d\n", ret);
		ret = -EFAULT;
		goto fail;
	}

	/* Free buffers */
	ecpri_dma_mhi_client_test_free_src_dest_buffers(idx);

fail:
	ret = mhi_dma_memcpy_disable(function);
	if (ret != 0) {
		DMA_UT_ERR("mhi_dma_enable failed, ret %d\n", ret);
		DMA_UT_TEST_FAIL_REPORT("mhi_dma_enable failed");
		ret = -EFAULT;
	}

fail_enable:
	mhi_dma_memcpy_destroy(function);

fail_init:

	return ret;
}

static int ecpri_dma_mhi_client_test_suite_vm_memcpy_pf_async(void* priv)
{
	int ret = 0;
	int idx;
	struct mhi_dma_function_params function;
	u32 pkt_content = ECPRI_DMA_MHI_TEST_PACKET_CONTENT;

	DMA_UT_DBG("=============================== Start MHI MEMCPY ASYNC PF"
		" ============================== = \n");

	/* Create func params for PF */
	ecpri_dma_mhi_test_create_func_params(&function,
		MHI_DMA_FUNCTION_TYPE_PHYSICAL, ECPRI_DMA_MHI_PF_ID);

	ret = ecpri_dma_mhi_test_get_func_idx(&function, &idx);
	if (ret != 0) {
		return ret;
	}

	ret = mhi_dma_memcpy_init(function);
	if (ret != 0) {
		DMA_UT_ERR("Memcopy_init failed for,"
			"FUNCTION TYPE: %d, VF_ID: %d\n",
			function.function_type,
			function.vf_id);
		DMA_UT_TEST_FAIL_REPORT("Memcopy_init failed\n");
		goto fail_init;
	}

	/* Invoke mhi_dma_enable */
	ret = mhi_dma_memcpy_enable(function);
	if (ret != 0) {
		DMA_UT_ERR("mhi_dma_enable failed, ret %d\n", ret);
		DMA_UT_TEST_FAIL_REPORT("mhi_dma_enable failed\n");
		ret = -EFAULT;
		goto fail_enable;
	}

	/* Allocate packets and buffers */
	ret = ecpri_dma_mhi_client_test_alloc_src_dest_buffers(ECPRI_DMA_MHI_PF_ID);
	if (ret != 0) {
		DMA_UT_ERR("Failed to allocate buffers, ret %d\n", ret);
		DMA_UT_TEST_FAIL_REPORT("Failed to allocate buffers\n");
		ret = -EFAULT;
		goto fail;
	}

	/* Assign packet to src */
	*(u32*)mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->
		src_buffer.virt_base = pkt_content;

	mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->async_user_data =
		pkt_content;

	/* invoke memcpy_async */
	ret = mhi_dma_async_memcpy(
		(u64)mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->
		dest_buffer.phys_base,
		(u64)mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->
		src_buffer.phys_base,
		sizeof(u32),
		function,
		&ecpri_dma_mhi_client_test_async_comp_cb,
		&mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->async_user_data);
	if (ret != 0) {
		DMA_UT_ERR("Failed async_memcpy, ret %d\n", ret);
		ret = -EFAULT;
		goto fail;
	}

	/* Wait for completion */
	DMA_UT_DBG("Wait for async completion event\n");
	if (wait_for_completion_timeout(
		&mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->
		ecpri_dma_mhi_test_async_comp,
		msecs_to_jiffies(ECPRI_DMA_MHI_TEST_CMPLN_TIMEOUT)) == 0)
	{
		DMA_UT_DBG("timeout waiting for async completion event");
		DMA_UT_TEST_FAIL_REPORT("failed waiting for state ready");
		ret = -ETIME;
		goto fail;
	}
	DMA_UT_DBG("Got DMA ready event\n");

	/* Verify content of src and dest buffers */
	if (memcmp(mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->
		src_buffer.virt_base,
		mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->
		dest_buffer.virt_base,
		mhi_client_test_suite_ctx[ECPRI_DMA_MHI_PF_ID]->
		dest_buffer.size)) {
		DMA_UT_ERR("Buffers don't match, ret %d\n", ret);
		ret = -EFAULT;
		goto fail;
	}

	/* Free buffers */
	ecpri_dma_mhi_client_test_free_src_dest_buffers(idx);

fail:
	ret = mhi_dma_memcpy_disable(function);
	if (ret != 0) {
		DMA_UT_DBG("mhi_dma_memcpy_disable failed, ret %d\n", ret);
		ret = -EFAULT;
	}

fail_enable:
	mhi_dma_memcpy_destroy(function);

fail_init:

	return ret;
}

static int ecpri_dma_mhi_client_test_suite_connect_endp_vm(void* priv)
{
	int ret = 0;
	int idx;
	static int vf_id = ECPRI_DMA_VM_IDS_VM0;

	struct ecpri_dma_mhi_client_context* mhi_dma_ctx = NULL;
	struct ecpri_dma_mhi_client_test_suite_context* ctx = NULL;

	DMA_UT_DBG("Start CONNECT ENDP VM%d\n", vf_id);
	ctx = mhi_client_test_suite_ctx[vf_id];

	/* Create func params */
	ecpri_dma_mhi_test_create_func_params(&ctx->function,
		MHI_DMA_FUNCTION_TYPE_VIRTUAL, vf_id);

	ret = ecpri_dma_mhi_test_get_func_idx(&ctx->function, &idx);
	if (ret != 0) {
		return ret;
	}

	/* Init driver */
	ret = ecpri_dma_mhi_client_test_utils_create_params_and_init(
		&ctx->function, &ctx->init_params,
		&ctx->out_params, &ctx->start_params);
	if (ret != 0) {
		DMA_UT_ERR("VF_ID %d failed", ctx->function.vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test has failed\n");
		return -EFAULT;
	}

	/* Check driver context */
	ret = ecpri_dma_mhi_client_test_utils_check_driver_state(
		idx, &ctx->init_params, mhi_dma_ctx, &ctx->function);
	if (ret != 0) {
		DMA_UT_ERR("Driver state for VF_ID %d / IDX %d has failed\n",
			ctx->function.vf_id, idx);
		return -EPERM;
	}

	/* Create connect params */
	ecpri_dma_mhi_client_test_utils_create_conn_params(
		&ctx->frst_src_conn_params, idx,
		ECPRI_DMA_MHI_TEST_EMPTY_BUFF,
		ECPRI_DMA_MHI_TEST_FRST_SRC_CHANNEL_ID);

	ecpri_dma_mhi_client_test_utils_create_conn_params(
		&ctx->frst_dest_conn_params, idx,
		ECPRI_DMA_MHI_TEST_BUFF_SIZE,
		ECPRI_DMA_MHI_TEST_FRST_DEST_CHANNEL_ID);

	/* Connect and verify */
	ret = ecpri_dma_mhi_utils_connect_and_verify_endps(&ctx->function,
		&ctx->frst_src_conn_params, &ctx->frst_dest_conn_params,
		&ctx->frst_src_disc_params.clnt_hdl,
		&ctx->frst_dest_disc_params.clnt_hdl, idx,
		ECPRI_DMA_MHI_TEST_FRST_SRC_CHANNEL_ID,
		ECPRI_DMA_MHI_TEST_FRST_DEST_CHANNEL_ID,
		ecpri_dma_mhi_client_test_mapping[idx].first_src_endp_id,
		ecpri_dma_mhi_client_test_mapping[idx].first_dest_endp_id);
	if (ret != 0) {
		DMA_UT_ERR("VF_ID %d / IDX %d failed", ctx->function.vf_id, idx);
		DMA_UT_TEST_FAIL_REPORT("Connect_endp for has failed\n");
		return -EFAULT;
	}

	/* Disconnect */
	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		ret = ecpri_dma_mhi_driver_ops.mhi_dma_disconnect_endp(ctx->function,
			&ctx->frst_dest_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Disconnect endp for DEST, VF_ID %d has failed\n",
				ctx->function.vf_id);
			return -EPERM;
		}

		ret = ecpri_dma_mhi_driver_ops.mhi_dma_disconnect_endp(ctx->function,
			&ctx->frst_src_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Disconnect endp for SRC, VF_ID %d has failed\n",
				ctx->function.vf_id);
			return -EPERM;
		}
	}
	else {
		ret = mhi_dma_disconnect_endp(ctx->function,
			&ctx->frst_dest_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Disconnect endp for DEST, VF_ID %d has failed\n",
				ctx->function.vf_id);
			return -EPERM;
		}

		ret = mhi_dma_disconnect_endp(ctx->function,
			&ctx->frst_src_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Disconnect endp for SRC, VF_ID %d has failed\n",
				ctx->function.vf_id);
			return -EPERM;
		}
	}

	mhi_dma_destroy(ctx->function);

	vf_id++;
	vf_id = vf_id % ECPRI_DMA_MHI_VIRTUAL_FUNCTION_NUM;

	return 0;
}

static int ecpri_dma_mhi_client_test_suite_connect_endp_all(void* priv) {
	int test_i;
	int ret = 0;

	struct ecpri_dma_mhi_client_context* mhi_dma_ctx = NULL;
	struct ecpri_dma_mhi_client_test_suite_context* ctx = NULL;

	/* Run tests for VMs only */
	for (test_i = 0; test_i < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM - 1; test_i++)
	{
		ctx = mhi_client_test_suite_ctx[test_i];

		DMA_UT_DBG("STARTING TEST FOR VF_ID %d\n",
			ctx->function.vf_id);

		/* Create function */
		ecpri_dma_mhi_test_create_func_params(&ctx->function,
			MHI_DMA_FUNCTION_TYPE_VIRTUAL, ECPRI_DMA_VM_IDS_VM0 + test_i);

		/* Register driver for the specific VM */
		ret = ecpri_dma_mhi_client_test_utils_create_params_and_init(
			&ctx->function, &ctx->init_params,
			&ctx->out_params, &ctx->start_params);
		if (ret != 0) {
			DMA_UT_ERR("VF_ID %d failed", ctx->function.vf_id);
			DMA_UT_TEST_FAIL_REPORT("Test has failed\n");
			return -EFAULT;
		}

		/* Check driver state for the specific VM/PF */
		DMA_UT_DBG("Starting DMA driver check for VF_ID %d\n",
			ctx->function.vf_id);
		ret = ecpri_dma_mhi_client_test_utils_check_driver_state(
			test_i, &ctx->init_params, mhi_dma_ctx, &ctx->function);
		if (ret != 0) {
			DMA_UT_ERR("Driver state for VF_ID %d / IDX %d has failed\n",
				ctx->function.vf_id, test_i);
			return -EPERM;
		}

		/* Connect First pair of ENDPs */
		ret = ecpri_dma_mhi_utils_connect_and_verify_endps(&ctx->function,
			&ctx->frst_src_conn_params, &ctx->frst_dest_conn_params,
			&ctx->frst_src_disc_params.clnt_hdl,
			&ctx->frst_dest_disc_params.clnt_hdl, test_i,
			ECPRI_DMA_MHI_TEST_FRST_SRC_CHANNEL_ID,
			ECPRI_DMA_MHI_TEST_FRST_DEST_CHANNEL_ID,
			ecpri_dma_mhi_client_test_mapping[test_i].first_src_endp_id,
			ecpri_dma_mhi_client_test_mapping[test_i].first_dest_endp_id);
		if (ret != 0) {
			DMA_UT_ERR("VF_ID %d / IDX %d failed", ctx->function.vf_id, test_i);
			DMA_UT_TEST_FAIL_REPORT("Connect_endp has failed\n");
			return -EFAULT;
		}

		/* Connect second pair of ENDPs */
		ret = ecpri_dma_mhi_utils_connect_and_verify_endps(&ctx->function,
			&ctx->second_src_conn_params, &ctx->second_dest_conn_params,
			&ctx->second_src_disc_params.clnt_hdl,
			&ctx->second_dest_disc_params.clnt_hdl,	test_i,
			ECPRI_DMA_MHI_TEST_SCND_SRC_CHANNEL_ID,
			ECPRI_DMA_MHI_TEST_SCND_DEST_CHANNEL_ID,
			ecpri_dma_mhi_client_test_mapping[test_i].second_src_endp_id,
			ecpri_dma_mhi_client_test_mapping[test_i].second_dest_endp_id);
		if (ret != 0) {
			DMA_UT_ERR("Connect endp for VF_ID %d / IDX %d has failed\n",
				ctx->function.vf_id, test_i);
			return -EPERM;
		}

		DMA_UT_DBG("FINISHED TEST FOR VF_ID %d\n",
			ctx->function.vf_id);
	}

	/* Run for PF only */
	DMA_UT_DBG("Starting PF check Test_id %d(%d)\n",
		test_i, ECPRI_DMA_MHI_PF_ID);
	if (test_i != ECPRI_DMA_MHI_PF_ID) {
		DMA_UT_TEST_FAIL_REPORT("Wrong test_id\n");
		return -EFAULT;
	}

	ctx = mhi_client_test_suite_ctx[test_i];

	/* Create function params */
	ecpri_dma_mhi_test_create_func_params(&ctx->function,
		MHI_DMA_FUNCTION_TYPE_PHYSICAL, ECPRI_DMA_MHI_PF_ID);

	/* Create init params and invoke */
	ret = ecpri_dma_mhi_client_test_utils_create_params_and_init(
		&ctx->function, &ctx->init_params,
		&ctx->out_params, &ctx->start_params);
	if (ret != 0) {
		DMA_UT_ERR("VF_ID %d failed", ctx->function.vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test has failed\n");
		return -EFAULT;
	}

	/* Check driver context */
	ret = ecpri_dma_mhi_client_test_utils_check_driver_state(
		test_i, &ctx->init_params, mhi_dma_ctx, &ctx->function);
	if (ret != 0) {
		DMA_UT_ERR("Driver state for VF_ID %d / IDX %d has failed\n",
			ctx->function.vf_id, test_i);
		return -EPERM;
	}

	/* Create connect_params for SRC ENDP */
	DMA_UT_DBG("PF CH_ID %d\n", ECPRI_DMA_MHI_TEST_FRST_SRC_CHANNEL_ID);
	ecpri_dma_mhi_client_test_utils_create_conn_params(
		&ctx->frst_src_conn_params, test_i,
		ECPRI_DMA_MHI_TEST_EMPTY_BUFF,
		ECPRI_DMA_MHI_TEST_FRST_SRC_CHANNEL_ID);

	/*Connect_endp for SRC ENDP - expected to fail*/
	ret = mhi_dma_connect_endp(ctx->function,
		&ctx->frst_src_conn_params,
		&ctx->frst_src_disc_params.clnt_hdl);
	if (ret != -EINVAL) {
		DMA_UT_ERR("PF expected to fail, ret = %d\n",
			ret);
		return -EPERM;
	}

	mhi_dma_destroy(ctx->function);

	ret = 0;

	/* Cleanup */
	for (test_i = 0; test_i < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM - 1; test_i++)
	{
		ctx = mhi_client_test_suite_ctx[test_i];

		/* Disconnect endps */
		ret = ecpri_dma_mhi_client_test_suite_utils_disconnect_endps(
			&ctx->function,
			&ctx->frst_src_disc_params, &ctx->frst_dest_disc_params,
			&ctx->second_src_disc_params, &ctx->second_dest_disc_params
		);
		if (ret != 0) {
			DMA_UT_ERR("Unable to disconnect ret = %d\n",
				ret);
			return -EPERM;
		}

		/* Destroy dma per VM */
		mhi_dma_destroy(ctx->function);
	}

	/* Destroy dma for PF */
	ctx = mhi_client_test_suite_ctx[test_i];
	mhi_dma_destroy(ctx->function);

	return ret;
}


static int
ecpri_dma_mhi_client_test_suite_hw_ch_vm_single_packet_single_buffer(void* priv)
{
	int ret = 0;
	int idx, i;
	enum ecpri_dma_ees ee;
	u8 vf_id = ECPRI_DMA_VM_IDS_VM0;

	struct ecpri_dma_mhi_client_context* mhi_dma_ctx = NULL;
	struct ecpri_dma_mhi_client_test_suite_context* ctx = NULL;

	for (i = 0; i < ECPRI_DMA_SMMU_CB_MAX; i++)
	{
		if (ecpri_dma_ctx->s1_bypass_arr[i] != true)
		{
			DMA_UT_ERR("SMMU must be bypassed for MHI HW CHs testing\n");
			return -EINVAL;
		}
	}

	DMA_UT_DBG("Start HW CH VM%d\n", vf_id);
	ctx = mhi_client_test_suite_ctx[vf_id];

	/* Create function */
	ecpri_dma_mhi_test_create_func_params(&ctx->function,
		MHI_DMA_FUNCTION_TYPE_VIRTUAL, vf_id);

	ret = ecpri_dma_mhi_test_get_func_idx(&ctx->function, &idx);
	if (ret != 0) {
		return ret;
	}

	/* Register driver and verify driver state */
	ret = ecpri_dma_mhi_client_test_utils_create_params_and_init(
		&ctx->function, &ctx->init_params,
		&ctx->out_params, &ctx->start_params);
	if (ret != 0) {
		DMA_UT_ERR("VF_ID %dfailed", ctx->function.vf_id);
		DMA_UT_TEST_FAIL_REPORT("Test has failed\n");
		return -EFAULT;
	}

	/* Check driver context */
	ret = ecpri_dma_mhi_client_test_utils_check_driver_state(
		idx, &ctx->init_params, mhi_dma_ctx, &ctx->function);
	if (ret != 0) {
		DMA_UT_ERR("Driver state for VF_ID %d / IDX %d has failed\n",
			ctx->function.vf_id, idx);
		return -EPERM;
	}

	/* Create loop-back */
	ret = ecpri_dma_mhi_client_test_util_setup_dma_endps(
		ecpri_dma_mhi_client_test_mapping[
			idx].first_src_endp_id,
		ecpri_dma_mhi_client_test_mapping[
			idx].first_dest_endp_id, true);
	if (ret != 0) {
		DMA_UT_ERR("VF_ID %d / IDX %d failed"
			" SRC ENDP ID %d, DEST ENDP ID %d\n", ctx->function.vf_id, idx,
			ecpri_dma_mhi_client_test_mapping[idx].first_src_endp_id,
			ecpri_dma_mhi_client_test_mapping[idx].first_dest_endp_id);
		DMA_UT_TEST_FAIL_REPORT("Loopback configuration failed");
		return -EFAULT;
	}

	/* Invoke connect_endp and verify - for first pair */
	ret = ecpri_dma_mhi_utils_connect_and_verify_endps(&ctx->function,
		&ctx->frst_src_conn_params, &ctx->frst_dest_conn_params,
		&ctx->frst_src_disc_params.clnt_hdl,
		&ctx->frst_dest_disc_params.clnt_hdl, idx,
		ECPRI_DMA_MHI_TEST_FRST_SRC_CHANNEL_ID,
		ECPRI_DMA_MHI_TEST_FRST_DEST_CHANNEL_ID,
		ecpri_dma_mhi_client_test_mapping[idx].first_src_endp_id,
		ecpri_dma_mhi_client_test_mapping[idx].first_dest_endp_id);
	if (ret != 0) {
		DMA_UT_ERR("VF_ID %d / IDX %d failed\n", ctx->function.vf_id, idx);
		DMA_UT_TEST_FAIL_REPORT("Connect_endp has failed\n");
		return -EFAULT;
	}

	ret = ecpri_dma_mhi_client_test_get_ee_index(ctx->function, &ee);
	if (ret != 0) {
		DMA_UT_ERR("VF_ID %d / IDX %d failed\n", ctx->function.vf_id, idx);
		DMA_UT_TEST_FAIL_REPORT("Get EE index has failed\n");
		return -EFAULT;
	}

	/* Generate, enqueue, and poll TRE  */
	ret = ecpri_dma_mhi_test_loopback_data_transfer(idx,
		ecpri_dma_mhi_client_test_host_ch_id_map[0],
		ecpri_dma_mhi_client_test_host_ch_id_map[1], ee);
	if (ret != 0) {
		DMA_UT_ERR("VF_ID %d / IDX %d failed"
			" SRC ENDP ID %d, DEST ENDP ID %d\n", ctx->function.vf_id, idx,
			ecpri_dma_mhi_client_test_mapping[idx].first_src_endp_id,
			ecpri_dma_mhi_client_test_mapping[idx].first_dest_endp_id);
		DMA_UT_TEST_FAIL_REPORT("Transfer has failed");
		return -EFAULT;
	}

	/* Reset loopback at the end of the test*/
	ret = ecpri_dma_mhi_client_test_util_setup_dma_endps(
		ecpri_dma_mhi_client_test_mapping[
			idx].first_src_endp_id,
		ecpri_dma_mhi_client_test_mapping[
			idx].first_dest_endp_id, false);
	if (ret != 0) {
		DMA_UT_ERR("VF_ID %d / IDX %d failed"
			" SRC ENDP ID %d, DEST ENDP ID %d\n", ctx->function.vf_id, idx,
			ecpri_dma_mhi_client_test_mapping[idx].first_src_endp_id,
			ecpri_dma_mhi_client_test_mapping[idx].first_dest_endp_id);
		DMA_UT_TEST_FAIL_REPORT("Loopback configuration has failed\n");
		return -EFAULT;
	}

	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		ret = ecpri_dma_mhi_driver_ops.mhi_dma_disconnect_endp(ctx->function,
			&ctx->frst_src_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Disconnect endp for SRC, VF_ID %d has failed\n",
				ctx->function.vf_id);
			return -EPERM;
		}

		ret = ecpri_dma_mhi_driver_ops.mhi_dma_disconnect_endp(ctx->function,
			&ctx->frst_dest_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Disconnect endp for SRC, VF_ID %d has failed\n",
				ctx->function.vf_id);
			return -EPERM;
		}
	}
	else {
		ret = mhi_dma_disconnect_endp(ctx->function,
			&ctx->frst_src_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Disconnect endp for SRC, VF_ID %d has failed\n",
				ctx->function.vf_id);
			return -EPERM;
		}

		ret = mhi_dma_disconnect_endp(ctx->function,
			&ctx->frst_dest_disc_params);
		if (ret != 0) {
			DMA_UT_ERR("Disconnect endp for SRC, VF_ID %d has failed\n",
				ctx->function.vf_id);
			return -EPERM;
		}
	}

	mhi_dma_destroy(ctx->function);

	DMA_UT_DBG("Finished HW CH VM%d\n", vf_id);

	return 0;
}

static int
ecpri_dma_mhi_client_test_suite_hw_ch_all_single_packet_single_buffer(void* priv)
{
	int ret = 0;
	int idx, i;
	int test_i;
	enum ecpri_dma_ees ee;
	struct ecpri_dma_mhi_client_test_suite_context* ctx = NULL;
	struct ecpri_dma_mhi_client_context* mhi_dma_ctx = NULL;

	for (i = 0; i < ECPRI_DMA_SMMU_CB_MAX; i++)
	{
		if (ecpri_dma_ctx->s1_bypass_arr[i] != true)
		{
			DMA_UT_ERR("SMMU must be bypassed for MHI HW CHs testing\n");
			return -EINVAL;
		}
	}

	/* Run tests only for VMs */
	for (test_i = 0; test_i < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM - 1; test_i++)
	{
		DMA_UT_DBG(" Start HW CH VM %d\n", test_i);
		ctx = mhi_client_test_suite_ctx[test_i];

		/* Create function */
		ecpri_dma_mhi_test_create_func_params(&ctx->function,
			MHI_DMA_FUNCTION_TYPE_VIRTUAL, ECPRI_DMA_VM_IDS_VM0 + test_i);

		ret = ecpri_dma_mhi_test_get_func_idx(&ctx->function, &idx);
		if (ret != 0) {
			return ret;
		}

		/* Register driver and verify driver state */
		ret = ecpri_dma_mhi_client_test_utils_create_params_and_init(
			&ctx->function, &ctx->init_params,
			&ctx->out_params, &ctx->start_params);
		if (ret != 0) {
			DMA_UT_ERR("VF_ID %d / IDX %d failed\n", ctx->function.vf_id);
			DMA_UT_TEST_FAIL_REPORT("Test has failed\n");
			return -EFAULT;
		}

		/* Check driver context */
		ret = ecpri_dma_mhi_client_test_utils_check_driver_state(
			idx, &ctx->init_params, mhi_dma_ctx, &ctx->function);
		if (ret != 0) {
			DMA_UT_ERR("VF_ID %d / IDX %d failed\n", ctx->function.vf_id, idx);
			DMA_UT_ERR("Driver state has failed\n");
			return -EPERM;
		}

		/* Create loop-back */
		ret = ecpri_dma_mhi_client_test_util_setup_dma_endps(
			ecpri_dma_mhi_client_test_mapping[
				idx].first_src_endp_id,
			ecpri_dma_mhi_client_test_mapping[
				idx].first_dest_endp_id, true);
		if (ret != 0) {
			DMA_UT_ERR("VF_ID %d / IDX %d failed"
				" SRC ENDP ID %d, DEST ENDP ID %d\n", ctx->function.vf_id, idx,
				ecpri_dma_mhi_client_test_mapping[idx].first_src_endp_id,
				ecpri_dma_mhi_client_test_mapping[idx].first_dest_endp_id);
			DMA_UT_TEST_FAIL_REPORT("Loopback configuration has failed\n");
			return -EFAULT;
		}

		ret = ecpri_dma_mhi_client_test_util_setup_dma_endps(
			ecpri_dma_mhi_client_test_mapping[
				idx].second_src_endp_id,
			ecpri_dma_mhi_client_test_mapping[
				idx].second_dest_endp_id, true);
		if (ret != 0) {
			DMA_UT_ERR("VF_ID %d / IDX %d failed"
				" SRC ENDP ID %d, DEST ENDP ID %d\n", ctx->function.vf_id, idx,
				ecpri_dma_mhi_client_test_mapping[idx].second_src_endp_id,
				ecpri_dma_mhi_client_test_mapping[idx].second_dest_endp_id);
			DMA_UT_TEST_FAIL_REPORT("Loopback configuration has failed\n");
			return -EFAULT;
		}

		/* Invoke connect_endp and verify - for first pair */
		ret = ecpri_dma_mhi_utils_connect_and_verify_endps(&ctx->function,
			&ctx->frst_src_conn_params, &ctx->frst_dest_conn_params,
			&ctx->frst_src_disc_params.clnt_hdl,
			&ctx->frst_dest_disc_params.clnt_hdl, idx,
			ECPRI_DMA_MHI_TEST_FRST_SRC_CHANNEL_ID,
			ECPRI_DMA_MHI_TEST_FRST_DEST_CHANNEL_ID,
			ecpri_dma_mhi_client_test_mapping[idx].first_src_endp_id,
			ecpri_dma_mhi_client_test_mapping[idx].first_dest_endp_id);
		if (ret != 0) {
			DMA_UT_ERR("VF_ID %d / IDX %d failed\n", ctx->function.vf_id, idx);
			DMA_UT_TEST_FAIL_REPORT("Connect_endp has failed\n");
			return -EFAULT;
		}

		/* Invoke connect_endp and verify - for second pair */
		ret = ecpri_dma_mhi_utils_connect_and_verify_endps(&ctx->function,
			&ctx->second_src_conn_params, &ctx->second_dest_conn_params,
			&ctx->second_src_disc_params.clnt_hdl,
			&ctx->second_dest_disc_params.clnt_hdl, idx,
			ECPRI_DMA_MHI_TEST_SCND_SRC_CHANNEL_ID,
			ECPRI_DMA_MHI_TEST_SCND_DEST_CHANNEL_ID,
			ecpri_dma_mhi_client_test_mapping[idx].second_src_endp_id,
			ecpri_dma_mhi_client_test_mapping[idx].second_dest_endp_id);
		if (ret != 0) {
			DMA_UT_ERR("VF_ID %d / IDX %d failed\n", ctx->function.vf_id, idx);
			DMA_UT_TEST_FAIL_REPORT("Connect_endp has failed\n");
			return -EFAULT;
		}

		ret = ecpri_dma_mhi_client_test_get_ee_index(ctx->function, &ee);
		if (ret != 0) {
			DMA_UT_ERR("VF_ID %d / IDX %d failed\n", ctx->function.vf_id, idx);
			DMA_UT_TEST_FAIL_REPORT("Get EE index has failed\n");
			return -EFAULT;
		}

		/* Generate, enqueue, and poll TRE for first pair*/
		ret = ecpri_dma_mhi_test_loopback_data_transfer(idx,
			ecpri_dma_mhi_client_test_host_ch_id_map[
				ECPRI_DMA_MHI_TEST_FRST_SRC_CHANNEL_ID],
			ecpri_dma_mhi_client_test_host_ch_id_map[
				ECPRI_DMA_MHI_TEST_FRST_DEST_CHANNEL_ID], ee);
		if (ret != 0) {
			DMA_UT_ERR("VF_ID %d / IDX %d failed"
				" SRC CH ID %d, DEST CH ID %d\n", ctx->function.vf_id, idx,
				ecpri_dma_mhi_client_test_host_ch_id_map[
					ECPRI_DMA_MHI_TEST_FRST_SRC_CHANNEL_ID],
				ecpri_dma_mhi_client_test_host_ch_id_map[
					ECPRI_DMA_MHI_TEST_FRST_DEST_CHANNEL_ID]);
			DMA_UT_TEST_FAIL_REPORT("Transfer has failed\n");
			return -EFAULT;
		}

		/* Generate, enqueue, and poll TRE for second pair */
		ret = ecpri_dma_mhi_test_loopback_data_transfer(idx,
			ecpri_dma_mhi_client_test_host_ch_id_map[
				ECPRI_DMA_MHI_TEST_SCND_SRC_CHANNEL_ID],
			ecpri_dma_mhi_client_test_host_ch_id_map[
				ECPRI_DMA_MHI_TEST_SCND_DEST_CHANNEL_ID], ee);
		if (ret != 0) {
			DMA_UT_ERR("VF_ID %d / IDX %d failed"
				" SRC CH ID %d, DEST CH ID %d\n", ctx->function.vf_id, idx,
				ecpri_dma_mhi_client_test_host_ch_id_map[
					ECPRI_DMA_MHI_TEST_SCND_SRC_CHANNEL_ID],
				ecpri_dma_mhi_client_test_host_ch_id_map[
					ECPRI_DMA_MHI_TEST_SCND_DEST_CHANNEL_ID]);
			DMA_UT_TEST_FAIL_REPORT("Transfer has failed\n");
			return -EFAULT;
		}

		/* Reset loopback at the end of the test*/
		ret = ecpri_dma_mhi_client_test_util_setup_dma_endps(
			ecpri_dma_mhi_client_test_mapping[
				idx].second_src_endp_id,
			ecpri_dma_mhi_client_test_mapping[
				idx].second_dest_endp_id, false);
		if (ret != 0) {
			DMA_UT_ERR("VF_ID %d / IDX %d failed"
				" SRC ENDP ID %d, DEST ENDP ID %d\n", ctx->function.vf_id, idx,
				ecpri_dma_mhi_client_test_mapping[idx].second_src_endp_id,
				ecpri_dma_mhi_client_test_mapping[idx].second_dest_endp_id);
			DMA_UT_TEST_FAIL_REPORT("Loopback configuration has failed\n");
			return -EFAULT;
		}

		ret = ecpri_dma_mhi_client_test_util_setup_dma_endps(
			ecpri_dma_mhi_client_test_mapping[
				idx].first_src_endp_id,
			ecpri_dma_mhi_client_test_mapping[
				idx].first_dest_endp_id, false);
		if (ret != 0) {
			DMA_UT_ERR("VF_ID %d / IDX %d failed"
				" SRC ENDP ID %d, DEST ENDP ID %d\n", ctx->function.vf_id, idx,
				ecpri_dma_mhi_client_test_mapping[idx].first_src_endp_id,
				ecpri_dma_mhi_client_test_mapping[idx].first_dest_endp_id);
			DMA_UT_TEST_FAIL_REPORT("Loopback configuration has failed\n");
			return -EFAULT;
		}

		DMA_UT_DBG("Finished HW CH VM %d\n", test_i);
	}

	/* Cleanup */
	for (test_i = 0; test_i < ECPRI_DMA_MHI_CLIENT_FUNCTION_NUM - 1; test_i++)
	{
		ctx = mhi_client_test_suite_ctx[test_i];
		/* Disconnect endps */
		ret = ecpri_dma_mhi_client_test_suite_utils_disconnect_endps(
			&ctx->function,
			&ctx->frst_src_disc_params, &ctx->frst_dest_disc_params,
			&ctx->second_src_disc_params, &ctx->second_dest_disc_params
		);
		if (ret != 0) {
			DMA_UT_ERR("Unable to disconnect ret = %d\n",
				ret);
			return -EPERM;
		}

		mhi_dma_destroy(ctx->function);
	}

	return ret;
}

/* Suite definition block */
DMA_UT_DEFINE_SUITE_START(mhi_client, "MHI Client suite",
	ecpri_dma_mhi_client_test_suite_setup,
	ecpri_dma_mhi_client_test_suite_teardown) {
	DMA_UT_ADD_TEST(
		init,
		"This test will verify that MHI Driver was initialized "
		"correctly according the matching VF ID. Test will ensure"
		" that deallocation processed correctly.",
		ecpri_dma_mhi_client_test_suite_mhi_init, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
		DMA_UT_ADD_TEST(
			init_all_vms,
			"This test will verify that MHI Driver was initialized "
			"correctly according the matching VF ID for all VMs. Test will"
			" ensure that deallocation processed correctly.",
			ecpri_dma_mhi_client_test_suite_mhi_init_all_vms, true,
			ECPRI_HW_V1_0, ECPRI_HW_MAX),
		DMA_UT_ADD_TEST(
			memcpy_init_vm,
			"This test will:"
			" verify that MHI Driver initialized correctly according"
			" to the matching VF ID;"
			" confirm that MHI memcpy context in the DMA driver was initialized"
			" successfully;"
			" A55 MHI memcopy ENDPS match the GSI CHs was initialized"
			" successfully. "
			" check that all relevant deallocation was processed correctly.",
			ecpri_dma_mhi_client_test_suite_vm_memcpy_init, true,
			ECPRI_HW_V1_0, ECPRI_HW_MAX),
		DMA_UT_ADD_TEST(
			memcpy_init_pf,
			"This test will:"
			" verify that MHI Driver initialized correctly according"
			" to the physical channel;"
			" confirm that MHI memcpy context in the DMA driver was initialized"
			" successfully;"
			" A55 MHI memcopy ENDPS match the GSI CHs was initialized"
			" successfully. "
			" check that all relevant deallocation was processed correctly.",
			ecpri_dma_mhi_client_test_suite_pf_memcpy_init, true,
			ECPRI_HW_V1_0, ECPRI_HW_MAX),
		DMA_UT_ADD_TEST(
			memcpy_init_all_vms_pf,
			"For all VMs test will: "
			" verify that MHI Driver initialized correctly according"
			" to the matching VF ID;"
			" confirm that MHI memcpy context in the DMA driver was initialized"
			" successfully; "
			" check A55 MHI memcopy ENDPS match the GSI CHs was"
			" initialized successfully; "
			" check that after first memcpy disable ENDPs & CHs still up;"
			" check that after second memcpy disable ENDPs & CHs are down;"
			" verify each memcpy_init causes ref_count to increase;"
			" verify all relevant deallocations were processed correctly.",
			ecpri_dma_mhi_client_test_suite_memcpy_init_all_vms_pf, true,
			ECPRI_HW_V1_0, ECPRI_HW_MAX),
		DMA_UT_ADD_TEST(
			memcpy_pf_sync,
			"This test will verify the sync function for PF",
			ecpri_dma_mhi_client_test_suite_vm_memcpy_pf_sync, true,
			ECPRI_HW_V1_0, ECPRI_HW_MAX),
		DMA_UT_ADD_TEST(
			memcpy_pf_async,
			"This test will verify the async function for PF",
			ecpri_dma_mhi_client_test_suite_vm_memcpy_pf_async, true,
			ECPRI_HW_V1_0, ECPRI_HW_MAX),
		DMA_UT_ADD_TEST(
			memcpy_connect_vm,
			"Connect endp test for VM0",
			ecpri_dma_mhi_client_test_suite_connect_endp_vm, true,
			ECPRI_HW_V1_0, ECPRI_HW_MAX),
		DMA_UT_ADD_TEST(
			memcpy_connect_all,
			"Connect endp test for all VMs and Pf",
			ecpri_dma_mhi_client_test_suite_connect_endp_all, true,
			ECPRI_HW_V1_0, ECPRI_HW_MAX),
		DMA_UT_ADD_TEST(
			hw_ch_vm,
			"Tests will verify the HW path for VM0. Test will connect ENDPs"
			" and send the test packet via loopback from SRC to DEST."
			" Test will compare the content of the recevied packet.",
			ecpri_dma_mhi_client_test_suite_hw_ch_vm_single_packet_single_buffer,
			false, ECPRI_HW_V1_0, ECPRI_HW_MAX),
		DMA_UT_ADD_TEST(
			hw_ch_all,
			"Tests will verify the HW path for all VMs and PF. "
			"Test will connect ENDPs and send the test packet "
			"via loopback from SRC to DEST. Test will compare "
			"the content of the recevied packet.",
			ecpri_dma_mhi_client_test_suite_hw_ch_all_single_packet_single_buffer,
			false, ECPRI_HW_V1_0, ECPRI_HW_MAX),
} DMA_UT_DEFINE_SUITE_END(mhi_client);
