/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/clk.h>
#include <linux/compat.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/fs.h>
#include <linux/genalloc.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/rbtree.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/interconnect.h>
#include <linux/netdevice.h>
#include <linux/delay.h>
#include <linux/msm_gsi.h>
#include <linux/time.h>
#include <linux/hashtable.h>
#include <linux/jhash.h>
#include <linux/pci.h>
// TODO: Uncomment when SSR support is added
//#include <soc/qcom/subsystem_restart.h>
#include <linux/soc/qcom/smem.h>
#include <linux/qcom_scm.h>
#include <asm/cacheflush.h>
#include <linux/soc/qcom/smem_state.h>
#include <linux/of_irq.h>
#include <linux/ctype.h>
#include <linux/of_address.h>
#include <linux/qcom_scm.h>
#include <linux/soc/qcom/mdt_loader.h>
#include <linux/panic_notifier.h>

#include "ecpri_dma_i.h"
#include "ecpri_dma_interrupts.h"
#include "ecpri_dma_dp.h"
#include "ecpri_dma_debugfs.h"
#include "ecpri_dma_utils.h"
#include "ecpri_dma_eth_client.h"
#include "ecpri_dma_mhi_client.h"
#include "dmahal.h"
#include "ecpri_dma_reg_dump.h"

#define ECPRI_DMA_EXCEPTION_MAX_INITIAL_CREDITS (20)

int ecpri_dma_plat_drv_probe(struct platform_device *pdev_p);

static const struct of_device_id ecpri_dma_plat_drv_match[] = {
	{.compatible = "qcom,ecpri-dma", },
	{}
};

static const struct dev_pm_ops ecpri_dma_pm_ops = {
	.suspend_late = ecpri_dma_ap_suspend,
	.resume_early = ecpri_dma_ap_resume,
};

static struct platform_driver ecpri_dma_plat_drv = {
	.probe = ecpri_dma_plat_drv_probe,
	.driver = {
		.name = DRV_NAME,
		.pm = &ecpri_dma_pm_ops,
		.of_match_table = ecpri_dma_plat_drv_match,
	},
};

static struct {
	bool present[ECPRI_DMA_SMMU_CB_MAX];
	bool arm_smmu;
	bool use_64_bit_dma_mask;
	u32 ecpri_dma_base;
	u32 ecpri_dma_size;
} smmu_info;

static struct ecpri_dma_smmu_cb_ctx smmu_cb[ECPRI_DMA_SMMU_CB_MAX];
static struct ecpri_dma_plat_drv_res ecpri_dma_res = { 0, };

struct ecpri_dma_context *ecpri_dma_ctx = NULL;
/* ecpri_dma_i.h is included in ecpri_dma_client modules and ecpri_dma_ctx is
 * declared as extern in ecpri_dma_i.h. So export ecpri_dma_ctx variable
 * to be visible to ecpri_dma_client module.
*/
EXPORT_SYMBOL(ecpri_dma_ctx);

/**
 * dma_assert() - general function for assertion
 */
void ecpri_dma_assert(void)
{
	pr_err("DMA: unrecoverable error has occurred, asserting\n");
	BUG();
}
EXPORT_SYMBOL(ecpri_dma_assert);

/**
 * ecpri_dma_ap_suspend() - suspend callback for runtime_pm
 * @dev: pointer to device
 *
 * TODO: Implement
 */
int ecpri_dma_ap_suspend(struct device *dev)
{
	return 0;
}

/**
 * ecpri_dma_ap_resume() - resume callback for runtime_pm
 * @dev: pointer to device
 *
 * This callback will be invoked by the runtime_pm framework when an AP resume
 * operation is invoked.
 *
 * Always returns 0 since resume should always succeed.
 */
int ecpri_dma_ap_resume(struct device *dev)
{
	return 0;
}

/**
 * ecpri_dma_get_smmu_ctx()- Return smmu context for the given cb_type
 *
 * Return value: pointer to smmu context address
 */
struct ecpri_dma_smmu_cb_ctx *ecpri_dma_get_smmu_ctx(
	enum ecpri_dma_smmu_cb_type cb_type)
{
	return &smmu_cb[cb_type];
}

static int ecpri_dma_smmu_ap_cb_probe(struct device *dev)
{
	struct ecpri_dma_smmu_cb_ctx *cb =
		ecpri_dma_get_smmu_ctx(ECPRI_DMA_SMMU_CB_AP);
	int fast = 0;
	int bypass = 0;
	u32 iova_ap_mapping[2];

	DMADBG("AP CB PROBE dev=%px\n", dev);

	if (smmu_info.use_64_bit_dma_mask) {
		if (dma_set_mask(dev, DMA_BIT_MASK(64)) ||
			dma_set_coherent_mask(dev, DMA_BIT_MASK(64))) {
			DMAERR("DMA set 64bit mask failed\n");
			return -EOPNOTSUPP;
		}
	} else {
		if (dma_set_mask(dev, DMA_BIT_MASK(32)) ||
			dma_set_coherent_mask(dev, DMA_BIT_MASK(32))) {
			DMAERR("DMA set 32bit mask failed\n");
			return -EOPNOTSUPP;
		}
	}

	DMADBG("AP CB PROBE dev=%px retrieving IOMMU mapping\n", dev);

	cb->iommu_domain = iommu_get_domain_for_dev(dev);
	if (IS_ERR_OR_NULL(cb->iommu_domain)) {
		DMAERR("could not get iommu domain\n");
		return -EINVAL;
	}

	DMADBG("AP CB PROBE mapping retrieved\n");

	cb->is_cache_coherent = of_property_read_bool(dev->of_node,
		"dma-coherent");
	cb->dev = dev;
	cb->valid = true;

	cb->va_start = cb->va_end = cb->va_size = 0;
	if (of_property_read_u32_array(
		dev->of_node, "qcom,iommu-dma-addr-pool",
		iova_ap_mapping, 2) == 0) {
		cb->va_start = iova_ap_mapping[0];
		cb->va_size = iova_ap_mapping[1];
		cb->va_end = cb->va_start + cb->va_size;
	}

	DMADBG("AP CB PROBE dev=%px va_start=0x%x va_size=0x%x\n",
		dev, cb->va_start, cb->va_size);

	/*
	 * Prior to these calls to iommu_domain_get_attr(), these
	 * attributes were set in this function relative to dtsi values
	 * defined for this driver.  In other words, if corresponding ecpri_dma
	 * driver owned values were found in the dtsi, they were read and
	 * set here.
	 *
	 * In this new world, the developer will use iommu owned dtsi
	 * settings to set them there.  This new logic below, simply
	 * checks to see if they've been set in dtsi.  If so, the logic
	 * further below acts accordingly...
	 */
	/* TODO: Uncomment once iommu_domain_get_attr API is added for Lassen
	iommu_domain_get_attr(cb->iommu_domain, DOMAIN_ATTR_S1_BYPASS, &bypass);
	iommu_domain_get_attr(cb->iommu_domain, DOMAIN_ATTR_FAST, &fast);
	*/

	DMADBG("AP CB PROBE dev=%px DOMAIN ATTRS bypass=%d fast=%d\n",
		dev, bypass, fast);

	ecpri_dma_ctx->s1_bypass_arr[ECPRI_DMA_SMMU_CB_AP] = (bypass != 0);

	smmu_info.present[ECPRI_DMA_SMMU_CB_AP] = true;
	ecpri_dma_ctx->num_smmu_cb_probed++;

	return 0;
}

static int ecpri_dma_panic_notifier(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	if (ecpri_dma_ctx != NULL) {
		if (ecpri_dma_ctx->is_device_crashed)
			return NOTIFY_DONE;
	}

	ecpri_dma_save_registers();

	return NOTIFY_DONE;
}

static struct notifier_block ecpri_dma_panic_blk = {
	.notifier_call = ecpri_dma_panic_notifier,
	/* DMA panic handler needs to run before modem shuts down */
	.priority = INT_MAX,
};


static void ecpri_dma_register_panic_hdlr(void)
{
	atomic_notifier_chain_register(&panic_notifier_list,
		&ecpri_dma_panic_blk);
}

static void ecpri_dma_notify_dma_ready(void)
{
	struct ecpri_dma_ready_cb_wrapper *entry;
	struct ecpri_dma_ready_cb_wrapper *next;
	ecpri_hwio_def_ecpri_spare_reg_u spare_reg;

	DMADBG("Notify that DMA driver is ready\n");
	mutex_lock(&ecpri_dma_ctx->lock);
	if (!ecpri_dma_is_ready()) {
		DMAERR("Cannot notify that DMA driver is ready\n");
		mutex_unlock(&ecpri_dma_ctx->lock);
		return;
	}

	list_for_each_entry_safe(entry, next,
		&ecpri_dma_ctx->ecpri_dma_ready_cb_list, link)
	{
		if (entry->info.notify) {
			entry->info.notify(entry->info.userdata);
			DMADBG("Invoked CB function %ps address %pf\n",
				entry->info.notify, entry->info.notify);
		}
		/* remove from list once notify is done */
		list_del(&entry->link);
		kfree(entry);
	}

	/* Trigger Q6 init without QMI */
	spare_reg.value = 1;
	ecpri_dma_hal_write_reg(
		ECPRI_SPARE_REG, spare_reg.value);

	mutex_unlock(&ecpri_dma_ctx->lock);

	DMADBG("Written to SPARE_REG to trigger Q6 init\n");
	DMADBG("Finished DMA ready notify\n");
}

static void ecpri_dma_gsi_notify_cb(struct gsi_per_notify *notify)
{
	/*
	 * These values are reported by hardware. Any error indicates
	 * hardware unexpected state.
	 */
	switch (notify->evt_id) {
	case GSI_PER_EVT_GLOB_ERROR:
		DMAERR("Got GSI_PER_EVT_GLOB_ERROR\n");
		DMAERR("Err_desc = 0x%04x\n", notify->data.err_desc);
		break;
	case GSI_PER_EVT_GLOB_GP1:
		DMAERR("Got GSI_PER_EVT_GLOB_GP1\n");
		ecpri_dma_assert();
		break;
	case GSI_PER_EVT_GLOB_GP2:
		DMAERR("Got GSI_PER_EVT_GLOB_GP2\n");
		ecpri_dma_assert();
		break;
	case GSI_PER_EVT_GLOB_GP3:
		DMAERR("Got GSI_PER_EVT_GLOB_GP3\n");
		ecpri_dma_assert();
		break;
	case GSI_PER_EVT_GENERAL_BREAK_POINT:
		DMAERR("Got GSI_PER_EVT_GENERAL_BREAK_POINT\n");
		break;
	case GSI_PER_EVT_GENERAL_BUS_ERROR:
		DMAERR("Got GSI_PER_EVT_GENERAL_BUS_ERROR\n");
		ecpri_dma_assert();
		break;
	case GSI_PER_EVT_GENERAL_CMD_FIFO_OVERFLOW:
		DMAERR("Got GSI_PER_EVT_GENERAL_CMD_FIFO_OVERFLOW\n");
		ecpri_dma_assert();
		break;
	case GSI_PER_EVT_GENERAL_MCS_STACK_OVERFLOW:
		DMAERR("Got GSI_PER_EVT_GENERAL_MCS_STACK_OVERFLOW\n");
		ecpri_dma_assert();
		break;
	default:
		DMAERR("Received unexpected evt: %d\n",
			notify->evt_id);
		ecpri_dma_assert();
	}
}

/**
 * ecpri_dma_active_clks_status() - update the current msm bus clock vote
 * status
 */
int ecpri_dma_active_clks_status(void)
{
	return 1;
}

static void ecpri_dma_handle_gsi_differ_irq(void)
{
	return;
}

static void ecpri_dma_exception_replenish_work(struct work_struct *work)
{
	int ret = 0;
	struct ecpri_dma_endp_context *ep;
	struct ecpri_dma_exception_replenish_work_wrap *work_data =
		container_of(work,
			     struct ecpri_dma_exception_replenish_work_wrap,
			     replenish_work);

	ep = &ecpri_dma_ctx->endp_ctx[ecpri_dma_ctx->exception_endp];

	DMADBG("replenish %d credits to exception\n",
	       work_data->num_to_replenish);

	ret = ecpri_dma_dp_exception_replenish(ep,
					       work_data->num_to_replenish);
	if (ret) {
		DMAERR("Failed to replenish exception endp\n");
		kfree(work_data);
		return;
	}

	kfree(work_data);

	DMADBG("Exception pipe is ready, driver is ready \n");

	mutex_lock(&ecpri_dma_ctx->lock);
	ecpri_dma_ctx->dma_initialization_complete = true;
	mutex_unlock(&ecpri_dma_ctx->lock);

	ecpri_dma_notify_dma_ready();
}

static int ecpri_dma_alloc_exception_endp(void)
{
	const struct dma_gsi_ep_config *gsi_ep_cfg;
	struct ecpri_dma_endp_context *ep;
	struct ecpri_dma_exception_replenish_work_wrap *work;
	bool found_exception = false;
	int ret = 0;
	int i;

	for (i = 0; i < ECPRI_DMA_ENDP_NUM_MAX; i++) {
		if (ecpri_dma_ctx->endp_map[i].valid &&
			ecpri_dma_ctx->endp_map[i].is_exception) {
			gsi_ep_cfg = &ecpri_dma_ctx->endp_map[i];
			ep = &ecpri_dma_ctx->endp_ctx[i];
			DMADBG("Exception endp is ENDP# %d\n", i);

			ep->gsi_ep_cfg = gsi_ep_cfg;
			if (ep->valid) {
				DMAERR("EP %d already allocated.\n", i);
				goto fail_gen;
			}

			work = kzalloc(
				sizeof(struct ecpri_dma_exception_replenish_work_wrap),
				GFP_ATOMIC);
			if (!work) {
				DMAERR("failed to alloc ecpri_dma_exception_replenish_work\n");
				ret = -ENOMEM;
				goto fail_gen;
			}

			INIT_WORK(&work->replenish_work,
				  ecpri_dma_exception_replenish_work);

			INIT_LIST_HEAD(&ep->available_outstanding_pkts_list);
			INIT_LIST_HEAD(&ep->completed_pkt_list);
			INIT_LIST_HEAD(&ep->outstanding_pkt_list);
			spin_lock_init(&ep->spinlock);

			tasklet_init(&ep->tasklet, ecpri_dma_dp_tasklet_exception_notify,
				(unsigned long)ep);

			/* Configure endp GSI params */
			ep->valid = true;
			ep->endp_id = i;
			ep->ring_length = gsi_ep_cfg->dma_if_aos;
			ep->int_modt = ECPRI_DMA_EXCEPTION_ENDP_MODT;
			ep->int_modc = ECPRI_DMA_EXCEPTION_ENDP_MODC;
			ep->buff_size = ECPRI_DMA_EXCEPTION_ENDP_BUFF_SIZE;
			ep->page_order = get_order(ep->buff_size);
			ep->is_over_pcie = false;

			ep->notify_comp =
				ecpri_dma_dp_exception_endp_notify_completion;

			ret = ecpri_dma_gsi_setup_channel(ep);
			if (ret) {
				DMAERR("Failed to setup GSI channel\n");
				goto fail_gen;
			}

			ret = gsi_start_channel(ep->gsi_chan_hdl);
			if (ret != GSI_STATUS_SUCCESS) {
				DMAERR("gsi_start_channel failed res=%d ep=%d.\n", ret, i);
				goto fail_start;
			}
			DMADBG("Exception endp (ep: %d) allocated and started\n", i);

			ep->available_outstanding_pkts_cache = kmem_cache_create(
				"DMA_OUTSTANDING_PKTS_WRAPPER",
				sizeof(struct ecpri_dma_outstanding_pkt_wrapper), 0, 0, NULL);
			if (!ep->available_outstanding_pkts_cache) {
				DMAERR("DMA outstanding pkts wrapper cache create failed\n");
				ret = -ENOMEM;
				goto fail_out_cache;
			}

			ep->available_exception_pkts_cache = kmem_cache_create(
				"DMA_EXCEPTION_PKTS_WRAPPER",
				sizeof(struct ecpri_dma_pkt), 0, 0, NULL);
			if (!ep->available_exception_pkts_cache) {
				DMAERR("DMA exception pkts wrapper cache create failed\n");
				ret = -ENOMEM;
				goto fail_exception_pkt;
			}

			ep->available_exception_buffs_cache = kmem_cache_create(
				"DMA_EXCEPTION_BUFFS_WRAPPER",
				sizeof(struct ecpri_dma_mem_buffer), 0, 0, NULL);
			if (!ep->available_exception_buffs_cache) {
				DMAERR("DMA exception buffs wrapper cache create failed\n");
				ret = -ENOMEM;
				goto fail_exception_buff;
			}

			/* Fill ring with credtis */
			work->num_to_replenish =
				gsi_ep_cfg->dma_if_aos - 1 >
						ECPRI_DMA_EXCEPTION_MAX_INITIAL_CREDITS ?
					      ECPRI_DMA_EXCEPTION_MAX_INITIAL_CREDITS :
					      gsi_ep_cfg->dma_if_aos - 1;

			found_exception = true;

			/* Save exception ENDP id for easier future access */
			ecpri_dma_ctx->exception_endp = i;

			queue_work(ecpri_dma_ctx->ecpri_dma_exception_wq,
				   &work->replenish_work);
			break;
		}
	}

	if(!found_exception) {
		DMAERR("Exception endp not found\n");
		return -EINVAL;
	}

	return ret;

fail_exception_buff:
	kmem_cache_destroy(ep->available_exception_pkts_cache);
fail_exception_pkt:
	kmem_cache_destroy(ep->available_outstanding_pkts_cache);
fail_out_cache:
	gsi_stop_channel(ep->gsi_chan_hdl);
fail_start:
	gsi_dealloc_channel(ep->gsi_chan_hdl);
fail_gen:
	return ret;
}

int ecpri_dma_alloc_endp(int endp_id, u32 ring_length,
				   struct ecpri_dma_moderation_config *mod_cfg,
				   bool is_over_pcie,
				   client_notify_comp notify_comp)
{
	const struct dma_gsi_ep_config *gsi_ep_cfg;
	struct ecpri_dma_endp_context *ep;
	int ret = 0;

	if(endp_id < 0 || endp_id >= ECPRI_DMA_ENDP_NUM_MAX || ring_length == 0||
		!mod_cfg) {
		DMAERR("Invalid params\n");
		return -EINVAL;
	}

	if (!ecpri_dma_ctx->endp_map[endp_id].valid) {
		DMAERR("EP %d is not valid\n", endp_id);
		return -EINVAL;
	}

	ep = &ecpri_dma_ctx->endp_ctx[endp_id];
	if (ep->valid) {
		DMAERR("EP %d already allocated.\n", endp_id);
		return -EINVAL;
	}

	gsi_ep_cfg = &ecpri_dma_ctx->endp_map[endp_id];
	ep->gsi_ep_cfg = gsi_ep_cfg;

	INIT_LIST_HEAD(&ep->available_outstanding_pkts_list);
	INIT_LIST_HEAD(&ep->completed_pkt_list);
	INIT_LIST_HEAD(&ep->outstanding_pkt_list);
	spin_lock_init(&ep->spinlock);

	if (gsi_ep_cfg->dir == ECPRI_DMA_ENDP_DIR_SRC) {
		tasklet_init(&ep->tasklet, ecpri_dma_tasklet_transmit_done,
			(unsigned long)ep);
	} else {
		tasklet_init(&ep->tasklet, ecpri_dma_tasklet_rx_done,
			     (unsigned long)ep);
	}

	/* Configure endp GSI params */
	ep->valid = true;
	ep->endp_id = endp_id;
	ep->ring_length = ring_length;
	ep->int_modt = mod_cfg->moderation_timer_threshold;
	ep->int_modc = mod_cfg->moderation_counter_threshold;
	ep->is_over_pcie = is_over_pcie;

	ep->notify_comp = notify_comp;

	ret = ecpri_dma_gsi_setup_channel(ep);
	if (ret) {
		DMAERR("Failed to setup GSI channel\n");
		return ret;
	}
	DMADBG("ENDP %d is allocated\n", endp_id);

	ep->available_outstanding_pkts_cache = kmem_cache_create(
		"DMA_OUTSTANDING_PKTS_WRAPPER",
		sizeof(struct ecpri_dma_outstanding_pkt_wrapper), 0, 0, NULL);
	if (!ep->available_outstanding_pkts_cache) {
		DMAERR("DMA outstanding pkts wrapper cache create failed\n");
		ret = -ENOMEM;
		return ret;
	}

	return 0;
}

int ecpri_dma_reset_endp(struct ecpri_dma_endp_context *endp_cfg)
{
	int ret = 0;

	if (!endp_cfg->valid) {
		DMADBG("ENDP %d isn't valid and cannot be stopped\n", endp_cfg->endp_id);
		return -EINVAL;
	}

	ret = ecpri_dma_gsi_reset_channel(endp_cfg);
	if (ret) {
		DMADBG("Stop ENDP %d failed with code %d\n", endp_cfg->endp_id, ret);
		return ret;
	}

	return ret;
}

int ecpri_dma_stop_endp(struct ecpri_dma_endp_context *endp_cfg)
{
	int ret = 0;

	if(!endp_cfg->valid) {
		DMADBG("ENDP %d isn't valid and cannot be stopped\n", endp_cfg->endp_id);
		return -EINVAL;
	}

	ret = ecpri_dma_gsi_stop_channel(endp_cfg);
	if (ret) {
		DMADBG("Stop ENDP %d failed with code %d\n", endp_cfg->endp_id, ret);
		return ret;
	}

	return ret;
}

int ecpri_dma_start_endp(struct ecpri_dma_endp_context *endp_cfg)
{
	int ret = 0;

	if (!endp_cfg->valid) {
		DMADBG("ENDP %d isn't valid and cannot be started\n", endp_cfg->endp_id);
		return -EINVAL;
	}

	ret = ecpri_dma_gsi_start_channel(endp_cfg);
	if (ret) {
		DMADBG("Stop ENDP %d failed with code %d\n", endp_cfg->endp_id, ret);
		return ret;
	}

	return ret;
}

int ecpri_dma_dealloc_endp(struct ecpri_dma_endp_context *endp_cfg)
{
	int ret = 0;
	struct list_head* pos = NULL, *n = NULL;
	struct ecpri_dma_outstanding_pkt_wrapper* curr_pkt_wrapper = NULL;

	if (!endp_cfg->valid) {
		DMADBG("ENDP %d isn't valid and cannot be deallocated\n", endp_cfg->endp_id);
		return -EINVAL;
	}

	ret = ecpri_dma_gsi_release_channel(endp_cfg);
	if (ret) {
		DMADBG("Release ENDP %d failed with code %d\n", endp_cfg->endp_id, ret);
		return ret;
	}

	/* Free allocated entry */
	list_for_each_safe(pos, n, &endp_cfg->outstanding_pkt_list)
	{
		curr_pkt_wrapper = list_entry(pos,
			struct ecpri_dma_outstanding_pkt_wrapper,
			link);
			list_del(&curr_pkt_wrapper->link);
			kmem_cache_free(endp_cfg->available_outstanding_pkts_cache,
				curr_pkt_wrapper);
	}

	pos = NULL;
	n = NULL;
	list_for_each_safe(pos, n, &endp_cfg->completed_pkt_list)
	{
		curr_pkt_wrapper = list_entry(pos,
			struct ecpri_dma_outstanding_pkt_wrapper,
			link);
		list_del(&curr_pkt_wrapper->link);
		kmem_cache_free(endp_cfg->available_outstanding_pkts_cache,
			curr_pkt_wrapper);
	}

	pos = NULL;
	n = NULL;
	list_for_each_safe(pos, n, &endp_cfg->available_outstanding_pkts_list)
	{
		curr_pkt_wrapper = list_entry(pos,
			struct ecpri_dma_outstanding_pkt_wrapper,
			link);
		list_del(&curr_pkt_wrapper->link);
		kmem_cache_free(endp_cfg->available_outstanding_pkts_cache,
			curr_pkt_wrapper);
	}

	kmem_cache_destroy(endp_cfg->available_outstanding_pkts_cache);
	endp_cfg->curr_outstanding_num = 0;
	endp_cfg->curr_completed_num = 0;

	endp_cfg->valid = false;

	return ret;
}

/**
 * ecpri_dma_post_init() - Initialize the DMA Driver (Part II).
 * This part contains all initialization which requires interaction with
 * DMA & GSI HW.
 *
 * Initialize DMA HAL
 * TODO: Complete documentation here
 */
static int ecpri_dma_post_init(void)
{
	bool ready = false;
	int result = 0, i = 0;
	struct gsi_per_props gsi_props;

	DMADBG("post_init start\n");

	if (ecpri_dma_ctx == NULL) {
		DMADBG("eCPRI DMA driver wasn't initialized\n");
		return -ENXIO;
	}

	/* Prevent consequent calls from trying to load the FW again. */
	mutex_lock(&ecpri_dma_ctx->lock);
	ready = ecpri_dma_is_ready();
	mutex_unlock(&ecpri_dma_ctx->lock);
	if (ready)
		return 0;

	if (ecpri_dma_hal_init(ecpri_dma_ctx->ecpri_hw_ver, ecpri_dma_ctx->mmio,
		ecpri_dma_ctx->ecpri_dma_cfg_offset, ecpri_dma_ctx->pdev)) {
		DMAERR("fail to init ecpri_dma_hal\n");
		result = -EFAULT;
		goto fail_dmahal;
	}

	//TODO: Implement REG SAVE & Reg save Init here

	/* Configure DMA HW */
	result = ecpri_dma_hw_init();
	if (result) {
		DMAERR("Error initializing eCPRI DMA HW\n");
		result = -ENODEV;
		goto fail_init_hw;
	}
	DMADBG("eCPRI DMA HW initialization sequence completed");

	/* Register DMA IRQ handler*/
	result = ecpri_dma_interrupts_init(ecpri_dma_ctx->ecpri_dma_irq,
		ecpri_dma_ctx->ee, &ecpri_dma_ctx->master_pdev->dev);
	if (result) {
		DMAERR("eCPRI DMA initialization of interrupts failed\n");
		result = -ENODEV;
		goto fail_init_interrupts;
	}

	memset(&gsi_props, 0, sizeof(gsi_props));
	gsi_props.ver = ecpri_dma_ctx->gsi_ver;
	gsi_props.ee = ecpri_dma_ctx->ee;
	gsi_props.intr = GSI_INTR_IRQ;
	gsi_props.phys_addr = ecpri_dma_ctx->gsi_mem_base;
	gsi_props.size = ecpri_dma_ctx->gsi_mem_size;
	for (i = 0; i < ECPRI_DMA_NUM_EE; i++)
	{
		if (i == ECPRI_DMA_EE_Q6)
			gsi_props.irq[i] = -1;
		else
			gsi_props.irq[i] = ecpri_dma_ctx->gsi_irq[i];

		switch (i) {
		case ECPRI_DMA_EE_VM0:
		case ECPRI_DMA_EE_VM1:
		case ECPRI_DMA_EE_VM2:
		case ECPRI_DMA_EE_VM3:
			gsi_props.mhi_er_id_limits_valid[i] = true;
			break;
		case ECPRI_DMA_EE_AP:
		case ECPRI_DMA_EE_Q6:
		case ECPRI_DMA_EE_PF:
		default:
			gsi_props.mhi_er_id_limits_valid[i] = false;
			break;
		}
	}
	gsi_props.mhi_er_id_limits[0] = 0;
	gsi_props.mhi_er_id_limits[1] = ECPRI_DMA_MHI_MAX_HW_CHANNELS - 1;
	gsi_props.notify_cb = ecpri_dma_gsi_notify_cb;
	gsi_props.req_clk_cb = NULL;
	gsi_props.rel_clk_cb = NULL;
	gsi_props.clk_status_cb = ecpri_dma_active_clks_status;
	gsi_props.enable_clk_bug_on = ecpri_dma_handle_gsi_differ_irq;

	result = gsi_register_device(&gsi_props,
		&ecpri_dma_ctx->gsi_dev_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		DMAERR(":gsi register error - %d\n", result);
		result = -ENODEV;
		goto fail_init_interrupts;
	}
	DMADBG("DMA gsi is registered\n");

	/* Set ENDP mappings according to HW version and flavor */
	result = ecpri_dma_get_endp_mapping(ecpri_dma_ctx->ecpri_hw_ver,
		ecpri_dma_ctx->hw_flavor, &ecpri_dma_ctx->endp_map);
	if (result) {
		DMAERR("Failed retrieving eCPRI DMA ENDP mapping\n");
		result = -ENODEV;
		goto fail_endp_map;
	}

	result = ecpri_dma_setup_dma_endps(ecpri_dma_ctx->endp_map);
	if (result) {
		DMAERR("Failed setup of eCPRI DMA ENDPs\n");
		result = -ENODEV;
		goto fail_endp_map;
	}
	result = ecpri_dma_enable_dma_endps(ecpri_dma_ctx->endp_map);
	if (result) {
		DMAERR("Failed enabling eCPRI DMA ENDPs\n");
		result = -ENODEV;
		goto fail_endp_map;
	}

	/* Init panic handler */
	ecpri_dma_register_panic_hdlr();

#ifdef CONFIG_DEBUG_FS
	ecpri_dma_debugfs_init();
#endif

#ifdef CONFIG_ECPRI_DMA_UT
	result = ecpri_dma_ut_module_init();
	if (result) {
		DMAERR("Failed UT module init\n");
		result = -ENODEV;
		goto fail_remove_debugfs;
	}
#endif

	result = ecpri_dma_alloc_exception_endp();
	if (result) {
		DMAERR("Failed allocating and starting exception ENDPs\n");
		result = -ENODEV;
		goto fail_endp_map;
	}

	DMADBG("eCPRI DMA post init completed");

	return 0;
#ifdef CONFIG_ECPRI_DMA_UT
fail_remove_debugfs:
	ecpri_dma_debugfs_remove();
#endif
fail_endp_map:
	gsi_deregister_device(ecpri_dma_ctx->gsi_dev_hdl, false);
fail_init_interrupts:
	ecpri_dma_interrupts_destroy(ecpri_dma_ctx->ecpri_dma_irq,
				     &ecpri_dma_ctx->master_pdev->dev);
fail_init_hw:
	ecpri_dma_hal_destroy();
fail_dmahal:
	return result;
}

static int ecpri_dma_get_dts_configuration(struct platform_device *pdev,
	struct ecpri_dma_plat_drv_res *dma_drv_res)
{
	int result = 0;
	struct resource *resource;
	int i = 0;

	dma_drv_res->ecpri_hw_ver = ECPRI_HW_NONE;
	dma_drv_res->hw_flavor = ECPRI_HW_FLAVOR_NONE;
	dma_drv_res->max_num_smmu_cb = ECPRI_DMA_SMMU_CB_MAX;
	dma_drv_res->use_uefi_boot = false;
	dma_drv_res->testing_mode = false;

	/* Get eCPRI HW Version */
	result = of_property_read_u32(pdev->dev.of_node, "qcom,ecpri-hw-ver",
		&dma_drv_res->ecpri_hw_ver);
	if ((result) || (dma_drv_res->ecpri_hw_ver == ECPRI_HW_NONE)) {
		DMAERR(":get resource failed for ecpri-hw-ver\n");
		return -ENODEV;
	}
	DMADBG(":ecpri_hw_ver = %d", dma_drv_res->ecpri_hw_ver);

	if (dma_drv_res->ecpri_hw_ver >= ECPRI_HW_MAX) {
		pr_err(":ECPRI version is greater than the MAX\n");
		return -ENODEV;
	}

	/* Get DMA HW Flavor */
	result = of_property_read_u32(pdev->dev.of_node, "qcom,ecpri-hw-flavor",
		&dma_drv_res->hw_flavor);
	if ((result) || (dma_drv_res->hw_flavor == ECPRI_HW_FLAVOR_NONE)) {
		DMAERR(":get resource failed for ecpri-hw-flavor\n");
		return -ENODEV;
	}
	DMADBG(":hw_flavor = %d", dma_drv_res->hw_flavor);

	/* Get DMA wrapper address */
	result = of_property_read_u32(pdev->dev.of_node, "qcom,ecpri-dma-cfg-offset",
		&dma_drv_res->ecpri_dma_cfg_offset);
	if (!result) {
		DMADBG(":Read offset of DMA_CFG from DMA_WRAPPER_BASE = 0x%x\n",
			dma_drv_res->ecpri_dma_cfg_offset);
	} else {
		dma_drv_res->ecpri_dma_cfg_offset = 0;
		DMADBG(":DMA_CFG_OFFSET not defined. Using default one\n");
	}

	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
		"ecpri-dma-base");
	if (!resource) {
		DMAERR(":get resource failed for ecpri-dma-base!\n");
		return -ENODEV;
	}
	dma_drv_res->ecpri_dma_mem_base = resource->start;
	dma_drv_res->ecpri_dma_mem_size = resource_size(resource);
	DMADBG(":ecpri-dma-base = 0x%x, size = 0x%x\n",
		dma_drv_res->ecpri_dma_mem_base,
		dma_drv_res->ecpri_dma_mem_size);

	smmu_info.ecpri_dma_base = dma_drv_res->ecpri_dma_mem_base;
	smmu_info.ecpri_dma_size = dma_drv_res->ecpri_dma_mem_size;

	/* Get DMA GSI address */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
		"gsi-base");
	if (!resource) {
		DMAERR(":get resource failed for gsi-base\n");
		return -ENODEV;
	}
	dma_drv_res->gsi_mem_base = resource->start;
	dma_drv_res->gsi_mem_size = resource_size(resource);
	DMADBG(":gsi-base = 0x%x, size = 0x%x\n",
		dma_drv_res->gsi_mem_base,
		dma_drv_res->gsi_mem_size);

	/* Get DMA GSI IRQ numbers */
	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
		"gsi-irq-ap");
	if (!resource) {
		DMAERR(":get resource failed for gsi-irq-ap\n");
		return -ENODEV;
	}
	dma_drv_res->gsi_irq[ECPRI_DMA_EE_AP] = resource->start;
	DMADBG(":gsi-irq-ap = %d\n", dma_drv_res->gsi_irq[ECPRI_DMA_EE_AP]);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
		"gsi-irq-vm0");
	if (!resource) {
		DMAERR(":get resource failed for gsi-irq-vm0\n");
		return -ENODEV;
	}
	dma_drv_res->gsi_irq[ECPRI_DMA_EE_VM0] = resource->start;
	DMADBG(":gsi-irq-vm0 = %d\n", dma_drv_res->gsi_irq[ECPRI_DMA_EE_VM0]);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
		"gsi-irq-vm1");
	if (!resource) {
		DMAERR(":get resource failed for gsi-irq-vm1\n");
		return -ENODEV;
	}
	dma_drv_res->gsi_irq[ECPRI_DMA_EE_VM1] = resource->start;
	DMADBG(":gsi-irq-vm1 = %d\n", dma_drv_res->gsi_irq[ECPRI_DMA_EE_VM1]);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
		"gsi-irq-vm2");
	if (!resource) {
		DMAERR(":get resource failed for gsi-irq-vm2\n");
		return -ENODEV;
	}
	dma_drv_res->gsi_irq[ECPRI_DMA_EE_VM2] = resource->start;
	DMADBG(":gsi-irq-vm2 = %d\n", dma_drv_res->gsi_irq[ECPRI_DMA_EE_VM2]);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
		"gsi-irq-vm3");
	if (!resource) {
		DMAERR(":get resource failed for gsi-irq-vm3\n");
		return -ENODEV;
	}
	dma_drv_res->gsi_irq[ECPRI_DMA_EE_VM3] = resource->start;
	DMADBG(":gsi-irq-vm3 = %d\n", dma_drv_res->gsi_irq[ECPRI_DMA_EE_VM3]);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
		"gsi-irq-pf");
	if (!resource) {
		DMAERR(":get resource failed for gsi-irq-pf\n");
		return -ENODEV;
	}
	dma_drv_res->gsi_irq[ECPRI_DMA_EE_PF] = resource->start;
	DMADBG(":gsi-irq-pf = %d\n", dma_drv_res->gsi_irq[ECPRI_DMA_EE_PF]);

	/* Get DMA IRQ number */
	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
		"ecpri-dma-irq");
	if (!resource) {
		DMAERR(":get resource failed for dma-irq\n");
		return -ENODEV;
	}
	dma_drv_res->ecpri_dma_irq = resource->start;
	DMADBG(":ecpri-dma-irq = %d\n", dma_drv_res->ecpri_dma_irq);

	/* Get EE number */
	result = of_property_read_u32(pdev->dev.of_node, "qcom,ee",
		&dma_drv_res->ee);
	if (result)
		dma_drv_res->ee = 0;
	DMADBG(":ee = %u\n", dma_drv_res->ee);

	/* Get using UEFI boot */
	dma_drv_res->use_uefi_boot =
		of_property_read_bool(pdev->dev.of_node,
			"qcom,use-uefi-boot");
	DMADBG(":Is UEFI loading used ? (%s)\n",
		dma_drv_res->use_uefi_boot
		? "Yes" : "No");

	/* Get test mode */
	dma_drv_res->testing_mode =
		of_property_read_bool(pdev->dev.of_node,
			"qcom,testing-mode");
	DMADBG(":Is in testing mode ? (%s)\n",
		dma_drv_res->testing_mode
		? "Yes" : "No");

	/* Get max number of SMMU context banks number */
	result = of_property_read_u32(pdev->dev.of_node,
		"qcom,max_num_smmu_cb",
		&dma_drv_res->max_num_smmu_cb);
	if (result)
		DMADBG(":using default max number of cb = %d\n",
			dma_drv_res->max_num_smmu_cb);
	else
		DMADBG(":found dma_drv_res->max_num_smmu_cb = %d\n",
			dma_drv_res->max_num_smmu_cb);

	/* Get SMMU configs*/
		/* Init SMMUs in bypass mode */
	for (i = 0; i < ECPRI_DMA_SMMU_CB_MAX; i++)
		ecpri_dma_ctx->s1_bypass_arr[i] = true;

	if (of_property_read_bool(pdev->dev.of_node, "qcom,arm-smmu")) {
		if (of_property_read_bool(pdev->dev.of_node,
			"qcom,use-64-bit-dma-mask"))
			smmu_info.use_64_bit_dma_mask = true;
		smmu_info.arm_smmu = true;
		result = ecpri_dma_smmu_ap_cb_probe(&pdev->dev);
		if (result)
			DMADBG(": Error parsing SMMU details\n");
		else
			DMADBG(": parsed SMMU details\n");
	} else {
		if (of_property_read_bool(pdev->dev.of_node,
			"qcom,use-64-bit-dma-mask")) {
			if (dma_set_mask(&pdev->dev, DMA_BIT_MASK(64)) ||
				dma_set_coherent_mask(&pdev->dev,
					DMA_BIT_MASK(64))) {
				DMAERR(":DMA set 64bit mask failed\n");
				return -EOPNOTSUPP;
			}
		} else {
			if (dma_set_mask(&pdev->dev, DMA_BIT_MASK(32)) ||
				dma_set_coherent_mask(&pdev->dev,
					DMA_BIT_MASK(32))) {
				DMAERR(":DMA set 32bit mask failed\n");
				return -EOPNOTSUPP;
			}
		}
	}

	return 0;
}

static enum gsi_ver ecpri_dma_get_gsi_ver(enum ecpri_hw_ver hw_ver)
{
	enum gsi_ver gsi_ver;

	switch (hw_ver) {
	case ECPRI_HW_V1_0:
		gsi_ver = GSI_VER_3_0;
		break;
	default:
		DMAERR("No GSI version for ecpri_dma type %d\n", hw_ver);
		WARN_ON(1);
		gsi_ver = GSI_VER_ERR;
	}

	DMADBG("GSI version %d\n", gsi_ver);

	return gsi_ver;
}

/**
 * ecpri_dma_pre_init() - Initialize the eCPRI DMA Driver.
 * This part contains all initialization which doesn't require DMA HW, such
 * as structure allocations and initializations, register writes, etc.
 *
 * @resource_p:	contain platform specific values from DTS file
 * @pdev:	The platform device structure representing the DMA driver
 *
 * Function initialization process:
 * Allocate memory for the driver context data struct
 * Initializing the ecpri_dma_ctx with :
 *    1)parsed values from the dts file
 *    2)parameters passed to the module initialization
 *    3)read HW values(such as core memory size)
 *    4)Initialize IPC logs
 * Map GSI core registers to CPU memory
 * Map DMA core registers to CPU memory
 * Vote for clocks
 * Initialize DMA Ready CB list
 */
static int ecpri_dma_pre_init(const struct ecpri_dma_plat_drv_res *resource_p,
	struct platform_device *dma_pdev)
{
	int result = 0;
	u32 i = 0;

	pr_info("DMA Driver initialization started\n");
	if (!ecpri_dma_ctx) {
		result = -ENOMEM;
		goto fail_mem_ctx;
	}
	memset(ecpri_dma_ctx->endp_ctx, 0, sizeof(ecpri_dma_ctx->endp_ctx));

	/* Init IPC logs */
	ecpri_dma_ctx->logbuf = ipc_log_context_create(ECPRI_DMA_IPC_LOG_PAGES,
		"ecpri_dma", 0);
	if (ecpri_dma_ctx->logbuf == NULL)
		DMADBG("failed to create IPC log, continue...\n");

	ecpri_dma_ctx->logbuf_low = ipc_log_context_create(ECPRI_DMA_IPC_LOG_PAGES,
		"ecpri_dma_low", 0);
	if (ecpri_dma_ctx->logbuf_low == NULL)
		DMADBG("failed to create IPC log, continue...\n");

	/* Set master pdev and pdev*/
	ecpri_dma_ctx->master_pdev = dma_pdev;
	ecpri_dma_ctx->pdev = &dma_pdev->dev;

	/* Init driver configurations according to DTSi values*/
	ecpri_dma_ctx->ecpri_hw_ver = resource_p->ecpri_hw_ver;
	ecpri_dma_ctx->hw_flavor = resource_p->hw_flavor;
	ecpri_dma_ctx->ecpri_dma_mem_base = resource_p->ecpri_dma_mem_base;
	ecpri_dma_ctx->ecpri_dma_mem_size = resource_p->ecpri_dma_mem_size;
	ecpri_dma_ctx->gsi_mem_base = resource_p->gsi_mem_base;
	ecpri_dma_ctx->gsi_mem_size = resource_p->gsi_mem_size;
	ecpri_dma_ctx->ecpri_dma_irq = resource_p->ecpri_dma_irq;
	for (i = 0; i < ECPRI_DMA_NUM_EE; i++)
		ecpri_dma_ctx->gsi_irq[i] = resource_p->gsi_irq[i];
	ecpri_dma_ctx->ee = resource_p->ee;
	ecpri_dma_ctx->max_num_smmu_cb = resource_p->max_num_smmu_cb;
	ecpri_dma_ctx->ecpri_dma_cfg_offset = resource_p->ecpri_dma_cfg_offset;
	ecpri_dma_ctx->use_uefi_boot = resource_p->use_uefi_boot;
	ecpri_dma_ctx->testing_mode = resource_p->testing_mode;
	ecpri_dma_ctx->ecpri_dma_num_endps = ECPRI_DMA_ENDP_NUM_MAX;

	/* setup DMA register access */
	DMADBG("Mapping 0x%x\n", resource_p->ecpri_dma_mem_base +
		ecpri_dma_ctx->ecpri_dma_cfg_offset);
	ecpri_dma_ctx->mmio = ioremap(resource_p->ecpri_dma_mem_base +
		ecpri_dma_ctx->ecpri_dma_cfg_offset,
		resource_p->ecpri_dma_mem_size);
	if (!ecpri_dma_ctx->mmio) {
		DMAERR(":ecpri_dma-base ioremap err\n");
		result = -EFAULT;
		goto fail_remap;
	}
	DMADBG(
		"base(0x%x)+offset(0x%x)=(0x%x) mapped to (%px) with len (0x%x)\n",
		resource_p->ecpri_dma_mem_base,
		ecpri_dma_ctx->ecpri_dma_cfg_offset,
		resource_p->ecpri_dma_mem_base + ecpri_dma_ctx->ecpri_dma_cfg_offset,
		ecpri_dma_ctx->mmio,
		resource_p->ecpri_dma_mem_size);

	/* setup GSI register access */
	if (gsi_map_base(ecpri_dma_ctx->gsi_mem_base, ecpri_dma_ctx->gsi_mem_size,
			 ecpri_dma_get_gsi_ver(ecpri_dma_ctx->ecpri_hw_ver)) != 0) {
		DMAERR("Allocation of gsi base failed\n");
		result = -EFAULT;
		goto fail_remap;
	}

	/* Get GSI version */
	ecpri_dma_ctx->gsi_ver =
		ecpri_dma_get_gsi_ver(ecpri_dma_ctx->ecpri_hw_ver);

	/* Init exception replenish WQ*/
	ecpri_dma_ctx->ecpri_dma_exception_wq = create_singlethread_workqueue(
		"ecpri_dma_exception_wq");
	if (!ecpri_dma_ctx->ecpri_dma_exception_wq) {
		DMAERR("workqueue creation failed\n");
		return -ENOMEM;
	}

	DMADBG("pre_init complete\n");

	return 0;

fail_remap:
	if(ecpri_dma_ctx->logbuf)
		ipc_log_context_destroy(ecpri_dma_ctx->logbuf);
	if (ecpri_dma_ctx->logbuf_low)
		ipc_log_context_destroy(ecpri_dma_ctx->logbuf_low);
fail_mem_ctx:
	return result;
}

int ecpri_dma_plat_drv_probe(struct platform_device *pdev_p)
{
	int result;
	struct device *dev = &pdev_p->dev;

	/*
	 * eCPRI DMA probe function can be called for multiple times as the
	 * same probe function handles multiple compatibilities
	 */
	//pr_err
	pr_info("ecpri_dma: DMA driver probing started for %s\n",
		pdev_p->dev.of_node->name);

	if (ecpri_dma_ctx == NULL) {
		DMAERR("ecpri_dma_ctx was not initialized\n");
		return -EPROBE_DEFER;
	}

	if (ecpri_dma_ctx->ecpri_hw_ver == 0) {

		/* Get eCPRI HW Version */
		result = of_property_read_u32(pdev_p->dev.of_node,
			"qcom,ecpri-hw-ver", &ecpri_dma_ctx->ecpri_hw_ver);
		if ((result) || (ecpri_dma_ctx->ecpri_hw_ver == ECPRI_HW_NONE)) {
			pr_err("ecpri_dma: get resource failed for ecpri-hw-ver!\n");
			return -ENODEV;
		}
		pr_err("ecpri_dma: ecpri_hw_ver = %d\n", ecpri_dma_ctx->ecpri_hw_ver);
	}

	if (ecpri_dma_ctx->ecpri_hw_ver >= ECPRI_HW_MAX) {
		pr_err(":ECPRI version is greater than the MAX\n");
		return -ENODEV;
	}

	DMADBG("eCPRI DMA driver probing started\n");
	DMADBG("dev->of_node->name = %s\n", dev->of_node->name);

	result = ecpri_dma_get_dts_configuration(pdev_p, &ecpri_dma_res);
	if (result) {
		DMAERR("DMA dts parsing failed\n");
		return result;
	}

	/* Proceed to real initialization */
	result = ecpri_dma_pre_init(&ecpri_dma_res, pdev_p);
	if (result) {
		DMAERR("ecpri_dma_pre_init failed\n");
		return result;
	}

	result = of_platform_populate(pdev_p->dev.of_node,
		ecpri_dma_plat_drv_match, NULL, &pdev_p->dev);
	if (result) {
		DMAERR("failed to populate platform\n");
		return result;
	}

	result = ecpri_dma_post_init();
	if (result) {
		DMAERR("DMA post init failed %d\n", result);
		return result;
	}

	if (result && result != -EPROBE_DEFER)
		DMAERR("ecpri_dma: ecpri_dma_plat_drv_probe failed\n");

	return result;
}

int ecpri_dma_register_ready_cb(void (*ecpri_dma_ready_cb)(void *user_data),
	void *user_data)
{
	struct ecpri_dma_ready_cb_wrapper *cb_wrapper;

	if (!ecpri_dma_ctx) {
		DMADBG("Can't register ready CB yet\n");
		return -EPERM;
	}

	mutex_lock(&ecpri_dma_ctx->lock);
	if (ecpri_dma_is_ready()) {
		DMADBG("eCPRI DMA driver finished initialization already\n");
		mutex_unlock(&ecpri_dma_ctx->lock);
		return -EEXIST;
	}

	cb_wrapper =
		kmalloc(sizeof(struct ecpri_dma_ready_cb_wrapper), GFP_KERNEL);
	if (!cb_wrapper) {
		mutex_unlock(&ecpri_dma_ctx->lock);
		return -ENOMEM;
	}

	cb_wrapper->info.notify = ecpri_dma_ready_cb;
	cb_wrapper->info.userdata = user_data;

	list_add_tail(&cb_wrapper->link, &ecpri_dma_ctx->ecpri_dma_ready_cb_list);
	mutex_unlock(&ecpri_dma_ctx->lock);

	return 0;
}

static int __init ecpri_dma_module_init(void)
{
	pr_info("eCPRI DMA module init\n");

	ecpri_dma_ctx = kzalloc(sizeof(*ecpri_dma_ctx), GFP_KERNEL);
	if (!ecpri_dma_ctx) {
		return -ENOMEM;
	}
	mutex_init(&ecpri_dma_ctx->lock);

	/* Init ready CB list */
	INIT_LIST_HEAD(&ecpri_dma_ctx->ecpri_dma_ready_cb_list);

	/* Register as a platform device driver */
	return platform_driver_register(&ecpri_dma_plat_drv);
}
subsys_initcall(ecpri_dma_module_init);

static void __exit ecpri_dma_module_exit(void)
{
	platform_driver_unregister(&ecpri_dma_plat_drv);
	kfree(ecpri_dma_ctx);
	ecpri_dma_ctx = NULL;
}
module_exit(ecpri_dma_module_exit);


//TODO: define dependencies
//MODULE_SOFTDEP("pre: subsys-pil-tz");
//MODULE_SOFTDEP("pre: qcom-arm-smmu-mod");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("eCPRI DMA HW device driver");
