/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __ECPRI_HWIO_DEF_H__
#define __ECPRI_HWIO_DEF_H__
/*
===========================================================================
*/
/**
@file ecpri_hwio.h
@brief Auto-generated HWIO interface include file.

This file contains HWIO register definitions for the following modules:
ECPRI_DMA.*

'Include' filters applied: <none>
'Exclude' filters applied: RESERVED DUMMY
*/

/*----------------------------------------------------------------------------
* MODULE: DMA
*--------------------------------------------------------------------------*/

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_HW_PARAMS_0
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 total_channels_n : 8;
	u32 src_channel_n : 8;
	u32 dst_channel_n : 8;
	u32 reserved0 : 8;
} ecpri_hwio_def_ecpri_hw_params_0_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_hw_params_0_s def;
	u32 value;
} ecpri_hwio_def_ecpri_hw_params_0_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_HW_PARAMS_2
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 qmb_max_outst_wr : 7;
	u32 reserved0 : 1;
	u32 qmb_max_outst_rd : 7;
	u32 reserved1 : 1;
	u32 gsi_ees_n : 4;
	u32 nfapi_reassembly_ctx_num : 8;
	u32 reserved2 : 4;
} ecpri_hwio_def_ecpri_hw_params_2_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_hw_params_2_s def;
	u32 value;
} ecpri_hwio_def_ecpri_hw_params_2_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_GEN_CFG
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 out_of_buf_stop_reset_mask_enable : 1;
	u32 gen_qmb_dynamic_asize : 1;
	u32 ack_mngr_ack_on_0_len : 1;
	u32 reserved0 : 29;
} ecpri_hwio_def_ecpri_gen_cfg_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_gen_cfg_s def;
	u32 value;
} ecpri_hwio_def_ecpri_gen_cfg_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_CLKON_CFG
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 cgc_open_gsi_if : 1;
	u32 cgc_open_ack_manager : 1;
	u32 cgc_open_nro : 1;
	u32 reserved0 : 29;
} ecpri_hwio_def_ecpri_clkon_cfg_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_clkon_cfg_s def;
	u32 value;
} ecpri_hwio_def_ecpri_clkon_cfg_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_QMB_CFG
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 write_ot_limit : 8;
	u32 read_ot_limit : 8;
	u32 read_beats_limit : 12;
	u32 reserved0 : 4;
} ecpri_hwio_def_ecpri_qmb_cfg_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_qmb_cfg_s def;
	u32 value;
} ecpri_hwio_def_ecpri_qmb_cfg_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_QMB_0_STATUS
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 write_outs : 8;
	u32 read_outs : 8;
	u32 read_beats : 12;
	u32 busy : 1;
	u32 reserved0 : 3;
} ecpri_hwio_def_ecpri_qmb_0_status_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_qmb_0_status_s def;
	u32 value;
} ecpri_hwio_def_ecpri_qmb_0_status_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_QMB_1_STATUS
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 write_outs : 8;
	u32 read_outs : 8;
	u32 read_beats : 12;
	u32 busy : 1;
	u32 reserved0 : 3;
} ecpri_hwio_def_ecpri_qmb_1_status_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_qmb_1_status_s def;
	u32 value;
} ecpri_hwio_def_ecpri_qmb_1_status_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_AOS_CFG
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 tx_rx_priority : 1;
	u32 reserved0 : 31;
} ecpri_hwio_def_ecpri_aos_cfg_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_aos_cfg_s def;
	u32 value;
} ecpri_hwio_def_ecpri_aos_cfg_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_TIMERS_XO_CLK_DIV_CFG
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 value : 9;
	u32 reserved0 : 22;
	u32 enable : 1;
} ecpri_hwio_def_ecpri_timers_xo_clk_div_cfg_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_timers_xo_clk_div_cfg_s def;
	u32 value;
} ecpri_hwio_def_ecpri_timers_xo_clk_div_cfg_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_TIMERS_PULSE_GRAN_CFG
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 gran_0 : 3;
	u32 gran_1 : 3;
	u32 gran_2 : 3;
	u32 gran_3 : 3;
	u32 reserved0 : 20;
} ecpri_hwio_def_ecpri_timers_pulse_gran_cfg_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_timers_pulse_gran_cfg_s def;
	u32 value;
} ecpri_hwio_def_ecpri_timers_pulse_gran_cfg_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_QTIME_SMP
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 pulse : 1;
	u32 reserved0 : 31;
} ecpri_hwio_def_ecpri_qtime_smp_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_qtime_smp_s def;
	u32 value;
} ecpri_hwio_def_ecpri_qtime_smp_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_QTIME_LSB
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 value : 32;
} ecpri_hwio_def_ecpri_qtime_lsb_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_qtime_lsb_s def;
	u32 value;
} ecpri_hwio_def_ecpri_qtime_lsb_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_QTIME_MSB
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 value : 32;
} ecpri_hwio_def_ecpri_qtime_msb_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_qtime_msb_s def;
	u32 value;
} ecpri_hwio_def_ecpri_qtime_msb_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_SNOC_FEC
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 axi_id : 8;
	u32 noc_port : 2;
	u32 direction : 1;
	u32 reserved0 : 3;
	u32 clear : 1;
	u32 valid : 1;
	u32 reserved1 : 16;
} ecpri_hwio_def_ecpri_snoc_fec_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_snoc_fec_s def;
	u32 value;
} ecpri_hwio_def_ecpri_snoc_fec_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_QMB_CFG_PARAM
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 req_ashared : 1;
	u32 req_aooowr : 1;
	u32 req_aooord : 1;
	u32 req_anoallocate : 1;
	u32 req_ainnershared : 1;
	u32 req_amemtype : 3;
	u32 reserved0 : 24;
} ecpri_hwio_def_ecpri_qmb_cfg_param_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_qmb_cfg_param_s def;
	u32 value;
} ecpri_hwio_def_ecpri_qmb_cfg_param_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_CFG_0
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 header_length : 8;
	u32 reserved0 : 24;
} ecpri_hwio_def_ecpri_nfapi_cfg_0_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_cfg_0_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_cfg_0_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_CFG_1
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 byte_offset_offset : 8;
	u32 total_sdu_length_offset : 8;
	u32 sequence_number_offset : 8;
	u32 time_stamp_offset : 8;
} ecpri_hwio_def_ecpri_nfapi_cfg_1_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_cfg_1_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_cfg_1_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_CFG_2
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 byte_offset_size : 8;
	u32 total_sdu_length_size : 8;
	u32 sequence_number_size : 8;
	u32 time_stamp_size : 8;
} ecpri_hwio_def_ecpri_nfapi_cfg_2_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_cfg_2_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_cfg_2_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_CFG_0
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 stall_mode : 1;
	u32 bad_timestamp_en : 1;
	u32 bad_total_sdu_length_pkt_en : 1;
	u32 reserved0 : 29;
} ecpri_hwio_def_ecpri_nfapi_reassembly_cfg_0_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_cfg_0_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_cfg_0_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_FEC_0
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 message_bigger_than_sdu_length : 1;
	u32 packet_bigger_than_sdu_length : 1;
	u32 bad_timestamp : 1;
	u32 bad_total_sdu_length : 1;
	u32 timeout : 1;
	u32 not_enough_contexts : 1;
	u32 not_enough_bytes : 1;
	u32 message_too_big : 1;
	u32 reserved0 : 24;
} ecpri_hwio_def_ecpri_nfapi_reassembly_fec_0_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_fec_0_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_fec_0_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_FEC_1
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 offset : 32;
} ecpri_hwio_def_ecpri_nfapi_reassembly_fec_1_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_fec_1_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_fec_1_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_FEC_2
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 sdu_length : 32;
} ecpri_hwio_def_ecpri_nfapi_reassembly_fec_2_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_fec_2_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_fec_2_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_FEC_3
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 timestamp : 32;
} ecpri_hwio_def_ecpri_nfapi_reassembly_fec_3_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_fec_3_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_fec_3_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_FEC_4
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 channel_id : 8;
	u32 reserved0 : 8;
	u32 context_id : 8;
	u32 reserved1 : 8;
} ecpri_hwio_def_ecpri_nfapi_reassembly_fec_4_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_fec_4_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_fec_4_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_FEC_5
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 sequence_number : 32;
} ecpri_hwio_def_ecpri_nfapi_reassembly_fec_5_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_fec_5_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_fec_5_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_ERROR_BAD_TOTAL_SDU_LENGTH_PKT_CNT
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 packet_count : 24;
	u32 reserved0 : 8;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_total_sdu_length_pkt_cnt_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_total_sdu_length_pkt_cnt_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_total_sdu_length_pkt_cnt_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_ERROR_BAD_TOTAL_SDU_LENGTH_BYTE_CNT_LSB
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 byte_count : 32;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_total_sdu_length_byte_cnt_lsb_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_total_sdu_length_byte_cnt_lsb_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_total_sdu_length_byte_cnt_lsb_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_ERROR_BAD_TOTAL_SDU_LENGTH_BYTE_CNT_MSB
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 byte_count : 8;
	u32 reserved0 : 24;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_total_sdu_length_byte_cnt_msb_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_total_sdu_length_byte_cnt_msb_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_total_sdu_length_byte_cnt_msb_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_ERROR_BAD_TIMESTAMP_PKT_CNT
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 packet_count : 24;
	u32 reserved0 : 8;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_timestamp_pkt_cnt_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_timestamp_pkt_cnt_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_timestamp_pkt_cnt_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_ERROR_BAD_TIMESTAMP_BYTE_CNT_LSB
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 byte_count : 32;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_timestamp_byte_cnt_lsb_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_timestamp_byte_cnt_lsb_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_timestamp_byte_cnt_lsb_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_ERROR_BAD_TIMESTAMP_BYTE_CNT_MSB
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 byte_count : 8;
	u32 reserved0 : 24;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_timestamp_byte_cnt_msb_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_timestamp_byte_cnt_msb_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_bad_timestamp_byte_cnt_msb_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_ERROR_PACKET_BIGGER_THAN_SDU_LENGTH_PKT_CNT
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 packet_count : 24;
	u32 reserved0 : 8;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_pkt_cnt_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_pkt_cnt_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_pkt_cnt_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_ERROR_PACKET_BIGGER_THAN_SDU_LENGTH_BYTE_CNT_LSB
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 byte_count : 32;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_byte_cnt_lsb_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_byte_cnt_lsb_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_byte_cnt_lsb_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_ERROR_PACKET_BIGGER_THAN_SDU_LENGTH_BYTE_CNT_MSB
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 byte_count : 8;
	u32 reserved0 : 24;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_byte_cnt_msb_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_byte_cnt_msb_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_packet_bigger_than_sdu_length_byte_cnt_msb_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_ERROR_MESSAGE_BYTES_BIGGER_THAN_SDU_LENGTH_PKT_CNT
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 packet_count : 24;
	u32 reserved0 : 8;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_message_bytes_bigger_than_sdu_length_pkt_cnt_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_message_bytes_bigger_than_sdu_length_pkt_cnt_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_message_bytes_bigger_than_sdu_length_pkt_cnt_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_ERROR_MESSAGE_BYTES_BIGGER_THAN_SDU_LENGTH_BYTE_CNT_LSB
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 byte_count : 32;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_message_bytes_bigger_than_sdu_length_byte_cnt_lsb_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_message_bytes_bigger_than_sdu_length_byte_cnt_lsb_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_message_bytes_bigger_than_sdu_length_byte_cnt_lsb_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_ERROR_MESSAGE_BYTES_BIGGER_THAN_SDU_LENGTH_BYTE_CNT_MSB
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 byte_count : 8;
	u32 reserved0 : 24;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_message_bytes_bigger_than_sdu_length_byte_cnt_msb_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_error_message_bytes_bigger_than_sdu_length_byte_cnt_msb_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_error_message_bytes_bigger_than_sdu_length_byte_cnt_msb_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_STATE
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 context_bresp_idle : 1;
	u32 address_resolution_idle : 1;
	u32 pipeline_buffer_idle : 1;
	u32 packet_handler_idle : 1;
	u32 timer_idle : 1;
	u32 sequence_handler_idle : 1;
	u32 reserved0 : 26;
} ecpri_hwio_def_ecpri_nfapi_reassembly_state_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_state_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_state_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_SPARE_REG
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 spare_bits : 16;
	u32 reserved0 : 16;
} ecpri_hwio_def_ecpri_spare_reg_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_spare_reg_s def;
	u32 value;
} ecpri_hwio_def_ecpri_spare_reg_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_STATE_GSI_IF
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 ecpri_gsi_aos_fsm_idle : 1;
	u32 ecpri_gsi_if_idle : 1;
	u32 ecpri_gsi_idle : 1;
	u32 reserved0 : 29;
} ecpri_hwio_def_ecpri_state_gsi_if_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_state_gsi_if_s def;
	u32 value;
} ecpri_hwio_def_ecpri_state_gsi_if_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_STATE_GSI_TLV_FIFO_EMPTY_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 channel_fifo_empty : 32;
} ecpri_hwio_def_ecpri_state_gsi_tlv_fifo_empty_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_state_gsi_tlv_fifo_empty_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_state_gsi_tlv_fifo_empty_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_STATE_GSI_AOS_FIFO_EMPTY_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 channel_fifo_empty : 32;
} ecpri_hwio_def_ecpri_state_gsi_aos_fifo_empty_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_state_gsi_aos_fifo_empty_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_state_gsi_aos_fifo_empty_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_YELLOW_MARKER_BELOW_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 endpoints : 32;
} ecpri_hwio_def_ecpri_yellow_marker_below_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_yellow_marker_below_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_yellow_marker_below_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_YELLOW_MARKER_BELOW_EN_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 endpoints : 32;
} ecpri_hwio_def_ecpri_yellow_marker_below_en_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_yellow_marker_below_en_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_yellow_marker_below_en_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_YELLOW_MARKER_BELOW_CLR_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 endpoints : 32;
} ecpri_hwio_def_ecpri_yellow_marker_below_clr_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_yellow_marker_below_clr_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_yellow_marker_below_clr_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_RED_MARKER_BELOW_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 endpoints : 32;
} ecpri_hwio_def_ecpri_red_marker_below_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_red_marker_below_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_red_marker_below_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_RED_MARKER_BELOW_EN_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 endpoints : 32;
} ecpri_hwio_def_ecpri_red_marker_below_en_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_red_marker_below_en_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_red_marker_below_en_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_RED_MARKER_BELOW_CLR_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 endpoints : 32;
} ecpri_hwio_def_ecpri_red_marker_below_clr_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_red_marker_below_clr_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_red_marker_below_clr_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_YELLOW_MARKER_SHADOW_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 endpoints : 32;
} ecpri_hwio_def_ecpri_yellow_marker_shadow_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_yellow_marker_shadow_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_yellow_marker_shadow_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_RED_MARKER_SHADOW_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 endpoints : 32;
} ecpri_hwio_def_ecpri_red_marker_shadow_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_red_marker_shadow_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_red_marker_shadow_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_YELLOW_MARKER_ABOVE_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 endpoints : 32;
} ecpri_hwio_def_ecpri_yellow_marker_above_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_yellow_marker_above_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_yellow_marker_above_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_YELLOW_MARKER_ABOVE_EN_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 endpoints : 32;
} ecpri_hwio_def_ecpri_yellow_marker_above_en_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_yellow_marker_above_en_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_yellow_marker_above_en_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_YELLOW_MARKER_ABOVE_CLR_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 endpoints : 32;
} ecpri_hwio_def_ecpri_yellow_marker_above_clr_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_yellow_marker_above_clr_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_yellow_marker_above_clr_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_RED_MARKER_ABOVE_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 endpoints : 32;
} ecpri_hwio_def_ecpri_red_marker_above_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_red_marker_above_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_red_marker_above_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_RED_MARKER_ABOVE_EN_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 endpoints : 32;
} ecpri_hwio_def_ecpri_red_marker_above_en_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_red_marker_above_en_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_red_marker_above_en_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_RED_MARKER_ABOVE_CLR_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 endpoints : 32;
} ecpri_hwio_def_ecpri_red_marker_above_clr_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_red_marker_above_clr_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_red_marker_above_clr_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_ENDP_CFG_DESTn
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 dest_mem_channel : 8;
	u32 arb_weight : 4;
	u32 reserved0 : 19;
	u32 use_dest_cfg : 1;
} ecpri_hwio_def_ecpri_endp_cfg_destn_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_endp_cfg_destn_s def;
	u32 value;
} ecpri_hwio_def_ecpri_endp_cfg_destn_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_ENDP_CFG_XBARn
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 dest_stream : 2;
	u32 reserved0 : 2;
	u32 xbar_tid : 4;
	u32 xbar_tuser : 20;
	u32 reserved1 : 3;
	u32 l2_segmentation_en : 1;
} ecpri_hwio_def_ecpri_endp_cfg_xbarn_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_endp_cfg_xbarn_s def;
	u32 value;
} ecpri_hwio_def_ecpri_endp_cfg_xbarn_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_ENDP_CFG_AGGR_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 aggr_type : 1;
	u32 reserved0 : 31;
} ecpri_hwio_def_ecpri_endp_cfg_aggr_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_endp_cfg_aggr_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_endp_cfg_aggr_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_ENDP_GSI_CFG_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 endp_en : 1;
	u32 reserved0 : 7;
	u32 endp_flush : 1;
	u32 reserved1 : 23;
} ecpri_hwio_def_ecpri_endp_gsi_cfg_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_endp_gsi_cfg_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_endp_gsi_cfg_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_ENDP_YELLOW_RED_MARKER_CFG_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 reserved0 : 10;
	u32 ecpri_yellow_marker_cfg : 6;
	u32 reserved1 : 10;
	u32 ecpri_red_marker_cfg : 6;
} ecpri_hwio_def_ecpri_endp_yellow_red_marker_cfg_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_endp_yellow_red_marker_cfg_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_endp_yellow_red_marker_cfg_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_ENDP_INIT_CTRL_STATUS_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 channel_state : 2;
	u32 reserved0 : 30;
} ecpri_hwio_def_ecpri_endp_init_ctrl_status_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_endp_init_ctrl_status_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_endp_init_ctrl_status_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_ENDP_AXI_ATTR_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 relax_order : 1;
	u32 nosnoop : 1;
	u32 tph : 2;
	u32 reserved0 : 28;
} ecpri_hwio_def_ecpri_endp_axi_attr_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_endp_axi_attr_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_endp_axi_attr_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_ENDP_NFAPI_REASSEMBLY_CFG_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 vm_id : 2;
	u32 reserved0 : 30;
} ecpri_hwio_def_ecpri_endp_nfapi_reassembly_cfg_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_endp_nfapi_reassembly_cfg_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_endp_nfapi_reassembly_cfg_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_SNOC_MONITORING_CFG
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 enable : 1;
	u32 reserved0 : 31;
} ecpri_hwio_def_ecpri_snoc_monitoring_cfg_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_snoc_monitoring_cfg_s def;
	u32 value;
} ecpri_hwio_def_ecpri_snoc_monitoring_cfg_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_QMB0_SNOC_MONITOR_CNT
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 ar_value : 5;
	u32 reserved0 : 1;
	u32 aw_value : 5;
	u32 reserved1 : 1;
	u32 r_value : 5;
	u32 reserved2 : 1;
	u32 w_value : 5;
	u32 reserved3 : 1;
	u32 b_value : 5;
	u32 reserved4 : 3;
} ecpri_hwio_def_ecpri_qmb0_snoc_monitor_cnt_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_qmb0_snoc_monitor_cnt_s def;
	u32 value;
} ecpri_hwio_def_ecpri_qmb0_snoc_monitor_cnt_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_QMB1_SNOC_MONITOR_CNT
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 ar_value : 5;
	u32 reserved0 : 1;
	u32 aw_value : 5;
	u32 reserved1 : 1;
	u32 r_value : 5;
	u32 reserved2 : 1;
	u32 w_value : 5;
	u32 reserved3 : 1;
	u32 b_value : 5;
	u32 reserved4 : 3;
} ecpri_hwio_def_ecpri_qmb1_snoc_monitor_cnt_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_qmb1_snoc_monitor_cnt_s def;
	u32 value;
} ecpri_hwio_def_ecpri_qmb1_snoc_monitor_cnt_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_GSI_SNOC_MONITOR_CNT
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 ar_value : 5;
	u32 reserved0 : 1;
	u32 aw_value : 5;
	u32 reserved1 : 1;
	u32 r_value : 5;
	u32 reserved2 : 1;
	u32 w_value : 5;
	u32 reserved3 : 1;
	u32 b_value : 5;
	u32 sync_pulse : 1;
	u32 reserved4 : 2;
} ecpri_hwio_def_ecpri_gsi_snoc_monitor_cnt_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_gsi_snoc_monitor_cnt_s def;
	u32 value;
} ecpri_hwio_def_ecpri_gsi_snoc_monitor_cnt_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DST_ACKMNGR_CMDQ_STATUS
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 cmdq_full : 1;
	u32 cmdq_depth : 8;
	u32 reserved0 : 23;
} ecpri_hwio_def_ecpri_dst_ackmngr_cmdq_status_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dst_ackmngr_cmdq_status_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dst_ackmngr_cmdq_status_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DST_ACKMNGR_CMDQ_STATUS_EMPTY_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 cmdq_empty : 32;
} ecpri_hwio_def_ecpri_dst_ackmngr_cmdq_status_empty_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dst_ackmngr_cmdq_status_empty_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dst_ackmngr_cmdq_status_empty_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DST_ACKMNGR_CMDQ_COUNT_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 fifo_count : 8;
	u32 reserved0 : 24;
} ecpri_hwio_def_ecpri_dst_ackmngr_cmdq_count_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dst_ackmngr_cmdq_count_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dst_ackmngr_cmdq_count_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_GSI_TLV_FIFO_STATUS_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 fifo_wr_ptr : 8;
	u32 fifo_rd_ptr : 8;
	u32 fifo_empty : 1;
	u32 fifo_almost_full : 1;
	u32 fifo_full : 1;
	u32 fifo_head_is_bubble : 1;
	u32 reserved0 : 12;
} ecpri_hwio_def_ecpri_gsi_tlv_fifo_status_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_gsi_tlv_fifo_status_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_gsi_tlv_fifo_status_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_GSI_AOS_FIFO_STATUS_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 fifo_wr_ptr : 8;
	u32 fifo_rd_ptr : 8;
	u32 fifo_empty : 1;
	u32 fifo_almost_full : 1;
	u32 fifo_full : 1;
	u32 fifo_head_is_bubble : 1;
	u32 reserved0 : 12;
} ecpri_hwio_def_ecpri_gsi_aos_fifo_status_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_gsi_aos_fifo_status_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_gsi_aos_fifo_status_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_ENDP_GSI_CONS_BYTES_TLV_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 cons_bytes : 21;
	u32 reserved0 : 11;
} ecpri_hwio_def_ecpri_endp_gsi_cons_bytes_tlv_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_endp_gsi_cons_bytes_tlv_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_endp_gsi_cons_bytes_tlv_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_ENDP_GSI_CONS_BYTES_AOS_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 cons_bytes : 21;
	u32 reserved0 : 11;
} ecpri_hwio_def_ecpri_endp_gsi_cons_bytes_aos_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_endp_gsi_cons_bytes_aos_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_endp_gsi_cons_bytes_aos_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_FL_PACKET_ID_CTRL
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 limit : 8;
	u32 reserved0 : 24;
} ecpri_hwio_def_ecpri_dma_fl_packet_id_ctrl_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_fl_packet_id_ctrl_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_fl_packet_id_ctrl_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_FL_PACKET_ID_STATUS
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 curr_value : 8;
	u32 max_hw_value : 8;
	u32 reserved0 : 16;
} ecpri_hwio_def_ecpri_dma_fl_packet_id_status_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_fl_packet_id_status_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_fl_packet_id_status_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_FL_MEMORY_SECTOR_CTRL
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 limit : 8;
	u32 reserved0 : 24;
} ecpri_hwio_def_ecpri_dma_fl_memory_sector_ctrl_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_fl_memory_sector_ctrl_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_fl_memory_sector_ctrl_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_FL_MEMORY_SECTOR_STATUS
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 curr_value : 8;
	u32 max_hw_value : 8;
	u32 reserved0 : 16;
} ecpri_hwio_def_ecpri_dma_fl_memory_sector_status_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_fl_memory_sector_status_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_fl_memory_sector_status_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_STREAM_CTRL
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 mem_limit : 8;
	u32 c2c_limit : 8;
	u32 l2_limit : 8;
	u32 fh_limit : 8;
} ecpri_hwio_def_ecpri_dma_stream_ctrl_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_stream_ctrl_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_stream_ctrl_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_STREAM_STATUS
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 mem_curr_value : 8;
	u32 c2c_curr_value : 8;
	u32 l2_curr_value : 8;
	u32 fh_curr_value : 8;
} ecpri_hwio_def_ecpri_dma_stream_status_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_stream_status_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_stream_status_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_STREAM_ARB
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 mem_weight : 4;
	u32 c2c_weight : 4;
	u32 l2_weight : 4;
	u32 fh_weight : 4;
	u32 reserved0 : 16;
} ecpri_hwio_def_ecpri_dma_stream_arb_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_stream_arb_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_stream_arb_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_XBAR
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 reserved0 : 8;
	u32 tx_fifo_watermark_fh : 8;
	u32 tx_fifo_watermark_c2c : 8;
	u32 reserved1 : 8;
} ecpri_hwio_def_ecpri_dma_xbar_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_xbar_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_xbar_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_DBG_CTRL
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 force_empty : 16;
	u32 dma_loopback : 2;
	u32 dma_irq : 2;
	u32 reserved0 : 12;
} ecpri_hwio_def_ecpri_dma_dbg_ctrl_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_dbg_ctrl_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_dbg_ctrl_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_GBL_CFG
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 qmb_512_rd_en : 1;
	u32 qmb_512_wr_en : 1;
	u32 l2_ar_weigth : 4;
	u32 reserved0 : 10;
	u32 frag_size : 16;
} ecpri_hwio_def_ecpri_dma_gbl_cfg_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_gbl_cfg_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_gbl_cfg_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_TESTBUS_CTRL
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 sel : 8;
	u32 sel_internal : 8;
	u32 reserved0 : 16;
} ecpri_hwio_def_ecpri_dma_testbus_ctrl_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_testbus_ctrl_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_testbus_ctrl_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_GP_REG3
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 xbar_tx_mode : 8;
	u32 xbar_rx_mode : 8;
	u32 data : 16;
} ecpri_hwio_def_ecpri_dma_gp_reg3_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_gp_reg3_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_gp_reg3_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_AOS_FIFO_STAT
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 dst : 16;
	u32 src : 16;
} ecpri_hwio_def_ecpri_dma_aos_fifo_stat_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_aos_fifo_stat_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_aos_fifo_stat_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_GP_STAT1
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 data : 32;
} ecpri_hwio_def_ecpri_dma_gp_stat1_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_gp_stat1_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_gp_stat1_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_TESTBUS
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 data : 32;
} ecpri_hwio_def_ecpri_dma_testbus_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_testbus_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_testbus_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_GP_STAT3
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 data : 32;
} ecpri_hwio_def_ecpri_dma_gp_stat3_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_gp_stat3_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_gp_stat3_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_IDLE_REG
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 data : 32;
} ecpri_hwio_def_ecpri_dma_idle_reg_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_idle_reg_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_idle_reg_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_EXCEPTION_CHANNEL
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 channel : 8;
	u32 enable : 1;
	u32 reserved0 : 23;
} ecpri_hwio_def_ecpri_dma_exception_channel_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_exception_channel_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_exception_channel_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DPL_TRIG_CTRL_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 b_enable : 1;
	u32 b_width : 5;
	u32 b_location : 5;
	u32 reserved0 : 1;
	u32 a_enable : 1;
	u32 a_width : 5;
	u32 a_location : 5;
	u32 reserved1 : 1;
	u32 dst_channel : 8;
} ecpri_hwio_def_ecpri_dpl_trig_ctrl_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dpl_trig_ctrl_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dpl_trig_ctrl_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DPL_TRIG_A_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 value : 32;
} ecpri_hwio_def_ecpri_dpl_trig_a_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dpl_trig_a_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dpl_trig_a_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DPL_TRIG_B_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 value : 32;
} ecpri_hwio_def_ecpri_dpl_trig_b_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dpl_trig_b_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dpl_trig_b_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_PKT_DROP_FULL
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 cnt : 24;
	u32 reserved0 : 8;
} ecpri_hwio_def_ecpri_dma_pkt_drop_full_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_pkt_drop_full_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_pkt_drop_full_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_PKT_DROP_RE_EMPTY
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 cnt : 24;
	u32 reserved0 : 8;
} ecpri_hwio_def_ecpri_dma_pkt_drop_re_empty_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_pkt_drop_re_empty_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_pkt_drop_re_empty_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_DPL_CFG
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 dpl_mtu : 8;
	u32 dpl_select : 4;
	u32 reserved0 : 2;
	u32 dpl_flush_pkt : 2;
	u32 reserved1 : 16;
} ecpri_hwio_def_ecpri_dma_dpl_cfg_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_dpl_cfg_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_dpl_cfg_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DMA_TPDM_CFG
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 record_type : 2;
	u32 reserved0 : 2;
	u32 record_rate : 4;
	u32 record_channel : 8;
	u32 record_src : 2;
	u32 record_en : 1;
	u32 reserved1 : 1;
	u32 unit_en : 12;
} ecpri_hwio_def_ecpri_dma_tpdm_cfg_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dma_tpdm_cfg_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dma_tpdm_cfg_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NSO_CFG
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 nfapi_hdr_byte_swap : 1;
	u32 min_pkt_enable : 1;
	u32 max_num_of_sectors : 7;
	u32 reserved0 : 7;
	u32 max_df_sectors : 4;
	u32 reserved1 : 12;
} ecpri_hwio_def_ecpri_nso_cfg_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nso_cfg_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nso_cfg_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NSO_JUMBO_PKT_CFG
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 jumbo_max_pkt_len : 16;
	u32 jumbo_min_pkt_len : 16;
} ecpri_hwio_def_ecpri_nso_jumbo_pkt_cfg_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nso_jumbo_pkt_cfg_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nso_jumbo_pkt_cfg_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NSO_SHORT_PKT_CFG
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 short_max_pkt_len : 16;
	u32 short_min_pkt_len : 16;
} ecpri_hwio_def_ecpri_nso_short_pkt_cfg_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nso_short_pkt_cfg_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nso_short_pkt_cfg_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NSO_JUMBO_PKT_EN_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 jumbo_pkt_en : 32;
} ecpri_hwio_def_ecpri_nso_jumbo_pkt_en_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nso_jumbo_pkt_en_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nso_jumbo_pkt_en_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NSO_LEN_ERR_STATUS_1
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 nfapi_msg_len_err : 1;
	u32 nfapi_msg_too_short : 1;
	u32 nfapi_msg_too_long : 1;
	u32 src_id_type : 1;
	u32 src_id : 7;
	u32 dst_id_type : 1;
	u32 dst_id : 7;
	u32 pkt_id : 8;
	u32 reserved0 : 5;
} ecpri_hwio_def_ecpri_nso_len_err_status_1_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nso_len_err_status_1_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nso_len_err_status_1_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NSO_LEN_ERR_STATUS_2
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 nfapi_remaining_len : 24;
	u32 reserved0 : 8;
} ecpri_hwio_def_ecpri_nso_len_err_status_2_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nso_len_err_status_2_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nso_len_err_status_2_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NSO_LEN_ERR_STATUS_3
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 tuser_tid : 32;
} ecpri_hwio_def_ecpri_nso_len_err_status_3_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nso_len_err_status_3_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nso_len_err_status_3_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NSO_LEN_ERR_STATUS_HDR_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 nfapi_msg_hdr : 32;
} ecpri_hwio_def_ecpri_nso_len_err_status_hdr_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nso_len_err_status_hdr_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nso_len_err_status_hdr_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NSO_DBG_CNTXT_n_INFO_1
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 eng_state : 3;
	u32 src_id : 7;
	u32 size : 14;
	u32 num_of_sectors : 4;
	u32 last : 1;
	u32 msg_too_long : 1;
	u32 jumbo_pkt_en : 1;
	u32 nfapi_hdr_valid : 1;
} ecpri_hwio_def_ecpri_nso_dbg_cntxt_n_info_1_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nso_dbg_cntxt_n_info_1_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nso_dbg_cntxt_n_info_1_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NSO_DBG_CNTXT_n_INFO_2
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 nfapi_remaining_len : 24;
	u32 eng_state : 3;
	u32 reserved0 : 5;
} ecpri_hwio_def_ecpri_nso_dbg_cntxt_n_info_2_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nso_dbg_cntxt_n_info_2_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nso_dbg_cntxt_n_info_2_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NSO_DBG_MISC_INFO
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 eng_state : 24;
	u32 total_num_of_sectors : 7;
	u32 reserved0 : 1;
} ecpri_hwio_def_ecpri_nso_dbg_misc_info_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nso_dbg_misc_info_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nso_dbg_misc_info_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_DPL_CFG_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 record_size : 9;
	u32 record_enable : 1;
	u32 reserved0 : 22;
} ecpri_hwio_def_ecpri_dpl_cfg_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_dpl_cfg_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_dpl_cfg_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_VM_CFG_0_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 max_messages : 8;
	u32 reserved0 : 8;
	u32 timer_gran_sel : 2;
	u32 reserved1 : 6;
	u32 timer_value : 8;
} ecpri_hwio_def_ecpri_nfapi_reassembly_vm_cfg_0_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_vm_cfg_0_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_vm_cfg_0_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_VM_CFG_1_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 large_message_byte_threshold : 32;
} ecpri_hwio_def_ecpri_nfapi_reassembly_vm_cfg_1_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_vm_cfg_1_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_vm_cfg_1_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_NFAPI_REASSEMBLY_VM_CFG_2_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 maximum_message_length : 21;
	u32 reserved0 : 11;
} ecpri_hwio_def_ecpri_nfapi_reassembly_vm_cfg_2_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_nfapi_reassembly_vm_cfg_2_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_nfapi_reassembly_vm_cfg_2_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_IRQ_STTS_EE_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 bad_snoc_access_irq : 1;
	u32 nro_packet_bigger_than_sdu_length_irq : 1;
	u32 nro_not_enough_contexts_irq : 1;
	u32 nro_not_enough_bytes_irq : 1;
	u32 nro_timeout_irq : 1;
	u32 nro_bad_total_sdu_length_irq : 1;
	u32 nro_bad_timestamp_irq : 1;
	u32 nfapi_segmentation_irq : 1;
	u32 pipe_yellow_marker_below_irq : 1;
	u32 pipe_yellow_marker_above_irq : 1;
	u32 pipe_red_marker_below_irq : 1;
	u32 pipe_red_marker_above_irq : 1;
	u32 nro_message_too_big_irq : 1;
	u32 nro_message_bigger_than_sdu_length_irq : 1;
	u32 xbar_rx_pkt_drop_type_1_irq : 1;
	u32 xbar_rx_pkt_drop_type_2_irq : 1;
	u32 xbar_rx_pkt_drop_type_3_irq : 1;
	u32 xbar_rx_pkt_drop_type_4_irq : 1;
	u32 reserved0 : 14;
} ecpri_hwio_def_ecpri_irq_stts_ee_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_irq_stts_ee_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_irq_stts_ee_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_IRQ_EN_EE_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 bad_snoc_access_irq_en : 1;
	u32 nro_packet_bigger_than_sdu_length_irq_en : 1;
	u32 nro_not_enough_contexts_irq_en : 1;
	u32 nro_not_enough_bytes_irq_en : 1;
	u32 nro_timeout_irq_en : 1;
	u32 nro_bad_total_sdu_length_irq_en : 1;
	u32 nro_bad_timestamp_irq_en : 1;
	u32 nfapi_segmentation_irq_en : 1;
	u32 pipe_yellow_marker_below_irq_en : 1;
	u32 pipe_yellow_marker_above_irq_en : 1;
	u32 pipe_red_marker_below_irq_en : 1;
	u32 pipe_red_marker_above_irq_en : 1;
	u32 nro_message_too_big_irq_en : 1;
	u32 nro_message_bigger_than_sdu_length_irq_en : 1;
	u32 xbar_rx_pkt_drop_type_1_irq_en : 1;
	u32 xbar_rx_pkt_drop_type_2_irq_en : 1;
	u32 xbar_rx_pkt_drop_type_3_irq_en : 1;
	u32 xbar_rx_pkt_drop_type_4_irq_en : 1;
	u32 reserved0 : 14;
} ecpri_hwio_def_ecpri_irq_en_ee_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_irq_en_ee_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_irq_en_ee_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_IRQ_CLR_EE_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 bad_snoc_access_irq_clr : 1;
	u32 nro_packet_bigger_than_sdu_length_irq_clr : 1;
	u32 nro_not_enough_contexts_irq_clr : 1;
	u32 nro_not_enough_bytes_irq_clr : 1;
	u32 nro_timeout_irq_clr : 1;
	u32 nro_bad_total_sdu_length_irq_clr : 1;
	u32 nro_bad_timestamp_irq_clr : 1;
	u32 nfapi_segmentation_irq_clr : 1;
	u32 pipe_yellow_marker_below_irq_clr : 1;
	u32 pipe_yellow_marker_above_irq_clr : 1;
	u32 pipe_red_marker_below_irq_clr : 1;
	u32 pipe_red_marker_above_irq_clr : 1;
	u32 nro_message_too_big_irq_clr : 1;
	u32 nro_message_bigger_than_sdu_length_irq_clr : 1;
	u32 xbar_rx_pkt_drop_type_1_irq_clr : 1;
	u32 xbar_rx_pkt_drop_type_2_irq_clr : 1;
	u32 xbar_rx_pkt_drop_type_3_irq_clr : 1;
	u32 xbar_rx_pkt_drop_type_4_irq_clr : 1;
	u32 reserved0 : 14;
} ecpri_hwio_def_ecpri_irq_clr_ee_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_irq_clr_ee_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_irq_clr_ee_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_ENDP_GSI_IF_FIFO_CFG_TLV_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 fifo_base_addr : 12;
	u32 reserved0 : 4;
	u32 fifo_size : 8;
	u32 reserved1 : 8;
} ecpri_hwio_def_ecpri_endp_gsi_if_fifo_cfg_tlv_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_endp_gsi_if_fifo_cfg_tlv_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_endp_gsi_if_fifo_cfg_tlv_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_ENDP_GSI_IF_FIFO_CFG_AOS_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 fifo_base_addr : 12;
	u32 reserved0 : 4;
	u32 fifo_size : 8;
	u32 reserved1 : 8;
} ecpri_hwio_def_ecpri_endp_gsi_if_fifo_cfg_aos_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_endp_gsi_if_fifo_cfg_aos_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_endp_gsi_if_fifo_cfg_aos_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_ENDP_CFG_VFID_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 vfid : 3;
	u32 reserved0 : 29;
} ecpri_hwio_def_ecpri_endp_cfg_vfid_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_endp_cfg_vfid_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_endp_cfg_vfid_n_u;

/*===========================================================================*/
/*!
@brief Bit Field definition of register: ECPRI_GSI_EE_VFID_n
*/
/*===========================================================================*/
/* Structure definition of register */
typedef struct {
	u32 vfid : 3;
	u32 reserved0 : 29;
} ecpri_hwio_def_ecpri_gsi_ee_vfid_n_s;

/* Union definition of register */
typedef union {
	ecpri_hwio_def_ecpri_gsi_ee_vfid_n_s def;
	u32 value;
} ecpri_hwio_def_ecpri_gsi_ee_vfid_n_u;


#endif /* __ECPRI_HWIO_DEF_H__ */
