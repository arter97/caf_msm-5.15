/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ECPRI_DMA_SS_CORE_H_
#define _ECPRI_DMA_SS_CORE_H_

#include "ecpri_dma_i.h"
#include "ecpri_dma_utils.h"
#include "ecpri_dma_ecpri_ss.h"
#include <linux/mutex.h>
#include <linux/workqueue.h>

/**
 * struct ecpri_dma_eth_client_context - DMA ETH client context
 *
 * @lock:                       general lock to protect sensetive resources
 * @wq:                         workqueue struct
 * @ecpri_hw_ver:               type of eCPRI HW type (e.g. eCPRI 1.0 etc')
 * @hw_flavor:                  current HW flavor (Ru \ Du - L2 \ Du - PCIe)
 * @ready_cb:                   ecpri_ss_core client ready callback notifier
 * @ready_user_data:            userdata for ecpri_ss_core ready cb
 * @event_notify_cb:            ecpri_ss_core client to notify eCPRI SS driver
 *                              of DMA events
 * @event_notify_user_data:     userdata for ecpri_ss_core event notify CB
 * @log_msg_cb:                 ecpri_ss_core client to log a message
 *                              with eCPRI SS driver logging API
 * @log_msg_user_data:          userdata for ecpri_ss_core logging API
 *
 *
 */
struct ecpri_dma_ss_core_context {
    spinlock_t lock;
    struct workqueue_struct* wq;
    enum ecpri_hw_ver ecpri_hw_ver;
    enum ecpri_hw_flavor hw_flavor;
    ecpri_dma_ready_cb ready_cb;
    void* ready_user_data;
    ecpri_dma_ecpri_ss_dma_event_notify_cb event_notify_cb;
    void* event_notify_user_data;
    ecpri_dma_ecpri_ss_log_msg_cb log_msg_cb;
    void* log_msg_user_data;
};

#endif /* _ECPRI_DMA_SS_CORE_H_ */
