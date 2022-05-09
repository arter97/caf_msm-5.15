/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "ecpri_dma_ut_framework.h"
#include "dmahal.h"
#include "gsihal.h"

/**
 * DMA Driver init DMA Unit-test suite
 * Suite will verify DMA driver configurations, DMA HW configurations & GSI
 * configurations.
 */

enum ecpri_hw_ver ecpri_dma_ut_curr_hw_ver;
enum ecpri_hw_flavor ecpri_dma_ut_curr_hw_flv;

static int ecpri_dma_test_driver_init_suite_setup(void **ppriv)
{
	DMA_UT_DBG("Start Setup\n");

	return 0;
}

static int ecpri_dma_test_driver_init_suite_teardown(void *priv)
{
	DMA_UT_DBG("Start Teardown\n");

	return 0;
}

static int ecpri_dma_test_driver_init_verify_dtsi_params()
{
	if (ecpri_dma_ctx->ecpri_hw_ver != ecpri_dma_ut_curr_hw_ver) {
		DMA_UT_LOG("Failed - Expected HW Ver: %d, Parsed DTSi HW ver: %d\n",
			ecpri_dma_ut_curr_hw_ver, ecpri_dma_ctx->ecpri_hw_ver);
		DMA_UT_TEST_FAIL_REPORT("failed on test verifying DTSi params");
		return -EFAULT;
	}

	if (ecpri_dma_ctx->hw_flavor != ecpri_dma_ut_curr_hw_flv) {
		DMA_UT_LOG(
			"Failed - Expected HW flavor: %d, Parsed DTSi HW flavor: %d\n",
			ecpri_dma_ut_curr_hw_flv, ecpri_dma_ctx->hw_flavor);
		DMA_UT_TEST_FAIL_REPORT("failed on test verifying DTSi params");
		return -EFAULT;
	}

	return 0;
}

static int ecpri_dma_test_driver_init_verify_endp_mapping()
{
	int res = 0;
	const struct dma_gsi_ep_config *test_endp_map;

	res = ecpri_dma_get_endp_mapping(ecpri_dma_ut_curr_hw_ver,
		ecpri_dma_ut_curr_hw_flv, &test_endp_map);
	if (res) {
		DMA_UT_LOG("Failed retrieving eCPRI DMA ENDP mapping\n");
		DMA_UT_TEST_FAIL_REPORT("failed on test retrieving ENDP mapping");
		return -EFAULT;
	}

	if (ecpri_dma_ctx->endp_map != test_endp_map) {
		DMA_UT_LOG(
			"Failed - Expected ENDP map pointer: %px, Actual pointer: %px\n",
			test_endp_map, ecpri_dma_ctx->endp_map);
		DMA_UT_TEST_FAIL_REPORT("failed on test verifying ENDP mapping");
		return -EFAULT;
	}

	return 0;
}

static int ecpri_dma_test_driver_init_verify_driver_cfg()
{
	int res = 0;

	res = ecpri_dma_test_driver_init_verify_dtsi_params();
	if (res != 0)
		return res;

	res = ecpri_dma_test_driver_init_verify_endp_mapping();
	if (res != 0)
		return res;

	return 0;
}

static int ecpri_dma_test_driver_init_dtsi_parsing_v1_ru(void *priv)
{
	int res = 0;

	DMA_UT_LOG("Start DTSi parsing verification for V1 HW RU flavor\n");
	ecpri_dma_ut_curr_hw_ver = ECPRI_HW_V1_0;
	ecpri_dma_ut_curr_hw_flv = ECPRI_HW_FLAVOR_RU;

	res = ecpri_dma_test_driver_init_verify_driver_cfg();
	if (res != 0)
		return res;

	return 0;
}

static int ecpri_dma_test_driver_init_dtsi_parsing_v1_du_pcie(void *priv)
{
	int res = 0;

	DMA_UT_LOG("Start DTSi parsing verification for V1 HW DU PCIe flavor\n");
	ecpri_dma_ut_curr_hw_ver = ECPRI_HW_V1_0;
	ecpri_dma_ut_curr_hw_flv = ECPRI_HW_FLAVOR_DU_PCIE;

	res = ecpri_dma_test_driver_init_verify_driver_cfg();
	if (res != 0)
		return res;

	return 0;
}

static int ecpri_dma_test_driver_init_dtsi_parsing_v1_du_l2(void *priv)
{
	int res = 0;

	DMA_UT_LOG("Start DTSi parsing verification for V1 HW DU L2 flavor\n");
	ecpri_dma_ut_curr_hw_ver = ECPRI_HW_V1_0;
	ecpri_dma_ut_curr_hw_flv = ECPRI_HW_FLAVOR_DU_L2;

	res = ecpri_dma_test_driver_init_verify_driver_cfg();
	if (res != 0)
		return res;

	return 0;
}

static int ecpri_dma_test_driver_init_dma_paths(void *priv)
{
	int i = 0;
	const struct dma_gsi_ep_config *endp_map;
	ecpri_hwio_def_ecpri_endp_cfg_destn_u endp_cfg_dest = { 0 };
	ecpri_hwio_def_ecpri_endp_cfg_destn_u endp_cfg_dest_hw = { 0 };
	ecpri_hwio_def_ecpri_endp_cfg_xbarn_u endp_cfg_xbar = { 0 };
	ecpri_hwio_def_ecpri_endp_cfg_xbarn_u endp_cfg_xbar_hw = { 0 };
	ecpri_hwio_def_ecpri_dma_exception_channel_u exception_ch = { 0 };
	ecpri_hwio_def_ecpri_dma_exception_channel_u exception_ch_hw = { 0 };
	ecpri_hwio_def_ecpri_endp_cfg_aggr_n_u cfg_aggr = { 0 };
	ecpri_hwio_def_ecpri_endp_cfg_aggr_n_u cfg_aggr_hw = { 0 };
	ecpri_hwio_def_ecpri_endp_nfapi_reassembly_cfg_n_u nfapi_cfg = { 0 };
	ecpri_hwio_def_ecpri_endp_nfapi_reassembly_cfg_n_u nfapi_cfg_hw = { 0 };
	ecpri_hwio_def_ecpri_endp_gsi_cfg_n_u endp_gsi_cfg = { 0 };
	ecpri_hwio_def_ecpri_endp_gsi_cfg_n_u endp_gsi_cfg_hw = { 0 };

	DMA_UT_LOG("Start DMA HW registers verification\n");

	if (!ecpri_dma_ctx || !ecpri_dma_ctx->endp_map) {
		DMA_UT_TEST_FAIL_REPORT("eCPRI DMA CTX isn't ready");
		return -EINVAL;
	}

	endp_map = ecpri_dma_ctx->endp_map;

	for (i = 0; i < ecpri_dma_ctx->ecpri_dma_num_endps; i++) {
		memset(&endp_gsi_cfg, 0, sizeof(endp_gsi_cfg));
		memset(&endp_gsi_cfg_hw, 0, sizeof(endp_gsi_cfg_hw));

		endp_gsi_cfg_hw.value = ecpri_dma_hal_read_reg_n(
			ECPRI_ENDP_GSI_CFG_n, i);

		if (endp_map[i].valid) {
			/* Verify ENDP is enabled */
			endp_gsi_cfg.def.endp_en = 1;

			if (endp_gsi_cfg.value != endp_gsi_cfg_hw.value) {
				DMA_UT_LOG(
					"ENDP %d is expected to be enabled\n", i);
				DMA_UT_TEST_FAIL_REPORT(
					"ENDP is expected to be valid");
				return -EFAULT;
			}

			if (endp_map[i].is_exception) {
				/* Verify ENDP is configured as exception */
				memset(&exception_ch, 0, sizeof(exception_ch));
				memset(&exception_ch_hw, 0, sizeof(exception_ch_hw));
				exception_ch.def.enable = 1;
				exception_ch.def.channel = i;

				exception_ch_hw.value = ecpri_dma_hal_read_reg(
					ECPRI_DMA_EXCEPTION_CHANNEL);

				if (exception_ch.value != exception_ch_hw.value) {
					DMA_UT_LOG(
						"ENDP %d is expected to be exception ENDP\n", i);
					DMA_UT_TEST_FAIL_REPORT(
						"Excepetion ENDP isn't configured as expected");
					return -EFAULT;
				}
				if (ecpri_dma_ctx->exception_endp != i) {
					DMA_UT_LOG(
						"ENDP %d is expected to be exception ENDP\n", i);
					DMA_UT_TEST_FAIL_REPORT(
						"Excepetion ENDP isn't configured as expected in ecpri_ctx");
					return -EFAULT;
				}
			}

			switch (endp_map[i].dir) {
			case ECPRI_DMA_ENDP_DIR_SRC:
				/* Verify SRC ENDP configs */
				memset(&endp_cfg_dest, 0,
					sizeof(endp_cfg_dest));
				memset(&endp_cfg_xbar, 0,
					sizeof(endp_cfg_xbar));
				memset(&endp_cfg_dest_hw, 0,
					sizeof(endp_cfg_dest_hw));
				memset(&endp_cfg_xbar_hw, 0,
					sizeof(endp_cfg_xbar_hw));

				switch (endp_map[i].stream_mode) {
				case ECPRI_DMA_ENDP_STREAM_MODE_M2M:
					endp_cfg_dest.def.use_dest_cfg = 1;
					endp_cfg_dest.def.dest_mem_channel =
						endp_map[i].dest;

					endp_cfg_dest_hw.value = ecpri_dma_hal_read_reg_n(
						ECPRI_ENDP_CFG_DEST_n, i);

					if (endp_cfg_dest.value != endp_cfg_dest_hw.value) {
						DMA_UT_LOG(
							"ENDP %d Destination isn't configured correctly\n",
							i);
						DMA_UT_TEST_FAIL_REPORT(
							"ENDP DEST register isn't configured correctly");
						return -EFAULT;
					}
					break;

				case ECPRI_DMA_ENDP_STREAM_MODE_M2S:
					endp_cfg_dest.def.use_dest_cfg = 0;
					endp_cfg_xbar.def.dest_stream =
						endp_map[i].dest;
					endp_cfg_xbar.def.xbar_tid =
						endp_map[i].tid.value;
					//TODO: Below are required for nFAPI
					//endp_cfg_xbar.xbar_user = Get from Core driver, need API
					endp_cfg_xbar.def.l2_segmentation_en =
						endp_map[i].is_nfapi ? 1 : 0;

					endp_cfg_dest_hw.value = ecpri_dma_hal_read_reg_n(
						ECPRI_ENDP_CFG_DEST_n, i);
					endp_cfg_xbar_hw.value = ecpri_dma_hal_read_reg_n(
						ECPRI_ENDP_CFG_XBAR_n, i);

					if (endp_cfg_dest.value != endp_cfg_dest_hw.value) {
						DMA_UT_LOG(
							"ENDP %d Destination isn't configured correctly\n",
							i);
						DMA_UT_TEST_FAIL_REPORT(
							"ENDP DEST register isn't configured correctly");
						return -EFAULT;
					}
					if (endp_cfg_xbar.value != endp_cfg_xbar_hw.value) {
						DMA_UT_LOG(
							"ENDP %d XBAR CFG isn't configured correctly\n",
							i);
						DMA_UT_TEST_FAIL_REPORT(
							"XBAR CFG register isn't configured correctly");
						return -EFAULT;
					}
					break;
				default:
					DMA_UT_TEST_FAIL_REPORT("SRC ENDP isn't M2M or S2M");
					return -EFAULT;
					break;
				}
				break;
			case ECPRI_DMA_ENDP_DIR_DEST:
				/* Verify DEST ENDP configs */
				switch (endp_map[i].stream_mode) {
				case ECPRI_DMA_ENDP_STREAM_MODE_M2M:
					break;
				case ECPRI_DMA_ENDP_STREAM_MODE_S2M:
					if (endp_map[i].is_nfapi) {
						memset(&cfg_aggr, 0,
							sizeof(cfg_aggr));
						memset(&cfg_aggr_hw, 0,
							sizeof(cfg_aggr_hw));
						memset(&nfapi_cfg, 0,
							sizeof(nfapi_cfg));
						memset(&nfapi_cfg_hw, 0,
							sizeof(nfapi_cfg_hw));

						cfg_aggr.def.aggr_type = 1;
						nfapi_cfg.def.vm_id =
							endp_map[i]
							.nfapi_dest_vm_id;

						cfg_aggr_hw.value = ecpri_dma_hal_read_reg_n(
							ECPRI_ENDP_CFG_AGGR_n, i);
						nfapi_cfg_hw.value = ecpri_dma_hal_read_reg_n(
							ECPRI_ENDP_NFAPI_REASSEMBLY_CFG_n, i);

						if (cfg_aggr.value != cfg_aggr_hw.value) {
							DMA_UT_LOG(
								"ENDP %d AGGR isn't configured correctly\n",
								i);
							DMA_UT_TEST_FAIL_REPORT(
								"ENDP AGGR isn't configured correctly");
							return -EFAULT;
						}
						if (nfapi_cfg.value != nfapi_cfg_hw.value) {
							DMA_UT_LOG(
								"ENDP %d Reassembly isn't configured correctly\n",
								i);
							DMA_UT_TEST_FAIL_REPORT(
								"ENDP Reassembly isn't configured correctly");
							return -EFAULT;
						}
						break;
					}
					break;
				default:
					DMA_UT_TEST_FAIL_REPORT("DEST ENDP isn't M2M or S2M");
					return -EFAULT;
					break;
				}
				break;
			default:
				DMA_UT_TEST_FAIL_REPORT("ENDP isn't SRC nor DEST");
				return -EFAULT;
				break;
			}
		} else {
			/* Verify ENDP is not enabled */
			endp_gsi_cfg.def.endp_en = 0;

			if (endp_gsi_cfg.value != endp_gsi_cfg_hw.value) {
				DMA_UT_LOG(
					"ENDP %d is expected to be disabled\n",
					i);
				DMA_UT_TEST_FAIL_REPORT(
					"ENDP is expected to be invalid");
				return -EFAULT;
			}
		}
	}

	return 0;
}

static int ecpri_dma_test_driver_init_gsi_hw(void *priv)
{
	int i = 0;
	struct ecpri_dma_endp_context *endp_ctx;
	const struct dma_gsi_ep_config *endp_map;
	struct gsihal_reg_ch_k_cntxt_0 cntx0_hw;

	DMA_UT_LOG("Start GSI HW registers verification\n");

	if (!ecpri_dma_ctx) {
		DMA_UT_TEST_FAIL_REPORT("eCPRI DMA CTX isn't ready");
		return -EINVAL;
	}

	endp_map = ecpri_dma_ctx->endp_map;
	endp_ctx = ecpri_dma_ctx->endp_ctx;

	for (i = 0; i < ecpri_dma_ctx->ecpri_dma_num_endps; i++) {
		if (endp_ctx[i].valid) {
			memset(&cntx0_hw, 0, sizeof(cntx0_hw));

			gsihal_read_reg_nk_fields(GSI_EE_n_GSI_CH_k_CNTXT_0,
				ecpri_dma_ctx->ee,
				endp_map[i].dma_gsi_chan_num, &cntx0_hw);

			if (cntx0_hw.element_size != GSI_EVT_RING_RE_SIZE_16B) {
				DMA_UT_LOG(
					"ENDP %d element size isn't configured correctly\n",
					i);
				DMA_UT_TEST_FAIL_REPORT(
					"ENDP element size isn't configured correctly");
				return -EFAULT;
			}
			if (cntx0_hw.chtype_protocol != GSI_CHAN_PROT_MHI) {
				DMA_UT_LOG(
					"ENDP %d protocol isn't configured correctly\n",
					i);
				DMA_UT_TEST_FAIL_REPORT(
					"ENDP protocol isn't configured correctly");
				return -EFAULT;
			}
			if (cntx0_hw.ee != endp_map[i].ee) {
				DMA_UT_LOG(
					"ENDP %d ee isn't configured correctly\n",
					i);
				DMA_UT_TEST_FAIL_REPORT(
					"ENDP ee isn't configured correctly");
				return -EFAULT;
			}
			if (endp_map[i].dir == ECPRI_DMA_ENDP_DIR_SRC)
			{
				if (cntx0_hw.chtype_dir != GSI_CHAN_DIR_TO_GSI) {
					DMA_UT_LOG(
						"ENDP %d direction isn't configured correctly\n",
						i);
					DMA_UT_TEST_FAIL_REPORT(
						"ENDP direction isn't configured correctly");
					return -EFAULT;
				}
			}
			else
			{
				if (cntx0_hw.chtype_dir != GSI_CHAN_DIR_FROM_GSI) {
					DMA_UT_LOG(
						"ENDP %d direction isn't configured correctly\n",
						i);
					DMA_UT_TEST_FAIL_REPORT(
						"ENDP direction isn't configured correctly");
					return -EFAULT;
				}
			}
			/* CH is valid therefor expected to be at least allocated */
			if (cntx0_hw.chstate == GSI_CHAN_STATE_NOT_ALLOCATED) {
				DMA_UT_LOG(
					"ENDP %d CH state isn't configured correctly\n",
					i);
				DMA_UT_TEST_FAIL_REPORT(
					"ENDP CH state isn't configured correctly");
				return -EFAULT;
			}
		}
	}

	return 0;
}
/* Suite definition block */
DMA_UT_DEFINE_SUITE_START(driver_init, "Driver init suite",
			  ecpri_dma_test_driver_init_suite_setup,
			  ecpri_dma_test_driver_init_suite_teardown){
	DMA_UT_ADD_TEST(
		dtsi_parsing_v1_ru,
		"This test will verify DTSi parsing of the driver for V1 RU flavor",
		ecpri_dma_test_driver_init_dtsi_parsing_v1_ru, false,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
	DMA_UT_ADD_TEST(
		dtsi_parsing_v1_du_pcie,
		"This test will verify DTSi parsing of the driver for V1 DU PCIe flavor",
		ecpri_dma_test_driver_init_dtsi_parsing_v1_du_pcie, false,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
	DMA_UT_ADD_TEST(
		dtsi_parsing_v1_du_l2,
		"This test will verify DTSi parsing of the driver for V1 DU L2 flavor",
		ecpri_dma_test_driver_init_dtsi_parsing_v1_du_l2, false,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
	DMA_UT_ADD_TEST(
		dma_paths,
		"This test will verify DMA HW was configured correctly",
		ecpri_dma_test_driver_init_dma_paths, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),
	DMA_UT_ADD_TEST(
		gsi_hw,
		"This test will verify GSI HW was configured correctly",
		ecpri_dma_test_driver_init_gsi_hw, true,
		ECPRI_HW_V1_0, ECPRI_HW_MAX),

} DMA_UT_DEFINE_SUITE_END(driver_init);
