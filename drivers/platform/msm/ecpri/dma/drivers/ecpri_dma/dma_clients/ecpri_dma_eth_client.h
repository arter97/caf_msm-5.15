/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ECPRI_DMA_ETH_CLIENT_H_
#define _ECPRI_DMA_ETH_CLIENT_H_

#include <linux/idr.h>
#include "ecpri_dma_i.h"
#include "ecpri_dma_utils.h"
#include "ecpri_dma_eth.h"
#include "ecpri_dma_dp.h"

/* Enums & Defenitions */

#define ECPRI_DMA_ETH_CLIENT_MAX_FH_CONNTECTIONS 12
#define ECPRI_DMA_ETH_CLIENT_MAX_C2C_CONNTECTIONS 2
#define ECPRI_DMA_ETH_CLIENT_MAX_L2_CONNTECTIONS 1
#define ECPRI_DMA_ETH_CLIENT_MAX_CONNTECTIONS ( \
                                ECPRI_DMA_ETH_CLIENT_MAX_FH_CONNTECTIONS + \
                                ECPRI_DMA_ETH_CLIENT_MAX_C2C_CONNTECTIONS + \
                                ECPRI_DMA_ETH_CLIENT_MAX_L2_CONNTECTIONS )
#define ECPRI_DMA_ETH_CLIENT_MIN_CONNECTION_ID 1

/* Structuers */

/**
 * struct ecpri_dma_eth_client_connection - DMA ETH connection context
 * @valid: True if connection is in use and is valid
 * @hdl: Unique connection handle
 * @tx_endp_ctx: Pointer to DMA ENDP context for this connection Tx ENDP
 * @rx_endp_ctx: Pointer to DMA ENDP context for this connection Rx ENDP
 * @rx_notify_mode: Currnet Rx ENDP notify mode (IRQ \ POLL)
 * @link_idx: Link index of the ENDP
 * @p_type : Port type of the ENDP
 * @tx_mod_cfg : Moderation configurations of the Tx ENDP
 * @received_irq_during_poll: Number of IRQ received whilst driver
 * in the POLL mode
 */
struct ecpri_dma_eth_client_connection {
    bool valid;
    ecpri_dma_eth_conn_hdl_t hdl;
    struct ecpri_dma_endp_context *tx_endp_ctx;
    struct ecpri_dma_endp_context *rx_endp_ctx;
    enum ecpri_dma_notify_mode rx_notify_mode;
    u32 link_idx;
    enum ecpri_dma_endp_stream_dest p_type;
    struct ecpri_dma_moderation_config tx_mod_cfg;
    u32 received_irq_during_poll;
};

/*
* ecpri_dma_eth_client_endp_mapping - ENDPs mapping per connection
* NOTE: Should be updated per driver change
*
* @valid: Is channel valid
* @tx_endp_id: TX ENDP of the connection
* @rx_endp_id: RX ENDP of the connection
*/
struct ecpri_dma_eth_client_endp_mapping {
    bool valid;
    u32 tx_endp_id;
    u32 rx_endp_id;
};

/**
  * struct ecpri_dma_eth_client_context - DMA ETH client context
  * @lock: General lock to protect sensetive resources
  * @is_eth_ready: True if ETH client has been initialized and is ready
  * @is_eth_notified_ready: True if ETH client has notified ETH driver that it
                            is ready
  * @idr: ETH client IDR instace for uniqe ID generation
  * @idr_lock: Spinlock to protect IDR usage
  * @connections: ETH client connection array
  * @ready_cb: ETH driver CB to invoke once DMA and ETH client are ready
  * @ready_cb_user_data: User data to use when invoking ready_cb
  * @tx_comp_cb: ETH driver CB to invoke upon receiving Tx completion
                 notification from DMA
  * @tx_comp_cb_user_data: User data to use when invoking tx_comp_cb
  * @rx_comp_cb: ETH driver CB to invoke upon receiving Rx completion
                 notification from DMA
  * @rx_comp_cb_user_data: User data to use when invoking rx_comp_cb
  * @link_to_endp_mapping: mapping of link_index to endps per hw ver & flavor
  */
struct ecpri_dma_eth_client_context {
    struct mutex lock;
    bool is_eth_ready;
    bool is_eth_notified_ready;
    struct idr idr;
    spinlock_t idr_lock;
    struct ecpri_dma_eth_client_connection
	    connections[ECPRI_DMA_ETH_CLIENT_MAX_CONNTECTIONS];
    struct ecpri_dma_eth_client_endp_mapping *link_to_endp_mapping;

    ecpri_dma_ready_cb ready_cb;
    void *ready_cb_user_data;

    ecpri_dma_eth_tx_comp_cb tx_comp_cb;
    void *tx_comp_cb_user_data;

    ecpri_dma_eth_rx_comp_cb rx_comp_cb;
    void *rx_comp_cb_user_data;
};

/* Functions */

/**
  * ecpri_dma_eth_client_init() - Initialising DMA ETH Client
  */
int ecpri_dma_eth_client_init(void);

/**
  * ecpri_dma_eth_client_destroy - Destroys and free resources held by
  * DMA ETH client context
  */
int ecpri_dma_eth_client_destroy(void);

#endif /* _ECPRI_DMA_ETH_CLIENT_H_ */
