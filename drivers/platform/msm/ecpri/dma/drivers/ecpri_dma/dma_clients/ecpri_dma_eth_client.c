/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/dmapool.h>
#include <linux/list.h>
#include "ecpri_dma_i.h"
#include "ecpri_dma_dp.h"
#include "ecpri_dma_eth_client.h"

struct ecpri_dma_eth_client_endp_mapping
	eth_client_endp_map[ECPRI_HW_MAX][ECPRI_HW_FLAVOR_MAX]
[ECPRI_DMA_ETH_CLIENT_MAX_CONNTECTIONS] = {
	/* RU Connections */
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_RU][0]       = { true, 0,  37 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_RU][1]       = { true, 1,  38 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_RU][2]       = { true, 2,  39 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_RU][3]       = { true, 3,  40 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_RU][4]       = { true, 4,  41 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_RU][5]       = { true, 5,  42 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_RU][6]       = { true, 6,  43 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_RU][7]       = { true, 7,  44 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_RU][8]       = { true, 8,  45 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_RU][9]       = { true, 9,  46 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_RU][10]      = { true, 10, 47 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_RU][11]      = { true, 11, 48 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_RU][12]      = { true, 14, 51 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_RU][13]      = { true, 15, 52 },
	/* DU-PCIe Connections */
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_PCIE][0]  = { true, 0,  37 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_PCIE][1]  = { true, 1,  38 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_PCIE][2]  = { true, 2,  39 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_PCIE][3]  = { true, 3,  40 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_PCIE][4]  = { true, 4,  41 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_PCIE][5]  = { true, 5,  42 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_PCIE][6]  = { true, 6,  43 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_PCIE][7]  = { true, 7,  44 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_PCIE][8]  = { true, 8,  45 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_PCIE][9]  = { true, 9,  46 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_PCIE][10] = { true, 10, 47 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_PCIE][11] = { true, 11, 48 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_PCIE][12] = { true, 34, 72 },
	/* DU-L2 Connections */
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_L2][0]    = { true, 0,  37 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_L2][1]    = { true, 1,  38 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_L2][2]    = { true, 2,  39 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_L2][3]    = { true, 3,  40 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_L2][4]    = { true, 4,  41 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_L2][5]    = { true, 5,  42 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_L2][6]    = { true, 6,  43 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_L2][7]    = { true, 7,  44 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_L2][8]    = { true, 8,  45 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_L2][9]    = { true, 9,  46 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_L2][10]   = { true, 10, 47 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_L2][11]   = { true, 11, 48 },
	[ECPRI_HW_V1_0][ECPRI_HW_FLAVOR_DU_L2][12]   = { true, 20, 57 },
};

struct ecpri_dma_eth_client_context *ecpri_dma_eth_client_ctx = NULL;

static struct ecpri_dma_eth_client_connection*
ecpri_dma_eth_client_get_conn_from_hdl (ecpri_dma_eth_conn_hdl_t hdl) {
	struct ecpri_dma_eth_client_connection *connection;

	spin_lock_bh(&ecpri_dma_eth_client_ctx->idr_lock);
	connection = idr_find(&ecpri_dma_eth_client_ctx->idr, hdl);
	spin_unlock_bh(&ecpri_dma_eth_client_ctx->idr_lock);

	return connection;
}

static void ecpri_dma_eth_client_register_ready(void *user_data)
{
	int i = 0;
	u32 hw_ver = ecpri_dma_get_ctx_hw_ver();
	u32 hw_flavor = ecpri_dma_get_ctx_hw_flavor();
	struct ecpri_dma_eth_client_endp_mapping *current_map =
		&eth_client_endp_map[hw_ver][hw_flavor][0];

	DMADBG_LOW("Begin\n");

	if (!ecpri_dma_eth_client_ctx || !ecpri_dma_eth_client_ctx->ready_cb) {
		DMAERR("ETH Ready Callback is NULL\n");
		return;
	}

	ecpri_dma_eth_client_ctx->link_to_endp_mapping = current_map;

	/* Endpoints mapping */
	for (i = 0; i < ECPRI_DMA_ETH_CLIENT_MAX_CONNTECTIONS; i++) {
		if (current_map[i].valid) {
			ecpri_dma_eth_client_ctx->connections[i].tx_endp_ctx =
				&ecpri_dma_ctx
					 ->endp_ctx[current_map[i].tx_endp_id];

			ecpri_dma_eth_client_ctx->connections[i].rx_endp_ctx =
				&ecpri_dma_ctx
					 ->endp_ctx[current_map[i].rx_endp_id];
		}
	}

	ecpri_dma_eth_client_ctx->is_eth_ready = true;

	ecpri_dma_eth_client_ctx->ready_cb(user_data);

	mutex_lock(&ecpri_dma_eth_client_ctx->lock);
	ecpri_dma_eth_client_ctx->is_eth_notified_ready = true;
	mutex_unlock(&ecpri_dma_eth_client_ctx->lock);

	DMADBG("Succeeded\n");
}

static void dma_eth_client_tx_comp_hdlr(
	struct ecpri_dma_endp_context *endp,
	struct ecpri_dma_pkt_completion_wrapper **comp_pkt,
	u32 num_of_completed)
{
	struct ecpri_dma_eth_client_connection *connection;

	DMADBG_LOW("Begin\n");

	if (!ecpri_dma_eth_client_ctx
	    || !ecpri_dma_eth_client_ctx->is_eth_ready) {
		DMAERR("Context not initialized\n");
		return;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_client_get_conn_from_hdl(endp->hdl);
	if (!connection || !connection->valid) {
		DMAERR("Connection invalid handle:%x, ENDP ID:%d\n", endp->hdl,
			endp->endp_id);
		return;
	}

	ecpri_dma_eth_client_ctx->tx_comp_cb(
		ecpri_dma_eth_client_ctx->tx_comp_cb_user_data,
		connection->hdl, comp_pkt, num_of_completed);

	DMADBG_LOW("Exit\n");
}

static void dma_eth_client_rx_comp_hdlr(
	struct ecpri_dma_endp_context *endp,
	struct ecpri_dma_pkt_completion_wrapper **comp_pkt,
	u32 num_of_completed)
{
	struct ecpri_dma_eth_client_connection *connection;

	DMADBG_LOW("Begin\n");

	if (!ecpri_dma_eth_client_ctx
	    || !ecpri_dma_eth_client_ctx->is_eth_ready) {
		DMAERR("Context not initialized\n");
		return;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_client_get_conn_from_hdl(endp->hdl);
	if (!connection || !connection->valid) {
		DMAERR("Connection invalid handle:%x, ENDP ID:%d\n", endp->hdl,
			endp->endp_id);
		return;
	}

	if (connection->rx_notify_mode == ECPRI_DMA_NOTIFY_MODE_IRQ) {
		/* Move connection to POLL mode, GSI alredy moved to poll */
		connection->rx_notify_mode = ECPRI_DMA_NOTIFY_MODE_POLL;

		/* Handle IRQ mode */
		ecpri_dma_eth_client_ctx->rx_comp_cb(
			ecpri_dma_eth_client_ctx->rx_comp_cb_user_data,
			connection->hdl);
	} else {
		/* Handle POLL mode*/
		connection->received_irq_during_poll++;
	}

	DMADBG_LOW("Exit\n");
}

/**
 * Parameters are being verified outside of this function and there's no need
 * to verify them again.
 */
static u32 ecpri_dma_eth_client_conn_hdl_alloc(void *ptr)
{
	ecpri_dma_eth_conn_hdl_t hdl;

	idr_preload(GFP_KERNEL);

	spin_lock_bh(&ecpri_dma_eth_client_ctx->idr_lock);
	hdl = idr_alloc(&ecpri_dma_eth_client_ctx->idr, ptr,
		ECPRI_DMA_ETH_CLIENT_MIN_CONNECTION_ID, 0, GFP_NOWAIT);
	spin_unlock_bh(&ecpri_dma_eth_client_ctx->idr_lock);

	idr_preload_end();

	return hdl;
}

/**
 * Parameters are being verified outside of this function and there's no need
 * to verify them again.
 */
static void ecpri_dma_eth_client_conn_hdl_remove(ecpri_dma_eth_conn_hdl_t hdl) {
	spin_lock_bh(&ecpri_dma_eth_client_ctx->idr_lock);
	idr_remove(&ecpri_dma_eth_client_ctx->idr, hdl);
	spin_unlock_bh(&ecpri_dma_eth_client_ctx->idr_lock);
}

int ecpri_dma_eth_client_init(void)
{
	DMADBG("eCPRI DMA ETH Client init\n");

	/* Check if the context was already initialized */
	if (ecpri_dma_eth_client_ctx)
		return 0;

	ecpri_dma_eth_client_ctx = kzalloc(sizeof(*ecpri_dma_eth_client_ctx),
		GFP_KERNEL);
	if (!ecpri_dma_eth_client_ctx) {
		DMAERR("Failed to allocate dma_eth_client ctx\n");
		return -ENOMEM;
	}

	mutex_init(&ecpri_dma_eth_client_ctx->lock);
	ecpri_dma_eth_client_ctx->is_eth_ready = false;
	ecpri_dma_eth_client_ctx->is_eth_notified_ready = false;

	idr_init(&ecpri_dma_eth_client_ctx->idr);
	spin_lock_init(&ecpri_dma_eth_client_ctx->idr_lock);

	DMADBG_LOW("Exit\n");

	return 0;
}

int ecpri_dma_eth_client_destroy(void)
{
	DMADBG("eCPRI DMA ETH Client destroy\n");

	/* Check if the context is allocated */
	if (!ecpri_dma_eth_client_ctx)
		return 0;

	mutex_destroy(&ecpri_dma_eth_client_ctx->lock);
	idr_destroy(&ecpri_dma_eth_client_ctx->idr);

	kfree(ecpri_dma_eth_client_ctx);
	ecpri_dma_eth_client_ctx = NULL;

	DMADBG_LOW("Exit\n");

	return 0;
}

int ecpri_dma_eth_register(struct ecpri_dma_eth_register_params *ready_info,
	bool *is_dma_ready)
{
	int ret = 0, i = 0;
	bool ready = false;
	u32 hw_ver = 0, hw_flavor = 0;
	struct ecpri_dma_eth_client_endp_mapping *current_map = NULL;

	DMADBG_LOW("Begin\n");

	if (!ready_info || !ready_info->notify_ready
	    || !ready_info->notify_rx_comp || !ready_info->notify_tx_comp
	    || !is_dma_ready) {
		DMAERR("Invalid paramteres\n");
		return -EINVAL;
	}

	if (ecpri_dma_eth_client_ctx
	    && ecpri_dma_eth_client_ctx->is_eth_ready) {
		DMAERR("Context is already initialized\n");
		return -EPERM;
	}

	ret = ecpri_dma_eth_client_init();
	if (ret){
		DMAERR("Failed to init ETH client\n");
		return ret;
	}

	/* Register callbacks*/
	ecpri_dma_eth_client_ctx->ready_cb
		= ready_info->notify_ready;
	ecpri_dma_eth_client_ctx->ready_cb_user_data
		= ready_info->userdata_ready;

	ecpri_dma_eth_client_ctx->tx_comp_cb
		= ready_info->notify_tx_comp;
	ecpri_dma_eth_client_ctx->tx_comp_cb_user_data
		= ready_info->userdata_tx;

	ecpri_dma_eth_client_ctx->rx_comp_cb
		= ready_info->notify_rx_comp;
	ecpri_dma_eth_client_ctx->rx_comp_cb_user_data
		= ready_info->userdata_rx;

	/* Check DMA driver state */

	mutex_lock(&ecpri_dma_ctx->lock);
	ready = ecpri_dma_is_ready();
	mutex_unlock(&ecpri_dma_ctx->lock);

	if (!ready) {
		ret = ecpri_dma_register_ready_cb(
			&ecpri_dma_eth_client_register_ready,
			ready_info->userdata_ready);

		/*
		 * If the call to ecpri_dma_register_ready_cb() above
		 * returns 0, this means that we've succeeded in
		 * queuing up a future call to ecpri_dma_ut_framework_init()
		 * and that the call to it will be made once the DMA
		 * becomes ready.  If this is the case, the call to
		 * ecpri_dma_ut_framework_init() below need not be made.
		 *
		 * If the call to ecpri_dma_register_ready_cb() above
		 * returns -EEXIST, it means that during the call to
		 * ecpri_dma_register_ready_cb(), the DMA has become
		 * ready, and hence, no indirect call to
		 * ecpri_dma_ut_framework_init() will be made, so we need to
		 * call it ourselves below.
		 *
		 * If the call to ecpri_dma_register_ready_cb() above
		 * return something other than 0 or -EEXIST, that's a
		 * hard error.
		 */

		if (ret == -EEXIST) {
			ready = true;
		} else {
			if (ret != 0) {
				DMAERR("DMA ETH client Register failed - %d\n",
					ret);
				return ret;
			}
		}
	} 

	if (ready) {
		hw_ver = ecpri_dma_get_ctx_hw_ver();
		hw_flavor = ecpri_dma_get_ctx_hw_flavor();
		current_map = &eth_client_endp_map[hw_ver][hw_flavor][0];

		ecpri_dma_eth_client_ctx->link_to_endp_mapping = current_map;

		/* Endpoints mapping */
		for (i = 0; i < ECPRI_DMA_ETH_CLIENT_MAX_CONNTECTIONS; i++) {
			if (current_map[i].valid) {
				ecpri_dma_eth_client_ctx->connections[i]
					.tx_endp_ctx =
					&ecpri_dma_ctx->endp_ctx
						 [current_map[i].tx_endp_id];

				ecpri_dma_eth_client_ctx->connections[i]
					.rx_endp_ctx =
					&ecpri_dma_ctx->endp_ctx
						 [current_map[i].rx_endp_id];
			}
		}

		ecpri_dma_eth_client_ctx->is_eth_ready = true;
	}

	*(is_dma_ready) = ready;

	DMADBG_LOW("Exit\n");

	return ret;
}

void ecpri_dma_eth_deregister(void)
{
	DMADBG_LOW("Begin\n");

	if (!ecpri_dma_eth_client_ctx
	    || !ecpri_dma_eth_client_ctx->is_eth_ready) {
		DMAERR("Context is not initialized\n");
		return;
	}

	ecpri_dma_eth_client_ctx->is_eth_ready		= false;
	ecpri_dma_eth_client_ctx->is_eth_notified_ready = false;

	ecpri_dma_eth_client_ctx->ready_cb              = NULL;
	ecpri_dma_eth_client_ctx->ready_cb_user_data    = NULL;

	ecpri_dma_eth_client_ctx->tx_comp_cb            = NULL;
	ecpri_dma_eth_client_ctx->tx_comp_cb_user_data  = NULL;

	ecpri_dma_eth_client_ctx->rx_comp_cb            = NULL;
	ecpri_dma_eth_client_ctx->rx_comp_cb_user_data  = NULL;

	DMADBG_LOW("Exit\n");
}

int ecpri_dma_eth_connect_endpoints(
	struct ecpri_dma_eth_endpoint_connect_params *params,
	ecpri_dma_eth_conn_hdl_t *hdl)
{
	int ret = 0;
	struct ecpri_dma_eth_client_connection *connection;
	struct ecpri_dma_moderation_config rx_mod_cfg = { 1, 0 };

	DMADBG_LOW("Begin\n");

	if (!hdl || !params ||
	params->link_index > ECPRI_DMA_ETH_CLIENT_MAX_CONNTECTIONS ||
		params->rx_ring_length == 0 || params->tx_ring_length == 0 ||
		params->p_type >= ECPRI_DMA_ENDP_STREAM_DEST_MAX ||
		!ecpri_dma_eth_client_ctx->
		link_to_endp_mapping[params->link_index].valid) {
		DMAERR("Invalid paramteres\n");
		return -EINVAL;
	}

	if (!ecpri_dma_eth_client_ctx
	    || !ecpri_dma_eth_client_ctx->is_eth_ready) {
		DMAERR("Context not initialized\n");
		return -EPERM;
	}

	/* Retrieve Connection */
	connection = &ecpri_dma_eth_client_ctx->connections[params->link_index];
	if (connection->valid) {
		DMAERR("Connection is already connected link_index: %d\n",
			params->link_index);
		return -EINVAL;
	}

	ret = ecpri_dma_alloc_endp(
		ecpri_dma_eth_client_ctx->
		link_to_endp_mapping[params->link_index].tx_endp_id,
		params->tx_ring_length, &params->tx_mod_cfg, false,
		&dma_eth_client_tx_comp_hdlr);

	if (ret != 0) {
		DMAERR("Unable to allocate Tx ENDP, ENDP ID:%d\n",
			ecpri_dma_eth_client_ctx->
			link_to_endp_mapping[params->link_index].tx_endp_id);
		return ret;
	}

	ret = ecpri_dma_alloc_endp(
		ecpri_dma_eth_client_ctx->
		link_to_endp_mapping[params->link_index].rx_endp_id,
		params->rx_ring_length, &rx_mod_cfg, false,
		&dma_eth_client_rx_comp_hdlr);

	if (ret != 0) {
		DMAERR("Unable to allocate Rx ENDP, ENDP ID:%d\n",
			ecpri_dma_eth_client_ctx->
			link_to_endp_mapping[params->link_index].rx_endp_id);
		return ret;
	}

	ret = ecpri_dma_enable_dma_endp(connection->tx_endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to enable Tx endpoint, ENDP ID:%d\n",
			ecpri_dma_eth_client_ctx->
			link_to_endp_mapping[params->link_index].tx_endp_id);
		return -EINVAL;
	}

	ret = ecpri_dma_enable_dma_endp(connection->rx_endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to enable Rx endpoint, ENDP ID:%d\n",
			ecpri_dma_eth_client_ctx->
			link_to_endp_mapping[params->link_index].rx_endp_id);
		return -EINVAL;
	}

	/* Connection allocation */
	connection->valid = true;

	*(hdl) = ecpri_dma_eth_client_conn_hdl_alloc((void *)connection);
	connection->hdl = *(hdl);
	connection->tx_endp_ctx->hdl = *(hdl);
	connection->rx_endp_ctx->hdl = *(hdl);
	connection->received_irq_during_poll = 0;

	connection->p_type = params->p_type;
	connection->link_idx = params->link_index;
	connection->rx_notify_mode = ECPRI_DMA_NOTIFY_MODE_IRQ;

	connection->tx_mod_cfg.moderation_counter_threshold =
		params->tx_mod_cfg.moderation_counter_threshold;
	connection->tx_mod_cfg.moderation_timer_threshold =
		params->tx_mod_cfg.moderation_timer_threshold;

	DMADBG_LOW("Exit\n");

	return ret;
}

int ecpri_dma_eth_disconnect_endpoints(ecpri_dma_eth_conn_hdl_t hdl)
{
	int ret = 0;
	struct ecpri_dma_eth_client_connection *connection;

	DMADBG_LOW("Begin\n");

	if (ecpri_dma_get_ctx_hw_ver() == ECPRI_HW_V1_0) {
		DMAERR("Disconnect not supported for v1\n");
		return -EPERM;
	}

	if (!ecpri_dma_eth_client_ctx
	    || !ecpri_dma_eth_client_ctx->is_eth_ready) {
		DMAERR("Context not initialized\n");
		return -EPERM;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_client_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMAERR("Connection invalid handle:%x\n", hdl);
		return -EINVAL;
	}

	ecpri_dma_eth_client_conn_hdl_remove(connection->hdl);

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

int ecpri_dma_eth_start_endpoints(ecpri_dma_eth_conn_hdl_t hdl)
{
	int ret = 0;
	struct ecpri_dma_eth_client_connection *connection;

	DMADBG_LOW("Begin\n");

	if (!ecpri_dma_eth_client_ctx
	    || !ecpri_dma_eth_client_ctx->is_eth_ready) {
		DMAERR("Context not initialized\n");
		return -EPERM;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_client_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMAERR("Connection invalid handle:%x, ENDP ID:%d\n", hdl,
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}

	/* Starting GSI CH */
	ret = ecpri_dma_start_endp(connection->tx_endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to start channel - Tx, ENDP ID:%d\n",
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}

	ret = ecpri_dma_start_endp(connection->rx_endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to start channel - Rx, ENDP ID:%d\n",
			connection->rx_endp_ctx->endp_id);
		return -EINVAL;
	}

	DMADBG_LOW("Exit\n");

	return ret;
}

int ecpri_dma_eth_stop_endpoints(ecpri_dma_eth_conn_hdl_t hdl)
{
	int ret = 0;
	struct ecpri_dma_eth_client_connection *connection;

	DMADBG_LOW("Begin\n");

	if (!ecpri_dma_eth_client_ctx
	    || !ecpri_dma_eth_client_ctx->is_eth_ready) {
		DMAERR("Context not initialized\n");
		return -EPERM;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_client_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMAERR("Connection invalid handle:%x, ENDP ID:%d\n", hdl,
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}


	/* Stopping GSI CH */
	ret = ecpri_dma_stop_endp(connection->tx_endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to stop channel - Tx, ENDP ID:%d\n",
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}

	ret = ecpri_dma_stop_endp(connection->rx_endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to stop channel - Rx, ENDP ID:%d\n",
			connection->rx_endp_ctx->endp_id);
		return -EINVAL;
	}

	DMADBG_LOW("Exit\n");

	return ret;
}

int ecpri_dma_eth_transmit(ecpri_dma_eth_conn_hdl_t hdl,
	struct ecpri_dma_pkt **pkts, u32 num_of_pkts, bool commit)
{
	int ret = 0;
	struct ecpri_dma_eth_client_connection *connection;

	DMADBG_LOW("Begin\n");

	if (!ecpri_dma_eth_client_ctx
	    || !ecpri_dma_eth_client_ctx->is_eth_ready) {
		DMAERR("Context not initialized\n");
		return -EPERM;
	}

	if(!pkts || num_of_pkts == 0){
		DMAERR("Invalid parameters\n");
		return -EINVAL;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_client_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMAERR("Connection invalid handle:%x\n", hdl);
		return -EINVAL;
	}

	/* Transmit */
	ret = ecpri_dma_dp_transmit(connection->tx_endp_ctx,
		pkts, num_of_pkts, commit);
	if (ret != 0) {
		DMAERR("Unable to transmit, handle:%x, ENDP ID:%d\n", hdl,
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}

	DMADBG_LOW("Exit\n");

	return ret;
}

int ecpri_dma_eth_commit(ecpri_dma_eth_conn_hdl_t hdl)
{
	int ret = 0;
	struct ecpri_dma_eth_client_connection *connection;

	DMADBG_LOW("Begin\n");

	if (!ecpri_dma_eth_client_ctx
	    || !ecpri_dma_eth_client_ctx->is_eth_ready) {
		DMAERR("Context not initialized\n");
		return -EPERM;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_client_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMAERR("Connection invalid handle:%x\n", hdl);
		return -EINVAL;
	}

	ret = ecpri_dma_dp_commit(connection->tx_endp_ctx);
	if (ret != 0) {
		DMAERR("Unable to commit, handle:%x, ENDP ID:%d\n", hdl,
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}

	DMADBG_LOW("Exit\n");

	return ret;
}

int ecpri_dma_eth_tx_ring_state(ecpri_dma_eth_conn_hdl_t hdl,
	u32 *available)
{
	int ret = 0;
	struct ecpri_dma_eth_client_connection *connection;

	DMADBG_LOW("Begin\n");

	if (!ecpri_dma_eth_client_ctx
	    || !ecpri_dma_eth_client_ctx->is_eth_ready) {
		DMAERR("Context not initialized\n");
		return -EPERM;
	}

	if(!available){
		DMAERR("Invalid parameters\n");
		return -EINVAL;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_client_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMAERR("Connection invalid handle:%x, ENDP ID:%d\n", hdl,
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}

	*(available) = connection->tx_endp_ctx->ring_length -
		connection->tx_endp_ctx->curr_outstanding_num;

	DMADBG_LOW("Exit\n");

	return ret;
}

int ecpri_dma_eth_rx_ring_state(ecpri_dma_eth_conn_hdl_t hdl,
	u32 *available)
{
	int ret = 0;
	struct ecpri_dma_eth_client_connection *connection;

	DMADBG_LOW("Begin\n");

	if (!ecpri_dma_eth_client_ctx
	    || !ecpri_dma_eth_client_ctx->is_eth_ready) {
		DMAERR("Context not initialized\n");
		return -EPERM;
	}

	if(!available){
		DMAERR("Invalid parameters\n");
		return -EINVAL;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_client_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMAERR("Connection invalid, handle:%x, ENDP ID:%d\n", hdl,
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}

	*(available) = connection->rx_endp_ctx->ring_length -
		connection->rx_endp_ctx->curr_outstanding_num;

	DMADBG_LOW("Exit\n");

	return ret;
}

int ecpri_dma_eth_rx_mode_set(ecpri_dma_eth_conn_hdl_t hdl,
	enum ecpri_dma_notify_mode mode)
{
	int ret = 0;
	struct ecpri_dma_eth_client_connection *connection;

	DMADBG_LOW("Begin\n");

	if(mode >= ECPRI_DMA_NOTIFY_MODE_MAX) {
		DMAERR("Invalid parameters\n");
		return -EINVAL;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_client_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMAERR("Connection invalid handle:%x, ENDP ID:%d\n", hdl,
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}

	connection->rx_notify_mode = mode;

	ret = ecpri_dma_set_endp_mode(connection->rx_endp_ctx, mode);
	if (ret != 0) {
		DMAERR("Unable to set Rx mode, handle:%x, ENDP ID:%d,"
		       "Current mode: %d, New mode: %d, \n", hdl,
			connection->rx_endp_ctx->endp_id,
			connection->rx_notify_mode, mode);
		return -EINVAL;
	}

	DMADBG_LOW("Exit\n");

	return ret;
}

int ecpri_dma_eth_rx_mode_get(ecpri_dma_eth_conn_hdl_t hdl,
	enum ecpri_dma_notify_mode *mode)
{
	int ret = 0;
	struct ecpri_dma_eth_client_connection *connection;

	DMADBG_LOW("Begin\n");

	if(!mode) {
		DMAERR("Invalid parameters\n");
		return -EINVAL;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_client_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMAERR("Connection invalid handle:%x, ENDP ID:%d\n", hdl,
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}

	ret = ecpri_dma_get_endp_mode(connection->rx_endp_ctx, mode);
	if (ret != 0) {
		DMAERR("Unable to get Rx mode, handle:%x, ENDP ID:%d\n", hdl,
			connection->rx_endp_ctx->endp_id);
		return -EINVAL;
	}

	DMADBG_LOW("Exit\n");

	return ret;
}

int ecpri_dma_eth_rx_poll(ecpri_dma_eth_conn_hdl_t hdl, u32 budget,
	struct ecpri_dma_pkt_completion_wrapper **pkts, u32 *actual_num)
{
	int ret = 0;
	struct ecpri_dma_eth_client_connection *connection;

	DMADBG_LOW("Begin\n");

	if (!pkts || !actual_num) {
		DMAERR("Invalid parameters\n");
		return -EINVAL;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_client_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMAERR("Connection invalid handle:%x, ENDP ID:%d\n", hdl,
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}

	ret = ecpri_dma_dp_rx_poll(connection->rx_endp_ctx, budget, pkts,
		actual_num);
	if (ret != 0) {
		DMAERR("Unable to perform Rx Poll, handle:%x, ENDP ID:%d\n", hdl,
			connection->rx_endp_ctx->endp_id);
		return -EINVAL;
	}

	DMADBG_LOW("Exit\n");

	return ret;
}

int ecpri_dma_eth_replenish_buffers(ecpri_dma_eth_conn_hdl_t hdl,
	struct ecpri_dma_pkt **pkts, u32 num_of_pkts, bool commit)
{
	int ret;
	struct ecpri_dma_eth_client_connection *connection;
	u32 num_of_pkts_remain = num_of_pkts;
	u32 num_of_pkts_to_send = num_of_pkts;

	DMADBG_LOW("Begin\n");

	if (!pkts || num_of_pkts == 0) {
		DMAERR("Invalid parameters\n");
		return -EINVAL;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_client_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMAERR("Connection invalid handle:%x, ENDP ID:%d\n", hdl,
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}

	while (num_of_pkts_remain) {
		num_of_pkts_to_send =
			num_of_pkts_remain > ECPRI_DMA_DP_MAX_DESC ?
				      ECPRI_DMA_DP_MAX_DESC : num_of_pkts_remain;

		ret = ecpri_dma_dp_transmit(connection->rx_endp_ctx, pkts,
					    num_of_pkts_to_send, commit);
		if (ret != 0) {
			DMAERR("Unable to transmit, handle:%x, ENDP ID:%d\n",
			       hdl, connection->rx_endp_ctx->endp_id);
			return -EINVAL;
		}
		pkts += num_of_pkts_to_send;
		num_of_pkts_remain -= num_of_pkts_to_send;
	}

	DMADBG_LOW("Exit\n");

	return ret;
}

int ecpri_dma_eth_query_stats(ecpri_dma_eth_conn_hdl_t hdl,
	struct ecpri_dma_connection_stats *stats)
{
	int ret = 0;
	struct ecpri_dma_eth_client_connection *connection;

	DMADBG_LOW("Begin\n");

	if (!ecpri_dma_eth_client_ctx
	    || !ecpri_dma_eth_client_ctx->is_eth_ready) {
		DMAERR("Context not initialized\n");
		return -EPERM;
	}

	if (!stats) {
		DMAERR("Invalid parameters\n");
		return -EINVAL;
	}

	/* Retrieve Connection */
	connection = ecpri_dma_eth_client_get_conn_from_hdl(hdl);
	if (!connection || !connection->valid) {
		DMAERR("Connection invalid handle:%x, ENDP ID:%d\n", hdl,
			connection->tx_endp_ctx->endp_id);
		return -EINVAL;
	}

	// TODO
	DMADBG_LOW("Exit\n");

	return ret;
}

/* API exposed structure */
const struct ecpri_dma_eth_ops ecpri_dma_eth_driver_ops = {
.ecpri_dma_eth_register = ecpri_dma_eth_register,
.ecpri_dma_eth_deregister = ecpri_dma_eth_deregister,
.ecpri_dma_eth_connect_endpoints = ecpri_dma_eth_connect_endpoints,
.ecpri_dma_eth_disconnect_endpoints =ecpri_dma_eth_disconnect_endpoints,
.ecpri_dma_eth_start_endpoints = ecpri_dma_eth_start_endpoints,
.ecpri_dma_eth_stop_endpoints = ecpri_dma_eth_stop_endpoints,
.ecpri_dma_eth_transmit = ecpri_dma_eth_transmit,
.ecpri_dma_eth_commit = ecpri_dma_eth_commit,
.ecpri_dma_eth_tx_ring_state = ecpri_dma_eth_tx_ring_state,
.ecpri_dma_eth_rx_ring_state = ecpri_dma_eth_rx_ring_state,
.ecpri_dma_eth_rx_mode_set = ecpri_dma_eth_rx_mode_set,
.ecpri_dma_eth_rx_mode_get = ecpri_dma_eth_rx_mode_get,
.ecpri_dma_eth_rx_poll = ecpri_dma_eth_rx_poll,
.ecpri_dma_eth_replenish_buffers = ecpri_dma_eth_replenish_buffers,
.ecpri_dma_eth_query_stats = ecpri_dma_eth_query_stats,
};

EXPORT_SYMBOL(ecpri_dma_eth_driver_ops);
