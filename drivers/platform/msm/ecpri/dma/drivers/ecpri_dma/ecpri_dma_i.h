/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ECPRI_DMA_I_H_
#define _ECPRI_DMA_I_H_

#include <linux/bitops.h>
#include <linux/export.h>
#include <linux/idr.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/errno.h>
#include <linux/ipc_logging.h>
#include <linux/dma-mapping.h>
#include <linux/string.h>
#include <linux/iommu.h>
#include <linux/version.h>
#include <linux/clk.h>
#include <linux/mhi_dma.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0))
#include <linux/qcom-iommu-util.h>
#endif
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/msm_gsi.h>
#include "ecpri_dma.h"
#include "gsi.h"

#if IS_ENABLED(CONFIG_DEBUG_FS) && !defined(CONFIG_DEBUG_FS)
#define CONFIG_DEBUG_FS (1)
#endif

#if IS_ENABLED(CONFIG_ECPRI_DMA_UT)  && !defined(CONFIG_ECPRI_DMA_UT)
#define CONFIG_ECPRI_DMA_UT (1)
#endif

#define ECPRI_DMA_RESET_WA_ENABLE (1)

#define DRV_NAME "ecpri-dma"

extern struct ecpri_dma_context *ecpri_dma_ctx;

/** MACROS **/

/*
 * Printing one error message in 5 seconds if multiple error messages
 * are coming back to back.
 */

#define WARNON_RATELIMIT_BURST 1
#define ECPRI_DMA_RATELIMIT_BURST 1
#define pr_err_ratelimited_ecpri_dma(fmt, args...)				\
({									\
	static DEFINE_RATELIMIT_STATE(_rs,				\
				      DEFAULT_RATELIMIT_INTERVAL,	\
				      ECPRI_DMA_RATELIMIT_BURST);		\
									\
	if (__ratelimit(&_rs))						\
		pr_err(fmt, ## args);					\
})

void ecpri_dma_assert(void);

#define ecpri_dma_assert_on(condition)\
do {\
	if (unlikely(condition))\
		ecpri_dma_assert();\
} while (0)

#define DMA_IPC_LOGGING(buf, fmt, args...) \
	do { \
		if (buf) \
			ipc_log_string((buf), fmt, __func__, __LINE__, \
				## args); \
	} while (0)

#define DMADBG(fmt, args...) \
	do { \
		pr_debug(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
		if (ecpri_dma_ctx) { \
			DMA_IPC_LOGGING(ecpri_dma_ctx->logbuf, \
				DRV_NAME " %s:%d " fmt, ## args); \
			DMA_IPC_LOGGING(ecpri_dma_ctx->logbuf_low, \
				DRV_NAME " %s:%d " fmt, ## args); \
		} \
	} while (0)

#define DMADBG_LOW(fmt, args...) \
	do { \
		pr_debug(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ##args);\
		if (ecpri_dma_ctx) \
			DMA_IPC_LOGGING(ecpri_dma_ctx->logbuf_low, \
				DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define DMAERR(fmt, args...) \
	do { \
		pr_err(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
		if (ecpri_dma_ctx) { \
			DMA_IPC_LOGGING(ecpri_dma_ctx->logbuf, \
				DRV_NAME " %s:%d " fmt, ## args); \
			DMA_IPC_LOGGING(ecpri_dma_ctx->logbuf_low, \
				DRV_NAME " %s:%d " fmt, ## args); \
		} \
	} while (0)

#define DMAERR_RL(fmt, args...) \
	do { \
		pr_err_ratelimited_ecpri_dma(DRV_NAME " %s:%d " fmt, __func__,\
		__LINE__, ## args);\
		if (ecpri_dma_ctx) { \
			DMA_IPC_LOGGING(ecpri_dma_ctx->logbuf, \
				DRV_NAME " %s:%d " fmt, ## args); \
			DMA_IPC_LOGGING(ecpri_dma_ctx->logbuf_low, \
				DRV_NAME " %s:%d " fmt, ## args); \
		} \
	} while (0)

/** Enums & Defenitions **/

/* Max ENDP ID for eCPRI DMA */
#define ECPRI_DMA_ENDP_NUM_MAX 74

/* Define for IPC log size */
#define ECPRI_DMA_IPC_LOG_PAGES 50

/* Define for GSI event RP allocation */
#define ECPRI_DMA_GSI_EVENT_RP_SIZE 8

/* Define num of EEs supported */
#define ECPRI_DMA_NUM_EE (ECPRI_DMA_EE_PF + 1)

/* Define for max tramit length*/
#define ECPRI_DMA_DP_MAX_DESC (20)

#define ECPRI_CLK_FREQ(x) (x * 1000 * 1000UL)

/* Define for DMA max nominal clock */
#define ECPRI_DMA_CLK_NOM_MAX (ECPRI_CLK_FREQ(466.50))

/* Define for GSI max nominal clock */
#define ECPRI_GSI_FAST_CLK_NOM_MAX (ECPRI_CLK_FREQ(500))
#define ECPRI_GSI_FAST_DIV2_CLK_NOM_MAX (ECPRI_CLK_FREQ(250))

/* Define for GCC max nominal clock */
#define GCC_AHB_CLK_NOM_MAX (ECPRI_CLK_FREQ(100))
#define GCC_XO_CLK_NOM_MAX (ECPRI_CLK_FREQ(19.20))

enum ecpri_dma_smmu_cb_type {
	ECPRI_DMA_SMMU_CB_AP,
	ECPRI_DMA_SMMU_CB_MAX
};

/** Structers **/

struct dma_xbar_cfg_tid {
	u32 dst_link_idx : 2;
	u32 dst_port_dx : 2;
	u32 reserved0 : 28;
};

/* Union definition xbar_cfg_tid */
union dma_xbar_cfg_tid_u {
	struct dma_xbar_cfg_tid def;
	u32 value;
};

/**
 * struct dma_gsi_ep_config - DMA GSI endpoint configurations
 *
 * @valid: ENDP is being used
 * @dma_gsi_chan_num: GSI channel number
 * @dma_if_tlv: number of DMA_IF TLV
 * @dma_if_aos: number of DMA_IF AOS
 * @ee: Execution environment
 * @prefetch_mode: Prefetch mode to be used
 * @prefetch_threshold: Prefetch empty level threshold.
 *  relevant for smart and free prefetch modes.
 * @dir: ENDP direction.
 * @stream_mode: ENDP stream mode - M2M \ S2M \ M2S.
 * @is_nfapi: True if ENDP is nFAPI.
 * @dest: For M2M SRC ENDPs matching DEST, for M2S ENDP matching DEST Stream.
 * @is_exception: True if ENDP is exception endp.
 * @nfapi_dest_vm_id: For M2S nFAPI ENDP matching VM ID.
 */
struct dma_gsi_ep_config {
	bool valid;
	int dma_gsi_chan_num;
	int dma_if_tlv;
	int dma_if_aos;
	enum ecpri_dma_ees ee;
	enum gsi_prefetch_mode prefetch_mode;
	uint8_t prefetch_threshold;
	enum ecpri_dma_endp_dir dir;
	enum ecpri_dma_endp_stream_mode stream_mode;
	union dma_xbar_cfg_tid_u tid;
	bool is_nfapi;
	u32 dest;
	bool is_exception;
	enum ecpri_dma_vm_ids nfapi_dest_vm_id;
};

/**
 * struct ecpri_dma_gsi_ep_mem_info - DMA GSI endpoint memory information
 *
 * @evt_ring_len: ENDP matching GSI event ring length
 * @evt_ring_base_addr: ENDP matching GSI event ring physical base address
 * @evt_ring_base_vaddr: ENDP matching GSI event ring virtual base address
 * @chan_ring_len: ENDP matching GSI transfer ring length
 * @chan_ring_base_addr: ENDP matching GSI transfer ring physical base address
 * @chan_ring_base_vaddr: ENDP matching GSI transfer ring virtual base address
 * @evt_ring_rp_addr: ENDP matching GSI event ring RP physical base address
 * @evt_ring_rp_vaddr: ENDP matching GSI event ring RP virtual base address
 */
struct ecpri_dma_gsi_ep_mem_info {
	u32 evt_ring_len;
	u64 evt_ring_base_addr;
	void *evt_ring_base_vaddr;
	u32 chan_ring_len;
	u64 chan_ring_base_addr;
	void *chan_ring_base_vaddr;
	u64 evt_ring_rp_addr;
	void *evt_ring_rp_vaddr;
};

/**
 * struct ecpri_dma_exception_stats - DMA exception statistics
 *
 * @num_of_pkts_recieved: Number of packets which were routed to exception ENDP
 * @num_of_bytes_recieved: Total number of bytes of these packets
 */
struct ecpri_dma_exception_stats {
	u32 num_of_pkts_recieved;
	u32 num_of_bytes_recieved;
};

/**
 * struct ecpri_dma_endp_context - DMA end point context
 * @valid: flag indicating id EP context is valid
 * @endp_id: ID of current endp
 * @hdl: connection handle
 * @gsi_ep_cfg: GSI EP configuration
 * @gsi_chan_hdl: EP's GSI channel handle
 * @gsi_evt_ring_hdl: EP's GSI channel event ring handle
 * @gsi_mem_info: EP's GSI channel rings info
 * @chan_scratch: EP's GSI channel scratch info
 * @dst_endp_index: destination EP index
 * @notify_comp: client specific CB for ENDP completion events
 *						notification
 * @curr_polling_state: Current chanel polling mode, relavent only for Rx CHs
 * @ring_length: EP's GSI channel ring size.
 * @disconnect_in_progress: Indicates client disconnect in progress.
 * @int_modt: EP interrupt moderation timer configuration
 * @int_modc: EP interrupt moderation counter configuration
 * @buff_size: EP buffer size
 * @page_order: EP buffer size page order
 * @use_msi: True if EP uses MSI interrupts
 * @is_over_ocie: True if EP rings are over PCIe
 * @is_endp_mhi_l2: True if in MHI protocol for L2
 * @eventless_endp: True if ENDP doesn't have an event ring
 * @outstanding_pkt_list: List of currently outstanding packets
 * @curr_outstanding_num: Size of the outstanding packets list
 * @completed_pkt_list: List of currently completed packets waiting to be
 *						handled
 * @curr_completed_num: Size of the completed packets list
 * @available_outstanding_pkts_cache: Cache for outstanding pkts wrappers alloc
 * @available_outstanding_pkts_list: List of pkt wrappers ready to be used
 * @avail_outstanding_pkts: Size of the ready pkt wrappers list
 * @available_exception_pkts_cache: Cache for exception pkts allocation,
 *									relavent only for exception endp
 * @available_exception_buffs_cache: Cache for exception buffers allocation,
 *									 relavent only for exception endp
 * @xmit_eot_cnt: atomic var to keep track of completed packets
 * @total_pkts_recv: EP statistics regarding number of packets received
 * @total_pkts_sent: EP statistics regarding number of packets sent
 * @total_bytes_sent: EP statistics regarding number of bytes sent
 * @tasklet: EP tasklet to handle completion notification
 * @spinlock: EP lock to sync accesses to EP resources
 * @l2_mhi_channel_ptr: Pointer to the MHI Channel CTX
 * @mask: Mask indicating number of messages assigned by the host to device
 *
 */
struct ecpri_dma_endp_context {
	bool valid;
	u32 endp_id;
	u32 hdl;
	const struct dma_gsi_ep_config *gsi_ep_cfg;
	unsigned long gsi_chan_hdl;
	unsigned long gsi_evt_ring_hdl;
	struct ecpri_dma_gsi_ep_mem_info gsi_mem_info;
	union __packed gsi_channel_scratch chan_scratch;
	u32 dst_endp_index;
	void (*notify_comp)(
		struct ecpri_dma_endp_context *endp,
		struct ecpri_dma_pkt_completion_wrapper **comp_pkt,
		u32 num_of_completed);
	atomic_t curr_polling_state;
	int ring_length;
	atomic_t disconnect_in_progress;
	u32 int_modt;
	u32 int_modc;
	u32 buff_size;
	u32 page_order;
	bool use_msi;
	bool is_over_pcie;
	bool disable_overflow_event;
	bool is_endp_mhi_l2;
	bool eventless_endp;
	struct list_head outstanding_pkt_list;
	u32 curr_outstanding_num;
	struct list_head completed_pkt_list;
	u32 curr_completed_num;
	struct kmem_cache *available_outstanding_pkts_cache;
	struct list_head available_outstanding_pkts_list;
	u32 avail_outstanding_pkts;
	struct kmem_cache *available_exception_pkts_cache;
	struct kmem_cache *available_exception_buffs_cache;
	atomic_t xmit_eot_cnt;

	u32 total_pkts_recv;
	u32 total_pkts_sent;
	u32 total_bytes_sent;

	struct tasklet_struct tasklet;
	spinlock_t spinlock;
	void* l2_mhi_channel_ptr;
};

/**
 * struct ecpri_dma_smmu_cb_ctx - DMA SMMU context bank context
 * @valid: is CB valid
 * @dev: device associated with the CB
 * @iommu_domain: the IO mmu domain associated with the CB
 * @va_start: CB virtual address start
 * @va_size: CB virtual address space size
 * @va_end: CB virtual address end of range
 * @shared: is CB shared
 * @is_cache_coherent: Does CB supprt cache coherency
 */
struct ecpri_dma_smmu_cb_ctx {
	bool valid;
	struct device *dev;
	struct iommu_domain *iommu_domain;
	u32 va_start;
	u32 va_size;
	u32 va_end;
	bool shared;
	bool is_cache_coherent;
};

struct ecpri_dma_exception_replenish_work_wrap {
	struct work_struct replenish_work;
	u32 num_to_replenish;
	void *private_data;
};

struct ecpri_dma_clks {
	struct clk* gcc_aggre_noc_ecpri_dma;
	struct clk* gcc_ecpri_noc_ahb;
	struct clk* gcc_ecpri_ahb;
	struct clk* gcc_ecpri_xo;
	struct clk* ecpri_cg_clk;
	struct clk* dma_clk;
	struct clk* dma_noc_clk;
	struct clk* dma_fast_clk;
	struct clk* dma_fast_div2_clk;
	struct clk* dma_fast_div2_noc_clk;
	struct clk* dma_nfapi_axi_clk;
};

struct ecpri_dma_icc_paths {
	struct icc_path* dma_to_ddr;
	struct icc_path* gsi_to_ddr;
	struct icc_path* appss_to_dma;
};

 /**
  * struct ecpri_dma_context - DMA context
  * @lock: General lock to protect sensetive resources
  * @logbuf: ipc log buffer for high priority messages
  * @logbuf_low: ipc log buffer for low priority messages
  * @ecpri_hw_ver: type of eCPRI HW type (e.g. eCPRI 1.0 etc')
  * @hw_flavor: current HW flavor (Ru \ Du - L2 \ Du - PCIe)
  * @master_pdev: platform device used to register with the kernel
  * @pdev: device used for DTSi parsing and memory allocations
  * @num_smmu_cb_probed: number of smmu already probed
  * @max_num_smmu_cb: number of smmu s1 cb supported
  * @s1_bypass_arr: smmu bypass array
  * @mmio: memory mapping of eCPRI register addresses
  * @ecpri_dma_cfg_offset: offset from eCPRI_DMA_WRAPPER_BASE to DMA registers
  * @ecpri_dma_mem_base: DMA registers memory base address
  * @ecpri_dma_mem_size: DMA registers memory size
  * @gsi_mem_base: GSI registers memory base address
  * @gsi_mem_size: GSI registers memory size
  * @pcie_intcntrlr_mem_base: PCIe registers memory base address
  * @pcie_intcntrlr_mem_size: PCIe registers memory size
  * @ecpri_dma_irq: DMA IRQ line number
  * @gsi_irq: GSI IRQ line numbers
  * @pcie_irq: PCIe IRQ line number
  * @use_uefi_boot: use uefi loading for GSI FW
  * @dma_initialization_complete:	True once DMA init is complete and DMA is
  *									ready to be used
  * @ecpri_dma_ready_cb_list: List of CBs to invoke once DMA driver is ready
  * @ee: Execution Enviorment ID
  * @testing_mode: True if driver is in testing mode
  * @is_device_crashed: True if device has crashed
  * @gsi_ver: GSI HW version
  * @gsi_dev_hdl: GSI device handle
  * @ecpri_dma_num_endps: Number of endps
  * @endp_map: ENDP configuration mapping matching to current flavor & version
  * @endp_ctx: ENDP context array
  * @exception_endp: Exception ENDP number
  * @ecpri_dma_exception_wq: WQ to handle Exception replenish
  * @exception_stats: Exception statistics
  *
  */
struct ecpri_dma_context {
	struct mutex lock;
	void *logbuf;
	void *logbuf_low;
	enum ecpri_hw_ver ecpri_hw_ver;
	enum ecpri_hw_flavor hw_flavor;
	struct platform_device *master_pdev;
	struct device *pdev;
	u32 num_smmu_cb_probed;
	u32 max_num_smmu_cb;
	bool s1_bypass_arr[ECPRI_DMA_SMMU_CB_MAX];
	void __iomem *mmio;
	u32 ecpri_dma_cfg_offset;
	u32 ecpri_dma_mem_base;
	u32 ecpri_dma_mem_size;
	u32 gsi_mem_base;
	u32 gsi_mem_size;
	u32 pcie_intcntrlr_mem_base;
	u32 pcie_intcntrlr_mem_size;
	u32 ecpri_dma_irq;
	u32 gsi_irq[ECPRI_DMA_NUM_EE];
	u32 pcie_irq;
	bool use_uefi_boot;
	bool dma_initialization_complete;
	struct list_head ecpri_dma_ready_cb_list;
	u32 ee;
	bool testing_mode;
	bool is_device_crashed;
	enum gsi_ver gsi_ver;
	unsigned long gsi_dev_hdl;
	u32 ecpri_dma_num_endps;
	const struct dma_gsi_ep_config *endp_map;
	struct ecpri_dma_endp_context endp_ctx[ECPRI_DMA_ENDP_NUM_MAX];
	u32 exception_endp;
	struct workqueue_struct *ecpri_dma_exception_wq;
	struct ecpri_dma_exception_stats exception_stats;
	struct ecpri_dma_clks clks;
	struct ecpri_dma_icc_paths icc_paths;
};

/**
  * struct ecpri_dma_plat_drv_res - DMA platform results
  * Used to parse DTSi
  */
struct ecpri_dma_plat_drv_res {
	enum ecpri_hw_ver ecpri_hw_ver;
	enum ecpri_hw_flavor hw_flavor;
	u32 ecpri_dma_mem_base;
	u32 ecpri_dma_mem_size;
	u32 gsi_mem_base;
	u32 gsi_mem_size;
	u32 pcie_intcntrlr_mem_base;
	u32 pcie_intcntrlr_mem_size;
	u32 ecpri_dma_irq;
	u32 gsi_irq[ECPRI_DMA_NUM_EE];
	u32 pcie_irq;
	u32 ee;
	u32 max_num_smmu_cb;
	u32 ecpri_dma_cfg_offset;
	bool use_uefi_boot;
	bool testing_mode;
};

/**
 * struct ecpri_dma_ready - eCPRI DMA readiness parameters
 *
 * @notify: eCPRI DMA client ready callback notifier
 * @userdata: userdata for DMA ready cb
 */
struct ecpri_dma_ready {
	ecpri_dma_ready_cb notify;
	void *userdata;
};

/**
 * struct ecpri_dma_ready_cb_wrapper - List node for DMA ready CBs
 *
 * @link: List member
 * @info: DMA ready CB parameters
 */
struct ecpri_dma_ready_cb_wrapper {
	struct list_head link;
	struct ecpri_dma_ready info;
};

/** Functions **/

typedef void (*client_notify_comp)(
	struct ecpri_dma_endp_context *endp,
	struct ecpri_dma_pkt_completion_wrapper **comp_pkt,
	u32 num_of_completed);

/**
 * ecpri_dma_register_ready_cb() - Register ready CBs with DMA driver
 * @ecpri_dma_ready_cb:	[in] CB function to be called once DMA driver is ready
 * @user_data: [in] User data to be sent when invoking the CB function
 *
 * Returns:	0 on success, -EEXIST if driver is already ready,
 * other negative on failure
 */
int ecpri_dma_register_ready_cb(void (*ecpri_dma_ready_cb)(void *user_data),
	void *user_data);

//TODO: Add documentation to all suspend functions below
int ecpri_dma_ap_suspend(struct device *dev);
int ecpri_dma_ap_resume(struct device *dev);

void *ecpri_dma_get_ipc_logbuf(void);
void *ecpri_dma_get_ipc_logbuf_low(void);

int ecpri_dma_alloc_endp(int endp_id, u32 ring_length,
	struct ecpri_dma_moderation_config *mod_cfg,
	bool is_over_pcie,
	client_notify_comp notify_comp);
int ecpri_dma_start_endp(struct ecpri_dma_endp_context *endp_cfg);
int ecpri_dma_stop_endp(struct ecpri_dma_endp_context *endp_cfg);
int ecpri_dma_reset_endp(struct ecpri_dma_endp_context *endp_cfg);
int ecpri_dma_dealloc_endp(struct ecpri_dma_endp_context *endp_cfg);


#endif /* _ECPRI_DMA_I_H_ */
