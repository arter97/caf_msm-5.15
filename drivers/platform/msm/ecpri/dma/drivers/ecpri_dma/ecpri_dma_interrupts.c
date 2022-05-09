/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/interrupt.h>
#include "ecpri_dma_i.h"
#include "ecpri_dma_interrupts.h"
#include "dmahal.h"

#define INTERRUPT_WORKQUEUE_NAME "ecpri_dma_interrupt_wq"
#define DMA_AGG_BUSY_TIMEOUT (msecs_to_jiffies(5))

struct ecpri_dma_interrupt_info {
	ecpri_dma_irq_handler_t handler;
	enum ecpri_dma_irq_type interrupt;
	void *private_data;
	bool deferred_flag;
};

struct ecpri_dma_interrupt_work_wrap {
	struct work_struct interrupt_work;
	ecpri_dma_irq_handler_t handler;
	enum ecpri_dma_irq_type interrupt;
	void *private_data;
};

static struct ecpri_dma_interrupt_info ecpri_dma_interrupt_to_cb[DMA_IRQ_MAX];
static struct workqueue_struct *ecpri_dma_interrupt_wq;
static u32 ecpri_dma_ee;
static spinlock_t suspend_wa_lock;
static void ecpri_dma_process_interrupts(bool isr_context);

static void ecpri_dma_deferred_interrupt_work(struct work_struct *work)
{
	struct ecpri_dma_interrupt_work_wrap *work_data =
		container_of(work,
			struct ecpri_dma_interrupt_work_wrap,
			interrupt_work);
	DMADBG("call handler from workq for interrupt %d...\n",
		work_data->interrupt);
	work_data->handler(work_data->interrupt, work_data->private_data);
	kfree(work_data);
}

static int ecpri_dma_handle_interrupt(int irq_num, bool isr_context)
{
	struct ecpri_dma_interrupt_info interrupt_info;
	struct ecpri_dma_interrupt_work_wrap *work_data;
	int res;

	interrupt_info = ecpri_dma_interrupt_to_cb[irq_num];
	if (interrupt_info.handler == NULL) {
		DMAERR("A callback function wasn't set for interrupt num %d\n",
			irq_num);
		return -EINVAL;
	}

	/* Force defer processing if in ISR context. */
	if (interrupt_info.deferred_flag || isr_context) {
		DMADBG_LOW("Defer handling interrupt %d\n",
			interrupt_info.interrupt);
		work_data = kzalloc(sizeof(struct ecpri_dma_interrupt_work_wrap),
			GFP_ATOMIC);
		if (!work_data) {
			DMAERR("failed allocating ecpri_dma_interrupt_work_wrap\n");
			res = -ENOMEM;
			goto fail_alloc_work;
		}
		INIT_WORK(&work_data->interrupt_work,
			ecpri_dma_deferred_interrupt_work);
		work_data->handler = interrupt_info.handler;
		work_data->interrupt = interrupt_info.interrupt;
		work_data->private_data = interrupt_info.private_data;
		queue_work(ecpri_dma_interrupt_wq, &work_data->interrupt_work);

	} else {
		DMADBG_LOW("Handle interrupt %d\n", interrupt_info.interrupt);
		interrupt_info.handler(interrupt_info.interrupt,
			interrupt_info.private_data);
	}

	return 0;

fail_alloc_work:
	return res;
}

static void ecpri_dma_process_interrupts(bool isr_context)
{
	u32 reg;
	u32 bmsk;
	u32 i = 0;
	u32 en;
	unsigned long flags;

	DMADBG_LOW("Enter isr_context=%d\n", isr_context);

	spin_lock_irqsave(&suspend_wa_lock, flags);
	en = ecpri_dma_hal_read_reg_n(ECPRI_IRQ_EN_EE_n, ecpri_dma_ee);
	reg = ecpri_dma_hal_read_reg_n(ECPRI_IRQ_STTS_EE_n, ecpri_dma_ee);
	while (en & reg) {
		DMADBG_LOW("en=0x%x reg=0x%x\n", en, reg);
		bmsk = 1;
		for (i = 0; i < DMA_IRQ_MAX; i++) {
			DMADBG_LOW("Check irq number %d\n", i);
			if (en & reg & bmsk) {
				DMADBG_LOW("Irq number %d asserted\n", i);
				/*
				 * handle the interrupt with spin_lock
				 * unlocked to avoid calling client in atomic
				 * context. mutual exclusion still preserved
				 * as the read/clr is done with spin_lock
				 * locked.
				 */
				spin_unlock_irqrestore(&suspend_wa_lock, flags);
				ecpri_dma_handle_interrupt(i, isr_context);
				spin_lock_irqsave(&suspend_wa_lock, flags);

				ecpri_dma_hal_write_reg_n(ECPRI_IRQ_CLR_EE_n, ecpri_dma_ee, bmsk);
			}
			bmsk = bmsk << 1;
		}

		reg = ecpri_dma_hal_read_reg_n(ECPRI_IRQ_STTS_EE_n, ecpri_dma_ee);
		/* since the suspend interrupt HW bug we must
		 * read again the EN register, otherwise the while is endless
		   TODO: Check if this limitation exists for DMA also*/
		en = ecpri_dma_hal_read_reg_n(ECPRI_IRQ_EN_EE_n, ecpri_dma_ee);
	}

	spin_unlock_irqrestore(&suspend_wa_lock, flags);
	DMADBG_LOW("Exit\n");
}

static irqreturn_t ecpri_dma_isr(int irq, void *ctxt)
{
	DMADBG_LOW("Enter\n");

	ecpri_dma_process_interrupts(true);
	DMADBG_LOW("Exit\n");

	return IRQ_HANDLED;
}

irq_handler_t ecpri_dma_get_isr(void)
{
	return ecpri_dma_isr;
}

/**
 * ecpri_dma_add_interrupt_handler() - Adds handler to an interrupt type
 * @interrupt:		Interrupt type
 * @handler:		The handler to be added
 * @deferred_flag:	whether the handler processing should be deferred in
 *					a workqueue
 * @private_data:	the client's private data
 *
 * Adds handler to an interrupt type and enable the specific bit
 * in IRQ_EN register, associated interrupt in IRQ_STTS register will be enabled
 */
int ecpri_dma_add_interrupt_handler(enum ecpri_dma_irq_type interrupt,
	ecpri_dma_irq_handler_t handler,
	bool deferred_flag,
	void *private_data)
{
	u32 val, bmsk;

	DMADBG("interrupt_enum(%d)\n", interrupt);
	if (interrupt < DMA_BAD_SNOC_ACCESS_IRQ ||
		interrupt >= DMA_IRQ_MAX) {
		DMAERR("invalid interrupt number %d\n", interrupt);
		return -EINVAL;
	}

	ecpri_dma_interrupt_to_cb[interrupt].deferred_flag = deferred_flag;
	ecpri_dma_interrupt_to_cb[interrupt].handler = handler;
	ecpri_dma_interrupt_to_cb[interrupt].private_data = private_data;
	ecpri_dma_interrupt_to_cb[interrupt].interrupt = interrupt;

	val = ecpri_dma_hal_read_reg_n(ECPRI_IRQ_EN_EE_n, ecpri_dma_ee);
	DMADBG("read ECPRI_IRQ_EN_EE_n register. reg = %d\n", val);
	bmsk = 1 << interrupt;
	val |= bmsk;
	ecpri_dma_hal_write_reg_n(ECPRI_IRQ_EN_EE_n, ecpri_dma_ee, val);
	DMADBG("wrote ECPRI_IRQ_EN_EE_n register. reg = %d\n", val);

	return 0;
}

/**
 * ecpri_dma_remove_interrupt_handler() - Removes handler to an interrupt type
 * @interrupt:		Interrupt type
 *
 * Removes the handler and disable the specific bit in IRQ_EN register
 */
int ecpri_dma_remove_interrupt_handler(enum ecpri_dma_irq_type interrupt)
{
	u32 val, bmsk;

	if (interrupt < DMA_BAD_SNOC_ACCESS_IRQ ||
		interrupt >= DMA_IRQ_MAX) {
		DMAERR("invalid interrupt number %d\n", interrupt);
		return -EINVAL;
	}

	kfree(ecpri_dma_interrupt_to_cb[interrupt].private_data);
	ecpri_dma_interrupt_to_cb[interrupt].deferred_flag = false;
	ecpri_dma_interrupt_to_cb[interrupt].handler = NULL;
	ecpri_dma_interrupt_to_cb[interrupt].private_data = NULL;
	ecpri_dma_interrupt_to_cb[interrupt].interrupt = -1;

	val = ecpri_dma_hal_read_reg_n(ECPRI_IRQ_EN_EE_n, ecpri_dma_ee);
	bmsk = 1 << interrupt;
	val &= ~bmsk;
	ecpri_dma_hal_write_reg_n(ECPRI_IRQ_EN_EE_n, ecpri_dma_ee, val);

	return 0;
}

/**
 * ecpri_dma_interrupts_init() - Initialize the DMA interrupts framework
 * @ecpri_dma_irq:	The interrupt number to allocate
 * @ee:				Execution environment
 * @ecpri_dma_dev:	The basic device structure representing the DMA driver
 *
 * - Initialize the ecpri_dma_interrupt_to_cb array
 * - Clear interrupts status
 * - Register the ecpri_dma interrupt handler - ecpri_dma_isr
 * - Enable apps processor wakeup by DMA interrupts
 */
int ecpri_dma_interrupts_init(u32 ecpri_dma_irq, u32 ee,
	struct device *ecpri_dma_dev)
{
	int idx;
	int res = 0;

	ecpri_dma_ee = ee;
	for (idx = 0; idx < DMA_IRQ_MAX; idx++) {
		ecpri_dma_interrupt_to_cb[idx].deferred_flag = false;
		ecpri_dma_interrupt_to_cb[idx].handler = NULL;
		ecpri_dma_interrupt_to_cb[idx].private_data = NULL;
		ecpri_dma_interrupt_to_cb[idx].interrupt = -1;
	}

	ecpri_dma_interrupt_wq = create_singlethread_workqueue(
		INTERRUPT_WORKQUEUE_NAME);
	if (!ecpri_dma_interrupt_wq) {
		DMAERR("workqueue creation failed\n");
		return -ENOMEM;
	}

	/*
	 * NOTE:
	 *
	 *  We'll only register an isr on non-emulator (ie. real UE)
	 *  systems.
	 *
	 *  On the emulator, emulator_soft_irq_isr() will be calling
	 *  ecpri_dma_isr, so hence, no isr registration here, and instead,
	 *  we'll pass the address of ecpri_dma_isr to the gsi layer where
	 *  emulator interrupts are handled...
	 */
	if (!ecpri_dma_ctx->testing_mode) {
		res = request_irq(ecpri_dma_irq, (irq_handler_t)ecpri_dma_isr,
			IRQF_TRIGGER_RISING, "ecpri_dma", ecpri_dma_dev);
		if (res) {
			DMAERR(
				"fail to register DMA IRQ handler irq=%d\n",
				ecpri_dma_irq);
			destroy_workqueue(ecpri_dma_interrupt_wq);
			ecpri_dma_interrupt_wq = NULL;
			return -ENODEV;
		}
		DMADBG("DMA IRQ handler irq=%d registered\n", ecpri_dma_irq);

		res = enable_irq_wake(ecpri_dma_irq);
		if (res)
			DMAERR("fail to enable DMA IRQ wakeup irq=%d res=%d\n",
				ecpri_dma_irq, res);
		else
			DMADBG("DMA IRQ wakeup enabled irq=%d\n", ecpri_dma_irq);
	}
	spin_lock_init(&suspend_wa_lock);
	return 0;
}

/**
 * ecpri_dma_interrupts_destroy() - Destroy the DMA interrupts framework
 * @ecpri_dma_irq:	The interrupt number to allocate
 * @ecpri_dma_dev:	The basic device structure representing the DMA driver
 *
 * - Disable apps processor wakeup by DMA interrupts
 * - Unregister the ecpri_dma interrupt handler - ecpri_dma_isr
 * - Destroy the interrupt workqueue
 */
void ecpri_dma_interrupts_destroy(u32 ecpri_dma_irq,
				  struct device *ecpri_dma_dev)
{
	if (!ecpri_dma_ctx->testing_mode) {
		disable_irq_wake(ecpri_dma_irq);
		free_irq(ecpri_dma_irq, ecpri_dma_dev);
	}
	destroy_workqueue(ecpri_dma_interrupt_wq);
	ecpri_dma_interrupt_wq = NULL;
}
