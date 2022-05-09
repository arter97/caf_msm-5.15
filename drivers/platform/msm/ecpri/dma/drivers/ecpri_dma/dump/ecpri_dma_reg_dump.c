/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "ecpri_dma_reg_dump.h"

#define READ_DMA_REG_ARR_N_K(reg_name, substruct, field_name) do {            \
		for (n = 0; n < GEN_ARR_SIZE_n(reg_name); n++) {                      \
			for (k = 0; k < GEN_ARR_SIZE_k(reg_name) + 1; k++) {              \
					ecpri_dma_reg_save.substruct.field_name.arr[n][k].value = \
					ecpri_dma_hal_read_reg_nk(reg_name, n, k);                \
			}													              \
		}															          \
	} while(0)

#define READ_DMA_REG_N_K(reg_name, substruct, field_name, __n, __k)     \
	ecpri_dma_reg_save.substruct.field_name.value =					    \
	ecpri_dma_hal_read_reg_nk(reg_name, __n, __k)


#define READ_DMA_REG_ARR_N(reg_name, substruct, field_name) do {        \
		for (n = 0; n < GEN_ARR_SIZE_n(reg_name); n++) {				\
				ecpri_dma_reg_save.substruct.field_name[n].value =      \
				ecpri_dma_hal_read_reg_n(reg_name, n);                  \
		}																\
	} while(0)

#define READ_DMA_REG_N(reg_name, substruct, field_name) do {            \
		for (n = 0; n < GEN_ARR_SIZE_n(reg_name); n++) {				\
				ecpri_dma_reg_save.substruct.field_name.value =         \
				ecpri_dma_hal_read_reg_n(reg_name, n);                  \
		}																\
	} while(0)

#define READ_DMA_REG_K(reg_name, substruct, field_name) do {            \
		for (k = 0; k < GEN_ARR_SIZE_k(reg_name); k++) {				\
				ecpri_dma_reg_save.substruct.field_name.value =         \
				ecpri_dma_hal_read_reg_nk(reg_name, 0, k);              \
		}																\
	} while(0)

#define READ_DMA_REG(reg_name, substruct, field_name)                   \
	ecpri_dma_reg_save.substruct.field_name.value =                     \
	ecpri_dma_hal_read_reg(reg_name)

#define WRITE_DMA_REG(reg_name, val)                                    \
	ecpri_dma_hal_write_reg(reg_name, val)


#define READ_GSI_REG_ARR_N_K(reg_name, substruct, field_name) do {            \
		for (n = 0; n < GEN_ARR_SIZE_n(reg_name); n++) {                      \
			for (k = 0; k < GEN_ARR_SIZE_k(reg_name); k++) {                  \
					ecpri_dma_reg_save.substruct.field_name.arr[n][k].value = \
					gsihal_read_reg_nk(reg_name, n, k);                       \
			}													              \
		}															          \
	} while(0)

#define READ_GSI_REG_N_K(reg_name, substruct, field_name, __n, __k)     \
	ecpri_dma_reg_save.substruct.field_name.value =					    \
	gsihal_read_reg_nk(reg_name, __n, __k)


#define READ_GSI_REG_ARR_N(reg_name, substruct, field_name) do {        \
		for (n = 0; n < GEN_ARR_SIZE_n(reg_name); n++) {				\
				ecpri_dma_reg_save.substruct.field_name[n].value =      \
				gsihal_read_reg_n(reg_name, n);                         \
		}																\
	} while(0)

#define READ_GSI_REG_N(reg_name, substruct, field_name) do {            \
		for (n = 0; n < GEN_ARR_SIZE_n(reg_name); n++) {				\
				ecpri_dma_reg_save.substruct.field_name.value =         \
				gsihal_read_reg_n(reg_name, n);                         \
		}																\
	} while(0)

#define READ_GSI_REG_K(reg_name, substruct, field_name) do {            \
		for (k = 0; k < GEN_ARR_SIZE_k(reg_name); k++) {				\
				ecpri_dma_reg_save.substruct.field_name.value =         \
				gsihal_read_reg_nk(reg_name, 0, k);                     \
		}																\
	} while(0)

#define READ_GSI_REG(reg_name, substruct, field_name)                   \
	ecpri_dma_reg_save.substruct.field_name.value =                     \
	gsihal_read_reg(reg_name)

#define READ_GSI_SHRAM_REG(reg_name, substruct, field_name, offset)     \
	ecpri_dma_reg_save.substruct.field_name.value =                     \
	gsihal_read_reg_n(reg_name, offset)

#define WRITE_GSI_REG(reg_name, val)                                    \
	gsihal_write_reg(reg_name, val)

/* DMA registers */
struct ecpri_dma_reg_save_dma_gen_s {
	ecpri_hwio_def_ecpri_state_gsi_if_u
		ecpri_state_gsi_if;
	ecpri_hwio_def_ecpri_state_gsi_tlv_fifo_empty_n_u
		ecpri_state_gsi_tlv_fifo_empty_n[
			GEN_ARR_SIZE_n(ECPRI_STATE_GSI_TLV_FIFO_EMPTY_n)];
	ecpri_hwio_def_ecpri_state_gsi_aos_fifo_empty_n_u
		ecpri_state_gsi_aos_fifo_empty_n[
			GEN_ARR_SIZE_n(ECPRI_STATE_GSI_TLV_FIFO_EMPTY_n)];
	ecpri_hwio_def_ecpri_spare_reg_u
		ecpri_spare_reg;
	ecpri_hwio_def_ecpri_hw_params_0_u
		ecpri_hw_params_0;
	ecpri_hwio_def_ecpri_hw_params_2_u
		ecpri_hw_params_2;
	ecpri_hwio_def_ecpri_gen_cfg_u
		ecpri_gen_cfg;
	ecpri_hwio_def_ecpri_clkon_cfg_u
		ecpri_clkon_cfg;
	ecpri_hwio_def_ecpri_qmb_cfg_u
		ecpri_qmb_cfg;
	ecpri_hwio_def_ecpri_qmb_0_status_u
		ecpri_qmb_0_status;
	ecpri_hwio_def_ecpri_qmb_1_status_u
		ecpri_qmb_1_status;
	ecpri_hwio_def_ecpri_aos_cfg_u
		ecpri_aos_cfg;
	ecpri_hwio_def_ecpri_timers_xo_clk_div_cfg_u
		ecpri_timers_xo_clk_div_cfg;
	ecpri_hwio_def_ecpri_timers_pulse_gran_cfg_u
		ecpri_timers_pulse_gran_cfg;
	ecpri_hwio_def_ecpri_qtime_lsb_u
		ecpri_qtime_lsb;
	ecpri_hwio_def_ecpri_qtime_msb_u
		ecpri_qtime_msb;
	ecpri_hwio_def_ecpri_snoc_fec_u
		ecpri_snoc_fec;
	ecpri_hwio_def_ecpri_qmb_cfg_param_u
		ecpri_qmb_cfg_param;
	ecpri_hwio_def_ecpri_dma_fl_packet_id_ctrl_u
		ecpri_dma_fl_packet_id_ctrl;
	ecpri_hwio_def_ecpri_dma_fl_packet_id_status_u
		ecpri_dma_fl_packet_id_status;
	ecpri_hwio_def_ecpri_dma_fl_memory_sector_ctrl_u
		ecpri_dma_fl_memory_sector_ctrl;
	ecpri_hwio_def_ecpri_dma_fl_memory_sector_status_u
		ecpri_dma_fl_memory_sector_status;
	ecpri_hwio_def_ecpri_dma_stream_ctrl_u
		ecpri_dma_stream_ctrl;
	ecpri_hwio_def_ecpri_dma_stream_status_u
		ecpri_dma_stream_status;
	ecpri_hwio_def_ecpri_dma_stream_arb_u
		ecpri_dma_stream_arb;
	ecpri_hwio_def_ecpri_dma_xbar_u
		ecpri_dma_xbar;
	ecpri_hwio_def_ecpri_dma_dbg_ctrl_u
		ecpri_dma_dbg_ctrl;
	ecpri_hwio_def_ecpri_dma_gbl_cfg_u
		ecpri_dma_gbl_cfg;
	ecpri_hwio_def_ecpri_dma_testbus_ctrl_u
		ecpri_dma_testbus_ctrl;
	ecpri_hwio_def_ecpri_dma_gp_reg3_u
		ecpri_dma_gp_reg3;
	ecpri_hwio_def_ecpri_dma_aos_fifo_stat_u
		ecpri_dma_aos_fifo_stat;
	ecpri_hwio_def_ecpri_dma_gp_stat1_u
		ecpri_dma_gp_stat1;
	ecpri_hwio_def_ecpri_dma_testbus_u
		ecpri_dma_testbus[ECPRI_DMA_REG_SAVE_TEST_BUS_MAX];
	ecpri_hwio_def_ecpri_dma_testbus_u
		ecpri_dma_testbus_internal[ECPRI_DMA_REG_SAVE_TEST_BUS_MAX] ;
	ecpri_hwio_def_ecpri_dma_gp_stat3_u
		ecpri_dma_gp_stat3;
	ecpri_hwio_def_ecpri_dma_idle_reg_u
		ecpri_dma_idle_reg;
	ecpri_hwio_def_ecpri_dma_exception_channel_u
		ecpri_dma_exception_channel;
	ecpri_hwio_def_ecpri_dma_pkt_drop_full_u
		ecpri_dma_pkt_drop_full;
	ecpri_hwio_def_ecpri_dma_pkt_drop_re_empty_u
		ecpri_dma_pkt_drop_re_empty;
	ecpri_hwio_def_ecpri_dma_tpdm_cfg_u
		ecpri_dma_tpdm_cfg;
};

struct ecpri_dma_reg_save_dma_gen_ee_s {
	ecpri_hwio_def_ecpri_irq_stts_ee_n_u
		irq_stts_ee_n;
	ecpri_hwio_def_ecpri_irq_en_ee_n_u
		irq_en_ee_n;
	ecpri_hwio_def_ecpri_gsi_ee_vfid_n_u
		gsi_ee_vfid_n;
};

struct ecpri_dma_reg_save_dma_dbg_s {
	ecpri_hwio_def_ecpri_irq_stts_ee_n_u
		ecpri_irq_stts_ee_n[GEN_ARR_SIZE_n(ECPRI_IRQ_STTS_EE_n)];
	ecpri_hwio_def_ecpri_snoc_monitoring_cfg_u
		ecpri_snoc_monitoring_cfg;
	ecpri_hwio_def_ecpri_qmb0_snoc_monitor_cnt_u
		ecpri_qmb0_snoc_monitor_cnt;
	ecpri_hwio_def_ecpri_qmb1_snoc_monitor_cnt_u
		ecpri_qmb1_snoc_monitor_cnt;
	ecpri_hwio_def_ecpri_gsi_snoc_monitor_cnt_u
		ecpri_gsi_snoc_monitor_cnt;
	ecpri_hwio_def_ecpri_dst_ackmngr_cmdq_status_u
		ecpri_dst_ackmngr_cmdq_status;
	ecpri_hwio_def_ecpri_dst_ackmngr_cmdq_status_empty_n_u
		ecpri_dst_ackmngr_cmdq_status_empty_n[
			GEN_ARR_SIZE_n(ECPRI_DST_ACKMNGR_CMDQ_STATUS_EMPTY_n)];
	ecpri_hwio_def_ecpri_dst_ackmngr_cmdq_count_n_u
		ecpri_dst_ackmngr_cmdq_count_n[
			GEN_ARR_SIZE_n(ECPRI_DST_ACKMNGR_CMDQ_COUNT_n)];
};

struct ecpri_dma_reg_save_dma_endps_s {
	ecpri_hwio_def_ecpri_endp_cfg_destn_u
		ecpri_endp_cfg_destn[GEN_ARR_SIZE_n(ECPRI_ENDP_CFG_DESTn)];
	ecpri_hwio_def_ecpri_endp_cfg_xbarn_u
		ecpri_endp_cfg_xbarn[GEN_ARR_SIZE_n(ECPRI_ENDP_CFG_XBARn)];
	ecpri_hwio_def_ecpri_endp_gsi_cfg_n_u
		ecpri_endp_gsi_cfg_n[GEN_ARR_SIZE_n(ECPRI_ENDP_GSI_CFG_n)];
	ecpri_hwio_def_ecpri_endp_init_ctrl_status_n_u
		ecpri_endp_init_ctrl_status_n[
			GEN_ARR_SIZE_n(ECPRI_ENDP_INIT_CTRL_STATUS_n)];
	ecpri_hwio_def_ecpri_endp_axi_attr_n_u
		ecpri_endp_axi_attr_n[GEN_ARR_SIZE_n(ECPRI_ENDP_AXI_ATTR_n)];
	ecpri_hwio_def_ecpri_endp_cfg_aggr_n_u
		ecpri_endp_cfg_aggr_n[GEN_ARR_SIZE_n(ECPRI_ENDP_CFG_AGGR_n)];
	ecpri_hwio_def_ecpri_endp_yellow_red_marker_cfg_n_u
		ecpri_endp_yellow_red_marker_cfg_n[
			GEN_ARR_SIZE_n(ECPRI_ENDP_YELLOW_RED_MARKER_CFG_n)];
	ecpri_hwio_def_ecpri_endp_nfapi_reassembly_cfg_n_u
		ecpri_endp_nfapi_reassembly_cfg_n[
			GEN_ARR_SIZE_n(ECPRI_ENDP_NFAPI_REASSEMBLY_CFG_n)];
	ecpri_hwio_def_ecpri_endp_gsi_cons_bytes_tlv_n_u
		ecpri_endp_gsi_cons_bytes_tlv_n[
			GEN_ARR_SIZE_n(ECPRI_ENDP_GSI_CONS_BYTES_TLV_n)];
	ecpri_hwio_def_ecpri_endp_gsi_cons_bytes_aos_n_u
		ecpri_endp_gsi_cons_bytes_aos_n[
			GEN_ARR_SIZE_n(ECPRI_ENDP_GSI_CONS_BYTES_AOS_n)];
	ecpri_hwio_def_ecpri_endp_gsi_if_fifo_cfg_tlv_n_u
		ecpri_endp_gsi_if_fifo_cfg_tlv_n[
			GEN_ARR_SIZE_n(ECPRI_ENDP_GSI_IF_FIFO_CFG_TLV_n)];
	ecpri_hwio_def_ecpri_endp_gsi_if_fifo_cfg_aos_n_u
		ecpri_endp_gsi_if_fifo_cfg_aos_n[
			GEN_ARR_SIZE_n(ECPRI_ENDP_GSI_IF_FIFO_CFG_AOS_n)];
	ecpri_hwio_def_ecpri_endp_cfg_vfid_n_u
		ecpri_endp_cfg_vfid_n[GEN_ARR_SIZE_n(ECPRI_ENDP_CFG_VFID_n)];
};

struct ecpri_dma_reg_save_dma_nfapi_cfg_s {
	ecpri_hwio_def_ecpri_nfapi_cfg_0_u ecpri_nfapi_cfg_0;
	ecpri_hwio_def_ecpri_nfapi_cfg_1_u ecpri_nfapi_cfg_1;
	ecpri_hwio_def_ecpri_nfapi_cfg_2_u ecpri_nfapi_cfg_2;
};

struct ecpri_dma_reg_save_dma_nfapi_reassembly_s {
	ecpri_hwio_def_ecpri_nfapi_reassembly_cfg_0_u
		ecpri_nfapi_reassembly_cfg_0;
	ecpri_hwio_def_ecpri_nfapi_reassembly_fec_0_u
		ecpri_nfapi_reassembly_fec_0;
	ecpri_hwio_def_ecpri_nfapi_reassembly_fec_1_u
		ecpri_nfapi_reassembly_fec_1;
	ecpri_hwio_def_ecpri_nfapi_reassembly_fec_2_u
		ecpri_nfapi_reassembly_fec_2;
	ecpri_hwio_def_ecpri_nfapi_reassembly_fec_3_u
		ecpri_nfapi_reassembly_fec_3;
	ecpri_hwio_def_ecpri_nfapi_reassembly_fec_4_u
		ecpri_nfapi_reassembly_fec_4;
	ecpri_hwio_def_ecpri_nfapi_reassembly_fec_5_u
		ecpri_nfapi_reassembly_fec_5;
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_total_sdu_length_pkt_cnt_u
		nfapi_reassembly_error_bad_total_sdu_length_pkt_cnt;
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_total_sdu_length_byte_cnt_lsb_u
		ecpri_nfapi_reassembly_error_bad_total_sdu_length_byte_cnt_lsb;
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_total_sdu_length_byte_cnt_msb_u
		ecpri_nfapi_reassembly_error_bad_total_sdu_length_byte_cnt_msb;
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_timestamp_pkt_cnt_u
		ecpri_nfapi_reassembly_error_bad_timestamp_pkt_cnt;
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_timestamp_byte_cnt_lsb_u
		ecpri_nfapi_reassembly_error_bad_timestamp_byte_cnt_lsb;
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_timestamp_byte_cnt_msb_u
		ecpri_nfapi_reassembly_error_bad_timestamp_byte_cnt_msb;
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_pkt_cnt_u
		ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_pkt_cnt;
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_byte_cnt_lsb_u
		ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_byte_cnt_lsb;
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_byte_cnt_msb_u
		ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_byte_cnt_msb;
	ecpri_hwio_def_ecpri_nfapi_reassembly_state_u
		ecpri_nfapi_reassembly_state;
	ecpri_hwio_def_ecpri_nfapi_reassembly_vm_cfg_0_n_u
		ecpri_nfapi_reassembly_vm_cfg_0_n[
			GEN_ARR_SIZE_n(ECPRI_NFAPI_REASSEMBLY_VM_CFG_0_n)];
	ecpri_hwio_def_ecpri_nfapi_reassembly_vm_cfg_1_n_u
		ecpri_nfapi_reassembly_vm_cfg_1_n[
			GEN_ARR_SIZE_n(ECPRI_NFAPI_REASSEMBLY_VM_CFG_1_n)];
	ecpri_hwio_def_ecpri_nfapi_reassembly_vm_cfg_2_n_u
		ecpri_nfapi_reassembly_vm_cfg_2_n[
			GEN_ARR_SIZE_n(ECPRI_NFAPI_REASSEMBLY_VM_CFG_2_n)];
};

struct ecpri_dma_reg_save_dma_nfapi_nso_cfg_s {
	ecpri_hwio_def_ecpri_nso_cfg_u
		ecpri_nso_cfg;
	ecpri_hwio_def_ecpri_nso_jumbo_pkt_cfg_u
		ecpri_nso_jumbo_pkt_cfg;
	ecpri_hwio_def_ecpri_nso_short_pkt_cfg_u
		ecpri_nso_short_pkt_cfg;
	ecpri_hwio_def_ecpri_nso_jumbo_pkt_en_n_u
		ecpri_nso_jumbo_pkt_en_n[
			GEN_ARR_SIZE_n(ECPRI_NSO_JUMBO_PKT_EN_n)];
	ecpri_hwio_def_ecpri_nso_len_err_status_1_u
		ecpri_nso_len_err_status_1;
	ecpri_hwio_def_ecpri_nso_len_err_status_2_u
		ecpri_nso_len_err_status_2;
	ecpri_hwio_def_ecpri_nso_len_err_status_3_u
		ecpri_nso_len_err_status_3;
	ecpri_hwio_def_ecpri_nso_len_err_status_hdr_n_u
		ecpri_nso_len_err_status_hdr_n[
			GEN_ARR_SIZE_n(ECPRI_NSO_LEN_ERR_STATUS_HDR_n)];
	ecpri_hwio_def_ecpri_nso_dbg_cntxt_n_info_1_u
		ecpri_nso_dbg_cntxt_n_info_1;
	ecpri_hwio_def_ecpri_nso_dbg_cntxt_n_info_2_u
		ecpri_nso_dbg_cntxt_n_info_2;
	ecpri_hwio_def_ecpri_nso_dbg_misc_info_u
		ecpri_nso_dbg_misc_info;
};

struct ecpri_dma_reg_save_dma_nfapi_s {
	struct ecpri_dma_reg_save_dma_nfapi_cfg_s nfapi_cfg;
	struct ecpri_dma_reg_save_dma_nfapi_reassembly_s nfapi_reassembly;
	struct ecpri_dma_reg_save_dma_nfapi_nso_cfg_s nfapi_nso_cfg;
};

struct ecpri_dma_reg_save_dma_dpl_s {
	ecpri_hwio_def_ecpri_dma_dpl_cfg_u
		ecpri_dma_dpl_cfg;
	ecpri_hwio_def_ecpri_dpl_trig_ctrl_n_u
		ecpri_dpl_trig_ctrl_n[GEN_ARR_SIZE_n(ECPRI_DPL_TRIG_CTRL_n)];
	ecpri_hwio_def_ecpri_dpl_trig_a_n_u
		ecpri_dpl_trig_a_n[GEN_ARR_SIZE_n(ECPRI_DPL_TRIG_A_n)];
	ecpri_hwio_def_ecpri_dpl_trig_b_n_u
		ecpri_dpl_trig_b_n[GEN_ARR_SIZE_n(ECPRI_DPL_TRIG_B_n)];
};

struct ecpri_dma_reg_save_dma_markers_s {
	ecpri_hwio_def_ecpri_yellow_marker_below_n_u
		ecpri_yellow_marker_below_n[
			GEN_ARR_SIZE_n(ECPRI_YELLOW_MARKER_BELOW_n)];
	ecpri_hwio_def_ecpri_yellow_marker_below_en_n_u
		ecpri_yellow_marker_below_en_n[
			GEN_ARR_SIZE_n(ECPRI_YELLOW_MARKER_BELOW_EN_n)];
	ecpri_hwio_def_ecpri_red_marker_below_n_u
		ecpri_red_marker_below_n[
			GEN_ARR_SIZE_n(ECPRI_RED_MARKER_BELOW_n)];
	ecpri_hwio_def_ecpri_red_marker_below_en_n_u
		ecpri_red_marker_below_en_n[
			GEN_ARR_SIZE_n(ECPRI_RED_MARKER_BELOW_EN_n)];
	ecpri_hwio_def_ecpri_yellow_marker_shadow_n_u
		ecpri_yellow_marker_shadow_n[
			GEN_ARR_SIZE_n(ECPRI_YELLOW_MARKER_SHADOW_n)];
	ecpri_hwio_def_ecpri_red_marker_shadow_n_u
		ecpri_red_marker_shadow_n[
			GEN_ARR_SIZE_n(ECPRI_RED_MARKER_SHADOW_n)];
	ecpri_hwio_def_ecpri_yellow_marker_above_n_u
		ecpri_yellow_marker_above_n[
			GEN_ARR_SIZE_n(ECPRI_YELLOW_MARKER_ABOVE_n)];
	ecpri_hwio_def_ecpri_yellow_marker_above_en_n_u
		ecpri_yellow_marker_above_en_n[
			GEN_ARR_SIZE_n(ECPRI_YELLOW_MARKER_ABOVE_EN_n)];
	ecpri_hwio_def_ecpri_red_marker_above_n_u
		ecpri_red_marker_above_n[
			GEN_ARR_SIZE_n(ECPRI_RED_MARKER_ABOVE_n)];
	ecpri_hwio_def_ecpri_red_marker_above_en_n_u
		ecpri_red_marker_above_en_n[
			GEN_ARR_SIZE_n(ECPRI_RED_MARKER_ABOVE_EN_n)];
};

struct dma_regs_save_hierarchy_s {
	struct ecpri_dma_reg_save_dma_gen_s     gen;
	struct ecpri_dma_reg_save_dma_gen_ee_s  gen_ee[ECPRI_DMA_MAX_EE];
	struct ecpri_dma_reg_save_dma_dbg_s     dbg;
	struct ecpri_dma_reg_save_dma_endps_s   endps[ECPRI_DMA_ENDP_NUM_MAX];
	struct ecpri_dma_reg_save_dma_nfapi_s   nfapi;
	struct ecpri_dma_reg_save_dma_dpl_s     dpl;
	struct ecpri_dma_reg_save_dma_markers_s markers;
};

/* GSI registers */
struct ecpri_dma_reg_save_gsi_gen_s {
	ecpri_gsi_hwio_def_gsi_gsi_cfg_u         gsi_cfg;
	ecpri_gsi_hwio_def_gsi_gsi_ree_cfg_u     gsi_ree_cfg;
	ecpri_gsi_hwio_def_gsi_gsi_inst_ram_n_u  gsi_inst_ram_n;
};

struct ecpri_dma_reg_save_gsi_gen_ee_s {
	ecpri_gsi_hwio_def_gsi_gsi_manager_ee_qos_n_u
		gsi_manager_ee_qos_n;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_status_u
		ee_n_gsi_status;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_hw_param_0_u
		ee_n_gsi_hw_param_0;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_hw_param_1_u
		ee_n_gsi_hw_param_1;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_hw_param_2_u
		ee_n_gsi_hw_param_2;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_hw_param_3_u
		ee_n_gsi_hw_param_3;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_hw_param_4_u
		ee_n_gsi_hw_param_4;
	ecpri_gsi_hwio_def_gsi_ee_n_cntxt_type_irq_u
		ee_n_cntxt_type_irq;
	ecpri_gsi_hwio_def_gsi_ee_n_cntxt_type_irq_msk_u
		ee_n_cntxt_type_irq_msk;
	GEN_REGS_ARRAY_2D(ecpri_gsi_hwio_def_gsi_ee_n_cntxt_src_gsi_ch_irq_k_u,
		GSI_EE_n_CNTXT_SRC_GSI_CH_IRQ_k) ee_n_cntxt_src_gsi_ch_irq_k;
	GEN_REGS_ARRAY_2D(ecpri_gsi_hwio_def_gsi_ee_n_cntxt_src_ev_ch_irq_k_u,
		GSI_EE_n_CNTXT_SRC_EV_CH_IRQ_k)	ee_n_cntxt_src_ev_ch_irq_k;
	GEN_REGS_ARRAY_2D(ecpri_gsi_hwio_def_gsi_ee_n_cntxt_src_gsi_ch_irq_msk_k_u,
		GSI_EE_n_CNTXT_SRC_GSI_CH_IRQ_MSK_k) ee_n_cntxt_src_gsi_ch_irq_msk_k;
	GEN_REGS_ARRAY_2D(ecpri_gsi_hwio_def_gsi_ee_n_cntxt_src_ev_ch_irq_msk_k_u,
		GSI_EE_n_CNTXT_SRC_EV_CH_IRQ_MSK_k)	ee_n_cntxt_src_ev_ch_irq_msk_k;
	GEN_REGS_ARRAY_2D(ecpri_gsi_hwio_def_gsi_ee_n_cntxt_src_ieob_irq_k_u,
		GSI_EE_n_CNTXT_SRC_IEOB_IRQ_k) ee_n_cntxt_src_ieob_irq_k;
	GEN_REGS_ARRAY_2D(ecpri_gsi_hwio_def_gsi_ee_n_cntxt_src_ieob_irq_msk_k_u,
		GSI_EE_n_CNTXT_SRC_IEOB_IRQ_MSK_k) ee_n_cntxt_src_ieob_irq_msk_k;
	ecpri_gsi_hwio_def_gsi_ee_n_cntxt_gsi_irq_stts_u
		ee_n_cntxt_gsi_irq_stts;
	ecpri_gsi_hwio_def_gsi_ee_n_cntxt_glob_irq_stts_u
		ee_n_cntxt_glob_irq_stts;
	ecpri_gsi_hwio_def_gsi_ee_n_error_log_u
		ee_n_error_log;
	ecpri_gsi_hwio_def_gsi_ee_n_cntxt_scratch_0_u
		ee_n_cntxt_scratch_0;
	ecpri_gsi_hwio_def_gsi_ee_n_cntxt_scratch_1_u
		ee_n_cntxt_scratch_1;
	ecpri_gsi_hwio_def_gsi_ee_n_cntxt_intset_u
		ee_n_cntxt_intset;
	ecpri_gsi_hwio_def_gsi_ee_n_cntxt_msi_base_lsb_u
		ee_n_cntxt_msi_base_lsb;
	ecpri_gsi_hwio_def_gsi_ee_n_cntxt_msi_base_msb_u
		ee_n_cntxt_msi_base_msb;
	ecpri_gsi_hwio_def_gsi_ee_n_cntxt_glob_irq_en_u
		ee_n_glob_irq_en;
	ecpri_gsi_hwio_def_gsi_ee_n_cntxt_gsi_irq_en_u
		ee_n_gsi_irq_en;
	ecpri_gsi_hwio_def_gsi_ee_n_cntxt_int_vec_u
		ee_n_int_vec;
};

/* GSI MCS channel scratch registers save data struct */
struct ecpri_dma_reg_save_gsi_mcs_channel_scratch_regs_s {
	ecpri_gsi_hwio_def_gsi_gsi_shram_n_u scratch_for_seq_low;
	ecpri_gsi_hwio_def_gsi_gsi_shram_n_u scratch_for_seq_high;
};

struct ecpri_dma_reg_save_gsi_ch_cntxt_per_ep_s {
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_cntxt_0_u
		ee_n_gsi_ch_k_cntxt_0;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_cntxt_1_u
		ee_n_gsi_ch_k_cntxt_1;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_cntxt_2_u
		ee_n_gsi_ch_k_cntxt_2;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_cntxt_3_u
		ee_n_gsi_ch_k_cntxt_3;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_cntxt_4_u
		ee_n_gsi_ch_k_cntxt_4;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_cntxt_5_u
		ee_n_gsi_ch_k_cntxt_5;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_cntxt_6_u
		ee_n_gsi_ch_k_cntxt_6;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_cntxt_7_u
		ee_n_gsi_ch_k_cntxt_7;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_cntxt_8_u
		ee_n_gsi_ch_k_cntxt_8;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_re_fetch_read_ptr_u
		ee_n_gsi_ch_k_re_fetch_read_ptr;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_re_fetch_write_ptr_u
		ee_n_gsi_ch_k_re_fetch_write_ptr;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_qos_u ee_n_gsi_ch_k_qos;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_scratch_0_u
		ee_n_gsi_ch_k_scratch_0;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_scratch_1_u
		ee_n_gsi_ch_k_scratch_1;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_scratch_2_u
		ee_n_gsi_ch_k_scratch_2;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_scratch_3_u
		ee_n_gsi_ch_k_scratch_3;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_scratch_4_u
		ee_n_gsi_ch_k_scratch_4;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_scratch_5_u
		ee_n_gsi_ch_k_scratch_5;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_scratch_6_u
		ee_n_gsi_ch_k_scratch_6;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_scratch_7_u
		ee_n_gsi_ch_k_scratch_7;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_scratch_8_u
		ee_n_gsi_ch_k_scratch_8;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_scratch_9_u
		ee_n_gsi_ch_k_scratch_9;
	ecpri_gsi_hwio_def_gsi_gsi_map_ee_n_ch_k_vp_table_u
		gsi_map_ee_n_ch_k_vp_table;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_db_eng_write_ptr_u
		gsi_ch_k_db_eng_write_ptr;
	ecpri_gsi_hwio_def_gsi_ee_n_gsi_ch_k_ch_almst_empty_thrshold_u
		gsi_ch_k_ch_almst_empty_thrshold;
	struct ecpri_dma_reg_save_gsi_mcs_channel_scratch_regs_s
		mcs_channel_scratch;
};

struct ecpri_dma_reg_save_gsi_evt_cntxt_per_ep_s {
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_cntxt_0_u ee_n_ev_ch_k_cntxt_0;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_cntxt_1_u ee_n_ev_ch_k_cntxt_1;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_cntxt_2_u ee_n_ev_ch_k_cntxt_2;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_cntxt_3_u ee_n_ev_ch_k_cntxt_3;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_cntxt_4_u ee_n_ev_ch_k_cntxt_4;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_cntxt_5_u ee_n_ev_ch_k_cntxt_5;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_cntxt_6_u ee_n_ev_ch_k_cntxt_6;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_cntxt_7_u ee_n_ev_ch_k_cntxt_7;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_cntxt_8_u ee_n_ev_ch_k_cntxt_8;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_cntxt_9_u ee_n_ev_ch_k_cntxt_9;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_cntxt_10_u ee_n_ev_ch_k_cntxt_10;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_cntxt_11_u ee_n_ev_ch_k_cntxt_11;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_cntxt_12_u ee_n_ev_ch_k_cntxt_12;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_cntxt_13_u ee_n_ev_ch_k_cntxt_13;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_scratch_0_u ee_n_ev_ch_k_scratch_0;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_scratch_1_u ee_n_ev_ch_k_scratch_1;
	ecpri_gsi_hwio_def_gsi_ee_n_ev_ch_k_scratch_2_u ee_n_ev_ch_k_scratch_2;
	ecpri_gsi_hwio_def_gsi_gsi_debug_ee_n_ev_k_vp_table_u
		gsi_debug_ee_n_ev_k_vp_table;
};

struct ecpri_dma_reg_save_gsi_debug_qsb_regs_s {
	// TODO: Check and verify how to work with log_# via log_sel
	ecpri_gsi_hwio_def_gsi_gsi_debug_qsb_log_sel_s gsi_debug_qsb_log_sel;
	ecpri_gsi_hwio_def_gsi_gsi_debug_qsb_log_0_s gsi_debug_qsb_log_0;
	ecpri_gsi_hwio_def_gsi_gsi_debug_qsb_log_1_s gsi_debug_qsb_log_1;
	ecpri_gsi_hwio_def_gsi_gsi_debug_qsb_log_2_s gsi_debug_qsb_log_2;
	// ---

	ecpri_gsi_hwio_def_gsi_gsi_debug_qsb_log_last_misc_idn_u
		gsi_debug_qsb_log_last_misc_idn[
			GEN_ARR_SIZE_n(GSI_GSI_DEBUG_QSB_LOG_LAST_MISC_IDn)];
};

struct ecpri_dma_reg_save_gsi_qsb_debug_s {
	ecpri_gsi_hwio_def_gsi_gsi_debug_busy_reg_u
		gsi_debug_busy_reg;
	ecpri_gsi_hwio_def_gsi_gsi_debug_qsb_log_last_misc_idn_u
		qsb_log_last_misc_idn;
	ecpri_gsi_hwio_def_gsi_gsi_debug_counter_cfgn_u
		gsi_debug_counter_cfgn;
	ecpri_gsi_hwio_def_gsi_gsi_debug_ree_prefetch_buf_ch_id_u
		gsi_debug_ree_prefetch_buf_ch_id;
	ecpri_gsi_hwio_def_gsi_gsi_debug_ree_prefetch_buf_status_u
		gsi_debug_ree_prefetch_buf_status;
	ecpri_gsi_hwio_def_gsi_gsi_debug_event_pending_k_u
		gsi_debug_event_pending_k;
	ecpri_gsi_hwio_def_gsi_gsi_debug_timer_pending_k_u
		gsi_debug_timer_pending_k;
	ecpri_gsi_hwio_def_gsi_gsi_debug_rd_wr_pending_k_u
		gsi_debug_rd_wr_pending_k;
	struct ecpri_dma_reg_save_gsi_debug_qsb_regs_s gsi_debug_qsb_reg;
};

struct ecpri_dma_reg_save_gsi_test_bus_s {
	u32 test_bus_selector[
		ARRAY_SIZE(ecpri_dma_reg_save_gsi_test_bus_selector_array)];
	ecpri_gsi_hwio_def_gsi_gsi_test_bus_reg_u
		test_bus_reg[
			ARRAY_SIZE(ecpri_dma_reg_save_gsi_test_bus_selector_array)];
};

struct ecpri_dma_reg_save_gsi_shram_ptr_regs_s {
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_ch_cntxt_base_addr_u
		gsi_shram_ptr_ch_cntxt_base_addr;
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_ev_cntxt_base_addr_u
		gsi_shram_ptr_ev_cntxt_base_addr;
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_re_storage_base_addr_u
		gsi_shram_ptr_re_storage_base_addr;
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_re_esc_buf_base_addr_u
		gsi_shram_ptr_re_esc_buf_base_addr;
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_ee_scrach_base_addr_u
		gsi_shram_ptr_ee_scrach_base_addr;
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_func_stack_base_addr_u
		gsi_shram_ptr_func_stack_base_addr;
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_mcs_scratch_base_addr_u
		gsi_shram_ptr_mcs_scratch_base_addr;
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_mcs_scratch1_base_addr_u
		gsi_shram_ptr_mcs_scratch1_base_addr;
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_mcs_scratch2_base_addr_u
		gsi_shram_ptr_mcs_scratch2_base_addr;
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_mcs_scratch3_base_addr_u
		gsi_shram_ptr_mcs_scratch3_base_addr;
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_ch_vp_trans_table_base_addr_u
		gsi_shram_ptr_ch_vp_trans_table_base_addr;
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_ev_vp_trans_table_base_addr_u
		gsi_shram_ptr_ev_vp_trans_table_base_addr;
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_user_info_data_base_addr_u
		gsi_shram_ptr_user_info_data_base_addr;
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_ee_cmd_fifo_base_addr_u
		gsi_shram_ptr_ee_cmd_fifo_base_addr;
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_ch_cmd_fifo_base_addr_u
		gsi_shram_ptr_ch_cmd_fifo_base_addr;
	ecpri_gsi_hwio_def_gsi_gsi_shram_ptr_eve_ed_storage_base_addr_u
		gsi_shram_ptr_eve_ed_storage_base_addr;
};

struct ecpri_dma_reg_save_gsi_iram_ptr_regs_s {
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_ch_cmd_u
		gsi_iram_ptr_ch_cmd;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_ee_generic_cmd_u
		gsi_iram_ptr_ee_generic_cmd;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_ch_db_u
		gsi_iram_ptr_ch_db;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_ev_db_u
		gsi_iram_ptr_ev_db;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_new_re_u
		gsi_iram_ptr_new_re;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_ch_dis_comp_u
		gsi_iram_ptr_ch_dis_comp;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_ch_empty_u
		gsi_iram_ptr_ch_empty;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_event_gen_comp_u
		gsi_iram_ptr_event_gen_comp;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_timer_expired_u
		gsi_iram_ptr_timer_expired;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_write_eng_comp_u
		gsi_iram_ptr_write_eng_comp;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_read_eng_comp_u
		gsi_iram_ptr_read_eng_comp;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_uc_gp_int_u
		gsi_iram_ptr_uc_gp_int;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_int_mod_stoped_u
		gsi_iram_ptr_int_mod_stoped;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_tlv_ch_not_full_u
		gsi_iram_ptr_tlv_ch_not_full;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_msi_db_u
		gsi_iram_ptr_msi_db;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_periph_if_tlv_in_0_u
		gsi_iram_ptr_periph_if_tlv_in_0;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_periph_if_tlv_in_0_u
		gsi_iram_ptr_periph_if_tlv_in_1;
	ecpri_gsi_hwio_def_gsi_gsi_iram_ptr_periph_if_tlv_in_0_u
		gsi_iram_ptr_periph_if_tlv_in_2;
};

struct ecpri_dma_reg_save_gsi_debug_cnt_s {
	ecpri_gsi_hwio_def_gsi_gsi_debug_countern_u
		cnt[GEN_ARR_SIZE_n(GSI_GSI_DEBUG_COUNTERn)];
};

struct ecpri_dma_reg_save_gsi_mcs_prof_regs_s {
	ecpri_gsi_hwio_def_gsi_gsi_mcs_profiling_bp_cnt_lsb_u
		gsi_mcs_profiling_bp_cnt_lsb;
	ecpri_gsi_hwio_def_gsi_gsi_mcs_profiling_bp_cnt_msb_u
		gsi_mcs_profiling_bp_cnt_msb;
	ecpri_gsi_hwio_def_gsi_gsi_mcs_profiling_bp_and_pending_cnt_lsb_u
		gsi_mcs_profiling_bp_and_pending_cnt_lsb;
	ecpri_gsi_hwio_def_gsi_gsi_mcs_profiling_bp_and_pending_cnt_msb_u
		gsi_mcs_profiling_bp_and_pending_cnt_msb;
	ecpri_gsi_hwio_def_gsi_gsi_mcs_profiling_mcs_busy_cnt_lsb_u
		gsi_mcs_profiling_mcs_busy_cnt_lsb;
	ecpri_gsi_hwio_def_gsi_gsi_mcs_profiling_mcs_busy_cnt_msb_u
		gsi_mcs_profiling_mcs_busy_cnt_msb;
	ecpri_gsi_hwio_def_gsi_gsi_mcs_profiling_mcs_idle_cnt_lsb_u
		gsi_mcs_profiling_mcs_idle_cnt_lsb;
	ecpri_gsi_hwio_def_gsi_gsi_mcs_profiling_mcs_idle_cnt_msb_u
		gsi_mcs_profiling_mcs_idle_cnt_msb;
};

struct ecpri_gsi_hwio_def_gsi_gsi_debug_sw_msk_s {
	ecpri_gsi_hwio_def_gsi_gsi_debug_sw_msk_reg_n_sec_k_rd_u
		arr[GEN_ARR_SIZE_n(GSI_GSI_DEBUG_SW_MSK_REG_n_SEC_k_RD)]
		[GEN_ARR_SIZE_k(GSI_GSI_DEBUG_SW_MSK_REG_n_SEC_k_RD)];
};

struct ecpri_dma_reg_save_gsi_debug_s {
	ecpri_gsi_hwio_def_gsi_gsi_debug_busy_reg_u
		gsi_debug_busy_reg;
	ecpri_gsi_hwio_def_gsi_gsi_debug_pc_from_sw_u
		gsi_debug_pc_from_sw;
	ecpri_gsi_hwio_def_gsi_gsi_debug_sw_stall_u
		gsi_debug_sw_stall;
	ecpri_gsi_hwio_def_gsi_gsi_debug_pc_for_debug_u
		gsi_debug_pc_for_debug;
	ecpri_gsi_hwio_def_gsi_gsi_debug_qsb_log_err_trns_id_u
		gsi_debug_qsb_log_err_trns_id;
	struct ecpri_dma_reg_save_gsi_qsb_debug_s gsi_qsb_debug;
	struct ecpri_dma_reg_save_gsi_test_bus_s gsi_test_bus;
	ecpri_gsi_hwio_def_gsi_gsi_debug_sw_rf_n_read_u
		rf_read_regs[GEN_ARR_SIZE_n(GSI_GSI_DEBUG_SW_RF_n_READ)];
	struct ecpri_dma_reg_save_gsi_mcs_prof_regs_s gsi_mcs_prof_regs;
	struct ecpri_dma_reg_save_gsi_debug_cnt_s gsi_cnt_regs;
	struct ecpri_dma_reg_save_gsi_iram_ptr_regs_s gsi_iram_ptrs;
	struct ecpri_dma_reg_save_gsi_shram_ptr_regs_s gsi_shram_ptrs;
	struct ecpri_gsi_hwio_def_gsi_gsi_debug_sw_msk_s debug_sw_msk;
};

/* GSI main hirearchy struct */
struct gsi_regs_save_hierarchy_s {
	u32 fw_ver;
	struct ecpri_dma_reg_save_gsi_gen_s gen;
	struct ecpri_dma_reg_save_gsi_gen_ee_s gen_ee[ECPRI_DMA_REG_SAVE_GSI_NUM_EE];
	struct ecpri_dma_reg_save_gsi_ch_cntxt_per_ep_s
		ch_cntxt[ECPRI_DMA_MAX_EE][ECPRI_DMA_REG_SAVE_GSI_CH_MAX];
	struct ecpri_dma_reg_save_gsi_evt_cntxt_per_ep_s
		evt_cntxt[ECPRI_DMA_MAX_EE][ECPRI_DMA_REG_SAVE_GSI_EV_CH_MAX];
	struct ecpri_dma_reg_save_gsi_debug_s debug;
};

/* FIFO Status */
struct ecpri_dma_reg_save_gsi_fifo_status_s {
	ecpri_hwio_def_ecpri_gsi_tlv_fifo_status_n_u
		gsi_tlv_fifo_status_n[GEN_ARR_SIZE_n(ECPRI_GSI_TLV_FIFO_STATUS_n)];
	ecpri_hwio_def_ecpri_gsi_aos_fifo_status_n_u
		gsi_aos_fifo_status_n[GEN_ARR_SIZE_n(ECPRI_GSI_AOS_FIFO_STATUS_n)];
};

/* Main reg save struct */
struct ecpri_dma_regs_save_hierarchy_s {
	struct dma_regs_save_hierarchy_s            dma;
	struct gsi_regs_save_hierarchy_s            gsi;
	struct ecpri_dma_reg_save_gsi_fifo_status_s gsi_fifo_status[
		ECPRI_DMA_ENDP_NUM_MAX];
};

static struct ecpri_dma_regs_save_hierarchy_s ecpri_dma_reg_save;

void ecpri_dma_save_registers(void) {
	int n, k, i = 0;
	int ee = 0;
	int endp_id = 0;
	int phys_ch_idx = 0;

	/* Save DMA registers */
	/* DMA gen */
	READ_DMA_REG_ARR_N(ECPRI_STATE_GSI_TLV_FIFO_EMPTY_n,
		dma.gen, ecpri_state_gsi_tlv_fifo_empty_n);
	READ_DMA_REG_ARR_N(ECPRI_STATE_GSI_AOS_FIFO_EMPTY_n,
		dma.gen, ecpri_state_gsi_aos_fifo_empty_n);
	READ_DMA_REG(ECPRI_STATE_GSI_IF,
		dma.gen, ecpri_state_gsi_if);
	READ_DMA_REG(ECPRI_SPARE_REG,
		dma.gen, ecpri_spare_reg);
	READ_DMA_REG(ECPRI_HW_PARAMS_0,
		dma.gen, ecpri_hw_params_0);
	READ_DMA_REG(ECPRI_HW_PARAMS_2,
		dma.gen, ecpri_hw_params_2);
	READ_DMA_REG(ECPRI_GEN_CFG,
		dma.gen, ecpri_gen_cfg);
	READ_DMA_REG(ECPRI_CLKON_CFG,
		dma.gen, ecpri_clkon_cfg);
	READ_DMA_REG(ECPRI_QMB_CFG,
		dma.gen, ecpri_qmb_cfg);
	READ_DMA_REG(ECPRI_QMB_0_STATUS,
		dma.gen, ecpri_qmb_0_status);
	READ_DMA_REG(ECPRI_QMB_1_STATUS,
		dma.gen, ecpri_qmb_1_status);
	READ_DMA_REG(ECPRI_AOS_CFG,
		dma.gen, ecpri_aos_cfg);
	READ_DMA_REG(ECPRI_TIMERS_XO_CLK_DIV_CFG,
		dma.gen, ecpri_timers_xo_clk_div_cfg);
	READ_DMA_REG(ECPRI_TIMERS_PULSE_GRAN_CFG,
		dma.gen, ecpri_timers_pulse_gran_cfg);
	READ_DMA_REG(ECPRI_QTIME_LSB,
		dma.gen, ecpri_qtime_lsb);
	READ_DMA_REG(ECPRI_QTIME_MSB,
		dma.gen, ecpri_qtime_msb);
	READ_DMA_REG(ECPRI_SNOC_FEC,
		dma.gen, ecpri_snoc_fec);
	READ_DMA_REG(ECPRI_QMB_CFG_PARAM,
		dma.gen, ecpri_qmb_cfg_param);
	READ_DMA_REG(ECPRI_DMA_FL_PACKET_ID_CTRL,
		dma.gen, ecpri_dma_fl_packet_id_ctrl);
	READ_DMA_REG(ECPRI_DMA_FL_PACKET_ID_STATUS,
		dma.gen, ecpri_dma_fl_packet_id_status);
	READ_DMA_REG(ECPRI_DMA_FL_MEMORY_SECTOR_CTRL,
		dma.gen, ecpri_dma_fl_memory_sector_ctrl);
	READ_DMA_REG(ECPRI_DMA_FL_MEMORY_SECTOR_STATUS,
		dma.gen, ecpri_dma_fl_memory_sector_status);
	READ_DMA_REG(ECPRI_DMA_STREAM_CTRL,
		dma.gen, ecpri_dma_stream_ctrl);
	READ_DMA_REG(ECPRI_DMA_STREAM_STATUS,
		dma.gen, ecpri_dma_stream_status);
	READ_DMA_REG(ECPRI_DMA_STREAM_ARB,
		dma.gen, ecpri_dma_stream_arb);
	READ_DMA_REG(ECPRI_DMA_XBAR,
		dma.gen, ecpri_dma_xbar);
	READ_DMA_REG(ECPRI_DMA_DBG_CTRL,
		dma.gen, ecpri_dma_dbg_ctrl);
	READ_DMA_REG(ECPRI_DMA_GBL_CFG,
		dma.gen, ecpri_dma_gbl_cfg);

	/* DMA test bus */
	/*
	* ecpri_dma_testbus[] - collects [SEL, SEL_INTERNAL]=[0..255, 0] values
    * ecpri_dma_testbus_internal[] - collects [SEL, SEL_INTERNAL]=[0, 0..255]
	*/
	for (i = 0; i < ECPRI_DMA_REG_SAVE_TEST_BUS_MAX; i++) {
		ecpri_dma_reg_save.dma.gen.ecpri_dma_testbus_ctrl.def.sel = i;
		WRITE_DMA_REG(ECPRI_DMA_TESTBUS_CTRL,
			ecpri_dma_reg_save.dma.gen.ecpri_dma_testbus_ctrl.value);

		READ_DMA_REG(ECPRI_DMA_TESTBUS, dma.gen,
			ecpri_dma_testbus[i]);
	}
	ecpri_dma_reg_save.dma.gen.ecpri_dma_testbus_ctrl.value = 0;
	for (i = 0; i < ECPRI_DMA_REG_SAVE_TEST_BUS_MAX; i++) {
		ecpri_dma_reg_save.dma.gen.ecpri_dma_testbus_ctrl.def.sel_internal = i;
		WRITE_DMA_REG(ECPRI_DMA_TESTBUS_CTRL,
			ecpri_dma_reg_save.dma.gen.ecpri_dma_testbus_ctrl.value);

		READ_DMA_REG(ECPRI_DMA_TESTBUS, dma.gen,
			ecpri_dma_testbus_internal[i]);
	}

	READ_DMA_REG(ECPRI_DMA_GP_REG3,
		dma.gen, ecpri_dma_gp_reg3);
	READ_DMA_REG(ECPRI_DMA_AOS_FIFO_STAT,
		dma.gen, ecpri_dma_aos_fifo_stat);
	READ_DMA_REG(ECPRI_DMA_GP_STAT1,
		dma.gen, ecpri_dma_gp_stat1);
	READ_DMA_REG(ECPRI_DMA_GP_STAT3,
		dma.gen, ecpri_dma_gp_stat3);
	READ_DMA_REG(ECPRI_DMA_IDLE_REG,
		dma.gen, ecpri_dma_idle_reg);
	READ_DMA_REG(ECPRI_DMA_EXCEPTION_CHANNEL,
		dma.gen, ecpri_dma_exception_channel);
	READ_DMA_REG(ECPRI_DMA_PKT_DROP_FULL,
		dma.gen, ecpri_dma_pkt_drop_full);
	READ_DMA_REG(ECPRI_DMA_PKT_DROP_RE_EMPTY,
		dma.gen, ecpri_dma_pkt_drop_re_empty);
	READ_DMA_REG(ECPRI_DMA_TPDM_CFG,
		dma.gen, ecpri_dma_tpdm_cfg);

	/* DMA gen_ee */
	for (ee = 0; ee < ECPRI_DMA_MAX_EE; ee++) {
		READ_DMA_REG_N(ECPRI_IRQ_STTS_EE_n,
			dma.gen_ee[ee], irq_stts_ee_n);
		READ_DMA_REG_N(ECPRI_IRQ_EN_EE_n,
			dma.gen_ee[ee], irq_en_ee_n);
		READ_DMA_REG_N(ECPRI_GSI_EE_VFID_n,
			dma.gen_ee[ee], gsi_ee_vfid_n);
	}

	/* DMA dbg */
	READ_DMA_REG_ARR_N(ECPRI_IRQ_STTS_EE_n,
		dma.dbg, ecpri_irq_stts_ee_n);
	READ_DMA_REG_ARR_N(ECPRI_DST_ACKMNGR_CMDQ_STATUS_EMPTY_n,
		dma.dbg, ecpri_dst_ackmngr_cmdq_status_empty_n);
	READ_DMA_REG_ARR_N(ECPRI_DST_ACKMNGR_CMDQ_COUNT_n,
		dma.dbg, ecpri_dst_ackmngr_cmdq_count_n);
	READ_DMA_REG(ECPRI_SNOC_MONITORING_CFG,
		dma.dbg, ecpri_snoc_monitoring_cfg);
	READ_DMA_REG(ECPRI_QMB0_SNOC_MONITOR_CNT,
		dma.dbg, ecpri_qmb0_snoc_monitor_cnt);
	READ_DMA_REG(ECPRI_QMB1_SNOC_MONITOR_CNT,
		dma.dbg, ecpri_qmb1_snoc_monitor_cnt);
	READ_DMA_REG(ECPRI_GSI_SNOC_MONITOR_CNT,
		dma.dbg, ecpri_gsi_snoc_monitor_cnt);
	READ_DMA_REG(ECPRI_DST_ACKMNGR_CMDQ_STATUS,
		dma.dbg, ecpri_dst_ackmngr_cmdq_status);

	/* DMA endps */
	for (endp_id = 0; endp_id < ECPRI_DMA_ENDP_NUM_MAX; endp_id++) {
		READ_DMA_REG_ARR_N(ECPRI_ENDP_CFG_DESTn,
			dma.endps[endp_id], ecpri_endp_cfg_destn);
		READ_DMA_REG_ARR_N(ECPRI_ENDP_CFG_XBARn,
			dma.endps[endp_id], ecpri_endp_cfg_xbarn);
		READ_DMA_REG_ARR_N(ECPRI_ENDP_GSI_CFG_n,
			dma.endps[endp_id], ecpri_endp_gsi_cfg_n);
		READ_DMA_REG_ARR_N(ECPRI_ENDP_INIT_CTRL_STATUS_n,
			dma.endps[endp_id], ecpri_endp_init_ctrl_status_n);
		READ_DMA_REG_ARR_N(ECPRI_ENDP_AXI_ATTR_n,
			dma.endps[endp_id], ecpri_endp_axi_attr_n);
		READ_DMA_REG_ARR_N(ECPRI_ENDP_CFG_AGGR_n,
			dma.endps[endp_id], ecpri_endp_cfg_aggr_n);
		READ_DMA_REG_ARR_N(ECPRI_ENDP_YELLOW_RED_MARKER_CFG_n,
			dma.endps[endp_id], ecpri_endp_yellow_red_marker_cfg_n);
		READ_DMA_REG_ARR_N(ECPRI_ENDP_NFAPI_REASSEMBLY_CFG_n,
			dma.endps[endp_id], ecpri_endp_nfapi_reassembly_cfg_n);
		READ_DMA_REG_ARR_N(ECPRI_ENDP_GSI_CONS_BYTES_TLV_n,
			dma.endps[endp_id], ecpri_endp_gsi_cons_bytes_tlv_n);
		READ_DMA_REG_ARR_N(ECPRI_ENDP_GSI_CONS_BYTES_AOS_n,
			dma.endps[endp_id], ecpri_endp_gsi_cons_bytes_aos_n);
		READ_DMA_REG_ARR_N(ECPRI_ENDP_GSI_IF_FIFO_CFG_TLV_n,
			dma.endps[endp_id], ecpri_endp_gsi_if_fifo_cfg_tlv_n);
		READ_DMA_REG_ARR_N(ECPRI_ENDP_GSI_IF_FIFO_CFG_AOS_n,
			dma.endps[endp_id], ecpri_endp_gsi_if_fifo_cfg_aos_n);
		READ_DMA_REG_ARR_N(ECPRI_ENDP_CFG_VFID_n,
			dma.endps[endp_id], ecpri_endp_cfg_vfid_n);
	}

	/* DMA nfapi */
	/* nfapi_cfg */
	READ_DMA_REG(ECPRI_NFAPI_CFG_0,
		dma.nfapi.nfapi_cfg, ecpri_nfapi_cfg_0);
	READ_DMA_REG(ECPRI_NFAPI_CFG_1,
		dma.nfapi.nfapi_cfg, ecpri_nfapi_cfg_1);
	READ_DMA_REG(ECPRI_NFAPI_CFG_2,
		dma.nfapi.nfapi_cfg, ecpri_nfapi_cfg_2);

	/* nfapi_reassembly */
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_CFG_0,
		dma.nfapi.nfapi_reassembly, ecpri_nfapi_reassembly_cfg_0);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_FEC_0,
		dma.nfapi.nfapi_reassembly, ecpri_nfapi_reassembly_fec_0);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_FEC_1,
		dma.nfapi.nfapi_reassembly, ecpri_nfapi_reassembly_fec_1);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_FEC_2,
		dma.nfapi.nfapi_reassembly, ecpri_nfapi_reassembly_fec_2);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_FEC_3,
		dma.nfapi.nfapi_reassembly, ecpri_nfapi_reassembly_fec_3);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_FEC_4,
		dma.nfapi.nfapi_reassembly, ecpri_nfapi_reassembly_fec_4);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_FEC_5,
		dma.nfapi.nfapi_reassembly, ecpri_nfapi_reassembly_fec_5);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_ERROR_BAD_TOTAL_SDU_LENGTH_PKT_CNT,
		dma.nfapi.nfapi_reassembly,
		nfapi_reassembly_error_bad_total_sdu_length_pkt_cnt);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_ERROR_BAD_TOTAL_SDU_LENGTH_BYTE_CNT_LSB,
		dma.nfapi.nfapi_reassembly,
		ecpri_nfapi_reassembly_error_bad_total_sdu_length_byte_cnt_lsb);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_ERROR_BAD_TOTAL_SDU_LENGTH_BYTE_CNT_MSB,
		dma.nfapi.nfapi_reassembly,
		ecpri_nfapi_reassembly_error_bad_total_sdu_length_byte_cnt_msb);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_ERROR_BAD_TIMESTAMP_PKT_CNT,
		dma.nfapi.nfapi_reassembly,
		ecpri_nfapi_reassembly_error_bad_timestamp_pkt_cnt);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_ERROR_BAD_TIMESTAMP_BYTE_CNT_LSB,
		dma.nfapi.nfapi_reassembly,
		ecpri_nfapi_reassembly_error_bad_timestamp_byte_cnt_lsb);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_ERROR_BAD_TIMESTAMP_BYTE_CNT_MSB,
		dma.nfapi.nfapi_reassembly,
		ecpri_nfapi_reassembly_error_bad_timestamp_byte_cnt_msb);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_ERROR_PACKET_BIGGER_THAN_SDU_LENGTH_PKT_CNT,
		dma.nfapi.nfapi_reassembly,
		ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_pkt_cnt);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_ERROR_PACKET_BIGGER_THAN_SDU_LENGTH_BYTE_CNT_LSB,
		dma.nfapi.nfapi_reassembly,
		ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_byte_cnt_lsb);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_ERROR_PACKET_BIGGER_THAN_SDU_LENGTH_BYTE_CNT_MSB,
		dma.nfapi.nfapi_reassembly,
		ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_byte_cnt_msb);
	READ_DMA_REG(ECPRI_NFAPI_REASSEMBLY_STATE,
		dma.nfapi.nfapi_reassembly,
		ecpri_nfapi_reassembly_state);
	READ_DMA_REG_ARR_N(ECPRI_NFAPI_REASSEMBLY_VM_CFG_0_n,
		dma.nfapi.nfapi_reassembly, ecpri_nfapi_reassembly_vm_cfg_0_n);
	READ_DMA_REG_ARR_N(ECPRI_NFAPI_REASSEMBLY_VM_CFG_1_n,
		dma.nfapi.nfapi_reassembly, ecpri_nfapi_reassembly_vm_cfg_1_n);
	READ_DMA_REG_ARR_N(ECPRI_NFAPI_REASSEMBLY_VM_CFG_2_n,
		dma.nfapi.nfapi_reassembly, ecpri_nfapi_reassembly_vm_cfg_2_n);

	/* nfapi_nso_cfg */
	READ_DMA_REG(ECPRI_NSO_CFG,
		dma.nfapi.nfapi_nso_cfg, ecpri_nso_cfg);
	READ_DMA_REG(ECPRI_NSO_JUMBO_PKT_CFG,
		dma.nfapi.nfapi_nso_cfg, ecpri_nso_jumbo_pkt_cfg);
	READ_DMA_REG(ECPRI_NSO_SHORT_PKT_CFG,
		dma.nfapi.nfapi_nso_cfg, ecpri_nso_short_pkt_cfg);
	READ_DMA_REG(ECPRI_NSO_LEN_ERR_STATUS_1,
		dma.nfapi.nfapi_nso_cfg, ecpri_nso_len_err_status_1);
	READ_DMA_REG(ECPRI_NSO_LEN_ERR_STATUS_2,
		dma.nfapi.nfapi_nso_cfg, ecpri_nso_len_err_status_2);
	READ_DMA_REG(ECPRI_NSO_LEN_ERR_STATUS_3,
		dma.nfapi.nfapi_nso_cfg, ecpri_nso_len_err_status_3);
	READ_DMA_REG(ECPRI_NSO_DBG_CNTXT_n_INFO_1,
		dma.nfapi.nfapi_nso_cfg, ecpri_nso_dbg_cntxt_n_info_1);
	READ_DMA_REG(ECPRI_NSO_DBG_CNTXT_n_INFO_2,
		dma.nfapi.nfapi_nso_cfg, ecpri_nso_dbg_cntxt_n_info_2);
	READ_DMA_REG(ECPRI_NSO_DBG_MISC_INFO,
		dma.nfapi.nfapi_nso_cfg, ecpri_nso_dbg_misc_info);
	READ_DMA_REG_ARR_N(ECPRI_NSO_JUMBO_PKT_EN_n,
		dma.nfapi.nfapi_nso_cfg, ecpri_nso_jumbo_pkt_en_n);
	READ_DMA_REG_ARR_N(ECPRI_NSO_LEN_ERR_STATUS_HDR_n,
		dma.nfapi.nfapi_nso_cfg, ecpri_nso_len_err_status_hdr_n);

	/* DMA dpl */
	READ_DMA_REG(ECPRI_DMA_DPL_CFG,
		dma.dpl, ecpri_dma_dpl_cfg);
	READ_DMA_REG_ARR_N(ECPRI_DPL_TRIG_CTRL_n,
		dma.dpl, ecpri_dpl_trig_ctrl_n);
	READ_DMA_REG_ARR_N(ECPRI_DPL_TRIG_A_n,
		dma.dpl, ecpri_dpl_trig_a_n);
	READ_DMA_REG_ARR_N(ECPRI_DPL_TRIG_B_n,
		dma.dpl, ecpri_dpl_trig_b_n);

	/* DMA markers */
	READ_DMA_REG_ARR_N(ECPRI_YELLOW_MARKER_BELOW_n,
		dma.markers, ecpri_yellow_marker_below_n);
	READ_DMA_REG_ARR_N(ECPRI_YELLOW_MARKER_BELOW_EN_n,
		dma.markers, ecpri_yellow_marker_below_en_n);
	READ_DMA_REG_ARR_N(ECPRI_RED_MARKER_BELOW_n,
		dma.markers, ecpri_red_marker_below_n);
	READ_DMA_REG_ARR_N(ECPRI_RED_MARKER_BELOW_EN_n,
		dma.markers, ecpri_red_marker_below_en_n);
	READ_DMA_REG_ARR_N(ECPRI_YELLOW_MARKER_SHADOW_n,
		dma.markers, ecpri_yellow_marker_shadow_n);
	READ_DMA_REG_ARR_N(ECPRI_RED_MARKER_SHADOW_n,
		dma.markers, ecpri_red_marker_shadow_n);
	READ_DMA_REG_ARR_N(ECPRI_YELLOW_MARKER_ABOVE_n,
		dma.markers, ecpri_yellow_marker_above_n);
	READ_DMA_REG_ARR_N(ECPRI_YELLOW_MARKER_ABOVE_EN_n,
		dma.markers, ecpri_yellow_marker_above_en_n);
	READ_DMA_REG_ARR_N(ECPRI_RED_MARKER_ABOVE_n,
		dma.markers, ecpri_red_marker_above_n);
	READ_DMA_REG_ARR_N(ECPRI_RED_MARKER_ABOVE_EN_n,
		dma.markers, ecpri_red_marker_above_en_n);


	/* Save GSI registers */
	ecpri_dma_reg_save.gsi.fw_ver =
		gsihal_read_reg_n(GSI_GSI_INST_RAM_n,
			ECPRI_DMA_REG_SAVE_FW_VER_ROW);

	/* gen */
	READ_GSI_REG(GSI_GSI_CFG, gsi.gen, gsi_cfg);
	READ_GSI_REG(GSI_GSI_REE_CFG, gsi.gen, gsi_ree_cfg);
	READ_GSI_REG_N(GSI_GSI_INST_RAM_n, gsi.gen, gsi_inst_ram_n);

	/* gen_ee */
	for (ee = 0; ee < ECPRI_DMA_MAX_EE; ee++) {
		READ_GSI_REG_N(GSI_GSI_MANAGER_EE_QOS_n,
			gsi.gen_ee[ee], gsi_manager_ee_qos_n);
		READ_GSI_REG_N(GSI_EE_n_GSI_STATUS,
			gsi.gen_ee[ee], ee_n_gsi_status);
		READ_GSI_REG_N(GSI_EE_n_GSI_HW_PARAM_0,
			gsi.gen_ee[ee], ee_n_gsi_hw_param_0);
		READ_GSI_REG_N(GSI_EE_n_GSI_HW_PARAM_1,
			gsi.gen_ee[ee], ee_n_gsi_hw_param_1);
		READ_GSI_REG_N(GSI_EE_n_GSI_HW_PARAM_2,
			gsi.gen_ee[ee], ee_n_gsi_hw_param_2);
		READ_GSI_REG_N(GSI_EE_n_GSI_HW_PARAM_3,
			gsi.gen_ee[ee], ee_n_gsi_hw_param_3);
		READ_GSI_REG_N(GSI_EE_n_GSI_HW_PARAM_4,
			gsi.gen_ee[ee], ee_n_gsi_hw_param_4);
		READ_GSI_REG_N(GSI_EE_n_CNTXT_TYPE_IRQ,
			gsi.gen_ee[ee], ee_n_cntxt_type_irq);
		READ_GSI_REG_N(GSI_EE_n_CNTXT_TYPE_IRQ_MSK,
			gsi.gen_ee[ee], ee_n_cntxt_type_irq_msk);
		READ_GSI_REG_N(GSI_EE_n_CNTXT_GSI_IRQ_STTS,
			gsi.gen_ee[ee], ee_n_cntxt_gsi_irq_stts);
		READ_GSI_REG_N(GSI_EE_n_CNTXT_GLOB_IRQ_STTS,
			gsi.gen_ee[ee], ee_n_cntxt_glob_irq_stts);
		READ_GSI_REG_N(GSI_EE_n_ERROR_LOG,
			gsi.gen_ee[ee], ee_n_error_log);
		READ_GSI_REG_N(GSI_EE_n_CNTXT_SCRATCH_0,
			gsi.gen_ee[ee], ee_n_cntxt_scratch_0);
		READ_GSI_REG_N(GSI_EE_n_CNTXT_SCRATCH_1,
			gsi.gen_ee[ee], ee_n_cntxt_scratch_1);
		READ_GSI_REG_N(GSI_EE_n_CNTXT_INTSET,
			gsi.gen_ee[ee], ee_n_cntxt_intset);
		READ_GSI_REG_N(GSI_EE_n_CNTXT_MSI_BASE_LSB,
			gsi.gen_ee[ee], ee_n_cntxt_msi_base_lsb);
		READ_GSI_REG_N(GSI_EE_n_CNTXT_MSI_BASE_MSB,
			gsi.gen_ee[ee], ee_n_cntxt_msi_base_msb);
		READ_GSI_REG_N(GSI_EE_n_CNTXT_GLOB_IRQ_EN,
			gsi.gen_ee[ee], ee_n_glob_irq_en);
		READ_GSI_REG_N(GSI_EE_n_CNTXT_GSI_IRQ_EN,
			gsi.gen_ee[ee], ee_n_gsi_irq_en);
		READ_GSI_REG_N(GSI_EE_n_CNTXT_INT_VEC,
			gsi.gen_ee[ee], ee_n_int_vec);
		READ_GSI_REG_ARR_N_K(GSI_EE_n_CNTXT_SRC_GSI_CH_IRQ_k, gsi.gen_ee[ee],
			ee_n_cntxt_src_gsi_ch_irq_k);
		READ_GSI_REG_ARR_N_K(GSI_EE_n_CNTXT_SRC_EV_CH_IRQ_k, gsi.gen_ee[ee],
			ee_n_cntxt_src_ev_ch_irq_k);
		READ_GSI_REG_ARR_N_K(GSI_EE_n_CNTXT_SRC_GSI_CH_IRQ_MSK_k, gsi.gen_ee[ee],
			ee_n_cntxt_src_gsi_ch_irq_msk_k);
		READ_GSI_REG_ARR_N_K(GSI_EE_n_CNTXT_SRC_EV_CH_IRQ_MSK_k, gsi.gen_ee[ee],
			ee_n_cntxt_src_ev_ch_irq_msk_k);
		READ_GSI_REG_ARR_N_K(GSI_EE_n_CNTXT_SRC_IEOB_IRQ_k, gsi.gen_ee[ee],
			ee_n_cntxt_src_ieob_irq_k);
		READ_GSI_REG_ARR_N_K(GSI_EE_n_CNTXT_SRC_IEOB_IRQ_MSK_k, gsi.gen_ee[ee],
			ee_n_cntxt_src_ieob_irq_msk_k);
	}

	/* ch_cntxt */
	for (ee = 0; ee < ECPRI_DMA_MAX_EE; ee++) {
		for (k = 0; k < ECPRI_DMA_REG_SAVE_GSI_CH_MAX; k++) {
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_CNTXT_0, gsi.ch_cntxt[ee][k],
				ee_n_gsi_ch_k_cntxt_0, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_CNTXT_1, gsi.ch_cntxt[ee][k],
				ee_n_gsi_ch_k_cntxt_1, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_CNTXT_2, gsi.ch_cntxt[ee][k],
				ee_n_gsi_ch_k_cntxt_2, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_CNTXT_3, gsi.ch_cntxt[ee][k],
				ee_n_gsi_ch_k_cntxt_3, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_CNTXT_4, gsi.ch_cntxt[ee][k],
				ee_n_gsi_ch_k_cntxt_4, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_CNTXT_5, gsi.ch_cntxt[ee][k],
				ee_n_gsi_ch_k_cntxt_5, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_CNTXT_6, gsi.ch_cntxt[ee][k],
				ee_n_gsi_ch_k_cntxt_6, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_CNTXT_7, gsi.ch_cntxt[ee][k],
				ee_n_gsi_ch_k_cntxt_7, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_CNTXT_8, gsi.ch_cntxt[ee][k],
				ee_n_gsi_ch_k_cntxt_8, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_RE_FETCH_READ_PTR,
				gsi.ch_cntxt[ee][k], ee_n_gsi_ch_k_re_fetch_read_ptr, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_RE_FETCH_WRITE_PTR,
				gsi.ch_cntxt[ee][k], ee_n_gsi_ch_k_re_fetch_read_ptr, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_QOS,
				gsi.ch_cntxt[ee][k], ee_n_gsi_ch_k_qos, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_SCRATCH_0,
				gsi.ch_cntxt[ee][k], ee_n_gsi_ch_k_scratch_0, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_SCRATCH_1,
				gsi.ch_cntxt[ee][k], ee_n_gsi_ch_k_scratch_1, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_SCRATCH_2,
				gsi.ch_cntxt[ee][k], ee_n_gsi_ch_k_scratch_2, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_SCRATCH_3,
				gsi.ch_cntxt[ee][k], ee_n_gsi_ch_k_scratch_3, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_SCRATCH_4,
				gsi.ch_cntxt[ee][k], ee_n_gsi_ch_k_scratch_4, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_SCRATCH_5,
				gsi.ch_cntxt[ee][k], ee_n_gsi_ch_k_scratch_5, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_SCRATCH_6,
				gsi.ch_cntxt[ee][k], ee_n_gsi_ch_k_scratch_6, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_SCRATCH_7,
				gsi.ch_cntxt[ee][k], ee_n_gsi_ch_k_scratch_7, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_SCRATCH_8,
				gsi.ch_cntxt[ee][k], ee_n_gsi_ch_k_scratch_8, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_SCRATCH_9,
				gsi.ch_cntxt[ee][k], ee_n_gsi_ch_k_scratch_9, ee, k);
			READ_GSI_REG_N_K(GSI_GSI_MAP_EE_n_CH_k_VP_TABLE,
				gsi.ch_cntxt[ee][k], gsi_map_ee_n_ch_k_vp_table, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_DB_ENG_WRITE_PTR,
				gsi.ch_cntxt[ee][k], gsi_ch_k_db_eng_write_ptr, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_GSI_CH_k_CH_ALMST_EMPTY_THRSHOLD,
				gsi.ch_cntxt[ee][k], gsi_ch_k_ch_almst_empty_thrshold, ee, k);

			/* mcs_channel_scratch */
			n = phys_ch_idx * ECPRI_DMA_REG_SAVE_BYTES_PER_CHNL_SHRAM;

			READ_GSI_SHRAM_REG(GSI_GSI_SHRAM_n,
				gsi.ch_cntxt[ee][k].mcs_channel_scratch,
				scratch_for_seq_low,
				n + ECPRI_GSI_OFFSET_WORDS_SCRATCH_FOR_SEQ_LOW);
			READ_GSI_SHRAM_REG(GSI_GSI_SHRAM_n,
				gsi.ch_cntxt[ee][k].mcs_channel_scratch,
				scratch_for_seq_high,
				n + ECPRI_GSI_OFFSET_WORDS_SCRATCH_FOR_SEQ_HIGH);
		}
	}

	/* evt_cntxt */
	for (ee = 0; ee < ECPRI_DMA_MAX_EE; ee++) {
		for (k = 0; k < ECPRI_DMA_REG_SAVE_GSI_EV_CH_MAX; k++) {
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_CNTXT_0, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_cntxt_0, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_CNTXT_1, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_cntxt_1, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_CNTXT_2, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_cntxt_2, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_CNTXT_3, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_cntxt_3, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_CNTXT_4, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_cntxt_4, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_CNTXT_5, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_cntxt_5, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_CNTXT_6, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_cntxt_6, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_CNTXT_7, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_cntxt_7, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_CNTXT_8, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_cntxt_8, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_CNTXT_9, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_cntxt_9, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_CNTXT_10, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_cntxt_10, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_CNTXT_11, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_cntxt_11, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_CNTXT_12, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_cntxt_12, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_CNTXT_13, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_cntxt_13, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_SCRATCH_0, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_scratch_0, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_SCRATCH_1, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_scratch_1, ee, k);
			READ_GSI_REG_N_K(GSI_EE_n_EV_CH_k_SCRATCH_2, gsi.evt_cntxt[ee][k],
				ee_n_ev_ch_k_scratch_2, ee, k);
			READ_GSI_REG_N_K(GSI_GSI_DEBUG_EE_n_EV_k_VP_TABLE,
				gsi.evt_cntxt[ee][k], gsi_debug_ee_n_ev_k_vp_table, ee, k);
		}
	}

	/* debug */
	READ_GSI_REG(GSI_GSI_DEBUG_BUSY_REG,
		gsi.debug, gsi_debug_busy_reg);
	READ_GSI_REG(GSI_GSI_DEBUG_PC_FROM_SW,
		gsi.debug, gsi_debug_pc_from_sw);
	READ_GSI_REG(GSI_GSI_DEBUG_SW_STALL,
		gsi.debug, gsi_debug_sw_stall);
	READ_GSI_REG(GSI_GSI_DEBUG_PC_FOR_DEBUG,
		gsi.debug, gsi_debug_pc_for_debug);
	READ_GSI_REG(GSI_GSI_DEBUG_QSB_LOG_ERR_TRNS_ID,
		gsi.debug, gsi_debug_qsb_log_err_trns_id);
	READ_GSI_REG_ARR_N(GSI_GSI_DEBUG_COUNTERn,
		gsi.debug.gsi_cnt_regs, cnt);

	/* gsi_qsb_debug */
	READ_GSI_REG(GSI_GSI_DEBUG_QSB_LOG_ERR_TRNS_ID,
		gsi.debug.gsi_qsb_debug, gsi_debug_busy_reg);
	READ_GSI_REG_N(GSI_GSI_DEBUG_QSB_LOG_LAST_MISC_IDn,
		gsi.debug.gsi_qsb_debug, qsb_log_last_misc_idn);
	READ_GSI_REG_N(GSI_GSI_DEBUG_COUNTER_CFGn,
		gsi.debug.gsi_qsb_debug, gsi_debug_counter_cfgn);
	READ_GSI_REG(GSI_GSI_DEBUG_REE_PREFETCH_BUF_CH_ID,
		gsi.debug.gsi_qsb_debug, gsi_debug_ree_prefetch_buf_ch_id);
	READ_GSI_REG(GSI_GSI_DEBUG_REE_PREFETCH_BUF_STATUS,
		gsi.debug.gsi_qsb_debug, gsi_debug_ree_prefetch_buf_status);
	READ_GSI_REG_K(GSI_GSI_DEBUG_EVENT_PENDING_k,
		gsi.debug.gsi_qsb_debug, gsi_debug_event_pending_k);
	READ_GSI_REG_K(GSI_GSI_DEBUG_TIMER_PENDING_k,
		gsi.debug.gsi_qsb_debug, gsi_debug_timer_pending_k);
	READ_GSI_REG_K(GSI_GSI_DEBUG_RD_WR_PENDING_k,
		gsi.debug.gsi_qsb_debug, gsi_debug_rd_wr_pending_k);

	/* GSI test bus */
	for (i = 0;
		i < ARRAY_SIZE(ecpri_dma_reg_save_gsi_test_bus_selector_array);
		i++) {
		/* Write test bus selector */
		WRITE_GSI_REG(GSI_GSI_TEST_BUS_SEL,
			ecpri_dma_reg_save_gsi_test_bus_selector_array[i]);

		READ_GSI_REG(GSI_GSI_TEST_BUS_REG, gsi.debug.gsi_test_bus,
			test_bus_reg[i]);
	}

	READ_GSI_REG_ARR_N(GSI_GSI_DEBUG_SW_RF_n_READ, gsi.debug, rf_read_regs);

	/* QSB regs */
	READ_GSI_REG_ARR_N(GSI_GSI_DEBUG_QSB_LOG_LAST_MISC_IDn,
		gsi.debug.gsi_qsb_debug.gsi_debug_qsb_reg,
		gsi_debug_qsb_log_last_misc_idn);

	/* gsi_mcs_prof_regs */
	READ_GSI_REG(GSI_GSI_MCS_PROFILING_BP_CNT_LSB,
		gsi.debug.gsi_mcs_prof_regs, gsi_mcs_profiling_bp_cnt_lsb);
	READ_GSI_REG(GSI_GSI_MCS_PROFILING_BP_CNT_MSB,
		gsi.debug.gsi_mcs_prof_regs, gsi_mcs_profiling_bp_cnt_msb);
	READ_GSI_REG(GSI_GSI_MCS_PROFILING_BP_AND_PENDING_CNT_LSB,
		gsi.debug.gsi_mcs_prof_regs, gsi_mcs_profiling_bp_and_pending_cnt_lsb);
	READ_GSI_REG(GSI_GSI_MCS_PROFILING_BP_AND_PENDING_CNT_MSB,
		gsi.debug.gsi_mcs_prof_regs, gsi_mcs_profiling_bp_and_pending_cnt_msb);
	READ_GSI_REG(GSI_GSI_MCS_PROFILING_MCS_BUSY_CNT_LSB,
		gsi.debug.gsi_mcs_prof_regs, gsi_mcs_profiling_mcs_busy_cnt_lsb);
	READ_GSI_REG(GSI_GSI_MCS_PROFILING_MCS_BUSY_CNT_MSB,
		gsi.debug.gsi_mcs_prof_regs, gsi_mcs_profiling_mcs_busy_cnt_msb);
	READ_GSI_REG(GSI_GSI_MCS_PROFILING_MCS_IDLE_CNT_LSB,
		gsi.debug.gsi_mcs_prof_regs, gsi_mcs_profiling_mcs_idle_cnt_lsb);
	READ_GSI_REG(GSI_GSI_MCS_PROFILING_MCS_IDLE_CNT_MSB,
		gsi.debug.gsi_mcs_prof_regs, gsi_mcs_profiling_mcs_idle_cnt_msb);

	/* gsi_iram_ptrs */
	READ_GSI_REG(GSI_GSI_IRAM_PTR_CH_CMD,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_ch_cmd);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_EE_GENERIC_CMD,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_ee_generic_cmd);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_CH_DB,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_ch_db);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_EV_DB,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_ev_db);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_NEW_RE,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_new_re);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_CH_DIS_COMP,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_ch_dis_comp);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_CH_EMPTY,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_ch_empty);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_TIMER_EXPIRED,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_timer_expired);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_EVENT_GEN_COMP,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_event_gen_comp);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_WRITE_ENG_COMP,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_write_eng_comp);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_READ_ENG_COMP,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_read_eng_comp);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_UC_GP_INT,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_uc_gp_int);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_INT_MOD_STOPED,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_int_mod_stoped);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_TLV_CH_NOT_FULL,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_tlv_ch_not_full);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_MSI_DB,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_msi_db);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_PERIPH_IF_TLV_IN_0,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_periph_if_tlv_in_0);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_PERIPH_IF_TLV_IN_1,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_periph_if_tlv_in_1);
	READ_GSI_REG(GSI_GSI_IRAM_PTR_PERIPH_IF_TLV_IN_2,
		gsi.debug.gsi_iram_ptrs, gsi_iram_ptr_periph_if_tlv_in_2);

	/* gsi_shram_ptrs */
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_CH_CNTXT_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_ch_cntxt_base_addr);
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_EV_CNTXT_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_ev_cntxt_base_addr);
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_RE_STORAGE_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_re_storage_base_addr);
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_RE_ESC_BUF_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_re_esc_buf_base_addr);
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_EE_SCRACH_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_ee_scrach_base_addr);
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_FUNC_STACK_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_func_stack_base_addr);
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_MCS_SCRATCH_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_mcs_scratch_base_addr);
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_MCS_SCRATCH1_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_mcs_scratch1_base_addr);
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_MCS_SCRATCH2_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_mcs_scratch2_base_addr);
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_MCS_SCRATCH3_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_mcs_scratch3_base_addr);
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_CH_VP_TRANS_TABLE_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_ch_vp_trans_table_base_addr);
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_EV_VP_TRANS_TABLE_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_ev_vp_trans_table_base_addr);
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_USER_INFO_DATA_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_user_info_data_base_addr);
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_EE_CMD_FIFO_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_ee_cmd_fifo_base_addr);
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_CH_CMD_FIFO_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_ch_cmd_fifo_base_addr);
	READ_GSI_REG(GSI_GSI_SHRAM_PTR_EVE_ED_STORAGE_BASE_ADDR,
		gsi.debug.gsi_shram_ptrs, gsi_shram_ptr_eve_ed_storage_base_addr);

	/* debug_sw_msk */
	READ_GSI_REG_ARR_N_K(GSI_GSI_DEBUG_SW_MSK_REG_n_SEC_k_RD, gsi.debug,
		debug_sw_msk);

	/* Save FIFO Status registers */
	for (endp_id = 0; endp_id < ECPRI_DMA_ENDP_NUM_MAX; endp_id++) {
		READ_DMA_REG_ARR_N(ECPRI_GSI_TLV_FIFO_STATUS_n,
			gsi_fifo_status[endp_id], gsi_tlv_fifo_status_n);
		READ_DMA_REG_ARR_N(ECPRI_GSI_AOS_FIFO_STATUS_n,
			gsi_fifo_status[endp_id], gsi_aos_fifo_status_n);
	}

	return;
}