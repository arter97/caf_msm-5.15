/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ipc_logging.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_dma.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pci.h>
#include <linux/scatterlist.h>
#include <linux/sched_clock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/msm_ep_pcie.h>
#include <linux/platform_device.h>

#include "../dmaengine.h"

/* global logging macros */
#define HDMA_LOG(h_dev, fmt, ...) do { \
	if (h_dev->klog_lvl != LOG_LVL_MASK_ALL) \
		dev_dbg(h_dev->dev, "[I] %s: %s: " fmt, h_dev->label, \
			__func__,  ##__VA_ARGS__); \
	if (h_dev->ipc_log && h_dev->ipc_log_lvl != LOG_LVL_MASK_ALL) \
		ipc_log_string(h_dev->ipc_log, \
			"[I] %s: %s: " fmt, h_dev->label, __func__, \
			##__VA_ARGS__); \
	} while (0)

#define HDMA_ERR(h_dev, fmt, ...) do { \
	if (h_dev->klog_lvl <= LOG_LVL_ERROR) \
		dev_err(h_dev->dev, "[E] %s: %s: " fmt, h_dev->label, \
			__func__, ##__VA_ARGS__); \
	if (h_dev->ipc_log && h_dev->ipc_log_lvl <= LOG_LVL_ERROR) \
		ipc_log_string(h_dev->ipc_log, \
			"[E] %s: %s: " fmt, h_dev->label, __func__, \
			##__VA_ARGS__); \
	} while (0)

#define HDMA_ASSERT(cond, msg) do { \
	if (cond) \
		panic(msg); \
	} while (0)

/* hdmac specific logging macros */
#define HDMAC_INFO(hc_dev, hv_ch, fmt, ...) do { \
	if (hc_dev->klog_lvl <= LOG_LVL_INFO) \
		pr_info("[I] %s: %s: %u: %u: %s: " fmt, hc_dev->label, \
			TO_HDMA_DIR_CH_STR(hc_dev->dir), hc_dev->ch_id, hv_ch, \
			__func__, ##__VA_ARGS__); \
	if (hc_dev->ipc_log && hc_dev->ipc_log_lvl <= LOG_LVL_INFO) \
		ipc_log_string(hc_dev->ipc_log, \
			       "[I] %s: EC_%u: EV_%u: %s: " fmt, \
				TO_HDMA_DIR_CH_STR(hc_dev->dir), \
				hc_dev->ch_id, hv_ch, __func__, \
				##__VA_ARGS__); \
	} while (0)

#define HDMAC_ERR(hc_dev, hv_ch, fmt, ...) do { \
	if (hc_dev->klog_lvl <= LOG_LVL_ERROR) \
		pr_err("[E] %s: %s: %u: %u: %s: " fmt, hc_dev->label, \
			TO_HDMA_DIR_CH_STR(hc_dev->dir), hc_dev->ch_id, hv_ch, \
			__func__, ##__VA_ARGS__); \
	if (hc_dev->ipc_log && hc_dev->ipc_log_lvl <= LOG_LVL_ERROR) \
		ipc_log_string(hc_dev->ipc_log, \
			       "[E] %s: EC_%u: EV_%u: %s: " fmt, \
				TO_HDMA_DIR_CH_STR(hc_dev->dir), \
				hc_dev->ch_id, hv_ch, __func__, \
				##__VA_ARGS__); \
	} while (0)

enum debug_log_lvl {
	LOG_LVL_VERBOSE,
	LOG_LVL_INFO,
	LOG_LVL_ERROR,
	LOG_LVL_MASK_ALL,
};

#define HDMA_DRV_NAME "hdma"
#define DEFAULT_KLOG_LVL (LOG_LVL_ERROR)

static struct hdma_dev *h_dev_info;

#ifdef CONFIG_QCOM_PCI_HDMA_DEBUG
#define DEFAULT_IPC_LOG_LVL (LOG_LVL_VERBOSE)
#define IPC_LOG_PAGES (40)

#define HDMA_IRQ(h_dev, hc_ch, fmt, ...) do { \
	if (h_dev->klog_lvl != LOG_LVL_MASK_ALL) \
		dev_dbg(h_dev->dev, "[IRQ] %s: HC_%u: %s: " fmt, h_dev->label, \
			hc_ch, __func__, ##__VA_ARGS__); \
	if (h_dev->ipc_log_irq && h_dev->ipc_log_lvl != LOG_LVL_MASK_ALL) \
		ipc_log_string(h_dev->ipc_log_irq, \
			"[IRQ] %s: HC_%u: %s: " fmt, h_dev->label, hc_ch, \
			__func__, ##__VA_ARGS__); \
	} while (0)

#define HDMAC_VERB(hc_dev, hv_ch, fmt, ...) do { \
	if (hc_dev->klog_lvl <= LOG_LVL_VERBOSE) \
		pr_info("[V] %s: %s: %u: %u: %s: " fmt, hc_dev->label, \
			TO_HDMA_DIR_CH_STR(hc_dev->dir), hc_dev->ch_id, hv_ch, \
			__func__, ##__VA_ARGS__); \
	if (hc_dev->ipc_log && hc_dev->ipc_log_lvl <= LOG_LVL_VERBOSE) \
		ipc_log_string(hc_dev->ipc_log, \
			       "[V] %s: HC_%u: HV_%u: %s: " fmt, \
				TO_HDMA_DIR_CH_STR(hc_dev->dir), \
				hc_dev->ch_id, hv_ch, __func__, \
				##__VA_ARGS__); \
	} while (0)

#else
#define IPC_LOG_PAGES (2)
#define DEFAULT_IPC_LOG_LVL (LOG_LVL_ERROR)
#define HDMA_IRQ(h_dev, hc_ch, fmt, ...)
#define HDMAC_REG(hc_dev, hv_ch, fmt, ...)
#define HDMAC_VERB(hc_dev, hv_ch, fmt, ...)
#endif


#define WR_CH_BASE (0x0)
#define RD_CH_BASE (0x100)
#define DMA_CH_BASE(d, n) ((d == HDMA_WR_CH ? WR_CH_BASE : RD_CH_BASE) +\
				(n * 0x200))

#define DMA_LLP_LOW_OFF_DIR_CH_N(d, n) (DMA_CH_BASE(d, n) + 0x10)
#define DMA_LLP_HIGH_OFF_DIR_CH_N(d, n) (DMA_CH_BASE(d, n) + 0x14)


/* HDMA Interrupt Registers */
#define DMA_INT_SETUP_OFF(d, n) (DMA_CH_BASE(d, n) + 0x88)
#define DMA_INT_STATUS_OFF(d, n) (DMA_CH_BASE(d, n) + 0x84)
#define DMA_INT_CLEAR_OFF(d, n) (DMA_CH_BASE(d, n) + 0x8c)
#define DMA_STATUS_OFF(d, n) (DMA_CH_BASE(d, n) + 0x80)

/* HDMA Channel Context Registers */
#define DMA_CONTROL1_OFF(d, n) (DMA_CH_BASE(d, n) + 0x34) /* Linked List Enable */
#define DMA_CYCLE_OFF(d, n) (DMA_CH_BASE(d, n) + 0x18) /* 0:Consumer Cycle Status 1:Cycle bit */
#define DMA_DOORBELL_OFF_DIR_CH_N(d, n) (DMA_CH_BASE(d, n) + 0x04) /* DB Offset Register */
#define DMA_CH_STATUS_OFF(d, n) (DMA_CH_BASE(d, n) + 0x80) /* Channel Status Register */
#define DMA_CH_EN_OFF(d, n) (DMA_CH_BASE(d, n)) /* Channel Enable Register */
#define DMA_CH_EN BIT(0)

#define DMA_NUM_CHAN_OFF (0x438)
#define DMA_NUM_CH_MASK (0x3ff)
#define DMA_NUM_WR_CH_SHIFT (0)
#define DMA_NUM_RD_CH_SHIFT (16)

#define HDMA_LABEL_SIZE (256)
#define HDMA_DESC_LIST_SIZE (256)
#define HDMA_NUM_MAX_HV_CH (32)
#define HDMA_NUM_TL_INIT (2)
#define HDMA_NUM_TL_ELE (1024)

#define HDMA_CH_CONTROL1_LLE BIT(0) /* Linked List Enable */
#define HDMA_CH_CYCLE_CCS BIT(1) /* Consumer Cycle Status */
#define HDMA_ELE_LWIE BIT(3) /* Local Watermark Interrupt Enable */
#define HDMA_ELE_LLP BIT(2) /* Load Link Pointer */
#define HDMA_ELE_CYCLE_CB BIT(0) /* Cycle bit */


#define HDMA_LOCAL_ABORT_INT_BIT BIT(6)
#define HDMA_LOCAL_STOP_INT_BIT BIT(4)
#define HDMA_WATERMARK_STATUS_BIT BIT(1)
#define HDMA_WATERMARK_MASK_BIT BIT(1)
#define HDMA_INT_ERR_MASK (0x0004)
#define HDMA_HW_STATUS_MASK (BIT(0) | BIT(1))

#define HDMA_MAX_SIZE 0x2000

#define REQ_OF_DMA_ARGS (2) /* # of arguments required from client */

/*
 * HDMAV CH ID = HDMA CH ID * HDMAV_BASE_CH_ID + HDMAV index
 *
 * Virtual channel is assigned a channel ID based on the physical
 * channel is it assigned to.
 * ex:
 *	physical channel 0: virtual base = 0
 *	physical channel 1: virtual base = 100
 *	physical channel 2: virtual base = 200
 */
#define HDMAV_BASE_CH_ID (100)
#define HDMAV_NO_CH_ID (99) /* RESERVED CH ID for no virtual channel */

enum hdma_dir_ch {
	HDMA_WR_CH,
	HDMA_RD_CH,
	HDMA_DIR_CH_MAX,
};

static const char *const hdma_dir_ch_str[HDMA_DIR_CH_MAX] = {
	[HDMA_WR_CH] = "WR",
	[HDMA_RD_CH] = "RD",
};

#define TO_HDMA_DIR_CH_STR(dir) (dir < HDMA_DIR_CH_MAX ? \
				hdma_dir_ch_str[dir] : "INVALID")

enum hdma_hw_state {
	HDMA_HW_STATE_INIT,
	HDMA_HW_STATE_ACTIVE,
	HDMA_HW_STATE_HALTED,
	HDMA_HW_STATE_STOPPED,
	HDMA_HW_STATE_MAX,
};

static const char *const hdma_hw_state_str[HDMA_HW_STATE_MAX] = {
	[HDMA_HW_STATE_INIT] = "INIT",
	[HDMA_HW_STATE_ACTIVE] = "ACTIVE",
	[HDMA_HW_STATE_HALTED] = "HALTED",
	[HDMA_HW_STATE_STOPPED] = "STOPPED",
};

#define TO_HDMA_HW_STATE_STR(state) (state < HDMA_HW_STATE_MAX ? \
					hdma_hw_state_str[state] : "INVALID")

/* Transfer list Data Element */
struct data_element {
	/* Channel Control */
	u32 ch_ctrl;
	/* Transfer Size */
	u32 size;
	/* Source Address Register */
	u64 sar;
	/* Destination Address Register */
	u64 dar;
};

/* Transfer list Link Element */
struct link_element {
	/* Channel Control */
	u32 ch_ctrl;
	u32 reserved0;
	/* points to new Transfer List */
	u64 lle_ptr;
	u32 reserved1;
	u32 reserved2;
};

union hdma_element {
	struct data_element de;
	struct link_element le;
};

/* Main structure for HDMA driver */
struct hdma_dev {
	struct	list_head node;
	struct	dma_device dma_device;
	struct	device_node *of_node;
	struct	device *dev;
	/* array of wr channels */
	struct	hdmac_dev *hc_wr_devs;
	/* array of rd channels */
	struct	hdmac_dev *hc_rd_devs;
	/*array of hdma lanes */
	struct	hdmac_lane *hc_lanes;
	/* array of virtual channels */
	struct	hdmav_dev *hv_devs;
	/* max physical channels */
	u32	n_max_hc_ch;
	/* max virtual channels */
	u32	n_max_hv_ch;
	u32	n_wr_ch;
	u32	n_rd_ch;
	/* index for wr round robin allocation */
	u32	cur_wr_ch_idx;
	/* index for rd round robin allocation */
	u32	cur_rd_ch_idx;
	/* index for next available HDMA virtual chan */
	u32	cur_hv_ch_idx;
	/* # of transfer list for each channel init */
	u32	n_tl_init;
	/* (# of de + le) */
	u32	n_tl_ele;
	phys_addr_t base_phys;
	size_t	base_size;
	void	__iomem *base;
	phys_addr_t dbi_phys;
	size_t	dbi_size;
	void	__iomem *dbi;
	char	label[HDMA_LABEL_SIZE];
	void	*ipc_log;
	void	*ipc_log_irq;
	enum	debug_log_lvl ipc_log_lvl;
	enum	debug_log_lvl klog_lvl;
};

/* HDMA physical channels */
struct hdmac_dev {
	struct	list_head hv_list;
	struct	hdma_dev *h_dev;
	u32	n_hv;
	u32	ch_id;
	int irq;
	spinlock_t hdma_lock;
	enum	hdma_dir_ch dir;
	/* address of next DE to process */
	dma_addr_t tl_dma_rd_p;
	/* next available DE in TL */
	struct	data_element *tl_wr_p;
	struct	data_element *tl_rd_p;
	/* link element of newest TL */
	struct	link_element *le_p;
	/* next available desc in DL */
	struct	hdma_desc **dl_wr_p;
	struct	hdma_desc **dl_rd_p;
	/* last desc of newest desc list */
	struct	hdma_desc **ldl_p;
	/* # of available DE in current TL (excludes LE) */
	u32	n_de_avail;
	/* processing tasklet */
	struct workqueue_struct		*pending_process_wq;
	struct work_struct		pending_work;
	char	label[HDMA_LABEL_SIZE];
	void	*ipc_log;
	enum	debug_log_lvl ipc_log_lvl;
	enum	debug_log_lvl klog_lvl;
	enum	hdma_hw_state hw_state;

	void	__iomem *int_setup_reg;
	void	__iomem *int_status_reg;
	void	__iomem *int_clear_reg;
	void	__iomem *ch_ctrl1_reg;
	void	__iomem *ch_cycle_reg;
	void	__iomem	*ch_en_reg;
	void	__iomem	*ch_status_reg;
	void	__iomem	*llp_low_reg;
	void	__iomem *llp_high_reg;
	void	__iomem *db_reg;
	int	is_channel_db_rang;
};

/* HDMA Channel Lane(TX and RX) */
struct hdmac_lane {
	struct hdmac_dev *wr;
	struct hdmac_dev *rd;
};

/* HDMA virtual channels */
struct hdmav_dev {
	struct	list_head node;
	struct	hdmac_dev *hc_dev;
	struct	list_head dl;
	struct	dma_chan dma_ch;
	enum	hdma_dir_ch dir;
	u32	ch_id;
	u32	priority;
	u32	n_de;
	u32	outstanding;
};

/* HDMA descriptor and last descriptor */
struct hdma_desc {
	struct	list_head node;
	struct	hdmav_dev *hv_dev;
	struct	dma_async_tx_descriptor tx;
	struct	data_element *de;

	/* Below are for LDESC (last desc of the list) */

	/* start of this transfer list (physical) */
	dma_addr_t tl_dma;
	/* start of this transfer list (virtual) */
	union	hdma_element *tl;
	/* start of this desc list */
	struct	hdma_desc **dl;
	/* next transfer list (physical) */
	dma_addr_t tl_dma_next;
	/* next transfer list (virtual) */
	union	hdma_element *tl_next;
	/* next desc list */
	struct	hdma_desc **dl_next;
};

static void hdma_issue_descriptor(struct hdmac_dev *hc_dev,
			struct hdma_desc *desc);

static int hdma_init_irq(struct hdmac_lane *hc_lane);

static void hdma_set_clear(void __iomem *addr, u32 set,
				u32 clear)
{
	u32 val;

	val = (readl_relaxed(addr) & ~clear) | set;
	writel_relaxed(val, addr);

	/* ensure register write goes through before next register operation */
	wmb();
}

static enum hdma_hw_state hdma_get_hw_state(struct hdmac_dev *hc_dev)
{
	u32 val;

	val = readl_relaxed(hc_dev->ch_status_reg);
	val &= HDMA_HW_STATUS_MASK;

	return val;
}

static struct hdmav_dev *to_hdmav_dev(struct dma_chan *dma_ch)
{
	return container_of(dma_ch, struct hdmav_dev, dma_ch);
}

static dma_cookie_t hdma_submit(struct dma_async_tx_descriptor *desc)
{
	dma_cookie_t cookie = {0};
	struct hdmav_dev *hv_dev = to_hdmav_dev(desc->chan);
	struct hdmac_dev *hc_dev = hv_dev->hc_dev;
	struct hdma_desc *hdma_desc = container_of(desc, struct hdma_desc, tx);

	spin_lock(&hc_dev->hdma_lock);
	HDMAC_VERB(hv_dev->hc_dev, hv_dev->ch_id, "enter\n");

	/* insert the descriptor to client descriptor list */
	list_add_tail(&hdma_desc->node, &hv_dev->dl);

	HDMAC_VERB(hv_dev->hc_dev, hv_dev->ch_id, "exit\n");
	spin_unlock(&hc_dev->hdma_lock);
	return cookie;
}

static void hdma_submit_pending_transfers(struct hdmav_dev *hv_dev)
{
	struct hdmac_dev *hc_dev = hv_dev->hc_dev;
	struct hdma_desc *desc;
	enum hdma_hw_state state;

	if (unlikely(list_empty(&hv_dev->dl))) {
		HDMAC_VERB(hc_dev, hv_dev->ch_id, "No descriptor to issue\n");
		return;
	}

	/* Skip queuing descriptors to controller in case DB is already rang */
	if (hc_dev->is_channel_db_rang == true)
		return;

	/*
	 * Skip submitting new elements to linked list,
	 * if the channel is in Active state
	 */
	state = hdma_get_hw_state(hc_dev);
	if (state == HDMA_HW_STATE_ACTIVE) {
		HDMAC_VERB(hc_dev, hv_dev->ch_id, "Channel is in active state\n");
		return;
	}

	list_for_each_entry(desc, &hv_dev->dl, node)
		hdma_issue_descriptor(hc_dev, desc);
	list_del_init(&hv_dev->dl);

	/* To start the hdma transaction, ring channel doorbell */
	HDMAC_VERB(hc_dev, HDMAV_NO_CH_ID, "ringing doorbell\n");
	writel_relaxed(BIT(0), hc_dev->db_reg);
	/* Ensure DB register is properly updated */
	mb();
}

static void hdmac_process_workqueue(struct work_struct *work)
{

	struct hdmac_dev *hc_dev = container_of(work, struct hdmac_dev, pending_work);
	struct hdma_dev *h_dev = hc_dev->h_dev;
	dma_addr_t llp_low, llp_high, llp;
	struct hdma_desc *desc;
	struct hdmav_dev *hv_dev = NULL;

	spin_lock(&hc_dev->hdma_lock);
	HDMAC_VERB(hc_dev, HDMAV_NO_CH_ID, "Enter\n");

	hc_dev->is_channel_db_rang = false;
	llp_low = readl_relaxed(hc_dev->llp_low_reg);
	llp_high = readl_relaxed(hc_dev->llp_high_reg);
	llp = (u64)(llp_high << 32) | llp_low;
	HDMAC_VERB(hc_dev, HDMAV_NO_CH_ID, "Start: DMA_LLP = %pad\n", &llp);

	while (hc_dev->tl_dma_rd_p != llp) {

		/* current element is a link element. Need to jump and free */
		if (hc_dev->tl_rd_p->ch_ctrl & HDMA_ELE_LLP) {
			struct hdma_desc *ldesc = *hc_dev->dl_rd_p;

			hc_dev->tl_dma_rd_p = ldesc->tl_dma_next;
			hc_dev->tl_rd_p = (struct data_element *)ldesc->tl_next;
			hc_dev->dl_rd_p = ldesc->dl_next;

			HDMAC_VERB(hc_dev, HDMAV_NO_CH_ID,
				"free transfer list: %pad\n", &ldesc->tl_dma);
			dma_free_coherent(h_dev->dev,
				sizeof(*ldesc->tl) * h_dev->n_tl_ele,
				ldesc->tl, ldesc->tl_dma);
			kfree(ldesc->dl);
			continue;
		}

		HDMAC_VERB(hc_dev, HDMAV_NO_CH_ID, "TL_DMA_RD_P: %pad\n",
			&hc_dev->tl_dma_rd_p);

		desc = *hc_dev->dl_rd_p;
		hc_dev->tl_dma_rd_p += sizeof(struct data_element);
		hc_dev->tl_rd_p++;
		hc_dev->dl_rd_p++;
		if (desc) {

			/*
			 * struct hdmav_dev address is taken from hdma_desc
			 * structure. And if it is update for first time is
			 * sufficient.
			 */
			if (hv_dev == NULL)
				hv_dev = desc->hv_dev;

			/*
			 * Clients might queue descriptors in the call back
			 * context. Release spinlock to avoid deadlock scenarios
			 * as we use same lock duing descriptor queuing.
			 */
			spin_unlock(&hc_dev->hdma_lock);
			dmaengine_desc_get_callback_invoke(&desc->tx, NULL);

			/* Acquire spinlock again to continue hdma operations */
			spin_lock(&hc_dev->hdma_lock);
			kfree(desc);
		} else {
			HDMAC_VERB(hc_dev, HDMAV_NO_CH_ID,
				"hdma desc is NULL\n");
		}
	}


	if (hv_dev == NULL) {
		HDMAC_VERB(hc_dev, HDMAV_NO_CH_ID, "hv_dev is NULL\n");
		goto exit;
	}

	hdma_submit_pending_transfers(hv_dev);

exit:
	HDMAC_VERB(hc_dev, HDMAV_NO_CH_ID, "Exit\n");
	hdma_set_clear(hc_dev->int_setup_reg, 0, HDMA_WATERMARK_MASK_BIT);
	spin_unlock(&hc_dev->hdma_lock);
}

static irqreturn_t handle_hdma_irq(int irq, void *data)
{
	struct hdmac_lane *hc_lane = data;
	u32 wr_int_status, rd_int_status;
	struct hdmac_dev *wr_hc_dev = hc_lane->wr;
	struct hdmac_dev *rd_hc_dev = hc_lane->rd;
#ifdef CONFIG_QCOM_PCI_HDMA_DEBUG
	struct hdma_dev *h_dev = wr_hc_dev->h_dev;
#endif

	wr_int_status = readl_relaxed(wr_hc_dev->int_status_reg);
	rd_int_status = readl_relaxed(rd_hc_dev->int_status_reg);

	hdma_set_clear(wr_hc_dev->int_clear_reg, wr_int_status, 0);
	hdma_set_clear(rd_hc_dev->int_clear_reg, rd_int_status, 0);

	HDMA_IRQ(h_dev, HDMAV_NO_CH_ID,
		"IRQ wr status: 0x%x rd status: 0x%x\n",
		wr_int_status, rd_int_status);

	HDMA_ASSERT((wr_int_status & HDMA_INT_ERR_MASK) ||
		(rd_int_status & HDMA_INT_ERR_MASK),
		"Error reported by H/W\n");

	if (wr_int_status & HDMA_WATERMARK_STATUS_BIT) {
		hdma_set_clear(wr_hc_dev->int_setup_reg, HDMA_WATERMARK_MASK_BIT, 0);
		queue_work(wr_hc_dev->pending_process_wq, &wr_hc_dev->pending_work);
	}

	if (rd_int_status & HDMA_WATERMARK_STATUS_BIT) {
		hdma_set_clear(rd_hc_dev->int_setup_reg, HDMA_WATERMARK_MASK_BIT, 0);
		queue_work(rd_hc_dev->pending_process_wq, &rd_hc_dev->pending_work);
	}

	return IRQ_HANDLED;
}

static struct dma_chan *hdma_of_dma_xlate(struct of_phandle_args *args,
					 struct of_dma *of_dma)
{
	struct hdma_dev *h_dev = (struct hdma_dev *)of_dma->of_dma_data;
	struct hdmac_dev *hc_dev;
	struct hdmav_dev *hv_dev;
	struct hdmac_lane *hc_lane;
	u32 ch_id, ret;

	if (args->args_count < REQ_OF_DMA_ARGS) {
		HDMA_ERR(h_dev,
			"HDMA requires at least %d arguments, client passed:%d\n",
			REQ_OF_DMA_ARGS, args->args_count);
		return NULL;
	}

	if (h_dev->cur_hv_ch_idx >= h_dev->n_max_hv_ch) {
		HDMA_ERR(h_dev, "No more HDMA virtual channels available\n");
		return NULL;
	}

	hv_dev = &h_dev->hv_devs[h_dev->cur_hv_ch_idx++];

	hv_dev->dir = args->args[0] ? HDMA_RD_CH : HDMA_WR_CH;
	hv_dev->priority = args->args[1];

	/* use round robin to allocate HDMA channel */
	if (hv_dev->dir == HDMA_WR_CH) {
		ch_id = h_dev->cur_wr_ch_idx++ % h_dev->n_wr_ch;
		hc_dev = &h_dev->hc_wr_devs[ch_id];
		hc_lane = &h_dev->hc_lanes[ch_id];
	} else {
		ch_id = h_dev->cur_rd_ch_idx++ % h_dev->n_rd_ch;
		hc_dev = &h_dev->hc_rd_devs[ch_id];
		hc_lane = &h_dev->hc_lanes[ch_id];
	}

	hv_dev->hc_dev = hc_dev;
	hv_dev->ch_id = HDMAV_BASE_CH_ID * hc_dev->ch_id + hc_dev->n_hv;

	list_add_tail(&hv_dev->node, &hc_dev->hv_list);

	HDMA_LOG(h_dev, "HC_ID: %u HV_ID: %u direction: %s priority: %u",
		hc_dev->ch_id, hv_dev->ch_id, TO_HDMA_DIR_CH_STR(hv_dev->dir),
		hv_dev->priority);

	ret = hdma_init_irq(hc_lane);
	if (ret) {
		HDMA_ERR(h_dev, "HDMA IRQ not found");
		return NULL;
	}

	writel_relaxed(DMA_CH_EN, hc_dev->ch_en_reg);

	return dma_get_slave_channel(&hv_dev->dma_ch);
}

static int hdma_alloc_transfer_list(struct hdmac_dev *hc_dev)
{
	struct hdma_dev *h_dev = hc_dev->h_dev;
	union hdma_element *tl;
	struct hdma_desc **dl;
	struct hdma_desc *ldesc;
	dma_addr_t tl_dma;
	u32 n_tl_ele = h_dev->n_tl_ele;

	HDMAC_VERB(hc_dev, HDMAV_NO_CH_ID, "enter\n");

	tl = dma_alloc_coherent(h_dev->dev, sizeof(*tl) * n_tl_ele,
				&tl_dma, GFP_ATOMIC);
	if (!tl)
		return -ENOMEM;

	dl = kcalloc(n_tl_ele, sizeof(*dl), GFP_ATOMIC);
	if (!dl)
		goto free_transfer_list;

	ldesc = kzalloc(sizeof(*ldesc), GFP_ATOMIC);
	if (!ldesc)
		goto free_descriptor_list;

	HDMAC_VERB(hc_dev, HDMAV_NO_CH_ID,
		"allocated transfer list dma: %pad\n", &tl_dma);

	dl[n_tl_ele - 1] = ldesc;

	if (hc_dev->tl_wr_p) {
		/* link current lists with new lists */
		hc_dev->le_p->lle_ptr = tl_dma;
		(*hc_dev->ldl_p)->tl_dma_next = tl_dma;
		(*hc_dev->ldl_p)->tl_next = tl;
		(*hc_dev->ldl_p)->dl_next = dl;
	} else {
		/* init read and write ptr if these are the initial lists */
		hc_dev->tl_dma_rd_p = tl_dma;
		hc_dev->tl_wr_p = hc_dev->tl_rd_p = (struct data_element *)tl;
		hc_dev->dl_wr_p = hc_dev->dl_rd_p = dl;
	}

	/* move ptr and compose LE and LDESC of new lists */
	hc_dev->le_p = (struct link_element *)&tl[n_tl_ele - 1];
	hc_dev->le_p->ch_ctrl = HDMA_ELE_LLP | HDMA_ELE_CYCLE_CB;

	/* setup ldesc */
	hc_dev->ldl_p = dl + n_tl_ele - 1;
	(*hc_dev->ldl_p)->tl_dma = tl_dma;
	(*hc_dev->ldl_p)->tl = tl;
	(*hc_dev->ldl_p)->dl = dl;

	HDMAC_VERB(hc_dev, HDMAV_NO_CH_ID, "exit\n");

	return 0;

free_descriptor_list:
	kfree(dl);
free_transfer_list:
	dma_free_coherent(h_dev->dev, sizeof(*tl) * n_tl_ele, tl, tl_dma);

	HDMAC_VERB(hc_dev, HDMAV_NO_CH_ID, "exit with error\n");

	return -ENOMEM;
}

static void hdma_free_chan_resources(struct dma_chan *chan)
{
	struct hdmav_dev *hv_dev = to_hdmav_dev(chan);
	struct hdmac_dev *hc_dev = hv_dev->hc_dev;
	struct hdma_dev *h_dev = hc_dev->h_dev;
	struct hdma_desc **ldesc;

	if (!hc_dev->n_hv)
		return;

	if (--hc_dev->n_hv)
		return;

	/* get ldesc of desc */
	ldesc = hc_dev->dl_wr_p + hc_dev->n_de_avail;
	while (ldesc) {
		struct hdma_desc *ldesc_t = *ldesc;

		/* move ldesc ptr to next list ldesc and free current lists */
		if (ldesc_t->dl_next)
			ldesc = ldesc_t->dl_next + h_dev->n_tl_ele - 1;
		else
			ldesc = NULL;

		if (ldesc_t->tl)
			dma_free_coherent(h_dev->dev,
				sizeof(*ldesc_t->tl) * h_dev->n_tl_ele,
				ldesc_t->tl, ldesc_t->tl_dma);
		kfree(ldesc_t->dl);
	}

	hc_dev->dl_wr_p = hc_dev->dl_rd_p = hc_dev->ldl_p = NULL;
	hc_dev->tl_wr_p = hc_dev->tl_rd_p = NULL;
	hc_dev->le_p = NULL;
	hc_dev->tl_dma_rd_p = 0;
}

static int hdma_alloc_chan_resources(struct dma_chan *chan)
{
	struct hdmav_dev *hv_dev = to_hdmav_dev(chan);
	struct hdmac_dev *hc_dev = hv_dev->hc_dev;
	struct hdma_dev *h_dev = hc_dev->h_dev;
	int ret = 0;

	/*
	 * If this is the first client for this HDMA channel, setup the initial
	 * transfer and descriptor lists and configure the H/W for it.
	 */
	if (!hc_dev->n_hv) {
		int i;

		for (i = 0; i < h_dev->n_tl_init; i++) {
			ret = hdma_alloc_transfer_list(hc_dev);
			if (ret)
				goto out;
		}

		/* Clear Interrupt Mask */
		writel_relaxed(0, hc_dev->int_setup_reg);
		/* Enable Abort and Stop Interrupts */
		writel_relaxed(HDMA_LOCAL_ABORT_INT_BIT |
			HDMA_LOCAL_STOP_INT_BIT, hc_dev->int_setup_reg);

		writel_relaxed(HDMA_CH_CYCLE_CCS, hc_dev->ch_cycle_reg);
		writel_relaxed(HDMA_CH_CONTROL1_LLE, hc_dev->ch_ctrl1_reg);
		writel_relaxed(lower_32_bits(hc_dev->tl_dma_rd_p),
				hc_dev->llp_low_reg);
		writel_relaxed(upper_32_bits(hc_dev->tl_dma_rd_p),
				hc_dev->llp_high_reg);
		hc_dev->n_de_avail = h_dev->n_tl_ele - 1;
	}
	hc_dev->n_hv++;

	return 0;
out:
	hdma_free_chan_resources(chan);
	return ret;
}

static inline void hdma_compose_data_element(struct hdmav_dev *hv_dev,
				struct data_element *de, dma_addr_t dst_addr,
				dma_addr_t src_addr, size_t size,
				unsigned long flags)
{
	HDMAC_VERB(hv_dev->hc_dev, hv_dev->ch_id,
		"size = %d, dst_addr: %pad\tsrc_addr: %pad\n",
		size, &dst_addr, &src_addr);

	if (flags & DMA_PREP_INTERRUPT)
		de->ch_ctrl |= HDMA_ELE_LWIE;
	de->size = size;
	de->sar = src_addr;
	de->dar = dst_addr;
}

static struct hdma_desc *hdma_alloc_descriptor(struct hdmav_dev *hv_dev)
{
	struct hdma_desc *desc;

	desc = kzalloc(sizeof(*desc) + sizeof(*desc->de),
			GFP_ATOMIC);
	if (!desc)
		return NULL;

	desc->de = (struct data_element *)(&desc[1]);
	desc->hv_dev = hv_dev;
	hv_dev->n_de++;

	dma_async_tx_descriptor_init(&desc->tx, &hv_dev->dma_ch);
	desc->tx.tx_submit = hdma_submit;

	return desc;
}

struct dma_async_tx_descriptor *hdma_prep_dma_memcpy(struct dma_chan *chan,
						dma_addr_t dst, dma_addr_t src,
						size_t len, unsigned long flags)
{
	struct hdmav_dev *hv_dev = to_hdmav_dev(chan);
	struct hdma_desc *desc;

	spin_lock(&hv_dev->hc_dev->hdma_lock);
	HDMAC_VERB(hv_dev->hc_dev, hv_dev->ch_id, "Enter\n");

	desc = hdma_alloc_descriptor(hv_dev);
	if (!desc)
		goto err;

	hdma_compose_data_element(hv_dev, desc->de, dst, src, len, flags);

	HDMAC_VERB(hv_dev->hc_dev, hv_dev->ch_id, "exit\n");
	spin_unlock(&hv_dev->hc_dev->hdma_lock);

	return &desc->tx;
err:
	HDMAC_VERB(hv_dev->hc_dev, hv_dev->ch_id,
		"hdma alloc descriptor failed for channel:%d\n", hv_dev->ch_id);
	spin_unlock(&hv_dev->hc_dev->hdma_lock);
	return NULL;
}

static void hdma_issue_descriptor(struct hdmac_dev *hc_dev,
				struct hdma_desc *desc)
{
	/* set descriptor for last Data Element */
	*hc_dev->dl_wr_p = desc;
	memcpy(hc_dev->tl_wr_p, desc->de, sizeof(*desc->de));

	/*
	 * Ensure Desc data element should be flushed to tl_wr_p
	 * before updating CB flag
	 */
	mb();

	HDMAC_VERB(hc_dev, HDMAV_NO_CH_ID,
			"size: %d, dst_addr: %pad\tsrc_addr: %pad\n",
			hc_dev->tl_wr_p->size, &hc_dev->tl_wr_p->dar,
			&hc_dev->tl_wr_p->sar);

	hc_dev->tl_wr_p->ch_ctrl |= HDMA_ELE_CYCLE_CB;

	/*
	 * Ensure that CB flag is properly flushed because
	 * HW starts processing the descriptor based on CB flag.
	 */
	mb();

	HDMAC_VERB(hc_dev, HDMAV_NO_CH_ID, "ch_ctrl = %d\n",
		hc_dev->tl_wr_p->ch_ctrl);

	hc_dev->dl_wr_p++;
	hc_dev->tl_wr_p++;
	hc_dev->n_de_avail--;

	/* dl_wr_p points to ldesc and tl_wr_p points link element */
	if (!hc_dev->n_de_avail) {
		int ret;

		hc_dev->tl_wr_p = (struct data_element *)
			(*hc_dev->dl_wr_p)->tl_next;
		hc_dev->dl_wr_p = (*hc_dev->dl_wr_p)->dl_next;
		hc_dev->n_de_avail = hc_dev->h_dev->n_tl_ele - 1;

		ret = hdma_alloc_transfer_list(hc_dev);
		HDMA_ASSERT(ret, "failed to allocate new transfer list\n");
	}
}

static void hdma_issue_pending(struct dma_chan *chan)
{
	struct hdmav_dev *hv_dev = to_hdmav_dev(chan);
	struct hdmac_dev *hc_dev = hv_dev->hc_dev;

	spin_lock(&hc_dev->hdma_lock);
	HDMAC_VERB(hc_dev, hv_dev->ch_id, "enter\n");

	hdma_submit_pending_transfers(hv_dev);

	HDMAC_VERB(hc_dev, hv_dev->ch_id, "exit\n");
	spin_unlock(&hc_dev->hdma_lock);
}

static int hdma_config(struct dma_chan *chan, struct dma_slave_config *config)
{
	return -EINVAL;
}

static int hdma_terminate_all(struct dma_chan *chan)
{
	return -EINVAL;
}

static int hdma_pause(struct dma_chan *chan)
{
	return -EINVAL;
}

static enum dma_status hdma_tx_status(struct dma_chan *chan,
	dma_cookie_t cookie, struct dma_tx_state *txstate)
{
	struct hdmav_dev *hv_dev = to_hdmav_dev(chan);
	struct hdmac_dev *hc_dev = hv_dev->hc_dev;
	enum dma_status status = DMA_IN_PROGRESS;
	enum hdma_hw_state hw_state;

	hw_state = hdma_get_hw_state(hc_dev);
	spin_lock(&hc_dev->hdma_lock);

	if (unlikely(list_empty(&hv_dev->dl)) &&
		((hw_state ==  HDMA_HW_STATE_INIT) ||
			(hw_state ==  HDMA_HW_STATE_STOPPED)))
		status =  DMA_COMPLETE;
	spin_unlock(&hc_dev->hdma_lock);
	return status;
}

static int hdma_resume(struct dma_chan *chan)
{
	return -EINVAL;
}

static int hdma_init_irq(struct hdmac_lane *hc_lane)
{
	struct hdmac_dev *wr_hc_dev = hc_lane->wr;
	struct hdmac_dev *rd_hc_dev = hc_lane->rd;
	struct hdma_dev *h_dev = wr_hc_dev->h_dev;
	int ret, ch_id;
	char irq_name[30];

	if (wr_hc_dev->irq || rd_hc_dev->irq)
		return 0;

	ch_id = wr_hc_dev->ch_id;
	scnprintf(irq_name, 30, "pci-hdma-int_%u", wr_hc_dev->ch_id);
	ret = of_irq_get_byname(h_dev->of_node, irq_name);
	if (ret <= 0) {
		HDMA_ERR(h_dev, "failed to get IRQ from DT. ret: %d\n", ret);
		return ret;
	}

	wr_hc_dev->irq = ret;
	rd_hc_dev->irq = ret;

	HDMA_LOG(h_dev, "Received HDMA irq %s:%d\n", irq_name, wr_hc_dev->irq);

	ret = devm_request_irq(h_dev->dev, wr_hc_dev->irq,
			       handle_hdma_irq, IRQF_TRIGGER_HIGH,
			       irq_name, hc_lane);
	if (ret < 0) {
		HDMA_ERR(h_dev, "failed to request irq: %d ret: %d\n",
			wr_hc_dev->irq, ret);
		return ret;
	}

	return 0;
}

static void hdma_init_log(struct hdma_dev *h_dev, struct hdmac_dev *hc_dev)
{
	if (!hc_dev) {
		snprintf(h_dev->label, HDMA_LABEL_SIZE, "%s_%llx",
			HDMA_DRV_NAME, (u64)h_dev->base_phys);

		h_dev->ipc_log_lvl = DEFAULT_IPC_LOG_LVL;
		h_dev->klog_lvl = DEFAULT_KLOG_LVL;
		h_dev->ipc_log = ipc_log_context_create(IPC_LOG_PAGES,
							h_dev->label, 0);
		h_dev->ipc_log_irq = ipc_log_context_create(IPC_LOG_PAGES,
							h_dev->label, 0);
	} else {
		snprintf(hc_dev->label, HDMA_LABEL_SIZE, "%s_%llx_%s_ch_%u",
			HDMA_DRV_NAME, (u64)h_dev->base_phys,
			(hc_dev->dir == HDMA_WR_CH ? "wr" : "rd"),
			hc_dev->ch_id);

		hc_dev->ipc_log_lvl = DEFAULT_IPC_LOG_LVL;
		hc_dev->klog_lvl = DEFAULT_KLOG_LVL;
		hc_dev->ipc_log = ipc_log_context_create(IPC_LOG_PAGES,
							hc_dev->label, 0);
	}
}

static int hdma_init_channels(struct hdma_dev *h_dev)
{
	int i, ret = 0;
	char wq_name[30];

	/* setup physical channels */
	for (i = 0; i < h_dev->n_max_hc_ch; i++) {
		struct hdmac_dev *hc_dev;
		struct hdmac_lane *hc_lane;
		enum hdma_dir_ch dir;
		u32 ch_id;
		u32 int_setup_off, int_status_off, int_clear_off;
		u32 ch_ctrl1_off, ch_cycle_off;
		u32 ch_en_off, ch_status_off, llp_low_off, llp_high_off, db_off;

		if (i < h_dev->n_wr_ch) {
			ch_id = i;
			hc_dev = &h_dev->hc_wr_devs[ch_id];
			hc_lane = &h_dev->hc_lanes[ch_id];
			hc_lane->wr = hc_dev;
			dir = HDMA_WR_CH;
			int_setup_off = DMA_INT_SETUP_OFF(dir, ch_id);
			int_status_off = DMA_INT_STATUS_OFF(dir, ch_id);
			int_clear_off = DMA_INT_CLEAR_OFF(dir, ch_id);
			ch_ctrl1_off = DMA_CONTROL1_OFF(dir, ch_id);
			ch_cycle_off = DMA_CYCLE_OFF(dir, ch_id);
			ch_en_off = DMA_CH_EN_OFF(dir, ch_id);
			ch_status_off = DMA_CH_STATUS_OFF(dir, ch_id);
			llp_low_off = DMA_LLP_LOW_OFF_DIR_CH_N(dir, ch_id);
			llp_high_off = DMA_LLP_HIGH_OFF_DIR_CH_N(dir, ch_id);
			db_off = DMA_DOORBELL_OFF_DIR_CH_N(dir, ch_id);
		} else {
			ch_id = i - h_dev->n_wr_ch;
			hc_dev = &h_dev->hc_rd_devs[ch_id];
			hc_lane = &h_dev->hc_lanes[ch_id];
			hc_lane->rd = hc_dev;
			dir = HDMA_RD_CH;
			int_setup_off = DMA_INT_SETUP_OFF(dir, ch_id);
			int_status_off = DMA_INT_STATUS_OFF(dir, ch_id);
			int_clear_off = DMA_INT_CLEAR_OFF(dir, ch_id);
			ch_ctrl1_off = DMA_CONTROL1_OFF(dir, ch_id);
			ch_cycle_off = DMA_CYCLE_OFF(dir, ch_id);
			ch_en_off = DMA_CH_EN_OFF(dir, ch_id);
			ch_status_off = DMA_CH_STATUS_OFF(dir, ch_id);
			llp_low_off = DMA_LLP_LOW_OFF_DIR_CH_N(dir, ch_id);
			llp_high_off = DMA_LLP_HIGH_OFF_DIR_CH_N(dir, ch_id);
			db_off = DMA_DOORBELL_OFF_DIR_CH_N(dir, ch_id);
		}

		hc_dev->h_dev = h_dev;
		hc_dev->ch_id = ch_id;
		hc_dev->dir = dir;
		hc_dev->hw_state = HDMA_HW_STATE_INIT;
		hc_dev->is_channel_db_rang = false;

		hdma_init_log(h_dev, hc_dev);

		INIT_LIST_HEAD(&hc_dev->hv_list);
		if (hc_dev->dir == HDMA_WR_CH)
			scnprintf(wq_name, 30, "hdma_pending_wq_wr_%u", hc_dev->ch_id);
		else
			scnprintf(wq_name, 30, "hdma_pending_wq_rd_%u", hc_dev->ch_id);

		hc_dev->pending_process_wq = alloc_workqueue(wq_name, WQ_HIGHPRI | WQ_UNBOUND, 1);
		if (!hc_dev->pending_process_wq) {
			ret = -ENOMEM;
			return ret;
		}

		INIT_WORK(&hc_dev->pending_work, hdmac_process_workqueue);

		HDMA_LOG(h_dev, "HC_DIR: %s HC_INDEX: %d HC_ADDR: 0x%pK\n",
			TO_HDMA_DIR_CH_STR(hc_dev->dir), hc_dev->ch_id, hc_dev);

		hc_dev->int_setup_reg = h_dev->base + int_setup_off;
		hc_dev->int_status_reg = h_dev->base + int_status_off;
		hc_dev->int_clear_reg = h_dev->base + int_clear_off;
		hc_dev->ch_ctrl1_reg = h_dev->base + ch_ctrl1_off;
		hc_dev->ch_cycle_reg = h_dev->base + ch_cycle_off;
		hc_dev->ch_status_reg = h_dev->base + ch_status_off;
		hc_dev->ch_en_reg = h_dev->base + ch_en_off;
		hc_dev->llp_low_reg = h_dev->base + llp_low_off;
		hc_dev->llp_high_reg = h_dev->base + llp_high_off;
		hc_dev->db_reg = h_dev->base + db_off;
		spin_lock_init(&hc_dev->hdma_lock);
	}

	/* setup virtual channels */
	for (i = 0; i < h_dev->n_max_hv_ch; i++) {
		struct hdmav_dev *hv_dev = &h_dev->hv_devs[i];

		dma_cookie_init(&hv_dev->dma_ch);
		INIT_LIST_HEAD(&hv_dev->dl);
		hv_dev->dma_ch.device = &h_dev->dma_device;

		list_add_tail(&hv_dev->dma_ch.device_node,
				&h_dev->dma_device.channels);

		HDMA_LOG(h_dev, "HV_INDEX: %d HV_ADDR: 0x%pK\n", i, hv_dev);
	}
	return ret;
}

static void hdma_init_dma_device(struct hdma_dev *h_dev)
{
	/* clear and set capabilities */
	dma_cap_zero(h_dev->dma_device.cap_mask);
	dma_cap_set(DMA_MEMCPY, h_dev->dma_device.cap_mask);

	h_dev->dma_device.dev = h_dev->dev;
	h_dev->dma_device.device_config = hdma_config;
	h_dev->dma_device.device_pause = hdma_pause;
	h_dev->dma_device.device_resume = hdma_resume;
	h_dev->dma_device.device_terminate_all = hdma_terminate_all;
	h_dev->dma_device.device_alloc_chan_resources =
		hdma_alloc_chan_resources;
	h_dev->dma_device.device_free_chan_resources =
		hdma_free_chan_resources;
	h_dev->dma_device.device_prep_dma_memcpy = hdma_prep_dma_memcpy;
	h_dev->dma_device.device_issue_pending = hdma_issue_pending;
	h_dev->dma_device.device_tx_status = hdma_tx_status;
}

void hdma_dump(void)
{
	int i;

	for (i = 0; i < HDMA_MAX_SIZE; i += 32) {
		pr_err("HDMA Reg : 0x%04x %08x %08x %08x %08x %08x %08x %08x %08x\n",
			i,
			readl_relaxed(h_dev_info->base + i),
			readl_relaxed(h_dev_info->base + (i + 4)),
			readl_relaxed(h_dev_info->base + (i + 8)),
			readl_relaxed(h_dev_info->base + (i + 12)),
			readl_relaxed(h_dev_info->base + (i + 16)),
			readl_relaxed(h_dev_info->base + (i + 20)),
			readl_relaxed(h_dev_info->base + (i + 24)),
			readl_relaxed(h_dev_info->base + (i + 28)));
	}
}
EXPORT_SYMBOL(hdma_dump);

/*
 * Initializes and enables HDMA driver and H/W block for PCIe controllers.
 * Only call this function if PCIe supports HDMA and has all its resources
 * turned on.
 */
int qcom_hdma_init(struct device *dev)
{
	int ret;
	struct hdma_dev *h_dev;
	struct device_node *of_node;
	const __be32 *prop_val;

	if (!dev || !dev->of_node) {
		pr_err("HDMA: invalid %s\n", dev ? "of_node" : "dev");
		return -EINVAL;
	}

	of_node = of_parse_phandle(dev->of_node, "hdma-parent", 0);
	if (!of_node) {
		pr_info("HDMA: no phandle for HDMA found\n");
		return -ENODEV;
	}

	if (!of_device_is_compatible(of_node, "qcom,pci-hdma")) {
		pr_info("HDMA: no compatible qcom,pci-hdma found\n");
		return -ENODEV;
	}

	h_dev = devm_kzalloc(dev, sizeof(*h_dev), GFP_KERNEL);
	if (!h_dev)
		return -ENOMEM;

	memset(h_dev, 0, sizeof(*h_dev));


	h_dev->dev = dev;
	h_dev->of_node = of_node;

	prop_val = of_get_address(h_dev->of_node, 0, NULL, NULL);
	if (!prop_val) {
		pr_err("HDMA: missing 'hdma_base reg' in devicetree\n");
		return -EINVAL;
	}

	h_dev->base_phys = be32_to_cpup(prop_val);
	if (!h_dev->base_phys) {
		pr_err("HDMA: failed to get HDMA base register address\n");
		return -EINVAL;
	}

	h_dev->base_size = be32_to_cpup(&prop_val[1]);
	if (!h_dev->base_size) {
		pr_err("HDMA: failed to get the size of HDMA register space\n");
		return -EINVAL;
	}

	h_dev->base = devm_ioremap(h_dev->dev, h_dev->base_phys,
					h_dev->base_size);
	if (!h_dev->base) {
		pr_err("HDMA: failed to remap HDMA base register\n");
		return -EFAULT;
	}

	prop_val = of_get_address(h_dev->of_node, 1, NULL, NULL);
	if (!prop_val) {
		pr_err("HDMA: missing 'controller DBI' in devicetree\n");
		return -EINVAL;
	}

	h_dev->dbi_phys = be32_to_cpup(prop_val);
	if (!h_dev->dbi_phys) {
		pr_err("HDMA: failed to get HDMA DBI register address\n");
		return -EINVAL;
	}

	h_dev->dbi_size = be32_to_cpup(&prop_val[1]);
	if (!h_dev->dbi_size) {
		pr_err("HDMA: failed to get the size of HDMA DBI register space\n");
		return -EINVAL;
	}

	h_dev->dbi = devm_ioremap(h_dev->dev, h_dev->dbi_phys,
					h_dev->dbi_size);
	if (!h_dev->dbi) {
		pr_err("HDMA: failed to remap HDMA DBI register\n");
		return -EFAULT;
	}

	hdma_init_log(h_dev, NULL);

	h_dev->n_wr_ch = (readl_relaxed(h_dev->dbi + DMA_NUM_CHAN_OFF) >>
			DMA_NUM_WR_CH_SHIFT) & DMA_NUM_CH_MASK;
	HDMA_LOG(h_dev, "number of write channels: %d\n",
		h_dev->n_wr_ch);

	h_dev->n_rd_ch = (readl_relaxed(h_dev->dbi + DMA_NUM_CHAN_OFF) >>
			DMA_NUM_RD_CH_SHIFT) & DMA_NUM_CH_MASK;
	HDMA_LOG(h_dev, "number of read channels: %d\n",
		h_dev->n_rd_ch);

	h_dev->n_max_hc_ch = h_dev->n_wr_ch + h_dev->n_rd_ch;
	HDMA_LOG(h_dev, "number of HDMA physical channels: %d\n",
		h_dev->n_max_hc_ch);

	ret = of_property_read_u32(h_dev->of_node, "qcom,n-max-hv-ch",
				&h_dev->n_max_hv_ch);
	if (ret)
		h_dev->n_max_hv_ch = HDMA_NUM_MAX_HV_CH;

	HDMA_LOG(h_dev, "number of HDMA virtual channels: %d\n",
		h_dev->n_max_hv_ch);

	ret = of_property_read_u32(h_dev->of_node, "qcom,n-tl-init",
				&h_dev->n_tl_init);
	if (ret)
		h_dev->n_tl_init = HDMA_NUM_TL_INIT;

	HDMA_LOG(h_dev,
		"number of initial transfer and descriptor lists: %d\n",
		h_dev->n_tl_init);

	ret = of_property_read_u32(h_dev->of_node, "qcom,n-tl-ele",
				&h_dev->n_tl_ele);
	if (ret)
		h_dev->n_tl_ele = HDMA_NUM_TL_ELE;

	HDMA_LOG(h_dev,
		"number of elements for transfer and descriptor list: %d\n",
		h_dev->n_tl_ele);

	h_dev->hc_wr_devs = devm_kcalloc(h_dev->dev, h_dev->n_wr_ch,
				sizeof(*h_dev->hc_wr_devs), GFP_KERNEL);
	if (!h_dev->hc_wr_devs)
		return -ENOMEM;

	memset(h_dev->hc_wr_devs, 0, sizeof(*h_dev->hc_wr_devs));

	h_dev->hc_rd_devs = devm_kcalloc(h_dev->dev, h_dev->n_rd_ch,
				sizeof(*h_dev->hc_rd_devs), GFP_KERNEL);
	if (!h_dev->hc_rd_devs)
		return -ENOMEM;

	memset(h_dev->hc_rd_devs, 0, sizeof(*h_dev->hc_rd_devs));

	h_dev->hv_devs = devm_kcalloc(h_dev->dev, h_dev->n_max_hv_ch,
				sizeof(*h_dev->hv_devs), GFP_KERNEL);
	if (!h_dev->hv_devs)
		return -ENOMEM;

	h_dev->hc_lanes = devm_kcalloc(h_dev->dev, h_dev->n_rd_ch,
				sizeof(*h_dev->hc_lanes), GFP_KERNEL);

	if (!h_dev->hc_lanes)
		return -ENOMEM;

	memset(h_dev->hc_lanes, 0, sizeof(*h_dev->hc_lanes));

	INIT_LIST_HEAD(&h_dev->dma_device.channels);
	ret = hdma_init_channels(h_dev);
	if (ret)
		return ret;

	hdma_init_dma_device(h_dev);
	h_dev_info = h_dev;

	ret = dma_async_device_register(&h_dev->dma_device);
	if (ret) {
		HDMA_ERR(h_dev, "failed to register device: %d\n", ret);
		return ret;
	}

	ret = of_dma_controller_register(h_dev->of_node, hdma_of_dma_xlate,
					h_dev);
	if (ret) {
		HDMA_ERR(h_dev, "failed to register controller %d\n", ret);
		dma_async_device_unregister(&h_dev->dma_device);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(qcom_hdma_init);

static int hdma_dev_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int hdma_dev_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id hdma_dev_match_table[] = {
	{	.compatible = "qcom,pci-hdma" },
	{}
};

static struct platform_driver hdma_dev_driver = {
	.driver		= {
		.name	= "qcom,pci-hdma",
		.of_match_table = hdma_dev_match_table,
	},
	.probe		= hdma_dev_probe,
	.remove		= hdma_dev_remove,
};

static int __init hdma_dev_init(void)
{
	pr_debug("%s\n", __func__);
	return platform_driver_register(&hdma_dev_driver);
}
subsys_initcall(hdma_dev_init);

static void __exit hdma_dev_exit(void)
{
	pr_debug("%s\n", __func__);
	platform_driver_unregister(&hdma_dev_driver);
}
module_exit(hdma_dev_exit);

MODULE_DESCRIPTION("QTI PCIe HDMA driver");
MODULE_LICENSE("GPL v2");
