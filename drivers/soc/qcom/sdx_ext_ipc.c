// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <sb_notification.h>
#include <linux/kfifo.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/panic_notifier.h>
#include <linux/gpio/consumer.h>

#define STATUS_UP 1
#define STATUS_DOWN 0
#define SB_FIFO_SIZE 8

enum subsys_policies {
	SUBSYS_PANIC = 0,
	SUBSYS_NOP,
};

static const char * const policies[] = {
	[SUBSYS_PANIC] = "PANIC",
	[SUBSYS_NOP] = "NOP",
};

enum gpios {
	STATUS_OUT = 0,
	STATUS_IN,
	WAKEUP_OUT,
	WAKEUP_IN,
	ERRFATAL_OUT,
	ERRFATAL_IN,
	NUM_GPIOS,
};

struct sb_gpio {
	const char *name;
	unsigned long flags;
};

static const struct sb_gpio gpiod_map[] = {
	[STATUS_OUT] = { .name = "qcom,status-out", .flags = GPIOD_OUT_HIGH },
	[STATUS_IN] = { .name = "qcom,status-in", .flags = GPIOD_IN },
	[WAKEUP_OUT] = { .name = "qcom,wakeup-out", .flags = GPIOD_OUT_HIGH },
	[WAKEUP_IN] = { .name = "qcom,wakeup-in", .flags = GPIOD_IN },
	[ERRFATAL_OUT] = { .name = "qcom,errfatal-out", .flags = GPIOD_OUT_HIGH },
	[ERRFATAL_IN] = { .name = "qcom,errfatal-in", .flags = GPIOD_IN },
};

struct gpio_cntrl {
	int gpios[NUM_GPIOS];
	struct gpio_desc *gpdesc[NUM_GPIOS];
	int status_irq;
	int wakeup_irq;
	int err_irq;
	int policy;
	struct device *dev;
	struct mutex policy_lock;
	struct notifier_block panic_blk;
	struct notifier_block err_panic_blk;
	struct notifier_block sideband_nb;
	DECLARE_KFIFO_PTR(st_in_fifo, u8);
	DECLARE_KFIFO_PTR(err_in_fifo, u8);
	wait_queue_head_t st_in_wq;
	wait_queue_head_t err_in_wq;
	struct cdev sb_cdev;
	struct cdev sb_err_cdev;
	dev_t sb_cdev_devid;
	dev_t sb_err_cdev_devid;
	struct class *sb_class;
	struct class *sb_err_class;
};

static ssize_t set_remote_status_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int status;

	if (kstrtoint(buf, 10, &status)) {
		dev_err(dev, "%s: Failed to read status\n", __func__);
		return -EINVAL;
	}

	if (status == STATUS_UP)
		sb_notifier_call_chain(EVENT_REMOTE_STATUS_UP, NULL);
	else if (status == STATUS_DOWN)
		sb_notifier_call_chain(EVENT_REMOTE_STATUS_DOWN, NULL);

	return count;
}
static DEVICE_ATTR_WO(set_remote_status);

static ssize_t policy_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	int ret;
	struct gpio_cntrl *mdm = dev_get_drvdata(dev);

	mutex_lock(&mdm->policy_lock);
	ret = scnprintf(buf, strlen(policies[mdm->policy]) + 1,
						 policies[mdm->policy]);
	mutex_unlock(&mdm->policy_lock);

	return ret;
}

static ssize_t policy_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct gpio_cntrl *mdm = dev_get_drvdata(dev);
	const char *p;
	int i, orig_count = count;

	p = memchr(buf, '\n', count);
	if (p)
		count = p - buf;

	for (i = 0; i < ARRAY_SIZE(policies); i++)
		if (!strncasecmp(buf, policies[i], count)) {
			mutex_lock(&mdm->policy_lock);
			mdm->policy = i;
			mutex_unlock(&mdm->policy_lock);
			return orig_count;
		}

	return -EPERM;
}
static DEVICE_ATTR_RW(policy);

static int sideband_notify(struct notifier_block *nb,
		unsigned long action, void *dev)
{
	struct gpio_cntrl *mdm = container_of(nb,
					struct gpio_cntrl, sideband_nb);

	switch (action) {
	case EVENT_REQUEST_WAKE_UP:
		gpiod_set_value(mdm->gpdesc[WAKEUP_OUT], 1);
		usleep_range(10000, 20000);
		gpiod_set_value(mdm->gpdesc[WAKEUP_OUT], 0);
		break;
	}

	return NOTIFY_OK;
}

static irqreturn_t status_in_hwirq_hdlr(int irq, void *p)
{
	pm_system_wakeup();

	return IRQ_WAKE_THREAD;
}

static irqreturn_t ap_status_change(int irq, void *dev_id)
{
	struct gpio_cntrl *mdm = dev_id;
	int state, ret;
	int active_low = 0;
	char evt;

	if (mdm->gpdesc[STATUS_IN])
		active_low = gpiod_is_active_low(mdm->gpdesc[STATUS_IN]);

	state = gpiod_get_value(mdm->gpdesc[STATUS_IN]);
	if ((!active_low && !state) || (active_low && state)) {
		if (mdm->policy) {
			evt = '0';
			dev_info(mdm->dev, "Remote-end went down, leaving local-end as it is\n");
			sb_notifier_call_chain(EVENT_REMOTE_STATUS_DOWN, NULL);
		} else
			panic("Remote-end went down, panicking local-end\n");
	} else {
		evt = '1';
		dev_info(mdm->dev, "Remote-end went up\n");
		sb_notifier_call_chain(EVENT_REMOTE_STATUS_UP, NULL);
	}

	ret = kfifo_put(&mdm->st_in_fifo, evt);
	if (ret)
		wake_up_interruptible_poll(&mdm->st_in_wq, POLLIN | POLLPRI);
	else
		pr_debug_ratelimited("status-in fifo full, event dropped!\n");

	return IRQ_HANDLED;
}

static irqreturn_t sdx_ext_ipc_wakeup_irq(int irq, void *dev_id)
{
	pr_info("%s: Received\n", __func__);

	sb_notifier_call_chain(EVENT_REMOTE_WOKEN_UP, NULL);

	return IRQ_HANDLED;
}

static irqreturn_t err_in_hwirq_hdlr(int irq, void *p)
{
	pm_system_wakeup();

	return IRQ_WAKE_THREAD;
}

static irqreturn_t ap_err_status_change(int irq, void *dev_id)
{
	struct gpio_cntrl *mdm = dev_id;
	int state, ret;
	int active_low = 0;
	char evt;

	if (mdm->gpdesc[ERRFATAL_IN])
		active_low = gpiod_is_active_low(mdm->gpdesc[ERRFATAL_IN]);

	state = gpiod_get_value(mdm->gpdesc[ERRFATAL_IN]);
	if ((!active_low && !state) || (active_low && state)) {
		if (mdm->policy) {
			evt = '0';
			dev_info(mdm->dev, "Remote-end went down, leaving local-end as it is\n");
			sb_notifier_call_chain(EVENT_REMOTE_STATUS_DOWN, NULL);
		} else
			panic("Remote-end went down, panicking local-end\n");
	} else {
		evt = '1';
		dev_info(mdm->dev, "Remote-end went up\n");
		sb_notifier_call_chain(EVENT_REMOTE_STATUS_UP, NULL);
	}

	ret = kfifo_put(&mdm->err_in_fifo, evt);
	if (ret)
		wake_up_interruptible_poll(&mdm->err_in_wq, POLLIN | POLLPRI);
	else
		pr_debug_ratelimited("errfatal-in fifo full, event dropped!\n");

	return IRQ_HANDLED;
}

static void remove_ipc(struct gpio_cntrl *mdm)
{
	int i;

	for (i = 0; i < NUM_GPIOS; ++i) {
		if (gpio_is_valid(mdm->gpios[i]))
			gpiod_put(mdm->gpdesc[i]);
	}
}

static int setup_ipc(struct gpio_cntrl *mdm)
{
	int i, ret, irq;
	struct device_node *node;

	node = mdm->dev->of_node;

	for (i = 0; i < ARRAY_SIZE(gpiod_map); i++) {
		dev_info(mdm->dev, "%s gpiod_map name[%d]=%s flag[%d]=%d\n",
				__func__, i, gpiod_map[i].name, i, gpiod_map[i].flags);
		mdm->gpdesc[i] = gpiod_get(mdm->dev, gpiod_map[i].name, gpiod_map[i].flags);
		if (IS_ERR(mdm->gpdesc[i])) {
			dev_err(mdm->dev,
			"Failed to get GPIO from DT property info\n");
			ret =  PTR_ERR(mdm->gpdesc[i]);
			mdm->gpios[i] = -1;
		} else {
			mdm->gpios[i] = desc_to_gpio(mdm->gpdesc[i]);
			dev_info(mdm->dev, "%s gpiod_get success & desc_to_gpio gpios[%d]=%d\n",
					__func__, i, mdm->gpios[i]);
		}
	}

	if (mdm->gpios[STATUS_IN] >= 0) {
		gpiod_direction_input(mdm->gpdesc[STATUS_IN]);

		irq = gpiod_to_irq(mdm->gpdesc[STATUS_IN]);
		if (irq < 0) {
			dev_err(mdm->dev, "bad STATUS_IN IRQ resource\n");
			return irq;
		}
		mdm->status_irq = irq;
	} else
		dev_info(mdm->dev, "STATUS_IN not used\n");

	if (mdm->gpios[STATUS_OUT] >= 0)
		gpiod_direction_output(mdm->gpdesc[STATUS_OUT], 1);
	else
		dev_info(mdm->dev, "STATUS_OUT not used\n");

	if (mdm->gpios[WAKEUP_OUT] >= 0)
		gpiod_direction_output(mdm->gpdesc[WAKEUP_OUT], 0);
	else
		dev_info(mdm->dev, "WAKEUP_OUT not used\n");

	if (mdm->gpios[WAKEUP_IN] >= 0) {
		gpiod_direction_input(mdm->gpdesc[WAKEUP_IN]);

		irq = gpiod_to_irq(mdm->gpdesc[WAKEUP_IN]);
		if (irq < 0) {
			dev_err(mdm->dev, "bad WAKEUP_IN IRQ resource\n");
			return irq;
		}
		mdm->wakeup_irq = irq;
	} else
		dev_info(mdm->dev, "WAKEUP_IN not used\n");

	if (mdm->gpios[ERRFATAL_OUT] >= 0)
		gpiod_direction_output(mdm->gpdesc[ERRFATAL_OUT], 1);
	else
		dev_info(mdm->dev, "ERRFATAL_OUT not used\n");

	if (mdm->gpios[ERRFATAL_IN] >= 0) {
		gpiod_direction_input(mdm->gpdesc[ERRFATAL_IN]);

		irq = gpiod_to_irq(mdm->gpdesc[ERRFATAL_IN]);
		if (irq < 0) {
			dev_err(mdm->dev, "bad ERRFATAL_IN IRQ resource\n");
			return irq;
		}
		mdm->err_irq = irq;
	} else
		dev_info(mdm->dev, "ERRFATAL_IN not used\n");

	return 0;
}

static int sdx_ext_ipc_panic(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	struct gpio_cntrl *mdm = container_of(this,
					struct gpio_cntrl, panic_blk);

	gpiod_set_value(mdm->gpdesc[STATUS_OUT], 0);

	return NOTIFY_DONE;
}

static int sdx_ext_ipc_err_panic(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	struct gpio_cntrl *mdm = container_of(this,
			struct gpio_cntrl, err_panic_blk);

	gpiod_set_value(mdm->gpdesc[ERRFATAL_OUT], 0);

	return NOTIFY_DONE;
}

static irqreturn_t hw_irq_handler(int irq, void *p)
{
	pm_system_wakeup();

	return IRQ_WAKE_THREAD;
}

static int sb_open(struct inode *inodp, struct file *filp)
{
	struct gpio_cntrl *mdm;

	mdm = container_of(inodp->i_cdev, struct gpio_cntrl, sb_cdev);
	filp->private_data = mdm;

	return 0;
}

static int sb_release(struct inode *inodp, struct file *filp)
{
	struct gpio_cntrl *mdm = filp->private_data;

	wake_up_interruptible_all(&mdm->st_in_wq);

	return 0;
}

/*
 * If the line has been toggled before user space read the gpio,
 * return it. Otherwise return current state of the gpio. The spinlock
 * ensures if more than one thread is reading, they observe latest
 * status of the fifo as it is consumable.
 */
static ssize_t sb_read(struct file *filp,
		char __user *buf, size_t count, loff_t *off)
{
	char evt[SB_FIFO_SIZE];
	int ret, to_copy, available, state;
	struct gpio_cntrl *mdm = filp->private_data;

	if (!buf || count < 1)
		return -EINVAL;

	spin_lock(&mdm->st_in_wq.lock);

	if (kfifo_is_empty(&mdm->st_in_fifo)) {
		spin_unlock(&mdm->st_in_wq.lock);
		state = gpiod_get_value(mdm->gpdesc[STATUS_IN]);
		evt[0] = state ? '1' : '0';
		if (copy_to_user(buf, evt, 1))
			return -EFAULT;
		return 1;
	}

	available = SB_FIFO_SIZE - kfifo_avail(&mdm->st_in_fifo);
	if (available < count)
		to_copy = available;
	else
		to_copy = count;

	ret = kfifo_out(&mdm->st_in_fifo, evt, to_copy);

	spin_unlock(&mdm->st_in_wq.lock);

	if (ret < to_copy)
		return -EIO;

	if (copy_to_user(buf, evt, to_copy))
		return -EFAULT;

	return to_copy;
}

static unsigned int sb_poll(struct file *filp,
			struct poll_table_struct *pt)
{
	unsigned int events = 0;
	struct gpio_cntrl *mdm = filp->private_data;

	poll_wait(filp, &mdm->st_in_wq, pt);

	spin_lock(&mdm->st_in_wq.lock);

	if (!kfifo_is_empty(&mdm->st_in_fifo))
		events = POLLIN | POLLPRI;

	spin_unlock(&mdm->st_in_wq.lock);

	return events;
}

static const struct file_operations sb_fileops = {
	.open = sb_open,
	.release = sb_release,
	.read = sb_read,
	.poll = sb_poll,
	.owner = THIS_MODULE,
};

static int sb_err_open(struct inode *inodp, struct file *filp)
{
	struct gpio_cntrl *mdm;

	mdm = container_of(inodp->i_cdev, struct gpio_cntrl, sb_err_cdev);
	filp->private_data = mdm;

	return 0;
}

static int sb_err_release(struct inode *inodp, struct file *filp)
{
	struct gpio_cntrl *mdm = filp->private_data;

	wake_up_interruptible_all(&mdm->err_in_wq);

	return 0;
}

/*
 * If the line has been toggled before user space read the gpio,
 * return it. Otherwise return current state of the gpio. The spinlock
 * ensures if more than one thread is reading, they observe latest
 * status of the fifo as it is consumable.
 */
static ssize_t sb_err_read(struct file *filp,
		char __user *buf, size_t count, loff_t *off)
{
	char evt[SB_FIFO_SIZE];
	int ret, to_copy, available, state;
	struct gpio_cntrl *mdm = filp->private_data;

	if (!buf || count < 1)
		return -EINVAL;

	spin_lock(&mdm->err_in_wq.lock);

	if (kfifo_is_empty(&mdm->err_in_fifo)) {
		spin_unlock(&mdm->err_in_wq.lock);
		state = gpiod_get_value(mdm->gpdesc[ERRFATAL_IN]);
		evt[0] = state ? '1' : '0';
		if (copy_to_user(buf, evt, 1))
			return -EFAULT;
		return 1;
	}

	available = SB_FIFO_SIZE - kfifo_avail(&mdm->err_in_fifo);
	if (available < count)
		to_copy = available;
	else
		to_copy = count;

	ret = kfifo_out(&mdm->err_in_fifo, evt, to_copy);

	spin_unlock(&mdm->err_in_wq.lock);

	if (ret < to_copy)
		return -EIO;

	if (copy_to_user(buf, evt, to_copy))
		return -EFAULT;

	return to_copy;
}

static unsigned int sb_err_poll(struct file *filp,
		struct poll_table_struct *pt)
{
	unsigned int events = 0;
	struct gpio_cntrl *mdm = filp->private_data;

	poll_wait(filp, &mdm->err_in_wq, pt);

	spin_lock(&mdm->err_in_wq.lock);

	if (!kfifo_is_empty(&mdm->err_in_fifo))
		events = POLLIN | POLLPRI;

	spin_unlock(&mdm->err_in_wq.lock);

	return events;
}

static const struct file_operations sb_err_fileops = {
	.open = sb_err_open,
	.release = sb_err_release,
	.read = sb_err_read,
	.poll = sb_err_poll,
	.owner = THIS_MODULE,
};

static int sdx_ext_ipc_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *node;
	struct gpio_cntrl *mdm;
	struct device *sb_dev;
	struct device *sb_err_dev;

	node = pdev->dev.of_node;
	mdm = devm_kzalloc(&pdev->dev, sizeof(*mdm), GFP_KERNEL);
	if (!mdm)
		return -ENOMEM;

	mdm->dev = &pdev->dev;
	ret = setup_ipc(mdm);
	if (ret) {
		dev_err(mdm->dev, "Error setting up gpios\n");
		return ret;
	}

	if (mdm->gpios[STATUS_OUT] >= 0) {
		mdm->panic_blk.notifier_call = sdx_ext_ipc_panic;
		atomic_notifier_chain_register(&panic_notifier_list,
						&mdm->panic_blk);
	}

	if (mdm->gpios[ERRFATAL_OUT] >= 0) {
		mdm->err_panic_blk.notifier_call = sdx_ext_ipc_err_panic;
		atomic_notifier_chain_register(&panic_notifier_list,
						&mdm->err_panic_blk);
	}

	mutex_init(&mdm->policy_lock);
	if (of_property_read_bool(pdev->dev.of_node, "qcom,default-policy-nop"))
		mdm->policy = SUBSYS_NOP;
	else
		mdm->policy = SUBSYS_PANIC;

	ret = device_create_file(mdm->dev, &dev_attr_policy);
	if (ret) {
		dev_err(mdm->dev, "cannot create sysfs attribute\n");
		goto sys_fail;
	}

	ret = device_create_file(mdm->dev, &dev_attr_set_remote_status);
	if (ret) {
		dev_err(mdm->dev, "cannot create sysfs attribute\n");
		goto sys_fail2;
	}

	platform_set_drvdata(pdev, mdm);

	if (mdm->gpios[STATUS_IN] >= 0) {
		ret = kfifo_alloc(&mdm->st_in_fifo, SB_FIFO_SIZE, GFP_KERNEL);
		if (ret < 0) {
			dev_err(mdm->dev,
				 "can't create status in fifo, %d\n", ret);
			goto irq_fail;
		}

		init_waitqueue_head(&mdm->st_in_wq);

		ret = devm_request_threaded_irq(mdm->dev, mdm->status_irq,
				status_in_hwirq_hdlr, ap_status_change,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
				IRQF_ONESHOT | IRQF_NO_SUSPEND,
				"ap status", mdm);
		if (ret < 0) {
			dev_err(mdm->dev,
				 "%s: STATUS_IN IRQ#%d request failed,\n",
				__func__, mdm->status_irq);
			goto irq_fail;
		}

		ret = alloc_chrdev_region(&mdm->sb_cdev_devid, 0, 1,
					"rmt_sys_evt");
		if (ret < 0) {
			dev_err(mdm->dev,
				 "can't allocate major number, %d\n", ret);
			goto fifo_fail;
		}

		cdev_init(&mdm->sb_cdev, &sb_fileops);
		cdev_add(&mdm->sb_cdev, mdm->sb_cdev_devid, 1);

		mdm->sb_class = class_create(THIS_MODULE, "rmt_sys_evt");
		if (IS_ERR(mdm->sb_class)) {
			dev_err(mdm->dev,
				"can't create rmt_sys_evt class, %d\n",
				-ENOMEM);
			goto cdev_fail;
		}

		sb_dev = device_create(mdm->sb_class, &pdev->dev,
						mdm->sb_cdev_devid, mdm,
						"rmt_sys_evt");
		if (IS_ERR(sb_dev)) {
			dev_err(mdm->dev,
				"can't create rmt_sys_evt device, %d\n",
				-ENOMEM);
			goto class_fail;
		}
	}

	if (mdm->gpios[WAKEUP_IN] >= 0) {
		ret = devm_request_threaded_irq(mdm->dev, mdm->wakeup_irq,
				hw_irq_handler, sdx_ext_ipc_wakeup_irq,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT |
				IRQF_NO_SUSPEND, "sdx_ext_ipc_wakeup", mdm);
		if (ret < 0) {
			dev_err(mdm->dev,
				"%s: WAKEUP_IN IRQ#%d request failed,\n",
				__func__, mdm->wakeup_irq);
			goto sbdev_fail;
		}
	}

	if (mdm->gpios[WAKEUP_OUT] >= 0) {
		mdm->sideband_nb.notifier_call = sideband_notify;
		ret = sb_register_evt_listener(&mdm->sideband_nb);
		if (ret) {
			dev_err(mdm->dev,
				"%s: sb_register_evt_listener failed!\n",
				__func__);
			goto sbdev_fail;
		}
	}

	if (mdm->gpios[ERRFATAL_IN] >= 0) {
		ret = kfifo_alloc(&mdm->err_in_fifo, SB_FIFO_SIZE, GFP_KERNEL);
		if (ret < 0) {
			dev_err(mdm->dev,
				"can't create error fatal in fifo, %d\n", ret);
			goto irq_fail;
		}

		init_waitqueue_head(&mdm->err_in_wq);

		ret = devm_request_threaded_irq(mdm->dev, mdm->err_irq,
				err_in_hwirq_hdlr, ap_err_status_change,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
				IRQF_ONESHOT | IRQF_NO_SUSPEND,
				"err fatal ap status", mdm);
		if (ret < 0) {
			dev_err(mdm->dev,
				"%s: ERRFATAL_IN IRQ#%d request failed,\n",
				__func__, mdm->err_irq);
			goto irq_fail;
		}

		ret = alloc_chrdev_region(&mdm->sb_err_cdev_devid, 0, 1,
					"rmt_err_sys_evt");
		if (ret < 0) {
			dev_err(mdm->dev,
				"can't allocate major number, %d\n", ret);
			goto fifo_err_fail;
		}

		cdev_init(&mdm->sb_err_cdev, &sb_err_fileops);
		cdev_add(&mdm->sb_err_cdev, mdm->sb_err_cdev_devid, 1);

		mdm->sb_err_class = class_create(THIS_MODULE, "rmt_err_sys_evt");
		if (IS_ERR(mdm->sb_err_class)) {
			dev_err(mdm->dev,
				"can't create rmt_err_sys_evt class, %d\n",
				-ENOMEM);
			goto cdev_err_fail;
		}

		sb_err_dev = device_create(mdm->sb_err_class, &pdev->dev,
						mdm->sb_err_cdev_devid, mdm,
						"rmt_err_sys_evt");
		if (IS_ERR(sb_err_dev)) {
			dev_err(mdm->dev,
				"can't create rmt_err_sys_evt device, %d\n",
				-ENOMEM);
			goto class_err_fail;
		}
	}

	return 0;

sbdev_fail:
	if (mdm->gpios[STATUS_IN] >= 0)
		device_destroy(mdm->sb_class, mdm->sb_cdev_devid);
	if (mdm->gpios[ERRFATAL_IN] >= 0)
		device_destroy(mdm->sb_err_class, mdm->sb_err_cdev_devid);
class_fail:
	if (mdm->gpios[STATUS_IN] >= 0)
		class_destroy(mdm->sb_class);
cdev_fail:
	if (mdm->gpios[STATUS_IN] >= 0) {
		cdev_del(&mdm->sb_cdev);
		unregister_chrdev_region(mdm->sb_cdev_devid, 1);
	}
fifo_fail:
	if (mdm->gpios[STATUS_IN] >= 0)
		kfifo_free(&mdm->st_in_fifo);
class_err_fail:
	if (mdm->gpios[ERRFATAL_IN] >= 0)
		class_destroy(mdm->sb_err_class);
cdev_err_fail:
	if (mdm->gpios[ERRFATAL_IN] >= 0) {
		cdev_del(&mdm->sb_err_cdev);
		unregister_chrdev_region(mdm->sb_err_cdev_devid, 1);
	}
fifo_err_fail:
	if (mdm->gpios[ERRFATAL_IN] >= 0)
		kfifo_free(&mdm->err_in_fifo);
irq_fail:
sys_fail:
	device_remove_file(mdm->dev, &dev_attr_policy);
sys_fail2:
	if (mdm->gpios[STATUS_OUT] >= 0)
		atomic_notifier_chain_unregister(&panic_notifier_list,
						&mdm->panic_blk);
	if (mdm->gpios[ERRFATAL_OUT] >= 0)
		atomic_notifier_chain_unregister(&panic_notifier_list,
						&mdm->err_panic_blk);
	remove_ipc(mdm);
	device_remove_file(mdm->dev, &dev_attr_set_remote_status);

	return ret;
}

static int sdx_ext_ipc_remove(struct platform_device *pdev)
{
	struct gpio_cntrl *mdm;

	mdm = dev_get_drvdata(&pdev->dev);
	if (mdm->gpios[STATUS_IN] >= 0) {
		disable_irq_wake(mdm->status_irq);
		device_destroy(mdm->sb_class, mdm->sb_cdev_devid);
		class_destroy(mdm->sb_class);
		cdev_del(&mdm->sb_cdev);
		unregister_chrdev_region(mdm->sb_cdev_devid, 1);
		kfifo_free(&mdm->st_in_fifo);
	}
	if (mdm->gpios[STATUS_OUT] >= 0)
		atomic_notifier_chain_unregister(&panic_notifier_list,
						&mdm->panic_blk);
	if (mdm->gpios[ERRFATAL_IN] >= 0) {
		disable_irq_wake(mdm->err_irq);
		device_destroy(mdm->sb_err_class, mdm->sb_err_cdev_devid);
		class_destroy(mdm->sb_err_class);
		cdev_del(&mdm->sb_err_cdev);
		unregister_chrdev_region(mdm->sb_err_cdev_devid, 1);
		kfifo_free(&mdm->err_in_fifo);
	}
	if (mdm->gpios[ERRFATAL_OUT] >= 0)
		atomic_notifier_chain_unregister(&panic_notifier_list,
						&mdm->err_panic_blk);
	remove_ipc(mdm);
	device_remove_file(mdm->dev, &dev_attr_policy);

	return 0;
}

#ifdef CONFIG_PM
static int sdx_ext_ipc_suspend(struct device *dev)
{
	struct gpio_cntrl *mdm = dev_get_drvdata(dev);

	if (mdm->gpios[WAKEUP_IN] >= 0)
		enable_irq_wake(mdm->wakeup_irq);
	if (mdm->gpios[STATUS_IN] >= 0)
		enable_irq_wake(mdm->status_irq);
	if (mdm->gpios[ERRFATAL_IN] >= 0)
		enable_irq_wake(mdm->err_irq);

	return 0;
}

static int sdx_ext_ipc_resume(struct device *dev)
{
	struct gpio_cntrl *mdm = dev_get_drvdata(dev);

	if (mdm->gpios[WAKEUP_IN] >= 0)
		disable_irq_wake(mdm->wakeup_irq);
	if (mdm->gpios[STATUS_IN] >= 0)
		disable_irq_wake(mdm->status_irq);
	if (mdm->gpios[ERRFATAL_IN] >= 0)
		disable_irq_wake(mdm->err_irq);

	return 0;
}

static const struct dev_pm_ops sdx_ext_ipc_pm_ops = {
	.suspend        =    sdx_ext_ipc_suspend,
	.resume         =    sdx_ext_ipc_resume,
};
#endif

static const struct of_device_id sdx_ext_ipc_of_match[] = {
	{ .compatible = "qcom,sdx-ext-ipc"},
	{ .compatible = "qcom,sa525m-idp"},
	{},
};

static struct platform_driver sdx_ext_ipc_driver = {
	.probe		= sdx_ext_ipc_probe,
	.remove		= sdx_ext_ipc_remove,
	.driver = {
		.name	= "sdx-ext-ipc",
		.of_match_table = sdx_ext_ipc_of_match,
#ifdef CONFIG_PM
		.pm = &sdx_ext_ipc_pm_ops,
#endif
	},
};

static int __init sdx_ext_ipc_register(void)
{
	return platform_driver_register(&sdx_ext_ipc_driver);
}
subsys_initcall(sdx_ext_ipc_register);

static void __exit sdx_ext_ipc_unregister(void)
{
	platform_driver_unregister(&sdx_ext_ipc_driver);
}
module_exit(sdx_ext_ipc_unregister);
MODULE_LICENSE("GPL");
