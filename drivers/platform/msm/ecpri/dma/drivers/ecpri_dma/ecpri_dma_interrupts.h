/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ECPRI_DMA_INTERRUPTS_H_
#define _ECPRI_DMA_INTERRUPTS_H_

/** Enums & Defiens **/

 /**
 * enum ecpri_dma_irq_type - DMA Interrupt Type
 * Used to register handlers for DMA interrupts
 *
 * Below enum is a logical mapping and not the actual interrupt bit in HW
 */
enum ecpri_dma_irq_type {
    DMA_BAD_SNOC_ACCESS_IRQ,
    NRO_PACKET_BIGGER_THAN_SDU_LENGTH_IRQ,
    NRO_NOT_ENOUGH_CONTEXTS_IRQ,
    NRO_NOT_ENOUGH_BYTES_IRQ,
    NRO_TIMEOUT_IRQ,
    NRO_BAD_TOTAL_SDU_LENGTH_IRQ,
    NRO_BAD_TIMESTAMP_IRQ,
    NFAPI_SEGMENTATION_IRQ,
    PIPE_YELLOW_MARKER_BELOW_IRQ,
    PIPE_YELLOW_MARKER_ABOVE_IRQ,
    PIPE_RED_MARKER_BELOW_IRQ,
    PIPE_RED_MARKER_ABOVE_IRQ,
    NRO_MESSAGE_TOO_BIG_IRQ,
    NRO_MESSAGE_BIGGER_THAN_SDU_LENGTH_IRQ,
    XBAR_RX_PKT_DROP_TYPE_1_IRQ,
    XBAR_RX_PKT_DROP_TYPE_2_IRQ,
    XBAR_RX_PKT_DROP_TYPE_3_IRQ,
    XBAR_RX_PKT_DROP_TYPE_4_IRQ,
    DMA_IRQ_MAX
};

 /** Functions **/

int ecpri_dma_interrupts_init(u32 ecpri_dma_irq, u32 ee,
    struct device *ecpri_dma_dev);
void ecpri_dma_interrupts_destroy(u32 ecpri_dma_irq,
    struct device *ecpri_dma_dev);

 /**
  * typedef ecpri_dma_irq_handler_t - irq handler/callback type
  * @param ecpri_dma_irq_type - [in] interrupt type
  * @param private_data - [in, out] the client private data
  *
  * callback registered by ecpri_dma_add_interrupt_handler function to
  * handle a specific interrupt type
  *
  * No return value
  */
typedef void (*ecpri_dma_irq_handler_t)(enum ecpri_dma_irq_type interrupt,
    void *private_data);

#endif /* _ECPRI_DMA_INTERRUPTS_H_ */